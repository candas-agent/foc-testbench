#!/usr/bin/env python3
"""
Pin scan helper (simplefoc-work #39): reads the frequency on CH1 of the Rigol DS1104Z over USB and gives the MCU
pin, for the pin scan firmware in toggle mode (tools/pin_scan.py --all). Probe x10 on CH1, ground clip on GND.
  python tools/scope_pin.py            # set the scope up, then one reading (several scales tried)
  python tools/scope_pin.py --watch 30 # readings every second for 30 s
Frequency -> pin: (base + n) * 100 Hz, base A=10, B=30, F=50, C=60.
The Rigol is on WinUSB (Zadig); pyusb gets libusb from libusb_package.
"""
import argparse
import time
import warnings

warnings.filterwarnings("ignore")

BASES = {10: "A", 30: "B", 50: "F", 60: "C"}
SCALES = [1.0, 0.2, 0.05, 0.02]  # V/div, AC coupling


def open_scope():
    import libusb_package
    import usb.backend.libusb1 as lb
    import usb.core as uc
    be = lb.get_backend(find_library=libusb_package.find_library)
    find = uc.find
    uc.find = lambda *a, **k: find(*a, backend=be, **{x: y for x, y in k.items() if x != "backend"})
    import pyvisa
    rm = pyvisa.ResourceManager("@py")
    res = [r for r in rm.list_resources() if r.startswith("USB") and "6833" in r]
    if not res:
        raise SystemExit("Rigol not found on USB")
    s = rm.open_resource(res[0], timeout=5000)
    return s


def setup(s):
    for c in [":CHAN1:DISP ON", ":CHAN1:PROB 10", ":CHAN1:COUP AC", ":CHAN1:OFFS 0", ":TIM:MAIN:SCAL 0.0002",
              ":TRIG:MODE EDGE", ":TRIG:EDG:SOUR CHAN1", ":TRIG:EDG:SLOP POS", ":TRIG:EDG:LEV 0", ":TRIG:SWE AUTO",
              ":RUN"]:
        s.write(c)
    for ch in (2, 3, 4):
        s.write(f":CHAN{ch}:DISP OFF")


def pin_of(f):
    k = round(f / 100)
    if abs(f - 100 * k) > 25:
        return None
    for base, port in BASES.items():
        if base <= k < base + 16:
            return f"P{port}{k - base}"
    return None


def num(s, q):
    """Rigol answers 'measure error!' or 9.9E37 when a measurement is not possible: None."""
    try:
        v = float(s.query(q))
    except ValueError:
        return None
    return v if v < 1e30 else None


def reading(s):
    """Tries the scales from large to small, keeps the first one with a valid frequency and a usable amplitude."""
    best = None
    for sc in SCALES:
        s.write(f":CHAN1:SCAL {sc}")
        time.sleep(0.6)
        f = num(s, ":MEAS:ITEM? FREQ,CHAN1")
        vpp = num(s, ":MEAS:ITEM? VPP,CHAN1")
        if f is not None and vpp is not None and vpp < 7 * sc:  # valid and not clipped (8 divisions on screen)
            best = (f, vpp, sc)
            if vpp > 2 * sc:  # at least 2 divisions: good enough
                break
    return best


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--watch", type=float, default=0)
    a = p.parse_args()
    s = open_scope()
    setup(s)
    end = time.time() + a.watch
    while True:
        r = reading(s)
        if r is None:
            print("no signal (no valid frequency on any scale)", flush=True)
        else:
            f, vpp, sc = r
            print(f"{f:8.1f} Hz  Vpp {vpp * 1000:6.0f} mV  ({sc} V/div)  -> {pin_of(f) or '?'}", flush=True)
        if time.time() >= end:
            break


if __name__ == "__main__":
    main()
