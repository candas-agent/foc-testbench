#!/usr/bin/env python3
"""
Live reading of the test bench variables (dbg structure) with pyOCD, saved to CSV.
Does not move the motor by itself: it only reads the memory of the running firmware.

  python tools/live.py --duration 5 --out live.csv
  python tools/live.py --fields ia angle_err --duration 2 --print
"""
import argparse
import csv
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from board_mem import BoardMemory, DBG_FIELDS  # noqa: E402


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--elf", default=os.path.join(".pio", "build", "esc1", "firmware.elf"))
    p.add_argument("--duration", type=float, default=5.0, help="[s]")
    p.add_argument("--fields", nargs="+", default=[f for f in DBG_FIELDS if f != "magic"])
    p.add_argument("--out", default="live.csv")
    p.add_argument("--print", action="store_true", help="also print the values")
    a = p.parse_args()

    unknown = set(a.fields) - set(DBG_FIELDS)
    if unknown:
        sys.exit(f"unknown fields {sorted(unknown)}, available: {DBG_FIELDS}")

    rows = []
    with BoardMemory(a.elf) as mem:
        t0 = time.perf_counter()
        while (t := time.perf_counter() - t0) < a.duration:
            d = mem.read_dbg()
            rows.append([f"{t:.6f}"] + [d[f] for f in a.fields])
            if a.print:
                print(" ".join(f"{f}={d[f]:.4g}" for f in a.fields))

    with open(a.out, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["t_s"] + a.fields)
        w.writerows(rows)
    print(f"{len(rows)} samples in {a.duration:g} s ({len(rows) / a.duration:.0f} Hz) -> {a.out}")


if __name__ == "__main__":
    main()
