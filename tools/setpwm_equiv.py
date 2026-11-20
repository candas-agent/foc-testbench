#!/usr/bin/env python3
"""
Equivalence test of BLDCDriver3PWM/6PWM::setPwm, dev vs perf/setpwm-single-limit, on a PC (no board).

Both versions are reproduced in float32 with the C semantics: the _constrain ternary (comparisons with NaN are
false), division / multiplication rounding (IEEE round to nearest, same as libgcc soft float and FPUs), then the
STM32 duty conversion `(uint32_t)(dc * (ARR+1))` (truncation; negative or NaN -> 0 as __fixunssfsi and VCVT.U32).
Prints how many duty cycles differ, by how many timer counts, and every case that differs by more than one count.

  python -I tools/setpwm_equiv.py
"""
import itertools
import numpy as np

f32 = np.float32


def constrain(x, lo, hi):
    # ((amt)<(low)?(low):((amt)>(high)?(high):(amt)))
    return np.where(x < lo, lo, np.where(x > hi, hi, x)).astype(f32)


def dev(u, vl, vs):
    u = constrain(u, f32(0), vl)
    with np.errstate(divide="ignore", invalid="ignore"):
        return constrain((u / vs).astype(f32), f32(0), f32(1))


def new(u, vl, vs):
    u_max = np.where(vl < vs, vl, vs).astype(f32)
    ok = u_max > 0
    with np.errstate(divide="ignore", invalid="ignore", over="ignore"):
        inv = (f32(1) / vs).astype(f32)
        dc = (constrain(u, f32(0), u_max) * inv).astype(f32)
    return np.where(ok, dc, f32(0)).astype(f32)


def counts(dc, top):
    with np.errstate(invalid="ignore", over="ignore"):
        x = (dc * f32(top)).astype(f32)
    bad = np.isnan(x) | (x < 0)
    x = np.where(bad, 0, np.minimum(x, 2**32 - 1))
    return np.trunc(x).astype(np.int64)


def compare(u, vl, vs, tops, label):
    a, b = dev(u, vl, vs), new(u, vl, vs)
    print(f"{label}: {u.size} cases, dc above 1: dev {int(np.sum(a > 1))} new {int(np.sum(b > 1))}, "
          f"dc NaN: dev {int(np.sum(np.isnan(a)))} new {int(np.sum(np.isnan(b)))}")
    worst = []
    for top in tops:
        ca, cb = counts(a, top), counts(b, top)
        d = cb - ca
        n1 = int(np.sum(np.abs(d) == 1))
        big = np.nonzero(np.abs(d) > 1)[0]
        print(f"  ARR+1 = {top:6d}: same {int(np.sum(d == 0))}, 1 count {n1}, more {big.size}")
        worst += [(top, int(i)) for i in big]
    return a, b, worst


def main():
    rng = np.random.default_rng(1)
    tops = [256, 2000, 2250, 4096, 65536]

    # 1) normal operation: random voltages, limit and supply in usual ranges (incl. limit above the supply)
    n = 2_000_000
    vs = rng.uniform(1, 60, n).astype(f32)
    vl = (vs * rng.uniform(0.1, 1.5, n)).astype(f32)
    u = rng.uniform(-5, 70, n).astype(f32)
    u[: n // 4] = vl[: n // 4]           # exactly at the limit
    u[n // 4: n // 2] = vs[n // 4: n // 2]  # exactly at the supply
    compare(u, vl, vs, tops, "random")

    # 2) every combination of special values
    sp = [f32(v) for v in [-12345, -1, -0.0, 0, 1e-6, 0.5, 6, 12, 24, 26, 26.000002, 1e6, np.inf, -np.inf, np.nan]]
    us = sp + [np.nextafter(f32(12), f32(0)), np.nextafter(f32(12), f32(99)), np.nextafter(f32(26), f32(0))]
    combos = np.array(list(itertools.product(us, sp, sp)), dtype=f32)
    a, b, worst = compare(combos[:, 0], combos[:, 1], combos[:, 2], tops, "special values")
    # classify the cases that differ by more than one count
    idx = sorted({i for _, i in worst})
    vl, vs = combos[idx, 1], combos[idx, 2]
    valid = (vs > 0) & np.isfinite(vs) & ~(vl <= 0)
    print(f"  cases differing by more than 1 count: {len(idx)}, with a valid configuration "
          f"(voltage_power_supply > 0, voltage_limit > 0 or NaN): {int(np.sum(valid))}")
    for name, m in [("voltage_power_supply = 0", vs == 0), ("voltage_power_supply < 0", vs < 0),
                    ("voltage_power_supply NaN", np.isnan(vs)), ("voltage_power_supply > 0", vs > 0)]:
        sel = [idx[k] for k in np.nonzero(m)[0]]
        if sel:
            pairs = sorted({(f"{a[i]:g}", f"{b[i]:g}") for i in sel})
            print(f"    {name}: {len(sel)} cases, dc dev -> new: {', '.join(f'{x} -> {y}' for x, y in pairs[:6])}")

if __name__ == "__main__":
    main()
