# Topics explored with this bench

Overview of everything investigated so far (last update 2026-11-21): what kind of change it is, where it applies,
whether it was measured, how much it would improve SimpleFOC, the risk and the effort, and where the code is.
Measurements and details are in [test-log.md](test-log.md). On-target audit of dev and of our PRs (bugs, what is
known upstream, what is left): [audit-dev.md](audit-dev.md).

**Tested**: ✅ measured on a board, 🔧 built or reviewed only, ❌ not tried yet.
**Impact** (how much it improves the library for its users), **Risk** (regressions, side effects, number of users
affected by a mistake), **Effort** (work left to get it merged, including tests): H high, M medium, L low.
**Priority**: P1 high impact or quick win with low risk and low effort, P2 worth doing next, P3 later or needs more
investigation / hardware, P4 not worth it now, ✔ done. The ratings are our judgement, not measurements.

## Priority list

| Priority | Topic | Status |
|---|---|---|
| P1 | `library.json` complete manifest | [simplefoc#593](https://github.com/simplefoc/Arduino-FOC/pull/593) open |
| P1 | `dt` once per loopFOC (loopFOC -6.1 % on F103) | [simplefoc#596](https://github.com/simplefoc/Arduino-FOC/pull/596) open, **do not merge as is**: differences with dev found on the target (voltage kick back from dc/estimated current), rework needed, see [audit-dev.md](audit-dev.md) |
| P1 | `setPwm` one limit and one division, guard for invalid voltages (loopFOC -3.0 % on F103) | [Candas1#23](https://github.com/Candas1/Arduino-FOC/pull/23) (15254e1, overflow bug found on the target and fixed): measure the loop time with the guard, update the description |
| P1 | Current sense pins compared as integers (loopFOC -2.7 % on F103) | [simplefoc#595](https://github.com/simplefoc/Arduino-FOC/pull/595) open |
| P1 | STM32F1 injected ADC interrupt re-enabled | [simplefoc#594](https://github.com/simplefoc/Arduino-FOC/pull/594) open |
| P1 | Hall velocity decays between edges | [candas-agent#3](https://github.com/candas-agent/Arduino-FOC/pull/3) open (review, then upstream PR) |
| P1 | Dead-time compensation | measured on 3 boards, [candas-agent#2](https://github.com/candas-agent/Arduino-FOC/pull/2) open (review, then upstream PR) |
| P1 | STM32 Arduino core 3.0 compatibility | [simplefoc#588](https://github.com/simplefoc/Arduino-FOC/issues/588) (third party): test and support |
| P1 | PID derivative skipped when D is 0 (-2.2 % with D = 0, +1.1 % with D set) | [Candas1#24](https://github.com/Candas1/Arduino-FOC/pull/24), tested on the target: review |
| P1 | Safety: Commander accepts negative / NaN limits and invalid modulation types (torque or voltage at target 0) | found on the target ([audit-dev.md](audit-dev.md) #11-12), not reported upstream |
| P1 | `angle_openloop` (and stepper) with a negative angle applies no voltage on STM32 (`_sin` of a negative angle = 0) | found on the target, cause in [simplefoc#505](https://github.com/simplefoc/Arduino-FOC/issues/505): add our evidence, propose a fix |
| P2 | HallSensor glitch 000/111: angle jumps 180 deg electrical for one update | found on the target, not reported upstream |
| P2 | `getDCCurrent`: sign wrong at angle 0, `_sqrtApprox` 3.4 % (dc_current +2.4 %); trapezoid sector out of the map at ~ -PI/6 | found on the target, not reported upstream |
| P2 | Fewer soft float operations per loop (divisions, float compares ~20 % of the F103 CPU time) | PC profile done, PID derivative done (see above); remaining: LowPassFilter alpha division, compares |
| P2 | Unit tests for edge cases of refactors | on-target test firmwares done (`src/equiv/`, fake clock / driver / sensors, simulated motor), PC math test (`test_native/`) |
| P2 | Circle limitation of the dq voltage and current | not started |
| P2 | Run time bus voltage and overvoltage protection | reading validated, protection not started |
| P2 | SmoothingSensor float/double fix | [drivers PR](https://github.com/Candas1/Arduino-FOC-drivers/pull/3) draft |
| P2 | GD32 loop optimisations, GD32 core `micros()` report | measured, for the GD32 driver / core |
| P3 | Low speed (< 2 rad/s) limited by the R_ds_on current sense errors | diagnosed, needs an independent current reference (scope + current probe) |

## Bug fixes

| Topic | Scope | Tested | Impact | Risk | Effort | Priority | Related issues | Branch / PR |
|---|---|---|---|---|---|---|---|---|
| `library.json` only had `build`: registry package without it, SimpleFOC archived, empty generic hardware functions linked | PlatformIO, all platforms | ✅ CI, 133 builds on 24 boards | H | L | L | P1 | [#357](https://github.com/simplefoc/Arduino-FOC/pull/357), [#443](https://github.com/simplefoc/Arduino-FOC/issues/443), [#445](https://github.com/simplefoc/Arduino-FOC/issues/445) | [Candas1#18](https://github.com/Candas1/Arduino-FOC/pull/18) merged → [simplefoc#593](https://github.com/simplefoc/Arduino-FOC/pull/593) open |
| F1 HAL disables the injected ADC interrupt: currents frozen with `SIMPLEFOC_STM32_ADC_INTERRUPT` (regression from `68120bf`) | STM32F1, low-side current sense, interrupt forced | ✅ GD32F103 and STM32F103, motor running | M | L | L | P1 | — | [Candas1#19](https://github.com/Candas1/Arduino-FOC/pull/19) merged → [simplefoc#594](https://github.com/simplefoc/Arduino-FOC/pull/594) open |
| SimpleFOC does not build with STM32 Arduino core 3.0 | STM32 | 🔧 observed (platform pinned to 19.7.1 here) | H | L | L | P1 | [#588](https://github.com/simplefoc/Arduino-FOC/issues/588) | third party |
| Hall velocity valid again after a `_micros()` wrap | `HallSensor` | ❌ | M | L | L | P2 | [#469](https://github.com/simplefoc/Arduino-FOC/pull/469) (open PR) | third party |
| GD32 core `micros()` goes back by up to 1 ms | CommunityGD32Cores core | ✅ worked around (`gd32_micros.c`) | M | L | L | P2 | — | not reported yet |
| `Sensor::getVelocity()`: value held by #453, then reset to 0 | Non hall sensors (encoders, magnetic) | ❌ | H | M | M | P3 | [#453](https://github.com/simplefoc/Arduino-FOC/pull/453), [#533](https://github.com/simplefoc/Arduino-FOC/issues/533), [#592](https://github.com/simplefoc/Arduino-FOC/issues/592) | none (needs an encoder bench) |
| Real speed ~8 % low at 2 rad/s with hall sensors | `HallSensor` + dead time | ✅ gone with dead-time compensation + hall velocity decay together (1.98 at 2 rad/s); with the soft PI -4 % at 1 rad/s instead of -14 % | M | — | — | ✔ (by the two PRs) | — | [candas-agent#2](https://github.com/candas-agent/Arduino-FOC/pull/2) + [#3](https://github.com/candas-agent/Arduino-FOC/pull/3) |
| FreeRTOS pulled in by the library finder on GD32 | PlatformIO + GD32 | ✅ worked around (`lib_ignore`) | L | L | L | P4 | — | bench and sketch |

## Performance

| Topic | Scope | Tested | Impact | Risk | Effort | Priority | Related issues | Branch / PR |
|---|---|---|---|---|---|---|---|---|
| `dt` computed once per `loopFOC` | All families | ✅ STM32F103: loopFOC -18.1 us (-6.1 %), +248 B flash, +4 B RAM, with the fix (dt since the previous foc_current step, user `Ts` kept); re-enable test: filters reset after a pause as in dev; GD32F130 203 -> 189 us (first version) | M | L | L | **P1** | [#325](https://github.com/simplefoc/Arduino-FOC/issues/325) | [Candas1#21](https://github.com/Candas1/Arduino-FOC/pull/21) merged → [simplefoc#596](https://github.com/simplefoc/Arduino-FOC/pull/596) open |
| Current sense pins compared as integers (`pin != _NC` instead of `_isset(pin)`) | All families (generic current sense + STM32 ADC), gain on MCUs without FPU | ✅ STM32F103: loopFOC -7.9 us (-2.7 %), -120 B flash | L | L | L | **P1** | — (not reported before) | [Candas1#20](https://github.com/Candas1/Arduino-FOC/pull/20) merged → [simplefoc#595](https://github.com/simplefoc/Arduino-FOC/pull/595) open |
| The three together | | ✅ STM32F103: loopFOC 297.1 -> 255.4 us (-14 %), total loop -11.5 %, flash -32 B | | | | | | [candas-agent test/perf-acceptable](https://github.com/candas-agent/Arduino-FOC/tree/test/perf-acceptable) |
| `setPwm`: one limit and one division (3PWM, 6PWM) | All families, gain on MCUs without FPU | ✅ STM32F103: loopFOC -9.0 us (-3.0 %) with the guard for invalid voltages (Vps <= 0: 0 % instead of 100 %), -56 B flash; PC equivalence test: at most one timer count | M | L | L | **P1** | [#356](https://github.com/simplefoc/Arduino-FOC/issues/356) (yours, 2023), [#529](https://github.com/simplefoc/Arduino-FOC/pull/529) (macros, open) | [candas-agent perf/setpwm-single-limit](https://github.com/candas-agent/Arduino-FOC/tree/perf/setpwm-single-limit) |
| SmoothingSensor: one `1/pole_pairs` division, `0.001f` instead of double | Drivers, MCUs without FPU | ✅ -6 % per `update()`, ~1.7 µs per loop | L | L | L | P2 | — | [Candas1/Arduino-FOC-drivers#3](https://github.com/Candas1/Arduino-FOC-drivers/pull/3) draft |
| GD32F130 loop: zero wait state flash linker script, fast math, no FreeRTOS, monotonic `micros()` | GD32 (dev-gd32 driver) | ✅ 1567 → 168 µs | H (GD32 users) | M | M | P2 | — | bench, sketch `perf/gd32-fast-loop` (local) |
| Shared index / cache for `_sincos`, faster `_normalizeAngle` | All families | ✅ part of the GD32 loop optimisation | M | M | M | P3 | [#530](https://github.com/simplefoc/Arduino-FOC/pull/530), [#572](https://github.com/simplefoc/Arduino-FOC/issues/572) | bench (`fastmath.cpp`) |
| Measure simplefoc#530 (faster `_normalizeAngle`, third party) on the F103 | F103 / MCUs without FPU | 🔧 built on dev (-376 B flash); PC comparison: can return slightly negative angles (trapezoid sector -1); dev returns 2PI in rare cases (sector 6) | M | M | L | P2 (feedback to #530) | [#530](https://github.com/simplefoc/Arduino-FOC/pull/530) | env `perf_pr530_f103`, `tools/normalize_equiv.py` |
| Velocity filter and PI from the move() time (rest of #325) | All families | ❌ (2 clock reads + dt computations per loop) | M | M | M | P2 (after #596) | [#325](https://github.com/simplefoc/Arduino-FOC/issues/325) | none |
| Dead-time compensation cost on MCUs without FPU | `BLDCMotor::setPhaseVoltage` | ✅ measured: +31 µs per `loopFOC` on the STM32F103 (~2000 cycles of soft float at 64 MHz); not measured on an FPU MCU | L | L | M | P3 | — | [candas-agent#2](https://github.com/candas-agent/Arduino-FOC/pull/2) (measure first, then optimise if worth it) |
| Loop code and libgcc soft float routines in RAM on STM32F103 | STM32F103 | ✅ slower: loopFOC +20 % (libgcc only, veneers) / +3.6 % (libgcc + loop files, +23 KB RAM); SRAM code goes through the system bus | — | — | — | P4 not worth it | — | `ld/f103rc_ram_*.ld`, envs `perf_ramgcc_f103`, `perf_ramhot_f103` |
| Fewer soft float operations per loop: divisions (LowPassFilter alpha, PID `/dt`, hall velocity, `setPwm`) and compares (`_constrain`, `_isset` on floats, ~20 % of the CPU time) | All families, mostly MCUs without FPU | ✅ PC profile on the F103: ~70 % of the time in libgcc soft float (add 17 %, mul 15 %, div 9 %, compares ~20 %) | M | L | M | P2 | — | none |
| STM32F103 clock | STM32F103 without crystal (hoverboard) | ✅ 64 MHz from the HSI (maximum without a crystal), not 72 | — | — | — | ✔ nothing to gain | — | test log |
| -O2 instead of -Os on STM32F103 | Projects | ✅ loopFOC -1 % for +4 KB (loop files) / +12 KB (all) | L | L | L | P4 | — | envs `perf_o2hot_f103`, `perf_o2_f103` |
| Double precision promotions | SimpleFOC dev | ✅ `-Wdouble-promotion` + ELF callers: only SmoothingSensor in the loop (drivers PR), library clean | — | — | — | ✔ nothing to fix | — | env `dblaudit_f103` |
| qfplib soft float | Cortex-M0/M3 without FPU | ✅ ~5 % faster | M | H (GPL licence) | L | P4 | — | bench, local |
| Firmware size (small atof, no `lib_archive`) | Projects | ✅ 45.8 → 34.8 KB on the sketch | L | L | L | P4 | — | sketch, local |
| `SIMPLEFOC_DISABLE_DEBUG` user flag | Projects | ✅ -5.4 KB flash on the F103 bench firmware | L | L | L | P3 (documentation) | — | env `size_nodebug_f103` |
| LTO (`-flto`) | Projects | ✅ built: **breaks SimpleFOC** (weak hardware functions: STM32 current sense dropped) | — | H | — | P4 never | — | env `size_lto_f103`, test log 21/11 |

## Control and new features

| Topic | Scope | Tested | Impact | Risk | Effort | Priority | Related issues | Branch / PR |
|---|---|---|---|---|---|---|---|---|
| **Hall velocity decays between edges** instead of holding then dropping to 0 | `HallSensor`, all families | ✅ A/B vs dev: stalls -64 / -76 %, current ripple -35 %, no change at 5 rad/s; with the dead-time compensation 0 stalls at ±2 rad/s. Cost: +16 B flash, +4 B RAM, loop time not measurable (F103) | H | L | L | **P1** | [#453](https://github.com/simplefoc/Arduino-FOC/pull/453), [#533](https://github.com/simplefoc/Arduino-FOC/issues/533) (same question for other sensors), [#469](https://github.com/simplefoc/Arduino-FOC/pull/469) (same function) | [candas-agent#3](https://github.com/candas-agent/Arduino-FOC/pull/3) (fix/hall-velocity-decay → dev) |
| **Dead-time compensation** | Current sense, all families | ✅ ESC1 and GD32F130 (THD 10 → 2.7 %); STM32F103: 0.38 V (1.45 × programmed), stalls at 2 rad/s -90 %, iq ripple -40 %, neutral at 5 rad/s; with the hall velocity decay: 0 stalls at ±2 rad/s, iq ripple -60 %, speed error gone. No effect below ~1 rad/s (threshold 0.02-0.2 A tried). Cost on the F103 (no FPU): `loopFOC` +31 µs (+10 %) when active, +3 µs at 0; +0.5 KB flash (+0.9 KB calibration) | H | M | M | **P1** | forum thread "PWM Deadtime compensation", [#200](https://github.com/simplefoc/Arduino-FOC/issues/200) | [candas-agent#2](https://github.com/candas-agent/Arduino-FOC/pull/2) (feat/deadtime-compensation-dev → dev; #1 on master closed) |
| Circle limitation of the dq voltage and current | All families | ❌ | H | M | M | P2 | [#246](https://github.com/simplefoc/Arduino-FOC/issues/246), [#317](https://github.com/simplefoc/Arduino-FOC/issues/317) | none |
| Overvoltage protection, run time bus voltage | All families | ✅ run time bus voltage read on STM32; no protection yet | H (safety) | M | M | P2 | — | bench (reading only) |
| Velocity PI tuned per MCU loop time | Documentation / examples | ✅ STM32F103 hoverboard: P 0.2, I 0.3, Tf 10 ms (with compensation + decay), as good or better than 0.4 / 1 from 1 to 5 rad/s, no speed dependent gains needed; softer or a longer filter is worse | M | L | L | P2 | — | test log |
| Low speed stalls below 2 rad/s come from current sense errors | R_ds_on current sense (hoverboard boards) | ✅ diagnosed: stalls mapped on the hall-counted position sit in 1-2 electrical sectors out of 6, not at a mechanical spot; a 0.02-0.04 A offset change moves that sector; offsets drift with temperature (0.02 → 0.05 A); re-centring at 0 V or from the 1x ud ripple helps a little (stalls 104 → 73), not reproducible enough to fix it. Edge log (exact times): with both PRs a ±40 % speed ripple remains, locked to the electrical position and repeatable (1-3 ms sd per sector); the hall sector widths (54-67°) do not explain it | M | M | H | P3 | — | bench (`XO`, `tools/offset_trim.py`, `offset_ripple.py`, `stall_map.py`, `stall_sectors.py`); needs a scope + current probe |
| Field weakening | All families, useful for hoverboard motors | ❌ (needs the circle limitation) | H | H | H | P3 | [#246](https://github.com/simplefoc/Arduino-FOC/issues/246), [#317](https://github.com/simplefoc/Arduino-FOC/issues/317) | none |
| Automatic minimum low-side on-time | Low-side current sense | ❌ | M | M | M | P3 | — | none |
| ADC sampling point shifted with timer channel 4 | STM32F1 (already done on GD32) | 🔧 reviewed (CC4 fires twice per period) | M | M | M | P3 | — | [Candas1 CC4](https://github.com/Candas1/Arduino-FOC/tree/CC4) (2024, to rewrite) |
| Battery side current limit (DC link shunt) | Boards with a DC link shunt (gen 1 hoverboard: PC1 right motor, PC0 left, from EFeru) | ❌ (not usable as a phase current reference: tiny at low speed, 0 at the PWM centre sampling) | M | M | M | P3 | — | none |
| Hall sector table and calibration (VESC style) | Hall | ✅ edges within ±5°, no measurable effect on this motor | L | L | M | P4 | [drivers#67](https://github.com/simplefoc/Arduino-FOC-drivers/pull/67) (60° hall detection), [forum post](https://community.simplefoc.com/t/hall-effect-sensor-skip-calibration/235/20) | bench only (`tools/hall_calib.py`, `hall_table.*`) |
| Velocity from the measured sector widths | Hall | ✅ good at low speed, worse at 5 rad/s | L | M | L | P4 | — | bench only |
| Velocity averaged over 6 hall edges | Hall | ✅ +5 % overspeed, oscillates at 5 rad/s | L | M | L | P4 | — | bench only |
| Friction feed-forward | Generic | ✅ no effect, also with the soft PI at 1 rad/s (0.1-0.3 A) | L | L | L | P4 | — | bench (`XF` command) |
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
| Unit tests on a PC (PlatformIO native, in the cloud) | Tooling | ❌ (needed: behaviour checks of refactors, see simplefoc#596) | M | L | M | P2 | — | none |
| Cloud builds: GitHub Actions, PlatformIO envs in parallel, firmware artifacts | Bench | ✅ 3 envs in 1-2 min (local: 1 min with the cache, ~8 min for a new library commit); .bin identical to the local build except 3 newlib path strings | — | — | — | ✔ | — | `.github/workflows/build.yml`, tools `--elf` |
| PC sampling profile while running (DWT_PCSR) | Bench | ✅ ~70 % of the F103 loop in libgcc soft float | — | — | — | ✔ | — | `tools/perf_run.py --pc N` |
| Re-enable test: filtered iq frozen during a pause, first loops after enabling | Bench | ✅ shows the filter reset (or not) after a pause | — | — | — | ✔ | — | `tools/reenable_test.py`, recorder `iq` / `en` fields |
| Equivalence test of a refactor on the PC (float32, C semantics, all special values) | Bench | ✅ setPwm: at most one timer count with valid voltages | — | — | — | ✔ | — | `tools/setpwm_equiv.py` |
| 5065 motor bench with three sensors (on-board encoder, halls, shaft encoder) | Bench | ❌ | M (enables encoder tests) | L | H | P3 | — | idea |
| Multi-board bench (ESC1, GD32F130, GD32F103, STM32F103) | Bench | ✅ | — | — | — | ✔ | — | this repository |
| Low speed campaign options: dead-time A/B (`--dt V@threshold`), PI / friction sweeps (`--pi P/I/Tf/XF`), targets, PC polled hall edges (`--edges`) | Bench | ✅ | — | — | — | ✔ | — | `tools/lowspeed_campaign.py` |
| Position without absolute encoder: hall sectors counted since boot (`dbg.hall_count`), stall maps | Bench | ✅ | — | — | — | ✔ | — | `tools/stall_map.py`, `tools/stall_sectors.py` |
| Hall edge log in firmware: each sector change timestamped in the interrupt, ring buffer read by pyOCD | Bench (F103, 8 KB RAM) | ✅ no edge lost; removes the polling spikes (speed sd 0.80 → 0.44 on the same run) | — | — | — | ✔ | — | `hall_edges` in bench.cpp, `BoardMemory.read_hall_edges()` |
| Speed and current against time, A/B charts | Bench | ✅ | — | — | — | ✔ | — | `tools/trace_run.py`, `plot_trace.py`, `plot_ab.py`, `docs/figures/` |
| Cost of a PR: loop time (XP profiler) and flash / RAM per symbol, library pinned to the exact commits | Bench | ✅ | — | — | — | ✔ (to do for every feature / performance PR) | — | `tools/perf_run.py`, envs `perf_dev_f103`, `perf_hall_f103` |
| ST-Link read throughput | Bench | ✅ 1.35 ms per read minimum (USB round trip), ~90 KB/s at best; ~20 % pyOCD Python | — | — | — | ✔ | — | test log |
| Generic firmware ring buffer: loop samples (iq, ud, angle, loop time) streamed continuously, fault snapshots | Bench | ❌ (< ~60 KB/s through the ST-Link: 1 loop in 2-4) | M (low speed diagnosis) | L | M | P2 | — | idea |
| Low speed campaign on the firmware edge log instead of the PC polled hall states | Bench | ❌ | M (exact stall counts) | L | L | P2 | — | `tools/lowspeed_campaign.py` |
