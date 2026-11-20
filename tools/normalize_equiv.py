#!/usr/bin/env python3
"""
_normalizeAngle on a PC: dev (fmodf, then + 2PI if negative) vs simplefoc PR #530 (angle - floorf(angle / 2PI) * 2PI),
float32 with the C semantics. Prints the largest difference, how often each version leaves [0, 2PI), and what the
trapezoid sector index (6 or 12 * a / 2PI, as in setPhaseVoltage) becomes at the edges.

  python -I tools/normalize_equiv.py
"""
import numpy as np

f32 = np.float32
TWO_PI = f32(6.28318530718)
INV = f32(1.0) / TWO_PI


def dev(a):
    r = np.fmod(a, TWO_PI).astype(f32)  # fmodf is exact
    return np.where(r >= 0, r, (r + TWO_PI).astype(f32)).astype(f32)


def pr530(a):
    rot = np.floor((a * INV).astype(f32)).astype(f32)
    return (a - (rot * TWO_PI).astype(f32)).astype(f32)


def check(label, a):
    with np.errstate(all="ignore"):
        d, n = dev(a), pr530(a)
    fin = np.isfinite(a)
    diff = np.abs((d - n).astype(np.float64))
    # a difference of ~2PI is the same angle (one at 0, the other at 2PI)
    wrap = np.abs(diff - float(TWO_PI)) < 1e-3
    real = np.where(wrap, 0, diff)
    print(f"{label}: {a.size} angles, max diff {real[fin].max():.3g} rad, "
          f"dev outside [0,2PI): {int(np.sum(fin & ((d < 0) | (d >= TWO_PI))))}, "
          f"#530 outside [0,2PI): {int(np.sum(fin & ((n < 0) | (n >= TWO_PI))))} "
          f"(min {float(np.min(n[fin])):.3g}, max - 2PI {float(np.max(n[fin]) - TWO_PI):.3g}), "
          f"wrapped 0 <-> 2PI: {int(np.sum(wrap & fin))}")
    for name, v in (("dev", d), ("#530", n)):
        s6 = np.floor((f32(6) * (v / TWO_PI).astype(f32)).astype(f32))
        print(f"   {name}: trapezoid 120 sector outside 0..5: {int(np.sum(fin & ((s6 < 0) | (s6 > 5))))}")


def main():
    rng = np.random.default_rng(3)
    check("random -1000..1000 rad", rng.uniform(-1000, 1000, 5_000_000).astype(f32))
    k = np.arange(-2000, 2000, dtype=f32)
    edges = np.concatenate([(k * TWO_PI).astype(f32)] +
                           [np.nextafter((k * TWO_PI).astype(f32), f32(s * np.inf)) for s in (1, -1)])
    check("multiples of 2PI and their neighbours", edges.astype(f32))
    check("electrical angles 0..15 x 2PI (hoverboard: 15 pole pairs)", rng.uniform(0, 15 * 6.2832, 5_000_000).astype(f32))


if __name__ == "__main__":
    main()
