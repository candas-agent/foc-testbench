#!/usr/bin/env python3
"""
Low speed campaign: THE MOTOR MOVES. Compares velocity estimates and a position ramp with one protocol.
  --dt 0,0.38 (dt_f103 firmware): dead-time compensation A/B, C0 once per compensation value.

Configurations (STM32F103 bench firmware with lib/testbench/src/hall_table.*):
  C0  SmoothingSensor (HallSensor velocity)                       ST0, velocity mode
  V0..V4  HallTableSensor ideal grid, velocity estimate SV0..SV4   ST2 SVn, velocity mode
  R20 / R5  SmoothingSensor, velocity by position ramp, P_angle 20 / 5   ST0, V<target>
Each configuration runs 2 rad/s (10 s), -2 rad/s (10 s), 5 rad/s (8 s), each a separate enable, twice
(second pass in reverse order). Velocity PI P 0.4 I 1 Tf 0.01.

Metrics independent of the estimate: hall edge intervals seen by the PC (cv, stalls = intervals > 3x nominal),
iq ripple. The estimated velocity is printed too, but each configuration estimates it differently.

  python tools/lowspeed_campaign.py --env smooth_f103_dev --csv data/lowspeed_campaign.csv
"""
import argparse
import math
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from board_mem import BoardMemory, RttPort  # noqa: E402

POLE_PAIRS = 15
CONFIGS = [  # name, setup commands, ramp
    ("C0 SmoothingSensor", ["ST0"], False),
    ("V1 HallSensor+decay", ["ST2", "SV1"], False),
    ("V2 last sector+decay", ["ST2", "SV2"], False),
    ("V4 V3 low / Hall high", ["ST2", "SV4"], False),
]
ALL_CONFIGS = CONFIGS
TARGETS = [(2.0, 10.0), (-2.0, 10.0), (5.0, 8.0)]
# hall state sequence when the rotor turns in the positive direction (hall calibration of 2026-11-20)
FWD_SEQ = [2, 6, 4, 5, 1, 3]
FWD_NEXT = {FWD_SEQ[k]: FWD_SEQ[(k + 1) % 6] for k in range(6)}
SECTOR_MECH = 2 * math.pi / (6 * POLE_PAIRS)
SETTLE = 2.0


class Abort(Exception):
    pass


def st(v):
    n = len(v)
    m = sum(v) / n
    return m, math.sqrt(sum((x - m) ** 2 for x in v) / n)


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--env", default="smooth_f103_dev")
    p.add_argument("--csv", default=None)
    p.add_argument("--passes", type=int, default=3)
    p.add_argument("--configs", default=None, help="comma separated name prefixes to run, e.g. C0,V1")
    p.add_argument("--dt", default=None,
                   help="dead-time A/B (dt_f103 firmware): comma separated compensations [V], e.g. 0,0.3; "
                        "runs C0 once per value instead of the configurations")
    p.add_argument("--dt-threshold", type=float, default=None, help="deadtime_comp_threshold [A] (DT command)")
    p.add_argument("--targets", default=None,
                   help="velocity:seconds list replacing the default targets, e.g. 1:15,-1:15,0.5:20")
    a = p.parse_args()
    elf = os.path.join(".pio", "build", a.env, "firmware.elf")
    global CONFIGS, TARGETS
    if a.targets:
        TARGETS = [(float(t.split(":")[0]), float(t.split(":")[1])) for t in a.targets.split(",")]
    if a.configs:
        CONFIGS = [c for c in CONFIGS if any(c[0].startswith(x) for x in a.configs.split(","))]
    if a.dt:
        # entry "<volts>" or "<volts>@<threshold A>"; the threshold is set for every configuration (it persists)
        CONFIGS = []
        for e in a.dt.split(","):
            v, _, th = e.partition("@")
            th = th or str(a.dt_threshold if a.dt_threshold else 0.2)
            CONFIGS.append((f"C0 dt {v} th {th}", ["ST0", "DM0", f"D{v}", f"DT{th}"], False))
    rows = []
    with BoardMemory(elf) as mem:
        port = RttPort(mem, timeout=0.1)

        def cmd(c, t=0.3):
            port.write((c + "\n").encode())
            end, out = time.time() + t, ""
            while time.time() < end:
                out += port.read(4096).decode(errors="replace")
                time.sleep(0.02)
            return out

        def run(target, dur):
            t0 = time.time()
            v, iq, edges, last_h = [], [], [], None
            while time.time() - t0 < dur:
                d = mem.read_dbg()
                if abs(d["velocity"]) > 20 or d["fault"] or max(abs(d["ia"]), abs(d["ib"])) > 3.5:
                    raise Abort(f"vel {d['velocity']:.1f} fault {d['fault']} ia {d['ia']:.1f} ib {d['ib']:.1f}")
                now = time.time()
                if now - t0 > SETTLE:
                    v.append(d["velocity"])
                    iq.append(d["iq"])
                    h = d["hall"]
                    if last_h is not None and h != last_h:
                        sign = 1 if FWD_NEXT.get(last_h) == h else (-1 if FWD_NEXT.get(h) == last_h else 0)
                        edges.append((now, sign))
                last_h = d["hall"]
            tot = sum(sg for _, sg in edges)
            want = (1 if tot > 0 else -1) if tot else (1 if target > 0 else -1)  # motion direction from the edges themselves
            nominal = SECTOR_MECH / abs(target)
            fwd = [t for t, sg in edges if sg == want]          # edges in the commanded direction
            back = sum(1 for _, sg in edges if sg == -want)      # edges against it (jitter / reversals)
            bad = sum(1 for _, sg in edges if sg == 0)           # skipped states (should be 0)
            rev = sum(1 for (_, a1), (_, a2) in zip(edges, edges[1:]) if a1 and a2 and a1 != a2)
            net = sum(sg for _, sg in edges) * want
            win = dur - SETTLE
            true_v = net * SECTOR_MECH / win
            iv = [b - c for c, b in zip(fwd, fwd[1:])]
            mv, sv = st(v)
            _, si = st(iq)
            if len(iv) >= 3:
                mi, sdi = st(iv)
                cv, stalls, mx = sdi / mi, sum(1 for x in iv if x > 3 * nominal), max(iv)
            else:
                cv, stalls, mx = float("nan"), -1, float("nan")
            return dict(vel_mean=mv, vel_sd=sv, iq_sd=si, edges=len(edges), back=back, bad=bad, rev=rev, net=net,
                        true_v=true_v, cv=cv, stalls=stalls, max_ms=1000 * mx, expected_edges=win / nominal)

        try:
            time.sleep(0.2)
            port.reset_input_buffer()
            for c in ["ME0", "VO", "MVP0.4", "MVI1", "MVF0.01", "MLC2", "MT2", "XF0"]:
                cmd(c)
            for pas in range(a.passes):
                k = pas % len(CONFIGS)
                order = CONFIGS[k:] + CONFIGS[:k]                 # rotate the order between passes
                for name, setup, ramp in order:
                    for tgt, dur in TARGETS:
                        cmd("ME0", 0.4)
                        cmd("VO")
                        for c in setup:
                            cmd(c)
                        if ramp:
                            cmd(f"V{tgt}")
                        else:
                            cmd("MC1")
                            cmd(f"M{tgt}")
                        cmd("ME1", 0.1)
                        r = run(tgt, dur)
                        cmd("ME0", 0.4)
                        cmd("VO")
                        r.update(config=name, target=tgt, pas=pas + 1)
                        rows.append(r)
                        print(f"pass {pas + 1} {name:24s} {tgt:+.0f}: net {r['net']:4d}/{r['expected_edges']:4.0f} "
                              f"true v {r['true_v']:+.2f} back {r['back']:3d} rev {r['rev']:3d} bad {r['bad']} | "
                              f"cv {100 * r['cv']:4.0f}% stalls {r['stalls']:3d} max {r['max_ms']:5.0f} ms | iq sd {r['iq_sd']:.2f}"
                              f" | est. vel {r['vel_mean']:+.2f} sd {r['vel_sd']:.2f}", flush=True)
                        time.sleep(0.5)
        except Abort as e:
            print("ABORT:", e, flush=True)
        finally:
            cmd("ME0", 0.4)
            cmd("VO")
            cmd("ST0")
            if a.dt:
                cmd("D0")
            cmd("MAP20")
            print("X ->", cmd("X", 0.8).strip().replace("\n", " | "))

    if a.csv and rows:
        keys = ["pas", "config", "target", "edges", "net", "back", "rev", "bad", "true_v", "expected_edges", "cv", "stalls",
                "max_ms", "iq_sd", "vel_mean", "vel_sd"]
        with open(a.csv, "w") as f:
            f.write(",".join(keys) + "\n")
            for r in rows:
                f.write(",".join(str(r[k]) for k in keys) + "\n")
    summary(rows)


def summary(rows):
    print("\nSUMMARY: mean (min-max over the passes)")
    def agg(rr, k, scale=1, fmt="{:.2f}"):
        x = [r[k] * scale for r in rr]
        return (fmt + " ({}-{})").format(sum(x) / len(x), fmt.format(min(x)), fmt.format(max(x)))
    for tgt, _ in TARGETS:
        print(f"\n target {tgt:+.0f} rad/s")
        for name, _, _ in CONFIGS:
            rr = [r for r in rows if r["config"] == name and r["target"] == tgt]
            if not rr:
                continue
            print(f"  {name:24s} true v {agg(rr, 'true_v')} | stalls {agg(rr, 'stalls', fmt='{:.1f}')} | back {agg(rr, 'back', fmt='{:.0f}')}"
                  f" | cv {agg(rr, 'cv', 100, '{:.0f}')}% | iq sd {agg(rr, 'iq_sd')}")


if __name__ == "__main__":
    main()
