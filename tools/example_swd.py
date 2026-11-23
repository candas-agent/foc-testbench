#!/usr/bin/env python3
"""
SimpleFOC GD32 split board example (env gd32_example_split) driven over SWD: its Commander is on Serial2, not
reachable on the bench. Reads the init results and, with --target, applies a torque current for a short time.
  python tools/example_swd.py --elf <elf>                       # status only, nothing written
  python tools/example_swd.py --elf <elf> --target 0.3 --time 2  # THE MOTOR TURNS (free wheel accelerates)
The target is clamped to +-0.5 A, set back to 0 at the end, and earlier if |velocity| exceeds --vmax.
Addresses of the class members resolved with arm-none-eabi-gdb from the ELF debug information.
"""
import argparse
import glob
import os
import re
import struct
import subprocess
import sys
import time

GDB = sorted(glob.glob(os.path.expanduser("~/.platformio/packages/toolchain-gccarmnoneeabi@1.90201.191206/bin/arm-none-eabi-gdb.exe")))
FLOATS = {"target": "motor.target", "iq": "motor.current.q", "id": "motor.current.d", "uq": "motor.voltage.q",
          "velocity": "motor.shaft_velocity", "zero_angle": "motor.zero_electric_angle",
          "offset_ia": "current_sense.offset_ia", "offset_ib": "current_sense.offset_ib"}
BYTES = {"status": "motor.motor_status", "direction": "motor.sensor_direction", "enabled": "motor.enabled",
         "cs_init": "current_sense.initialized", "drv_init": "driver.initialized"}
STATUS = {0: "uninitialized", 1: "initializing", 2: "uncalibrated", 3: "calibrating", 4: "ready", 8: "error",
          14: "calib_failed", 15: "init_failed"}


def addresses(elf):
    names = {**FLOATS, **BYTES}
    args = [GDB[0], "-batch"] + sum([["-ex", f"print &{v}"] for v in names.values()], []) + [elf]
    out = subprocess.run(args, capture_output=True, text=True).stdout
    found = re.findall(r"\) (0x[0-9a-f]+) <", out)
    if len(found) != len(names):
        sys.exit("gdb could not resolve the addresses:\n" + out)
    return dict(zip(names, (int(a, 16) for a in found)))


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--elf", required=True)
    p.add_argument("--target", type=float)
    p.add_argument("--time", type=float, default=2.0)
    p.add_argument("--vmax", type=float, default=8.0)
    a = p.parse_args()
    adr = addresses(a.elf)

    from pyocd.core.helpers import ConnectHelper
    session = ConnectHelper.session_with_chosen_probe(connect_mode="attach", target_override="cortex_m",
                                                      frequency=4_000_000, options={"resume_on_disconnect": True})
    if session is None:
        sys.exit("no debug probe found")
    with session:
        t = session.board.target

        def rf(n):
            return struct.unpack("<f", struct.pack("<I", t.read32(adr[n])))[0]

        def rb(n):
            return t.read8(adr[n])

        def wf(n, v):
            t.write32(adr[n], struct.unpack("<I", struct.pack("<f", v))[0])

        print(f"status {STATUS.get(rb('status'), rb('status'))}  enabled {rb('enabled')}  driver init {rb('drv_init')}  "
              f"current sense init {rb('cs_init')}")
        print(f"zero electric angle {rf('zero_angle'):.3f} rad  direction {struct.unpack('b', bytes([rb('direction')]))[0]}  "
              f"offsets ia {rf('offset_ia'):.3f} V ib {rf('offset_ib'):.3f} V")
        print(f"target {rf('target'):.3f} A  iq {rf('iq'):.3f} A  id {rf('id'):.3f} A  uq {rf('uq'):.2f} V  "
              f"velocity {rf('velocity'):.2f} rad/s")
        if a.target is None:
            return
        target = max(-0.5, min(0.5, a.target))
        try:
            wf("target", target)
            t0 = time.time()
            while time.time() - t0 < a.time:
                v = rf("velocity")
                print(f"t {time.time() - t0:4.1f} s  iq {rf('iq'):6.3f} A  id {rf('id'):6.3f} A  uq {rf('uq'):5.2f} V  "
                      f"velocity {v:6.2f} rad/s")
                if abs(v) > a.vmax:
                    print("velocity limit: target set to 0")
                    break
                time.sleep(0.2)
        finally:
            wf("target", 0.0)
        time.sleep(0.3)
        print(f"target 0: iq {rf('iq'):.3f} A  velocity {rf('velocity'):.2f} rad/s")


if __name__ == "__main__":
    main()
