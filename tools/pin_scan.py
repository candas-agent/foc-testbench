#!/usr/bin/env python3
"""
Pin scan of an unknown GD32F130 / GD32E230 QFN32 board (firmware src/pinscan, envs pinscan_*, simplefoc-work #39).
SAFETY: main supply and battery disconnected, board on the ST-Link 3.3 V only. The firmware never drives a pin,
it only switches the internal pull-up / pull-down.
  python tools/pin_scan.py --elf <elf>              # classify again and print the table
  python tools/pin_scan.py --elf <elf> --all        # every pin toggles its pull at its own frequency
  python tools/pin_scan.py --elf <elf> --pin PA7    # only PA7 toggles (confirmation)
  python tools/pin_scan.py --elf <elf> --stop       # back to idle (all pull-down)
Frequency on the scope -> pin: (base + n) * 100 Hz, base A=10, B=30, F=50, C=60 (PA7 1.7 kHz, PB1 3.1 kHz, PF0 5.0 kHz).
Classes: "free" = follows the pull (nothing drives it: gate driver input, unused pin, connector),
"low" / "high" = held by something stronger than ~40 kOhm (op amp output, divider, pull resistor, ground...).
"""
import argparse
import glob
import os
import re
import struct
import subprocess
import sys
import time

GDB = sorted(glob.glob(os.path.expanduser("~/.platformio/packages/toolchain-gccarmnoneeabi*/bin/arm-none-eabi-gdb.exe")))
MAGIC = 0x5C4A0001
MAX = 40
OFF = {"magic": 0, "mode": 4, "only": 8, "done": 12, "count": 16, "port": 20, "bit": 20 + MAX,
       "rd_down": 20 + 2 * MAX, "rd_up": 20 + 3 * MAX, "adc_mv": 20 + 4 * MAX, "freq_hz": 20 + 6 * MAX,
       "tick_hz": 20 + 8 * MAX}
SIZE = 24 + 8 * MAX


def scan_address(elf):
    out = subprocess.run([GDB[0], "-batch", "-ex", "print &scan", elf], capture_output=True, text=True).stdout
    m = re.search(r"\) (0x[0-9a-f]+) <", out)
    if not m:
        sys.exit("gdb could not resolve &scan:\n" + out)
    return int(m.group(1), 16)


def read_block(t, adr):
    return bytes(t.read_memory_block8(adr, SIZE))


def table(b):
    count = struct.unpack_from("<I", b, OFF["count"])[0]
    tick = struct.unpack_from("<I", b, OFF["tick_hz"])[0]
    print(f"time base {tick} Hz (measured against delay(), toggle periods computed from it)")
    print(f"{'pin':5} {'pull-down':>9} {'pull-up':>7}  {'class':6} {'ADC mV':>7} {'toggle':>9}")
    for i in range(count):
        name = f"P{chr(b[OFF['port'] + i])}{b[OFF['bit'] + i]}"
        d, u = b[OFF["rd_down"] + i], b[OFF["rd_up"] + i]
        cls = "free" if (d, u) == (0, 1) else "low" if (d, u) == (0, 0) else "high" if (d, u) == (1, 1) else "odd"
        mv = struct.unpack_from("<H", b, OFF["adc_mv"] + 2 * i)[0]
        f = struct.unpack_from("<H", b, OFF["freq_hz"] + 2 * i)[0]
        print(f"{name:5} {d:>9} {u:>7}  {cls:6} {'' if mv == 0xFFFF else mv:>7} {f / 1000:>7.1f} kHz")


def pin_index(b, pin):
    count = struct.unpack_from("<I", b, OFF["count"])[0]
    for i in range(count):
        if f"P{chr(b[OFF['port'] + i])}{b[OFF['bit'] + i]}" == pin.upper():
            return i
    sys.exit(f"{pin} is not in the scanned pins")


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--elf", required=True)
    g = p.add_mutually_exclusive_group()
    g.add_argument("--all", action="store_true")
    g.add_argument("--pin")
    g.add_argument("--stop", action="store_true")
    a = p.parse_args()
    adr = scan_address(a.elf)

    from pyocd.core.helpers import ConnectHelper
    session = ConnectHelper.session_with_chosen_probe(connect_mode="attach", target_override="cortex_m",
                                                      frequency=4_000_000, options={"resume_on_disconnect": True})
    if session is None:
        sys.exit("no debug probe found")
    with session:
        t = session.board.target
        b = read_block(t, adr)
        if struct.unpack_from("<I", b, OFF["magic"])[0] != MAGIC:
            sys.exit("pin scan firmware not found (magic)")
        if a.all:
            t.write32(adr + OFF["mode"], 1)
            print("all pins toggle; frequency -> pin: (base + n) * 100 Hz, base A=10, B=30, F=50. --stop to end")
        elif a.pin:
            i = pin_index(b, a.pin)
            t.write32(adr + OFF["mode"], 0)
            time.sleep(0.05)
            t.write32(adr + OFF["only"], i)
            t.write32(adr + OFF["mode"], 2)
            f = struct.unpack_from("<H", b, OFF["freq_hz"] + 2 * i)[0]
            print(f"only {a.pin.upper()} toggles, {f / 1000:.1f} kHz. --stop to end")
        elif a.stop:
            t.write32(adr + OFF["mode"], 0)
            print("idle, all pins pull-down")
        else:
            done = struct.unpack_from("<I", b, OFF["done"])[0]
            t.write32(adr + OFF["mode"], 3)
            for _ in range(100):
                time.sleep(0.05)
                b = read_block(t, adr)
                if struct.unpack_from("<I", b, OFF["done"])[0] != done:
                    break
            else:
                sys.exit("no new classification (firmware busy in a toggle mode? use --stop first)")
            table(b)


if __name__ == "__main__":
    main()
