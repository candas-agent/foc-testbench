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
- Only the first 32 KB of flash run without wait states: ld/gd32f130_fastloop.ld places the loop code and the
  virtual tables there (link fails if `__fast_end` > 0x08008000). Code called every loop goes in that list.
  The linker script is not tracked by the build: delete .pio/build/<env>/firmware.elf after changing it.
- No `lib_archive` in platformio.ini: it would override the library.json of SimpleFOC and testbench
  (`"libArchive": false`, they replace weak functions). The other libraries are archived (unused objects dropped).

## Gen 1 hoverboard mainboard (STM32F103RC / GD32F103RC, envs `smooth_f103_*`, `hallvel_f103`, `f103_adc_it*`)
- Plain SimpleFOC (`BENCH_NO_DEADTIME`), right motor, RTT. Power button bridged on the bench: the board is on as soon
  as the supply is on; disconnect the ST-Link 3.3 V when the board is supplied.
- `ALIGN_VOLTAGE` 0.8 V gives ~1.5 A (1.5 V would give ~3.7 A): never raise it without asking.
- Without `SIMPLEFOC_STM32_ADC_INTERRUPT` (frozen currents with SimpleFOC dev on F1, see #594).
- Commands that move the motor: ME1, SA, V<rad/s>, and the tools hall_calib.py, lowspeed_campaign.py, perf_run.py,
  reenable_test.py, trace_run.py.
- With R_ds_on current sense the phase currents read while the motor is disabled are meaningless (tens of A):
  only check them when enabled.
- The STM32F103 runs at 64 MHz (no crystal, HSI), not 72.

## Workflow
- Build in the cloud (preferred, several envs in parallel, 1-2 min): push first, then
  `gh workflow run build.yml --repo candas-agent/foc-testbench -f envs='["perf_dev_f103","<env>"]'`,
  `gh run watch <id> --exit-status`, `gh run download <id> -D .pio/cloud/<id>`; flash with
  `.venv/Scripts/pyocd.exe flash -t stm32f103rc .pio/cloud/<id>/<env>/firmware.elf` and give the tools `--elf`.
  Only envs whose libraries are pushed (no `file://` or lib/qfplib). Same machine code as a local build.
- Any change of platformio.ini wipes every local .pio/build: another reason to build in the cloud.
- Before a PR that claims "same behaviour": written edge-case check (NaN, 0, negative, NOT_SET, rounding, first
  call, after a pause, user-set parameters), PC test when possible (e.g. tools/setpwm_equiv.py).
- Local build: `pio run -e esc1` (or `-e gd32f130`), flash: `pio run -e esc1 -t upload`.
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
