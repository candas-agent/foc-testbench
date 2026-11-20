#!/usr/bin/env python3
"""
Loop cost of a change, measured while running: THE MOTOR MOVES. Velocity mode at --speed, then reads the loop profile
(XP, DWT cycle counter, firmware built with -D BENCH_PROFILE) --reads times; each XP = mean and max of the last 256
loops for each part (loopFOC = sensor update + current sense + FOC + setPhaseVoltage, move = velocity loop).
Prints the mean of the averages and the largest max, and appends one row per part to --csv.

  python tools/perf_run.py --env perf_dev_f103 --label dev --csv data/2026-11-20/perf.csv
  python tools/perf_run.py --env dt_f103 --dt 0.38 --label "+ compensation" --csv data/2026-11-20/perf.csv
"""
import argparse
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from board_mem import BoardMemory, RttPort  # noqa: E402


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--env", required=True)
    p.add_argument("--dt", default=None, help="dead-time compensation [V] (only on firmwares that have it)")
    p.add_argument("--speed", type=float, default=2.0)
    p.add_argument("--settle", type=float, default=3.0)
    p.add_argument("--reads", type=int, default=10)
    p.add_argument("--label", default="")
    p.add_argument("--csv", default=None)
    a = p.parse_args()
    reads = []
    with BoardMemory(os.path.join(".pio", "build", a.env, "firmware.elf")) as mem:
        port = RttPort(mem, timeout=0.1)

        def cmd(c, t=0.3):
            port.write((c + "\n").encode())
            end, out = time.time() + t, ""
            while time.time() < end:
                out += port.read(4096).decode(errors="replace")
                time.sleep(0.02)
            return out

        time.sleep(0.2)
        port.reset_input_buffer()
        setup = ["ME0", "VO", "MLC2", "MT2", "MC1", "M0", "ST0", "MVP0.4", "MVI1", "MVF0.01"]
        if a.dt is not None:
            setup += ["XF0", "DM0", f"D{a.dt}", "DT0.2"]
        for c in setup + [f"M{a.speed}"]:
            cmd(c)
        try:
            cmd("ME1", 0.0)
            t0 = time.time()
            while time.time() - t0 < a.settle:
                d = mem.read_dbg()
                if abs(d["velocity"]) > 20 or d["fault"] or max(abs(d["ia"]), abs(d["ib"])) > 3.5:
                    raise RuntimeError(f"abort: {d}")
            for _ in range(a.reads):
                d = mem.read_dbg()
                if abs(d["velocity"]) > 20 or d["fault"] or max(abs(d["ia"]), abs(d["ib"])) > 3.5:
                    raise RuntimeError(f"abort: {d}")
                out = cmd("XP", 0.5)
                parts = {m.group(1): (float(m.group(2)), float(m.group(3)))
                         for m in re.finditer(r"^(\w+): ([\d.]+) / ([\d.]+)", out, re.M)}
                if "TOTAL" in parts:
                    reads.append(parts)
        finally:
            cmd("ME0", 0.4)
            cmd("M0")
            if a.dt is not None:
                cmd("D0")
            print("X ->", cmd("X", 0.8).strip().replace("\n", " | "))
    names = [n for n in reads[0] if all(n in r for r in reads)]
    print(f"{a.label or a.env}: {len(reads)} reads of 256 loops")
    rows = []
    for n in names:
        avg = sum(r[n][0] for r in reads) / len(reads)
        mx = max(r[n][1] for r in reads)
        print(f"  {n:8s} avg {avg:6.1f} us  max {mx:6.1f} us")
        rows.append((a.label, a.env, a.dt or "", n, f"{avg:.2f}", f"{mx:.1f}", len(reads)))
    if a.csv:
        new = not os.path.exists(a.csv)
        with open(a.csv, "a") as f:
            if new:
                f.write("label,env,dt,part,avg_us,max_us,reads\n")
            for r in rows:
                f.write(",".join(str(x) for x in r) + "\n")


if __name__ == "__main__":
    main()
