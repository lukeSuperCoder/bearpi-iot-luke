#!/usr/bin/env python3
"""
Send emoji number to BearPi board via UART.
Usage:
    python3 send_emoji.py 5          # single-shot: display emoji #5
    python3 send_emoji.py -i         # interactive mode
    python3 send_emoji.py -p /dev/tty.usbmodem12345 5
"""

import argparse
import sys
import time

try:
    import serial
    import serial.tools.list_ports
except ImportError:
    print("Error: pyserial not installed. Run: pip install pyserial", file=sys.stderr)
    sys.exit(1)

DEFAULT_BAUD = 115200
EMOJI_COUNT = 20


def find_serial_port():
    """Auto-detect BearPi USB serial port (macOS/Linux)."""
    for p in serial.tools.list_ports.comports():
        if "usbmodem" in p.device or "ttyACM" in p.device or "ttyUSB" in p.device:
            return p.device
    return None


def send_emoji(port, baud, number):
    """Send emoji number and return the board's response."""
    ser = serial.Serial(port, baud, timeout=2.0)
    time.sleep(0.1)
    ser.reset_input_buffer()
    ser.write(f"{number}\n".encode("ascii"))
    response = ser.readline().decode("ascii", errors="replace").strip()
    ser.close()
    return response


def interactive_mode(port, baud):
    """Interactive REPL: type a number to display an emoji."""
    ser = serial.Serial(port, baud, timeout=2.0)
    time.sleep(0.1)
    ser.reset_input_buffer()
    print(f"Connected to {port} @ {baud} baud. Type 1-{EMOJI_COUNT}, or 'q' to quit.")

    while True:
        try:
            user_input = input("emoji> ").strip()
        except (EOFError, KeyboardInterrupt):
            print("\nBye!")
            break

        if user_input.lower() in ("q", "quit", "exit"):
            print("Bye!")
            break

        if not user_input:
            continue

        ser.write(f"{user_input}\n".encode("ascii"))
        response = ser.readline().decode("ascii", errors="replace").strip()
        if response:
            print(f"  <- {response}")
        else:
            print("  <- (no response, timeout)")

    ser.close()


def main():
    parser = argparse.ArgumentParser(description="Send emoji number to BearPi via UART")
    parser.add_argument("number", nargs="?", type=int, help="Emoji number 1-20")
    parser.add_argument("-p", "--port", default=None, help="Serial port (auto-detect if omitted)")
    parser.add_argument("-b", "--baud", type=int, default=DEFAULT_BAUD, help="Baud rate (default: 115200)")
    parser.add_argument("-i", "--interactive", action="store_true", help="Interactive mode")
    args = parser.parse_args()

    port = args.port or find_serial_port()
    if not port:
        print("Error: No serial port found. Use -p to specify.", file=sys.stderr)
        sys.exit(1)

    if args.interactive or args.number is None:
        interactive_mode(port, args.baud)
    else:
        if args.number < 1 or args.number > EMOJI_COUNT:
            print(f"Error: Number must be 1-{EMOJI_COUNT}, got {args.number}", file=sys.stderr)
            sys.exit(1)
        print(f"Connected to {port}")
        response = send_emoji(port, args.baud, args.number)
        print(response)


if __name__ == "__main__":
    main()
