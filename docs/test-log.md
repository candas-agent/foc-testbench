# Test log

One entry per test session. Keep the CSV exports in `data/<date>/` (ignored by git by default, add them explicitly if you want to share them).

## Summary (2026-11-18)

| | B-G431B-ESC1 | GD32F130 split board |
|---|---|---|
| Supply, PWM | 12 V, 25 kHz | 26 V, 16 kHz |
| Programmed dead time | 0.8 µs (dead_zone 2 %) | 0.83 µs (60 cycles, fixed by the GD32 driver) |
| Expected loss Vbus × dead time | 0.24 V | 0.35 V |
| Calibrated compensation | 0.113 V (ratio 0.47) | 0.605 V (ratio 1.75) |
| Effective dead time (comp / Vbus × T_pwm) | ~0.38 µs | **~1.45 µs** |
| R estimated (motor + wires + switches) | 0.49 Ohm | 0.50 Ohm |
| Best result | current mode 2 A: THD 5.3 % -> 2.3 % (mean of 4 pairs) | voltage mode 1.8 V / 2 A: THD 10.0 % -> 2.7 % |

- **Why the GD32 board needs so much compensation**: its effective dead time is ~1.75 x the programmed one
  (slow MOSFETs and gate drivers add switching delays; on the ESC1 the effective dead time is shorter than the
  programmed one), the 26 V bus turns that time into a larger voltage, and the hoverboard motor needs only ~1 V
  for 2 A: the ~0.6 V lost per phase is more than half of the useful voltage. At 0.6 V no current flows, at 0.9 V
  the wheel only turns with the compensation. Consistent with the 0.7 V found by hand on a GD32 board in 2023
  (SimpleFOC forum), and with halving the programmed dead time there (60 -> 30 cycles) only bringing it to 0.5 V.
- **Lowering the programmed dead time** would reduce the loss but risks shoot-through with these slow switches:
  the compensation is the safe way.
- **Calibration**: within a factor ~2 of the best value on the ESC1 (0.11-0.24 V equivalent), close to the
  value found by hand on the GD32 board. The 2x forum rule over-compensates on the ESC1.
- **Compensation source**: on the setpoints (DM0, default) it is exactly 0 at zero torque. On the measured
  currents (DM1, threshold >= compensation / R) the wheel felt freer at zero torque on the GD32 board, without
  vibration: better for low torque use. One user, two runs.
- **GD32 board specifics**: R_ds_on current sensing needs the driver enabled while the offsets are measured,
  noise +-0.8 A; loop ~0.8-1.2 ms (no FPU), observer off, current controller at 100 Hz; hall alignment
  2.09 rad / CCW from the split board sketch.
- **Not done yet**: closed loop velocity tests with and without compensation, repeating the GD32 comparisons.
- **GD32 loop time** (2026-11-18, zero torque, motor enabled): 1567 -> 219 us (observer off), 1995 -> 286 us (on):
  code and virtual tables in the first 32 KB of flash (the only zero wait state area), see the last entry.

## Setup

| Item | Value |
|---|---|
| Board | B-G431B-ESC1 |
| Motor | hoverboard, 15 pole pairs, open loop (no sensor) |
| Supply voltage (measured) | 12 V (lab supply, ideal diode on +) |
| PWM frequency / dead_zone | 25 kHz / 0.02 |
| Firmware commit (Arduino-FOC fork) | 52c1205 (drivers fork 624b912) |

## Results

| Date | Mode (MT0/MT2) | Speed | Comp [V] | Threshold | Calibration [V] | THD [%] | \|i\|<10% [%] | Notes |
|---|---|---|---|---|---|---|---|---|
| 2026-11-18 | calibration DC1.5 | 0 | - | 0.2 | 0.113 (ratio 0.47) | | | R est. 0.49 Ohm, min. threshold 0.23 A |
| 2026-11-18 | MT0 (DB0.3, DE1) | 0.5 | 0 | 0.2 | - | 4.27 | 7.5 | peak 0.40 A, obs ripple 4.6 deg, wheel only oscillated |
| 2026-11-18 | MT0 (DB0.3, DE1) | 0.5 | 0.113 | 0.2 | 0.113 | 8.47 | 6.0 | peak 0.43 A, obs ripple 25.7 deg, wheel only oscillated |
| 2026-11-18 | MT0 (DB0.6, DE0) | 0.5 | 0 | 0.2 | - | | | tools/ol_run.py 8 s, peak 1.0 A, wheel turns |
| 2026-11-18 | MT0 (DB0.6, DE1) | 0.5 | 0 | 0.2 | - | 3.43 | 7.5 | peak 0.70 A, obs ripple 11.7 deg, wheel turns |
| 2026-11-18 | MT0 (DB0.6, DE1) | 0.5 | 0.113 | 0.2 | 0.113 | 8.71 | 3.5 | peak 0.79 A, obs ripple 8.6 deg, steep zero crossing |
| 2026-11-18 | MT0 (DB0.6, DE1) | 0.5 | 0 | 0.3 | - | 4.16 | 6.5 | sweep, obs 11.7 deg |
| 2026-11-18 | MT0 (DB0.6, DE1) | 0.5 | 0.04 | 0.3 | - | 5.71 | 4.5 | sweep, obs 10.8 deg |
| 2026-11-18 | MT0 (DB0.6, DE1) | 0.5 | 0.07 | 0.3 | - | 6.83 | 6.0 | sweep, obs 9.9 deg |
| 2026-11-18 | MT0 (DB0.6, DE1) | 0.5 | 0.113 | 0.3 | 0.113 | 7.81 | 3.0 | sweep, obs 8.6 deg |
| 2026-11-18 | MT2 (2 A) | 2 | 0 | 0.3 | - | 4.55 | 7.5 | INVALID: current PI capped at 0.456 V (bench bug, fixed) |
| 2026-11-18 | MT2 (2 A) | 2 | 0.113 | 0.3 | 0.113 | 11.97 | 8.5 | INVALID: current PI capped (bench bug, fixed) |
| 2026-11-18 | MT2 (2 A, fixed fw) | 2 | 0 | 0.3 | - | 8.07 | 6.0 | peak 1.96 A, obs 5.9 deg, 3 spikes in the averaged wave |
| 2026-11-18 | MT2 (2 A, fixed fw) | 2 | 0.113 | 0.3 | 0.113 | 2.37 | 6.5 | peak 2.02 A, obs 5.2 deg, smooth |
| 2026-11-18 | MT2 (2 A), 4 runs | 2 | 0 | 0.3 | - | 5.55 (sd 1.74) | 6.25 | obs 6.29 deg (sd 0.30) |
| 2026-11-18 | MT2 (2 A), 4 runs | 2 | 0.113 | 0.3 | 0.113 | 2.43 (sd 0.60) | 6.62 | obs 5.39 deg (sd 0.66) |
| 2026-11-18 | MT2 (2 A) sweep | 2 | 0 | 0.3 | - | 4.33 | 7.0 | obs 5.97 deg |
| 2026-11-18 | MT2 (2 A) sweep | 2 | 0.113 | 0.3 | 0.113 | 1.56 | 6.5 | obs 5.16 deg (0.47 x Vbus*dt) |
| 2026-11-18 | MT2 (2 A) sweep | 2 | 0.24 | 0.3 | - | 1.49 | 6.5 | obs 4.90 deg (1 x Vbus*dt, dekutree64 rule) |
| 2026-11-18 | MT2 (2 A) sweep | 2 | 0.48 | 0.3 | - | 2.21 | 6.0 | obs 7.74 deg (2 x Vbus*dt, 2023 forum rule) |
| 2026-11-18 | GD32 calibration DC1.5 | 0 | - | 0.2 | 0.605 (ratio 1.75) | | | GD32F130 slave, 26 V, R est. 0.50 Ohm, min. threshold 1.22 A |
| 2026-11-18 | GD32 MT0 1.2 V | 0.5 | 0 | 0.2 | - | 18.0 | 9.7 | fundamental 0.70 A, wheel turns with jerks |
| 2026-11-18 | GD32 MT0 1.2 V, DE1 | 0.5 | 0.605 | 0.2 | 0.605 | 11.4 | 9.7 | fundamental 0.64 A |
| 2026-11-18 | GD32 MT0 1.8 V | 0.5 | 0 | 0.2 | - | 10.0 | 13.9 | fundamental 2.05 A, 12 s |
| 2026-11-18 | GD32 MT0 1.8 V, DE1 | 0.5 | 0.605 | 0.2 | 0.605 | 2.7 | 8.3 | fundamental 2.02 A, 12 s, fewer jerks |

## 2026-11-18: first power-up

- Supply 12 V, motor connected, motor disabled. No fault.
- Current sense offsets (pyOCD, 2 s): ia +0.02 A, ib -0.03 A, ic -0.02 A, noise ~0.035 A rms.
- `pio test -f test_static`: test_current_offset PASSED, test_compensation_sign PASSED.
- Oscilloscope (Rigol DS1104Z) over USB with pyvisa-py + WinUSB: identity and settings OK, waveform download unreliable (2/20). Use LAN later.
- Calibration `DC1.5` (5 s, rotor locked, motor disabled after): R est. 0.49 Ohm, compensation 0.113 V,
  ratio to Vbus*dead_zone (0.24 V) = 0.47, minimum threshold measured 0.23 A (setting 0.20). No fault.
- Open loop voltage mode 0.5 rad/s (`campaign.py --modes voltage --speeds 0.5 --comps 0 0.1128 --via pyocd`,
  data/2026-11-18_openloop_v0.5): peak current only ~0.4 A (expected 1.8 A with R = 0.17 Ohm; measured R 0.49 Ohm
  plus back-EMF). Zero crossing share 7.5 % -> 6.0 % (pure sine 6.4 %), but THD 4.3 % -> 8.5 % and observer ripple
  4.6 -> 25.7 deg. Noise (~0.035 A) is large compared to 0.4 A: one run each, not conclusive.
  User observation: the wheel did not rotate, it only oscillated (torque too low at 0.4 A for cogging/friction).
  Observer figures meaningless for this run.
- `tools/ol_run.py --voltage 0.6 --speed 0.5 --duration 8`: peak phase current 1.0 A, the wheel turns.
- Comparison at 0.6 V (data/2026-11-18_openloop_v0.5_0.6V): with 0.113 V the zero crossing plateau disappears
  and turns into a step (|i|<10 % share 3.5 %, below the 6.4 % of a pure sine), THD 3.4 % -> 8.7 %, peak
  0.70 -> 0.79 A despite DE1. Looks over-compensated (or ramp threshold 0.2 A too narrow, measured min. 0.23 A).
  Observer ripple 11.7 -> 8.6 deg. One run each.
- Sweep at 0.6 V, threshold 0.3 A (data/2026-11-18_sweep_0.6V_th0.3): THD rises steadily with the compensation
  (4.2 -> 5.7 -> 6.8 -> 7.8 %), the observer ripple falls steadily (11.7 -> 10.8 -> 9.9 -> 8.6 deg). The
  |i|<10 % share is noisy (6.5 / 4.5 / 6.0 / 3.0 %) and no clear plateau is visible even without compensation:
  at ~0.75 A in voltage mode the dead-time distortion is within the measurement noise (4 periods, ~0.035 A).
- Current mode 2 A, 2 rad/s (data/2026-11-18_current_2A_v2): the measured phase current only reaches 0.65 A peak
  (0.93 A with 0.113 V compensation) for a 2 A setpoint. THD 4.6 % -> 12.0 %. To investigate before drawing
  conclusions: iq/id/uq/ud during the run (tools/ol_run.py now prints them), supply current.
  Together with R est. 0.49 Ohm (nominal 0.17) and 0.4 A at 0.3 V (expected 1.8 A), a current sense scaling
  problem is possible.
- Diagnostic `tools/ol_run.py --mode current --speed 2 --duration 6`: iq 0.50 A, id 0.43 A, uq 0.456 V, ud 0.434 V.
  The current PIs were capped at 0.456 V = open loop voltage left by the sweep (0.6 - 4/pi * 0.113).
  Cause: SimpleFOC copies voltage_limit into PID_current_q/d.limit on MC/MT commands, and the bench then only
  wrote motor.voltage_limit = FOC_VOLTAGE_LIMIT. Fixed in bench.cpp (motor.updateVoltageLimit()); the current
  mode results above are invalid. The low currents in voltage mode are consistent with R ~0.49 Ohm once the
  dead-time voltage loss (~0.11 V of 0.3 V) is taken into account: no sign of a current sense scaling error.
- After the voltage limit fix, `ol_run.py --mode current --speed 2`: immediate overcurrent fault (fault 2) on
  enable. Commander query: PID_current_d.P = -23269778 (= -12345 * 2*pi*300). BLDCMotor(pp, R, KV, L) sets only
  Lq, Ld stays NOT_SET and tuneCurrentController() uses it (SimpleFOC issue, not the compensation). Fixed in
  bench.cpp by passing Ld. Was hidden before by the 0.456 V cap. The voltage mode runs and the calibration did
  not show any fault and are not affected by the d current PI.
- Ld fix flashed: PID_current_q/d P 0.695, I 313.7. `ol_run.py --mode current --current 1 --speed 2 --duration 6`:
  iq 0.999 A, id -0.002 A, uq 0.83 V, ud -0.47 V, peak phase current 1.15 A, no fault. Current mode works.
- Current mode 2 A, 2 rad/s with both fixes (data/2026-11-18_current_2A_v2_fixed): THD 8.1 % -> 2.4 % with 0.113 V,
  observer ripple 5.9 -> 5.2 deg, no fault. The uncompensated wave has 3 isolated spikes (~0.3-0.4 A at 40, 222
  and 340 deg) that weigh in its THD; the wheel jerks in open loop. One run each: repeat to confirm.
- 3 more off/on pairs (data/2026-11-18_current_2A_rep1..3). Over the 4 pairs: THD 8.07/5.05/4.99/4.09 % ->
  2.37/3.01/2.71/1.62 % (mean 5.55 -> 2.43 %), observer ripple mean 6.29 -> 5.39 deg; lower with compensation in
  every pair. Zero crossing share ~6.3-6.6 % in both cases (close to a pure sine): in current mode the PI already
  corrects most of the plateau, the compensation removes the remaining harmonics.
- Current mode sweep with the forum rules scaled to this bench (Vbus*dead_zone = 0.24 V,
  data/2026-11-18_current_2A_sweep): THD 4.33 / 1.56 / 1.49 / 2.21 %, observer 5.97 / 5.16 / 4.90 / 7.74 deg for
  0 / 0.113 / 0.24 / 0.48 V. 0.113 and 0.24 V are equivalent within the run-to-run spread (THD sd ~0.6 %);
  0.48 V (2x rule from the 2023 forum posts, other board) over-compensates here, mostly visible on the observer.
- 3 rotated repeats of 0.113 / 0.17 / 0.24 V (data/2026-11-18_current_2A_opt_rep1..3). All current mode 2 A,
  2 rad/s runs of the day pooled:

  | Comp [V] | n | THD mean (sd) [%] | Observer ripple mean (sd) [deg] |
  |---|---|---|---|
  | 0 | 5 | 5.31 (1.60) | 6.22 (0.29) |
  | 0.113 | 8 | 2.31 (0.83) | 5.25 (0.50) |
  | 0.17 | 3 | 3.08 (0.88) | 5.21 (0.76) |
  | 0.24 | 4 | 2.77 (1.45) | 4.58 (0.42) |
  | 0.48 | 1 | 2.21 | 7.74 |

  Any value between 0.113 and 0.24 V clearly beats no compensation; between them the THD differences are within
  the run-to-run spread (open loop jerks). The observer ripple is lowest at 0.24 V (1 x Vbus*dead_zone).

## 2026-11-18: GD32F130 split board (slave TT-SD2.2), env gd32f130

- Master board has a shorted MOSFET (wheel hard to turn unpowered): not used. Slave board was read-protected
  (original firmware): unlocked with OpenOCD (`stm32f1x unlock 0`, flash erased), then flashed.
- Supply 26 V direct (no ideal diode), Vbus measured 25.8 V. Commands over RTT with a ST-Link V2, pyOCD reads.
- Current sense offsets must be measured with the driver enabled (R_ds_on sensing): fixed, 0 V test currents
  -0.03 / -0.04 A mean, noise +-0.8 A. loopFOC 1068 us with the observer, 796 us without (observer off by default).
- Open loop voltage mode 0.6 V, 0.5 rad/s: the wheel did not turn. Static vector, rotor locked: 0.6 V -> no
  current above the noise, 1.2 V (2.1 V between the two driven phases) -> 0.72 / -0.75 A: ~0.67 V lost per phase.
  Phase order and current sense channels checked against the open loop angle (4.24 rad): correct.
- Calibration `DC1.5` (ALIGN_VOLTAGE 1.5 V on this board): R est. 0.50 Ohm, compensation **0.605 V**, ratio to
  Vbus*dead_zone (0.346 V) = 1.75, minimum threshold measured 1.22 A. Close to the 0.7 V found by hand in 2023
  on a GD32 board (SimpleFOC forum).
- Open loop voltage mode, 0.5 rad/s (`ol_run.py --port rtt --env gd32f130`, live pyOCD reads, data/2026-11-18_gd32_voltage):
  - 0.9 V: without compensation the wheel only oscillates, with 0.605 V it turns (user observation).
  - 1.2 V without compensation: the wheel turns with jerks, fundamental current 0.70 A (as predicted from the
    ~0.67 V/phase loss).
  - 1.2 V with 0.605 V and equal fundamental (`--equal`, DE1): fundamental 0.64 A (the calibrated compensation
    nearly matches the loss). Waveforms folded on the open loop angle (~8 periods): THD 18.0 % -> 11.4 %, the
    plateaus at the zero crossings (~0 and ~180 deg) mostly gone. One run each.
  - 1.8 V, 12 s each, script cut 3.5 A: fundamental 2.05 A without, 2.02 A with 0.605 V and equal fundamental.
    THD 10.0 % -> **2.7 %**, |i|<10 % 13.9 % -> 8.3 % (pure sine 6.4 %): without compensation the current is
    trapezoidal (flat tops, kink at the zero crossing), with it close to a sine. Residual noise rms 0.24 -> 0.20 A.
    The user felt fewer jerks with compensation (also at 1.2 V), some remain (cogging, open loop). One run each.
  - 2.4 V: stopped by the script cut (3.8 A) in all four attempts: at start-up without ramp (4.10 / 3.80 A), and
    ~0.3 s after reaching 2.4 V with a 1 s ramp from 1.2 V (3.82 / 4.01 A): ~3.1 A fundamental + noise spikes
    (+-0.8 A) exceed the cut. No board fault. 1.8 V (~2 A) is the practical maximum with the current cuts.
- Current mode (MT2), 1 A, 2 rad/s, 6 s (data/2026-11-18_gd32_current): the PI holds iq 1.000 A, id ~0 with
  the 100 Hz bandwidth and the ~1.26 kHz loop. Wheel turns with many jerks (user).
  - Without compensation: controller voltage uq 1.47 / ud -0.35 V (|u| 1.51 V, ESC1: 0.95 V same conditions),
    THD 17.9 %, |i|<10 % 11.1 %.
  - With 0.605 V (threshold 0.2 A): |u| 0.83 V (the compensation supplies the dead-time voltage the PI had to
    add), THD 21.0 %, |i|<10 % 4.2 %: a step of the current at the zero crossing (~185 deg), the compensation
    switches too abruptly (measured min. threshold 1.22 A). Both waveforms are skewed by the rotor jerks.
- Hall sensors (PB11, PF1, PC14) with the known alignment of the split board sketch (2.09 rad, CCW, no motor
  movement at start-up): 6 states in order 3-1-5-4-6-2, interrupts OK. Open loop check 1.8 V / 0.5 rad/s: sensor
  angle 74.6 deg el. ahead of the open loop angle, spread 17 deg (90 deg minus a ~15 deg load angle): alignment OK.
- Closed loop zero torque test (MC0, MT2, target 0 A, 20 s each, wheel turned by hand up to ~8 rad/s,
  data/2026-11-18_gd32_hall): no fault.
  - no compensation: iq sd 0.244 A, id sd 0.179 A
  - 0.605 V on setpoints (DM0): compensation stays exactly 0 as designed, iq sd 0.202 A, id sd 0.208 A
  - 0.605 V on measured currents (DM1, threshold 1.22 A): compensation sd 0.071 V, iq sd 0.155 A, id sd 0.227 A
  - Repeated with motor "beeps" marking the phases (open loop 1.6 V at 60 rad/s for 0.3 s, N beeps before
    phase N, one long beep at the end), wheel turned up to ~10 rad/s: iq sd 0.246 / 0.237 / 0.211 A,
    id sd 0.195 / 0.189 / 0.177 A for no compensation / DM0 / DM1. No fault.
  - User feel (repeated test): less resistance when turning the wheel with the compensation on measured
    currents (DM1, threshold 1.22 A); no difference expected nor felt with DM0 (0 at zero torque by design).
    Consistent with the lower iq fluctuations in both runs. No vibration caused by DM1 with this threshold.

## 2026-11-18: GD32F130 loop optimisation (env gd32f130, slave board, zero torque)

Loop time with the motor enabled, closed loop torque MC0/MT2, target 0 A, compensation off, wheel at rest
(`loop_us` = SimpleFOC loopfoc_time_us, same script for the three firmwares; the profiler (XP) agrees within 2 %):

| Firmware | Observer off | Observer on |
|---|---|---|
| Before (e26cee3) | 1567 us | 1995 us |
| Optimised (env gd32f130) | **219 us** | **286 us** |
| Optimised + Qfplib-M3 (env gd32f130_qfp, not in the repository) | 202 us | 264 us |

What made the difference, in order of effect:
- **Only the first 32 KB of flash run without wait states** (datasheet: "a long time delay when CPU fetches the
  instructions out of the range"). Measured: the same loop takes 931 cycles at 0x080077A1 and 5106 cycles at
  0x08008DA1 (5.5x). The firmware is ~60 KB and the linker had put libm, micros, the bench and the RTT code above.
  `ld/gd32f130_fastloop.ld` places the loop code first; the link fails if it exceeds 32 KB (`__fast_end`).
- **Constant data read from the slow flash is slow too**: the C++ virtual tables (one read per virtual call:
  sensor, current sense, driver, stream) moved to the fast area: 321 -> 220 us.
- `_sincos` / `_normalizeAngle` (weak SimpleFOC functions) replaced by `fastmath.cpp`: same table and results,
  one float multiply for sin and cos, no `fmodf`.
- FreeRTOS was linked (lib_archive = false) and hooked the SysTick interrupt: `lib_ignore = FreeRTOS for GD32`
  (also removes the jitter: max = mean now, 250 bytes of RAM saved).
- Qfplib-M3 (add/sub/mul/div/sqrt only, through `-Wl,--wrap`): -8 % only, the comparisons, conversions and calls
  of libgcc still dominate. GPL v2: kept out of the repository (`lib/qfplib` ignored), env `gd32f130_qfp` documents it.

**micros() goes backwards on the GD32 core**: up to 930 us, 7 times in 10 s (counted with XP). The core reads the
tick counter and SysTick->VAL without checking a pending SysTick wrap. SimpleFOC uses unsigned differences of
`_micros()`, so the SmoothingSensor then jumps the hall angle by pi/3 electrical for one loop (also in the old
firmware, less often). `gd32_micros.c` (`-Wl,--wrap=getCurrentMicros`) fixes it: 0 backward steps since.
Worth reporting to CommunityGD32Cores (systick.c getCurrentMicros).

Notes:
- The GD32F130 is rated 48 MHz max (datasheet), the Arduino core sets 72 MHz (board F_CPU says 48 MHz). All the
  numbers above are at 72 MHz.
- Flash 91 % (gd32f130), 95 % (gd32f130_qfp). XB micro benchmarks only with `-D BENCH_MICROBENCH` (~3 KB).
- SmoothingSensor compares the velocity with double literals (`0.001`): patched locally only, to fix in the
  drivers fork (no measurable effect at rest).
- Tools: `XP` (loop profile, DWT cycle counter), `tools/pc_sample.py` (statistical profile of the program counter,
  biased: it never sees the slow flash, use it only to rank functions).

## 2026-11-18: GD32F130 loop, library level optimisations (env gd32f130_perf, zero torque)

SimpleFOC from the local clone `C:/Dev/Arduino-FOC-gd32`, branch `perf/gd32-loop` (not pushed):
test/gd32-deadtime + simplefoc `dev` merged + the commits below. SmoothingSensor changes (drivers) in
`patches/simplefocdrivers-smoothing-perf.patch`, applied by hand to `.pio/libdeps/<env>/SimpleFOCDrivers`.
Same measurement as the previous entry (`loop_us`, motor enabled, MC0/MT2, 0 A, wheel at rest). Each step
includes the previous ones:

| Step | Observer off | Observer on | Kept |
|---|---|---|---|
| Previous entry (env gd32f130) | 219 us | 286 us | |
| + simplefoc `dev` merged (d/q decoupling lines commented out on dev) | 209 us | 277 us | yes |
| + PR #529 as is (`__builtin_fmaxf/fminf`, call libm without -ffast-math) | 227 us | 297 us | no |
| + #529 with plain templates (one evaluation per argument) | 209 us | 276 us | no (no gain: GCC already merged the repeated soft float calls) |
| + `_sincos` result reused for the same angle (`BENCH_SINCOS_CACHE`, fastmath.cpp) | 203 us | 270 us | yes |
| + one dt per loopFOC for the 2 current filters and 2 current PIDs (#325) | 189 us | 257 us | yes |
| + LPF alpha cached when dt unchanged | 192.5 us | 259 us | no (dt changes by 1 us between loops) |
| + PID derivative skipped when D = 0 | 188.5 us | 255 us | no (within noise) |
| + `setPwm`: one division instead of 3; SmoothingSensor: 1/pole_pairs once | 185 us | 250 us | yes |
| + `motion_downsample = 3` (move() every 4th loop, torque mode) | 171 us | 238 us | yes (config) |
| + control loop files at -O2 (scripts/o2_hot.py, +2.7 KB flash) | **168 us** | **232 us** | yes |
| Qfplib-M3 on the 171 us version (env gd32f130_perf_qfp, GPL, not in the repo) | **160.5 us** | **221 us** | option |

Qfplib + -O2 together exceed the 32 KB zero wait state area by 900 bytes (the cold functions of the same
files, calibration and alignment, would have to be moved out by section name).

Where the time goes now (gd32f130_perf_qfp, observer off, total 163 us with the profiler):
loopFOC 129 us = current filters + PIDs 42, setPhaseVoltage (SVPWM + setPwm) 40, getFOCCurrents 22,
hall + smoothing sensor update 18, electrical angle 6; move 6 (mean, every 4th loop); bench only: debug
dbg variables 20, safety 4. Without the bench code the loop would be ~140 us (~7 kHz). What is left is
soft float arithmetic and comparisons spread over many small operations: the next step would be fixed point.

Not measured (wheel at rest): HallSensor::getVelocity (3 divisions, returns 0 at rest) and the velocity
dependent paths: the loop will be a bit slower when the wheel turns.

## 2026-11-19: firmware size (build only, board not powered)

Map file analysis (per object) of the bench and of Split_Hoverboard_SimpleFOC (branch perf/gd32-fast-loop):
- `lib_archive = false` constructed and kept every global object of every library, used or not:
  SimpleFOCRegisters (with `new`, so malloc), Wire, Wire1, SPI. `lib_archive = true` drops them. The reason
  for `false` (the GD32 driver functions must replace the weak generic ones of SimpleFOC) is handled with
  `-Wl,-u,adc_val -Wl,-u,sampling_point`, which force the two GD32 driver objects in. Checked in the
  disassembly: ADC, PWM and sync functions are the GD32 ones. GD32 envs only: on the ESC1, "generic_mcu"
  would come before "stm32" in the archive.
- newlib `atof()` is `strtod()`: double precision, hexadecimal, locale, malloc, ~8 KB of flash and ~460 bytes
  of RAM, only for Commander's typed numbers. `small_atof.c` (single precision atof/atoi/atol, GD32 only):
  same float result as strtod for decimal and exponent inputs (checked against a float32 emulation).

| Firmware | Flash before | Flash after | RAM before | RAM after |
|---|---|---|---|---|
| Split_Hoverboard_SimpleFOC GD32F130C8 (hall) | 45.8 KB | **33.7 KB** | 6088 B | **5356 B** |
| Split_Hoverboard_SimpleFOC, SENSORLESS | 45.3 KB | 32.9 KB | 6012 B | 5152 B |
| Bench env gd32f130 | 59.5 KB | **47.1 KB** | 7456 B | **6676 B** |
| Bench env gd32f130_perf | 62.3 KB | 49.7 KB | | 6696 B |
| Bench ESC1 (STM32G431), test env esc1_size | 84.3 KB | **67.5 KB** | 17044 B | **16288 B** |

ESC1: same changes in the test env `esc1_size` (env esc1 unchanged). There "generic_mcu" comes before "stm32"
in the archive, so `-Wl,-u,adcBuffer1 -Wl,-u,_Z12_getPwmStatePv` are required (b_g431_mcu.cpp, stm32_mcu.cpp):
checked in the disassembly, the PWM/ADC functions are the STM32/B-G431 ones (HAL_ADC_Start_DMA, stm32_setPwm...).
Removed there: strtod family, SimpleFOCRegisters, Wire with the ST I2C HAL. Not run on the ESC1 yet.

**Update (same day): no lib_archive at all, the robust version.** SimpleFOC already declares `"libArchive": false`
in its library.json, but any `lib_archive` line in platformio.ini (true or false) overrides it for every library.
Without the line: SimpleFOC (and testbench, now with its own library.json, because fastmath.cpp replaces weak
SimpleFOC functions) are linked as objects, the hardware functions are always the right ones on any MCU, no
`-u` needed; the other libraries are archived, so their unused global objects (SimpleFOCRegisters...) go away.
Wire/SPI stay (~0.9 KB on GD32, ~5 KB with the ST I2C HAL on the ESC1): SimpleFOC's I2C/SPI sensors refer to them.
Rule: a library that replaces weak functions of another one must declare `"libArchive": false` (the sketch
`src/` files are never archived). Exception to keep in mind: the STM32G4 CORDIC trig functions of SimpleFOC
drivers are only linked if the sketch calls `SimpleFOC_CORDIC_Config()` (as their README says).

| Firmware | Original | No lib_archive | + small atof | (forced archives + -u, not kept) |
|---|---|---|---|---|
| Split_Hoverboard_SimpleFOC GD32F130C8 | 45.8 KB / 6088 B | | **34.8 KB / 5548 B** | 33.7 KB |
| Bench gd32f130 | 59.5 KB / 7456 B | | **48.2 KB / 6868 B** | 47.1 KB |
| Bench ESC1 (esc1 / esc1_size) | 84.3 KB / 17044 B | 81.5 KB / 17020 B | **72.5 KB / 16656 B** | 67.5 KB |

Still in the firmware, not removable from the sketch: float printing (`Print::printFloat` in double, ~2 KB of
double soft float), the stepper/hybrid alignment code of CurrentSense (~2 KB, selected at run time),
`characteriseMotor`, the framework timer code kept by the interrupt vectors (~2 KB). Not run on hardware yet.

## 2026-11-19: SmoothingSensor PR measurement, hoverboard board with a GD32F103RC (STM32 core, genericSTM32F103RC; envs smooth_f103_dev / smooth_f103_fix)

Board on the 26 V lab supply (1 A limit, power button bridged, ST-Link 3.3 V not connected), motor never enabled.
Plain SimpleFOC upstream dev (ae7b3ad, `BENCH_NO_DEADTIME`), only the drivers differ: upstream dev (1f67e02) vs
candas-agent fix/smoothing-sensor-float (1 / pole_pairs once per call, 0.001f). The MCU is a GD32F103RC, not an
STM32F103 (IDCODE 0x13090414, Cortex-M3 r2p1 CPUID 0x412FC231). Its flash timing differs from the STM32F103
(GD32 flash has a zero wait state area, size to check in the datasheet; STM32F103: 2 wait states at 64 MHz),
so the absolute cycle counts may differ on a real STM32F103; dev vs fix stays comparable. The generic STM32F103RC variant
runs at **64 MHz** (HSI), not 72. `XB` micro benchmarks, `sensor update` = SmoothingSensor::update() including
the wrapped HallSensor::update() (virtual call through motor.sensor). The motor is stopped, so the velocity is
forced (bench code, then restored) to time the phase correction branch. min = mean (stable).

| Cycles per call | dev | fix | difference |
|---|---|---|---|
| sensor update, v = 0 (no phase correction) | 1309 | 1319 | +10 (+0.8 %) |
| sensor update, v = +1 rad/s | 1590 | 1493 | **-97 (-6.1 %)** |
| sensor update, v = -1 rad/s | 1447 | 1358 | **-89 (-6.2 %)** |

fdiv = 156 cycles, fmul = 46. At standstill dev and fix both do one division (the compiler shares
`_PI_3 / pole_pairs` between the two `_constrain` bounds); fix adds 2 fmul but drops the 2 double comparisons:
about equal. While turning dev divides again for the phase correction: fix saves ~90-100 cycles, ~1.5 us per
call at 64 MHz. Flash: update() 348 -> 332 bytes. Not measured: loop time with the motor running (needs the
power stage).

### Same board, wheel turning by hand, zero torque (MC0, MT2, target 0 A)

First run with `-D SIMPLEFOC_STM32_ADC_INTERRUPT` (from the hoverboard sketch): wheel hard to turn. The measured
currents were frozen (identical values at every read, ADC1 CR1 JEOCIE = 0 after the run while JDR1/JDR2 kept
changing): the F1 HAL `HAL_ADC_IRQHandler` disables the JEOC interrupt when the regular group is software start
and not continuous (stm32f1xx_hal_adc.c:1855), which is the SimpleFOC dev F1 configuration, so the interrupt
buffer kept its first value. The current PI saw 0 A and applied random voltages (Vd up to -1.25 V at standstill),
and the bench overcurrent cut was blind (the 1 A supply limit protected). Flag removed (TIM1 has a repetition
counter, the injected registers are read directly): wheel free, measured iq/id within +-0.2 A (R_ds_on noise),
Uq follows the velocity (~0.2-0.4 V per rad/s, 0.33 expected) but with the opposite sign: the zero angle
2.09 rad / CCW from the sketch probably does not match this wiring, redo `SA` before any torque test.
With the motor disabled the R_ds_on readings show ~-11 A (MOSFETs off), expected.

`XP` loop profile, 6 reads (each = mean of 256 loops) per firmware, wheel turned by hand at ~1-7 rad/s:

| µs | fix | dev |
|---|---|---|
| loopFOC (each read) | 200.0 199.8 197.6 197.4 199.7 198.4 | 200.7 202.0 201.0 197.3 201.6 200.5 |
| loopFOC mean | **198.8** | **200.5** |
| total loop mean | 264.4 | 268.1 |

loopFOC is ~1.7 µs (0.8 %) shorter with the fix, in line with the micro benchmark (~1.5 µs per update while
turning). Indicative only: 6 reads, hand-turned wheel (dev run had moments at standstill), `move` alternates
between ~25 and ~35 µs in both builds.

### Same board (GD32F103RC), control modes, env smooth_f103_dev (2026-11-19, 26 V, 1 A supply limit)

- **Open loop, current mode 2 A** (sent MT0 before MC3 by mistake: the bench forced foc_current): phase A clean sine
  ±2.0-2.2 A, period ~0.4 s = 1.05 rad/s with 15 pole pairs. **Open loop voltage 0.3 V**: ~0.17 A peak, almost
  nothing (dead time + MOSFETs). Open loop current 1 A: iq 0.88-1.06 A, uq 0.8-1.0 V.
- **Static vector** (angle open loop, target 0; at electrical angle 0 the voltage is on β, phase A gets nothing, read
  phase B): 0.5 V -> 0.42 A, 0.7 V -> 0.89 A, 0.9 V -> 1.54 A, i.e. ~0.35 V lost then ~3.2 A/V. `ALIGN_VOLTAGE`
  1.5 V would have pushed ~3.7 A: set to **0.8 V** in board_stm32f103_split.h.
- **SA** (automatic alignment): CCW, zero electric angle 2.0944 (= 2π/3, hall sector boundary), same as the sketch
  (2.09 / CCW). "PP check: fail - est. pp: 18" (hall estimate, typical ±20 %, the motor has 15). My earlier
  "Uq sign opposite" remark was wrong: velocity and Uq were read 0.5 s apart while the wheel went back and forth.
- **Closed loop torque**: ±0.3 A: iq follows (±0.31 mean) but the wheel does not move (static friction/cogging).
  ±0.8 A: +12.7 / -12.8 rad/s, reverses in < 0.4 s, uq saturates at FOC_VOLTAGE_LIMIT 4 V (back-EMF) so iq drops
  to ~0.45 A. Stops in ~0.6 s at 0 A.
- **Closed loop velocity** (velocity sampled at ~250 Hz from dbg with pyOCD, stats after 1 s, 3 s per target):

| P | I | Tf [s] | 2 rad/s: mean / sd / time stopped | 5 rad/s: mean / sd |
|---|---|---|---|---|
| 0.5 | 10 | 0.005 (SimpleFOC default, 0.2 s reads) | 0 to 4.3 | 4.7 to 5.3 |
| 0.6 | 5 | 0.05 (old sketch, voltage mode) | 0.3 to 3.7 | 3.3 to 6.7 (oscillates ~1.2 Hz) |
| 0.3 | 5 | 0.005 | 2.04 / 1.35 / 15 % | 5.00 / 0.15 |
| 0.6 | 5 | 0.005 | 2.00 / 1.19 / 12 % | 5.00 / 0.16 |
| 1.0 | 5 | 0.005 | 1.97 / 1.06 / 11 % | 5.00 / 0.20 |
| 0.3 | 5 | 0.01 | 2.09 / 1.53 / 19 % | 5.00 / 0.10 |
| 0.6 | 5 | 0.01 | 1.99 / 1.39 / 21 % | 5.00 / 0.12 |
| 1.0 | 5 | 0.01 | 1.97 / 1.14 / 7 % | 5.00 / 0.13 |
| 0.3 | 5 | 0.02 | 2.05 / 1.57 / 21 % | 5.00 / 0.12 |
| 0.6 | 5 | 0.02 | 1.94 / 1.37 / 16 % | **5.00 / 0.07** |
| 1.0 | 5 | 0.02 | 2.03 / 1.11 / 3 % | 5.00 / 0.16 |
| 1.0 | 2 | 0.005 | 1.98 / 1.14 / 12 % | 5.01 / 0.20 |
| 1.0 | 10 | 0.005 | 1.96 / 1.18 / 12 % | 5.00 / 0.19 |
| 1.0 | 20 | 0.005 | 1.98 / 1.44 / 21 % | 5.00 / 0.37 |

  5 rad/s is fine with every set. 2 rad/s stays stick-slip with every set (sd > 1 rad/s, stops): at 2 rad/s the halls
  give only ~29 edges/s (6 per electrical turn, 15 pole pairs), one velocity update every ~35 ms, plus strong
  friction/cogging; higher P helps a little (sd 1.06-1.11) at the cost of iq noise (sd 0.8-0.9 A). Gains alone do
  not fix low speed; needs a better velocity estimate (higher resolution sensor, observer) or friction feed-forward.
- **Closed loop angle** (MC2, velocity PI P 1.0 I 5 Tf 0.01, P_angle default, velocity_limit 10; target set to the
  current shaft angle before enabling): +3 rad and back. Peak velocity 12.1 rad/s (iq up to ±1.8 A), at the target in
  < 1 s, final error +0.002 / 0.000 rad as seen by the sensor (hall resolution 2π/90 = 0.07 rad mechanical, so the real
  accuracy is about ±0.035 rad). Holding current 0.1-0.5 A at standstill. No fault.

## 2026-11-19: SIMPLEFOC_STM32_ADC_INTERRUPT on a genuine STM32F103 (same hoverboard board model)

Chip: STM32F103 high density, IDCODE 0x10036414 (rev 0x1003), CPUID 0x411FC231 (Cortex-M3 r1p1), 256 KB flash,
read protection off. 26 V supply, motor disabled for the whole test (the hall input reads state 7 = not connected).
Same SimpleFOC dev (ae7b3ad) and drivers dev, env `f103_adc_it` (= smooth_f103_dev + `-D SIMPLEFOC_STM32_ADC_INTERRUPT`)
vs `smooth_f103_dev` (no flag). Phase currents read 3 times (XB "pc" line, `currentSense.getPhaseCurrents()`):

| Build | ADC1 CR1 (JEOCIE bit 7) | JDR1 / JDR2 | Phase currents A / B, 3 reads |
|---|---|---|---|
| with the flag | 0x100 (JEOCIE = 0) | changing (0x906, 0x8FC / 0x8F4, 0x8F2) | -0.039548 / -0.041217, **identical 3 times** |
| without | 0x100 | changing | -11.50/-11.56, -11.42/-11.40, -11.71/-11.64 (MOSFETs off, R_ds_on reading meaningless, expected) |

Same as on the GD32F103: with the flag the interrupt buffer keeps the value of the first conversion (taken during the
offset calibration, so it reads ~0 A forever). Cause: the F1 HAL `HAL_ADC_IRQHandler` disables the JEOC interrupt after
the first injected conversion when the regular group is software start and not continuous
(stm32f1xx_hal_adc.c:1855), which is the SimpleFOC dev F1 ADC configuration. Not GD32 specific.

Root cause found in the SimpleFOC history: the F1 ADC regular group used `ContinuousConvMode = ENABLE` since the
flag was added (5c724d9, 2023-11, Candas1) up to 2025-12, so the HAL condition was false and the JEOC interrupt
stayed enabled (the user tested it then). Commit 68120bf (2026-01-07, "inital support for reading regular channels")
changed `_adc_init` in stm32f1_hal.cpp to `ContinuousConvMode = DISABLE` (with a new `_adc_init_regular`), which makes
`HAL_ADC_IRQHandler` disable JEOC after the first injected conversion: regression on F1 with SIMPLEFOC_STM32_ADC_INTERRUPT.

### Fix candidate tested on the genuine STM32F103 (env f103_adc_it_fix, local branch fix/f1-adc-interrupt from simplefoc dev ae7b3ad)

One line in stm32f1_mcu.cpp `HAL_ADCEx_InjectedConvCpltCallback`: `__HAL_ADC_ENABLE_IT(AdcHandle, ADC_IT_JEOC);` after
`_handleInjectedConvCpltCallback` (the F1 HAL calls the callback right after disabling JEOC, stm32f1xx_hal_adc.c:1858-1868).
- Motor disabled: ADC1 CR1 = 0x180 (JEOCIE = 1), phase currents change between reads (-11.7/-11.7, -11.2/-11.1,
  -11.4/-11.4, MOSFETs off as without the flag).
- Zero torque, wheel turned by hand ~20 s (7913 dbg reads): JEOCIE stays 1, iq mean +0.002 A sd 0.144 A, 4172 distinct
  iq values (not frozen), |vel| up to 6.2 rad/s, Uq follows the velocity with the same sign, ~0.30 V per rad/s
  (0.33 expected from KV 17), read in the same dbg sample. No fault.
Other families (reading the installed HAL of the STM32 core 2.12, not tested on hardware): F4/F7 only disable JEOC when
the injected group is also software triggered, G4/L4/H7 only when the injected group is software triggered; SimpleFOC
triggers injected conversions from the timer TRGO in every family, so only F1 is affected.
Full build time of a new env on this PC: 478 s (Defender exclusions added by the user did not change it).

### Motor running on the genuine STM32F103: interrupt mode (flag + fix) vs registers read directly (no flag)

Same board, same firmware apart from the flag, wheel free, torque ±0.8 A (4 s each) then velocity 5 rad/s (5 s, P 1.0
I 5 Tf 0.01), `XP` = mean of 256 loops:

| µs | no flag (injected registers read directly) | flag + fix (JEOC interrupt every PWM period) |
|---|---|---|
| loopFOC, torque | 291-295 | 326-331 (+35, +12 %) |
| move, torque | 54-55 | 60-61 |
| total loop, torque | 397-402 | 442-450 (+46, +11.5 %) |
| total loop, velocity | 424-428 | 476-478 |

Torque identical in both modes (±12.7 rad/s). The interrupt (HAL_ADC_IRQHandler + SimpleFOC callback, 16 kHz) costs
~11 % of the CPU on this F103 at 64 MHz: reading the injected registers directly when the timer has a repetition
counter (the automatic choice) is clearly better; the flag is only for forcing the interrupt. Same firmware without
the flag on the GD32F103: loopFOC ~199 µs vs 293 µs on the STM32F103 (flash wait states). At 5 rad/s the velocity
oscillates on the STM32 in both modes (sd 2.2-2.4 rad/s vs 0.13 on the GD32): the PI tuned on the faster GD32 loop
does not suit the slower STM32 loop, unrelated to the interrupt.

**Run time bus voltage** (`_readRegularADCVoltage(PC2)` every 20 ms, the feature added by simplefoc 68120bf; PC2 =
DCLINK of EFeru defines.h): works in both modes while the motor runs, **0 errors** (interrupt mode: 2837 reads,
no flag: 6682 reads), no visible effect on the current control (same torque) and ~1-2 µs mean loop cost. Read
29.2-29.5 V stopped, 29.6-30.0 V running, for 26.0 V on the supply display with the gain 33 of EFeru's default
calibration: gain set to 29.3 for this board.

## 2026-11-20: hall sensor calibration (tools/hall_calib.py), genuine STM32F103 board, motor remounted

Reassembly check first (open loop 1 A: phases ±1.4/±1.3 A centred, hall sequence 4-5-1-3-2-6 clean, 85 transitions;
torque ±0.8 A: +12.65/-12.64 rad/s; Vbus 26.3 V, 0 errors): same as before the drop.

Calibration: angle open loop + current control (MC4 + MT2), 1.5 A, 4 electrical turns per direction at 6 s per
turn, separate enables, PC reading dbg with pyOCD. Transitions saved in data/hall_calib_stm32f103.csv. No fault.
Forward sequence 2-6-4-5-1-3 = sectors 4-3-2-1-0-5 (decreasing: consistent with sensor_direction CCW). All three
sensors toggle.

| Edge | forward | backward | fwd-bwd | mean | spread fwd/bwd | spacing | error vs 60° grid |
|---|---|---|---|---|---|---|---|
| 2-6 | 37.7 | 16.2 | +21.5 | 26.9 | 2.5 / 2.8 | 65.9 | -1.1 |
| 4-6 | 103.4 | 82.2 | +21.2 | 92.8 | 1.1 / 2.8 | 57.5 | +4.8 |
| 4-5 | 162.5 | 138.0 | +24.4 | 150.3 | 1.7 / 2.0 | 58.6 | +2.2 |
| 1-5 | 219.5 | 198.2 | +21.3 | 208.8 | 1.5 / 1.8 | 57.5 | +0.8 |
| 1-3 | 276.5 | 256.2 | +20.3 | 266.3 | 0.5 / 1.3 | 56.6 | -1.7 |
| 2-3 | 330.8 | 315.0 | +15.8 | 322.9 | 0.7 / 0.9 | 64.0 | -5.1 |

(degrees electrical, rotor angle = open loop angle + 90°)

| State | centre from samples (fwd / bwd / mean) | centre from edges | width |
|---|---|---|---|
| 1 | 248.2 / 227.2 / 237.7 | 237.6 | 57.5 |
| 2 | 4.4 / 345.9 / 355.2 | 354.9 | 64.0 |
| 3 | 303.8 / 285.2 / 294.5 | 294.6 | 56.6 |
| 4 | 133.1 / 110.1 / 121.6 | 121.5 | 57.5 |
| 5 | 191.0 / 168.0 / 179.5 | 179.5 | 58.6 |
| 6 | 70.4 / 49.4 / 59.9 | 59.9 | 65.9 |

- Edges within -5.1 / +4.8° electrical of an ideal 60° grid (0.34° mechanical); sector widths 56.6 to 65.9°.
- Both centre methods agree within 0.3°: no visible distortion of the edges by cogging at this resolution.
- fwd-bwd ~21° (16-24): rotor lag behind the field (friction, cogging at 1.5 A) + hall hysteresis, removed by the mean.
- Repeatability over the 4 turns (different pole pairs) 0.5-2.8°: magnet-to-magnet differences stay small.
- Centres give an equivalent zero electric angle of ~120-125° (mean ~122°) if the SimpleFOC sector angle is taken as
  the centre (direction CCW): close to the 2.0944 rad (120°) found by SA. Convention to confirm before using it.

### Hall table in motion (lib/testbench/src/hall_table.*, command ST0/ST1/ST2), same board

HallTableSensor (bench only): rotor angle from the measured edges (entry edge + velocity * time since the transition,
limited to the measured sector width; middle of the sector at standstill), rotor frame (CW, zero 0). Standstill check
in state 3: SmoothingSensor 300.3°, measured table 294.6° (centre), ideal grid 298.0°: conventions consistent.
Velocity PI P 0.6 I 5 Tf 0.02 for all, stats after 1 s:

| Mode | torque +0.8 A | 2 rad/s: mean / sd / stopped | 5 rad/s: mean / sd | iq sd at 2 / 5 rad/s |
|---|---|---|---|---|
| SmoothingSensor (today's code) | +12.65 rad/s | 2.00 / 2.26 / 17 % | 5.06 / 2.78 | 1.05 / 1.10 A |
| same class, ideal 60° grid | +12.72 | 1.98 / 2.54 / 12 % | 5.09 / 2.70 | 1.15 / 1.10 |
| same class, measured table | +12.78 | 2.07 / 2.39 / 16 % | 5.02 / 2.73 | 1.09 / 1.09 |

No measurable difference: the ±5° edge errors are hidden by the velocity loop oscillation of this slower STM32 loop
(sd 2.3-2.8 rad/s in every mode, vs 0.07-0.2 on the GD32 with similar gains) and by the hall velocity estimate.
The torque mode top speed is +1 % with the table but within the run-to-run spread seen before (12.64-12.78).
Conclusion: the hall table is not worth a library change for this motor for now; tune the STM32 velocity loop first,
then a finer metric (id ripple at constant speed in torque mode) would be needed to see the table.

### Velocity PI sweep for the STM32F103 loop (same sweep script, 3 s per target, stats after 1 s)

| P | I | Tf [s] | 2 rad/s: sd / stopped | 5 rad/s: mean / sd | iq sd at 5 rad/s |
|---|---|---|---|---|---|
| 0.2 | 5 | 0.01 | 1.93 / 39 % | 4.97 / 3.58 | 0.80 |
| 0.4 | 5 | 0.01 | 1.75 / 29 % | 5.00 / 0.15 | 0.15 |
| 0.7 | 5 | 0.01 | 2.21 / 33 % | 5.04 / 1.96 | 0.96 |
| 0.2 | 5 | 0.02 | 4.66 / 2 % | 4.83 / 4.50 | 1.08 |
| 0.4 | 5 | 0.02 | 1.80 / 27 % | 4.96 / 3.09 | 0.97 |
| 0.7 | 5 | 0.02 | 2.75 / 7 % | 5.01 / 2.90 | 1.19 |
| 0.2 | 5 | 0.04 | 5.95 / 2 % | 4.14 / 5.36 | 1.28 |
| 0.4 | 5 | 0.04 | 3.49 / 4 % | 5.18 / 3.44 | 1.17 |
| 0.7 | 5 | 0.04 | 2.82 / 5 % | 5.06 / 2.89 | 1.30 |
| 0.4 | 1 | 0.01 | **1.33 / 18 %** | **5.01 / 0.12** | **0.14** |
| 0.4 | 2 | 0.01 | 1.45 / 21 % | 5.00 / 0.11 | 0.14 |
| 0.4 | 10 | 0.01 | 3.70 / 13 % | 5.00 / 3.65 | 1.18 |

On the slower STM32 loop (~400 µs) a longer velocity filter or a higher I makes it oscillate (sd 3-6 rad/s,
even reversing); best **P 0.4, I 1-2, Tf 0.01**: 5 rad/s sd 0.11-0.12 rad/s, iq sd 0.14 A. 2 rad/s stays stick-slip
(sd 1.3, 18 % stopped): hall velocity + friction, as on the GD32. The previous hall table comparison (P 0.6 I 5
Tf 0.02) was done in an oscillating loop and is not conclusive.

### Hall table comparison redone with the tuned PI (P 0.4, I 1, Tf 0.01)

| Mode | torque +0.8 A | 2 rad/s: sd / stopped | 5 rad/s: mean / sd | iq sd 2 / 5 | id mean / sd at 5 rad/s |
|---|---|---|---|---|---|
| SmoothingSensor | +12.67 | 1.32 / 16 % | 5.01 / 0.11 | 0.42 / 0.14 | -0.002 / 0.14 |
| ideal 60° grid | +12.72 | 1.26 / 15 % | 5.00 / 0.12 | 0.41 / 0.14 | 0.000 / 0.14 |
| measured table | +12.79 | 1.33 / 17 % | 5.01 / 0.12 | 0.42 / 0.15 | -0.003 / 0.14 |

With a stable velocity loop the three modes are still identical within the noise (velocity, iq and id ripple).
Only the torque mode top speed (voltage limited) is in the same order in both runs: SmoothingSensor < ideal grid <
table, +0.9 %: a small but repeatable effect of a more exact angle at the voltage limit. On this motor (hall edges
within ±5°) the table brings nothing measurable for low speed stability; not worth a library change for now.

### Low speed (2 rad/s) improvement attempts, tuned PI (P 0.4, I 1, Tf 0.01), same board

1. **Friction feed-forward** (bench command XF, `feed_forward_current.q` = XF × sign of the velocity target, linear
   below 0.5 rad/s): 0 / 0.2 / 0.3 A give the same result (2 rad/s sd 1.30-1.35, 14-18 % stopped). The PI integrator
   just carries less; friction does not drive the stick-slip cycle.
2. **Velocity estimate** (bench HallTableSensor, own velocity, modes ST3 ideal grid / ST4 table): edge to edge velocity
   = measured width of the sector just left / time spent in it, never above current sector width / time since the
   last edge (decays instead of holding then dropping to 0). Independent metric: hall edge intervals seen by the PC
   (~4 ms resolution), count of intervals > 100 ms (~3x nominal at 2 rad/s = real stall).

| Mode | 2 rad/s: vel sd / iq sd / edge interval cv / >100 ms / max | -2 rad/s: cv / >100 ms | 5 rad/s: vel sd / iq sd |
|---|---|---|---|
| SmoothingSensor (HallSensor velocity) | 1.18 / 0.38 / 80 % / 7 / 134 ms | 88 % / 9 | 0.13 / 0.14 |
| ideal grid + own velocity, 1st version (last sector only) | 1.09 / 0.35 / - | - | 0.26 / 0.16 |
| table + own velocity, 1st version | 0.98 / 0.33 / - | - | 0.57 / 0.21 |
| ideal grid + blended (last sector < 2 rad/s, 6 sector average > 4) | **0.36 / 0.17 / 27 % / 0 / 63 ms** | 60 % / 2 | 2.00 / 0.61 |
| table + blended | **0.41 / 0.17 / 37 % / 0 / 86 ms** | 57 % / 2 | 1.87 / 0.57 |

At 2 rad/s the new estimate removes the stalls (independent hall metric: 7 → 0 intervals > 100 ms, cv 80 → 27-37 %)
and halves the current ripple. Backward less improved (9 → 2 stalls). At 5 rad/s the 6 sector average (one electrical
turn, ~80 ms) adds too much delay: the loop oscillates (sd 1.9-2.0). Next: keep the decaying last sector velocity at
low speed and blend towards HallSensor's velocity (sd 0.13 at 5 rad/s) instead of the 6 sector average.

### Low speed campaign (tools/lowspeed_campaign.py, data/lowspeed_campaign.csv), 2 passes, PI P 0.4 I 1 Tf 0.01

2 and -2 rad/s for 10 s, 5 rad/s for 8 s (2 s settling excluded), each a separate enable, second pass in reverse
order. Hall edges counted by the PC (expected 229 at 2 rad/s, 430 at 5 rad/s); stalls = edge intervals > 3x nominal.
Mean of the 2 passes (± = difference between passes):

| Config | 2 rad/s: edges / cv / stalls / iq sd | -2 rad/s: cv / stalls / iq sd | 5 rad/s: cv / iq sd |
|---|---|---|---|
| C0 SmoothingSensor | 216 / 82 % / 16.5 ±3 / 0.40 | 92 % / 24.0 ±6 / 0.42 | 44 % / 0.15 |
| V0 grid + HallSensor velocity (control) | 215 / 86 % / 20.0 ±6 / 0.41 | 87 % / 18.0 ±2 / 0.41 | 47 % / 0.15 |
| V1 HallSensor + decay | 218 / 66 % ±27 / 9.0 ±16 / 0.28 ±0.07 | 73 % ±23 / 11.5 ±17 / 0.30 | 50 % / 0.16 |
| V2 last sector (measured width) + decay | 217 / 75 % / 10.5 ±5 / 0.31 | 75 % / 12.0 ±4 / 0.31 | 48 % / 0.20 |
| V3 6 sectors + decay | 250 / 96 % / 16.0 ±2 / 0.33 | 97 % / 13.5 ±1 / 0.34 | 73 % / 0.51 |
| V4 V3 low / HallSensor high | 244 / 88 % / 16.0 ±4 / 0.30 | 87 % / 14.5 ±1 / 0.30 | 47 % / 0.15 |
| R20 position ramp, P_angle 20 | 320 / 83 % / 1.0 / 1.17 | 84 % / 3.0 / 1.16 | 85 % / 0.99 |
| R5 position ramp, P_angle 5 | 251 / 88 % / 10.0 / 0.59 | 91 % / 15.0 / 0.59 | 45 % / 0.15 |

- V0 = C0: the test class itself changes nothing (good control).
- More edges than expected (V3/V4 ~245, R5 251, R20 ~320 for 229) means back-and-forth motion (edges counted in both
  directions), not a faster wheel: the position ramps vibrate (iq sd 0.6-1.2 A), R20 also at 5 rad/s.
- The real mean speed in velocity mode is ~6 % low (216/229 edges) while the estimate reads 2.00: the HallSensor
  velocity overestimates around stalls.
- Best at 2 rad/s: V2 (consistent: stalls -40 to -50 %, iq ripple -25 %, no extra edges; 5 rad/s iq 0.20 vs 0.15)
  and V1 (best single run, cv 52 %, 1 stall, but very different between passes). V4 keeps 5 rad/s intact with
  -25 % iq ripple at 2 rad/s but shows extra edges.
- The edge count cannot tell forward from backward edges: a signed count (direction from the hall sequence) would
  separate jitter from real motion. To add before deciding.

### Low speed campaign 2: signed hall edges, 3 passes (data/lowspeed_campaign2.csv)

Edges classified forward/backward from the hall sequence (2-6-4-5-1-3), stalls and cv on the edges in the direction
of motion only, true speed = net edges × 4° mechanical / 8 s. Mean (min-max) over 3 passes:

| Config | 2 rad/s: true speed / stalls / cv / iq sd | -2 rad/s: true speed / stalls / iq sd | 5 rad/s: true speed / iq sd |
|---|---|---|---|
| C0 SmoothingSensor (HallSensor velocity) | 1.85 / 24 (21-26) / 89 % / 0.42 | 1.84 / 23 (17-27) / 0.41 | 4.99 / 0.14 |
| V1 HallSensor velocity + decay | 1.91 / **4 (0-10)** / 59 % / **0.27** | 1.86 / 12 (5-19) / 0.32 | 5.02 / 0.16 |
| V2 last sector (measured width) + decay | 1.90 / **3.3 (2-5)** / 66 % / 0.29 | 1.90 / **9 (2-14)** / **0.28** | 5.08 / 0.22 |
| V4 6 sectors + decay low, HallSensor high | 2.10 / 15 (12-17) / 88 % / 0.30 | 2.13 / 19 (18-20) / 0.31 | 5.00 / 0.15 |

- Almost no backward edges (0-3 per run): the extra edges of V4 in campaign 1 were a real overspeed (+5 %), not jitter.
- Speed accuracy at 2 rad/s: HallSensor velocity -7.5 %, V1/V2 -5 %, V4 +5 % (the 6 sector average with decay
  underestimates the velocity, the PI pushes faster).
- V1 and V2 cut the stalls by 50-85 % (forward) and 45-60 % (backward) and the current ripple by ~30 %. At 5 rad/s
  V1 is neutral (iq sd 0.16 vs 0.14), V2 is worse (0.22).
- Over both campaigns V1 varies more between runs (forward stalls 17, 1, 10, 2, 0) than V2 (13, 8, 3, 5, 2).

Summary of all low speed attempts on this motor (hall only, STM32F103, tuned PI P 0.4 I 1 Tf 0.01):
hall table (angle): no effect; friction feed-forward: no effect; position ramp: vibrates; 6 sector average: worse at
speed; **decaying velocity between hall edges (V1)**: clear gain at low speed, neutral at 5 rad/s, smallest change
(only HallSensor::getVelocity); measured sector widths on top (V2): a bit more regular backward, worse at 5 rad/s.

### A/B: SimpleFOC dev vs candas-agent fix/hall-velocity-decay (b691018), same board and protocol

Library HallSensor::getVelocity() changed (no test class): SmoothingSensor + HallSensor, PI P 0.4 I 1 Tf 0.01,
3 passes (data/hallvel_ab_branch.csv, data/hallvel_ab_dev.csv). Branch run first, then dev reflashed.

| | 2 rad/s: stalls / cv / iq sd / true speed | -2 rad/s: stalls / iq sd | 5 rad/s: cv / iq sd / true speed |
|---|---|---|---|
| SimpleFOC dev | 28.3 (25-31) / 93 % / 0.43 / 1.82 | 22.0 (21-23) / 0.43 | 36 % / 0.14 / 4.99 |
| fix/hall-velocity-decay | **10.3 (6-14) / 82 % / 0.29** / 1.83 | **5.3 (2-9) / 0.26** | 38 % / 0.14 / 5.00 |

Stalls -64 % forward, -76 % backward; current ripple -33 / -40 %; 5 rad/s identical. The real mean speed stays
~8 % low at 2 rad/s in both (not addressed by this change). One dev run at -2 rad/s shows a 1.37 s gap with 8 skipped
hall states and only 103 net edges while the estimated velocity and iq stayed normal: most likely a pyOCD read
stall on the PC, not the wheel (its stall count, 21, is in line with the other dev runs).

## 2026-11-20 - Dead-time compensation on the STM32F103 board (alone, on SimpleFOC dev)

Firmware `dt_f103`: candas-agent `feat/deadtime-compensation-dev` (d0f4aa3) = SimpleFOC dev ae7b3ad + the compensation
commit only (52c1205, cherry-picked: the original `feat/deadtime-compensation` was based on master b3d8f39, 56 commits
behind dev). Plain HallSensor velocity of dev (no decay), drivers dev, 16 kHz, dead_zone 1 %, 26 V.

**Calibration** (`DC1.5`, twice): 0.382 / 0.375 V, ratio 1.47 / 1.44 to Vbus x dead_zone (0.26 V), R 0.57 Ohm.
Effective dead time ~1.45 x the programmed one (GD32F130 board: 1.75, ESC1: 0.47).

**Low speed A/B** in the same firmware, compensation 0 vs 0.38 V switched at run time (`D<volts>`, setpoint source
DM0, threshold 0.2 A), `lowspeed_campaign.py --dt 0,0.38`, 3 passes with alternating order, PI P 0.4 I 1 Tf 0.01
(data/2026-11-20/dt_f103_ab.csv):

| | 2 rad/s: stalls / cv / iq sd / true speed | -2 rad/s: stalls / cv / iq sd | 5 rad/s: cv / iq sd / true speed |
|---|---|---|---|
| compensation 0 | 19.0 (19-19) / 86 % / 0.42 / 1.87 | 22.3 (21-25) / 89 % / 0.42 | 40 % / 0.14 / 5.00 |
| compensation 0.38 V | **1.0 (0-3) / 46 % / 0.26** / 1.87 | **2.0 (2-2) / 47 % / 0.25** | 39 % / 0.16 / 4.99 |

- Stalls -95 % forward, -91 % backward; hall interval cv halved; current ripple -38 / -40 %. Very consistent between
  passes. At 5 rad/s neutral (iq sd +0.02).
- Clearly stronger than the hall velocity decay alone on the same board (stalls -64 / -76 %, cv 93 -> 82 %): most of
  the low speed jerkiness was the dead-time voltage error, not the velocity estimate.
- The real speed stays ~7 % low at 2 rad/s with or without compensation: not caused by the dead time.
- Not done yet: compensation + hall velocity decay together (test env only, the branches stay separate), source
  from the measured currents (DM1), threshold sweep.

### Combined: dead-time compensation + hall velocity decay (test only, branches stay separate)

Firmware `combo_f103`: local branch test/combo-dt-hallvel = feat/deadtime-compensation-dev (d0f4aa3) +
fix/hall-velocity-decay (b691018, cherry-picked cleanly as 7074ac1), not pushed. Same protocol, compensation 0 vs
0.38 V at run time, 3 passes (data/2026-11-20/combo_f103_ab.csv):

| | 2 rad/s: stalls / cv / iq sd / true speed | -2 rad/s: stalls / cv / iq sd / true speed | 5 rad/s: cv / iq sd |
|---|---|---|---|
| decay only (comp 0) | 13.7 (7-23) / 77 % / 0.32 / 1.84 | 3.0 (1-6) / 58 % / 0.26 / 1.89 | 39 % / 0.14 |
| decay + comp 0.38 V | **0 (0-0) / 24 % / 0.17 / 1.98** | **0 (0-0) / 24 % / 0.17 / 1.98** | 39 % / 0.16 |

Overview at 2 rad/s forward (stalls / cv / iq sd / true speed), all on this board and protocol:
dev 19-28 / 86-93 % / 0.42 / 1.82-1.87; decay 10-14 / 77-82 % / 0.29-0.32 / 1.83-1.84;
compensation 1 / 46 % / 0.26 / 1.87; **both 0 / 24 % / 0.17 / 1.98**.

- The two changes are complementary: no stall at all in 6 runs, hall interval cv divided by 3.5 vs dev, current
  ripple -60 %, identical in both directions and between passes.
- **The real speed error is gone** (1.98 instead of ~1.85 at +-2 rad/s): the ~7-8 % deficit only disappears with
  both changes; each one alone leaves it. 5 rad/s unchanged.
- The "decay only" row is noisier than in the earlier A/B (stalls 7-23 vs 6-14).

### Slower: +-1 and +-0.5 rad/s, decay only vs decay + compensation (combo_f103)

Same firmware and PI, `--targets 1:15,-1:15,0.5:20,-0.5:20`, 3 passes (data/2026-11-20/combo_f103_slow.csv).
Nominal sector time 70 ms at 1 rad/s, 140 ms at 0.5 rad/s (stall = interval > 3x nominal: 209 / 419 ms).

| | +1: stalls / cv / iq sd / true speed | -1: stalls / cv / true speed | +0.5: back / cv / true speed | -0.5: back / cv / true speed |
|---|---|---|---|---|
| decay only | 11.7 (10-13) / 89 % / 0.26 / 0.87 | 11.7 (9-15) / 87 % / 0.86 | 10 / 84 % / 0.49 | 10 / 86 % / 0.48 |
| decay + comp 0.38 V | 11.7 (10-13) / 85 % / 0.27 / 0.86 | 8.3 (4-12) / 79 % / 0.85 | 11 / 82 % / 0.49 | 13 / 84 % / 0.48 |

- **Below 2 rad/s the compensation no longer helps**: at 1 rad/s ~12 stalls per 13 s in both cases, hall interval
  cv ~85 %, real speed -14 %. At 2 rad/s the same firmware had 0 stalls and cv 24 %.
- The estimated velocity has the right mean (1.00) but sd ~0.68 rad/s: the velocity loop limit-cycles on the
  sector-quantised estimate. Likely cause of the real speed deficit: with irregular sectors, the value of a short
  sector is held during the following long one, so the time-averaged estimate overestimates the speed and the PI
  settles below the target (consistent with the deficit vanishing when the cv fell to 24 % at 2 rad/s).
- At 0.5 rad/s the mean speed is right (0.49) but the motion is irregular too (cv ~84 %) with ~10 back-and-forth
  hall edges per run (rotor rocking on an edge). The stall metric is meaningless there (419 ms > the 200 ms
  velocity timeout).
- Ideas, not tried: lower compensation threshold (iq is small at these speeds, the ramp may only apply part of the
  compensation), lower velocity PI gains or a longer velocity filter below 2 rad/s, velocity estimate from the
  SmoothingSensor angle over a fixed window.

### Compensation threshold sweep at +-1 rad/s (combo_f103)

`--dt 0,0.38@0.2,0.38@0.1,0.38@0.05,0.38@0.02 --targets 1:15,-1:15` (an entry `<volts>@<threshold>` now sets DT for
each configuration), data/2026-11-20/combo_f103_th_sweep.csv. **Stopped during pass 2 by the firmware run timeout**
(fault 1, 30 s since ME1, phase currents 0.2 A): a 15 s run lasted more than 30 s on the PC side, most likely a pyOCD
read stall; the safety did its job. So 1 pass for 0 and 0.02 A, 2 passes for the others (mean, min-max):

| threshold | +1: stalls / cv / iq sd | -1: stalls / cv / iq sd |
|---|---|---|
| comp 0 | 12 / 88 % / 0.26 | 19 / 95 % / 0.27 |
| 0.2 A | 11.5 (11-12) / 86 % / 0.27 | 8.5 (8-9) / 78 % / 0.24 |
| 0.1 A | 9.0 (9-9) / 81 % / 0.27 | 8.0 (8-8) / 78 % / 0.25 |
| 0.05 A | 15.5 (15-16) / 87 % / 0.28 | 5.0 / 75 % / 0.24 |
| 0.02 A | 12 / 89 % / 0.29 | 10 / 82 % / 0.26 |

- No threshold fixes 1 rad/s: stalls 5-16 for all values, no trend, cv 75-89 %; real speed 0.85-0.88 everywhere.
  The partial compensation at low iq is not the cause. Backward the compensation helps somewhat (19 -> 5-10 stalls),
  forward not at all.
- Next suspects: the velocity loop on a sector-quantised estimate (sd ~0.6-0.7 rad/s at 1 rad/s) -> softer PI or a
  longer velocity filter below 2 rad/s, or a velocity estimate over a fixed window from the SmoothingSensor angle.
