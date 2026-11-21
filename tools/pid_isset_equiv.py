#!/usr/bin/env python3
"""
Exhaustive check on a PC (all 2^32 float32 bit patterns) that, in PIDController::operator(),
  _isset(output_ramp) && output_ramp > 0   ==   output_ramp > 0
with the C semantics (_isset = != -12345.0f, NaN compares false). The limit test computed once in a bool is the
same value both times (limit is not changed in between).

  python -I tools/pid_isset_equiv.py
"""
import numpy as np

NOT_SET = np.float32(-12345.0)
diff = 0
chunk = 1 << 26
for start in range(0, 1 << 32, chunk):
    r = np.arange(start, start + chunk, dtype=np.uint64).astype(np.uint32).view(np.float32)
    with np.errstate(all="ignore"):
        old = (r != NOT_SET) & (r > 0)
        new = r > 0
    diff += int(np.sum(old != new))
print(f"{1 << 32} float bit patterns, differences: {diff}")
