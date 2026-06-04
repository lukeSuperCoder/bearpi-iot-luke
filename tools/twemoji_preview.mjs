/**
 * Generate a local HTML preview page for Twemoji 72x72 assets.
 * Maps Unicode codepoint filenames to emoji characters and names.
 *
 * Usage: node tools/twemoji_preview.mjs
 * Then open tools/twemoji_preview.html in your browser.
 */

import { writeFileSync } from "fs";

// Unicode CLDR short names for common emoji (covering most frequently used)
// Format: "codepoint": "name"
const EMOJI_NAMES = {
  // Smileys & Emotion
  "1f600": "Grinning Face", "1f603": "Smiling with Open Eyes", "1f604": "Smiling with Open Mouth & Eyes",
  "1f601": "Beaming Face", "1f606": "Grinning Squinting", "1f605": "Grinning with Sweat",
  "1f923": "Rolling on Floor Laughing", "1f602": "Face with Tears of Joy", "1f642": "Slightly Smiling",
  "1f643": "Upside-Down Face", "1f609": "Winking Face", "1f60a": "Smiling with Hearts",
  "1f607": "Halo Face", "1f970": "Smiling with Heart-Eyes", "1f60d": "Heart Eyes",
  "1f929": "Star-Struck", "1f618": "Face Blowing a Kiss", "1f617": "Kissing Face",
  "1f61a": "Kissing Face with Closed Eyes", "1f619": "Kissing Smiling Eyes",
  "1f972": "Smiling Face with Tear", "1f60b": "Savoring Food", "1f61b": "Face with Tongue",
  "1f61c": "Winking with Tongue", "1f61d": "Squinting with Tongue", "1f911": "Money-Mouth Face",
  "1f917": "Hugging Face", "1f92d": "Face with Hand Over Mouth", "1f92b": "Shushing Face",
  "1f914": "Thinking Face", "1f910": "Zipper-Mouth Face", "1f928": "Face with Raised Eyebrow",
  "1f610": "Neutral Face", "1f611": "Expressionless", "1f636": "Face Without Mouth",
  "1f60f": "Smirking Face", "1f612": "Unamused Face", "1f644": "Rolling Eyes",
  "1f62c": "Grimacing Face", "1f62b": "Weary Face", "1f925": "Lying Face",
  "1f60c": "Relieved Face", "1f614": "Pensive Face", "1f62a": "Sleepy Face",
  "1f924": "Drooling Face", "1f634": "Sleeping Face", "1f637": "Face with Medical Mask",
  "1f912": "Face with Thermometer", "1f915": "Face with Head-Bandage", "1f922": "Nauseated Face",
  "1f92e": "Face Vomiting", "1f927": "Sneezing Face", "1f975": "Hot Face", "1f976": "Cold Face",
  "1f974": "Woozy Face", "1f635": "Dizzy Face", "1f92f": "Exploding Head",
  "1f620": "Angry Face", "1f608": "Angry with Horns", "1f621": "Pouting Face",
  "1f92c": "Face with Symbols on Mouth", "1f63a": "Smiling Cat", "1f638": "Grinning Cat with Eyes",
  "1f639": "Cat with Tears of Joy", "1f63b": "Smiling Cat with Heart-Eyes",
  "1f63c": "Cat with Wry Smile", "1f63d": "Kissing Cat", "1f640": "Weary Cat",
  "1f63f": "Crying Cat", "1f63e": "Pouting Cat", "1f480": "Skull", "1f4a9": "Pile of Poo",
  "1f479": "Ogre", "1f47a": "Goblin", "1f47b": "Ghost", "1f47d": "Alien",
  "1f47e": "Alien Monster", "1f916": "Robot", "1f633": "Flushed Face", "1f62e": "Surprised Face",
  "1f62f": "Hushed Face", "1f632": "Astonished Face", "1f631": "Face Screaming in Fear",
  "1f630": "Anxious Face with Sweat", "1f625": "Sad but Relieved", "1f622": "Crying Face",
  "1f62d": "Loudly Crying Face", "1f624": "Persevering Face", "1f629": "Tired Face",
  "1f62e-200d-1f4a8": "Face Exhaling", "1f623": "Triumph Face", "1f61e": "Disappointed Face",
  "1f61f": "Worried Face", "1f626": "Frowning with Open Mouth", "1f627": "Anguished Face",
  "1f628": "Fearful Face", "1f613": "Downcast Face with Sweat", "1f61d": "Squinting Face",
  "1f60e": "Cool Face (Sunglasses)", "1f913": "Nerd Face", "1f978": "Disguised Face",
  "1f921": "Clown Face",

  // People & Body
  "1f44b": "Waving Hand", "1f91a": "Raised Back of Hand", "1f590": "Splayed Fingers",
  "270c": "Victory Hand", "1f596": "Vulcan Salute", "1f44c": "OK Hand",
  "1f44d": "Thumbs Up", "1f44e": "Thumbs Down", "1f44a": "Oncoming Fist",
  "270a": "Raised Fist", "1f91b": "Left-Facing Fist", "1f91c": "Right-Facing Fist",
  "1f44f": "Clapping Hands", "1f64c": "Raising Hands", "1f90f": "Pinched Fingers",
  "1f91d": "Handshake", "1f64f": "Folded Hands (Pray)", "1f485": "Nail Polish",
  "1f919": "Call Me Hand", "1f91e": "Crossed Fingers", "1f4aa": "Flexed Biceps",
  "1f9be": "Mechanical Arm", "1f9bf": "Mechanical Leg", "1f9b5": "Ear",
  "1f9b6": "Foot", "1f442": "Ear", "1f443": "Nose", "1f444": "Mouth",
  "1f445": "Biting Lip", "1f441": "Eye", "1f440": "Eyes", "1f464": "Brain",
  "1f421": "Anatomical Heart", "1f9ec": "DNA",

  // Hearts & Symbols
  "2764": "Red Heart", "1f9e1": "Orange Heart", "1f49b": "Yellow Heart",
  "1f49a": "Green Heart", "1f499": "Blue Heart", "1f49c": "Purple Heart",
  "1f90e": "Brown Heart", "1f5a4": "Black Heart", "1f90d": "White Heart",
  "1f494": "Broken Heart", "2763": "Heart Exclamation", "1f495": "Two Hearts",
  "1f49e": "Revolving Hearts", "1f493": "Beating Heart", "1f497": "Growing Heart",
  "1f496": "Sparkling Heart", "1f498": "Heart with Arrow", "1f49d": "Heart with Ribbon",
  "1f49f": "Heart Decoration", "262e": "Peace Symbol", "271d": "Latin Cross",
  "262a": "Star and Crescent", "1f549": "Om", "1f54e": "Menorah", "2638": "Wheel of Dharma",
  "2721": "Star of David", "262f": "Yin Yang", "1f4ae": "White Flower",
  "1f4af": "Hundred Points", "2728": "Sparkles", "1f517": "Link Symbol",
  "1f531": "Trident Emblem", "1f4a1": "Light Bulb", "1f4a0": "Diamond Dot",
  "1f4a2": "Anger Symbol", "1f4a3": "Bomb", "1f4a4": "Zzz (Sleep)",
  "1f4a5": "Collision", "1f4a6": "Sweat Droplets", "1f4a7": "Droplet",
  "1f4a8": "Dashing Away", "1f4ab": "Dizzy", "1f4ac": "Speech Balloon",
  "1f4ad": "Thought Balloon", "1f5e8": "Left Speech Bubble", "1f3ab": "Ticket",

  // Animals & Nature
  "1f436": "Dog Face", "1f431": "Cat Face", "1f42d": "Mouse Face", "1f439": "Hamster",
  "1f430": "Rabbit Face", "1f43b": "Bear Face", "1f98c": "Deer", "1f42f": "Tiger Face",
  "1f437": "Pig Face", "1f43c": "Panda Face", "1f435": "Monkey Face", "1f412": "Monkey",
  "1f98a": "Fox", "1f9a1": "Beaver", "1f98d": "Gorilla", "1f99d": "Raccoon",
  "1f981": "Lion", "1f42e": "Cow Face", "1f417": "Boar", "1f434": "Horse Face",
  "1f993": "Zebra", "1f98c": "Deer", "1f42c": "Dolphin", "1f41f": "Fish",
  "1f420": "Tropical Fish", "1f421": "Blowfish", "1f988": "Shark", "1f40b": "Whale",
  "1f98b": "Butterfly", "1f40c": "Snail", "1f41b": "Bug", "1f41c": "Ant",
  "1f41d": "Honeybee", "1f41e": "Lady Beetle", "1f40e": "Horse", "1f982": "Scorpion",
  "1f338": "Cherry Blossom", "1f490": "Bouquet", "1f33a": "Hibiscus", "1f33b": "Sunflower",
  "1f33c": "Blossom", "1f337": "Tulip", "1f340": "Four Leaf Clover", "1f339": "Rose",
  "1f343": "Leaf Fluttering in Wind", "1f341": "Maple Leaf", "1f342": "Fallen Leaf",
  "1f33f": "Herb", "1f33e": "Ear of Rice", "1f344": "Mushroom", "1f335": "Cactus",

  // Food & Drink
  "1f347": "Grapes", "1f348": "Melon", "1f349": "Watermelon", "1f34a": "Tangerine",
  "1f34b": "Lemon", "1f34c": "Banana", "1f34d": "Pineapple", "1f34e": "Red Apple",
  "1f351": "Peach", "1f352": "Cherries", "1f353": "Strawberry", "1f345": "Tomato",
  "1f35e": "Bread", "1f357": "Poultry Leg", "1f356": "Meat on Bone", "1f354": "Hamburger",
  "1f35f": "French Fries", "1f355": "Slice of Pizza", "1f32d": "Hot Dog",
  "1f369": "Doughnut", "1f370": "Shortcake", "1f36a": "Cookie", "1f36b": "Chocolate Bar",
  "1f366": "Ice Cream", "1f367": "Shaved Ice", "1f368": "Ice Cream (Soft)",
  "1f36d": "Lollipop", "1f36e": "Custard", "1f37f": "Popcorn", "1f362": "Oden",
  "1f363": "Sushi", "1f371": "Bento Box", "1f373": "Cooking", "1f372": "Pot of Food",
  "1f374": "Fork and Knife", "1f375": "Teacup", "1f376": "Sake", "1f377": "Wine Glass",
  "1f378": "Cocktail Glass", "1f379": "Tropical Drink", "1f37a": "Beer Mug",
  "1f37b": "Clinking Beer Mugs", "2615": "Hot Beverage", "1f3c6": "Trophy",
  "1f3c5": "Sports Medal", "1f947": "1st Place Medal", "1f948": "2nd Place Medal",
  "1f949": "3rd Place Medal",

  // Objects
  "1f4bb": "Laptop", "1f5a5": "Desktop Computer", "1f5a8": "Printer",
  "1f4f1": "Mobile Phone", "1f4f2": "Mobile Phone with Arrow",
  "1f4bb": "Keyboard", "1f4bd": "Computer Disk", "1f4be": "Floppy Disk",
  "1f4bf": "Optical Disk", "1f4c0": "DVD", "1f3a5": "Movie Camera",
  "1f4f7": "Camera", "1f4f9": "Video Camera", "1f4fa": "Television",
  "1f4fb": "Radio", "1f50a": "Speaker High Volume", "1f507": "Speaker Muted",
  "1f508": "Speaker Low Volume", "1f50d": "Magnifying Glass Left",
  "1f50e": "Magnifying Glass Right", "1f512": "Lock", "1f513": "Open Lock",
  "1f50f": "Lock with Pen", "1f510": "Locked with Key", "1f511": "Key",
  "1f527": "Wrench", "1f528": "Hammer", "1f6e0": "Hammer and Wrench",
  "1f529": "Nut and Bolt", "1f4a1": "Light Bulb", "1f526": "Flashlight",
  "1f4dc": "Scroll", "1f4d6": "Open Book", "1f4d3": "Notebook",
  "1f4da": "Books", "1f4d2": "Ledger", "1f4c3": "Page with Curl",
  "1f4c4": "Page Facing Up", "1f4c5": "Calendar", "1f4c6": "Tear-Off Calendar",
  "1f4c7": "Card Index", "1f4c8": "Chart Up", "1f4c9": "Chart Down",
  "1f4ca": "Bar Chart", "1f4cb": "Clipboard", "1f4cc": "Pushpin",
  "1f4ce": "Paperclip", "1f587": "Linked Paperclips", "1f4cf": "Straight Ruler",
  "1f4d0": "Triangular Ruler", "2702": "Scissors", "1f3a8": "Artist Palette",
  "1f3ac": "Clapper Board", "1f3a4": "Microphone", "1f3a7": "Headphone",
  "1f3b5": "Musical Note", "1f3b6": "Musical Notes", "1f3b9": "Musical Keyboard",
  "1f3bb": "Violin", "1f3ba": "Trumpet", "1f3b7": "Saxophone", "1f3b8": "Guitar",

  // Travel & Places
  "1f30d": "Earth (Europe/Africa)", "1f30e": "Earth (Americas)", "1f30f": "Earth (Asia/Australia)",
  "1f315": "Full Moon", "1f319": "Crescent Moon", "1f31f": "Glowing Star",
  "2b50": "Star", "1f31e": "Sun with Face", "1f308": "Rainbow",
  "1f4a8": "Dashing Away", "2601": "Cloud", "1f324": "Sun Behind Cloud",
  "1f326": "Sun Behind Rain Cloud", "1f327": "Cloud with Rain",
  "1f328": "Cloud with Snow", "1f329": "Cloud with Lightning",
  "1f32a": "Cloud with Tornado", "1f32b": "Fog", "1f32c": "Wind Face",
  "1f3d4": "Snow-Capped Mountain", "1f5fb": "Mount Fuji", "1f3d6": "Beach with Umbrella",
  "1f3d5": "Camping", "1f3d3": "Stadium", "1f3f0": "Castle",
  "1f3ef": "Japanese Castle", "1f3e0": "House", "1f3e1": "House with Garden",
  "1f3e2": "Office Building", "1f3e5": "Hospital", "1f3e6": "Bank",
  "1f3e8": "Hotel", "1f3ea": "Convenience Store", "1f3eb": "School",
  "1f30b": "Volcano", "1f5fc": "Tokyo Tower", "1f5fd": "Statue of Liberty",
  "26f2": "Fountain", "1f6d5": "Hindu Temple", "1f6d6": "Hut",

  // Transport
  "1f680": "Rocket", "1f6f8": "Rocket (Flying Saucer)", "2708": "Airplane",
  "1f681": "Helicopter", "1f698": "Sport Utility Vehicle", "1f697": "Automobile",
  "1f699": "Recreational Vehicle", "1f69a": "Delivery Truck", "1f69b": "Articulated Lorry",
  "1f692": "Fire Engine", "1f691": "Ambulance", "1f693": "Police Car",
  "1f695": "Taxi", "1f68c": "Bus", "1f683": "Railway Car", "1f684": "High-Speed Train",
  "1f685": "Bullet Train", "1f6b2": "Bicycle", "1f6f5": "Motor Scooter",
  "1f3cd": "Motorcycle", "1f6a8": "Police Car Light", "1f6a5": "Horizontal Traffic Light",
  "1f6a6": "Vertical Traffic Light", "1f6a7": "Construction", "26fd": "Fuel Pump",
  "1f6bf": "Shower", "1f6c1": "Bathtub", "1f6c0": "Person Taking Bath",
  "1f6bd": "Toilet", "1f514": "Bell", "1f515": "Bell with Slash",

  // Sky & Weather
  "1f31a": "New Moon Face", "1f31b": "First Quarter Moon Face",
  "1f31c": "Last Quarter Moon Face", "1f321": "Thermometer",

  // Flags & Symbols
  "1f6a9": "Triangular Flag", "1f3c1": "Chequered Flag", "1f389": "Party Popper",
  "1f38a": "Confetti Ball", "1f38b": "Tanabata Tree", "1f380": "Ribbon",
  "1f381": "Wrapped Gift", "1f382": "Birthday Cake", "1f384": "Christmas Tree",
  "1f386": "Fireworks", "1f387": "Sparkler", "1f9e7": "Red Envelope",

  // Activities & Sports
  "26bd": "Soccer Ball", "1f3c0": "Basketball", "1f3c8": "American Football",
  "26be": "Baseball", "1f3be": "Tennis", "1f3d0": "Volleyball",
  "1f3c9": "Rugby Football", "1f3b1": "Pool 8 Ball", "1f3af": "Direct Hit",
  "1f3ae": "Joystick", "1f3b0": "Slot Machine", "1f3b2": "Game Die",
  "1f0cf": "Joker", "1f004": "Mahjong Tile", "265f": "Chess Pawn",
  "1f3b3": "Bowling", "26f3": "Flag in Hole", "1f3f8": "Badminton",
  "1f94a": "Boxing Glove", "1f94b": "Martial Arts Uniform", "1f938": "Person Cartwheeling",
  "1f3c4": "Person Surfing", "1f3ca": "Person Swimming", "1f3cb": "Person Lifting Weights",
  "1f93a": "Person Fencing", "26f8": "Ice Skate", "1f3bf": "Skis",
  "1f6fc": "Roller Skate", "1f3bd": "Running Shirt",

  // Special / Common
  "1f4af": "Hundred Points", "1f522": "Input Latin Numbers", "1f523": "Input Symbols",
  "1f4f0": "Newspaper", "1f4e2": "Loudspeaker", "1f4e3": "Megaphone",
  "1f4e8": "Incoming Envelope", "1f4e9": "Envelope with Arrow", "1f4ea": "Closed Mailbox",
  "1f4eb": "Open Mailbox", "1f4ec": "Open Mailbox with Raised Flag",
  "1f4ed": "Closed Mailbox with Lowered Flag", "1f4ee": "Postbox",
  "1f4e6": "Package", "1f4e7": "E-Mail", "1f4e5": "Inbox Tray", "1f4e4": "Outbox Tray",
  "1f441-200d-1f5e8": "Eye in Speech Bubble",
  "1f595": "Middle Finger", "1f518": "Radio Button", "2611": "Check Box with Check",
  "2612": "Ballot Box with X", "2714": "Heavy Check Mark", "274c": "Cross Mark",
  "274e": "Cross Mark Button", "2753": "Question Mark", "2757": "Exclamation Mark",
  "203c": "Double Exclamation", "2049": "Exclamation Question", "3030": "Wavy Dash",
  "1f4b0": "Money Bag", "1f4b2": "Heavy Dollar Sign", "1f4b5": "Dollar Banknote",
  "1f4b4": "Yen Banknote", "1f4b6": "Euro Banknote", "1f4b7": "Pound Banknote",
  "1f4b3": "Credit Card", "1f4b8": "Money with Wings", "1f4b9": "Chart Increasing with Yen",

  // Skin tone modifiers note: base emoji + 1f3fb/1f3fc/1f3fd/1f3fe/1f3ff
  "1f3fb": "Light Skin Tone", "1f3fc": "Medium-Light Skin Tone",
  "1f3fd": "Medium Skin Tone", "1f3fe": "Medium-Dark Skin Tone", "1f3ff": "Dark Skin Tone",

  // Country flags use regional indicator pairs (1f1e6-1f1ff)
  "1f1e6": "Regional Indicator A", "1f1e7": "Regional Indicator B",
  "1f1e8": "Regional Indicator C", "1f1e9": "Regional Indicator D",
  "1f1ea": "Regional Indicator E", "1f1eb": "Regional Indicator F",
};

// Convert codepoint string to actual emoji character
function codepointToEmoji(cp) {
  try {
    const codes = cp.split("-").map(c => parseInt(c, 16));
    return String.fromCodePoint(...codes);
  } catch {
    return "";
  }
}

// Generate HTML
function generateHTML() {
  const entries = Object.entries(EMOJI_NAMES).sort((a, b) => {
    const ai = parseInt(a[0].split("-")[0], 16);
    const bi = parseInt(b[0].split("-")[0], 16);
    return ai - bi;
  });

  const html = `<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="UTF-8">
<title>Twemoji Preview - Emoji Codepoint Reference</title>
<style>
  body { font-family: -apple-system, "Segoe UI", Roboto, sans-serif; max-width: 1000px; margin: 0 auto; padding: 20px; background: #f5f5f5; }
  h1 { text-align: center; color: #333; }
  .info { text-align: center; color: #666; margin-bottom: 20px; }
  .search-box { display: block; width: 100%; max-width: 400px; margin: 0 auto 20px; padding: 10px 16px; font-size: 16px; border: 2px solid #ddd; border-radius: 8px; outline: none; }
  .search-box:focus { border-color: #1da1f2; }
  .grid { display: grid; grid-template-columns: repeat(auto-fill, minmax(200px, 1fr)); gap: 10px; }
  .card { background: white; border-radius: 8px; padding: 12px; display: flex; align-items: center; gap: 10px; border: 1px solid #e0e0e0; transition: box-shadow 0.2s; }
  .card:hover { box-shadow: 0 2px 8px rgba(0,0,0,0.15); }
  .card img { width: 48px; height: 48px; flex-shrink: 0; }
  .card .emoji { font-size: 36px; flex-shrink: 0; width: 48px; text-align: center; }
  .card .info-col { min-width: 0; }
  .card .name { font-weight: 600; font-size: 13px; color: #333; word-wrap: break-word; }
  .card .code { font-family: monospace; font-size: 11px; color: #999; margin-top: 2px; }
  .card .fname { font-size: 10px; color: #1da1f2; margin-top: 1px; }
  .footer { text-align: center; color: #999; margin-top: 30px; font-size: 12px; }
</style>
</head>
<body>
<h1>Twemoji 72x72 Preview</h1>
<p class="info">共 ${entries.length} 个常用 Emoji | 输入关键词搜索 | 点击卡片复制文件名</p>
<input type="text" class="search-box" id="search" placeholder="Search emoji name or codepoint..." />
<div class="grid" id="grid">
${entries.map(([cp, name]) => {
  const emoji = codepointToEmoji(cp);
  const twemojiUrl = `https://cdn.jsdelivr.net/gh/twitter/twemoji@14.0.2/assets/72x72/${cp}.png`;
  return `<div class="card" data-name="${name.toLowerCase()}" data-cp="${cp}" onclick="copyFilename('${cp}.png')" title="Click to copy filename">
    <img src="${twemojiUrl}" alt="${name}" onerror="this.style.display='none';this.nextElementSibling.style.display='block'" />
    <span class="emoji" style="display:none">${emoji}</span>
    <div class="info-col">
      <div class="name">${name}</div>
      <div class="code">U+${cp.toUpperCase().replace(/-/g, ' U+')}</div>
      <div class="fname">${cp}.png</div>
    </div>
  </div>`;
}).join("\n")}
</div>
<p class="footer">Data from Twemoji v14.0 (Unicode 14.0) | CDN: jsdelivr</p>
<script>
document.getElementById('search').addEventListener('input', function(e) {
  const q = e.target.value.toLowerCase();
  document.querySelectorAll('.card').forEach(c => {
    const match = c.dataset.name.includes(q) || c.dataset.cp.includes(q);
    c.style.display = match ? '' : 'none';
  });
});
function copyFilename(text) {
  navigator.clipboard.writeText(text).then(() => {
    const old = document.title;
    document.title = 'Copied: ' + text;
    setTimeout(() => document.title = old, 1000);
  });
}
</script>
</body>
</html>`;
  return html;
}

const outputPath = "tools/twemoji_preview.html";
writeFileSync(outputPath, generateHTML());
console.log(`Generated: ${outputPath}`);
console.log("Open this file in your browser to browse all Twemoji emojis.");
console.log("Each card shows: emoji image, name, Unicode codepoint, and filename.");
console.log("Click any card to copy the filename to clipboard.");
