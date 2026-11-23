#!/usr/bin/env python3
"""
GD32F130 board: PhaseState check of the GD32 6PWM driver (firmware src/phasecheck, envs gd32_phase_dev / _pr).
For each set of phase states, writes them into the firmware over SWD, then reads the TIMER0 output enable bits
(CHCTL2) and samples the 6 PWM pins (GPIO input registers): % of the samples at the high level.
ONLY with the main supply OFF (board on the ST-Link 3.3 V): the firmware refuses to start the driver above 10 V.
  python tools/phase_hilo.py --elf .pio/cloud/<run>/gd32_phase_pr/firmware.elf

Low side drivers active low on this board: low side pin HIGH = MOSFET off. High side pin HIGH = MOSFET on.
"""
import argparse
import os
import struct
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from board_mem import elf_symbols  # noqa: E402

CHCTL2 = 0x40012C20          # TIMER0 channel control register 2 (GD32F1x0)
GPIOA_ISTAT = 0x48000010     # GPIO input registers (GD32F1x0)
GPIOB_ISTAT = 0x48000410
OFF, ON, HI, LO = 0, 1, 2, 3
NAMES = {OFF: "OFF", ON: "ON", HI: "HI", LO: "LO"}
# phase: (timer channel, high side pin (port, bit), low side pin)
PHASES = {"A": (2, ("A", 10), ("B", 15)), "B": (1, ("A", 9), ("B", 14)), "C": (0, ("A", 8), ("B", 13))}
STEPS = [(ON, ON, ON), (OFF, OFF, OFF), (HI, HI, HI), (LO, LO, LO), (HI, LO, OFF), (LO, OFF, HI), (ON, ON, ON)]
SAMPLES = 400


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--elf", required=True)
    p.add_argument("--duty", type=float, default=0.3)
    a = p.parse_args()
    sym = elf_symbols(a.elf, ["req_state", "req_duty", "status", "vbus_measured", "applied"])

    from pyocd.core.helpers import ConnectHelper
    session = ConnectHelper.session_with_chosen_probe(connect_mode="attach", target_override="cortex_m",
                                                      frequency=4_000_000, options={"resume_on_disconnect": True})
    if session is None:
        sys.exit("no debug probe found")
    with session:
        t = session.board.target
        rd32 = t.read32
        status = rd32(sym["status"][0])
        vbus = struct.unpack("<f", struct.pack("<I", rd32(sym["vbus_measured"][0])))[0]
        print(f"status {status}  vbus {vbus:.2f} V")
        if status != 1:
            sys.exit("driver not running (2: bus voltage above 10 V, 3: driver init failed, 0: not started)")

        def write_states(states):
            t.write_memory_block8(sym["req_state"][0], list(states))
            before = rd32(sym["applied"][0])
            while rd32(sym["applied"][0]) < before + 2:   # applied by the firmware loop
                time.sleep(0.01)

        t.write32(sym["req_duty"][0], struct.unpack("<I", struct.pack("<f", a.duty))[0])
        try:
            for states in STEPS:
                write_states(states)
                v = rd32(CHCTL2)
                counts = {}
                for _ in range(SAMPLES):
                    pa, pb = rd32(GPIOA_ISTAT), rd32(GPIOB_ISTAT)
                    for port, bit in [("A", 8), ("A", 9), ("A", 10), ("B", 13), ("B", 14), ("B", 15)]:
                        val = pa if port == "A" else pb
                        counts[(port, bit)] = counts.get((port, bit), 0) + ((val >> bit) & 1)
                label = "/".join(NAMES[s] for s in states)
                print(f"\nstates A/B/C = {label:12s} CHCTL2 0x{v:04X}")
                for i, (ph, (ch, hpin, lpin)) in enumerate(PHASES.items()):
                    en, nen = (v >> (4 * ch)) & 1, (v >> (4 * ch + 2)) & 1
                    hp = 100 * counts[hpin] / SAMPLES
                    lp = 100 * counts[lpin] / SAMPLES
                    print(f"  phase {ph} ({NAMES[states[i]]:3s}) CH{ch}EN {en} CH{ch}NEN {nen}   "
                          f"high side P{hpin[0]}{hpin[1]} {hp:5.1f} % high   low side P{lpin[0]}{lpin[1]} {lp:5.1f} % high")
        finally:
            write_states((OFF, OFF, OFF))
            print("\nall phases OFF")


if __name__ == "__main__":
    main()
