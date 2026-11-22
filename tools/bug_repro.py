#!/usr/bin/env python3
"""
Reproduces the bugs of docs/audit-dev.md on the powered STM32F103 board: THE MOTOR MOVES (low values).
Each scenario sends bench / SimpleFOC commands through RTT, holds each step a given time while reading dbg
(uq, iq, phase currents, velocity, enabled) and prints a summary per step: does the target still change something,
does ME0 stop the motor, what happens at the next ME1. Some steps also start the recorder (one sample per loop).

Safety: the motor is disabled (ME0) if a phase current goes above --imax (3 A) or the velocity above --vmax
(10 rad/s), at the end of each scenario and on any error. The bench safety features (overcurrent cut, run timeout)
stay on.

  python tools/bug_repro.py --elf .pio/cloud/<run>/perf_dev_f103/firmware.elf --scenario mlu --out data/2026-11-22/mlu.csv
  scenarios: mlu mlc mwt mlv olneg hall dt596
"""
import argparse
import csv
import os
import sys
import time

sys.path.insert(0, os.path.dirname(__file__))
from board_mem import BoardMemory, RttPort  # noqa: E402

# closed loop torque, voltage mode, limit 0.6 V (~1 A stalled), target 0 (sensor aligned at boot on this board)
BASE_VOLT = ["ME0", "XR1", "M0", "MC0", "MT0", "MLU0.6", "MLC1", "MLV10", "ST0"]
BASE_CURR = ["ME0", "XR1", "M0", "MC0", "MT2", "MLU2", "MLC0.5", "MLV10", "ST0"]

# (label, command or None, hold [s], start the recorder [period us] or 0)
SCENARIOS = {
    # 11: negative voltage limit
    "mlu": (BASE_VOLT, [
        ("enabled, target 0", "ME1", 1.0, 0),
        ("MLU-0.5 (limit -0.5 V)", "MLU-0.5", 2.0, 0),
        ("target +0.3", "M0.3", 1.0, 0),
        ("target -0.3", "M-0.3", 1.0, 0),
        ("target 0", "M0", 1.0, 0),
        ("ME0", "ME0", 0.7, 0),
        ("ME1 again (limit still -0.5)", "ME1", 1.0, 0),
        ("ME0", "ME0", 0.5, 0),
        ("limit back to 0.6 V", "MLU0.6", 0.1, 0),
        ("enabled, target 0", "ME1", 1.0, 0),
    ]),
    # 11: negative current limit
    "mlc": (BASE_CURR, [
        ("enabled, target 0", "ME1", 1.0, 0),
        ("MLC-0.3 (limit -0.3 A)", "MLC-0.3", 2.0, 0),
        ("target +0.2", "M0.2", 1.0, 0),
        ("target -0.2", "M-0.2", 1.0, 0),
        ("target 0", "M0", 1.0, 0),
        ("ME0", "ME0", 0.7, 0),
        ("limit back to 0.5 A", "MLC0.5", 0.1, 0),
        ("enabled, target 0", "ME1", 1.0, 0),
    ]),
    # 12: invalid modulation type
    "mwt": (BASE_VOLT, [
        ("enabled, target 0.4 V", "M0.4", 0.1, 0),
        ("ME1", "ME1", 1.5, 0),
        ("MWT9 (invalid modulation)", "MWT9", 1.0, 0),
        ("target 0", "M0", 1.5, 0),
        ("target -0.4", "M-0.4", 1.0, 0),
        ("ME0", "ME0", 0.7, 0),
        ("ME1 again, target 0", "M0", 0.1, 0),
        ("ME1", "ME1", 1.0, 0),
        ("ME0", "ME0", 0.5, 0),
        ("MWT1 (SpaceVectorPWM back)", "MWT1", 0.1, 0),
        ("ME1, target 0", "ME1", 1.0, 0),
    ]),
    # 11: negative velocity limit, angle mode
    "mlv": (["ME0", "XR1", "M0", "MT0", "MLU0.6", "MLV2", "ST0", "MC2"], [
        ("angle mode, holds the present angle", "ME1", 1.5, 0),
        ("MLV-2 (velocity limit -2)", "MLV-2", 1.5, 0),
        ("target = present angle + 3 rad", "+3", 4.0, 0),
        ("ME0", "ME0", 0.5, 0),
        ("velocity limit back to 2", "MLV2", 0.1, 0),
    ]),
    # 1-2: angle open loop towards a negative angle (0.5 V applied: ~0.4 A)
    "olneg": (["ME0", "XR0", "M0", "MC4", "MT0", "MLV0.3"], [
        ("angle open loop 0.3 V, target 0", "ME1", 1.0, 0),
        ("target +0.3 rad", "M0.3", 2.0, 0),
        ("target -0.3 rad", "M-0.3", 3.0, 0),
        ("target -0.6 rad", "M-0.6", 1.5, 0),
        ("target +0.3 rad", "M0.3", 4.0, 0),
        ("ME0", "ME0", 0.5, 0),
    ]),
    # 10: hall glitch, closed loop velocity 1 rad/s, foc_current
    "hall": (["ME0", "VO", "MLC2", "MT2", "MC1", "M0", "ST0", "MVP0.4", "MVI1", "MVF0.01", "M1"], [
        ("velocity 1 rad/s", "ME1", 3.0, 0),
        ("XG0 (state 000, 1 loop), recorded", "XG0", 0.5, 400),
        ("XG7 (state 111, 1 loop), recorded", "XG7", 0.5, 400),
        ("XG0,5 (5 loops), recorded", "XG0,5", 0.5, 400),
        ("ME0", "ME0", 0.5, 0),
    ]),
    # simplefoc#596: dc_current for 0.1 s then back to foc_current (compare dev and the PR firmware)
    "dt596": (["ME0", "XR1", "M0", "MC0", "MT2", "MLU2", "MLC0.5", "ST0"], [
        ("foc_current, target 0.2 A", "M0.2", 0.1, 0),
        ("ME1", "ME1", 1.5, 0),
        ("MT1 (dc_current)", "MT1", 0.1, 0),
        ("MT2 back to foc_current, recorded", "MT2", 0.6, 400),
        ("MT1 (dc_current)", "MT1", 0.1, 0),
        ("MT2 back to foc_current, recorded", "MT2", 0.6, 400),
        ("ME0", "ME0", 0.5, 0),
    ]),
}


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--elf", required=True)
    p.add_argument("--scenario", required=True, choices=SCENARIOS)
    p.add_argument("--out", required=True, help="CSV of all dbg reads (and <out>_rec<k>.csv per recording)")
    p.add_argument("--imax", type=float, default=3.0)
    p.add_argument("--vmax", type=float, default=10.0)
    a = p.parse_args()
    setup, steps = SCENARIOS[a.scenario]
    os.makedirs(os.path.dirname(a.out) or ".", exist_ok=True)
    rows = []
    with BoardMemory(a.elf) as mem:
        port = RttPort(mem, timeout=0.1)

        def cmd(c, t=0.25):
            port.write((c + "\n").encode())
            end, out = time.time() + t, ""
            while time.time() < end:
                out += port.read(4096).decode(errors="replace")
                time.sleep(0.02)
            return out.strip().replace("\r", "").replace("\n", " | ")

        time.sleep(0.2)
        port.reset_input_buffer()
        try:
            cmd("ME0")
            # wait for the motor to stop (it may still coast after the previous scenario)
            t_wait = time.time()
            while abs(mem.read_dbg()["velocity"]) > 0.3 and time.time() - t_wait < 20:
                time.sleep(0.2)
            for c in setup:
                r = cmd(c)
                if "forced" in r or "only open loop" in r:
                    print("setup:", c, "->", r)
            print("modulation:", cmd("MWT"), "| X:", cmd("X", 0.6)[:160])
            t0 = time.time()
            nrec = 0
            for label, c, hold, rec_us in steps:
                if c == "+3":   # target relative to the present angle (angle mode)
                    # shaft angle is not in dbg: use the target set by MC2 (the present angle) + 3
                    import re
                    m_ = re.search(r"Target:\s*(-?[\d.]+)", cmd("M", 0.6))
                    if not m_:
                        raise RuntimeError("could not read the present target")
                    cur = float(m_.group(1))
                    c = f"M{cur + 3:.3f}"
                if rec_us:
                    cmd(f"RN{rec_us}", 0.05)
                reply = cmd(c, 0.05)
                t_step = time.time()
                stats = {"uq": 0.0, "i": 0.0, "v_sum": 0.0, "v_max": 0.0, "n": 0, "en": None,
                         "sp_min": 1e9, "sp_max": 0.0, "el_min": 1e9, "el_max": -1e9}
                while time.time() - t_step < hold:
                    d = mem.read_dbg()
                    # duty cycles really applied: TIM1 compare registers (6PWM on TIM1), in % of the period
                    tim = mem.target
                    arr = tim.read32(0x40012C2C) + 1
                    duty = [100.0 * tim.read32(0x40012C00 + off) / arr for off in (0x34, 0x38, 0x3C)]
                    spread = max(duty) - min(duty)
                    t = time.time() - t0
                    imax = max(abs(d["ia"]), abs(d["ib"]), abs(d["ic"])) if d["enabled"] else 0.0
                    rows.append({"t": t, "step": label, **{k: d[k] for k in ("uq", "ud", "iq", "id", "ia", "ib",
                                 "velocity", "electrical_angle", "enabled", "fault", "hall", "hall_count")},
                                 "duty_a": duty[0], "duty_b": duty[1], "duty_c": duty[2]})
                    if d["enabled"]:
                        stats["uq"] = max(stats["uq"], abs(d["uq"]))
                        stats["i"] = max(stats["i"], imax)
                    stats["v_sum"] += d["velocity"]; stats["n"] += 1
                    stats["v_max"] = max(stats["v_max"], abs(d["velocity"]))
                    stats["en"] = d["enabled"]
                    if d["enabled"]:
                        stats["sp_min"] = min(stats["sp_min"], spread); stats["sp_max"] = max(stats["sp_max"], spread)
                    stats["el_min"] = min(stats["el_min"], d["electrical_angle"])
                    stats["el_max"] = max(stats["el_max"], d["electrical_angle"])
                    if imax > a.imax or abs(d["velocity"]) > a.vmax or d["fault"]:
                        cmd("ME0", 0.1)
                        raise RuntimeError(f"abort in '{label}': imax {imax:.2f} A, velocity {d['velocity']:.2f}, "
                                           f"fault {d['fault']}")
                n = max(stats["n"], 1)
                print(f"{label:45s} enabled {stats['en']}  max|uq| {stats['uq']:5.2f} V  max|i| {stats['i']:5.2f} A  "
                      f"mean vel {stats['v_sum'] / n:6.2f}  max|vel| {stats['v_max']:5.2f}  "
                      f"duty spread {stats['sp_min'] if stats['sp_min'] < 1e8 else 0:5.2f}-{stats['sp_max']:5.2f} %  "
                      f"el.angle {stats['el_min']:7.2f}..{stats['el_max']:7.2f}  reply: {reply[:40]}")
                if rec_us:
                    for _ in range(50):
                        if mem.recording_done():
                            break
                        time.sleep(0.05)
                    rec = mem.read_recording()
                    nrec += 1
                    path = a.out.replace(".csv", f"_rec{nrec}.csv")
                    with open(path, "w", newline="") as f:
                        w = csv.DictWriter(f, fieldnames=list(rec[0].keys()))
                        w.writeheader(); w.writerows(rec)
                    iqs = [s["iq"] for s in rec]
                    ias = [abs(s["ia"]) for s in rec]
                    print(f"   recording {len(rec)} samples: iq min {min(iqs):.3f} max {max(iqs):.3f}, "
                          f"max|ia| {max(ias):.2f} A -> {path}")
                    # hall glitch tests: comp_a carries the hall state (BENCH_NO_DEADTIME builds)
                    bad = [k for k, smp in enumerate(rec) if smp["comp_a"] in (0.0, 7.0)]
                    if bad:
                        lo, hi = max(0, bad[0] - 4), min(len(rec), bad[-1] + 8)
                        print(f"   invalid hall state in samples {bad[0]}..{bad[-1]}; around it:")
                        print("     hall:", " ".join(f"{int(rec[k]['comp_a']):5d}" for k in range(lo, hi)))
                        print("     iq  :", " ".join(f"{rec[k]['iq']:5.2f}" for k in range(lo, hi)))
                        print("     ia  :", " ".join(f"{rec[k]['ia']:5.2f}" for k in range(lo, hi)))
        finally:
            cmd("ME0", 0.3)
            cmd("M0")
            cmd("XR0"); cmd("MLU0.3"); cmd("MLC2"); cmd("MLV10"); cmd("MT2"); cmd("MWT1")   # bench defaults
    with open(a.out, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        w.writeheader(); w.writerows(rows)
    print("saved", a.out)


if __name__ == "__main__":
    main()
