#!/usr/bin/env python3
"""
Hall sensor calibration, VESC style: THE MOTOR MOVES (slowly, rotor dragged by a rotating current vector).

The field is turned in angle open loop with current control (MC4 + MT2: the current PI imposes the current
vector at the commanded angle, so the dead time does not bias it). The rotor follows the field. The PC reads
the commanded electrical angle and the hall state with pyOCD (~300 reads/s) and records the angle of every hall
transition, forward then backward. Forward/backward averaging cancels the rotor lag (friction, cogging) and the
hall hysteresis, which change sign with the direction.

Angles are rotor electrical angles in the SimpleFOC convention: with a q current at the open loop angle theta,
the field (and the rotor d axis) is at theta + pi/2.

  python tools/hall_calib.py --env smooth_f103_dev                 # defaults: 1.5 A, 4 electrical turns, 6 s/turn
  python tools/hall_calib.py --env smooth_f103_dev --current 1.0 --turns 3

Each direction is a separate enable (< 30 s, the bench run timeout). Aborts on a fault, a phase current above
--max-current, an invalid hall state (0/7), or if the rotor stops following (no transition for too long).
"""
import argparse
import math
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from board_mem import BoardMemory, RttPort  # noqa: E402

POLE_PAIRS = 15
TWO_PI = 2 * math.pi
# SimpleFOC HallSensor: hall state (CBA bits) -> electric sector
ELECTRIC_SECTORS = [-1, 0, 4, 5, 2, 1, 3, -1]


def wrap(a):
    return a % TWO_PI


def deg(a):
    return math.degrees(a)


def circ_mean(angles):
    s = sum(math.sin(a) for a in angles)
    c = sum(math.cos(a) for a in angles)
    return wrap(math.atan2(s, c))


def circ_diff(a, b):
    """a - b in (-pi, pi]"""
    return (a - b + math.pi) % TWO_PI - math.pi


class Abort(Exception):
    pass


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--env", default="smooth_f103_dev", help="PlatformIO env of the flashed firmware (for the ELF)")
    p.add_argument("--current", type=float, default=1.5, help="current amplitude [A] (open loop MT2 uses the current limit)")
    p.add_argument("--turns", type=int, default=4, help="electrical turns per direction")
    p.add_argument("--period", type=float, default=6.0, help="seconds per electrical turn")
    p.add_argument("--max-current", type=float, default=3.0, help="abort above this phase current [A]")
    p.add_argument("--csv", default=None, help="save the transitions to this CSV file")
    a = p.parse_args()
    if a.current > 2.0:
        sys.exit("current above 2 A: not allowed by this script")
    run_s = a.turns * a.period + 3
    if run_s > 28:
        sys.exit(f"one direction would take {run_s:.0f} s, above the 30 s bench run timeout: fewer turns or a shorter period")
    vel_mech = TWO_PI / (a.period * POLE_PAIRS)          # mechanical rad/s
    sweep_mech = a.turns * TWO_PI / POLE_PAIRS           # mechanical rad per direction
    elf = os.path.join(".pio", "build", a.env, "firmware.elf")

    with BoardMemory(elf) as mem:
        port = RttPort(mem, timeout=0.1)

        def cmd(c, t=0.3):
            port.write((c + "\n").encode())
            end, out = time.time() + t, ""
            while time.time() < end:
                out += port.read(4096).decode(errors="replace")
                time.sleep(0.02)
            return out

        def shaft_angle():
            m = re.findall(r"[-+]?\d+\.\d+", cmd("MMG6", 0.3))
            if not m:
                raise Abort("cannot read the shaft angle (MMG6)")
            return float(m[-1])

        transitions = []  # (direction, from_state, to_state, rotor_angle)
        samples = []      # (direction, state, rotor_angle) of every read inside a complete sector
        try:
            time.sleep(0.2)
            port.reset_input_buffer()
            cmd("ME0", 0.3)
            for c in ["MC4", "MT2", f"MLC{a.current}", f"MLV{vel_mech:.5f}"]:
                cmd(c)
            for direction in (+1, -1):
                start = shaft_angle()
                cmd(f"M{start:.5f}")                 # target = current open loop angle: no jump of the setpoint
                cmd("ME1", 0.3)
                time.sleep(1.5)                      # rotor snaps to the field and settles
                d = mem.read_dbg()
                prev_state, prev_theta = d["hall"], d["electrical_angle"]
                print(f"{'forward' if direction > 0 else 'backward'}: start angle {start:.4f} rad, hall {prev_state}", flush=True)
                cmd(f"M{start + direction * sweep_mech:.5f}", 0.05)
                t0 = last_tr = time.time()
                segment = None  # [direction, state, rotor angles read in this state, entered by a transition]
                while time.time() - t0 < a.turns * a.period + 1.0:
                    d = mem.read_dbg()
                    if d["fault"]:
                        raise Abort(f"fault {d['fault']}")
                    if max(abs(d["ia"]), abs(d["ib"])) > a.max_current:
                        raise Abort(f"phase current ia {d['ia']:.2f} ib {d['ib']:.2f} A")
                    s, theta = d["hall"], d["electrical_angle"]
                    if s in (0, 7):
                        raise Abort(f"invalid hall state {s}")
                    if s != prev_state:
                        # transition between the two reads: take the middle of the two commanded angles
                        mid = wrap(prev_theta + circ_diff(theta, prev_theta) / 2)
                        transitions.append((direction, prev_state, s, wrap(mid + math.pi / 2)))
                        # samples of the sector just left count only if it was entered by a transition (complete)
                        if segment and segment[0] == direction and segment[1] == prev_state and len(segment[2]) >= 10:
                            samples.extend((direction, prev_state, x) for x in segment[2])
                        segment = [direction, s, [], True]
                        prev_state, last_tr = s, time.time()
                    elif segment:
                        segment[2].append(wrap(theta + math.pi / 2))
                    if s == prev_state and time.time() - last_tr > a.period / 6 * 3 and time.time() - t0 > 2:
                        raise Abort("no hall transition for 3 expected sectors: the rotor does not follow")
                    prev_theta = theta
                cmd("ME0", 0.4)
                time.sleep(0.5)
        except Abort as e:
            print("ABORT:", e)
        finally:
            cmd("M0", 0.2)
            print("ME0 ->", cmd("ME0", 0.4).strip())
            cmd("MLC2", 0.2)
            cmd("MLV10", 0.2)
            print("X ->", cmd("X", 0.8).strip().replace("\n", " | "))

    if a.csv:
        with open(a.csv, "w") as f:
            f.write("direction,from,to,rotor_angle_rad\n")
            for t in transitions:
                f.write(f"{t[0]},{t[1]},{t[2]},{t[3]:.5f}\n")
    report(transitions, samples)


def report(transitions, samples=()):
    if not transitions:
        print("no transition recorded")
        return
    fw = [t for t in transitions if t[0] > 0]
    bw = [t for t in transitions if t[0] < 0]
    print(f"\n{len(fw)} forward and {len(bw)} backward transitions")
    seq = [t[2] for t in fw[:7]]
    print("forward state sequence:", seq, " sectors:", [ELECTRIC_SECTORS[s] for s in seq])
    # each sensor must be seen low and high (SimpleFOC: hall_state = C + (B << 1) + (A << 2))
    seen = {t[1] for t in transitions} | {t[2] for t in transitions}
    for name, bit in (("A", 2), ("B", 1), ("C", 0)):
        levels = {(s >> bit) & 1 for s in seen}
        print(f"hall {name}: {'OK, toggles' if levels == {0, 1} else 'STUCK at ' + str(levels.pop()) + ' (sensor or wiring fault?)'}")
    missing = sorted(set(range(1, 7)) - seen)
    if missing:
        print(f"states never seen: {missing} (120 deg sensors give states 1..6; a 60 deg layout gives other states)")

    # an edge is the boundary between two states, whatever the direction
    edges = {}
    for d, f, to, ang in transitions:
        key = tuple(sorted((f, to)))
        edges.setdefault(key, {+1: [], -1: []})[d].append(ang)
    rows = []
    for key, v in edges.items():
        if not v[+1] or not v[-1]:
            continue
        mf, mb = circ_mean(v[+1]), circ_mean(v[-1])
        mean = wrap(mb + circ_diff(mf, mb) / 2)
        rows.append((mean, key, mf, mb, circ_diff(mf, mb), len(v[+1]), len(v[-1]),
                     max(abs(circ_diff(x, mf)) for x in v[+1]), max(abs(circ_diff(x, mb)) for x in v[-1])))
    if len(rows) < 6:
        print(f"only {len(rows)} edges seen in both directions: not enough for the table")
    if not rows:
        return
    rows.sort()
    # ideal sensor: edges every 60 deg; offset = circular mean of (edge - k*60)
    ideal_offset = circ_mean([r[0] - k * math.pi / 3 for k, r in enumerate(rows)])
    print("\nedge (states)  forward  backward  fwd-bwd   mean    spread fwd/bwd   spacing   error vs ideal 60 deg grid")
    for k, r in enumerate(rows):
        mean, key, mf, mb, hyst, nf, nb, sf, sb = r
        nxt = rows[(k + 1) % len(rows)][0]
        spacing = wrap(nxt - mean)
        err = circ_diff(mean, wrap(ideal_offset + k * math.pi / 3))
        print(f"  {key[0]}-{key[1]}       {deg(mf):7.1f}  {deg(mb):7.1f}  {deg(hyst):+6.1f}  {deg(mean):7.1f}"
              f"   {deg(sf):4.1f} / {deg(sb):4.1f}      {deg(spacing):5.1f}     {deg(err):+5.1f}   (n {nf}/{nb})")
    errs = [abs(circ_diff(r[0], wrap(ideal_offset + k * math.pi / 3))) for k, r in enumerate(rows)]
    print(f"\nideal grid offset {deg(ideal_offset):.1f} deg; max edge error {deg(max(errs)):.1f} deg electrical"
          f" ({deg(max(errs)) / POLE_PAIRS:.2f} deg mechanical)")

    # sector centres two ways: (1) mean of all angles read in the state (VESC-like, assumes +-30 deg around it),
    # (2) middle of the two measured edges (gives the real width too). Same centre = symmetric sector.
    edge_mean = {r[1]: r[0] for r in rows}
    print("\nstate  centre from samples fwd / bwd / mean   centre from edges   width   difference")
    for s in sorted({t[1] for t in samples} | {k for key in edge_mean for k in key}):
        f = [x for d, st, x in samples if st == s and d > 0]
        b = [x for d, st, x in samples if st == s and d < 0]
        line = f"  {s}    "
        cm = None
        if f and b:
            cf, cb = circ_mean(f), circ_mean(b)
            cm = wrap(cb + circ_diff(cf, cb) / 2)
            line += f"{deg(cf):6.1f} / {deg(cb):6.1f} / {deg(cm):6.1f}      "
        else:
            line += "  (not enough samples)          "
        bounds = [m for key, m in edge_mean.items() if s in key]
        if len(bounds) == 2:
            e1, e2 = bounds
            # the state lies on the arc between its two edges that does not contain the other edges: the shorter one
            arc = wrap(e2 - e1)
            lo, width = (e1, arc) if arc <= math.pi else (e2, TWO_PI - arc)
            ce = wrap(lo + width / 2)
            line += f"{deg(ce):6.1f}          {deg(width):5.1f}"
            if cm is not None:
                line += f"   {deg(circ_diff(cm, ce)):+5.1f}"
        print(line)
    print("fwd-bwd = rotor lag + hall hysteresis (removed by the mean); spread = repeatability over the turns")


if __name__ == "__main__":
    main()
