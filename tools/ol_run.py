#!/usr/bin/env python3
"""
Short open loop run with live current monitoring (pyOCD) and software current cut.
THE MOTOR MOVES: only run it when someone is next to the bench.

  python tools/ol_run.py --port COM8 --voltage 0.6 --speed 0.5 --duration 8
  python tools/ol_run.py --port COM8 --mode current --speed 2 --duration 8
  python tools/ol_run.py --port rtt --env gd32f130 --mode current --current 1 --speed 2 --duration 6

The heartbeat is enabled: the board disables the motor if this script stops.
The motor is always disabled at the end (ME0), also on error or Ctrl+C.
"""
import argparse
import os
import sys
import time

import serial

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from board_mem import BoardMemory, RttPort  # noqa: E402


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--port", required=True, help='serial port (ESC1) or "rtt" (GD32 board, through the ST-Link)')
    p.add_argument("--env", default="esc1", help="PlatformIO environment of the flashed firmware (for the ELF)")
    p.add_argument("--elf", default=None, help="firmware ELF (default .pio/build/<env>/firmware.elf)")
    p.add_argument("--mode", choices=["voltage", "current"], default="voltage")
    p.add_argument("--voltage", type=float, default=0.3, help="open loop voltage amplitude in voltage mode [V]")
    p.add_argument("--speed", type=float, default=0.5, help="open loop velocity [rad/s]")
    p.add_argument("--current", type=float, default=None,
                   help="current amplitude in current mode [A] (MLC, default: firmware CURRENT_LIMIT)")
    p.add_argument("--comp", type=float, default=0.0, help="dead-time compensation [V]")
    p.add_argument("--equal", action="store_true",
                   help="voltage mode: lower the open loop voltage by 4/pi*comp (DE1), same fundamental as without "
                        "compensation if the compensation matches the dead-time loss")
    p.add_argument("--duration", type=float, default=8.0, help="[s]")
    p.add_argument("--imax", type=float, default=3.0, help="software cut on any phase current [A]")
    p.add_argument("--ramp", type=float, default=0.0,
                   help="voltage mode: start at --ramp-from V and reach --voltage after this time [s] "
                        "(avoids the current peak while the rotor catches the field)")
    p.add_argument("--ramp-from", type=float, default=1.2, help="start voltage of the ramp [V]")
    p.add_argument("--out", default=None, help="CSV of the live reads (t_s, ia, ib, comp_a, electrical_angle)")
    a = p.parse_args()
    elf = a.elf or os.path.join(".pio", "build", a.env, "firmware.elf")

    peak = 0.0
    n = 0
    late = []  # dq values over the second half of the run (steady state)
    rows = []  # live reads
    cut = None
    t0 = None
    out = ""
    # one pyOCD session for the memory reads and, with --port rtt, for the commands
    with BoardMemory(elf) as mem:
        if a.port.lower() == "rtt":
            ser = RttPort(mem, timeout=0.05)
        else:
            ser = serial.Serial(a.port, 115200, timeout=0.05)

        def send(cmd):
            ser.write((cmd + "\n").encode())
            time.sleep(0.05)

        try:
            for cmd in ("ME0", "XC", "XH1", "MC3", "MT0" if a.mode == "voltage" else "MT2", "DE1" if a.equal else "DE0",
                        f"D{a.comp:.4f}", f"DB{min(a.voltage, a.ramp_from) if a.ramp > 0 else a.voltage}", f"M{a.speed}"):
                send(cmd)
            if a.current is not None:
                send(f"MLC{a.current}")
            ser.reset_input_buffer()
            send("ME1")
            t0 = time.perf_counter()
            last_ping = t0
            last_ramp = -1.0
            while (t := time.perf_counter() - t0) < a.duration:
                # voltage ramp, one command every 0.1 s (the commands also keep the heartbeat alive)
                if a.ramp > 0 and a.mode == "voltage" and t - last_ramp >= 0.1 and last_ramp < a.ramp:
                    k = min(t / a.ramp, 1.0)
                    u = a.ramp_from + k * (a.voltage - a.ramp_from)
                    send(f"DB{u:.3f}")
                    last_ramp = t if k < 1.0 else a.ramp
                    last_ping = time.perf_counter()
                if t - (last_ping - t0) > 0.5:
                    send("P")
                    last_ping = time.perf_counter()
                d = mem.read_dbg()
                n += 1
                rows.append((t, d["ia"], d["ib"], d["comp_a"], d["electrical_angle"]))
                i = max(abs(d["ia"]), abs(d["ib"]), abs(d["ic"]))
                peak = max(peak, i)
                if t > a.duration / 2:
                    late.append((d["iq"], d["id"], d["uq"], d["ud"]))
                if i > a.imax:
                    cut = f"software cut: {i:.2f} A > {a.imax} A at t={t:.2f} s"
                    break
                if d["fault"]:
                    cut = f"board fault {int(d['fault'])} at t={t:.2f} s"
                    break
        finally:
            # disable before closing the pyOCD session (closing takes longer than the heartbeat timeout)
            send("ME0")
            send("XH0")
            final = mem.read_dbg()
            time.sleep(0.1)
            out = ser.read(8192).decode(errors="replace")
            ser.close()

    print(f"{a.mode} mode, {a.voltage if a.mode == 'voltage' else 'current_limit'} "
          f"{'V' if a.mode == 'voltage' else ''}, {a.speed} rad/s, comp {a.comp} V")
    print(f"{n} reads, peak phase current {peak:.2f} A")
    if late:
        m = [sum(v[k] for v in late) / len(late) for k in range(4)]
        print(f"second half mean: iq {m[0]:.3f} A, id {m[1]:.3f} A, uq {m[2]:.3f} V, ud {m[3]:.3f} V")
    print(f"open loop angle at the end: {final['electrical_angle']:.2f} rad el.")
    # fundamental of the phase currents, synchronous with the open loop angle (least squares, second half):
    # i = a*cos(angle) + b*sin(angle) + c -> amplitude sqrt(a^2 + b^2), robust to the measurement noise
    half = [r for r in rows if r[0] > a.duration / 2]
    if len(half) > 20:
        import numpy as np
        ang = np.array([r[4] for r in half])
        X = np.column_stack([np.cos(ang), np.sin(ang), np.ones_like(ang)])
        for name, col in (("ia", 1), ("ib", 2)):
            y = np.array([r[col] for r in half])
            coef, *_ = np.linalg.lstsq(X, y, rcond=None)
            fit = X @ coef
            print(f"{name}: fundamental amplitude {np.hypot(coef[0], coef[1]):.3f} A, "
                  f"residual rms {np.sqrt(np.mean((y - fit) ** 2)):.3f} A (noise + harmonics)")
    if a.out:
        import csv
        with open(a.out, "w", newline="") as f:
            w = csv.writer(f)
            w.writerow(["t_s", "ia", "ib", "comp_a", "electrical_angle"])
            w.writerows(rows)
        print(f"{len(rows)} reads -> {a.out}")
    if cut:
        print("STOPPED EARLY:", cut)
    for line in out.splitlines():
        if "DISABLED" in line or "fault" in line.lower():
            print("  <", line)
    print("motor disabled (ME0 sent)")


if __name__ == "__main__":
    main()
