#!/usr/bin/env python3
"""
GD32F130 split board: reads the TIMER0 and GPIOA/GPIOB configuration registers set by the GD32 driver
(pyOCD, attach, no halt), to compare two firmwares (envs gd32drv_dev and gd32drv_pr).
Does not move the motor: read only. Run it after the firmware has started (driver.init done), motor disabled.

  python tools/gd32_drv_regs.py --save data/gd32drv_dev_regs.json
  python tools/gd32_drv_regs.py --save data/gd32drv_pr_regs.json
  python tools/gd32_drv_regs.py --compare data/gd32drv_dev_regs.json data/gd32drv_pr_regs.json
"""
import argparse
import json
import sys

TIMER0 = 0x40012C00  # GD32F1x0: APB1_BUS_BASE + 0x12C00
GPIOA = 0x48000000
GPIOB = 0x48000400

TIMER_REGS = {"CTL0": 0x00, "CTL1": 0x04, "SMCFG": 0x08, "DMAINTEN": 0x0C, "CHCTL0": 0x18, "CHCTL1": 0x1C,
              "CHCTL2": 0x20, "PSC": 0x28, "CAR": 0x2C, "CREP": 0x30, "CH0CV": 0x34, "CH1CV": 0x38,
              "CH2CV": 0x3C, "CH3CV": 0x40, "CCHP": 0x44}
GPIO_REGS = {"CTL": 0x00, "OMODE": 0x04, "OSPD": 0x08, "PUD": 0x0C, "AFSEL0": 0x20, "AFSEL1": 0x24}
# CH0CV..CH2CV follow the duty cycles: only their presence is compared, not their value
VARIABLE = {"TIMER0.CH0CV", "TIMER0.CH1CV", "TIMER0.CH2CV"}


def dead_time_cycles(cchp):
    r = cchp & 0xFF
    if r & 0x80 == 0:
        return r
    if r & 0xC0 == 0x80:
        return (64 + (r & 0x3F)) * 2
    if r & 0xE0 == 0xC0:
        return (32 + (r & 0x1F)) * 8
    return (32 + (r & 0x1F)) * 16


def read_regs():
    from pyocd.core.helpers import ConnectHelper
    regs = {}
    with ConnectHelper.session_with_chosen_probe(connect_mode="attach", target_override="cortex_m") as s:
        t = s.board.target
        for name, off in TIMER_REGS.items():
            regs[f"TIMER0.{name}"] = t.read32(TIMER0 + off)
        for port, base in (("GPIOA", GPIOA), ("GPIOB", GPIOB)):
            for name, off in GPIO_REGS.items():
                regs[f"{port}.{name}"] = t.read32(base + off)
    return regs


def show(regs):
    for k, v in regs.items():
        print(f"{k:16s} 0x{v:08X}")
    print(f"dead time: {dead_time_cycles(regs['TIMER0.CCHP'])} cycles")


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--save", help="JSON file for the registers read")
    p.add_argument("--compare", nargs=2, metavar=("A", "B"), help="compare two saved JSON files")
    a = p.parse_args()
    if a.compare:
        ra, rb = (json.load(open(f)) for f in a.compare)
        diff = [k for k in ra if k not in VARIABLE and ra[k] != rb.get(k)]
        for k in diff:
            print(f"{k:16s} 0x{ra[k]:08X} -> 0x{rb[k]:08X}")
        print("identical (duty cycle registers excluded)" if not diff else f"{len(diff)} register(s) differ")
        sys.exit(1 if diff else 0)
    regs = read_regs()
    show(regs)
    if a.save:
        json.dump(regs, open(a.save, "w"), indent=1)


if __name__ == "__main__":
    main()
