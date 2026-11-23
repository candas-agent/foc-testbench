# FOC test bench - instructions for Claude

Bench firmware and tools to measure SimpleFOC changes on hoverboard boards (see README.md). This repository holds
code, tools and measurements only. Topics, priorities and decisions are tracked in the private repo
Candas1/simplefoc-work (one issue per topic), not here. Traps of the tools and boards: docs/pitfalls.md.

## Safety rules
- Never flash the board or start/enable the motor without the user's explicit confirmation in the current session,
  and only when the user says they are next to the bench.
- Never raise ALIGN_VOLTAGE, OPENLOOP_VOLTAGE, FOC_VOLTAGE_LIMIT or CURRENT_LIMIT without asking: phase resistance
  ~0.17 Ohm, 1 V is ~6 A.
- Keep the safety features (run timeout, overcurrent cut, heartbeat) enabled; never weaken them without asking.
- The firmware starts with the motor disabled; `ME0` after each test (in open loop the current flows at target 0).
  On an error or unexpected behaviour: `ME0` before investigating.
- Calibration (`DC<amps>`) keeps a DC current for seconds: max 2 A unless the user says otherwise. `XM`
  (characteriseMotor) is blocking, the overcurrent cut does not run: ask first.
- What moves the motor: `ME1`, `DC`, `SA`, `V<rad/s>`, `pio test -f test_motor`, and the tools campaign.py,
  ol_run.py, zero_torque.py, hall_calib.py, lowspeed_campaign.py, perf_run.py, reenable_test.py, trace_run.py,
  bug_repro.py, ff_test.py, trap_test.py (bench_cmd.py with those commands). `test_static` flashes the board.

## Boards
- **GD32F130 split board** (envs `gd32f130`, `gd32*`): 26 V, no ideal diode (energy goes back to the supply: low
  speeds), bus voltage read at start-up only, RTT only (`--port rtt --env gd32f130`), R_ds_on current sense. Driver
  from Candas1 `dev-gd32`. DEAD_ZONE 0.01335 = 60 cycles (0.83 us). Flash with OpenOCD (pitfalls.md).
- **Gen 1 hoverboard mainboard** (STM32F103RC / GD32F103RC, envs `*_f103`): plain SimpleFOC (`BENCH_NO_DEADTIME`),
  right motor, RTT, power button bridged. `ALIGN_VOLTAGE` 0.8 V (~1.5 A). 64 MHz.
- **B-G431B-ESC1** (env `esc1`): 12 V, serial 115200 through the ST-Link, `pio test` suites.

## Workflow
- Build in the cloud: push first, then from Bash
  `gh workflow run build.yml --repo candas-agent/foc-testbench -f envs='["perf_dev_f103","<env>"]'`,
  `gh run watch <id> --exit-status`, `gh run download <id> -D .pio/cloud/<id>`; give the tools `--elf`.
  F103: `.venv/Scripts/pyocd.exe flash -t stm32f103rc <elf>`. Only envs whose libraries are pushed.
- Pin every library to the exact commit measured. Envs made for one PR can be removed once it is merged: the test
  log keeps the commits.
- Comparisons: same bench firmware and flags on both sides, A/B alternated, loop time with `XP` / perf_run.py, flash
  and RAM per symbol (library only).
- "Same behaviour" claims: on-target test against dev (docs/equiv-tests.md), the dev build on both sides must give
  100 % identical results. `equiv_pwm_*` firmwares: main supply off only.
- `XR1` (plain SimpleFOC) for bug reproduction, always `XR0` after.
- The dbg / Sample layouts are mirrored in tools/board_mem.py: update both together and bump DBG_MAGIC.
- Record every result in docs/test-log.md (date, env, commits, numbers) and commit after each experiment.
