#!/usr/bin/env python3
"""
Trapezoidal 120 modulation check on the GD32F130 board: THE MOTOR MOVES (open loop, voltage mode).
Sets MWT2 (Trapezoid_120), runs --duration s at --voltage / --speed, then disables the motor and puts SVPWM back
(MWT1). With R_ds_on current sense the floating phase reads nonsense, which trips the bench overcurrent cut: --xi sets
the bench threshold (XI) for the run (ask the user first) and puts XI4 back at the end; this script cuts at --imax on
the two driven phases only. While running, samples the TIMER0 channel enable bits (CHCTL2) and the phase currents with the open loop
electrical angle. Prints how often each phase channel is off and, per group of sectors, the rms current of the phase
that should float there (SimpleFOC trap_120_map: A floats in sectors 0/3, C in 1/4, B in 2/5; ic = -(ia + ib)).
  python tools/trap_test.py --elf .pio/cloud/<run>/gd32drv_ch0/firmware.elf
"""
import argparse
import math
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from board_mem import BoardMemory, RttPort  # noqa: E402

CHCTL2 = 0x40012C20
FLOATING = {0: "A", 3: "A", 1: "C", 4: "C", 2: "B", 5: "B"}


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--elf", required=True)
    p.add_argument("--voltage", type=float, default=1.2)
    p.add_argument("--speed", type=float, default=0.5)
    p.add_argument("--duration", type=float, default=5.0)
    p.add_argument("--imax", type=float, default=3.0, help="software cut on the two driven phases [A]")
    p.add_argument("--xi", type=float, default=None, help="bench overcurrent threshold during the run (restored to 4)")
    a = p.parse_args()

    off = {0: 0, 1: 0, 2: 0}
    over = 0  # consecutive reads above --imax (the angle and the currents are not read at the same instant: near a
    # sector change the phase that just started floating can be taken for a driven one)
    n_reg = 0
    floating = {"A": [], "B": [], "C": []}
    driven = {"A": [], "B": [], "C": []}
    with BoardMemory(a.elf) as mem:
        ser = RttPort(mem, timeout=0.05)

        def send(cmd):
            ser.write((cmd + "\n").encode())
            time.sleep(0.05)

        try:
            if a.xi is not None:
                send(f"XI{a.xi}")
            for cmd in ("ME0", "XC", "XH1", "MWT2", "MC3", "MT0", "DE0", "D0.0000", f"DB{a.voltage}", f"M{a.speed}"):
                send(cmd)
            send("ME1")
            t0 = last_ping = time.perf_counter()
            while (t := time.perf_counter()) - t0 < a.duration:
                if t - last_ping > 0.5:
                    send("P")
                    last_ping = time.perf_counter()
                v = mem.target.read32(CHCTL2)
                n_reg += 1
                for ch in range(3):
                    if not (v >> (4 * ch)) & 1:
                        off[ch] += 1
                d = mem.read_dbg()
                if d["fault"]:
                    print(f"board fault {int(d['fault'])}")
                    break
                ia, ib = d["ia"], d["ib"]
                ic = -(ia + ib)
                angle = d["electrical_angle"]
                sector = int(6 * ((angle + math.pi / 6) % (2 * math.pi)) / (2 * math.pi)) % 6
                cur = {"A": ia, "B": ib, "C": ic}
                f = FLOATING[sector]
                # only the sector where C floats has two valid measured phases (A, B); elsewhere the floating
                # phase reads nonsense: cut on the measured driven phase(s) only
                valid = [cur[ph] for ph in "AB" if ph != f]
                over = over + 1 if max(abs(x) for x in valid) > a.imax else 0
                if over >= 5:
                    print(f"software cut: {max(abs(x) for x in valid):.2f} A in sector {sector} (5 reads in a row)")
                    break
                floating[f].append(cur[f])
                for ph in "ABC":
                    if ph != f:
                        driven[ph].append(cur[ph])
        finally:
            send("ME0")
            send("MWT1")
            if a.xi is not None:
                send("XI4")
            print("motor disabled (ME0), SVPWM back (MWT1)" + (", bench overcurrent back to 4 A (XI4)" if a.xi is not None else ""))

    def rms(x):
        return math.sqrt(sum(v * v for v in x) / len(x)) if x else float("nan")

    print(f"{n_reg} register reads: channel off CH0 {100 * off[0] / n_reg:.0f} %, CH1 {100 * off[1] / n_reg:.0f} %, "
          f"CH2 {100 * off[2] / n_reg:.0f} % of the time (expected ~33 % each)")
    print(f"phase C (ic = -(ia + ib)): rms while it should float {rms(floating['C']):.3f} A "
          f"({len(floating['C'])} reads, sectors 1 and 4: A and B driven, both measurements valid)")


if __name__ == "__main__":
    main()
