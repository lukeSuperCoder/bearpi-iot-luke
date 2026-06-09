# LLM 对话表情展示 (C3_wifi_llm_emoji)

每次 Claude Code 助手回复完成后,自动调用 ZhiPu GLM 把回复内容压成 1..20 中的一个表情编号,通过 TCP 发给 BearPi 开发板,LCD 显示对应表情。

## 架构

```
Claude Code reply 完成
        │ Stop hook (.claude/settings.local.json)
        ▼
node tools/llm_hook/emoji_llm_hook.js   (stdin: JSON {last_assistant_message})
        │
        ├─ 读 tools/llm_hook/emoji_llm_config.json (API key + 板子 IP)
        ├─ HTTPS POST → open.bigmodel.cn (system prompt 见 emoji_llm_prompt.txt)
        ├─ 解析返回的 1-20 数字 (失败 → fallback #6 Neutral)
        └─ TCP connect → board_ip:8080, 发送 "N\n", 关闭
                │
                ▼
        BearPi LCD 显示对应表情
```

## 依赖

- **Node.js** ≥ 16 (脚本只用 `https`/`net`/`fs` 内置模块,无需 `npm install`)
- **Claude Code** CLI (Stop hook 由 Claude Code 执行)
- **ZhiPu GLM API Key** (在 https://bigmodel.cn 控制台申请)
- **BearPi 开发板** 烧录了 `C3_wifi_llm_emoji` 固件

## 配置步骤

### 1. 烧录固件

在项目根目录:

```bash
start menuconfig.exe          # 选择 C3_wifi_llm_emoji
make clean && make -j8
make download
```

如果你的 WiFi 不是默认的 `HeyBro` / `123456798`,先改 `applications/C3_wifi_llm_emoji/main.c` 第 33-34 行再编译。

烧录后板子启动,LCD 顶部会显示 `Srv <board_ip>:8080`,**记下这个 IP**。

### 2. 配置 LLM Hook

```bash
cp tools/llm_hook/emoji_llm_config.example.json tools/llm_hook/emoji_llm_config.json
```

编辑 `tools/llm_hook/emoji_llm_config.json`:

- `glm.api_key`: 填入你的 ZhiPu API Key
- `board.host`: 填入上一步看到的板子 IP (例如 `192.168.3.42`)
- `logging.enabled`: 调试时设 `true`,稳定后改 `false`

`tools/llm_hook/emoji_llm_config.json` 已在 `.gitignore` 中,API key 不会被提交。

### 3. 注册 Claude Code Stop hook

编辑 `.claude/settings.local.json` (没有就新建),加入:

```json
{
  "hooks": {
    "Stop": [
      {
        "matcher": "",
        "hooks": [
          {
            "type": "command",
            "command": "node \"D:/work/bear-stm32/bearpi-iot_std/tools/llm_hook/emoji_llm_hook.js\"",
            "timeout": 20
          }
        ]
      }
    ]
  }
}
```

> 路径里有空格/Windows 盘符都无所谓,只要用双引号包起来即可。

**重启 Claude Code** 让 hook 生效。

## 验证

### Phase A: 板子 TCP 通了

PC 命令行直接发数字测一下:

```bash
node -e "require('net').connect(8080,'<board_ip>').on('connect',s=>{s.write('5\n');s.end()})"
```

板子 LCD 应该显示 Cool (戴墨镜) 表情。

### Phase B: hook 脚本独立测试 (不依赖 Claude)

```bash
echo '{"last_assistant_message":"Great, I fixed the bug and the tests pass!"}' | node tools/llm_hook/emoji_llm_hook.js
```

板子应在 3-8 秒内更新到积极表情 (大概率 1 Grinning 或 2 Tears of Joy)。

如果 `logging.enabled = true`,可以查 `tools/llm_hook/emoji_llm_hook.log` 看到一行 `OK emoji=N text_len=...`。

### Phase C: 端到端

跟 Claude 说有情绪色彩的话:

- "帮我调试这个崩溃" → 期望 #9 Angry 或 #10 Crying
- "解释一下 CPU 缓存原理" → 期望 #17 Nerd 或 #18 Thinking
- "成功了谢谢!" → 期望 #1 或 #4
- Claude 改自己之前的说法 → 期望 #13 Flushed

每次 Claude 回完话,板子应在 3-8 秒内更新表情。

## 调试

| 现象 | 排查 |
|---|---|
| 板子表情完全不变 | TCP 不通: 检查 `board.host` IP 是否正确、防火墙是否放行 8080 |
| 板子永远显示 #6 Neutral | GLM 调用失败: 开 `logging.enabled` 看 `emoji_llm_hook.log`,通常是 API key 无效或网络不通 |
| Claude 卡顿/报错 | 不应该发生 - 脚本所有路径都 `process.exit(0)`。如果真的发生,临时把 `settings.local.json` 里的 `Stop` hook 删掉,然后查 log |
| 想换 prompt | 直接编辑 `tools/llm_hook/emoji_llm_prompt.txt`,无需重启 Claude |
| 想换表情数量 | 目前固定 20,如需修改要同时改 prompt 文件、`emoji_data.c` 数组、`process_command` 范围检查 |

## 表情编号速查

```
 1 Grinning         咧嘴笑       任务完成、简单确认
 2 Tears of Joy     笑哭         幽默、夸张成功
 3 Winking          眨眼         提示、调侃
 4 Heart Eyes       爱心眼       强烈推荐、赞美
 5 Cool             戴墨镜       优化、最佳实践
 6 Neutral          中性         默认/无强烈情绪 (fallback)
 7 Unamused         不悦         反对、不推荐
 8 Disappointed     失望         方案不可行
 9 Angry            愤怒         严重 bug、安全风险
10 Crying           哭泣         失败、错误
11 Surprised        惊讶         意外发现
12 Screaming        尖叫         重大问题
13 Flushed          脸红         致歉、修正
14 Sleeping         睡觉         等待、空闲
15 Dizzy            眩晕         困惑
16 Money Mouth      金钱嘴       成本优化
17 Nerd             书呆子       深入技术细节
18 Thinking         思考         分析、推理
19 Hugging          拥抱         鼓励、安慰
20 Exploding Head   爆炸头       顿悟、震撼
```

## 安全注意

- `tools/llm_hook/emoji_llm_config.json` 含 API key,**不要提交 git** (`.gitignore` 已配置)
- `.claude/settings.local.json` 含本机绝对路径,也在 `.gitignore` 中
- hook 脚本只读 stdin,不向 stdout/stderr 输出任何内容,避免污染 Claude 终端

## 故障保险

如果 hook 异常导致 Claude 体验差,有两个紧急关闭方式:

1. 删除 `.claude/settings.local.json` 中的 `hooks.Stop` 块
2. 把 `tools/llm_hook/emoji_llm_config.json` 重命名/删除 - 脚本会立即静默退出

## 文件清单

| 路径 | 用途 |
|---|---|
| `applications/C3_wifi_llm_emoji/` | 固件源码 (3 个文件) |
| `tools/llm_hook/emoji_llm_hook.js` | Hook 脚本 |
| `tools/llm_hook/emoji_llm_prompt.txt` | GLM system prompt |
| `tools/llm_hook/emoji_llm_config.example.json` | 配置模板 (提交到 git) |
| `tools/llm_hook/emoji_llm_config.json` | 真实配置 (gitignored) |
| `tools/llm_hook/emoji_llm_hook.log` | 运行日志 (gitignored, 可选) |
| `.claude/settings.local.json` | Claude Code hook 注册 (gitignored) |

---

## 串口版本 (C4_uart_llm_emoji)

C3 依赖 WiFi/TCP,在没有可用 2.4GHz AP 的环境下 (例如办公室只有 5GHz) 跑不起来。串口版本把传输换成 ST-Link VCP,只插一根 USB 线就能跑通同样的「AI 回复 → 表情」链路。

### 架构

```
Claude Code reply 完成
        │ Stop hook (.claude/settings.local.json)
        ▼
python3 tools/llm_hook/emoji_llm_hook_serial.py   (stdin: JSON {last_assistant_message})
        │
        ├─ 读 tools/llm_hook/emoji_llm_config_serial.json
        ├─ HTTPS POST → open.bigmodel.cn (system prompt 见 emoji_llm_prompt.txt)
        ├─ 解析返回的 "N|summary" (失败 → fallback #6 + "LLM call failed")
        └─ os.open(/dev/cu.usbmodem*) + termios raw + os.write(b"N|summary\n")
                │ ST-Link VCP (USB CDC)
                ▼
        BearPi USART1 RX IRQ (PA10, 115200 8N1)
                │
                ▼
        LCD 上方显示对应表情, 下方显示英文摘要 (按 240px 宽度自动换行)
```

### 协议格式

板子接收单行 UTF-8 文本帧,以 `\n` 结尾:

```
<N>|<summary>\n
```

- `N`: 1-20 的表情编号
- `|`: 英文半角竖线 (兼容全角 `｜`,Python hook 都能解析)
- `summary`: ASCII 英文摘要,≤40 字符,不含换行/竖线/markdown
- LCD 240px 宽 / 8px 每字符 = 每行最多 30 字符,超出自动换行到下一行
- 错误格式 (无 `|` 或 N 越界) 板子静默忽略,仅 printf 输出便于调试

### 为什么是 Python 不是 Node

macOS 上 ST-Link VCP 走 CDC-ACM,实测以下三种方式都送不进板子:

- `stty` + shell `printf > /dev/cu.usbmodem*`
- `dd of=/dev/cu.usbmodem* conv=fsync`
- Node.js `fs.openSync('w'/'r+'/'rs+')` + `fs.writeSync`

只有 Python 的 `termios.tcsetattr + os.write` 稳定可用。这是 macOS CDC-ACM 驱动的怪癖,Linux 上 Node/shell 路径是正常的。

### 依赖

- **Python 3** (macOS 自带或 python.org 安装都行,只用 stdlib)
- **Claude Code** CLI (Stop hook 由 Claude Code 执行)
- **ZhiPu GLM API Key** (与 TCP 版可复用同一个 token)
- **BearPi 开发板** 烧录了 `C4_uart_llm_emoji` 固件,通过 ST-Link USB 接 PC

### 配置步骤

#### 1. 烧录固件

```bash
python menuconfig.py            # 选 C4_uart_llm_emoji
make clean && make -j8
make download_mac
```

板子启动后 LCD 上方显示默认 emoji (#6 Neutral),下方等待 Stop hook 推送摘要。

#### 2. 找到串口设备名

```bash
ls -1 /dev/cu.usbmodem*
```

macOS ST-Link VCP 通常是 `/dev/cu.usbmodem1203`、`/dev/cu.usbmodem14103` 等。配置文件支持 glob 自动选第一个匹配。

#### 3. 配置 LLM Hook

```bash
cp tools/llm_hook/emoji_llm_config_serial.example.json tools/llm_hook/emoji_llm_config_serial.json
```

编辑 `tools/llm_hook/emoji_llm_config_serial.json`:

- `glm.api_key`: 填 ZhiPu API Key (可复用 `~/.claude/settings.json` 的 `ANTHROPIC_AUTH_TOKEN`)
- `glm.model`: 默认 `glm-4-flash` (快且免费额度大;`glm-5.1` 在个人账户上一般不可用)
- `board.serial_port`: 具体 `/dev/cu.usbmodemXXXX` 或 glob `/dev/cu.usbmodem*`
- `logging.enabled`: 调试时 `true`,稳定后 `false`

#### 4. 注册 Claude Code Stop hook

编辑 `.claude/settings.local.json`:

```json
{
  "hooks": {
    "Stop": [
      {
        "matcher": "",
        "hooks": [
          {
            "type": "command",
            "command": "python3 \"/Users/luke/Desktop/project/iot-space/bearpi-iot-luke/tools/llm_hook/emoji_llm_hook_serial.py\"",
            "timeout": 20
          }
        ]
      }
    ]
  }
}
```

**重启 Claude Code** 让 hook 生效。

### 验证

#### Phase A: 板子能收 (不依赖 hook)

```bash
python3 -c "
import os, termios, tty
fd = os.open('/dev/cu.usbmodem1203', os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
attrs = termios.tcgetattr(fd); tty.setraw(fd); attrs = termios.tcgetattr(fd)
attrs[4] = attrs[5] = 115200; termios.tcsetattr(fd, termios.TCSANOW, attrs)
termios.tcflush(fd, termios.TCIOFLUSH)
os.write(fd, b'5|Completed LCD optimization\n')
"
```

板子 LCD 应在 100ms 内上方变成 #5 Cool,下方显示 `Completed LCD optimiz` / `ation` 两行 (30 字符/行换行)。

发 60 字符长摘要验证换行:

```bash
# 替换上面 os.write 一行为:
os.write(fd, b'1|Build succeeded tests passed fixed three bugs and added two new unit tests for coverage\n')
```

应显示 2 行摘要 + emoji #1。

#### Phase B: hook 独立测 (不依赖 Claude)

```bash
echo '{"last_assistant_message":"Build succeeded and tests pass!"}' | \
  python3 tools/llm_hook/emoji_llm_hook_serial.py
```

3-8 秒内板子应更新为积极表情 (#1/#2/#5 居多) + 英文摘要 (例如 `Build OK, tests pass`)。

#### Phase C: 端到端

退出 Claude Code 重启。跟 Claude 说有情绪色彩的话,每次回复后 3-8 秒内板子 LCD 应更新。

### 排错

| 现象 | 排查 |
|---|---|
| 板子完全不变 | 串口路径错或被独占: `ls /dev/cu.usbmodem*`,关掉 `screen`/Arduino IDE/其它占用程序 |
| 板子永远显示 #6 + "LLM call failed" | GLM 调用失败: 开 `logging.enabled`,查 `emoji_llm_hook_serial.log`,常见为 API key 无效或 model 名错 (`glm-5.1` 不可用 → 改 `glm-4-flash`)、`max_tokens` 太小 (应 ≥50) |
| 表情变了但下方摘要为空 | GLM 没遵循 `N|summary` 格式,只输出了纯数字。hook 会兜底为空摘要。检查 prompt 是否更新,或临时增大 `temperature` |
| 摘要被截断到 40 字符 | 设计如此 (LCD 显示空间有限)。如需更长,要同时改 hook `[:40]` 截断、prompt 字数限制、main.c 的 `SUMMARY_CHARS_PER_LINE` |
| `[SSL: CERTIFICATE_VERIFY_FAILED]` | python.org macOS 版缺 CA bundle: 脚本会自动尝试 `certifi` → `/etc/ssl/cert.pem`;若都不在,`pip3 install certifi` |
| Claude 卡顿 | 不应发生: 所有路径 `sys.exit(0)`。若真发生,临时删 `settings.local.json` 里 `Stop` hook |
| `Multiple serial ports, using: ...` | 配了 glob 但机器上有多个 USB CDC 设备;改成具体设备名可消除歧义 |

### TCP 版 vs 串口版

| 维度 | C3 (TCP) | C4 (串口) |
|---|---|---|
| 网络 | 需要 2.4GHz WiFi AP | 不需要,一根 USB |
| 主机语言 | Node.js (内置模块) | Python (stdlib) |
| 跨平台 | Windows/Linux/macOS | 当前仅 macOS (Linux 可用同代码;Windows 需改 `pyserial`) |
| 速率 | ~1 秒 | ~3-8 秒 (含 GLM 推理) |
| 抗噪声 | TCP 重传 | 字节级,无重传 (但单字节命令几乎不会出错) |
| 占用 | 板子跑 ESP8266 + TCP server | 板子只跑 USART1 RX IRQ |

两者**并存**,通过 `Kconfig` 切换固件,Claude Code 同一时刻只该装一个 Stop hook (否则 GLM 会被调用两次)。

### 文件清单 (串口版)

| 路径 | 用途 |
|---|---|
| `applications/C4_uart_llm_emoji/` | 固件源码 (`main.c` + `emoji_data.{c,h}`) |
| `tools/llm_hook/emoji_llm_hook_serial.py` | Hook 脚本 |
| `tools/llm_hook/emoji_llm_prompt.txt` | GLM system prompt (与 TCP 版共用) |
| `tools/llm_hook/emoji_llm_config_serial.example.json` | 配置模板 (提交到 git) |
| `tools/llm_hook/emoji_llm_config_serial.json` | 真实配置 (gitignored) |
| `tools/llm_hook/emoji_llm_hook_serial.log` | 运行日志 (gitignored, 可选) |

