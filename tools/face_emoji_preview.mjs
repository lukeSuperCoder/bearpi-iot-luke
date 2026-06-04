/**
 * Generate a local HTML preview page for downloaded Face emojis.
 * Shows each emoji with its image, name, and filename.
 *
 * Usage: node tools/face_emoji_preview.mjs
 */

import { readFileSync, writeFileSync, readdirSync } from "fs";
import { join, basename } from "path";
import { PNG } from "pngjs";

const INPUT_DIR = "tools/emoji_png_faces";
const OUTPUT_FILE = "tools/face_emoji_preview.html";

const NAMES = [
  "Grinning Face", "Beaming Face", "Face with Tears of Joy", "Smiling with Open Eyes",
  "Beaming with Eyes", "Grinning with Sweat", "Grinning Squinting", "Halo Face",
  "Winking Face", "Smiling with Hearts", "Savoring Food", "Relieved Face",
  "Heart Eyes", "Cool Face (Sunglasses)", "Smirking Face", "Neutral Face",
  "Expressionless", "Unamused Face", "Downcast with Sweat", "Pensive Face",
  "Kissing Face", "Face Blowing a Kiss", "Kissing Smiling Eyes", "Kissing Closed Eyes",
  "Face with Tongue", "Squinting with Tongue", "Disappointed Face", "Worried Face",
  "Angry Face", "Pouting Face", "Crying Face", "Persevering Face",
  "Tired Face", "Sleepy Face", "Weary Face", "Grimacing Face",
  "Loudly Crying Face", "Surprised Face", "Hushed Face", "Screaming in Fear",
  "Astonished Face", "Flushed Face", "Sleeping Face", "Dizzy Face",
  "Face Without Mouth", "Medical Mask Face", "Slightly Smiling", "Upside-Down Face",
  "Money-Mouth Face", "Thermometer Face", "Nerd Face", "Head-Bandage Face",
  "Hugging Face", "Clown Face", "Nauseated Face", "Rolling on Floor Laughing",
  "Drooling Face", "Lying Face", "Sneezing Face", "Raised Eyebrow Face",
  "Star-Struck", "Heart-Eyes Smiling", "Smiling with Tear", "Woozy Face",
  "Hot Face", "Cold Face", "Disguised Face", "Shushing Face",
  "Symbols on Mouth", "Hand Over Mouth", "Face Vomiting", "Exploding Head",
  "Smiling Cat", "Cat with Tears of Joy", "Smiling Cat Heart-Eyes",
  "Cat with Wry Smile", "Kissing Cat", "Weary Cat", "Crying Cat", "Pouting Cat",
];

const files = readdirSync(INPUT_DIR).filter(f => f.endsWith(".png")).sort();

function codepointToEmoji(filename) {
  // Extract codepoint from CDN filename pattern - just show the file image
  return "";
}

const CDN_BASE = "https://cdn.jsdelivr.net/gh/twitter/twemoji@14.0.2/assets/72x72";

const html = `<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="UTF-8">
<title>Face Emoji Preview - ${files.length} emojis</title>
<style>
  body { font-family: -apple-system, "Segoe UI", Roboto, sans-serif; max-width: 1100px; margin: 0 auto; padding: 20px; background: #1a1a2e; color: #eee; }
  h1 { text-align: center; color: #fff; }
  .info { text-align: center; color: #aaa; margin-bottom: 20px; font-size: 14px; }
  .search-box { display: block; width: 100%; max-width: 400px; margin: 0 auto 24px; padding: 10px 16px; font-size: 16px; border: 2px solid #333; border-radius: 8px; background: #16213e; color: #fff; outline: none; }
  .search-box:focus { border-color: #1da1f2; }
  .stats { text-align: center; margin-bottom: 16px; color: #1da1f2; font-size: 13px; }
  .grid { display: grid; grid-template-columns: repeat(auto-fill, minmax(180px, 1fr)); gap: 10px; }
  .card { background: #16213e; border-radius: 10px; padding: 12px; display: flex; align-items: center; gap: 10px; border: 1px solid #0f3460; cursor: pointer; transition: all 0.2s; }
  .card:hover { background: #0f3460; border-color: #1da1f2; transform: translateY(-1px); }
  .card img { width: 52px; height: 52px; flex-shrink: 0; }
  .card .idx { font-size: 10px; color: #555; font-weight: bold; }
  .card .name { font-weight: 600; font-size: 12px; color: #ddd; }
  .card .file { font-family: monospace; font-size: 10px; color: #1da1f2; margin-top: 2px; }
</style>
</head>
<body>
<h1>Face Emojis (${files.length})</h1>
<p class="info">Twemoji 14.0 Face & Expression emojis | Click card to copy filename</p>
<input type="text" class="search-box" id="search" placeholder="Search name or index..." oninput="filter()" />
<div class="stats" id="stats">Showing ${files.length} emojis</div>
<div class="grid" id="grid">
${files.map((f, i) => {
  const name = NAMES[i] || basename(f, ".png");
  const localImg = `../${INPUT_DIR}/${f}`;
  return `<div class="card" data-name="${name.toLowerCase()}" onclick="copy('${f}')">
    <img src="${localImg}" alt="${name}" loading="lazy" />
    <div>
      <div class="idx">#${String(i).padStart(2, '0')}</div>
      <div class="name">${name}</div>
      <div class="file">${f}</div>
    </div>
  </div>`;
}).join("\n")}
</div>
<script>
function filter() {
  const q = document.getElementById('search').value.toLowerCase();
  let n = 0;
  document.querySelectorAll('.card').forEach(c => {
    const show = c.dataset.name.includes(q) || c.innerHTML.includes(q);
    c.style.display = show ? '' : 'none';
    if (show) n++;
  });
  document.getElementById('stats').textContent = 'Showing ' + n + ' emojis';
}
function copy(text) {
  navigator.clipboard.writeText(text);
  document.title = 'Copied: ' + text;
  setTimeout(() => document.title = 'Face Emojis (${files.length})', 800);
}
</script>
</body>
</html>`;

writeFileSync(OUTPUT_FILE, html);
console.log(`Generated: ${OUTPUT_FILE} (${files.length} emojis)`);
console.log("Open in browser to preview all face emojis.");
