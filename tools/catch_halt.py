"""Recovery when a firmware disables SWD: attach to the target as soon as it answers (replug the board 3.3 V,
keep the ST-Link on USB), halt it and leave it halted. Then: openocd ... -c init -c halt -c "stm32f1x mass_erase 0".
Usage: python tools/catch_halt.py [seconds]"""
import sys
import time
import logging
from pyocd.core.helpers import ConnectHelper

logging.disable(logging.CRITICAL)
end = time.time() + float(sys.argv[1]) if len(sys.argv) > 1 else time.time() + 120
n = 0
while time.time() < end:
    n += 1
    try:
        with ConnectHelper.session_with_chosen_probe(
                connect_mode="attach", target_override="cortex_m", frequency=1000000,
                options={"resume_on_disconnect": False}) as s:
            t = s.board.target
            t.halt()
            pc = t.read_core_register("pc")
            ctl = t.read32(0x48000000)
            print(f"connected at try {n}: halted, pc=0x{pc:08X}, GPIOA.CTL=0x{ctl:08X}", flush=True)
            sys.exit(0)
    except SystemExit:
        raise
    except Exception:
        pass
print(f"no connection after {n} tries", flush=True)
