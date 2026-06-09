#!/usr/bin/env python3
"""
Claude Code "Stop" hook (serial transport): 把助手每次回复压成 1..20 的表情编号,
通过 ST-Link VCP 串口发给 BearPi (C4_uart_llm_emoji 示例), 板子 LCD 显示对应表情。

数据流:
  stdin JSON {last_assistant_message}
    -> 读取 tools/llm_hook/emoji_llm_config_serial.json
    -> 文本截断 (首 500 + 尾 1500 字符)
    -> HTTPS POST ZhiPu GLM (system prompt 见 emoji_llm_prompt.txt)
    -> 解析返回的 1-20 数字 (失败 -> fallback #6 Neutral)
    -> termios 配置串口 + os.write(b"N\\n")
    -> exit(0)

任何错误必须吞掉 (sys.exit(0)), 绝不能阻塞 Claude Code。

为何是 Python 不是 Node: macOS 上 ST-Link VCP 是 CDC-ACM, shell `stty + printf`
和 Node fs.writeSync 实测都送不进板子; Python `termios.tcsetattr` 切到 raw 模式后
`os.write` 稳定可用。详见 docs/device-dev/llm-emoji-hook.md 串口章节。
"""

import json
import os
import re
import sys
import ssl
import fcntl
import termios
import tty
import glob
import urllib.request
import urllib.error
from datetime import datetime, timezone

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROMPT_PATH = os.path.join(SCRIPT_DIR, "emoji_llm_prompt.txt")


def _make_ssl_context():
    """python.org macOS builds 缺省不带 CA bundle, 手动找:
       1) certifi (若装了), 2) /etc/ssl/cert.pem (macOS 系统), 3) Python framework 默认。
       找不到就返回 None, 让 urllib 用缺省 (会失败但能在 Linux/正常环境下跑)。"""
    candidates = []
    try:
        import certifi
        candidates.append(certifi.where())
    except Exception:
        pass
    candidates.append("/etc/ssl/cert.pem")  # macOS system
    fw = os.environ.get("SSL_CERT_FILE")
    if fw:
        candidates.append(fw)
    for ca in candidates:
        if ca and os.path.exists(ca):
            ctx = ssl.create_default_context(cafile=ca)
            ctx.check_hostname = True
            ctx.verify_mode = ssl.CERT_REQUIRED
            return ctx
    return ssl.create_default_context()


def load_config():
    """支持两种文件名: emoji_llm_config_serial.json (推荐) 或
    emoji_llm_hook_config_serial.json。返回 dict 或 None。"""
    for name in ("emoji_llm_config_serial.json",
                 "emoji_llm_hook_config_serial.json"):
        p = os.path.join(SCRIPT_DIR, name)
        if os.path.exists(p):
            with open(p, "r", encoding="utf-8") as f:
                return json.load(f)
    return None


def log(cfg, msg):
    if not cfg or not cfg.get("logging", {}).get("enabled"):
        return
    file = cfg["logging"].get("file", "emoji_llm_hook_serial.log")
    full = file if os.path.isabs(file) else os.path.join(SCRIPT_DIR, file)
    line = f"{datetime.now(timezone.utc).isoformat()} {msg}\n"
    try:
        with open(full, "a", encoding="utf-8") as f:
            f.write(line)
    except Exception:
        pass


def truncate(text, max_chars):
    if len(text) <= max_chars:
        return text
    head_len = min(500, max_chars // 4)
    tail_len = max_chars - head_len
    return text[:head_len] + "\n...[truncated]...\n" + text[-tail_len:]


def call_glm(cfg, user_text, system_prompt):
    """同步 HTTPS POST 到 ZhiPu GLM, 返回 (emoji_num 1-20, summary str)。失败抛异常。"""
    glm = cfg["glm"]
    body = json.dumps({
        "model": glm.get("model", "glm-4-flash"),
        "temperature": glm.get("temperature", 0.3),
        "max_tokens": glm.get("max_tokens", 50),
        "stream": False,
        "messages": [
            {"role": "system", "content": system_prompt},
            {"role": "user",
             "content": f"请把以下 AI 助手回复压缩为 'N|summary' 格式 (N 1-20, summary 是 ≤40 字符 ASCII 英文):\n\n---\n{user_text}\n---\n\n只输出 N|summary:"},
        ],
    }).encode("utf-8")

    url = glm.get("api_url", "https://open.bigmodel.cn/api/paas/v4/chat/completions")
    req = urllib.request.Request(
        url,
        data=body,
        method="POST",
        headers={
            "Authorization": f"Bearer {glm['api_key']}",
            "Content-Type": "application/json",
            "Accept": "application/json",
        },
    )
    timeout = glm.get("request_timeout_ms", 8000) / 1000.0
    ctx = _make_ssl_context()
    with urllib.request.urlopen(req, timeout=timeout, context=ctx) as resp:
        raw = resp.read().decode("utf-8", errors="replace")

    j = json.loads(raw)
    choices = j.get("choices") or []
    if not choices:
        raise ValueError(f"GLM empty choices: {raw[:200]}")
    content = (choices[0].get("message", {}).get("content") or "").strip()

    # 优先按 "N|summary" 格式解析 (兼容半角 | 和全角 ｜)
    m = re.match(r'^\s*(\d+)\s*[\|｜]\s*(.+?)\s*$', content, re.DOTALL)
    if m:
        emoji_num = int(m.group(1))
        if 1 <= emoji_num <= 20:
            summary = re.sub(r'\s+', ' ', m.group(2)).strip()
            summary = re.sub(r'[|｜\r\n]', ' ', summary).strip()
            summary = summary[:40]
            return emoji_num, summary

    # 兜底：纯数字格式 (GLM 没遵循新格式)
    for tok in content.replace(",", " ").replace("。", " ").split():
        if tok.isdigit():
            n = int(tok)
            if 1 <= n <= 20:
                return n, ""

    raise ValueError(f"No valid N|summary in GLM reply: {content[:100]}")


def resolve_serial_port(cfg):
    """读 cfg.board.serial_port, 支持 glob (默认 /dev/cu.usbmodem*)。"""
    configured = cfg.get("board", {}).get("serial_port") or "/dev/cu.usbmodem*"
    if "*" in configured or "?" in configured:
        matches = sorted(glob.glob(configured))
        if not matches:
            raise FileNotFoundError(f"No serial port matching {configured}")
        if len(matches) > 1:
            log(cfg, f"Multiple serial ports, using: {matches[0]}")
        return matches[0]
    return configured


def send_to_board(cfg, emoji_num, summary):
    """打开串口, 切 raw 模式, 写 b'N|summary\\n', 关闭。失败抛异常。"""
    port = resolve_serial_port(cfg)
    baud = cfg.get("board", {}).get("baud_rate", 115200)
    term = cfg.get("board", {}).get("line_terminator", "\n")
    payload = f"{emoji_num}|{summary}{term}".encode("utf-8")

    # O_RDWR 读写 / O_NOCTTY 不让 tty 控制本进程 / O_NONBLOCK 防止卡 open
    fd = os.open(port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)

    try:
        # 切回阻塞模式, 让 write 等到字符送出
        flags = fcntl.fcntl(fd, fcntl.F_GETFL, 0)
        fcntl.fcntl(fd, fcntl.F_SETFL, flags & ~os.O_NONBLOCK)

        # macOS Python termios 把波特率直接存 attrs[4] (ispeed) / attrs[5] (ospeed),
        # 值用裸整数 (B115200 == 115200); Linux 上是 cflag bitmask, 但 Linux 走
        # /dev/ttyUSB* 路径不同。我们只支持 macOS, 直接赋整数即可。
        attrs = termios.tcgetattr(fd)
        tty.setraw(fd)
        attrs = termios.tcgetattr(fd)
        _check_baud_supported(baud)
        attrs[4] = baud  # ispeed
        attrs[5] = baud  # ospeed
        attrs[2] |= termios.CREAD  # c_cflag 强制使能接收, 部分平台必需
        termios.tcsetattr(fd, termios.TCSANOW, attrs)
        # 清空缓冲区, 避免之前残留字节污染本次写入
        termios.tcflush(fd, termios.TCIOFLUSH)

        os.write(fd, payload)
        # macOS CDC 不需要 tcdrain, 但加上无害且能确保发送
        try:
            termios.tcdrain(fd)
        except Exception:
            pass
    finally:
        os.close(fd)


def _check_baud_supported(baud):
    """校验波特率在 termios 表内。macOS Python termios 用裸整数 (B115200==115200)。"""
    supported = {9600, 19200, 38400, 57600, 115200, 230400, 460800, 500000, 921600}
    if baud not in supported:
        raise ValueError(f"Unsupported baud rate: {baud}")


def main():
    cfg = None
    try:
        cfg = load_config()
    except Exception:
        sys.exit(0)
    if not cfg:
        sys.exit(0)  # 没配置 = 功能未启用

    # 读 stdin (5 秒兜底超时由 Claude Code hook 自身的 timeout 控制)
    try:
        stdin_raw = sys.stdin.buffer.read().decode("utf-8", errors="replace")
    except Exception:
        sys.exit(0)
    if not stdin_raw.strip():
        sys.exit(0)

    try:
        payload = json.loads(stdin_raw)
    except Exception:
        sys.exit(0)

    text = (payload.get("last_assistant_message") or "").strip()
    if not text:
        sys.exit(0)

    try:
        with open(PROMPT_PATH, "r", encoding="utf-8") as f:
            system_prompt = f.read()
    except Exception:
        sys.exit(0)

    max_chars = cfg.get("prompt", {}).get("max_input_chars", 2000)
    truncated = truncate(text, max_chars)

    fallback = cfg.get("prompt", {}).get("fallback_emoji", 6)
    fallback_summary = cfg.get("prompt", {}).get("fallback_summary", "LLM call failed")
    try:
        emoji_num, summary = call_glm(cfg, truncated, system_prompt)
    except Exception as e:
        log(cfg, f"GLM error: {e}")
        emoji_num, summary = fallback, fallback_summary

    if not isinstance(emoji_num, int) or not (1 <= emoji_num <= 20):
        emoji_num = fallback
    if not isinstance(summary, str):
        summary = fallback_summary

    try:
        send_to_board(cfg, emoji_num, summary)
        log(cfg, f"OK emoji={emoji_num} summary='{summary}' text_len={len(text)}")
    except Exception as e:
        log(cfg, f"Board error: {e}")

    sys.exit(0)


if __name__ == "__main__":
    try:
        main()
    except SystemExit:
        raise
    except Exception:
        # 任何未捕获异常也吞掉
        sys.exit(0)
