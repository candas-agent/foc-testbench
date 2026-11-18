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

## Hardware

- B-G431B-ESC1 (on-board ST-Link, low-side current sensing with the STM32G4 op-amps)
- Hoverboard motor, 15 pole pairs (hall sensors not connected: open loop only)
- **Lab power supply with current limit**
- Optional: oscilloscope

### Safety

The phase resistance of a hoverboard motor is very low (~0.17 Ω): in open loop `I ≈ U / R`, so **1 V is already ~6 A**. Default limits in [`include/config.h`](include/config.h) are deliberately low. Use a current-limited supply, fix the motor, and stay next to the bench.

## Software setup

1. Install [PlatformIO](https://platformio.org/).
2. Adapt [`include/config.h`](include/config.h): motor parameters and the **real measured supply voltage** (the compensation volts are converted to duty cycle with it).
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
| `R838` | record 800 samples (phase A current, compensation, observer angle error) every 838 µs, then disable the motor and dump them as CSV |
| `RN838` | same without serial dump and without disabling the motor (read with pyOCD) |
| `RD` | dump the last recording again |
| `P` | ping (silent), keeps the heartbeat alive |

Defaults are in [`include/config.h`](include/config.h). The calibration current is clamped to `CALIB_MAX_CURRENT_LIMIT` (2 A).

## Watching the variables (MCUViewer)

[MCUViewer](https://github.com/klonyyy/MCUViewer) reads the variables through the ST-Link without slowing down the loop. Point it to `.pio/build/<env>/firmware.elf`.

All useful values are grouped in the global `dbg` structure (defined in [`lib/testbench`](lib/testbench/src/bench.h), shared by the firmware and the tests):

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

`motor.deadtime_compensation` can also be changed live from MCUViewer.

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

## Reading the board memory with pyOCD

pyOCD reads the variables through the ST-Link while the firmware runs, like MCUViewer (close MCUViewer first, only one tool can use the probe). The addresses come from `.pio/build/esc1/firmware.elf`; the layouts are mirrored in [`tools/board_mem.py`](tools/board_mem.py) and checked with `dbg.magic`.

```
python tools/live.py --duration 5 --out live.csv          # all dbg fields, as fast as the probe allows
python tools/live.py --fields ia angle_err --print         # selected fields, printed
```

`campaign.py --via pyocd` reads the recordings from the board memory instead of the serial dump (faster). Commands and the heartbeat still go through serial. On Windows pyOCD needs libusb, normally installed with it by pip.

## Analysing the recordings

The campaign script does this automatically. For MCUViewer recordings, export them to CSV, then:

```
pip install -r tools/requirements.txt
python tools/compare_currents.py off.csv on.csv --labels "comp off" "comp on" \
    --time-col <time column> --cur-col <current column> --time-scale 1e-3 --out comparison.png
```

The script resamples the data, averages all electrical periods into one, and reports the **THD** and the share of the period where `|i| < 10 %` of the peak (6.4 % for a pure sine, more with a dead-time plateau). Check the column names and the time unit of your export (`--time-scale` converts to seconds).
