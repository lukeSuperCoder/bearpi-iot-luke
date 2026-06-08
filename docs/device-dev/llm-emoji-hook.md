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
