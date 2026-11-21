#!/usr/bin/env python3
"""
Reads the results of the on-target equivalence test firmwares (src/equiv/equiv_main.cpp, envs equiv_*) with pyOCD,
waiting until every test is done. Does not move anything: equiv_pid uses no peripheral, equiv_pwm needs the MAIN
SUPPLY OFF (it refuses to start the PWM if the bus voltage reads > 10 V).

  python tools/equiv_read.py --elf .pio/cloud/<run>/equiv_pid_pr_f103/firmware.elf
"""
import argparse
import os
import struct
import sys
import time

sys.path.insert(0, os.path.dirname(__file__))
from board_mem import elf_symbols  # noqa: E402

MAGIC = 0xE0E0A002
FIELDS = ["magic", "state", "cases", "identical", "expected", "unexpected", "new_out_of_range", "dev_out_of_range",
          "max_count_diff", "one_count", "reg_mismatch"]
FMT = "<11If10f"
STATES = {0: "running", 1: "done", 2: "REFUSED (bus voltage > 10 V: switch the main supply off)", 3: "driver init FAILED"}
BAD = {"equiv_pid": ["P", "I", "D", "error", "error_prev", "limit", "ramp", "Ts", "out_dev", "out_new"],
       "equiv_pwm6": ["Vps", "Vlimit", "U", "dc_dev", "dc_new", "reg_dev", "reg_new", "ARR+1", "phase", "-"],
       "equiv_pwm3": ["Vps", "Vlimit", "U", "dc_dev", "dc_new", "reg_dev", "reg_new", "ARR+1", "phase", "-"]}


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--elf", required=True)
    p.add_argument("--timeout", type=float, default=600)
    a = p.parse_args()
    names = [n for n in BAD]
    syms = elf_symbols(a.elf, [], optional=names)
    present = {n: syms[n] for n in names if syms.get(n)}
    if not present:
        sys.exit("no equiv result in this ELF")
    size = struct.calcsize(FMT)
    from pyocd.core.helpers import ConnectHelper
    with ConnectHelper.session_with_chosen_probe(connect_mode="attach", target_override="cortex_m",
                                                 options={"resume_on_disconnect": True}) as session:
        target = session.board.target
        t0 = time.time()
        while True:
            res = {}
            for n, addr in present.items():
                addr = addr[0] if isinstance(addr, tuple) else addr
                v = struct.unpack(FMT, bytes(target.read_memory_block8(addr, size)))
                res[n] = (dict(zip(FIELDS + ["vbus"], v[:12])), v[12:])
            if any(r["magic"] != MAGIC for r, _ in res.values()):
                states = "not started"
            else:
                states = [r["state"] for r, _ in res.values()]
                if all(s != 0 for s in states):
                    break
            if time.time() - t0 > a.timeout:
                sys.exit(f"timeout, states {states}")
            time.sleep(2)
    for n, (r, bad) in res.items():
        print(f"{n}: {STATES.get(r['state'], r['state'])}, {r['cases']} cases: identical {r['identical']}, "
              f"expected differences {r['expected']}, UNEXPECTED {r['unexpected']}")
        if n != "equiv_pid":
            print(f"   bus {r['vbus']:.2f} V, max register difference (valid supply) {r['max_count_diff']}, "
                  f"one count {r['one_count']}, duty out of [0,1] new {r['new_out_of_range']} / dev "
                  f"{r['dev_out_of_range']}, register != dc*(ARR+1) {r['reg_mismatch']}")
        if r["unexpected"]:
            print("   first unexpected:", ", ".join(f"{k}={v:g}" for k, v in zip(BAD[n], bad)))


if __name__ == "__main__":
    main()
