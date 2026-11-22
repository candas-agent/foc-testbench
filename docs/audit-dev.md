# Audit of SimpleFOC dev and of our PRs (2026-11-21)

What was tested on the STM32F103 board without moving anything, what is wrong, what is already known upstream,
and what is left to fix. Raw results are in [test-log.md](test-log.md) (21/11 entries); priorities in [topics.md](topics.md).

## How it was tested

All tests run on the STM32F103 (real compiler, soft float, `-Os`), without any peripheral: fake driver (stores the
phase voltages), fake current sense, fake sensor and a **fake clock** (every library call of `_micros()` returns a
test variable, link option `-Wl,--wrap=_Z7_microsv`). Nothing can move or heat, the board can be supplied normally
(the PWM tests need the main supply off, see below). Tests that only compare formulas also run on a PC.

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

## Our PRs

| PR | Result | To do |
|---|---|---|
| [simplefoc#595](https://github.com/simplefoc/Arduino-FOC/pull/595) pins as integers | identical for all 68 M ints tested (pins are `int`, `_NC = -12345`) | nothing |
| [simplefoc#596](https://github.com/simplefoc/Arduino-FOC/pull/596) dt once per loopFOC | **withdrawn on 22/11** (versions identical to dev: v2 -2.3 %, v3 -2.5 %, not worth a new API). Was: **do not merge as is** (maintainer to be told by Candas1). Differs from dev: (1) back to `foc_current` after < 0.3 s in `dc_current` / `estimated_current`: the PI integrates the whole absence, **voltage kick** (voltage.q 1.90 -> 3.70 V, 1.18 -> 1.81 V on the first loop); (2) pause of 0.3-0.5 s: first PI step with another dt (~3 %); (3) two loopFOC in the same microsecond: current filters restart from the new value; (4) `Ts` set then back to NOT_SET: here dev is the odd one. Cause: the new path does not update the filters' / PIDs' own timestamps, the old path then starts from a stale one | rework: each filter / PID keeps its own dt computation as dev, but with the timestamp read at the start of loopFOC (same behaviour as dev, smaller gain, to measure) |
| [Candas1#23](https://github.com/Candas1/Arduino-FOC/pull/23) setPwm one limit | bug found and fixed (15254e1): `voltage_power_supply` < ~3e-39 made `1/Vps` infinite, register 65535 (> 100 %). Retested with the real 6PWM / 3PWM drivers: no unexpected difference, at most 1 timer count | measure the loop time with the guard (motor), update the description |
| [Candas1#24](https://github.com/Candas1/Arduino-FOC/pull/24) PID D = 0 | identical except the documented D = 0 cases (NaN / signed zero), 2 M cases on the target | Candas1: review |

## Bugs found in dev (ae7b3ad)

"Known" = already reported upstream by someone else (do not duplicate; our measurements can be added as evidence).

| # | Where | Problem | Measured effect | Known upstream? |
|---|---|---|---|---|
| 1 | `_sin` / `_cos` | negative angle cast to `unsigned` (undefined in C++): 0 on STM32 | `_sin(a) = 0` for all a < 0, `_cos(a) = 0` for a < -PI/2 | [#505](https://github.com/simplefoc/Arduino-FOC/issues/505) open |
| 2 | consequence of 1: **`angle_openloop` with a negative angle** (`angleOpenloop()` does not normalise, unlike `velocityOpenloop()`) | electrical angle negative | **no voltage applied** (3 phases at 6 V, traced at -35 rad electrical) below -PI/2, wrong vector between -PI/2 and 0; **same with `StepperMotor`** | new (the consequence is not in #505) |
| 3 | `_sin` above ~411775 rad | float -> unsigned saturates | wrong value (very long open loop angle runs) | new (same cause) |
| 4 | trapezoid sector, angle ~ -PI/6 | `_normalizeAngle` returns exactly 2PI -> sector 6 / 12, read past `trap_120_map` / `trap_150_map` | 8 of 8000 angles tested; negative angles only (open loop) | new |
| 5 | non-centred trapezoid, `Uq < 0` | `center = Uq` -> all phases <= 0, clamped to 0 | no voltage in one direction | [#580](https://github.com/simplefoc/Arduino-FOC/pull/580) open |
| 6 | `CurrentSense::getDCCurrent(angle)` | `if (motor_electrical_angle)`: an angle of exactly 0 means "no angle" | positive value when the angle is 0.0 and iq < 0 | new |
| 7 | `_sqrtApprox` (used by `getDCCurrent`) | no Newton step | up to 3.4 % error; closed loop `dc_current`: **0.512 A for 0.5 A**; `_sqrtApprox(-1)` = 3e38 | new |
| 8 | `_atan2` | FLT_MIN guard missing | `_atan2(0,0)` NaN, also (+-inf, +-inf) NaN, (1, NaN) = 0.785 | [#583](https://github.com/simplefoc/Arduino-FOC/pull/583) open (0,0) |
| 9 | `_normalizeAngle` | exactly 2PI for tiny negative angles, inf -> NaN | 4 in 4 M | cause of 4 |
| 10 | `HallSensor`, invalid state 000 / 111 (glitch, broken wire) | `ELECTRIC_SECTORS` gives sector -1, used as is | **angle jumps up to 3 sectors = 180 deg electrical** for that update (reverse torque), back right after, no drift | new (#577, #584, #469 are about other points) |
| 11 | `Commander` limits (`updateVoltageLimit` / `updateCurrentLimit` / `updateVelocityLimit`) | no check: a negative limit swaps the `_constrain` bounds, `atof("nan")` removes the limit | simulated motor: **`MLU-2` -> 2 V at target 0**, **`MLC-1` -> 1.04 A at target 0 A**, **`MLUnan` -> 10 V** (limit was 6 V), **`MLV-5` in angle mode runs the wrong way** (-11 rad for +3) | new |
| 12 | `Commander` `MWT<n>` | invalid modulation type: no `case` in `setPhaseVoltage`, the last voltages stay | **2 V stay applied** after the target goes back to 0 | new |

Also reported by others and not retested here: current sense alignment swaps (#573, #574, #575), HallSensor
uninitialised state (#577), `motion_downsample` off by one and `characteriseMotor` correction factor (#572).

## Reproduced on the powered board (2026-11-22)

`tools/bug_repro.py` (motor moves, low values), details in [test-log.md](test-log.md) (22/11). Applied duty cycles read
from the timer registers.

- **#11 negative limits**: `MLU-0.5` -> 0.5 V applied whatever the target, and again after `ME0` / `ME1`; `MLC-0.3`
  -> the motor accelerated alone at target 0 and reached 10 rad/s with a negative target (aborted). **Stuck target:
  yes. `ME0` always stops, but the next `ME1` applies the same voltage again** until a valid limit is set.
- **#12 `MWT9`**: duties frozen at the last voltage whatever the target; **re-applied at the next `ME1`**.
- **#1-2 angle open loop, negative target**: duty spread 0 % (no voltage) below -PI/2 electrical.
- **#10 hall glitch**: seen in the recording, small current effect at 1 rad/s (within the R_ds_on noise).
- **simplefoc#596**: back from `dc_current` after 0.1 s, **iq 1.79 A and -1.26 A (wrong direction) for 8-19 ms for a
  0.2 A target**; dev at most 0.85 A for single samples. Confirms "do not merge as is".
- Not conclusive: `MLV-2` in angle mode (0.6 V does not start the motor).

## Checked correct

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

## Fix ideas (to discuss, one PR each, with an on-target test)

- 1-3: normalise the electrical angle in `setPhaseVoltage` (or the #505 fix, `int` instead of `unsigned`, which halves
  the positive range). Add our measurement to #505.
- 4: guard the sector index (or fix `_normalizeAngle` for the 2PI result).
- 6: pass "no angle" with a separate overload or NOT_SET instead of 0.
- 7: one Newton step in `_sqrtApprox`, or `sqrtf` where precision matters.
- 10: ignore 000 / 111 (keep the previous sector), check SmoothingSensor too.
- 11-12: refuse negative / NaN limits and invalid modulation types in `Commander::motor()` or in the `update*Limit`
  functions (safety: torque at target 0).
