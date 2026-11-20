#!/usr/bin/env python3
"""Mean sector time and stalls per electrical sector (hall_count mod 6) and per electrical period, from the
lowspeed_campaign.py --edges log.   python tools/stall_sectors.py data/2026-11-20/stall_map_edges.csv"""
import csv, math, sys
from collections import defaultdict
SM = 2*math.pi/90
runs = defaultdict(list)
for r in csv.DictReader(open(sys.argv[1])):
    runs[(float(r["target"]), int(r["pas"]))].append((float(r["t"]), int(r["sign"]), int(r["hall"]), int(r["hall_count"])))
for want in (1, -1):
    cls = defaultdict(list); st = defaultdict(set); per = defaultdict(list)
    for (tgt, pas), e in runs.items():
        if (tgt > 0) != (want > 0): continue
        f = [(t, h, hc) for t, sg, h, hc in e if sg == want]
        for (t1, h1, hc1), (t2, _, _) in zip(f, f[1:]):
            x = (t2 - t1) / (SM / abs(tgt))
            cls[hc1 % 6].append(x); st[hc1 % 6].add(h1)
            per[(hc1 % 90) // 6].append(x)
    print("forward" if want > 0 else "backward")
    for k in sorted(cls):
        v = cls[k]
        print(f"  sector class {k} hall state {sorted(st[k])}: mean {sum(v)/len(v):.2f} x nominal, stalls {sum(1 for x in v if x > 3)}/{len(v)}")
    print("  per electrical period (15 per turn), mean x nominal:", " ".join(f"{sum(v)/len(v):.2f}" for k, v in sorted(per.items())))
