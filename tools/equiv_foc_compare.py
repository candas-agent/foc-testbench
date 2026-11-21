#!/usr/bin/env python3
"""
Flashes the loopFOC scenario test firmwares (src/equiv/equiv_foc.cpp, envs equiv_foc_dev_f103 and
equiv_foc_pr_f103), reads their traces with pyOCD and compares them bit for bit, scenario by scenario.
No peripheral is used by these firmwares (fake driver, current sense, sensor and clock): nothing can move.
Also runs the pin test firmware (equiv_pins_f103) if given.

  python tools/equiv_foc_compare.py --dev <dev elf> --pr <pr elf> [--pins <pins elf>]
"""
import argparse
import os
import struct
import subprocess
import sys
import time

import numpy as np

sys.path.insert(0, os.path.dirname(__file__))
from board_mem import elf_symbols  # noqa: E402

PYOCD = os.path.join(os.path.dirname(sys.executable), "pyocd.exe")
NAMES = (["filter + PID called directly", "steady loop"] +
         [f"disabled {p} s, enabled again" for p in (0.05, 0.25, 0.29, 0.31, 0.4, 0.6)] +
         ["Ts set by the user", "Ts set, then back to NOT_SET (after loop 60)"] +
         [f"{m} mode {a} s, back to foc_current (after loop 30)" for m in ("voltage", "dc_current", "estimated_current")
          for a in (0.1, 0.4)] +
         ["clock not moving every 10th loop", "_micros wrap"])
COLS = {0: ["lpf", "pid", "dt_us", "x"]}
COLS_MOTOR = ["voltage.q", "voltage.d", "current.q", "current.d"]


def flash(elf):
    subprocess.run([PYOCD, "flash", "-t", "stm32f103rc", elf], check=True, capture_output=True)
    time.sleep(3)


def session():
    from pyocd.core.helpers import ConnectHelper
    return ConnectHelper.session_with_chosen_probe(connect_mode="attach", target_override="cortex_m",
                                                   options={"resume_on_disconnect": True})


def run_foc(elf):
    flash(elf)
    syms = elf_symbols(elf, ["foc_result", "foc_trace"])
    with session() as s:
        t = s.board.target
        for _ in range(120):
            res = struct.unpack("<3I20II", bytes(t.read_memory_block8(syms["foc_result"][0], 4 * 24)))
            if res[0] == 0xE0E0F001 and res[1] == 1:
                break
            time.sleep(1)
        else:
            sys.exit(f"{elf}: not done, result {res}")
        n = res[2]
        raw = bytes(t.read_memory_block8(syms["foc_trace"][0], 16 * n))
    trace = np.frombuffer(raw, dtype=np.float32).reshape(n, 4)
    return trace, list(res[3:23]), res[23]


def run_pins(elf):
    flash(elf)
    syms = elf_symbols(elf, ["pin_result"])
    with session() as s:
        t = s.board.target
        for _ in range(600):
            magic, state, cases, differ, first = struct.unpack(
                "<4Ii", bytes(t.read_memory_block8(syms["pin_result"][0], 20)))
            if magic == 0xE0E0F002 and state == 1:
                break
            time.sleep(1)
        else:
            sys.exit("pins: not done")
    print(f"pins: {cases} int values, _isset(pin) != (pin != _NC) for {differ}" +
          (f" (first {first})" if differ else ""))


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--dev")
    p.add_argument("--pr")
    p.add_argument("--pins")
    a = p.parse_args()
    if a.pins:
        run_pins(a.pins)
    if not (a.dev and a.pr):
        return
    td, sd, init_d = run_foc(a.dev)
    tp, sp, init_p = run_foc(a.pr)
    print(f"initFOC: dev {init_d}, PR {init_p}; loops recorded: dev {len(td)}, PR {len(tp)}")
    if sd != sp or len(td) != len(tp):
        sys.exit(f"scenario boundaries differ: {sd} / {sp}")
    same = (td.view(np.uint32) == tp.view(np.uint32)) | (np.isnan(td) & np.isnan(tp))
    for k, name in enumerate(NAMES):
        lo, hi = sd[k], sd[k + 1]
        bad = np.nonzero(~same[lo:hi].all(axis=1))[0]
        print(f"{k:2d} {name}: {hi - lo} loops (30 warm-up), {len(bad)} differ" + (f", first at loop {bad[0]}" if len(bad) else ""))
        cols = COLS.get(k, COLS_MOTOR)
        for i in bad[:3]:
            row = lo + i
            d = ", ".join(f"{c} {td[row, j]:.6g} -> {tp[row, j]:.6g}" for j, c in enumerate(cols)
                          if not same[row, j])
            print(f"   loop {i}: {d}")
    np.savez(os.path.join(os.path.dirname(a.pr), "foc_traces.npz"), dev=td, pr=tp, starts=np.array(sd))


if __name__ == "__main__":
    main()
