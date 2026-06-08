#!/usr/bin/env node
/*
 * Bridge: browser WebSocket <-> BearPi TCP socket (ESP8266 server on port 8080)
 * Plus a simple HTTP server hosting the controller HTML page and emoji_png
 * preview thumbnails.
 *
 * Uses only Node.js built-in modules -- no npm install needed.
 *
 * Usage:
 *   node tools/emoji_wifi_server.js --ip 192.168.1.50
 */

"use strict";

const http = require("http");
const net = require("net");
const crypto = require("crypto");
const fs = require("fs");
const path = require("path");
const url = require("url");
const { exec } = require("child_process");

const HERE = __dirname;
const HTTP_PORT = 8000;
const WS_PORT = 8000; // WS uses the same HTTP server via upgrade event

const args = parseArgs(process.argv.slice(2));
const BOARD_IP = args.ip;
const BOARD_PORT = args.port || 8080;

if (!BOARD_IP) {
  console.error("Error: --ip <board_ip> is required");
  console.error("Example: node tools/emoji_wifi_server.js --ip 192.168.1.50");
  process.exit(1);
}

const MIME = {
  ".html": "text/html; charset=utf-8",
  ".js":   "application/javascript",
  ".css":  "text/css",
  ".png":  "image/png",
  ".jpg":  "image/jpeg",
  ".svg":  "image/svg+xml",
  ".json": "application/json",
  ".ico":  "image/x-icon",
};

/* ---- HTTP server: serve static files from tools/ ---- */
const httpServer = http.createServer((req, res) => {
  const pathname = decodeURIComponent(url.parse(req.url).pathname);
  let filePath = path.normalize(path.join(HERE, pathname));
  if (!filePath.startsWith(HERE)) {
    res.writeHead(403); res.end("Forbidden"); return;
  }
  if (filePath === HERE) filePath = path.join(HERE, "emoji_wifi_controller.html");

  fs.readFile(filePath, (err, data) => {
    if (err) {
      res.writeHead(404, { "Content-Type": "text/plain" });
      res.end("Not found: " + pathname);
      return;
    }
    const ext = path.extname(filePath).toLowerCase();
    res.writeHead(200, { "Content-Type": MIME[ext] || "application/octet-stream" });
    res.end(data);
  });
});

httpServer.listen(HTTP_PORT, () => {
  console.log(`[HTTP] serving on http://localhost:${HTTP_PORT}`);
});

/* ---- WebSocket server on the same HTTP server via 'upgrade' event ---- */
httpServer.on("upgrade", (req, socket) => {
  const key = req.headers["sec-websocket-key"];
  if (!key) { socket.destroy(); return; }
  const accept = crypto
    .createHash("sha1")
    .update(key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11")
    .digest("base64");
  const headers = [
    "HTTP/1.1 101 Switching Protocols",
    "Upgrade: websocket",
    "Connection: Upgrade",
    `Sec-WebSocket-Accept: ${accept}`,
    "",
    "",
  ].join("\r\n");
  socket.write(headers);
  handleWsClient(socket);
});

function wsSend(socket, message) {
  const payload = Buffer.from(message, "utf8");
  const len = payload.length;
  let frame;
  if (len < 126) {
    frame = Buffer.alloc(2 + len);
    frame[0] = 0x81;            // FIN + text
    frame[1] = len;
    payload.copy(frame, 2);
  } else if (len < 65536) {
    frame = Buffer.alloc(4 + len);
    frame[0] = 0x81;
    frame[1] = 126;
    frame.writeUInt16BE(len, 2);
    payload.copy(frame, 4);
  } else {
    frame = Buffer.alloc(10 + len);
    frame[0] = 0x81;
    frame[1] = 127;
    frame.writeBigUInt64BE(BigInt(len), 2);
    payload.copy(frame, 10);
  }
  socket.write(frame);
}

/* Minimal WS frame parser (text frames only, no masking of incoming not needed - clients always mask) */
function handleWsClient(socket) {
  console.log("[WS] browser connected");
  let tcp = null;
  let buf = Buffer.alloc(0);

  function cleanup() {
    try { socket.destroy(); } catch (_) {}
    if (tcp) { try { tcp.end(); } catch (_) {} tcp = null; }
  }

  // Connect to board TCP
  tcp = net.connect(BOARD_PORT, BOARD_IP);
  tcp.setTimeout(5000);
  tcp.on("connect", () => {
    tcp.setTimeout(0);
    console.log(`[TCP] connected to board ${BOARD_IP}:${BOARD_PORT}`);
    wsSend(socket, "TCP:connected");
  });
  tcp.on("timeout", () => {
    wsSend(socket, "ERR:connect timeout");
    cleanup();
  });
  tcp.on("error", (err) => {
    console.log(`[TCP] error: ${err.message}`);
    wsSend(socket, `ERR:${err.message}`);
    cleanup();
  });
  tcp.on("data", (data) => {
    const text = data.toString("utf8").trim();
    if (text) wsSend(socket, `RECV:${text}`);
  });
  tcp.on("close", () => {
    wsSend(socket, "TCP:closed");
    cleanup();
  });

  socket.on("data", (chunk) => {
    buf = Buffer.concat([buf, chunk]);
    // Parse one or more frames
    while (buf.length >= 2) {
      const b0 = buf[0], b1 = buf[1];
      const opcode = b0 & 0x0f;
      const masked = (b1 & 0x80) !== 0;
      let payloadLen = b1 & 0x7f;
      let offset = 2;
      if (payloadLen === 126) {
        if (buf.length < 4) return;
        payloadLen = buf.readUInt16BE(2);
        offset = 4;
      } else if (payloadLen === 127) {
        if (buf.length < 10) return;
        payloadLen = Number(buf.readBigUInt64BE(2));
        offset = 10;
      }
      let maskKey = null;
      if (masked) {
        if (buf.length < offset + 4) return;
        maskKey = buf.slice(offset, offset + 4);
        offset += 4;
      }
      if (buf.length < offset + payloadLen) return;
      let payload = buf.slice(offset, offset + payloadLen);
      if (masked) {
        const unmasked = Buffer.allocUnsafe(payload.length);
        for (let i = 0; i < payload.length; i++) {
          unmasked[i] = payload[i] ^ maskKey[i & 3];
        }
        payload = unmasked;
      }
      buf = buf.slice(offset + payloadLen);

      // opcode 8 = close, 9 = ping, 10 = pong, 1 = text, 0 = continuation
      if (opcode === 8) { cleanup(); return; }
      if (opcode === 9) {
        // pong back
        const pong = Buffer.alloc(2 + payload.length);
        pong[0] = 0x8a; pong[1] = payload.length;
        payload.copy(pong, 2);
        socket.write(pong);
        continue;
      }
      if (opcode === 1 || opcode === 0) {
        const msg = payload.toString("utf8");
        if (tcp && !tcp.destroyed) {
          tcp.write(msg + "\n");
          console.log(`[TCP] sent ${JSON.stringify(msg + "\n")}`);
        }
      }
    }
  });

  socket.on("error", (err) => {
    console.log(`[WS] socket error: ${err.message}`);
    cleanup();
  });
  socket.on("close", () => {
    console.log("[WS] browser disconnected");
    cleanup();
  });
}

/* ---- Args + launch browser ---- */
function parseArgs(argv) {
  const out = { ip: null, port: 0 };
  for (let i = 0; i < argv.length; i++) {
    if (argv[i] === "--ip") out.ip = argv[++i];
    else if (argv[i] === "--port") out.port = parseInt(argv[++i], 10);
    else if (argv[i] === "--http-port") { /* not used here */ }
  }
  return out;
}

console.log(`[Bridge] board target: ${BOARD_IP}:${BOARD_PORT}`);

const launchUrl = `http://localhost:${HTTP_PORT}/emoji_wifi_controller.html?wsport=${HTTP_PORT}`;
console.log(`Open in browser: ${launchUrl}`);
if (process.platform === "win32") {
  exec(`start "" "${launchUrl}"`);
} else if (process.platform === "darwin") {
  exec(`open "${launchUrl}"`);
}
