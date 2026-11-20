#!/usr/bin/env python3
"""
Measure and re-centre the current sense offsets: THE MOTOR IS ENABLED, at 0 V in open loop (no current, no torque).

Open loop angle mode, voltage mode, amplitude 0 (DB0, MC4, MT0), ME1: the real phase currents are 0, the mean
measured currents are the offset error. With --trim, XO moves the offsets by that error, then they are measured again.
The motor is disabled at the end and the bench is put back in closed loop velocity / foc_current (DB0.3, MT2, MC1).

  python tools/offset_trim.py --env combo_f103 [--trim]
"""
import argparse
import os
import statistics as st
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from board_mem import BoardMemory, RttPort  # noqa: E402


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--env", default="combo_f103")
    p.add_argument("--trim", action="store_true", help="re-centre the offsets (XO) and measure again")
    a = p.parse_args()
    with BoardMemory(os.path.join(".pio", "build", a.env, "firmware.elf")) as mem:
        port = RttPort(mem, timeout=0.1)

        def cmd(c, t=0.3):
            port.write((c + "\n").encode())
            end, out = time.time() + t, ""
            while time.time() < end:
                out += port.read(4096).decode(errors="replace")
                time.sleep(0.02)
            return out.strip().replace("\n", " | ")

        def measure(label):
            rows, t0 = [], time.time()
            while time.time() - t0 < 2:
                d = mem.read_dbg()
                if max(abs(d["ia"]), abs(d["ib"])) > 3 or d["fault"] or not d["enabled"]:
                    raise RuntimeError(f"abort: {d}")
                rows.append(d)
            ia = st.mean(r["ia"] for r in rows)
            ib = st.mean(r["ib"] for r in rows)
            print(f"{label}: ia {ia:+.4f} ib {ib:+.4f} ic {-ia - ib:+.4f} A ({len(rows)} samples, sd ia "
                  f"{st.pstdev(r['ia'] for r in rows):.3f})", flush=True)

        time.sleep(0.2)
        port.reset_input_buffer()
        for c in ["ME0", "VO", "D0", "XF0", "DB0", "MC4", "MT0"]:
            print(c, "->", cmd(c))
        try:
            cmd("ME1", 0.1)
            time.sleep(1.0)
            measure("before")
            if a.trim:
                print("XO ->", cmd("XO", 1.0))
                time.sleep(0.5)
                measure("after ")
        finally:
            cmd("ME0", 0.4)
            for c in ["DB0.3", "MT2", "MC1", "M0"]:
                cmd(c)
            print("X ->", cmd("X", 0.8))


if __name__ == "__main__":
    main()
