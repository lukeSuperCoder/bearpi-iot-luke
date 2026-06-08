#!/usr/bin/env node
/*
 * Claude Code "Stop" hook: 把助手每次回复压成 1..20 的表情编号,
 * 通过 TCP 发给 BearPi 开发板 (C3_wifi_llm_emoji 示例),板子 LCD 显示对应表情。
 *
 * 数据流:
 *   stdin JSON {last_assistant_message}
 *     -> 读取 tools/llm_hook/emoji_llm_config.json
 *     -> 文本截断 (首 500 + 尾 1500 字符)
 *     -> HTTPS POST ZhiPu GLM (system prompt 见 emoji_llm_prompt.txt)
 *     -> 解析返回的 1-20 数字 (失败 -> fallback #6 Neutral)
 *     -> net.connect board_ip:8080, 发送 "N\n", 关闭
 *     -> exit(0)
 *
 * 任何错误必须吞掉 (process.exit(0)),绝不能阻塞 Claude Code。
 */

"use strict";

const https = require("https");
const net   = require("net");
const fs    = require("fs");
const path  = require("path");

const SCRIPT_DIR = __dirname;
const CONFIG_PATH = path.join(SCRIPT_DIR, "emoji_llm_hook_config.json");
const PROMPT_PATH = path.join(SCRIPT_DIR, "emoji_llm_prompt.txt");

// 兼容两种文件名: emoji_llm_config.json (推荐) 或 emoji_llm_hook_config.json
function loadConfig() {
    const candidates = [
        path.join(SCRIPT_DIR, "emoji_llm_config.json"),
        CONFIG_PATH,
    ];
    for (const p of candidates) {
        if (fs.existsSync(p)) {
            return JSON.parse(fs.readFileSync(p, "utf8"));
        }
    }
    return null;
}

function log(cfg, msg) {
    if (!cfg || !cfg.logging || !cfg.logging.enabled) return;
    const file = cfg.logging.file || "emoji_llm_hook.log";
    const full = path.isAbsolute(file) ? file : path.join(SCRIPT_DIR, file);
    const line = `${new Date().toISOString()} ${msg}\n`;
    try { fs.appendFileSync(full, line); } catch (_) { /* swallow */ }
}

function truncate(text, max) {
    if (text.length <= max) return text;
    const headLen = Math.min(500, Math.floor(max / 4));
    const tailLen = max - headLen;
    return text.slice(0, headLen) + "\n...[truncated]...\n" + text.slice(-tailLen);
}

function callGLM(cfg, userText, systemPrompt) {
    return new Promise((resolve, reject) => {
        const body = JSON.stringify({
            model: cfg.glm.model || "glm-5.1",
            temperature: cfg.glm.temperature != null ? cfg.glm.temperature : 0.3,
            max_tokens: cfg.glm.max_tokens || 8,
            stream: false,
            messages: [
                { role: "system", content: systemPrompt },
                { role: "user",   content: `请将以下 AI 助手回复压缩为一个表情编号 (1-20):\n\n---\n${userText}\n---\n\n只输出数字:` }
            ]
        });
        const url = new URL(cfg.glm.api_url || "https://open.bigmodel.cn/api/paas/v4/chat/completions");
        const opts = {
            method: "POST",
            hostname: url.hostname,
            path: url.pathname + url.search,
            headers: {
                "Authorization": `Bearer ${cfg.glm.api_key}`,
                "Content-Type": "application/json",
                "Accept": "application/json",
                "Content-Length": Buffer.byteLength(body)
            },
            timeout: cfg.glm.request_timeout_ms || 8000
        };
        const req = https.request(opts, res => {
            let data = "";
            res.on("data", c => data += c);
            res.on("end", () => {
                if (res.statusCode !== 200) {
                    return reject(new Error(`GLM HTTP ${res.statusCode}: ${data.slice(0, 200)}`));
                }
                try {
                    const json = JSON.parse(data);
                    const content = (json.choices && json.choices[0] && json.choices[0].message && json.choices[0].message.content || "").trim();
                    const m = content.match(/\b(20|1\d|[1-9])\b/);
                    if (!m) return reject(new Error(`No number in GLM reply: ${content.slice(0, 100)}`));
                    resolve(parseInt(m[1], 10));
                } catch (e) {
                    reject(new Error(`GLM JSON parse: ${e.message}`));
                }
            });
        });
        req.on("timeout", () => { req.destroy(); reject(new Error("GLM timeout")); });
        req.on("error", reject);
        req.write(body);
        req.end();
    });
}

function sendToBoard(cfg, emojiNum) {
    return new Promise((resolve, reject) => {
        const term = (cfg.board && cfg.board.line_terminator) || "\n";
        const payload = `${emojiNum}${term}`;
        const sock = net.connect({
            host: cfg.board.host,
            port: cfg.board.port || 8080
        });
        sock.setTimeout(cfg.board.connect_timeout_ms || 3000);
        sock.once("connect", () => {
            sock.write(payload, () => {
                sock.end();
                resolve();
            });
        });
        sock.once("timeout", () => { sock.destroy(); reject(new Error("board timeout")); });
        sock.once("error", reject);
    });
}

async function main() {
    let cfg;
    try {
        cfg = loadConfig();
    } catch (e) {
        // 配置文件 JSON 解析失败也不报错,直接退出
        process.exit(0);
    }
    if (!cfg) process.exit(0); // 没配置 = 功能未启用

    let payload;
    try { payload = JSON.parse(stdinBuf); } catch (_) { process.exit(0); }

    const text = (payload && payload.last_assistant_message || "").toString();
    if (!text.trim()) process.exit(0);

    let systemPrompt;
    try {
        systemPrompt = fs.readFileSync(PROMPT_PATH, "utf8");
    } catch (_) {
        process.exit(0); // prompt 文件缺失 = 静默退出
    }

    const max = (cfg.prompt && cfg.prompt.max_input_chars) || 2000;
    const truncated = truncate(text, max);

    let emojiNum;
    try {
        emojiNum = await callGLM(cfg, truncated, systemPrompt);
    } catch (e) {
        log(cfg, `GLM error: ${e.message}`);
        emojiNum = (cfg.prompt && cfg.prompt.fallback_emoji) || 6;
    }

    if (!Number.isInteger(emojiNum) || emojiNum < 1 || emojiNum > 20) {
        emojiNum = (cfg.prompt && cfg.prompt.fallback_emoji) || 6;
    }

    try {
        await sendToBoard(cfg, emojiNum);
        log(cfg, `OK emoji=${emojiNum} text_len=${text.length}`);
    } catch (e) {
        log(cfg, `Board error: ${e.message}`);
    }

    process.exit(0);
}

// ---- 读 stdin ----
let stdinBuf = "";
process.stdin.setEncoding("utf8");
process.stdin.on("data", chunk => { stdinBuf += chunk; });
process.stdin.on("end", () => { main().catch(() => process.exit(0)); });
process.stdin.on("error", () => process.exit(0));

// 兜底: 500ms 内没收到 stdin 就退出 (说明 hook 被错误调用)
setTimeout(() => process.exit(0), 500);
