#!/usr/bin/env python3
"""
Estimate the current sense offset error WHILE RUNNING, from the 1x electrical ripple of ud. THE MOTOR MOVES.

The current loop keeps the measured id at 0. With a stator frame offset error (Ia_off, Ib_off) on the measured
currents, the real d current is -(I_alpha_off cos(th) + I_beta_off sin(th)): it rotates with the electrical angle
and the d voltage gets ud ~ R * id_real (omega*L and the back-EMF have no d component at these speeds). Fitting
ud(th) = c + A cos(th) + B sin(th) gives I_alpha_off = -A/R, I_beta_off = -B/R, then ia_off = I_alpha_off,
ib_off = (-I_alpha_off + sqrt(3) I_beta_off) / 2. The d axis avoids the friction / speed loop that drives iq.
Both directions are fitted separately (consistency check). Velocity mode, settings as lowspeed_campaign.py.

  python tools/offset_ripple.py --env combo_f103 --speed 3 --duration 12 [--dt 0.38] [--pi 0.2/0.3/0.01]
"""
import argparse
import math
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from board_mem import BoardMemory, RttPort  # noqa: E402


def fit(rows):
    # least squares on [1, cos, sin]
    import numpy as np
    th = np.array([r["electrical_angle"] for r in rows])
    ud = np.array([r["ud"] for r in rows])
    m = np.column_stack([np.ones_like(th), np.cos(th), np.sin(th)])
    coef, res, *_ = np.linalg.lstsq(m, ud, rcond=None)
    resid = ud - m @ coef
    s2 = resid.var() * len(ud) / max(1, len(ud) - 3)
    cov = s2 * np.linalg.inv(m.T @ m)
    return coef, np.sqrt(np.diag(cov)), resid.std()


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--env", default="combo_f103")
    p.add_argument("--speed", type=float, default=3.0)
    p.add_argument("--duration", type=float, default=12.0)
    p.add_argument("--settle", type=float, default=2.0)
    p.add_argument("--R", type=float, default=0.57, help="phase resistance [Ohm] (dead-time calibration)")
    p.add_argument("--dt", default="0.38", help="dead-time compensation [V]")
    p.add_argument("--pi", default="0.2/0.3/0.01", help="velocity PI P/I/Tf")
    a = p.parse_args()
    kp, ki, tf = a.pi.split("/")
    with BoardMemory(os.path.join(".pio", "build", a.env, "firmware.elf")) as mem:
        port = RttPort(mem, timeout=0.1)

        def cmd(c, t=0.3):
            port.write((c + "\n").encode())
            end, out = time.time() + t, ""
            while time.time() < end:
                out += port.read(4096).decode(errors="replace")
                time.sleep(0.02)
            return out.strip().replace("\n", " | ")

        time.sleep(0.2)
        port.reset_input_buffer()
        for c in ["ME0", "VO", "XF0", "DB0.3", "MLC2", "MT2", "MC1", "M0", "ST0", "DM0", f"D{a.dt}", "DT0.2",
                  f"MVP{kp}", f"MVI{ki}", f"MVF{tf}"]:
            cmd(c)
        results = []
        try:
            for sgn in (1, -1):
                cmd(f"M{sgn * a.speed}")
                cmd("ME1", 0.1)
                t0, rows = time.time(), []
                while time.time() - t0 < a.duration:
                    d = mem.read_dbg()
                    if abs(d["velocity"]) > 20 or d["fault"] or max(abs(d["ia"]), abs(d["ib"])) > 3.5:
                        raise RuntimeError(f"abort: {d}")
                    if time.time() - t0 > a.settle:
                        rows.append(d)
                cmd("ME0", 0.4)
                (c0, ca, cb), (sc, sa, sb), sd = fit(rows)
                i_al, i_be = -ca / a.R, -cb / a.R
                ia, ib = i_al, (-i_al + math.sqrt(3) * i_be) / 2
                mean_v = sum(r["velocity"] for r in rows) / len(rows)
                mean_iq = sum(r["iq"] for r in rows) / len(rows)
                print(f"{sgn * a.speed:+.1f} rad/s: {len(rows)} samples, vel {mean_v:+.2f}, iq {mean_iq:+.3f} A | ud = "
                      f"{c0:+.4f} {ca:+.4f} cos {cb:+.4f} sin (+-{sa:.4f}, resid sd {sd:.3f} V) -> "
                      f"offset error ia {ia:+.4f} ib {ib:+.4f} ic {-ia - ib:+.4f} A", flush=True)
                results.append((ia, ib))
                time.sleep(0.5)
        finally:
            cmd("ME0", 0.4)
            for c in ["M0", "D0", "MVP0.4", "MVI1", "MVF0.01"]:
                cmd(c)
            print("X ->", cmd("X", 0.8))
        if len(results) == 2:
            ia = sum(r[0] for r in results) / 2
            ib = sum(r[1] for r in results) / 2
            print(f"mean of both directions: ia {ia:+.4f} ib {ib:+.4f} A  (correction: XO{-ia:.4f},{-ib:.4f})")


if __name__ == "__main__":
    main()
