#!/usr/bin/env python3
"""
Figure of the 2 rad/s A/B on the gen 1 hoverboard board (STM32F103, hall sensors, foc_current, R_ds_on current sense):
SimpleFOC dev vs dead-time compensation (candas-agent/Arduino-FOC#2) vs hall velocity decay (#3) vs both.

Sources (tools/lowspeed_campaign.py, 3 passes x (+2, -2 rad/s), 8 s measured per run):
  dt_f103_ab.csv     firmware dt_f103 (dev + compensation): dt 0 -> "dev", dt 0.38 -> "compensation"
  combo_f103_ab.csv  firmware combo_f103 (dev + compensation + hall decay): dt 0 -> "hall velocity", dt 0.38 -> "both"
All metrics come from the hall edges seen by the PC, not from the firmware velocity estimate.

  python tools/plot_ab.py [--out docs/figures/f103_ab_2rads.png]
"""
import argparse
import csv
import math
import os

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402

DATA = os.path.join("data", "2026-11-20")
CONDS = [  # label, file, config
    ("SimpleFOC dev", "dt_f103_ab.csv", "C0 dt 0"),
    ("+ dead-time\ncompensation", "dt_f103_ab.csv", "C0 dt 0.38"),
    ("+ hall velocity\ndecay", "combo_f103_ab.csv", "C0 dt 0"),
    ("both", "combo_f103_ab.csv", "C0 dt 0.38"),
]
COLORS = ["#8a8984", "#eb6834", "#2a78d6", "#1baf7a"]
INK, MUTED, GRID = "#1f1f1e", "#6b6a64", "#e4e3dd"


def load(fname, cfg, target_abs=2.0):
    with open(os.path.join(DATA, fname)) as f:
        return [r for r in csv.DictReader(f) if r["config"] == cfg and abs(float(r["target"])) == target_abs]


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--out", default=os.path.join("docs", "figures", "f103_ab_2rads.png"))
    a = p.parse_args()
    rows = [load(f, c) for _, f, c in CONDS]
    metrics = [
        ("Stalls per run", "hall sector > 3x nominal time, 8 s at 2 rad/s", lambda r: float(r["stalls"]), "{:.1f}"),
        ("Real speed", "from the hall edges, % of the 2 rad/s target",
         lambda r: 100 * abs(float(r["true_v"])) / abs(float(r["target"])), "{:.0f} %"),
        ("Speed irregularity", "coefficient of variation of the hall intervals",
         lambda r: 100 * float(r["cv"]), "{:.0f} %"),
        ("iq ripple", "standard deviation of the measured iq [A]", lambda r: float(r["iq_sd"]), "{:.2f} A"),
    ]
    plt.rcParams.update({"font.size": 10, "axes.edgecolor": MUTED, "axes.labelcolor": INK,
                         "xtick.color": MUTED, "ytick.color": MUTED, "text.color": INK})
    fig, axes = plt.subplots(2, 2, figsize=(11, 7.6))
    for ax, (title, sub, fn, fmt) in zip(axes.flat, metrics):
        for k, rr in enumerate(rows):
            vals = [fn(r) for r in rr]
            m = sum(vals) / len(vals)
            ax.bar(k, m, width=0.62, color=COLORS[k], zorder=2)
            # individual runs (both directions, 3 passes)
            n = len(vals)
            xs = [k - 0.18 + 0.36 * i / max(1, n - 1) for i in range(n)]
            ax.scatter(xs, vals, s=22, color="white", edgecolor=INK, linewidth=0.8, zorder=3)
            ax.annotate(fmt.format(m), (k, max(vals + [m])), xytext=(0, 7), textcoords="offset points",
                        ha="center", va="bottom", fontsize=9.5, fontweight="bold", color=INK)
        if title == "Real speed":
            ax.axhline(100, color=INK, linewidth=1, linestyle=(0, (4, 3)), zorder=1)
            ax.set_ylim(85, 103)
        else:
            ax.set_ylim(0, ax.get_ylim()[1] * 1.12)
        ax.set_title(title, loc="left", fontsize=12, fontweight="bold", color=INK, pad=18)
        ax.text(0, 1.02, sub, transform=ax.transAxes, fontsize=9, color=MUTED)
        ax.set_xticks(range(len(CONDS)), [c[0] for c in CONDS], fontsize=9)
        ax.grid(axis="y", color=GRID, linewidth=0.8, zorder=0)
        for s in ("top", "right"):
            ax.spines[s].set_visible(False)
        ax.tick_params(axis="x", length=0)
    fig.suptitle("Hoverboard motor at 2 rad/s, gen 1 board (STM32F103), hall sensors, foc_current",
                 x=0.02, ha="left", fontsize=13.5, fontweight="bold")
    fig.text(0.02, 0.935, "Bars: mean of 6 runs (+2 and -2 rad/s, 3 passes, 8 s each); dots: individual runs. "
             "Velocity PI P 0.4 I 1 Tf 0.01, compensation 0.38 V. At 5 rad/s all four are fine (0-1 stall).",
             fontsize=9, color=MUTED)
    fig.tight_layout(rect=(0, 0, 1, 0.92), h_pad=2.5)
    os.makedirs(os.path.dirname(a.out), exist_ok=True)
    fig.savefig(a.out, dpi=150, facecolor="white")
    print(a.out)
    for (name, _, _), rr in zip(CONDS, rows):
        print(name.replace("\n", " "), len(rr), "runs")


if __name__ == "__main__":
    main()
