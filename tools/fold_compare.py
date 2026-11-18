#!/usr/bin/env python3
"""
Compare phase current waveforms recorded live by tools/ol_run.py (--out): each record is folded on the open loop
electrical angle (one averaged period), both curves are aligned on the phase of their fundamental, and the THD,
the zero crossing share (|i| < 10 % of the peak, 6.4 % for a pure sine) and the fundamental are printed.

  python tools/fold_compare.py data/run/v1.8_comp0.csv data/run/v1.8_comp0.605_equal.csv \
      --labels "no compensation" "compensation 0.605 V" --title "GD32 board, 1.8 V, 0.5 rad/s" --out compare.png
"""
import argparse
import csv

import numpy as np
import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402

COLORS = ["#9aa4b1", "#d9480f", "#1c7ed6", "#2b8a3e"]


def load(path, skip):
    rows = list(csv.DictReader(open(path)))
    t = np.array([float(r["t_s"]) for r in rows])
    ia = np.array([float(r["ia"]) for r in rows])
    ang = np.mod(np.array([float(r["electrical_angle"]) for r in rows]), 2 * np.pi)
    keep = t > skip  # skip the start (rotor catching the field, voltage ramp)
    return ia[keep], ang[keep]


def fold(ia, ang, nbins):
    idx = (ang / (2 * np.pi) * nbins).astype(int) % nbins
    w = np.array([ia[idx == b].mean() if np.any(idx == b) else np.nan for b in range(nbins)])
    ok = ~np.isnan(w)
    w = np.where(ok, w, np.interp(np.arange(nbins), np.arange(nbins)[ok], w[ok]))
    w = w - w.mean()
    spec = np.fft.rfft(w)
    h = np.abs(spec) / nbins
    thd = np.sqrt(np.sum(h[2:16] ** 2)) / h[1]
    peak = 2 * h[1]
    flat = np.mean(np.abs(w) < 0.1 * peak)
    return w, thd, flat, peak, np.angle(spec[1])


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("files", nargs="+", help="CSV files from ol_run.py --out")
    p.add_argument("--labels", nargs="+", default=None)
    p.add_argument("--title", default="Phase A current folded on the open loop angle")
    p.add_argument("--skip", type=float, default=1.0, help="seconds skipped at the start of each record")
    p.add_argument("--bins", type=int, default=72, help="angle bins per electrical period")
    p.add_argument("--out", default="fold_compare.png")
    a = p.parse_args()
    labels = a.labels or a.files

    fig, ax = plt.subplots(figsize=(9, 5), dpi=130)
    x = (np.arange(a.bins) + 0.5) * 360 / a.bins
    ref = None
    for k, (f, label) in enumerate(zip(a.files, labels)):
        ia, ang = load(f, a.skip)
        w, thd, flat, peak, phase = fold(ia, ang, a.bins)
        ref = phase if ref is None else ref
        w = np.roll(w, int(round((phase - ref) / (2 * np.pi) * a.bins)))  # same phase of the fundamental
        print(f"{label}: {len(ia)} samples, fundamental peak {peak:.3f} A, THD {100 * thd:.1f} %, "
              f"|i|<10 % {100 * flat:.1f} %")
        ax.plot(x, w, color=COLORS[k % len(COLORS)], lw=2.2, label=f"{label}, THD {100 * thd:.1f} %")
    ax.axhline(0, color="grey", lw=0.5)
    ax.set_xlim(0, 360)
    ax.set_xticks(range(0, 361, 60))
    ax.set_xlabel("open loop electrical angle [deg]")
    ax.set_ylabel("phase A current [A]")
    ax.set_title(a.title, fontsize=10)
    ax.grid(alpha=0.3)
    ax.legend(fontsize=9)
    fig.tight_layout()
    fig.savefig(a.out)
    print(a.out)


if __name__ == "__main__":
    main()
