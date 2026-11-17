# FOC test bench: PWM dead-time compensation

Test bench to evaluate a PWM dead-time compensation for [SimpleFOC](https://github.com/simplefoc/Arduino-FOC) on a **B-G431B-ESC1** board driving a **hoverboard motor with hall sensors**.

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
- Hoverboard motor, 15 pole pairs, hall sensors on the board's hall/encoder connector (`A_HALL1..3`)
- **Lab power supply with current limit**
- Optional: oscilloscope

### Safety

The phase resistance of a hoverboard motor is very low (~0.17 Ω): in open loop `I ≈ U / R`, so **1 V is already ~6 A**. Default limits in [`include/config.h`](include/config.h) are deliberately low. Use a current-limited supply, fix the motor, and stay next to the bench.

## Software setup

1. Install [PlatformIO](https://platformio.org/).
2. Adapt [`include/config.h`](include/config.h): motor parameters and the **real measured supply voltage** (the compensation volts are converted to duty cycle with it).
3. Build and flash:
   ```
   pio run -e esc1_hall -t upload            # hall sensors only
   pio run -e esc1_hall_smoothing -t upload  # hall + SmoothingSensor (angle extrapolation)
   pio device monitor
   ```

Do not add `PIO_FRAMEWORK_ARDUINO_ENABLE_CDC` to the build flags on this board: it uses one of the PWM pins as USB data line. The serial port goes through the ST-Link virtual COM port.

To check that the forked library is the one being compiled: the build fails if `motor.deadtime_compensation` does not exist.

## Serial commands

At start-up the motor is aligned (it moves), then runs in **velocity open loop, voltage mode, target 0**, with the compensation **disabled**.

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
| `MC3` | velocity open loop (`MC1` velocity closed loop, `MC0` torque) |
| `MT0` / `MT2` | voltage / foc_current torque mode |
| `MLC1.5` | current limit |

In open loop (voltage mode) the voltage limit is managed by the test bench (`DB`, `DE`); in the other modes it is set to `FOC_VOLTAGE_LIMIT`.

## Watching the variables (MCUViewer)

[MCUViewer](https://github.com/klonyyy/MCUViewer) reads the variables through the ST-Link without slowing down the loop. Point it to `.pio/build/<env>/firmware.elf`.

All useful values are grouped in the global `dbg` structure:

| Variable | Content |
|---|---|
| `dbg.ia`, `dbg.ib`, `dbg.ic` | measured phase currents [A] |
| `dbg.comp_a`, `dbg.comp_b`, `dbg.comp_c` | compensation added on each phase [V] |
| `dbg.iq`, `dbg.id` | filtered dq currents [A] |
| `dbg.uq`, `dbg.ud` | dq voltages [V] |
| `dbg.electrical_angle`, `dbg.velocity` | angle [rad], velocity [rad/s] |
| `dbg.loop_us` | loopFOC period [µs] |

`motor.deadtime_compensation` can also be changed live from MCUViewer.

## Test procedure

1. **Calibration** (open loop, motor still): `DC1.5`, note the value and the ratio to `Vbus × dead_zone`.
2. **Open loop** at low speed, e.g. `M2`: record `dbg.ia` with `D0`, then with the calibrated value (`DE1` for the same amplitude). The plateau around zero should disappear.
3. **foc_current with hall sensors** (`MT2`, `MC1`): same comparison, then the zero torque test (`MC0`, `M0`, turn the wheel by hand, there should be no vibration).
4. Same with the `esc1_hall_smoothing` firmware.

Log the results in [`docs/test-log.md`](docs/test-log.md).

## Analysing the recordings

Export the MCUViewer recording to CSV, then:

```
pip install -r tools/requirements.txt
python tools/compare_currents.py off.csv on.csv --labels "comp off" "comp on" \
    --time-col <time column> --cur-col <current column> --time-scale 1e-3 --out comparison.png
```

The script resamples the data, averages all electrical periods into one, and reports the **THD** and the share of the period where `|i| < 10 %` of the peak (6.4 % for a pure sine, more with a dead-time plateau). Check the column names and the time unit of your export (`--time-scale` converts to seconds).
