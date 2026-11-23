# On-target equivalence and audit tests

How to check a refactor against SimpleFOC dev on the board, and how dev itself was audited (STM32F103, 2026-11-21).
Results are in [test-log.md](test-log.md) (21/11 and 22/11 entries). The bugs found and what to do about them are
tracked outside this repository.

## How it works

All tests run on the STM32F103 (real compiler, soft float, `-Os`), without any peripheral: fake driver (stores the
phase voltages), fake current sense, fake sensor and a **fake clock** (every library call of `_micros()` returns a
test variable, link option `-Wl,--wrap=_Z7_microsv`). Nothing can move or heat, the board can be supplied normally
(the PWM tests need the main supply off, see below). Tests that only compare formulas also run on a PC.

Rule: the same firmware built with dev on both sides must give 100 % identical results (checks the reference copy).

| Firmware (env) | Source | Reader | What |
|---|---|---|---|
| `equiv_pid_dev_f103` / `equiv_pid_pr_f103` | `src/equiv/equiv_main.cpp` (`EQUIV_PID`) | `tools/equiv_read.py --elf ...` | PR `PIDController` vs a verbatim copy of dev, 2 M random and special cases |
| `equiv_pwm_dev_f103` / `equiv_pwm_pr_f103` | same (`EQUIV_PWM`) | `tools/equiv_read.py --elf ...` | real `BLDCDriver6PWM` (TIM1) and `BLDCDriver3PWM` (PC6-PC8) `setPwm`, timer registers read back vs dev. **Main supply off, board on the ST-Link 3.3 V** (refuses to start above 10 V on the bus) |
| `equiv_foc_dev_f103` / `equiv_foc_pr_f103` | `src/equiv/equiv_foc.cpp` (`EQUIV_FOC`) | `tools/equiv_foc_compare.py --dev ... --pr ...` | `loopFOC` + `move` scenarios (pauses, mode changes, `Ts`, clock wrap), dev vs PR traces bit for bit |
| `equiv_pins_f103` | same (`EQUIV_PINS`) | `tools/equiv_foc_compare.py --pins ...` | `_isset(pin)` vs `pin != _NC` for 68 M ints |
| `equiv_audit_dev_f103` | `src/equiv/equiv_audit.cpp` (`EQUIV_AUDIT`) | `tools/equiv_audit_read.py --elf ...` | ~60 checks of dev: math, modulations, current transforms, Sensor, closed loop on a simulated motor, initFOC alignment, HallSensor, SmoothingSensor, stepper, LPF/PID, Commander |
| PC | `test_native/audit/math_audit.cpp` | `g++ -Dweak= -I test_native/stub -I <simplefoc>/src/common ...` | `_sin`, `_cos`, `_atan2`, `_normalizeAngle`, `_sqrtApprox` vs double |

Build them in the cloud (`gh workflow run build.yml ... -f envs='["equiv_audit_dev_f103"]'`), then the readers
flash and read with pyOCD. The simulated motor (closed loop checks): pp 7, R 1 Ohm, flux 0.01 Wb, J 1e-4, steady
state currents, sensor mounted either way, true zero electric angle 0.7 rad.

## Reproducing on the powered board

`tools/bug_repro.py` (THE MOTOR MOVES, low values) replays the dev bugs found by the audit and reads the applied duty
cycles from TIM1: `mlu` / `mlc` / `mlv` negative or NaN Commander limits, `mwt` invalid modulation type, `olneg`
`angle_openloop` with a negative target, `hall` invalid hall state 000 / 111 (bench command `XG`), `dt596` the
withdrawn simplefoc#596. Use `XR1` (plain SimpleFOC behaviour) and `XR0` after.

## Checked correct in dev (ae7b3ad)

- `_sin` / `_cos` on [0, 2PI] (1.6e-4), sine and SVPWM vectors (centred or not), centred trapezoid, `getFOCCurrents`
  with 2 or 3 phases (3e-4 A).
- `Sensor` velocity and angle at +-0.05 / 3 / 200 rad/s across the 2^31 us wrap of `micros()`.
- Closed loop on the simulated motor, sensor mounted both ways: velocity +-20 rad/s, angle +3 / -3 rad with the
  velocity limit kept, `foc_current` 0.5 / -0.5 A and 2 A limited to 1 A, `estimated_current`, applied voltage never
  above `voltage_limit`, **`initFOC` real alignment** finds the direction and 0.700 rad.
- `velocity_openloop` both ways; `SmoothingSensor` on `HallSensor` continuous across hall edges both ways
  (drivers 1f67e02); `HallSensor` steady rotation across the 2^31 us wrap, reversal, stop.
- LPF step response, PID output ramp, PID anti-windup, target reset by `updateMotionControlType`.

## Not tested yet

Other Commander commands (gains, filters), `motion_downsample`, magnetic sensors and encoders (need the hardware),
current sense alignment (reported by others).
