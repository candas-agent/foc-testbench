#!/usr/bin/env python3
"""
Compare phase current waveforms recorded with MCUViewer (CSV export), e.g. with and
without dead-time compensation.

For each file:
  - resamples the signal on a uniform time grid (MCUViewer samples are not evenly spaced)
  - finds the electrical frequency (FFT peak)
  - folds all periods on top of each other and averages them -> one clean period
  - computes the THD (harmonics 2..15) and the "zero crossing flatness":
    share of the period where |i| < 10% of the peak. A pure sine gives ~6.4%,
    the dead-time plateau around zero makes it larger.

Output: a PNG with the averaged waveforms and a table printed in the terminal.

Usage:
  python compare_currents.py off.csv on.csv --labels "comp off" "comp on" \
      --time-col time --cur-col dbg.ia --out comparison.png

Column names: check the header of your CSV export, columns can also be given by index (0, 1, ...).
"""
import argparse
import csv
import sys

import numpy as np
import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt


def read_csv(path, time_col, cur_col):
    with open(path, newline="") as f:
        sample = f.read(4096)
        f.seek(0)
        try:
            dialect = csv.Sniffer().sniff(sample, delimiters=",;\t")
        except csv.Error:
            dialect = csv.excel
        rows = list(csv.reader(f, dialect))
    header = rows[0]
    has_header = any(not _is_number(h) for h in header)
    data_rows = rows[1:] if has_header else rows

    def col_index(col):
        if col.isdigit():
            return int(col)
        if not has_header or col not in header:
            sys.exit(f"{path}: column '{col}' not found, available: {header}")
        return header.index(col)

    ti, ci = col_index(time_col), col_index(cur_col)
    t, i = [], []
    for r in data_rows:
        try:
            t.append(float(r[ti].replace(",", ".")) if dialect.delimiter != "," else float(r[ti]))
            i.append(float(r[ci].replace(",", ".")) if dialect.delimiter != "," else float(r[ci]))
        except (ValueError, IndexError):
            continue
    t, i = np.array(t), np.array(i)
    order = np.argsort(t)
    return t[order], i[order]


def _is_number(s):
    try:
        float(s.replace(",", "."))
        return True
    except ValueError:
        return False


def analyse(t, i, n_bins=200):
    # uniform resampling
    dt = np.median(np.diff(t))
    tu = np.arange(t[0], t[-1], dt)
    iu = np.interp(tu, t, i)
    iu = iu - np.mean(iu)

    # electrical frequency: FFT peak (ignoring DC)
    spec = np.abs(np.fft.rfft(iu * np.hanning(len(iu))))
    freqs = np.fft.rfftfreq(len(iu), dt)
    k = np.argmax(spec[1:]) + 1
    f0 = freqs[k]
    period = 1.0 / f0

    # fold the periods, phase reference = rising zero crossings of the fundamental
    phase = 2 * np.pi * f0 * tu
    fund = np.sum(iu * np.exp(-1j * phase))
    phi = np.angle(fund)  # iu ~ A*cos(phase + phi)
    folded = np.mod(phase + phi + np.pi / 2, 2 * np.pi)  # 0 at the rising zero crossing
    bins = np.linspace(0, 2 * np.pi, n_bins + 1)
    idx = np.digitize(folded, bins) - 1
    wave = np.array([iu[idx == b].mean() if np.any(idx == b) else np.nan for b in range(n_bins)])
    centers = (bins[:-1] + bins[1:]) / 2

    # THD from the averaged period
    w = np.nan_to_num(wave)
    harm = np.abs(np.fft.rfft(w)) / len(w)
    h1 = harm[1]
    thd = np.sqrt(np.sum(harm[2:16] ** 2)) / h1 if h1 > 0 else np.nan

    peak = np.nanmax(np.abs(wave))
    flat = np.mean(np.abs(wave) < 0.1 * peak)
    return dict(f0=f0, period=period, centers=centers, wave=wave, thd=thd, flat=flat, peak=peak)


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("files", nargs="+")
    p.add_argument("--labels", nargs="+")
    p.add_argument("--time-col", default="0", help="time column name or index (default 0)")
    p.add_argument("--cur-col", default="1", help="current column name or index (default 1)")
    p.add_argument("--time-scale", type=float, default=1.0, help="factor to convert the time column to seconds (e.g. 1e-3 for ms)")
    p.add_argument("--out", default="comparison.png")
    a = p.parse_args()

    labels = a.labels or a.files
    if len(labels) != len(a.files):
        sys.exit("--labels must have one label per file")

    fig, ax = plt.subplots(figsize=(9, 5))
    print(f"{'file':30s} {'f_el [Hz]':>10s} {'peak [A]':>9s} {'THD [%]':>8s} {'|i|<10% [%]':>12s}")
    for path, label in zip(a.files, labels):
        t, i = read_csv(path, a.time_col, a.cur_col)
        r = analyse(t * a.time_scale, i)
        print(f"{label[:30]:30s} {r['f0']:10.2f} {r['peak']:9.3f} {100 * r['thd']:8.2f} {100 * r['flat']:12.1f}")
        ax.plot(np.degrees(r["centers"]), r["wave"], label=f"{label} (THD {100 * r['thd']:.1f}%)")

    ref = analyse(t * a.time_scale, i)  # last file for the ideal sine amplitude
    ax.plot(np.degrees(ref["centers"]), ref["peak"] * np.sin(ref["centers"]), "k--", lw=0.8, label="ideal sine")
    ax.axhline(0, color="grey", lw=0.5)
    ax.set_xlabel("electrical angle from rising zero crossing [deg]")
    ax.set_ylabel("phase current [A]")
    ax.set_title("Phase current, one electrical period (averaged)")
    ax.legend()
    ax.grid(alpha=0.3)
    fig.tight_layout()
    fig.savefig(a.out, dpi=120)
    print(f"\nplot saved to {a.out}  (pure sine: THD 0%, |i|<10% = 6.4%)")


if __name__ == "__main__":
    main()
