#!/usr/bin/env python3
"""
Convert emoji PNGs to RGB565 binary files for SD card storage.

Usage:
    python3 tools/emoji_sd_prepare.py /path/to/sd_card
    python3 tools/emoji_sd_prepare.py /path/to/sd_card -i /path/to/custom_pngs
"""

import argparse
import os
import struct
import sys

try:
    from PIL import Image
except ImportError:
    print("Error: Pillow not installed. Run: pip install Pillow", file=sys.stderr)
    sys.exit(1)

IMG_W = 64
IMG_H = 64

# Default names matching tools/emoji_png/ filenames
DEFAULT_NAMES = [
    "Grinning", "Tears of Joy", "Winking", "Heart Eyes", "Cool",
    "Neutral", "Unamused", "Disappointed", "Angry", "Crying",
    "Surprised", "Screaming", "Flushed", "Sleeping", "Dizzy",
    "Money Mouth", "Nerd", "Thinking", "Hugging", "Exploding Head",
]


def png_to_rgb565(img_path):
    """Convert a PNG image to RGB565 big-endian binary data."""
    img = Image.open(img_path).convert("RGBA")
    img = img.resize((IMG_W, IMG_H), Image.LANCZOS)

    data = bytearray(IMG_W * IMG_H * 2)
    for y in range(IMG_H):
        for x in range(IMG_W):
            r, g, b, a = img.getpixel((x, y))
            # Alpha blend with black background
            r = round(r * a / 255)
            g = round(g * a / 255)
            b = round(b * a / 255)
            rgb565 = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
            i = (y * IMG_W + x) * 2
            data[i] = (rgb565 >> 8) & 0xFF
            data[i + 1] = rgb565 & 0xFF

    return bytes(data)


def main():
    parser = argparse.ArgumentParser(description="Prepare SD card with emoji RGB565 files")
    parser.add_argument("sd_path", help="Path to SD card root directory")
    parser.add_argument("-i", "--input", default="tools/emoji_png",
                        help="Input directory containing PNG files (default: tools/emoji_png)")
    args = parser.parse_args()

    input_dir = args.input
    if not os.path.isdir(input_dir):
        print(f"Error: Input directory not found: {input_dir}", file=sys.stderr)
        sys.exit(1)

    emoji_dir = os.path.join(args.sd_path, "emoji")
    os.makedirs(emoji_dir, exist_ok=True)

    png_files = sorted(f for f in os.listdir(input_dir) if f.lower().endswith(".png"))
    if not png_files:
        print(f"No PNG files found in {input_dir}", file=sys.stderr)
        sys.exit(1)

    names = []

    for i, png_file in enumerate(png_files):
        name = DEFAULT_NAMES[i] if i < len(DEFAULT_NAMES) else os.path.splitext(png_file)[0]
        # Clean up name: replace underscores with spaces, capitalize
        name = name.replace("_", " ")
        names.append(name)

        rgb565_data = png_to_rgb565(os.path.join(input_dir, png_file))
        out_path = os.path.join(emoji_dir, f"{i + 1:02d}.rgb565")

        with open(out_path, "wb") as f:
            f.write(rgb565_data)

        print(f"  {png_file} -> {i + 1:02d}.rgb565 ({name})")

    # Write names.txt
    names_path = os.path.join(emoji_dir, "names.txt")
    with open(names_path, "w") as f:
        f.write("\n".join(names) + "\n")

    print(f"\nDone: {len(png_files)} emojis written to {emoji_dir}")
    print(f"  names.txt: {len(names)} entries")
    print(f"  Total size: {len(png_files) * IMG_W * IMG_H * 2} bytes")


if __name__ == "__main__":
    main()
