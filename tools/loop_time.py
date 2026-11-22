#!/usr/bin/env python3
"""
Loop time while the motor runs (GD32 board, open loop velocity in current mode, as ol_run.py): THE MOTOR MOVES.
Reads the loop profile (XP, firmware built with -D BENCH_PROFILE, mean and max of the last 256 loops per part)
--reads times after --settle seconds, prints the mean of the averages and the largest max of each part,
then disables the motor (ME0). Appends one row per part to --csv.
  python tools/loop_time.py --env gd32f130 --elf .pio/cloud/<run>/gd32drv_pr/firmware.elf --label pr --csv data/x.csv
"""
import argparse
import csv
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from board_mem import BoardMemory, RttPort  # noqa: E402

LINE = re.compile(r"^(\w+): ([\d.]+) / ([\d.]+)\s*$")


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--env", default="gd32f130")
    p.add_argument("--elf", required=True)
    p.add_argument("--current", type=float, default=1.0)
    p.add_argument("--speed", type=float, default=2.0)
    p.add_argument("--settle", type=float, default=2.0)
    p.add_argument("--reads", type=int, default=5)
    p.add_argument("--label", default="")
    p.add_argument("--csv", default=None)
    a = p.parse_args()

    with BoardMemory(a.elf) as mem:
        ser = RttPort(mem, timeout=0.05)

        def send(cmd, wait=0.05):
            ser.write((cmd + "\n").encode())
            time.sleep(wait)

        def reply(wait=0.6):
            out, t0 = b"", time.perf_counter()
            while time.perf_counter() - t0 < wait:
                n = ser.in_waiting
                if n:
                    out += ser.read(n)
                time.sleep(0.02)
            return out.decode(errors="replace")

        parts = {}
        try:
            for cmd in ("ME0", "XC", "XH1", "MC3", "MT2", "DE0", "D0.0000", f"M{a.speed}", f"MLC{a.current}"):
                send(cmd)
            ser.reset_input_buffer()
            send("ME1")
            t0 = time.perf_counter()
            while time.perf_counter() - t0 < a.settle:
                send("P", 0.4)  # heartbeat
                if mem.read_dbg()["fault"]:
                    raise SystemExit("board fault")
            for _ in range(a.reads):
                ser.reset_input_buffer()
                send("XP", 0.0)
                for line in reply().splitlines():
                    m = LINE.match(line.strip().lstrip("<").strip())
                    if m:
                        parts.setdefault(m.group(1), []).append((float(m.group(2)), float(m.group(3))))
                if mem.read_dbg()["fault"]:
                    raise SystemExit("board fault")
        finally:
            send("ME0")
            print("motor disabled (ME0 sent)")

    rows = []
    for name, v in parts.items():
        avg = sum(x[0] for x in v) / len(v)
        mx = max(x[1] for x in v)
        print(f"{name:8s} avg {avg:7.1f} us  max {mx:7.1f} us  ({len(v)} reads)")
        rows.append([a.label, name, f"{avg:.1f}", f"{mx:.1f}", len(v)])
    if a.csv and rows:
        new = not os.path.exists(a.csv)
        with open(a.csv, "a", newline="") as f:
            w = csv.writer(f)
            if new:
                w.writerow(["label", "part", "avg_us", "max_us", "reads"])
            w.writerows(rows)


if __name__ == "__main__":
    main()
