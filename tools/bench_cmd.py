#!/usr/bin/env python3
"""
Send bench commands and print the replies, over the serial port (ESC1) or RTT through the ST-Link (GD32 board).

  python tools/bench_cmd.py --port rtt --env gd32f130 X D S          # read-only: safety, dead-time, sensor state
  python tools/bench_cmd.py --port COM8 D X
  python tools/bench_cmd.py --port rtt --env gd32f130 --wait "Measured compensation" 40 DC1.5   # calibration

Commands that move the motor (ME1, DC..., SA) are allowed but the motor is always disabled at the end (ME0).
--wait PATTERN TIMEOUT waits for PATTERN in the reply of the LAST command (long commands such as DC).
"""
import argparse
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from board_mem import BoardMemory, RttPort  # noqa: E402

MOVING = re.compile(r"^(ME1|DC|SA)", re.IGNORECASE)


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--port", required=True, help='serial port (ESC1) or "rtt" (GD32 board, through the ST-Link)')
    p.add_argument("--env", default="esc1", help="PlatformIO environment of the flashed firmware (for the ELF)")
    p.add_argument("--elf", default=None, help="firmware ELF (default .pio/build/<env>/firmware.elf)")
    p.add_argument("--wait", nargs=2, metavar=("PATTERN", "TIMEOUT"), default=None,
                   help="wait for PATTERN (regex) in the reply of the last command, at most TIMEOUT seconds")
    p.add_argument("--reply-time", type=float, default=0.8, help="time to collect each reply [s]")
    p.add_argument("commands", nargs="+")
    a = p.parse_args()
    elf = a.elf or os.path.join(".pio", "build", a.env, "firmware.elf")
    moving = any(MOVING.match(c) for c in a.commands)

    def run(port):
        def read_for(t, pattern=None):
            end, out = time.time() + t, ""
            while time.time() < end:
                out += port.read(4096).decode(errors="replace")
                if pattern and re.search(pattern, out):
                    time.sleep(0.5)
                    out += port.read(4096).decode(errors="replace")
                    break
                time.sleep(0.05)
            return out

        try:
            time.sleep(0.2)
            port.reset_input_buffer()
            for k, c in enumerate(a.commands):
                port.write((c + "\n").encode())
                last = k == len(a.commands) - 1
                if last and a.wait:
                    out = read_for(float(a.wait[1]), a.wait[0])
                else:
                    out = read_for(a.reply_time)
                print(f"--- {c}:")
                print(out.strip())
        finally:
            if moving:
                port.write(b"ME0\n")
                time.sleep(0.3)
                print("--- ME0 sent (motor disabled)")

    if a.port.lower() == "rtt":
        with BoardMemory(elf) as mem:
            run(RttPort(mem, timeout=0.1))
    else:
        import serial
        ser = serial.Serial(a.port, 115200, timeout=0.1)
        try:
            run(ser)
        finally:
            ser.close()


if __name__ == "__main__":
    main()
