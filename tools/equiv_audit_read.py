#!/usr/bin/env python3
"""
Flashes and reads the on-target audit of SimpleFOC dev functions (src/equiv/equiv_audit.cpp, env
equiv_audit_dev_f103). No peripheral is used (fake driver / current sense / sensor / clock): nothing can move.

  python tools/equiv_audit_read.py --elf .pio/cloud/<run>/equiv_audit_dev_f103/firmware.elf [--no-flash]
"""
import argparse
import os
import struct
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(__file__))
from board_mem import elf_symbols  # noqa: E402

PYOCD = os.path.join(os.path.dirname(sys.executable), "pyocd.exe")
CHECKS = {
    1: ("_sin/_cos, negative angles [-50, 0)", "angle, _sin, _cos"),
    2: ("_sin, large angles [4e5, 4.3e5]", "angle, _sin"),
    3: ("_sin/_cos on [0, 2PI]", "angle"),
    4: ("angle open loop, voltage vector amplitude, shaft angle < 0", "shaft angle, amplitude, expected"),
    5: ("angle open loop, voltage vector amplitude, shaft angle >= 0", "shaft angle, amplitude, expected"),
    6: ("setPhaseVoltage SinePWM, not centered", "angle, Uq, min phase, max phase"),
    7: ("setPhaseVoltage SinePWM, centered", "angle, Uq, min phase, max phase"),
    8: ("setPhaseVoltage SpaceVectorPWM, not centered", "angle, Uq, min phase, max phase"),
    9: ("setPhaseVoltage SpaceVectorPWM, centered", "angle, Uq, min phase, max phase"),
    10: ("setPhaseVoltage Trapezoid_120, not centered", "angle, Uq, min phase, max phase"),
    11: ("setPhaseVoltage Trapezoid_120, centered", "angle, Uq, min phase, max phase"),
    12: ("setPhaseVoltage Trapezoid_150, not centered", "angle, Uq, min phase, max phase"),
    13: ("setPhaseVoltage Trapezoid_150, centered", "angle, Uq, min phase, max phase"),
    14: ("trapezoid sector index in range", "angle, normalized, sector6, sector12"),
    15: ("getFOCCurrents vs exact Park/Clarke (2 and 3 phases)", "angle, id, iq, two phases"),
    16: ("getDCCurrent magnitude (relative error)", "angle, id, iq, result"),
    23: ("getDCCurrent sign", "angle, id, iq, result"),
    17: ("Sensor 0.05 rad/s across the 2^31 us wrap", "t from wrap [us], velocity, expected, angle error"),
    18: ("Sensor -0.05 rad/s across the 2^31 us wrap", "t from wrap [us], velocity, expected, angle error"),
    19: ("Sensor 3 rad/s across the 2^31 us wrap", "t from wrap [us], velocity, expected, angle error"),
    20: ("Sensor -3 rad/s across the 2^31 us wrap", "t from wrap [us], velocity, expected, angle error"),
    21: ("Sensor 200 rad/s across the 2^31 us wrap", "t from wrap [us], velocity, expected, angle error"),
    22: ("Sensor -200 rad/s across the 2^31 us wrap", "t from wrap [us], velocity, expected, angle error"),
    30: ("sensor CW: velocity mode +-20 rad/s (relative error)", "target, true velocity, shaft_velocity"),
    31: ("sensor CW: angle mode +3 / -3 rad", "target, shaft_angle, max |velocity|"),
    32: ("sensor CW: angle mode, velocity limit 10 rad/s respected", "max |velocity|"),
    33: ("sensor CW: foc_current torque 0.5 / -0.5 / 2 A (limit 1 A)", "target, true iq, max |iq|"),
    34: ("sensor CW: applied voltage <= voltage_limit 6 V", "max amplitude"),
    35: ("sensor CW: initFOC alignment (direction, zero angle 0.7), then 15 rad/s", "zero_electric_angle, sensor_direction, velocity, initFOC ok"),
    40: ("sensor CCW: velocity mode +-20 rad/s (relative error)", "target, true velocity, shaft_velocity"),
    41: ("sensor CCW: angle mode +3 / -3 rad", "target, shaft_angle, max |velocity|"),
    42: ("sensor CCW: angle mode, velocity limit 10 rad/s respected", "max |velocity|"),
    43: ("sensor CCW: foc_current torque 0.5 / -0.5 / 2 A (limit 1 A)", "target, true iq, max |iq|"),
    44: ("sensor CCW: applied voltage <= voltage_limit 6 V", "max amplitude"),
    45: ("sensor CCW: initFOC alignment (direction, zero angle 0.7), then 15 rad/s", "zero_electric_angle, sensor_direction, velocity, initFOC ok"),
    50: ("HallSensor forward 1 sector / 2 ms across 2^31 us", "count, angle, velocity, expected"),
    51: ("HallSensor backward 1 sector / 2 ms across 2^31 us", "count, angle, velocity, expected"),
    52: ("HallSensor glitch to 000/111: angle jump (sectors)", "sector, bad state, angle before, during"),
    53: ("HallSensor glitch to 000/111: angle after (sectors of drift)", "sector, bad state, angle before, after"),
    54: ("HallSensor velocity after reversal / when stopped", "velocity after reversal, velocity stopped"),
}
REC = "<3If4f"


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--elf", required=True)
    p.add_argument("--no-flash", action="store_true")
    a = p.parse_args()
    if not a.no_flash:
        subprocess.run([PYOCD, "flash", "-t", "stm32f103rc", a.elf], check=True, capture_output=True)
        time.sleep(3)
    syms = elf_symbols(a.elf, ["audit", "audit_state"])
    from pyocd.core.helpers import ConnectHelper
    with ConnectHelper.session_with_chosen_probe(connect_mode="attach", target_override="cortex_m",
                                                 options={"resume_on_disconnect": True}) as s:
        t = s.board.target
        for _ in range(300):
            magic, state, n = struct.unpack("<3I", bytes(t.read_memory_block8(syms["audit_state"][0], 12)))
            if magic == 0xE0E0A0D1 and state == 1:
                break
            time.sleep(1)
        else:
            sys.exit(f"audit not done (magic {magic:#x}, state {state}, {n} checks)")
        size = struct.calcsize(REC)
        raw = bytes(t.read_memory_block8(syms["audit"][0], size * n))
    for k in range(n):
        cid, cases, fails, err, *w = struct.unpack_from(REC, raw, k * size)
        name, wn = CHECKS.get(cid, (f"check {cid}", "w0, w1, w2, w3"))
        flag = "FAIL" if fails else "ok  "
        print(f"{flag} {cid:2d} {name}: {cases} cases, {fails} outside tolerance, max error {err:.4g}")
        if fails:
            print(f"        worst: {wn} = " + ", ".join(f"{v:.6g}" for v in w[:len(wn.split(','))]))


if __name__ == "__main__":
    main()
