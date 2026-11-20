#!/usr/bin/env python3
"""
Map the low speed stalls on the rotor position, from the edge log of lowspeed_campaign.py --edges.

The position is the hall sector count since boot (dbg.hall_count) modulo one mechanical turn (6 x pole pairs = 90
sectors on the hoverboard motor). No absolute encoder: the reference is wherever the rotor was at power-up, so it
only holds while the board stays powered; it is enough to see whether the stalls come back at the same place on
every turn (position dependent friction / cogging / a tight spot) or anywhere (control loop).

For each sector position: number of visits, mean time spent in it relative to the nominal sector time, and stalls
(time > 3x nominal). A position dependent cause shows as a few sectors with long times on every turn, in both runs
of the same direction.

  python tools/stall_map.py data/2026-11-20/stall_map_edges.csv
"""
import csv
import math
import sys
from collections import defaultdict

POLE_PAIRS = 15
TURN = 6 * POLE_PAIRS
SECTOR_MECH = 2 * math.pi / TURN


def main(path):
    runs = defaultdict(list)
    with open(path) as f:
        for r in csv.DictReader(f):
            runs[(r["config"], float(r["target"]), int(r["pas"]))].append(
                (float(r["t"]), int(r["sign"]), int(r["hall_count"])))

    by_dir = {1: defaultdict(list), -1: defaultdict(list)}   # position -> list of (time / nominal)
    turns = {1: 0.0, -1: 0.0}
    for (cfg, tgt, pas), edges in runs.items():
        want = 1 if tgt > 0 else -1
        nominal = SECTOR_MECH / abs(tgt)
        fwd = [(t, hc) for t, sg, hc in edges if sg == want]
        for (t1, hc1), (t2, _) in zip(fwd, fwd[1:]):
            by_dir[want][hc1 % TURN].append((t2 - t1) / nominal)
        if len(fwd) > 1:
            turns[want] += (len(fwd) - 1) / TURN

    for d, name in ((1, "forward"), (-1, "backward")):
        pos = by_dir[d]
        if not pos:
            continue
        allr = [x for v in pos.values() for x in v]
        stalls = sum(1 for x in allr if x > 3)
        print(f"\n{name}: {turns[d]:.1f} turns, {len(allr)} sectors, {stalls} stalls, mean time {sum(allr) / len(allr):.2f} x nominal")
        # sectors ranked by mean time
        stats = []
        for p, v in pos.items():
            stats.append((sum(v) / len(v), p, len(v), sum(1 for x in v if x > 3), max(v)))
        stats.sort(reverse=True)
        print("  slowest positions (position / visits / mean x nominal / stalls / max x nominal):")
        for m, p, n, s, mx in stats[:10]:
            print(f"    {p:3d}  {n:3d}  {m:5.2f}  {s:3d}  {mx:5.2f}")
        # concentration: share of the stalls in the 10 % slowest positions vs uniform
        top = {p for _, p, _, _, _ in stats[:max(1, len(stats) // 10)]}
        st_top = sum(1 for p, v in pos.items() if p in top for x in v if x > 3)
        if stalls:
            print(f"  stalls in the 10 % slowest positions: {st_top}/{stalls} ({100 * st_top / stalls:.0f} %, uniform ~10 %)")
        # repeatability: correlation of the per position mean time between the first and second half of the visits
        a, b = [], []
        for p, v in pos.items():
            if len(v) >= 2:
                h = len(v) // 2
                a.append(sum(v[:h]) / h)
                b.append(sum(v[h:]) / (len(v) - h))
        if len(a) > 3:
            ma, mb = sum(a) / len(a), sum(b) / len(b)
            cov = sum((x - ma) * (y - mb) for x, y in zip(a, b))
            sa = math.sqrt(sum((x - ma) ** 2 for x in a))
            sb = math.sqrt(sum((y - mb) ** 2 for y in b))
            if sa and sb:
                print(f"  repeatability (correlation of the per position time, first vs second half of the visits): {cov / (sa * sb):+.2f}")


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "data/stall_map_edges.csv")
