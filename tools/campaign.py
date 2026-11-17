#!/usr/bin/env python3
"""
Measurement campaign over the serial port: for each torque mode, speed and compensation value,
runs the motor in open loop, records the phase A current on the board (R command) and saves it as CSV.
At the end: summary table (THD, zero crossing flatness) and one comparison plot per mode and speed.

THE MOTOR MOVES. Current-limited supply, motor fixed, someone next to the bench.
The heartbeat is enabled during the campaign: if this script stops, the board disables the motor.

Example:
  python tools/campaign.py --port COM5 --speeds 1 2 4 --comps 0 cal --modes current --via pyocd

  --comps accepts values in volts and "cal" (runs the calibration first and uses the result).
  --via serial: the recording is dumped over serial; --via pyocd: read from the board memory through
  the ST-Link (faster, needs the firmware ELF; close MCUViewer first). Commands and heartbeat always use serial.

In voltage mode the open loop amplitude (--ol-voltage) must exceed the back-EMF (~0.32 V per rad/s with
KV 17 and 15 pole pairs): speeds that are too high for it are skipped. Use the current mode for higher speeds.
"""
import argparse
import csv
import datetime
import math
import os
import re
import sys
import time

import serial  # pyserial

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from compare_currents import analyse  # noqa: E402

import numpy as np  # noqa: E402
import matplotlib  # noqa: E402

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402

MODES = {"voltage": "MT0", "current": "MT2"}
POLE_PAIRS = 15
RECORD_SAMPLES = 800          # include/config.h
PERIODS_PER_RECORD = 4
BEMF_V_PER_RADS = 0.32        # phase peak back-EMF per mechanical rad/s (KV 17, 15 pole pairs)


def angle_ripple(err):
    """standard deviation of the angle error around its circular mean [rad]"""
    mean = np.arctan2(np.mean(np.sin(err)), np.mean(np.cos(err)))
    d = np.angle(np.exp(1j * (err - mean)))
    return float(np.sqrt(np.mean(d ** 2)))


class Bench:
    def __init__(self, port, baud=115200):
        self.ser = serial.Serial(port, baud, timeout=0.1)
        self.last_ping = 0

    def send(self, cmd):
        self.ser.write((cmd + "\n").encode())
        self.last_ping = time.time()

    def ping(self):
        if time.time() - self.last_ping > 0.5:
            self.send("P")

    def readline(self):
        line = self.ser.readline().decode(errors="replace").strip()
        if line:
            print("  <", line)
        return line

    def wait(self, seconds):
        t0 = time.time()
        while time.time() - t0 < seconds:
            self.ping()
            line = self.readline()
            if "MOTOR DISABLED" in line and "recording done" not in line:
                raise RuntimeError(line)

    def wait_for(self, pattern, timeout):
        t0 = time.time()
        while time.time() - t0 < timeout:
            self.ping()
            line = self.readline()
            m = re.search(pattern, line)
            if m:
                return m
            if "MOTOR DISABLED" in line and "recording done" not in line:
                raise RuntimeError(line)
        raise TimeoutError(f"no '{pattern}' within {timeout} s")

    def record_pyocd(self, mem, period_us, timeout):
        self.send(f"RN{period_us}")
        t0 = time.time()
        time.sleep(0.2)
        while not mem.recording_done():
            if time.time() - t0 > timeout:
                raise TimeoutError("recording not finished")
            self.ping()
            self.readline()
        self.send("ME0")
        samples = mem.read_recording()
        header = list(samples[0].keys())
        return header, [[str(x[k]) for k in header] for x in samples]

    def record(self, period_us, timeout):
        self.send(f"R{period_us}")
        self.wait_for("REC_BEGIN", timeout)
        rows = []
        header = self.readline()
        while True:
            line = self.readline()
            if line == "REC_END":
                break
            if line:
                rows.append(line.split(","))
        return header.split(","), rows

    def close(self):
        try:
            self.send("ME0")
            self.send("XH0")
        finally:
            self.ser.close()


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--port", required=True)
    p.add_argument("--speeds", nargs="+", type=float, default=[0.5, 2.0], help="open loop velocities [rad/s]")
    p.add_argument("--comps", nargs="+", default=["0", "cal"], help='compensation values [V] or "cal"')
    p.add_argument("--modes", nargs="+", default=["voltage", "current"], choices=list(MODES))
    p.add_argument("--calib-current", type=float, default=1.5)
    p.add_argument("--settle", type=float, default=2.0, help="settling time before recording [s]")
    p.add_argument("--ol-voltage", type=float, default=0.3, help="open loop voltage amplitude in voltage mode [V]")
    p.add_argument("--via", choices=["serial", "pyocd"], default="serial", help="how to read the recordings")
    p.add_argument("--elf", default=os.path.join(".pio", "build", "esc1", "firmware.elf"))
    p.add_argument("--out", default=None, help="output folder (default data/<date-time>)")
    a = p.parse_args()

    out = a.out or os.path.join("data", datetime.datetime.now().strftime("%Y-%m-%d_%H%M%S"))
    os.makedirs(out, exist_ok=True)

    print("THE MOTOR WILL MOVE. Check: current-limited supply, motor fixed, you next to the bench.")
    if input("Start? [y/N] ").strip().lower() != "y":
        return

    bench = Bench(a.port)
    results = []
    mem = None
    if a.via == "pyocd":
        from board_mem import BoardMemory
        mem = BoardMemory(a.elf).__enter__()
    try:
        time.sleep(2)  # board reset on serial open
        bench.ser.reset_input_buffer()
        bench.send("ME0")
        bench.send("XC")
        bench.send("XH1")  # heartbeat: the board disables the motor if this script stops
        bench.send("MC3")  # velocity open loop
        bench.send(f"DB{a.ol_voltage}")

        comps = []
        for c in a.comps:
            if c == "cal":
                bench.send(f"DC{a.calib_current}")
                m = bench.wait_for(r"Measured compensation \[V\]: ([-\d.]+)", 60)
                comps.append(("cal", float(m.group(1))))
            else:
                comps.append((c, float(c)))

        for mode in a.modes:
            bench.send(MODES[mode])
            bench.send("DE1" if mode == "voltage" else "DE0")  # same fundamental amplitude in voltage mode
            for speed in a.speeds:
                if mode == "voltage" and speed * BEMF_V_PER_RADS > 0.8 * a.ol_voltage:
                    print(f"\n!! skipping {mode} {speed:g} rad/s: back-EMF ~{speed * BEMF_V_PER_RADS:.2f} V "
                          f"too close to the open loop voltage {a.ol_voltage} V")
                    continue
                f_el = speed * POLE_PAIRS / (2 * math.pi)
                period_us = int(PERIODS_PER_RECORD / f_el * 1e6 / RECORD_SAMPLES)
                for label, value in comps:
                    name = f"{mode}_v{speed:g}_comp{label}"
                    print(f"\n== {name} (comp {value:.4f} V)")
                    bench.send(f"D{value:.4f}")
                    bench.send(f"M{speed}")
                    bench.send("ME1")
                    bench.wait(a.settle)
                    timeout = PERIODS_PER_RECORD / f_el + 10
                    if mem is not None:
                        header, rows = bench.record_pyocd(mem, period_us, timeout)
                    else:
                        header, rows = bench.record(period_us, timeout)
                    path = os.path.join(out, name + ".csv")
                    with open(path, "w", newline="") as f:
                        w = csv.writer(f)
                        w.writerow(header)
                        w.writerows(rows)
                    t = np.array([float(r[0]) for r in rows]) * 1e-6
                    i = np.array([float(r[1]) for r in rows])
                    err = np.array([float(x[header.index("angle_err")]) for x in rows])
                    r = analyse(t, i)
                    results.append(dict(name=name, mode=mode, speed=speed, comp_label=label, comp=value,
                                        thd=r["thd"], flat=r["flat"], peak=r["peak"], f0=r["f0"], res=r,
                                        obs=angle_ripple(err)))
                    bench.wait(1.0)  # motor already disabled by the recorder, let it cool a bit
    finally:
        bench.close()
        if mem is not None:
            mem.__exit__(None, None, None)

    # summary
    summary = os.path.join(out, "summary.csv")
    with open(summary, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["name", "mode", "speed", "comp_V", "f_el_Hz", "peak_A", "THD_pct", "flat_pct", "obs_ripple_deg"])
        for r in results:
            w.writerow([r["name"], r["mode"], r["speed"], f"{r['comp']:.4f}", f"{r['f0']:.2f}",
                        f"{r['peak']:.3f}", f"{100 * r['thd']:.2f}", f"{100 * r['flat']:.1f}",
                        f"{np.degrees(r['obs']):.2f}"])
    print(f"\n{'name':32s} {'THD [%]':>8s} {'|i|<10% [%]':>12s} {'obs ripple [deg]':>17s}")
    for r in results:
        print(f"{r['name']:32s} {100 * r['thd']:8.2f} {100 * r['flat']:12.1f} {np.degrees(r['obs']):17.2f}")

    # one plot per mode and speed
    for mode in a.modes:
        for speed in a.speeds:
            sel = [r for r in results if r["mode"] == mode and r["speed"] == speed]
            if not sel:
                continue
            fig, ax = plt.subplots(figsize=(9, 5))
            for r in sel:
                res = r["res"]
                ax.plot(np.degrees(res["centers"]), res["wave"],
                        label=f"comp {r['comp']:.3f} V (THD {100 * r['thd']:.1f}%)")
            ax.axhline(0, color="grey", lw=0.5)
            ax.set_xlabel("electrical angle from rising zero crossing [deg]")
            ax.set_ylabel("phase A current [A]")
            ax.set_title(f"{mode} mode, {speed:g} rad/s")
            ax.legend()
            ax.grid(alpha=0.3)
            fig.tight_layout()
            fig.savefig(os.path.join(out, f"{mode}_v{speed:g}.png"), dpi=120)
            plt.close(fig)
    print(f"\nresults in {out}")


if __name__ == "__main__":
    main()
