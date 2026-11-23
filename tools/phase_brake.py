#!/usr/bin/env python3
"""
GD32F130 board: PhaseState test UNDER POWER (firmware src/phasebrake, envs gd32_brake_dev / _pr). Sets one of the
fixed modes of the firmware (the three phases always in the same state with the same duty cycle), prints TIMER0
CHCTL2 and leaves the mode applied: turn the wheel by hand and feel if it brakes. Mode 0 (all OFF) at the end of a
test session: python tools/phase_brake.py --elf ... --mode 0
  modes: 0 OFF, 1 ON 0 %, 2 LO 0 %, 3 LO 95 %, 4 HI 0 %
"""
import argparse
import os
import struct
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from board_mem import elf_symbols  # noqa: E402

CHCTL2 = 0x40012C20
MODES = ["OFF", "ON 0 %", "LO 0 %", "LO 95 %", "HI 0 %"]


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--elf", required=True)
    p.add_argument("--mode", type=int, required=True, choices=range(len(MODES)))
    a = p.parse_args()
    sym = elf_symbols(a.elf, ["req_mode", "status", "vbus_measured", "mode_applied", "applied"])

    from pyocd.core.helpers import ConnectHelper
    session = ConnectHelper.session_with_chosen_probe(connect_mode="attach", target_override="cortex_m",
                                                      frequency=4_000_000, options={"resume_on_disconnect": True})
    if session is None:
        sys.exit("no debug probe found")
    with session:
        t = session.board.target
        status = t.read32(sym["status"][0])
        vbus = struct.unpack("<f", struct.pack("<I", t.read32(sym["vbus_measured"][0])))[0]
        print(f"status {status}  vbus {vbus:.1f} V")
        if status != 1:
            sys.exit("driver not running (2: bus voltage too high, 3: driver init failed, 0: not started)")
        t.write32(sym["req_mode"][0], a.mode)
        before = t.read32(sym["applied"][0])
        while t.read32(sym["applied"][0]) < before + 2:
            time.sleep(0.01)
        v = t.read32(CHCTL2)
        print(f"mode {t.read32(sym['mode_applied'][0])} ({MODES[a.mode]})  CHCTL2 0x{v:04X}")


if __name__ == "__main__":
    main()
