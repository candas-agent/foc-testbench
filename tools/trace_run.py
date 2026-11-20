#!/usr/bin/env python3
"""
Record a time trace of one velocity run: THE MOTOR MOVES. Every dbg read is logged (time, hall state, hall_count,
iq, id, velocity estimate, ...) so tools/plot_trace.py can draw speed and current against time.
With a firmware that has the hall edge log, every edge is also saved with its exact board time (<out>_edges.csv):
plot_trace.py then computes the speed from those times instead of the PC polling (~370 reads/s).

Same settings as lowspeed_campaign.py: SmoothingSensor (ST0), foc_current, velocity PI P 0.4 I 1 Tf 0.01,
dead-time compensation D<v> (DM0, threshold 0.2 A). Run it once per firmware / compensation:

  python tools/trace_run.py --env dt_f103 --dt 0 --label dev --out data/2026-11-20/trace_dev.csv
"""
import argparse
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from board_mem import BoardMemory, RttPort, DBG_FIELDS  # noqa: E402


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--env", default="combo_f103")
    p.add_argument("--dt", default="0", help="dead-time compensation [V]")
    p.add_argument("--speed", type=float, default=2.0)
    p.add_argument("--duration", type=float, default=12.0)
    p.add_argument("--pi", default="0.4/1/0.01", help="velocity PI P/I/Tf")
    p.add_argument("--label", default="")
    p.add_argument("--out", required=True)
    a = p.parse_args()
    kp, ki, tf = a.pi.split("/")
    rows, edges = [], []
    with BoardMemory(os.path.join(".pio", "build", a.env, "firmware.elf")) as mem:
        port = RttPort(mem, timeout=0.1)

        def cmd(c, t=0.3):
            port.write((c + "\n").encode())
            end, out = time.time() + t, ""
            while time.time() < end:
                out += port.read(4096).decode(errors="replace")
                time.sleep(0.02)
            return out.strip().replace("\n", " | ")

        time.sleep(0.2)
        port.reset_input_buffer()
        for c in ["ME0", "VO", "XF0", "MLC2", "MT2", "MC1", "M0", "ST0", "DM0", f"D{a.dt}", "DT0.2",
                  f"MVP{kp}", f"MVI{ki}", f"MVF{tf}", f"M{a.speed}"]:
            cmd(c)
        has_edges = mem.hall_edge_capacity > 0
        if has_edges:
            mem.read_hall_edges()          # start point: only the edges of this run
        try:
            cmd("ME1", 0.0)
            t0 = time.time()
            k = 0
            while time.time() - t0 < a.duration:
                k += 1
                if has_edges and k % 10 == 0:
                    now = time.time() - t0
                    edges += [(t_us, c, now) for t_us, c in mem.read_hall_edges()]
                d = mem.read_dbg()
                if abs(d["velocity"]) > 20 or d["fault"] or max(abs(d["ia"]), abs(d["ib"])) > 3.5:
                    raise RuntimeError(f"abort: {d}")
                d["t"] = time.time() - t0
                rows.append(d)
        finally:
            if has_edges:
                now = time.time() - t0
                edges += [(t_us, c, now) for t_us, c in mem.read_hall_edges()]
            cmd("ME0", 0.4)
            for c in ["M0", "D0", "MVP0.4", "MVI1", "MVF0.01"]:
                cmd(c)
            print("X ->", cmd("X", 0.8))
    keys = ["t"] + [k for k in DBG_FIELDS if k != "magic"]
    os.makedirs(os.path.dirname(a.out) or ".", exist_ok=True)
    with open(a.out, "w") as f:
        f.write(f"# env {a.env} dt {a.dt} speed {a.speed} PI {a.pi} label {a.label}\n")
        f.write(",".join(keys) + "\n")
        for d in rows:
            f.write(",".join(str(d[k]) for k in keys) + "\n")
    if edges:
        ep = os.path.splitext(a.out)[0] + "_edges.csv"
        with open(ep, "w") as f:
            f.write("t_us,count,pc_t\n")   # pc_t: PC time [s since ME1] when the edge was read (for alignment)
            for e in edges:
                f.write(f"{e[0]},{e[1]},{e[2]:.4f}\n")
        print(f"{len(edges)} hall edges -> {ep}")
    print(f"{len(rows)} samples in {a.duration:.0f} s ({len(rows) / a.duration:.0f} /s) -> {a.out}")


if __name__ == "__main__":
    main()
