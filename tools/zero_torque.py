#!/usr/bin/env python3
"""
Zero torque test in closed loop (needs a position sensor: GD32 board with hall sensors).
THE MOTOR IS ENERGIZED: someone turns the wheel by hand during each phase and compares the feel.

  python tools/zero_torque.py --port rtt --env gd32f130 --comp 0.605 --threshold 1.22

Phases (torque mode MC0, current control MT2, target 0 A):
  1. no compensation
  2. compensation on the current setpoints (DM0): exactly 0 at zero torque by design
  3. compensation on the filtered measured currents (DM1), threshold >= compensation / R
Before phase N the motor "beeps" N times (the field spun at ~140 Hz electrical, the rotor cannot follow and
vibrates), one long beep at the end: stop turning the wheel during the beeps to feel them.
Heartbeat on: the board disables the motor if this script stops. Motor disabled at the end of each phase.
"""
import argparse
import csv
import os
import statistics as st
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from board_mem import BoardMemory, RttPort  # noqa: E402


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--port", required=True, help='serial port or "rtt" (GD32 board, through the ST-Link)')
    p.add_argument("--env", default="gd32f130", help="PlatformIO environment of the flashed firmware (for the ELF)")
    p.add_argument("--elf", default=None, help="firmware ELF (default .pio/build/<env>/firmware.elf)")
    p.add_argument("--comp", type=float, required=True, help="dead-time compensation [V] (calibrated value)")
    p.add_argument("--threshold", type=float, required=True,
                   help="ramp threshold for DM1 [A], at least compensation / R (printed by the calibration)")
    p.add_argument("--duration", type=float, default=20.0, help="duration of each phase [s]")
    p.add_argument("--imax", type=float, default=3.0, help="software cut on any phase current [A]")
    p.add_argument("--no-beep", action="store_true", help="no beeps between the phases")
    p.add_argument("--beep-voltage", type=float, default=1.6, help="beep voltage [V] (must exceed the dead-time loss)")
    p.add_argument("--out", default=None, help="folder for the CSV of each phase")
    a = p.parse_args()
    elf = a.elf or os.path.join(".pio", "build", a.env, "firmware.elf")
    cases = [("no compensation", ["D0", "DM0", "DT0.2"]),
             (f"compensation {a.comp} V on setpoints (DM0)", [f"D{a.comp}", "DM0", "DT0.2"]),
             (f"compensation {a.comp} V on measured currents (DM1, threshold {a.threshold} A)",
              [f"D{a.comp}", "DM1", f"DT{a.threshold}"])]
    if a.out:
        os.makedirs(a.out, exist_ok=True)

    with BoardMemory(elf) as mem:
        if a.port.lower() == "rtt":
            port = RttPort(mem, timeout=0.1)
        else:
            import serial
            port = serial.Serial(a.port, 115200, timeout=0.1)

        def send(c):
            port.write((c + "\n").encode())
            time.sleep(0.05)

        def beep(n, length=0.3):
            if a.no_beep:
                return
            try:
                for c in ("ME0", "XC", "XH0", "MT0", "MC3", "DE0", "D0", f"DB{a.beep_voltage}", "M60"):
                    send(c)
                for _ in range(n):
                    send("ME1")
                    t0 = time.time()
                    while time.time() - t0 < length:
                        d = mem.read_dbg()
                        if max(abs(d["ia"]), abs(d["ib"])) > a.imax or d["fault"]:
                            break
                    send("ME0")
                    time.sleep(0.35)
            finally:
                send("ME0")
                send("M0")

        try:
            time.sleep(2)
            for k, (name, setup) in enumerate(cases):
                beep(k + 1)
                time.sleep(1.0)
                rows, stop = [], None
                try:
                    for c in ["ME0", "XC", "XH1", "MT2", "MC0"] + setup + ["M0"]:
                        send(c)
                    port.reset_input_buffer()
                    send("ME1")
                    t0 = last = time.time()
                    while time.time() - t0 < a.duration:
                        if time.time() - last > 0.5:
                            send("P")
                            last = time.time()
                        d = mem.read_dbg()
                        rows.append((time.time() - t0, d["iq"], d["id"], d["ia"], d["ib"], d["uq"], d["ud"],
                                     d["velocity"], d["comp_a"]))
                        if max(abs(d["ia"]), abs(d["ib"])) > a.imax:
                            stop = f"software cut {max(abs(d['ia']), abs(d['ib'])):.2f} A"
                            break
                        if d["fault"]:
                            stop = f"board fault {d['fault']}"
                            break
                finally:
                    send("ME0")
                    send("XH0")
                msgs = port.read(4096).decode(errors="replace").strip().splitlines()
                on = rows[5:] or rows
                print(f"== {k + 1}. {name}: {len(rows)} reads" + (f", STOPPED: {stop}" if stop else ""))
                for label, i in (("iq", 1), ("id", 2), ("uq", 5), ("ud", 6), ("comp_a", 8)):
                    v = [r[i] for r in on]
                    print(f"   {label:7s} mean {st.mean(v):7.3f}  sd {st.pstdev(v):6.3f}")
                print(f"   |velocity| max {max(abs(r[7]) for r in on):.2f} rad/s")
                for m in msgs:
                    if "DISABLED" in m or "No aligned" in m or "forced" in m:
                        print("   <", m)
                if a.out:
                    with open(os.path.join(a.out, f"zero_torque_{k + 1}.csv"), "w", newline="") as f:
                        w = csv.writer(f)
                        w.writerow(["t_s", "iq", "id", "ia", "ib", "uq", "ud", "velocity", "comp_a"])
                        w.writerows(rows)
                if stop:
                    break
                if k < len(cases) - 1:
                    time.sleep(5)
            beep(1, length=1.0)
        finally:
            send("ME0")
            send("XH0")
            port.close()


if __name__ == "__main__":
    main()
