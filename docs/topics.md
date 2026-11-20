# Topics explored with this bench

Overview of everything investigated so far (last update 2026-11-20): what kind of change it is, where it applies,
whether it was measured, how much it would improve SimpleFOC, the risk and the effort, and where the code is.
Measurements and details are in [test-log.md](test-log.md).

**Tested**: ✅ measured on a board, 🔧 built or reviewed only, ❌ not tried yet.
**Impact** (how much it improves the library for its users), **Risk** (regressions, side effects, number of users
affected by a mistake), **Effort** (work left to get it merged, including tests): H high, M medium, L low.
**Priority**: P1 high impact or quick win with low risk and low effort, P2 worth doing next, P3 later or needs more
investigation / hardware, P4 not worth it now, ✔ done. The ratings are our judgement, not measurements.

## Priority list

| Priority | Topic | Status |
|---|---|---|
| P1 | `library.json` complete manifest | [simplefoc#593](https://github.com/simplefoc/Arduino-FOC/pull/593) open |
| P1 | STM32F1 injected ADC interrupt re-enabled | [simplefoc#594](https://github.com/simplefoc/Arduino-FOC/pull/594) open |
| P1 | Hall velocity decays between edges | branch ready, measured, no PR yet |
| P1 | STM32 Arduino core 3.0 compatibility | [simplefoc#588](https://github.com/simplefoc/Arduino-FOC/issues/588) (third party): test and support |
| P2 | `dt` once per `loopFOC`, `setPwm` single division | measured in the GD32 optimisation, separate PRs to make |
| P2 | Dead-time compensation | measured, branch exists |
| P2 | Circle limitation of the dq voltage and current | not started |
| P2 | Run time bus voltage and overvoltage protection | reading validated, protection not started |
| P2 | SmoothingSensor float/double fix | [drivers PR](https://github.com/Candas1/Arduino-FOC-drivers/pull/3) draft |
| P2 | GD32 loop optimisations, GD32 core `micros()` report | measured, for the GD32 driver / core |

## Bug fixes

| Topic | Scope | Tested | Impact | Risk | Effort | Priority | Related issues | Branch / PR |
|---|---|---|---|---|---|---|---|---|
| `library.json` only had `build`: registry package without it, SimpleFOC archived, empty generic hardware functions linked | PlatformIO, all platforms | ✅ CI, 133 builds on 24 boards | H | L | L | P1 | [#357](https://github.com/simplefoc/Arduino-FOC/pull/357), [#443](https://github.com/simplefoc/Arduino-FOC/issues/443), [#445](https://github.com/simplefoc/Arduino-FOC/issues/445) | [Candas1#18](https://github.com/Candas1/Arduino-FOC/pull/18) merged → [simplefoc#593](https://github.com/simplefoc/Arduino-FOC/pull/593) open |
| F1 HAL disables the injected ADC interrupt: currents frozen with `SIMPLEFOC_STM32_ADC_INTERRUPT` (regression from `68120bf`) | STM32F1, low-side current sense, interrupt forced | ✅ GD32F103 and STM32F103, motor running | M | L | L | P1 | — | [Candas1#19](https://github.com/Candas1/Arduino-FOC/pull/19) merged → [simplefoc#594](https://github.com/simplefoc/Arduino-FOC/pull/594) open |
| SimpleFOC does not build with STM32 Arduino core 3.0 | STM32 | 🔧 observed (platform pinned to 19.7.1 here) | H | L | L | P1 | [#588](https://github.com/simplefoc/Arduino-FOC/issues/588) | third party |
| Hall velocity valid again after a `_micros()` wrap | `HallSensor` | ❌ | M | L | L | P2 | [#469](https://github.com/simplefoc/Arduino-FOC/pull/469) (open PR) | third party |
| GD32 core `micros()` goes back by up to 1 ms | CommunityGD32Cores core | ✅ worked around (`gd32_micros.c`) | M | L | L | P2 | — | not reported yet |
| `Sensor::getVelocity()`: value held by #453, then reset to 0 | Non hall sensors (encoders, magnetic) | ❌ | H | M | M | P3 | [#453](https://github.com/simplefoc/Arduino-FOC/pull/453), [#533](https://github.com/simplefoc/Arduino-FOC/issues/533), [#592](https://github.com/simplefoc/Arduino-FOC/issues/592) | none (needs an encoder bench) |
| Real speed ~8 % low at 2 rad/s with hall sensors | `HallSensor` | ✅ observed, not fixed | M | M | M | P3 | — | none |
| FreeRTOS pulled in by the library finder on GD32 | PlatformIO + GD32 | ✅ worked around (`lib_ignore`) | L | L | L | P4 | — | bench and sketch |

## Performance

| Topic | Scope | Tested | Impact | Risk | Effort | Priority | Related issues | Branch / PR |
|---|---|---|---|---|---|---|---|---|
| `dt` computed once per `loopFOC` | All families | ✅ part of the GD32 loop optimisation | M | L | L | P2 | [#325](https://github.com/simplefoc/Arduino-FOC/issues/325) | [candas-agent perf/gd32-loop](https://github.com/candas-agent/Arduino-FOC/tree/perf/gd32-loop), PR to split out |
| `setPwm` with a single division | STM32 / GD32 drivers | ✅ same | L | L | L | P2 | — | same |
| SmoothingSensor: one `1/pole_pairs` division, `0.001f` instead of double | Drivers, MCUs without FPU | ✅ -6 % per `update()`, ~1.7 µs per loop | L | L | L | P2 | — | [Candas1/Arduino-FOC-drivers#3](https://github.com/Candas1/Arduino-FOC-drivers/pull/3) draft |
| GD32F130 loop: zero wait state flash linker script, fast math, no FreeRTOS, monotonic `micros()` | GD32 (dev-gd32 driver) | ✅ 1567 → 168 µs | H (GD32 users) | M | M | P2 | — | bench, sketch `perf/gd32-fast-loop` (local) |
| Shared index / cache for `_sincos`, faster `_normalizeAngle` | All families | ✅ part of the GD32 loop optimisation | M | M | M | P3 | [#530](https://github.com/simplefoc/Arduino-FOC/pull/530), [#572](https://github.com/simplefoc/Arduino-FOC/issues/572) | bench (`fastmath.cpp`) |
| Loop code in RAM / 72 MHz on STM32F103 | STM32F103 | ❌ (STM32F103 47 % slower than GD32F103, same code) | M | M | M | P3 | — | none |
| qfplib soft float | Cortex-M0/M3 without FPU | ✅ ~5 % faster | M | H (GPL licence) | L | P4 | — | bench, local |
| Firmware size (small atof, no `lib_archive`) | Projects | ✅ 45.8 → 34.8 KB on the sketch | L | L | L | P4 | — | sketch, local |

## Control and new features

| Topic | Scope | Tested | Impact | Risk | Effort | Priority | Related issues | Branch / PR |
|---|---|---|---|---|---|---|---|---|
| **Hall velocity decays between edges** instead of holding then dropping to 0 | `HallSensor`, all families | ✅ A/B vs dev: stalls -64 / -76 %, current ripple -35 %, no change at 5 rad/s | H | L | L | **P1** | [#453](https://github.com/simplefoc/Arduino-FOC/pull/453), [#533](https://github.com/simplefoc/Arduino-FOC/issues/533) (same question for other sensors), [#469](https://github.com/simplefoc/Arduino-FOC/pull/469) (same function) | [candas-agent fix/hall-velocity-decay](https://github.com/candas-agent/Arduino-FOC/tree/fix/hall-velocity-decay), no PR yet |
| Dead-time compensation | Current sense, all families | ✅ ESC1 and GD32F130 (THD 10 → 2.7 %); not yet on the F103 | H | M | M | P2 | forum thread "PWM Deadtime compensation", [#200](https://github.com/simplefoc/Arduino-FOC/issues/200) | [candas-agent feat/deadtime-compensation](https://github.com/candas-agent/Arduino-FOC/tree/feat/deadtime-compensation) |
| Circle limitation of the dq voltage and current | All families | ❌ | H | M | M | P2 | [#246](https://github.com/simplefoc/Arduino-FOC/issues/246), [#317](https://github.com/simplefoc/Arduino-FOC/issues/317) | none |
| Overvoltage protection, run time bus voltage | All families | ✅ run time bus voltage read on STM32; no protection yet | H (safety) | M | M | P2 | — | bench (reading only) |
| Velocity PI tuned per MCU loop time | Documentation | ✅ STM32F103: P 0.4, I 1, Tf 10 ms | M | L | L | P2 | — | test log |
| Field weakening | All families, useful for hoverboard motors | ❌ (needs the circle limitation) | H | H | H | P3 | [#246](https://github.com/simplefoc/Arduino-FOC/issues/246), [#317](https://github.com/simplefoc/Arduino-FOC/issues/317) | none |
| Automatic minimum low-side on-time | Low-side current sense | ❌ | M | M | M | P3 | — | none |
| ADC sampling point shifted with timer channel 4 | STM32F1 (already done on GD32) | 🔧 reviewed (CC4 fires twice per period) | M | M | M | P3 | — | [Candas1 CC4](https://github.com/Candas1/Arduino-FOC/tree/CC4) (2024, to rewrite) |
| Battery side current limit (DC link shunt) | Boards with a DC link shunt | ❌ | M | M | M | P3 | — | none |
| Hall sector table and calibration (VESC style) | Hall | ✅ edges within ±5°, no measurable effect on this motor | L | L | M | P4 | [drivers#67](https://github.com/simplefoc/Arduino-FOC-drivers/pull/67) (60° hall detection), [forum post](https://community.simplefoc.com/t/hall-effect-sensor-skip-calibration/235/20) | bench only (`tools/hall_calib.py`, `hall_table.*`) |
| Velocity from the measured sector widths | Hall | ✅ good at low speed, worse at 5 rad/s | L | M | L | P4 | — | bench only |
| Velocity averaged over 6 hall edges | Hall | ✅ +5 % overspeed, oscillates at 5 rad/s | L | M | L | P4 | — | bench only |
| Friction feed-forward | Generic | ✅ no effect | L | L | L | P4 | — | bench (`XF` command) |
| Velocity through a position ramp at low speed | Generic | ✅ vibrates with P_angle 5-20 | M | M | M | P4 (revisit with tuning) | [#534](https://github.com/simplefoc/Arduino-FOC/issues/534) | bench (`V` command) |
| Cogging compensation | Encoder only | ❌ (hall sensors too coarse) | M | M | H | P4 | — | future bench |
| Hall to observer handover | Hall + MXLEMMING | ❌ | M | H | H | P4 | — | none |
| R_ds_on temperature compensation | R_ds_on current sense | ❌ | L | M | M | P4 | — | none |

## ADC infrastructure and boards

| Topic | Scope | Tested | Impact | Risk | Effort | Priority | Related issues | Branch / PR |
|---|---|---|---|---|---|---|---|---|
| Reading other ADC channels (`_readRegularADCVoltage`) | STM32 | ✅ 0 errors with and without the ADC interrupt | — | — | — | ✔ already in dev | — | — |
| GD32 API update | dev-gd32 driver | ✅ built | — | — | — | ✔ | — | [Candas1#17](https://github.com/Candas1/Arduino-FOC/pull/17) merged |
| ADC noise at 72 MHz vs 48 MHz | GD32F130 | ❌ | L | L | L | P3 | — | none (a measurement) |
| Two motors on the F103 hoverboard board (TIM8 trigger) | STM32F1 + hoverboard | ❌ | M | H | H | P3 | — | none |
| STM32 ADC unification, injected + regular channels | STM32, all families | 🔧 reviewed, bugs found | M | H | H | P4 | — | Candas1 `new_adc*` (old) |

## Tools and bench

| Topic | Scope | Tested | Impact | Risk | Effort | Priority | Related issues | Branch / PR |
|---|---|---|---|---|---|---|---|---|
| PlatformIO CI (library archiving check) | SimpleFOC repository | ✅ | M | L | L | P2 | [#593](https://github.com/simplefoc/Arduino-FOC/pull/593) | [candas-agent ci/platformio-libarchive](https://github.com/candas-agent/Arduino-FOC/tree/ci/platformio-libarchive) |
| Unit tests on a PC (PlatformIO native, in the cloud) | Tooling | ❌ | M | L | M | P3 | — | none |
| 5065 motor bench with three sensors (on-board encoder, halls, shaft encoder) | Bench | ❌ | M (enables encoder tests) | L | H | P3 | — | idea |
| Multi-board bench (ESC1, GD32F130, GD32F103, STM32F103) | Bench | ✅ | — | — | — | ✔ | — | this repository |
