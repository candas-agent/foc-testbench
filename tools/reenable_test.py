#!/usr/bin/env python3
"""
Current filters after a pause: THE MOTOR MOVES. foc_current velocity mode at --speed for --run s, then motor
disabled for --pause s, then enabled again with a 0 rad/s target while the recorder logs every loop (filtered iq =
motor.current.q, enabled flag). Prints iq just before the pause and the first loops after enabling.

Expected with SimpleFOC dev: the current filters restart from the new value (no memory of the iq before the pause).

  python tools/reenable_test.py --env perf_dev_f103 --csv data/2026-11-20/reenable.csv
"""
import argparse
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from board_mem import BoardMemory, RttPort  # noqa: E402


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--env", required=True)
    p.add_argument("--speed", type=float, default=2.0)
    p.add_argument("--run", type=float, default=2.0)
    p.add_argument("--pause", type=float, default=1.0)
    p.add_argument("--show", type=int, default=12, help="loops printed after enabling")
    p.add_argument("--label", default="")
    p.add_argument("--csv", default=None)
    a = p.parse_args()

    def check(d):
        if abs(d["velocity"]) > 20 or d["fault"] or max(abs(d["ia"]), abs(d["ib"])) > 3.5:
            raise RuntimeError(f"abort: {d}")

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
        for c in ["ME0", "VO", "MLC2", "MT2", "MC1", "M0", "ST0", "MVP0.4", "MVI1", "MVF0.01", f"M{a.speed}"]:
            cmd(c)
        try:
            cmd("ME1", 0.0)
            t0 = time.time()
            while time.time() - t0 < a.run:
                check(mem.read_dbg())
            iq_before = mem.read_dbg()["iq"]
            cmd("ME0", 0.0)
            cmd("M0", a.pause)
            # recorder every loop, then enable: both lines in one write, handled in successive loops
            port.write(b"RN0\nME1\n")
            t0 = time.time()
            while time.time() - t0 < 0.5:
                check(mem.read_dbg())
        finally:
            cmd("ME0", 0.4)
            cmd("M0")
        rec = mem.read_recording()

    k = next((i for i, s in enumerate(rec) if s["en"] > 0.5), None)
    if k is None:
        print("enable not recorded")
        return
    frozen = rec[k - 1]["iq"] if k > 0 else float("nan")
    print(f"{a.label or a.env}: iq before the pause {iq_before:+.3f} A, iq while disabled {frozen:+.3f} A")
    print("  loop  t[us]   iq[A]")
    rows = []
    for i, s in enumerate(rec[k:k + a.show]):
        print(f"  {i:4d} {s['t_us'] - rec[k]['t_us']:6d} {s['iq']:+.3f}")
        rows.append((a.label, a.env, i, s["t_us"] - rec[k]["t_us"], f"{s['iq']:.4f}", f"{frozen:.4f}"))
    if a.csv:
        new = not os.path.exists(a.csv)
        with open(a.csv, "a") as f:
            if new:
                f.write("label,env,loop,t_us,iq,iq_disabled\n")
            for r in rows:
                f.write(",".join(str(x) for x in r) + "\n")


if __name__ == "__main__":
    main()
