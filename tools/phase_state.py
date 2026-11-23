#!/usr/bin/env python3
"""
GD32F130 board: TIMER0 channel enable bits (CHCTL2) with the motor disabled, enabled (open loop, 0 V, 1 s) and
disabled again. The motor is enabled at 0 V: the bridges switch at 50 %, no torque.
  python tools/phase_state.py --elf .pio/cloud/<run>/gd32drv_ch0/firmware.elf
"""
import argparse
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from board_mem import BoardMemory, RttPort  # noqa: E402

CHCTL2 = 0x40012C20  # TIMER0 channel control register 2 (GD32F1x0)


def decode(v):
    out = []
    for ch in range(4):
        en = (v >> (4 * ch)) & 1
        nen = (v >> (4 * ch + 2)) & 1
        out.append(f"CH{ch}{'+' if en else '-'}{'/N+' if nen else '/N-'}" if ch < 3 else f"CH3{'+' if en else '-'}")
    return " ".join(out)


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--elf", required=True)
    a = p.parse_args()
    with BoardMemory(a.elf) as mem:
        ser = RttPort(mem, timeout=0.05)

        def send(cmd):
            ser.write((cmd + "\n").encode())
            time.sleep(0.1)

        def show(label):
            v = mem.target.read32(CHCTL2)
            print(f"{label:22s} CHCTL2 0x{v:04X}  {decode(v)}")

        try:
            for cmd in ("ME0", "XC", "XH1", "MC3", "MT0", "DE0", "D0.0000", "DB0", "M0"):
                send(cmd)
            show("motor disabled")
            send("ME1")
            time.sleep(1.0)
            show("motor enabled (0 V)")
        finally:
            send("ME0")
        time.sleep(0.2)
        show("motor disabled again")


if __name__ == "__main__":
    main()
