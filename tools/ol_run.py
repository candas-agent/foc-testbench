#!/usr/bin/env python3
"""
Short open loop run with live current monitoring (pyOCD) and software current cut.
THE MOTOR MOVES: only run it when someone is next to the bench.

  python tools/ol_run.py --port COM8 --voltage 0.6 --speed 0.5 --duration 8
  python tools/ol_run.py --port COM8 --mode current --speed 2 --duration 8

The heartbeat is enabled: the board disables the motor if this script stops.
The motor is always disabled at the end (ME0), also on error or Ctrl+C.
"""
import argparse
import os
import sys
import time

import serial

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from board_mem import BoardMemory  # noqa: E402


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--port", required=True)
    p.add_argument("--elf", default=os.path.join(".pio", "build", "esc1", "firmware.elf"))
    p.add_argument("--mode", choices=["voltage", "current"], default="voltage")
    p.add_argument("--voltage", type=float, default=0.3, help="open loop voltage amplitude in voltage mode [V]")
    p.add_argument("--speed", type=float, default=0.5, help="open loop velocity [rad/s]")
    p.add_argument("--current", type=float, default=None,
                   help="current amplitude in current mode [A] (MLC, default: firmware CURRENT_LIMIT)")
    p.add_argument("--comp", type=float, default=0.0, help="dead-time compensation [V]")
    p.add_argument("--duration", type=float, default=8.0, help="[s]")
    p.add_argument("--imax", type=float, default=3.0, help="software cut on any phase current [A]")
    a = p.parse_args()

    ser = serial.Serial(a.port, 115200, timeout=0.05)

    def send(cmd):
        ser.write((cmd + "\n").encode())
        time.sleep(0.05)

    peak = 0.0
    n = 0
    late = []  # dq values over the second half of the run (steady state)
    cut = None
    t0 = None
    try:
        with BoardMemory(a.elf) as mem:
            for cmd in ("ME0", "XC", "XH1", "MC3", "MT0" if a.mode == "voltage" else "MT2", "DE0",
                        f"D{a.comp:.4f}", f"DB{a.voltage}", f"M{a.speed}"):
                send(cmd)
            if a.current is not None:
                send(f"MLC{a.current}")
            ser.reset_input_buffer()
            send("ME1")
            t0 = time.perf_counter()
            last_ping = t0
            while (t := time.perf_counter() - t0) < a.duration:
                if t - (last_ping - t0) > 0.5:
                    send("P")
                    last_ping = time.perf_counter()
                d = mem.read_dbg()
                n += 1
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
            # disable now: closing the pyOCD session takes longer than the heartbeat timeout
            send("ME0")
            final = mem.read_dbg()
    finally:
        send("ME0")
        send("XH0")
        out = ser.read(8192).decode(errors="replace")
        ser.close()

    print(f"{a.mode} mode, {a.voltage if a.mode == 'voltage' else 'current_limit'} "
          f"{'V' if a.mode == 'voltage' else ''}, {a.speed} rad/s, comp {a.comp} V")
    print(f"{n} reads, peak phase current {peak:.2f} A")
    if late:
        m = [sum(v[k] for v in late) / len(late) for k in range(4)]
        print(f"second half mean: iq {m[0]:.3f} A, id {m[1]:.3f} A, uq {m[2]:.3f} V, ud {m[3]:.3f} V")
    print(f"open loop angle at the end: {final['electrical_angle']:.2f} rad el.")
    if cut:
        print("STOPPED EARLY:", cut)
    for line in out.splitlines():
        if "DISABLED" in line or "fault" in line.lower():
            print("  <", line)
    print("motor disabled (ME0 sent)")


if __name__ == "__main__":
    main()
