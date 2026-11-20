#!/usr/bin/env python3
"""
Current filters after a pause: THE MOTOR MOVES (up to --speed). foc_current velocity mode, enabled at standstill,
velocity step 0 -> --speed and the motor disabled --step-time s later, during the acceleration (filtered iq high,
then frozen while disabled), pause --pause s, then enabled again with a 0 rad/s target. The recorder logs every
loop (filtered iq = motor.current.q, enabled flag). Prints the frozen iq and the first loops after enabling.

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
    p.add_argument("--elf", default=None, help="firmware ELF (default .pio/build/<env>/firmware.elf, e.g. a cloud build)")
    p.add_argument("--speed", type=float, default=2.0)
    p.add_argument("--step-time", type=float, default=0.06)
    p.add_argument("--pause", type=float, default=1.0)
    p.add_argument("--show", type=int, default=12, help="loops printed after enabling")
    p.add_argument("--label", default="")
    p.add_argument("--csv", default=None)
    a = p.parse_args()

    def check(d):
        # phase currents only when enabled: with R_ds_on current sense the readings are meaningless when disabled
        if abs(d["velocity"]) > 20 or d["fault"] or (d["enabled"] and max(abs(d["ia"]), abs(d["ib"])) > 3.5):
            raise RuntimeError(f"abort: {d}")

    with BoardMemory(a.elf or os.path.join(".pio", "build", a.env, "firmware.elf")) as mem:
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
        for c in ["ME0", "VO", "MLC2", "MT2", "MC1", "M0", "ST0", "MVP0.4", "MVI1", "MVF0.01"]:
            cmd(c)
        try:
            cmd("ME1", 0.5)
            check(mem.read_dbg())
            # 1st recording: velocity step, motor disabled during the acceleration
            port.write(b"RN0\n")
            time.sleep(0.02)
            port.write(f"M{a.speed}\n".encode())
            time.sleep(a.step_time)
            port.write(b"ME0\n")
            time.sleep(0.3)
            check(mem.read_dbg())
            rec1 = mem.read_recording()
            cmd("M0", a.pause)
            # 2nd recording: started while disabled, then enabled (target 0)
            port.write(b"RN0\n")
            time.sleep(0.03)
            port.write(b"ME1\n")
            t0 = time.time()
            while time.time() - t0 < 0.4:
                check(mem.read_dbg())
        finally:
            cmd("ME0", 0.4)
            cmd("M0")
        rec = mem.read_recording()

    k = next((i for i, s in enumerate(rec) if s["en"] > 0.5), None)
    if k is None:
        print("enable not recorded")
        return
    iq_max = max((r["iq"] for r in rec1 if r["en"] > 0.5), key=abs, default=float("nan"))
    frozen = rec[k - 1]["iq"] if k > 0 else float("nan")
    print(f"{a.label or a.env}: max iq during the step {iq_max:+.3f} A, iq frozen while disabled {frozen:+.3f} A "
          f"({k} disabled loops recorded before enabling)")
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
