# FOC test bench: PWM dead-time compensation

Test bench to evaluate a PWM dead-time compensation for [SimpleFOC](https://github.com/simplefoc/Arduino-FOC) on a **B-G431B-ESC1** board driving a **hoverboard motor**, in open loop (no position sensor for now), with the MXLEMMING sensorless observer running in parallel.

Background: the [PWM Deadtime compensation](https://community.simplefoc.com/t/pwm-deadtime-compensation/3767) thread on the SimpleFOC forum.

> ⚠️ Work in progress. The firmware changes under test have only been compile-checked so far, they have not been validated on hardware yet.

## What is tested

The code under test lives in two forks, pulled directly by PlatformIO:

| Library | Branch | Content |
|---|---|---|
| [candas-agent/Arduino-FOC](https://github.com/candas-agent/Arduino-FOC/pull/1) | `feat/deadtime-compensation` | dead-time compensation + automatic calibration |
| [candas-agent/Arduino-FOC-drivers](https://github.com/candas-agent/Arduino-FOC-drivers/pull/1) | `fix/mxlemming-checks` | MXLEMMING observer parameter checks, `fabsf` |

### Dead-time compensation in a nutshell

During the dead-time both transistors of a half bridge are off and the phase voltage is set by the current direction, so each phase loses about `Vbus × dead-time / T_pwm` against its current. This flattens the phase current around its zero crossings.

The compensation adds `±deadtime_compensation` to each phase voltage in the direction of the phase current:

- the phase current directions are **reconstructed from the dq currents** (setpoints by default, or filtered measured currents) with an inverse Park/Clarke transform, instead of using the noisy raw phase currents;
- a **linear ramp** around zero current (`deadtime_comp_threshold`) replaces `sign()` to avoid chattering;
- with the default (setpoints), the compensation is exactly 0 at zero torque, so it cannot cause vibrations when the motor is moved by hand at zero torque.

`motor.calibrateDeadtimeCompensation(max_current)` measures the effective dead-time voltage error: a DC voltage is applied on the d axis (rotor aligned, no movement) in both directions, a line `U = R·I + U0` is fitted, and `deadtime_compensation = 3/4 · U0` (the three phase errors seen on the alpha axis add up to 4/3 of one phase error).

Overview of all the topics explored with this bench (bugs, performance, features, branches and PRs): [docs/topics.md](docs/topics.md). Measurements: [docs/test-log.md](docs/test-log.md).

## Hardware

Three board types are supported, selected by the PlatformIO environment (board settings in `include/board_*.h`):

| Env | Board | Commands | Notes |
|---|---|---|---|
| `esc1` | B-G431B-ESC1 | serial (ST-Link virtual COM port) | low-side current sensing with the STM32G4 op-amps, 12 V |
| `gd32f130` | hoverboard split board TT-SD2.2 "master" (GD32F130C8) | RTT through an ST-Link (`--port rtt`) | current sensing on the low side MOSFETs R_ds_on (2 phases), 26 V, fixed 0.83 µs dead time |
| `smooth_f103_dev`, `hallvel_f103`, ... | gen 1 hoverboard mainboard (STM32F103RC or GD32F103RC), right motor | RTT through an ST-Link (`--port rtt`) | plain SimpleFOC (no dead-time compensation, `BENCH_NO_DEADTIME`), R_ds_on current sense, DC link voltage read at run time |

All the libraries come from GitHub, pinned to the commits that were measured (see the comments in `platformio.ini`):
the project builds on any machine. Only `lib/qfplib` (GPL, envs `*_qfp`) has to be added by hand.

- Hoverboard motor, 15 pole pairs (hall sensors not used: open loop only)
- **Lab power supply with current limit**
- Optional: oscilloscope

### GD32F130 split board

- Driver: `Candas1/Arduino-FOC#dev-gd32` (synced with SimpleFOC master) merged with the compensation, branch `candas-agent/Arduino-FOC#test/gd32-deadtime`. The driver ignores `dead_zone` and sets a fixed `DEAD_TIME` of 60 cycles (0.83 µs, 1.33 % at 16 kHz): `DEAD_ZONE` in `board_gd32f130_split.h` only serves the expected value `Vbus × dead_zone`.
- Power latch: the board switches itself off without the power button. On the bench, bridge the 2 pins of the switch connector; the firmware also holds PB2 high.
- ST-Link: GND, SWDIO, SWCLK on the 4-pin debug header next to the MCU. Do not connect the ST-Link 3.3 V when the board is powered.
- No ideal diode on the supply here: energy fed back by the motor goes to the supply. Keep low speeds and don't spin the wheel fast by hand. The bus voltage is measured once at start-up only (PA4), the ADC is then used by the current sense.
- 8 KB of RAM: the recorder holds 100 samples (`RECORD_SAMPLES`), the tools read the size from the ELF. `pio test` is not available (no serial port).
- Hall sensors (PB11, PF1, PC14) with the SimpleFOC drivers `SmoothingSensor`, linked at start-up with the known alignment of the split board sketch (zero electric angle 2.09 rad, CCW): no motor movement at start-up. `S` prints their state, `SA` re-runs the automatic SimpleFOC alignment if needed (**the motor moves**, then it is disabled). The closed loop modes are allowed (`MC0` torque, `MC1` velocity), always with current control (`MT2`), torque target clamped to `TORQUE_TARGET_MAX` (1 A).
- Observer off at start-up on this board (`DO1` to enable it): no FPU, the loop takes ~0.8 ms without it.
- Commands: same as below, through RTT: `python tools/ol_run.py --port rtt --env gd32f130 ...`, `python tools/campaign.py --port rtt --env gd32f130 ...`.

### Gen 1 hoverboard mainboard (STM32F103RC / GD32F103RC)

- Pins from [EFeru's hoverboard firmware](https://github.com/EFeru/hoverboard-firmware-hack-FOC/blob/main/Inc/defines.h) (right motor): PWM on TIM1 (PA8/9/10, PB13/14/15), phase currents PC4/PC5 (R_ds_on), halls PC10/11/12, power latch PA5, DC link voltage PC2. Board file `include/board_stm32f103_split.h`.
- Some boards carry a GD32F103RC instead of the STM32F103RC: same firmware (STM32 Arduino core, generic STM32F103RC variant, 64 MHz on the internal oscillator), but different flash timings.
- `ALIGN_VOLTAGE` 0.8 V (≈1.5 A on this board, see the test log); DC link voltage gain 29.3 (calibrated on one board, EFeru's default calibration gives 33).
- Do not add `-D SIMPLEFOC_STM32_ADC_INTERRUPT` with SimpleFOC dev: the F1 HAL disables the injected interrupt and the currents freeze (fixed in [simplefoc/Arduino-FOC#594](https://github.com/simplefoc/Arduino-FOC/pull/594)).
- Velocity PI that works on this slower loop (~400 µs): `MVP0.4`, `MVI1`, `MVF0.01`.
- Extra commands: `ST0`/`ST1`/`ST2` sensor (SmoothingSensor / measured hall table / ideal grid), `SV0`..`SV4` velocity estimate of the table sensor, `XF<A>` friction feed-forward, `XG0` / `XG7[,loops]` invalid hall state 000 / 111 injected for 1 (or n) control loops (test of docs/audit-dev.md #10), `XR1` / `XR0` plain SimpleFOC behaviour (the bench no longer sets the voltage limit every loop nor forces foc_current) / bench behaviour, `XK0`..`XK3` voltage feed-forward (off, back-EMF, + R*i, + dq decoupling), `XM<V>[,<comp>]` `characteriseMotor(V)` with the dead-time compensation optionally forced along +d (the bench overcurrent cut does not run during it), `V<rad/s>` / `VO` velocity through a position ramp. Tools: `tools/hall_calib.py` (hall edge calibration, the motor turns slowly), `tools/lowspeed_campaign.py` (low speed comparison).

### Safety

The phase resistance of a hoverboard motor is very low (~0.17 Ω): in open loop `I ≈ U / R`, so **1 V is already ~6 A**. Default limits in [`include/config.h`](include/config.h) are deliberately low. Use a current-limited supply, fix the motor, and stay next to the bench.

## Software setup

1. Install [PlatformIO](https://platformio.org/).
2. Adapt [`include/config.h`](include/config.h) (motor, limits) and the board file `include/board_esc1.h` or `include/board_gd32f130_split.h`: the **real measured supply voltage** (the compensation volts are converted to duty cycle with it).
3. Build and flash:
   ```
   pio run -e esc1 -t upload
   pio device monitor
   ```

Do not add `PIO_FRAMEWORK_ARDUINO_ENABLE_CDC` to the build flags on this board: it uses one of the PWM pins as USB data line. The serial port goes through the ST-Link virtual COM port.

To check that the forked library is the one being compiled: the build fails if `motor.deadtime_compensation` does not exist.

## Serial commands

At start-up the motor does not move and stays **disabled** (`ME1` to enable). It is configured in **velocity open loop, voltage mode, target 0**, with the compensation **disabled**. Without position sensor only the open loop motion controls are allowed (`MC3` velocity, `MC4` angle); in open loop the voltage (or current) is applied even at target 0, so disable the motor when not testing to avoid heating.

### Dead-time compensation (`D`)

| Command | Action |
|---|---|
| `D` | print the settings and the expected value `Vbus × dead_zone` |
| `D0.3` | set `deadtime_compensation` to 0.3 V (`D0` disables it) |
| `DT0.2` | set `deadtime_comp_threshold` (A, or V in voltage mode without phase resistance) |
| `DM1` / `DM0` | foc_current: use filtered measured dq currents / setpoints (default) |
| `DC1.5` | run the calibration with 1.5 A max, sets `deadtime_compensation` |
| `DB0.3` | open loop voltage amplitude [V] |
| `DE1` / `DE0` | open loop: lower the voltage by `4/π × compensation` to keep the same fundamental amplitude with and without compensation |

### Motor (`M`, standard SimpleFOC [motor commands](https://docs.simplefoc.com/commander_motor))

| Command | Action |
|---|---|
| `M2` | target (rad/s in velocity modes) |
| `ME0` / `ME1` | disable / enable the motor |
| `MC3` / `MC4` | velocity / angle open loop (closed loop modes are refused, no sensor) |
| `MT0` / `MT2` | voltage mode / current control (foc_current in the open loop frame) |
| `MLC1.5` | current limit, also the current amplitude in open loop with `MT2` |

In open loop (voltage mode) the voltage limit is managed by the test bench (`DB`, `DE`); in the other modes it is set to `FOC_VOLTAGE_LIMIT`.

### Safety (`X`), recorder (`R`), heartbeat (`P`)

| Command | Action |
|---|---|
| `X` | print the safety settings and the last fault (1 timeout, 2 overcurrent, 3 heartbeat) |
| `XT30` | run timeout [s]: the motor is disabled after this time enabled (0 = none) |
| `XI4` | overcurrent threshold [A] on any phase |
| `XH1` / `XH0` | heartbeat on/off: the motor is disabled if no command arrives for 2 s |
| `XC` | clear the last fault |
| `XP` | loop profile: mean/max time of each part of the loop, CPU cycle counter (`-D BENCH_PROFILE`, env gd32f130) |
| `XB` | micro benchmarks of the float primitives, motor disabled (`-D BENCH_MICROBENCH`, ~3 KB of flash, indicative only) |
| `R838` | record 800 samples (phase A current, compensation, observer angle error) every 838 µs, then disable the motor and dump them as CSV |
| `RN838` | same without serial dump and without disabling the motor (read with pyOCD) |
| `RD` | dump the last recording again |
| `P` | ping (silent), keeps the heartbeat alive |

Defaults are in [`include/config.h`](include/config.h). The calibration current is clamped to `CALIB_MAX_CURRENT_LIMIT` (2 A).

## Debug variables (`dbg`)

All useful values are grouped in the global `dbg` structure (defined in [`lib/testbench`](lib/testbench/src/bench.h), shared by the firmware and the tests), read live with pyOCD (see below):

| Variable | Content |
|---|---|
| `dbg.ia`, `dbg.ib`, `dbg.ic` | measured phase currents [A] |
| `dbg.comp_a`, `dbg.comp_b`, `dbg.comp_c` | compensation added on each phase [V] |
| `dbg.iq`, `dbg.id` | filtered dq currents [A] |
| `dbg.uq`, `dbg.ud` | dq voltages [V] |
| `dbg.electrical_angle`, `dbg.velocity` | angle [rad], velocity [rad/s] |
| `dbg.loop_us` | loopFOC period [µs] |
| `dbg.obs_angle`, `dbg.angle_err` | observer electrical angle, observer − open loop angle [rad] |
| `dbg.fault`, `dbg.enabled` | last safety fault, motor enabled |

`motor.deadtime_compensation` is changed live with the `D` command.

## Test procedure

1. **Calibration** (motor still): `DC1.5`, note the value and the ratio to `Vbus × dead_zone`.
2. **Open loop, voltage mode** (`MT0`) at low speed, e.g. `ME1` then `M0.5`: record `dbg.ia` with `D0`, then with the calibrated value (`DE1` for the same amplitude). The plateau around zero should disappear. The open loop voltage must exceed the back-EMF (~0.32 V per rad/s with KV 17 and 15 pole pairs): 0.3 V is only enough up to ~0.5 rad/s, use the current mode for higher speeds.
3. **Open loop with current control** (`MT2`): the current is regulated to `current_limit` in the open loop frame. Compare `D0`, compensation with setpoints (`DM0`) and with measured currents (`DM1`, threshold above `compensation / R`).
4. **Sensorless observer** (current mode, 1-4 rad/s): `dbg.angle_err` is the observer angle minus the open loop angle. Its mean is the load angle, its ripple (at 6× the electrical frequency) comes from voltage errors such as the dead-time: compare it with and without compensation, and find the lowest speed where it stays stable.

Later, with position sensors (hall sensors to be soldered, or an encoder): foc_current in closed loop, the zero torque test (turn the wheel by hand at zero torque, there should be no vibration), the observer compared to a real angle, and the switch from open loop to sensorless closed loop.

Log the results in [`docs/test-log.md`](docs/test-log.md).

## Automated tests (`pio test`)

The tests run on the board, PlatformIO flashes them and collects the results over serial.

```
pio test -e esc1 -f test_static   # the motor does NOT move
pio test -e esc1 -f test_motor    # THE MOTOR MOVES
```

| Suite | Test | Checks |
|---|---|---|
| `test_static` | `test_current_offset` | current sense offsets < 0.1 A with the driver disabled |
| | `test_compensation_sign` | the voltage added on each phase follows the phase current direction and the ramp (driver disabled, only the computed voltages are compared) |
| `test_motor` | `test_calibration` | 3 calibrations, spread < 15 %, ratio to `Vbus × dead_zone` between 0.25 and 4 |
| | `test_openloop_voltage` | open loop voltage mode at 0.5 rad/s: the zero crossing plateau is smaller with the calibrated compensation |
| | `test_openloop_current` | same with current control (foc_current in the open loop frame) at 2 rad/s |
| | `test_observer_ripple` | observer angle error ripple at 1, 2 and 4 rad/s, lower with the compensation at 1 rad/s |

The run timeout and the overcurrent cut stay active during the tests.

## Measurement campaign

`tools/campaign.py` drives the firmware over serial: for each torque mode, speed and compensation value it runs the motor, records the phase A current on the board, saves the CSV files, a summary table and one comparison plot per mode and speed. The heartbeat is enabled, so the board disables the motor if the script stops.

```
pip install -r tools/requirements.txt
python tools/campaign.py --port COM5 --speeds 0.5 1 2 4 --comps 0 cal --modes voltage current --via pyocd
```

`--comps` takes values in volts and `cal` (runs the calibration first). Voltage mode speeds too high for the open loop voltage (`--ol-voltage`) are skipped. The summary includes the THD, the zero crossing flatness and the observer angle ripple. Results go to `data/<date-time>/`.

### Short monitored run

`tools/ol_run.py` runs the motor for a few seconds in open loop (voltage or current mode), reads the phase currents live with pyOCD, stops above `--imax` (3 A by default) and prints the mean dq currents and voltages. THE MOTOR MOVES.

```
python tools/ol_run.py --port COM8 --voltage 0.6 --speed 0.5 --duration 8
python tools/ol_run.py --port COM8 --mode current --current 1 --speed 2 --duration 6
```

### Other tools

| Tool | Moves the motor | Purpose |
|---|---|---|
| `tools/bench_cmd.py --port rtt --env gd32f130 X D S` | only with `ME1`, `DC`, `SA` (always `ME0` at the end) | send commands, print the replies; `--wait PATTERN TIMEOUT` for long commands (`DC1.5`) |
| `tools/ol_run.py ... --out run.csv` | yes | live reads to CSV, current fundamental fitted on the open loop angle, `--equal` (DE1), `--ramp` |
| `tools/fold_compare.py a.csv b.csv --labels ...` | no | waveforms from `ol_run.py --out` folded on the angle: THD, zero crossing share, plot |
| `tools/zero_torque.py --port rtt --env gd32f130 --comp 0.605 --threshold 1.22` | yes (closed loop, hall board) | zero torque test: no compensation / DM0 / DM1, 20 s each, the motor beeps N times before phase N |
| `tools/equiv_read.py --elf <equiv_pid/pwm elf>` | no (`equiv_pwm_*`: main supply off) | on-target equivalence of PR code vs dev, see [docs/audit-dev.md](docs/audit-dev.md) |
| `tools/equiv_foc_compare.py --dev <elf> --pr <elf> [--pins <elf>]` | no | loopFOC scenarios with a fake clock, dev vs PR traces |
| `tools/equiv_audit_read.py --elf <equiv_audit elf>` | no | audit of SimpleFOC dev functions on the target (fake driver / sensors / clock, simulated motor) |
| `tools/bug_repro.py --elf <elf> --scenario mlu\|mlc\|mwt\|mlv\|olneg\|hall\|dt596` | yes (low values, abort above 3 A / 10 rad/s) | reproduces the bugs of docs/audit-dev.md on the powered board, reads the applied duty cycles from TIM1 |
| `tools/ff_test.py --elf <elf> [--pi P,I,Tf ...] [--modes 0 2]` | yes (+-3 rad/s) | velocity steps with each voltage feed-forward mode, current setpoint recorded |

## Reading the board memory with pyOCD

pyOCD reads the variables through the ST-Link while the firmware runs (only one tool can use the probe at a time). The addresses come from `.pio/build/esc1/firmware.elf`; the layouts are mirrored in [`tools/board_mem.py`](tools/board_mem.py) and checked with `dbg.magic`.

```
python tools/live.py --duration 5 --out live.csv          # all dbg fields, as fast as the probe allows
python tools/live.py --fields ia angle_err --print         # selected fields, printed
```

`campaign.py --via pyocd` reads the recordings from the board memory instead of the serial dump (faster). Commands and the heartbeat still go through serial. On Windows pyOCD needs libusb, normally installed with it by pip.

## Analysing the recordings

The campaign script does this automatically. For recordings exported as CSV by another tool:

```
pip install -r tools/requirements.txt
python tools/compare_currents.py off.csv on.csv --labels "comp off" "comp on" \
    --time-col <time column> --cur-col <current column> --time-scale 1e-3 --out comparison.png
```

The script resamples the data, averages all electrical periods into one, and reports the **THD** and the share of the period where `|i| < 10 %` of the peak (6.4 % for a pure sine, more with a dead-time plateau). Check the column names and the time unit of your export (`--time-scale` converts to seconds).
