#!/usr/bin/env python3
"""
GD32 driver sampling point sweep: THE MOTOR MOVES (open loop in voltage mode, the currents are only measured).
For each value, writes the driver's `gd32_timer_sampling_point` (or older `sampling_point`) global (fraction of the PWM period where TIMER0 CH3 triggers
the injected ADC conversions) through SWD with the motor disabled, then runs tools/ol_run.py and keeps the
fundamental amplitude and the residual of ia and ib. A wrong sampling instant shows as a fundamental away from
the reference and a larger residual. Restores the initial value at the end.
  python tools/sampling_sweep.py --elf <firmware.elf> --points 0.40 0.50 0.60 ... --csv data/x.csv
"""
import argparse
import csv
import os
import re
import struct
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from board_mem import BoardMemory, elf_symbols  # noqa: E402

FUND = re.compile(r"^(i[ab]): fundamental amplitude ([\d.]+) A, residual rms ([\d.]+) A")
PEAK = re.compile(r"peak phase current ([\d.]+) A")


def write_sp(elf, value):
    with BoardMemory(elf) as mem:
        # gd32_timer_sampling_point since the GD32 cleanup (point 3), sampling_point before
        names = ["gd32_timer_sampling_point", "sampling_point"]
        syms = elf_symbols(elf, [], optional=names)
        found = [n for n in names if n in syms]
        if not found:
            raise RuntimeError(f"none of {names} in the ELF")
        addr = syms[found[0]][0]
        old = struct.unpack("<f", bytes(mem.target.read_memory_block8(addr, 4)))[0]
        mem.target.write_memory_block8(addr, list(struct.pack("<f", value)))
        return old


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--elf", required=True)
    p.add_argument("--points", type=float, nargs="+", required=True)
    p.add_argument("--voltage", type=float, default=1.2)
    p.add_argument("--speed", type=float, default=0.5)
    p.add_argument("--duration", type=float, default=4.0)
    p.add_argument("--csv", default=None)
    a = p.parse_args()

    tools = os.path.dirname(os.path.abspath(__file__))
    initial = None
    rows = []
    try:
        for sp in a.points:
            old = write_sp(a.elf, sp)
            if initial is None:
                initial = old
            out = subprocess.run([sys.executable, os.path.join(tools, "ol_run.py"), "--port", "rtt", "--env", "gd32f130",
                                  "--elf", a.elf, "--voltage", str(a.voltage), "--speed", str(a.speed),
                                  "--duration", str(a.duration), "--imax", "6"],
                                 capture_output=True, text=True, timeout=120).stdout
            r = {"sp": sp}
            for line in out.splitlines():
                m = FUND.match(line.strip())
                if m:
                    r[m.group(1) + "_fund"] = float(m.group(2))
                    r[m.group(1) + "_res"] = float(m.group(3))
                m = PEAK.search(line)
                if m:
                    r["peak"] = float(m.group(1))
                if "cut" in line or "fault" in line and "last fault: 0" not in line:
                    r["note"] = line.strip()
            rows.append(r)
            print(f"sp {sp:.3f}: ia {r.get('ia_fund', float('nan')):.3f} A (res {r.get('ia_res', float('nan')):.3f}), "
                  f"ib {r.get('ib_fund', float('nan')):.3f} A (res {r.get('ib_res', float('nan')):.3f}), "
                  f"peak {r.get('peak', float('nan')):.2f} A {r.get('note', '')}", flush=True)
    finally:
        if initial is not None:
            write_sp(a.elf, initial)
            print(f"sampling_point restored to {initial:.4f}")
    if a.csv:
        keys = ["sp", "ia_fund", "ia_res", "ib_fund", "ib_res", "peak", "note"]
        new = not os.path.exists(a.csv)
        with open(a.csv, "a", newline="") as f:
            w = csv.DictWriter(f, fieldnames=keys)
            if new:
                w.writeheader()
            w.writerows(rows)


if __name__ == "__main__":
    main()
