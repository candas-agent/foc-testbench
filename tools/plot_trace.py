#!/usr/bin/env python3
"""
Speed and current against time for several trace_run.py recordings (one row per recording).
Speed: real speed from the hall edges (one sector = 2 pi / 90 rad, speed = sector / time between two edges),
independent of the firmware velocity estimate. Current: measured iq.

  python tools/plot_trace.py --out docs/figures/f103_trace_2rads.png \
      "SimpleFOC dev=data/2026-11-20/trace_dev.csv" "+ compensation=data/2026-11-20/trace_dt.csv" ...
"""
import argparse
import csv
import math
import os

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402

POLE_PAIRS = 15
SECTOR_MECH = 2 * math.pi / (6 * POLE_PAIRS)
FWD_SEQ = [2, 6, 4, 5, 1, 3]
FWD_NEXT = {FWD_SEQ[k]: FWD_SEQ[(k + 1) % 6] for k in range(6)}
COLORS = ["#8a8984", "#eb6834", "#2a78d6", "#1baf7a"]
INK, MUTED, GRID = "#1f1f1e", "#6b6a64", "#e4e3dd"


def load(path):
    with open(path) as f:
        rows = [r for r in csv.DictReader(line for line in f if not line.startswith("#"))]
    return [{k: float(v) for k, v in r.items()} for r in rows]


def edge_speed(rows, t_from):
    """(times, speeds): one point per hall edge, speed of the sector that just ended (signed)."""
    ts, vs, last_h, last_t = [], [], None, None
    for r in rows:
        h = int(r["hall"])
        if last_h is not None and h != last_h:
            sign = 1 if FWD_NEXT.get(last_h) == h else (-1 if FWD_NEXT.get(h) == last_h else 0)
            if last_t is not None and sign and r["t"] >= t_from:
                ts.append(r["t"])
                vs.append(sign * SECTOR_MECH / (r["t"] - last_t))
            last_t = r["t"]
        last_h = h
    return ts, vs


def edge_speed_logged(path, t_from, rows=None):
    """Same as edge_speed, from the firmware hall edge log (exact board times). Board time is mapped to the PC
    time of the dbg trace with the smallest (read time - edge time) offset: within a few ms of the real one."""
    with open(path) as f:
        ev = [(int(r["t_us"]), int(r["count"]), float(r["pc_t"])) for r in csv.DictReader(f)]
    if len(ev) < 2:
        return [], []
    # unwrap micros() and align on the PC time base
    t, last, wraps = [], ev[0][0], 0
    for t_us, _, _ in ev:
        if t_us < last - (1 << 31):
            wraps += 1
        last = t_us
        t.append((t_us + wraps * (1 << 32)) / 1e6)
    off = min(pc - tb for tb, (_, _, pc) in zip(t, ev))
    # the hall count follows the sensor direction (SENSOR_DIRECTION, -1 on the F103 bench): orient it like the
    # motor velocity, using the sign of the firmware velocity estimate over the run
    sgn = 1
    if rows:
        mv = sum(r["velocity"] for r in rows) / len(rows)
        if (ev[-1][1] - ev[0][1]) * mv < 0:
            sgn = -1
    ts, vs = [], []
    for k in range(1, len(ev)):
        dc = ev[k][1] - ev[k - 1][1]
        tk = t[k] + off
        if dc and tk >= t_from:
            ts.append(tk)
            vs.append(sgn * dc * SECTOR_MECH / (t[k] - t[k - 1]))   # dc = +-1 (more if an edge was missed)
    return ts, vs


def main():
    p = argparse.ArgumentParser()
    p.add_argument("traces", nargs="+", help="label=path")
    p.add_argument("--out", required=True)
    p.add_argument("--target", type=float, default=2.0)
    p.add_argument("--start", type=float, default=2.0, help="skip the start-up [s]")
    p.add_argument("--window", type=float, default=4.0, help="seconds shown")
    a = p.parse_args()
    plt.rcParams.update({"font.size": 10, "axes.edgecolor": MUTED, "xtick.color": MUTED, "ytick.color": MUTED,
                         "text.color": INK})
    n = len(a.traces)
    fig, axes = plt.subplots(n, 2, figsize=(13, 2.3 * n + 1.3), sharex=True, gridspec_kw={"width_ratios": [1.25, 1]})
    if n == 1:
        axes = [axes]
    t1 = a.start + a.window
    for k, spec in enumerate(a.traces):
        label, path = spec.split("=", 1)
        rows = [r for r in load(path) if r["t"] <= t1 + 0.5]
        col = COLORS[k % len(COLORS)]
        ep = os.path.splitext(path)[0] + "_edges.csv"
        ts, vs = edge_speed_logged(ep, a.start, rows) if os.path.exists(ep) else edge_speed(rows, a.start)
        src = "firmware edge log" if os.path.exists(ep) else "PC polling"
        sel = [(t, v) for t, v in zip(ts, vs) if t <= t1]
        stalls = sum(1 for _, v in sel if 0 < abs(v) < abs(a.target) / 3)
        ax = axes[k][0]
        ax.axhline(a.target, color=INK, linewidth=1, linestyle=(0, (4, 3)), zorder=1)
        ax.step([t for t, _ in sel], [v for _, v in sel], where="pre", color=col, linewidth=1.6, zorder=3)
        ax.set_ylim(0, 3.2 * a.target)
        ax.set_ylabel("rad/s", color=MUTED)
        ax.text(0.01, 0.95, f"{label}", transform=ax.transAxes, va="top", fontsize=11, fontweight="bold", zorder=5, bbox=dict(facecolor="white", edgecolor="none", pad=1.5))
        ax.text(0.99, 0.95, f"{stalls} stall{'s' if stalls != 1 else ''}",
                transform=ax.transAxes, va="top", ha="right", fontsize=9, color=MUTED, zorder=5, bbox=dict(facecolor="white", edgecolor="none", pad=1.5))
        ax2 = axes[k][1]
        tt = [r["t"] for r in rows if a.start <= r["t"] <= t1]
        iq = [r["iq"] for r in rows if a.start <= r["t"] <= t1]
        ax2.plot(tt, iq, color=col, linewidth=0.8, zorder=3)
        ax2.axhline(0, color=MUTED, linewidth=0.8, zorder=1)
        ax2.set_ylim(-1.0, 2.0)
        ax2.set_ylabel("iq [A]", color=MUTED)
        if iq:
            m = sum(iq) / len(iq)
            sd = math.sqrt(sum((x - m) ** 2 for x in iq) / len(iq))
            ax2.text(0.99, 0.95, f"sd {sd:.2f} A", transform=ax2.transAxes, va="top", ha="right", fontsize=9, color=MUTED)
        for x in (ax, ax2):
            x.grid(axis="y", color=GRID, linewidth=0.8, zorder=0)
            x.set_xlim(a.start, t1)
            for s in ("top", "right"):
                x.spines[s].set_visible(False)
    axes[0][0].set_title(f"Real speed per hall sector ({src})", loc="left", fontsize=11.5,
                         fontweight="bold", pad=18)
    axes[0][1].set_title("Measured iq", loc="left", fontsize=11.5, fontweight="bold", pad=18)
    axes[0][0].text(0, 1.03, "dashed: target; stall = sector slower than 1/3 of the target", transform=axes[0][0].transAxes,
                    fontsize=9, color=MUTED)
    axes[-1][0].set_xlabel("time [s]", color=MUTED)
    axes[-1][1].set_xlabel("time [s]", color=MUTED)
    fig.suptitle(f"Hoverboard motor, velocity target {a.target:g} rad/s, gen 1 board (STM32F103), hall sensors, foc_current",
                 x=0.01, ha="left", fontsize=13, fontweight="bold")
    fig.tight_layout(rect=(0, 0, 1, 0.97))
    fig.savefig(a.out, dpi=150, facecolor="white")
    print(a.out)


if __name__ == "__main__":
    main()
