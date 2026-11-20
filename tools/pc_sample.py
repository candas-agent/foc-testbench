#!/usr/bin/env python3
"""
Statistical profiler: samples the program counter of the running firmware (DWT_PCSR) through the probe and
prints the share of samples per function. Does not stop the CPU and sends no command (motor state unchanged).

  python tools/pc_sample.py --env gd32f130 --samples 20000
"""
import argparse
import bisect
import collections
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from board_mem import BoardMemory  # noqa: E402
from elftools.elf.elffile import ELFFile  # noqa: E402

DWT_PCSR = 0xE000101C
DEMCR = 0xE000EDFC


def functions(elf_path):
    out = []
    with open(elf_path, "rb") as f:
        elf = ELFFile(f)
        for sym in elf.get_section_by_name(".symtab").iter_symbols():
            if sym["st_info"]["type"] == "STT_FUNC" and sym["st_size"] > 0:
                out.append((sym["st_value"] & ~1, sym["st_size"], sym.name))
    out.sort()
    return out


def demangle(names):
    try:
        import subprocess
        tool = os.path.expanduser("~/.platformio/packages/toolchain-gccarmnoneeabi/bin/arm-none-eabi-c++filt")
        res = subprocess.run([tool], input="\n".join(names), capture_output=True, text=True)
        return dict(zip(names, res.stdout.splitlines()))
    except Exception:
        return {n: n for n in names}


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--env", default="gd32f130")
    p.add_argument("--elf", default=None)
    p.add_argument("--samples", type=int, default=20000)
    p.add_argument("--top", type=int, default=30)
    a = p.parse_args()
    elf = a.elf or os.path.join(".pio", "build", a.env, "firmware.elf")
    funcs = functions(elf)
    starts = [f[0] for f in funcs]
    counts = collections.Counter()
    slow = 0
    with BoardMemory(elf) as mem:
        t = mem.target
        t.write32(DEMCR, t.read32(DEMCR) | (1 << 24))
        n = 0
        while n < a.samples:
            pc = t.read32(DWT_PCSR)
            if pc == 0xFFFFFFFF:  # core halted/sleeping
                continue
            n += 1
            k = bisect.bisect_right(starts, pc) - 1
            if k >= 0 and pc < funcs[k][0] + funcs[k][1]:
                counts[funcs[k][2]] += 1
            else:
                counts[f"?{pc:08x}"] += 1
            if 0x08008000 <= pc < 0x08010000:
                slow += 1
    names = demangle([n for n, _ in counts.most_common(a.top)])
    print(f"{n} samples, {100.0 * slow / n:.1f} % in the slow flash (>= 0x08008000)")
    for name, c in counts.most_common(a.top):
        print(f"{100.0 * c / n:6.2f} %  {names.get(name, name)}")


if __name__ == "__main__":
    main()
