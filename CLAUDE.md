# FOC test bench - instructions for Claude

Test bench for the PWM dead-time compensation added to SimpleFOC (fork candas-agent/Arduino-FOC,
branch feat/deadtime-compensation) and for the MXLEMMING observer fixes (fork candas-agent/Arduino-FOC-drivers,
branch fix/mxlemming-checks). Hardware: B-G431B-ESC1 or GD32F130 hoverboard split board + hoverboard motor
(very low phase resistance, ~0.17 Ohm), no position sensor used for now: open loop only.

## Safety rules
- Never flash the board or start/enable the motor without the user's explicit confirmation in the current session.
- Never raise ALIGN_VOLTAGE, OPENLOOP_VOLTAGE, FOC_VOLTAGE_LIMIT or CURRENT_LIMIT without asking: I ~ U/R, 1 V is ~6 A.
- Calibration (`DC<amps>` command) keeps a DC current for several seconds: max 2 A unless the user says otherwise.
- The firmware starts with the motor disabled; disable it again (`ME0`) after each test: in open loop the current flows even at target 0.
- If the serial output shows an error or the behaviour is unexpected, disable the motor (`ME0`) before investigating.

- `pio test -e esc1 -f test_motor`, `tools/campaign.py`, `tools/ol_run.py`, `tools/zero_torque.py` and
  `tools/bench_cmd.py` with ME1/DC/SA make the motor move: ask for confirmation before each run,
  and only when the user says they are next to the bench. `test_static` does not move the motor but still flashes the board.
- Keep the safety features (run timeout, overcurrent cut, heartbeat) enabled; never weaken them without asking.

## GD32F130 split board (env `gd32f130`)
- Supplied directly (no ideal diode): energy fed back by the motor goes to the supply, keep low speeds.
  26 V supply. The bus voltage is only measured at start-up (`X` prints it), there is no overvoltage cut.
- Commands go through RTT (tools `--port rtt --env gd32f130`), no serial port, no `pio test`.
- Current sense on the MOSFETs R_ds_on: less accurate than the ESC1 shunts, check the offsets at power-up.
- Dead time fixed by the GD32 driver (60 cycles, 0.83 us); DEAD_ZONE in the board file only serves the expected value.

## Workflow
- Build: `pio run -e esc1` (or `-e gd32f130`), flash: `pio run -e esc1 -t upload`.
- Tests: `pio test -e esc1 -f test_static`, `pio test -e esc1 -f test_motor`.
- Campaign: `python tools/campaign.py --port <port> ...` (results in data/, summarise them for the user).
- Hardware setup, safety and recorder code: lib/testbench (shared by src/main.cpp and the tests).
- pyOCD: `tools/live.py` (read dbg live), `tools/board_mem.py` (ELF symbols, recorder). Only one tool on the ST-Link
  at a time (pyOCD or an upload). The dbg/Sample layouts are mirrored in board_mem.py: update both together and
  bump DBG_MAGIC.
- Open loop voltage mode only works below ~0.5 rad/s with 0.3 V (back-EMF ~0.32 V per rad/s): use MT2 above.
- The MXLEMMING observer runs in parallel (not linked to the motor); `angle_err` ripple is the metric.
- Serial: 115200 baud through the ST-Link virtual COM port. Commands are documented in README.md.
- Record results in docs/test-log.md.
