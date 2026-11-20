#!/usr/bin/env python3
"""
Equivalence test of PIDController::operator() on a PC: dev vs perf/pid-skip-zero-derivative (derivative skipped
when D == 0). float32 with the C semantics (_constrain ternary, _isset = != -12345.0f, NaN compares false).
Compares the output and the stored state (integral_prev, output_prev, error_prev) bit for bit (NaN == NaN).

  python -I tools/pid_equiv.py
"""
import itertools
import numpy as np

f32 = np.float32
NOT_SET = f32(-12345.0)


def constrain(x, lo, hi):
    return np.where(x < lo, lo, np.where(x > hi, hi, x)).astype(f32)


def pid(skip, P, I, D, e, ep, ip, op, limit, ramp, dt):
    with np.errstate(all="ignore"):
        prop = (P * e).astype(f32)
        integ = (ip + (((I * dt).astype(f32) * f32(0.5)).astype(f32) * (e + ep).astype(f32)).astype(f32)).astype(f32)
        has_lim = limit != NOT_SET
        integ = np.where(has_lim, constrain(integ, -limit, limit), integ).astype(f32)
        der = (((D * (e - ep).astype(f32)).astype(f32)) / dt).astype(f32)
        if skip:
            der = np.where(D != 0, der, f32(0)).astype(f32)
        out = ((prop + integ).astype(f32) + der).astype(f32)
        out = np.where(has_lim, constrain(out, -limit, limit), out).astype(f32)
        use_ramp = (ramp != NOT_SET) & (ramp > 0)
        rate = ((out - op).astype(f32) / dt).astype(f32)
        up = (op + (ramp * dt).astype(f32)).astype(f32)
        dn = (op - (ramp * dt).astype(f32)).astype(f32)
        ramped = np.where(rate > ramp, up, np.where(rate < -ramp, dn, out)).astype(f32)
        out = np.where(use_ramp, ramped, out).astype(f32)
    return out, integ


def same(a, b):
    return (a.view(np.uint32) == b.view(np.uint32)) | (np.isnan(a) & np.isnan(b))


def report(label, args):
    o1, i1 = pid(False, *args)
    o2, i2 = pid(True, *args)
    ok_o, ok_i = same(o1, o2), same(i1, i2)
    D = args[2]
    print(f"{label}: {o1.size} cases, output differs {int(np.sum(~ok_o))} (D != 0: {int(np.sum(~ok_o & (D != 0)))}), "
          f"integral differs {int(np.sum(~ok_i))}")
    bad = np.nonzero(~ok_o)[0]
    kinds = {}
    for i in bad:
        k = (f"D={float(D[i]):g}", f"e={float(args[3][i]):g}", f"ep={float(args[4][i]):g}",
             f"old={float(o1[i]):g}", f"new={float(o2[i]):g}")
        kinds.setdefault(k[3:], k)
    for k, v in list(kinds.items())[:12]:
        print("   ", " ".join(v))
    return bad


def main():
    rng = np.random.default_rng(2)
    n = 2_000_000
    P = rng.uniform(0, 5, n).astype(f32)
    I = rng.uniform(0, 100, n).astype(f32)
    D = np.where(rng.random(n) < 0.5, f32(0), rng.uniform(-0.1, 0.1, n)).astype(f32)
    D[:1000] = f32(-0.0)
    e, ep = rng.uniform(-30, 30, n).astype(f32), rng.uniform(-30, 30, n).astype(f32)
    ip, op = rng.uniform(-30, 30, n).astype(f32), rng.uniform(-30, 30, n).astype(f32)
    limit = np.where(rng.random(n) < 0.3, NOT_SET, rng.uniform(0.1, 30, n)).astype(f32)
    ramp = np.where(rng.random(n) < 0.5, NOT_SET, rng.uniform(0, 1000, n)).astype(f32)
    dt = rng.uniform(1e-5, 0.5, n).astype(f32)
    report("random", (P, I, D, e, ep, ip, op, limit, ramp, dt))

    sp = [f32(v) for v in [0, -0.0, 1e-6, 1, -1, 1e6, -1e6, np.inf, -np.inf, np.nan]]
    Dv = [f32(0), f32(-0.0), f32(0.01)]
    combos = np.array(list(itertools.product([f32(1)], [f32(10)], Dv, sp, sp, sp, [f32(0)],
                                             [NOT_SET, f32(5)], [NOT_SET, f32(100)], [f32(1e-3), f32(1e-6)])), dtype=f32)
    cols = [combos[:, k] for k in range(10)]
    report("special values", cols)


if __name__ == "__main__":
    main()
