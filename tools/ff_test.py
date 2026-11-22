#!/usr/bin/env python3
"""
Voltage feed-forward test (bench command XK): velocity steps in closed loop, foc_current, with each feed-forward
mode, on the STM32F103 board. THE MOTOR MOVES (+-3 rad/s by default).

For each mode the motor is enabled at 0 rad/s, then steps 0 -> +V -> 0 -> -V -> 0 (1 s each). During each step the
recorder takes one sample every 2 ms (iq filtered, current setpoint) and dbg is polled (velocity). Per step:
RMS of (current setpoint - iq), velocity rise time 10-90 %, overshoot, mean error over the last 0.3 s.
Modes are alternated over --rounds to average out drifts.

  python tools/ff_test.py --elf .pio/cloud/<run>/perf_dev_f103/firmware.elf --out data/2026-11-22/ff.csv
"""
import argparse
import csv
import math
import os
import sys
import time

sys.path.insert(0, os.path.dirname(__file__))
from board_mem import BoardMemory, RttPort  # noqa: E402

MODES = {0: "off", 1: "back-EMF", 2: "back-EMF + R*i", 3: "back-EMF + R*i + dq decoupling"}


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--elf", required=True)
    p.add_argument("--out", required=True)
    p.add_argument("--speed", type=float, default=3.0)
    p.add_argument("--modes", type=int, nargs="*", default=[0, 1, 2, 3])
    p.add_argument("--rounds", type=int, default=2)
    p.add_argument("--imax", type=float, default=3.5)
    p.add_argument("--vmax", type=float, default=8.0)
    a = p.parse_args()
    os.makedirs(os.path.dirname(a.out) or ".", exist_ok=True)
    results = []
    with BoardMemory(a.elf) as mem:
        port = RttPort(mem, timeout=0.1)

        def cmd(c, t=0.25):
            port.write((c + "\n").encode())
            end, out = time.time() + t, ""
            while time.time() < end:
                out += port.read(4096).decode(errors="replace")
                time.sleep(0.02)
            return out

        def check(d):
            imax = max(abs(d["ia"]), abs(d["ib"])) if d["enabled"] else 0.0
            if imax > a.imax or abs(d["velocity"]) > a.vmax or d["fault"]:
                cmd("ME0", 0.1)
                raise RuntimeError(f"abort: imax {imax:.2f} A, velocity {d['velocity']:.2f}, fault {d['fault']}")

        time.sleep(0.2)
        port.reset_input_buffer()
        try:
            for rnd in range(a.rounds):
                order = a.modes if rnd % 2 == 0 else list(reversed(a.modes))
                for mode in order:
                    cmd("ME0")
                    t_wait = time.time()
                    while abs(mem.read_dbg()["velocity"]) > 0.3 and time.time() - t_wait < 20:
                        time.sleep(0.2)
                    for c in ["XR0", "VO", "MLC2", "MT2", "MC1", "M0", "ST0", "MVP0.4", "MVI1", "MVF0.01", f"XK{mode}"]:
                        cmd(c)
                    cmd("ME1", 0.0)
                    t0 = time.time()
                    while time.time() - t0 < 1.0:
                        check(mem.read_dbg())
                    for target in (a.speed, 0.0, -a.speed, 0.0):
                        prev = None
                        cmd("RN2000", 0.02)
                        cmd(f"M{target}", 0.0)
                        ts = time.time()
                        vel = []
                        while time.time() - ts < 1.0:
                            d = mem.read_dbg()
                            check(d)
                            vel.append((time.time() - ts, d["velocity"], d["uq"]))
                        for _ in range(40):
                            if mem.recording_done():
                                break
                            time.sleep(0.05)
                        rec = mem.read_recording()
                        err = [s["angle_err"] - s["iq"] for s in rec]   # current setpoint - filtered iq
                        rms = math.sqrt(sum(e * e for e in err) / len(err))
                        start = vel[0][1]
                        step = target - start
                        rise = None
                        if abs(step) > 0.5:
                            t10 = next((t for t, v, _ in vel if (v - start) / step >= 0.1), None)
                            t90 = next((t for t, v, _ in vel if (v - start) / step >= 0.9), None)
                            rise = (t90 - t10) if (t10 is not None and t90 is not None) else None
                            over = max(((v - start) / step - 1.0) for _, v, _ in vel) * 100
                        else:
                            over = max(abs(v) for _, v, _ in vel)  # from +-V to 0: largest |velocity| after 0.3 s?
                        tail = [v for t, v, _ in vel if t > 0.7]
                        mean_err = sum(tail) / len(tail) - target if tail else float("nan")
                        uqmax = max(abs(u) for _, _, u in vel)
                        r = {"round": rnd, "mode": mode, "target": target, "start": round(start, 3), "iq_err_rms": rms,
                             "rise_s": rise, "overshoot": over, "end_err": mean_err, "uq_max": uqmax}
                        results.append(r)
                        print(f"mode {mode} ({MODES[mode]:31s}) {start:5.2f} -> {target:5.2f}: iq error RMS {rms:5.3f} A  "
                              f"rise {('%.3f s' % rise) if rise else '  -    '}  "
                              f"{'overshoot %5.1f %%' % over if abs(step) > 0.5 else 'max|v| %5.2f   ' % over}  "
                              f"end error {mean_err:6.3f}  max|uq| {uqmax:4.2f} V")
                    cmd("ME0", 0.3)
        finally:
            cmd("ME0", 0.3)
            cmd("M0")
            cmd("XK0")
    with open(a.out, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(results[0].keys()))
        w.writeheader(); w.writerows(results)
    print("\nmean per mode (steps 0 -> +-V only for rise / overshoot):")
    for mode in a.modes:
        rs = [r for r in results if r["mode"] == mode]
        steps = [r for r in rs if r["target"] != 0.0 and r["rise_s"]]
        rms = sum(r["iq_err_rms"] for r in rs) / len(rs)
        rise = sum(r["rise_s"] for r in steps) / len(steps) if steps else float("nan")
        over = sum(r["overshoot"] for r in steps) / len(steps) if steps else float("nan")
        endr = sum(abs(r["end_err"]) for r in rs) / len(rs)
        print(f"  mode {mode} {MODES[mode]:31s}: iq error RMS {rms:.3f} A, rise {rise:.3f} s, overshoot {over:5.1f} %, "
              f"|end error| {endr:.3f} rad/s")
    print("saved", a.out)


if __name__ == "__main__":
    main()
