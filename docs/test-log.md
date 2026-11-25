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

### Velocity PI sweep at +-1 rad/s (combo_f103, compensation 0.38 V threshold 0.2 A)

`--dt 0.38@0.2 --pi 0.4/1/0.01,0.2/1/0.01,0.2/0.3/0.01,0.4/1/0.03 --targets 1:15,-1:15`, 3 passes, no fault
(data/2026-11-20/combo_f103_pi_sweep.csv). PI = P / I / velocity filter Tf [s].

| PI | +1: stalls / cv / iq sd / true speed | -1: stalls / cv / iq sd / true speed |
|---|---|---|
| 0.4 / 1 / 0.01 (reference) | 11.0 (10-12) / 84 % / 0.27 / 0.85 | 7.7 (6-9) / 76 % / 0.24 / 0.86 |
| 0.2 / 1 / 0.01 | 12.0 (10-14) / 86 % / 0.19 / 0.93 | 10.7 (5-19) / 81 % / 0.19 / 0.94 |
| **0.2 / 0.3 / 0.01** | **1.7 (1-2) / 46 % / 0.16 / 0.96** | **4.7 (3-7) / 57 % / 0.17 / 0.95** |
| 0.4 / 1 / 0.03 | 12.3 (9-14) / 92 % / 0.25 / 0.90 | 13.0 (9-15) / 91 % / 0.25 / 0.90 |

- **The velocity PI was the limit at 1 rad/s**: P 0.2 I 0.3 cuts the stalls by 85 % forward / 40 % backward, halves
  the cv, current ripple -40 %, real speed error -14 % -> -4 %. Lower P alone already fixes most of the speed error
  (0.93) but not the stalls; the lower I is what removes them (less integrator wind-up between sector updates).
- A longer velocity filter (30 ms) is worse, as at 2 rad/s.
- To do: even softer (P 0.1-0.2, I 0.1-0.3), and check these gains at 2 and 5 rad/s (they may be too soft there:
  gain scheduling by speed if so).

### Soft PI at 2 and 5 rad/s, and softer at +-1 rad/s (combo_f103, compensation 0.38 V)

At 2 and 5 rad/s (data/2026-11-20/combo_f103_pi_2_5.csv, 3 passes) P 0.2 I 0.3 is as good or slightly better:

| PI | 2: stalls / cv / iq sd / true | -2: stalls / cv / iq sd / true | 5: cv / iq sd / true |
|---|---|---|---|
| 0.4 / 1 / 0.01 | 0 / 25 % / 0.18 / 1.98 | 0 / 25 % / 0.18 / 1.98 | 40 % / 0.16 / 5.00 |
| 0.2 / 0.3 / 0.01 | 0 / 23 % / 0.15 / 1.99 | 0 / 22 % / 0.15 / 1.99 | 39 % / 0.15 / 4.98 |

Softer at +-1 rad/s (data/2026-11-20/combo_f103_pi_soft.csv, 3 passes):

| PI | +1: stalls / cv / true | -1: stalls / cv / true |
|---|---|---|
| 0.2 / 0.3 / 0.01 (repeat) | 6.0 (3-8) / 63 % / 0.95 | 7.3 (5-9) / 72 % / 0.95 |
| 0.1 / 0.3 / 0.01 | 8.0 (7-9) / 87 % / 0.96 | 5.0 (2-10) / 64 % / 0.97 |
| 0.2 / 0.1 / 0.01 | 8.3 (6-10) / 69 % / 0.88 | 10.3 (7-12) / 78 % / 0.87 |
| 0.1 / 0.1 / 0.01 | 5.3 (2-8) / 73 % / 0.92 | 6.7 (5-8) / 86 % / 0.92 |

- The repeat of 0.2 / 0.3 is clearly worse than its first run (stalls 1.7 -> 6.0 forward, 4.7 -> 7.3 backward):
  large run-to-run variance at 1 rad/s. Over the 6 passes: 3.9 forward / 6.0 backward vs 11 / 7.7 for 0.4 / 1, so
  a real gain forward, small backward. The real speed gain is robust (0.95-0.97 vs 0.85).
- Softer than P 0.2 I 0.3 brings nothing; I 0.1 brings the speed error back (0.88-0.92).
- **Keep P 0.2, I 0.3, Tf 0.01** for this board: better from 1 to 5 rad/s, no need for speed-dependent gains.

### Friction feed-forward at +-1 rad/s with the soft PI (combo_f103, comp 0.38 V, PI 0.2 / 0.3 / 0.01)

`--pi 0.2/0.3/0.01/<XF>` (4th field = XF friction feed-forward [A]), 3 passes, no fault
(data/2026-11-20/combo_f103_friction.csv):

| XF [A] | +1: stalls / cv / true | -1: stalls / cv / true |
|---|---|---|
| 0 | 6.0 (4-9) / 65 % / 0.95 | 7.3 (3-10) / 69 % / 0.95 |
| 0.1 | 4.3 (3-7) / 59 % / 0.95 | 7.3 (5-9) / 73 % / 0.94 |
| 0.2 | 4.0 (2-5) / 58 % / 0.95 | 6.7 (4-9) / 72 % / 0.94 |
| 0.3 | 6.0 (4-8) / 65 % / 0.94 | 9.0 (5-12) / 79 % / 0.93 |

- Still no clear effect, now with a slower integrator either: 0.1-0.2 A slightly fewer stalls forward, within the
  run-to-run spread; 0.3 A slightly worse. Same conclusion as this morning: a constant friction torque is not what
  makes the wheel stall. Probably position dependent (cogging, a tight spot): to check by mapping the stalls on
  the hall-counted angle.

### Where do the stalls happen? Stalls mapped on the hall-counted position (combo_f103)

No absolute encoder: firmware dbg gets `hall_count` (electric_rotations*6 + sector since boot, DBG_MAGIC 0xF0CDB005),
position = hall_count mod 90 (one turn), valid while the board stays powered. `lowspeed_campaign.py --edges` logs every
hall edge, `tools/stall_map.py` (per mechanical position) and `tools/stall_sectors.py` (per electrical sector) analyse
it. Comp 0.38 V, PI 0.2 / 0.3 / 0.01, +-1 rad/s 22 s x 4 passes = 12 turns per direction, no fault
(data/2026-11-20/stall_map_edges.csv, stall_map_runs.csv).

- The slow positions repeat from turn to turn (correlation first / second half of the visits +0.82 forward, +0.75
  backward), 75 % / 58 % of the stalls in the 10 % slowest positions.
- **But they are electrical, not mechanical**: forward, the 10 slowest positions are all = 2 mod 6, i.e. the same
  hall sector in every electrical period. Per electrical sector (mean time / nominal, stalls):

| hall state | 1 | 5 | 4 | 6 | 2 | 3 |
|---|---|---|---|---|---|---|
| forward | 0.64 / 0 | 1.21 / 7 | **2.27 / 47** | 0.85 / 1 | 0.68 / 1 | 0.63 / 0 |
| backward | 0.88 / 0 | 0.82 / 1 | 0.78 / 0 | 0.68 / 0 | 1.37 / 17 | **1.75 / 30** |

- Per electrical period (15 per turn) the mean time only varies 0.89-1.31: the mechanical position (tight spot,
  friction) plays a minor role. 84 % (forward) and 98 % (backward) of the stalls are in 1-2 sectors out of 6.
- The sector widths are normal there (state 4: 57.5°, states 2/3: 64.0 / 56.6°), so it is not the hall geometry.
  A torque dip at a fixed electrical angle, i.e. a 1x electrical frequency disturbance: typical of a current sense
  offset error (R_ds_on sensing, offsets measured once at start-up), or one phase weaker (driver / MOSFET / dead time
  per phase). Cogging would repeat per slot, not once per electrical period.
- Offsets could not be checked at rest: with the driver disabled the R_ds_on sense reads ~-11 A (MOSFETs off).
- Next: same mapping in voltage torque mode (no current sense in the loop) and/or with the motor enabled at zero
  current to read the offsets; if the dip moves or disappears, it is the current sense.

### Current offsets: measured at 0 V, re-centred (XO), stalls mapped again (combo_f103)

New: `XO` bench command (re-centres the offsets from the mean measured current, only when enabled in open loop at
0 V), `tools/offset_trim.py` (DB0 MC4 MT0 ME1, measure, XO, measure). The bench forces foc_current in closed loop:
"MT0" there gave zero *measured* current with uq 0.32 / ud -0.14 V applied, not a 0 V test.

Offset error at 0 V (real current 0): warm, after ~1 h of tests: ia -0.047, ib -0.047, ic +0.094 A; just after a
reflash (boot calibration): -0.021 / -0.025 A; after XO: -0.008 / -0.007 A. Some drift with temperature.

Same mapping as before after XO (comp 0.38 V, PI 0.2 / 0.3 / 0.01, +-1 rad/s 22 s x 4, no fault,
data/2026-11-20/stall_map_trim_edges.csv):

| hall state | 1 | 5 | 4 | 6 | 2 | 3 | stalls |
|---|---|---|---|---|---|---|---|
| forward before XO | 0.64 | 1.21 | **2.27** | 0.85 | 0.68 | 0.63 | 56 |
| forward after XO | 0.65 | 0.65 | 0.84 | 1.02 | **1.66** | **1.42** | 21 |
| backward before XO | 0.88 | 0.82 | 0.78 | 0.68 | **1.37** | **1.75** | 48 |
| backward after XO | 0.78 | **2.61** | 1.16 | 0.60 | 0.54 | 0.71 | 71 |

(mean sector time / nominal)

- **A 0.02-0.04 A change of the offsets moves the slow sector by ~2 sectors (~120° electrical)** in both
  directions, and the slow sectors stay roughly opposite between forward and backward: a 1x electrical torque
  disturbance tied to the current measurement. Still repeatable per position (+0.87 / +0.89), mechanical position
  still minor (per electrical period 0.92-1.39).
- Nulling the offsets at 0 V does not remove it: the offset that matters is the one *while running* (R_ds_on
  sensing: switching, MOSFET temperature, sampling instant), not the one at 0 V. At 1 rad/s iq is only a few
  tenths of an ampere, so a few tens of mA of error already decide where the wheel stalls.
- Ideas: estimate the running offset from the 1x electrical ripple of the current PI output (ud/uq) at constant
  speed, or tune the offsets on the bench to minimise the stalls; an independent current reference (scope + current
  probe) would settle it.
- DC link current (right motor shunt on PC1, EFeru defines.h, not read by the bench yet): at low speed its mean is
  only power / Vbus (~10-70 mA), and with the SimpleFOC sampling at the PWM centre (zero vector) it is 0 at the
  sampling instant, so it cannot be compared to a phase current without a dedicated sampling instant.

### Running offset from the 1x ripple of ud (tools/offset_ripple.py)

Velocity mode +-3 rad/s 12 s each (comp 0.38 V, PI 0.2 / 0.3 / 0.01), offsets as left by XO. Least squares
ud(th) = c + A cos th + B sin th on ~3800 samples per direction (+-0.0017 V on A, B, i.e. +-3 mA):

| direction | iq | A / B [V] | offset error ia / ib / ic [A] |
|---|---|---|---|
| +3 rad/s | +0.275 | +0.0035 / -0.0142 | -0.006 / +0.025 / -0.019 |
| -3 rad/s | -0.267 | -0.0265 / -0.0062 | +0.047 / -0.014 / -0.033 |

- The two directions differ well beyond the fit precision: the 1x ud ripple is not only an offset. The part common
  to both directions (the offset) is ia +0.020, ib +0.005 A; the part that changes sign with the direction (~0.015 V)
  matches a ~1° electrical 1x angle error projecting the back-EMF (~0.96 V at 3 rad/s) on the d axis, or another
  sign dependent effect.
- `XO<da>,<db>` now shifts the measured currents by hand (XO alone still re-centres at 0 V). Not applied yet
  (needs a reflash): XO then XO-0.020,-0.005, check with offset_ripple.py, map the stalls again.

### Running offset correction applied (XO then XO-0.020,-0.005), checked, stalls mapped

After a reflash: offset at 0 V -0.027 / -0.026 A, XO -> -0.007 / -0.005 A, then the measured currents shifted by
-0.020 / -0.005 A. offset_ripple.py again: +3 rad/s ia -0.015 ib -0.031, -3 rad/s ia -0.049 ib +0.025, mean
ia -0.032 ib -0.003 A, instead of ~0: **the ud ripple estimate is not reproducible** (each direction moved by
30-90 mA between two runs with similar offsets), it cannot be used to correct the offsets as it is.

Stall map with this shift (data/2026-11-20/stall_map_shift_edges.csv), mean sector time / nominal:

| hall state | 1 | 5 | 4 | 6 | 2 | 3 | stalls |
|---|---|---|---|---|---|---|---|
| forward | 0.60 | 0.76 | **1.46** | **1.54** | 1.12 | 0.80 | 23 |
| backward | 1.03 | **2.17** | 0.99 | 0.57 | 0.61 | 0.91 | 50 |

- The forward slow sector moved again (2/3 -> 4/6) with a 0.02 A shift, the backward one stayed on state 5.
  Stalls over the three maps (forward + backward, 12 turns each): 104 (boot offsets, warm) -> 92 (XO) -> 73 (XO + shift).
- Conclusion for now: at 1 rad/s the stall location is decided by current errors of a few tens of mA (R_ds_on sense,
  sample sd ~0.18 A), which drift and are not separable from other 1x effects with the bench data alone. An
  independent current reference is needed to go further (scope + current probe on one phase), or a board with real
  shunts. Speeds >= 2 rad/s are fine with compensation + hall decay + soft PI.

## 2026-11-20 - time traces at 2 rad/s (figures)

tools/trace_run.py (one 12 s run, every dbg read logged), tools/plot_trace.py -> docs/figures/f103_trace_2rads.png;
tools/plot_ab.py -> docs/figures/f103_ab_2rads.png (bars from dt_f103_ab.csv / combo_f103_ab.csv).
Velocity PI 0.4/1/0.01, D 0.38 V, threshold 0.2 A. Firmware dt_f103 for dev / compensation, combo_f103 for hall
velocity / both (combo_f103 flashed back afterwards). Data: data/2026-11-20/trace_{dev,dt,hallvel,both}.csv.

| setup | stalls in 8 s | iq sd |
|---|---|---|
| dev | 19 | 0.44 A |
| + compensation | 5 | 0.34 A |
| + hall velocity decay | 9 | 0.30 A |
| both | 0 | 0.17 A |

Same picture as the A/B campaign. The real speed (hall edges) shows it best: dev alternates ~0.6 rad/s sectors
and ~4.5 rad/s catch-ups once per electrical period; both stays at 1.5-2.5 rad/s. iq shows a 1x electrical
oscillation (~0 to 1.2 A) without the corrections, flat noise with both.

## 2026-11-20 - hall edge log in firmware (not tested on the board yet)

The PC reads dbg ~370 times/s through the ST-Link (USB round trips), so hall edge times seen by the PC are off by
up to ~2.7 ms (~8 % of a sector at 2 rad/s): spikes on the speed traces. Now every sector change is timestamped in
the hall interrupt (micros()) into a ring buffer `hall_edges[1024]` / `hall_edge_n` (8 KB, F103 only: the GD32F130
has 8 KB of RAM). `BoardMemory.read_hall_edges()` returns the new edges (overflow detected), trace_run.py saves them
to `<out>_edges.csv` and plot_trace.py uses them when present. Reader tested offline (wrap, overflow, micros wrap).
Loop cost size check (same flags, symbol sizes): compensation +0.5 KB flash always (setPhaseVoltage +380 B,
loopFOC +100 B), +0.9 KB calibrateDeadtimeCompensation if called, +20 B RAM per motor; hall decay +16 B flash,
+4 B RAM. Loop time: tools/perf_run.py (XP at 2 rad/s), envs perf_dev_f103 / perf_hall_f103, not run yet.

### Edge log validated on the board (combo_f103, D 0.38, 2 rad/s, 12 s)
337 edges logged, all consecutive (no loss, no glitch). The hall count runs opposite to the motor (SENSOR_DIRECTION
-1): plot_trace.py orients it with the sign of the velocity estimate. Same run, speed per sector:
PC polling mean 2.19 sd 0.80 max 4.48 rad/s; firmware log mean 2.07 sd 0.44 max 3.05 rad/s. The spikes were polling
artefacts; what remains is a clean pattern repeating every electrical period (6 sectors, ~0.21 s): partly real
1x electrical torque ripple, partly the unequal hall sector widths (to be separated with the hall calibration).
Figure docs/figures/f103_edge_log_check.png. ST-Link read timing (motor off): 1.35 ms for 4 bytes, 2.3 ms for
88 bytes, 12 ms for 1 KB, ceiling ~90 KB/s; mostly USB waiting, ~20 % pyOCD Python.
Per sector position (count mod 6, 46 visits each): mean time 28 / 28 / 31 / 35 / 42 / 49 ms with only 1-3 ms sd:
the pattern is locked to the electrical position and very repeatable. The hall sector widths (calibration:
54-67 deg el) explain at most +-12 % and do not match it (the 67 deg sector is one of the fastest): the remaining
+-40 % is a real 1x electrical speed ripple, the same signature as the current sense offset ripple seen earlier.

## 2026-11-20 - loop cost of the two PRs (STM32F103, 64 MHz, no FPU)

tools/perf_run.py: 2 rad/s velocity mode, foc_current, 10 XP reads of 256 loops each, same bench code and flags
for all firmwares (perf_dev_f103 = dev ae7b3ad, perf_hall_f103 = dev + b691018, dt_f103 = dev + d0f4aa3,
combo_f103 = both). Data data/2026-11-20/perf.csv. Mean loopFOC / move [us]:

| firmware | loopFOC | move |
|---|---|---|
| dev | 297.0 | 81.3 |
| + hall velocity decay | 297.4 | 84.9 |
| + compensation, D 0 | 299.9 | 80.5 |
| + compensation, D 0.38 | 331.1 | 84.3 |
| both, D 0 (x2) | 300.1 / 301.3 | 85.2 / 85.2 |
| both, D 0.38 (x2) | 330.8 / 331.5 | 84.9 / 85.1 |

- Compensation active: **+31 us in loopFOC** (+10 %, ~2000 cycles at 64 MHz: soft float on the M3), repeatable to 1 us.
  Code present but D 0: +3 us. On a whole bench loop of ~431 us that is +7 % (2.3 -> 2.15 kHz).
- Hall velocity decay: not measurable (move varies 80-85 us between runs of the same firmware; getVelocity +8 B).
- The bench "debug" part also grows +39 us with compensation on: bench code recomputing the compensation for dbg,
  not library cost (excluded above).

### Time traces of the 4 setups with the edge log (2 rad/s, 12 s each)
Same settings as the polled traces; docs/figures/f103_trace_2rads_edgelog.png, data tracelog_{dev,dt,hallvel,both}*.csv.
The curves are now clean: dev and hall velocity alone show a 1x electrical cycle (one ~0.6 rad/s sector, then a
~4 rad/s catch-up), the compensation halves it, both keep 1.5-2.7 rad/s. iq sd 0.43 / 0.26 / 0.31 / 0.16 A.

## 2026-11-20 - STM32F103 loop: clock, -O2, double precision

**Clock** (RCC read by pyOCD, motor off): HSE off, PLL = HSI/2 x 16 = **64 MHz** (not 72), APB1 /2, flash 2 wait
states, prefetch on (FLASH_ACR 0x32). The board has no crystal: the core variant (generic_clock.c, F103R(C-D-E)T)
starts the HSI. 64 MHz is the maximum from the HSI on an F103, nothing to gain there. The profiler converts with
SystemCoreClock, so all the us figures were right; only the "72 MHz" label was wrong (fixed above).

**-O2** (tools/perf_run.py, 2 rad/s, 10 XP reads, same firmware as perf_dev_f103 otherwise; data/2026-11-20/perf_o2.csv):

| firmware | loopFOC [us] | move [us] | total [us] | flash [B] |
|---|---|---|---|---|
| perf_dev_f103 (-Os) | 297.1 | 80.8 | 430.9 | 89604 |
| perf_o2hot_f103 (loop files -O2, scripts/o2_hot.py) | 294.2 | 80.2 | 427.6 | 93748 |
| perf_o2_f103 (whole firmware -O2) | 294.2 | 78.0 | 425.8 | 101844 |

Only -1 % for +4 / +12 KB: the loop time is not in the compiled SimpleFOC code but in the libgcc soft float
routines (already optimised, not affected by -O2) and the flash wait states. Not worth it.

**Double precision** (env dblaudit_f103 with -Wdouble-promotion, then the callers of __aeabi_d* / f2d in the ELF):
in the loop only SmoothingSensor::update() (fixed by Candas1/Arduino-FOC-drivers#3). SimpleFOC dev itself is clean
except Encoder.cpp:93 (intentional, once per index). The rest is command parsing (strtod, Commander) and
Sensor::getPreciseAngle(), not called in the loop. Nothing to fix in the library.

Remaining levers on the F103: fewer float divisions per loop (LowPassFilter alpha = Tf/(Tf+dt) on every call, PID
derivative / dt, HallSensor velocity, setPwm / voltage_power_supply x3), libgcc float routines and hot code in RAM
(no wait states), fixed point (not upstreamable).

## 2026-11-20 - STM32F103: where the loop time goes, code in RAM (slower)

**PC profile** (tools/perf_run.py --pc 3000: DWT_PCSR sampled while running at 2 rad/s, perf_dev_f103):
~70 % of the CPU time is in the libgcc soft float routines: __aeabi_fadd 16.7 %, __mulsf3 15.2 %, __divsf3 9.1 %,
float compares ~20 % (__nesf2 9.0 %, __aeabi_cfcmple 7.5 %, __aeabi_fcmp* ~4 %), int to float __floatdisf 3.7 % /
__floatsisf 2.9 %. Then PIDController 2.8 %, getCurrentMicros 2.7 %, LowPassFilter 1.6 %, _sin 1.3 %, fmodf 1.1 %.
The compares come from `_constrain` and `_isset` (`!= NOT_SET`, float) on every loop: direct call sites in
FOCMotor::loopFOC (18), BLDCDriver6PWM::setPwm (12), _writeDutyCycle6PWM (12), PIDController (12),
BLDCMotor::setPhaseVoltage (6), CurrentSense::getABCurrents (5), LowsideCurrentSense::getPhaseCurrents (3).
Each soft float compare goes through 2-3 libgcc functions.

**Code in RAM** (ld/f103rc_ram_libgcc.ld, ld/f103rc_ram_hot.ld: copies of the core ldscript, the objects moved to
.data with EXCLUDE_FILE; data/2026-11-20/perf_ram.csv):

| firmware | loopFOC [us] | move [us] | total [us] | RAM used |
|---|---|---|---|---|
| perf_dev_f103 (all in flash) | 297.1 | 80.8 | 430.9 | 44.3 % |
| perf_ramgcc_f103 (libgcc float in RAM) | 357.6 (+20 %) | 97.4 | 516.0 | 47.6 % |
| perf_ramhot_f103 (libgcc float + loop files in RAM, +23 KB) | 307.8 (+3.6 %) | 83.7 | 447.0 | 94.5 % |

Slower: on the F1 the Cortex-M3 fetches code from SRAM through the system bus (no flash prefetch, shared with the
data accesses), and calls between flash and RAM go through long branch veneers (3-5 % of the samples). With 2 wait
states and the prefetch, the flash is the fastest place. Not worth it; the GD32F103 advantage is its zero wait
state flash, not reproducible on the STM32F103.

## 2026-11-20 - "acceptable" loop optimisations on the STM32F103 (one branch each, from dev ae7b3ad)

Branches on candas-agent/Arduino-FOC: perf/setpwm-single-limit (ae62ac9), perf/loopfoc-dt-once (c07cdbf),
perf/current-sense-pins-int (d88e569), all three merged in test/perf-acceptable (b356781). No API or behaviour change:
- setPwm (3PWM and 6PWM): one limit to min(voltage_limit, voltage_power_supply), then one multiplication by
  1/voltage_power_supply: same duty cycle, 7 float compares instead of 12, 1 division instead of 3 (simplefoc#356).
- loopFOC: one elapsed time for the 2 current filters and the 2 current PIs (simplefoc#325, same commit as GD32).
- pins: `pin != _NC` (integer) instead of `_isset(pin)` (int converted to float, soft float compare) in
  Inline/LowsideCurrentSense and the STM32 injected ADC read.

tools/perf_run.py at 2 rad/s, 10 XP reads of 256 loops, dev and the combined firmware measured twice
(data/2026-11-20/perf_accept.csv). Flash = whole firmware (same bench code and flags):

| firmware | loopFOC [us] | vs dev | move [us] | total [us] | flash [B] |
|---|---|---|---|---|---|
| dev (x2) | 297.1 / 297.1 | | 81.8 | 432.1 | 89604 |
| setPwm one limit, one division | 286.3 | -10.8 (-3.6 %) | 79.8 | 419.1 | 89524 (-80) |
| dt once per loopFOC | 273.7 | -23.4 (-7.9 %) | 80.5 | 407.1 | 89764 (+160) |
| pins compared as integers | 289.2 | -7.9 (-2.7 %) | 79.6 | 413.9 | 89484 (-120) |
| the three (x2) | 255.3 / 255.5 | -41.7 (-14.0 %) | 81.6 / 82.5 | 381.8 / 383.0 | 89572 (-32) |

The gains add up (10.8 + 23.4 + 7.9 = 42.1 us vs 41.7 measured), repeatable to 0.2 us. They pay the dead-time
compensation cost (+31 us) with 11 us to spare. RAM unchanged. move unchanged (none of them touch it).

Re-measured after renaming the dt overloads to explicit `filter(x, dt)` / `compute(error, dt)` (perf/loopfoc-dt-once
79aee30; 176b773 only adds a comment): dev 297.1 us, branch 273.5 us (-23.6 us, -7.9 %), flash +160 B, RAM same.
Same design as discussed in simplefoc#325 (yours, 2023: dekutree64 overload, runger "sample the clock once",
Copper280z API compatible split); only the 4 current filters / PIDs of loopFOC, not the sensor / velocity ones.

## 2026-11-20 - dt once: two behaviour changes found and fixed, first cloud builds

Review of simplefoc#596 found two behaviour changes vs dev: (1) a fixed `Ts` set by the user on the current
filters / PIDs was ignored, (2) the current filters were no longer reset after a pause (motor disabled, other torque
mode), because loopFOC keeps running and its loop time stays normal. Fix (candas-agent perf/loopfoc-dt-once 413a9ed,
on top of the user's branch: perf/loopfoc-dt-once-fix): dt = time since the previous foc_current step (loopFOC
timestamp, new uint32 member), and `_isset(Ts) ? Ts : dt` for each object. Edge cases left: back to foc_current after
< 0.3 s in another torque mode (one loop with the absence as dt), two loops in the same microsecond (impossible).

Firmwares built in the cloud (GitHub Actions, workflow build.yml, run 44827114770: 3 envs in parallel in 1 min 42 s;
local: 52 s / 62 s with the cache, 469 s for the new SimpleFOC commit). Cloud and local .bin differ only by 3 newlib
assert path strings (linux-x64 / win32-x64), same code. Flashed with pyOCD, tools take --elf.

Loop time (perf_run, 2 rad/s; data/2026-11-20/perf_dtfix.csv) and flash / RAM (PlatformIO, recorder now 24 B/sample):

| firmware | loopFOC [us] | flash [B] | RAM [B] |
|---|---|---|---|
| dev | 297.3 | 89668 | 25752 |
| dt once (#596 as opened) | 273.7 (-23.6) | 89828 (+160) | 25752 |
| dt once + fix | 279.2 (-18.1, -6.1 %) | 89916 (+248) | 25756 (+4) |

The fix costs 5.5 us (4 soft float `_isset(Ts)` compares + the timestamp), more than the 2-3 us expected.

Re-enable test (tools/reenable_test.py: velocity step 0 -> 2 rad/s, motor disabled 60 ms later with a filtered iq
of 0.6-1.1 A frozen, 1 s pause, enabled with target 0; recorder every loop; data/2026-11-20/reenable.csv). Filtered
iq on the first loop after enabling, 3 runs each:

| firmware | iq frozen [A] | first loop [A] | first loop / frozen |
|---|---|---|---|
| dev | 0.90 / 0.61 / 0.84 | +0.06 / +0.37 / -0.09 | 0.07 / 0.60 / -0.10 |
| dt once (#596 as opened) | 0.84 / 0.88 / 1.05 | +0.48 / +0.45 / +0.59 | 0.57 / 0.51 / 0.56 |
| dt once + fix | 0.81 / 1.09 / 0.58 | -0.15 / +0.06 / -0.10 | -0.18 / 0.05 / -0.17 |

#596 as opened always restarts at about half the value from before the pause (then decays over a few loops); dev and
the fix restart at the noise level (+-0.4 A at standstill: one dev run at 0.37 A). `Ts` priority: code review only
(no bench command sets Ts).

## 2026-11-20 - setPwm one limit / one division, with a guard for invalid voltages

candas-agent perf/setpwm-single-limit d41eef5 (3PWM and 6PWM): u_max = min(voltage_limit, voltage_power_supply);
if u_max > 0: dc = constrain(U, 0, u_max) * (1 / voltage_power_supply), else dc = 0 (one float compare more).

Equivalence on a PC (tools/setpwm_equiv.py, float32 with the C semantics, duty = (uint32)(dc * (ARR+1))):
- 2 million random cases (supply 1-60 V, limit 0.1-1.5 x supply, U from -5 to 70 V, a quarter exactly at the limit,
  a quarter exactly at the supply): dc never above 1, never NaN; duty identical in 96 %, one timer count less or
  more in 4 % (U * (1/Vps) vs U / Vps rounding), never more than one count (ARR+1 = 256 to 65536).
- 4050 combinations of special values (-12345, -1, -0, 0, 1e-6 ... 1e6, +-inf, NaN for U, limit and supply):
  246 differ by more than one count, all with voltage_power_supply <= 0: dev gave 100 % (U / 0 = inf -> 1, also
  with both negative), now 0 %. No difference with a valid configuration. NaN only where dev was NaN too (U NaN,
  or U and supply infinite).

Loop time (cloud build run 44832025239, perf_run 2 rad/s, dev / branch alternated twice;
data/2026-11-20/perf_setpwm.csv): dev 296.9 / 296.9 us, branch 287.9 / 287.9 us: -9.0 us (-3.0 %; -10.8 us without
the guard). Flash -56 B, RAM same. Saturation (U >= supply) not run on the motor (needs full voltage): covered by
the PC test only.

## 2026-11-21 (night, compile only, board off) - more loop and size explorations

Soft float calls per loop function in the dev ELF (static count): setPhaseVoltage 69 (mostly the trapezoid modes;
SVPWM: _sincos, inverse Park / Clarke products already shared by the compiler, 4 compares for min / max),
_writeDutyCycle6PWM 46 (hardware 6PWM path: 3 x u2f, fmul, f2u), loopFOC 45, PIDController 32, getABCurrents 18
(only one compare used on a 2-phase board: not worth changing, and pins instead of `== 0` would change the formula
when a measured current is exactly 0).

**PID: derivative skipped when D is 0** (candas-agent perf/pid-skip-zero-derivative 6f0f96c): `output = P + I;
if (D != 0) output += D*(e - e_prev)/dt`. D is 0 in the 3 loop PIDs (current q, d, velocity). In the binary, with D =
0 each call skips fsub, fmul, fdiv and fadd for one fcmpeq. Flash +8 B. PC equivalence (tools/pid_equiv.py, float32,
C semantics): 2 million random cases (half with D = 0, with / without limit and ramp): output and integral identical
bit for bit; 24000 special-value combinations: identical whenever D != 0; with D = 0 only an infinite error differs
(dev: 0 x inf = NaN output; branch: finite, limited) and the sign of a zero output. Expected (not measured: board off):
~10-13 us less in loopFOC (2 PIDs, ~250 cycles per soft division from the setPwm measurement), ~6 us in move.

**Size** (cloud build run 44833596974, whole firmware, same bench code):

| variant | flash [B] | RAM [B] |
|---|---|---|
| dev (perf_dev_f103) | 90204 | 27288 |
| `-D SIMPLEFOC_DISABLE_DEBUG` (no debug messages) | 84820 (-5384, -6 %) | same |
| `-ffast-math` | 90124 (-80) | same |
| `-flto` | 83508 (-6696) | 18816 |

**LTO must not be used with SimpleFOC**: the STM32 low-side current sense overrides are lost (no
HAL_ADCEx_InjectedStart, _findBestADCForInjectedPins, no HAL_ADCEx_InjectedGetValue call left: the generic weak
functions are linked instead, i.e. no current measurement), and the bench `hall_edges` buffer (read only by pyOCD)
is removed. SIMPLEFOC_DISABLE_DEBUG is a safe user flag (-5.4 KB) for projects that do not need the messages.

### Clock reads and _normalizeAngle (night, compile only)

Clock reads per loop in the library (dev): loopFOC 1, move 1, SmoothingSensor 1, LowPassFilter 3 (current q / d,
velocity), PIDController 3 (current q / d, velocity) = 9 (getCurrentMicros + HAL_GetTick were 3.6 % of the loop).
simplefoc#596 removes 4. Next candidate (stacked on #596, same pattern): the velocity filter and PI from the move()
time (updateMotionControlTime), with the same care (user Ts, reset after a pause: own timestamp of the velocity step).
Not written yet.

LowPassFilter alpha = Tf / (Tf + dt): the default Tf is not 0, the division is needed (no simple gain).

**simplefoc#530** (jaguilar, faster `_normalizeAngle`: `angle - floorf(angle / 2PI) * 2PI` instead of fmodf, plus
normalisation in `_electricalAngle`, the open loop angle and the alignment), cherry-picked on dev ae7b3ad (candas-agent
test/pr530-on-dev a083b84, one conflict: dev already uses fmodf), env perf_pr530_f103 built in the cloud (run
44834847297): flash -376 B, `_normalizeAngle` = fmul, floorf, fmul, fsub (fmodf was ~1.1 % of the loop). To measure.
Comparison on the PC (tools/normalize_equiv.py, float32):
- electrical angles 0..15 turns: max difference 3.3e-6 rad, both always in [0, 2PI);
- -1000..1000 rad: max difference 3e-5 rad; #530 returns slightly **negative** angles in 8 of 5 million (down to
  -6e-5 rad); near multiples of 2PI (k = -2000..2000): 634 of 12000 outside [0, 2PI), down to -0.001 rad;
- consequence in setPhaseVoltage trapezoid modes: sector = 6 * a / 2PI becomes -1 (out of trap_120_map) in those
  cases; dev has the same problem much more rarely (returns exactly 2PI once in 12000 near multiples: sector 6).
Worth telling jaguilar / the maintainers (not posted: the user decides).

## 21/11 - PID D=0, PID redundant _isset, #530 vs dev (STM32F103, 2 rad/s)
tools/perf_run.py, 10 XP reads of 256 loops, cloud builds (dev and isset: run 44876679852, pid: 44834046818,
#530: 44834847297), alternated; data/2026-11-21/perf_pid.csv. Wait ~3 s after pyocd flash before perf_run
(otherwise dbg.magic reads 0).

| firmware | loopFOC | move | total | flash |
|---|---|---|---|---|
| dev ae7b3ad (3 runs) | 297.0 us | 81.6 us | 431.7 us | 90208 B |
| PID derivative skipped when D=0 (2-3 runs) | 292.2 (-1.6%) | 78.0 (-4.4%) | 423.0 (-2.0%) | +8 B |
| PID redundant _isset removed (2 runs) | 295.5 (-0.5%) | 81.4 | 430.2 (-0.35%) | -16 B |
| #530 _normalizeAngle (1 run) | 295.7 (-0.4%) | 80.9 | 428.5 (-0.7%) | |

Redundant _isset: only one float compare call less per PID in the binary (GCC already merged part of it);
exhaustive PC check (tools/pid_isset_equiv.py, all 2^32 floats): same ramp condition.

D = 1e-6 on the 3 PIDs (perf_run --extra MQD0.000001 MDD0.000001 MVD0.000001, dev / pid alternated twice;
data/2026-11-21/perf_pid_dset.csv): dev loopFOC 304.9 / move 84.3 / total 442.3 us, PID branch 307.5 / 85.9 /
446.2 us: **+3.9 us (+0.9 %) when D is set** (~1.3 us per PID, more than the 0.5 us estimated from one compare).
Dev itself is 10.6 us slower with D = 1e-6 than with D = 0: libgcc multiply/divide take a fast path on a zero operand.

## 2026-11-21 - On-target equivalence tests (src/equiv, tools/equiv_read.py)

PR code linked in a test firmware vs a verbatim copy of dev, real compiler and libgcc. The same firmware linked with
dev gives 100 % identical (checks the copies). Board fed by the ST-Link 3.3 V only for the PWM tests (main supply
off: ~2 V on the bus with a multimeter, firmware reads 1.9-5.6 V; gate drivers need 12-15 V).

- **PID** (6f0f96c, 2 M cases incl. 0, -0, +-inf, NaN, NOT_SET, random bit patterns): identical 1941006, expected
  58994 (D == 0 with NaN in dev or signed zero), unexpected 0.
- **setPwm d41eef5** (500 k cases per driver, 6PWM on TIM1 motor pins, 3PWM on PC6-PC8, registers read back):
  normal supply: at most 1 timer count, never above 100 %. **Bug found**: supply below ~3e-39 (denormal), 1/Vps
  overflows to inf: register 65535 (> ARR+1 = 2000, i.e. 100 % or more) in 6 (6PWM) / 8 (3PWM) phases; dev stayed
  bounded. Not seen on the PC test (no denormal supply in its special values).
- **setPwm 15254e1** (fix: `inv_supply < INFINITY` in the condition, else no voltage): unexpected 0, max 1 count with
  a valid supply, register always == dc * (ARR+1). dc can exceed 1 by one ulp with a supply ~3e38 V (4 phases):
  register 2000 = ARR+1, the same as dev. Flash -40 B vs dev (was -56 B). Loop time with the guard: to measure.

## 2026-11-21 - On-target checks of the PRs already sent upstream (simplefoc#595, #596)

- **#595 pins** (equiv_pins_f103, Candas1 94074ca): `_isset(pin)` vs `pin != _NC` for every int in [-2^25, 2^25],
  INT32_MIN/MAX and 2^20 random ints (68157443 values): no difference (pins are `int`, _NC = (int)-12345).
- **#596 dt once** (equiv_foc_dev/pr_f103, Candas1 a87b5e1): loopFOC + move with a fake driver, current sense,
  sensor and a fake clock (`-Wl,--wrap=_Z7_microsv`), a fresh motor per case, traces compared bit for bit
  (tools/equiv_foc_compare.py). Identical: filter/PID called directly (operator(), steps 0 to 2 s), steady loop,
  disabled 0.05 / 0.25 / 0.29 / 0.6 s, Ts set, voltage mode 0.1 s, _micros wrap. **Different:**
  1. pause (disabled or another torque mode) between 0.3 and 0.5 s: the first PID step after it uses the time since
     the PID's last own-timed call (the first step of the run) instead of the pause (old operator() with a stale
     timestamp, the new path does not update it). Small (~3 % of voltage.q here).
  2. back to foc_current after < 0.3 s in dc_current or estimated_current (which use LPF/PID_current_q): the PR uses
     the absence as dt (documented) -> integral kick: voltage.q 1.90 -> 3.70 V (dc_current 0.1 s), 1.18 -> 1.81 V
     (estimated_current 0.1 s) on the first loop.
  3. loopFOC twice in the same microsecond (dt = 0): PR takes the old path, the current filters see a stale
     timestamp (> 0.3 s) and restart from the new value; dev keeps the previous value (alpha = 1).
  4. Ts set then set back to NOT_SET at run time: dev uses a stale timestamp on the first step, the PR does not.

## 2026-11-21 - Audit of SimpleFOC dev on the target (src/equiv/equiv_audit.cpp, tools/equiv_audit_read.py)

No peripheral (fake driver / current sense / sensor / clock). Summary and links in C:\Dev\notes\audit_dev.md.
Found in dev: `_sin`/`_cos` of a negative angle = 0 on STM32 (simplefoc#505) -> **angle_openloop with a negative
target applies no voltage** (all phases 6 V, electrical angle -35 rad traced); `_sin` wrong above ~411775 rad;
trapezoid sector 6/12 (out of the map) for angles ~ -PI/6; non-centred trapezoid with Uq < 0 gives nothing (#580);
getDCCurrent sign wrong when the angle is exactly 0; `_sqrtApprox` up to 3.4 % error (dc_current); `_atan2(0,0)` NaN
(#583). Correct: _sin/_cos on [0, 2PI] (1.6e-4), sine/SVPWM vectors, centred trapezoid, getFOCCurrents (3e-4 A),
Sensor velocity/angle at +-0.05/3/200 rad/s across the 2^31 us wrap.

Audit part 2 (same firmware): closed loop with a simulated motor (pp 7, R 1, flux 0.01, J 1e-4, sensor CW and CCW,
zero angle 0.7): velocity +-20 rad/s (7e-5), angle +3/-3 rad (3e-5 rad, velocity limit kept), foc_current 0.5/-0.5 A
and 2 A limited to 1 A (peak 1.07 A), applied voltage <= voltage_limit, initFOC real alignment finds the direction
and 0.700 rad: all correct. HallSensor (private state set directly): steady rotation both ways across 2^31 us OK,
reversal and stop OK; **a glitch to 000/111 makes the angle jump up to 3 sectors (180 deg electrical)** for that
update (sector -1 from ELECTRIC_SECTORS), no drift after.

Audit part 3: dc_current torque (rotor held) gives 0.512 A for 0.5 A (+2.4 %, _sqrtApprox); estimated_current OK;
velocity_openloop +-10 rad/s OK; SmoothingSensor on HallSensor continuous across edges both ways (drivers 1f67e02);
**StepperMotor angle_openloop with a negative angle: no voltage** (same _sin cause); LPF step response, PID ramp,
PID anti-windup, updateMotionControlType targets OK.

Audit part 4 (Commander::motor() on the simulated motor): MLU-2 -> 2 V applied at target 0; MLUnan -> 10 V applied
(limit was 6 V); MWT9 (invalid modulation) -> last 2 V stay applied at target 0; MLC-1 -> 1.04 A at target 0 A;
MLV-5 in angle mode -> runs the wrong way (-11 rad for +3). No validation of the values (atof, enum cast).

## 2026-11-22 - Bugs of docs/audit-dev.md reproduced on the powered STM32F103 board

tools/bug_repro.py (THE MOTOR MOVES, low values, abort above 3 A / 10 rad/s), bench firmware perf_dev_f103 (dev) and
perf_dt596_f103 (simplefoc#596), cloud run 45023777514; data/2026-11-22/. New bench commands: `XR1` plain SimpleFOC
(the bench no longer sets the voltage limit every loop nor forces foc_current in closed loop), `XG0`/`XG7[,n]` invalid
hall state injected for n loops (hall state recorded in the comp_a column of the recorder). Applied duty cycles read
from the TIM1 compare registers (spread between phases, % of the period).

| Case | What happens on the board |
|---|---|
| #11 `MLU-0.5`, torque voltage mode | 0.5 V applied (duty spread 3.35 %) **whatever the target** (+0.3, -0.3, 0): the target is ignored. `ME0` stops it (0 %); **`ME1` again applies 0.5 V again**; only a valid `MLU` restores. Motor did not turn (0.5 V below the friction torque) |
| #11 `MLC-0.3`, foc_current | uq stuck at ~0.63 V whatever the target. Earlier run with the bench voltage limit (4 V): **the motor accelerated alone at target 0 (6 rad/s), reached 10 rad/s with target -0.2 (aborted)** |
| #12 `MWT9` | **duties frozen** at the last 0.4 V (2.65 %) for target 0 and -0.4; `ME0` gives 0 %; **`ME1` with target 0 re-applies the old 0.4 V**; `MWT1` restores |
| #1-2 angle open loop, target -0.6 rad (0.3 V) | electrical angle -9.0 .. -4.7 rad: **duty spread 0.00 % (no voltage at all)**; partial between -PI/2 and 0; back to normal for a positive target |
| #11 `MLV-2`, angle mode | not conclusive: 0.6 V does not start the motor (simulation result stands) |
| #10 hall glitch `XG0` / `XG7` / `XG0,5` at 1 rad/s, foc_current | glitch visible in the recording; iq dips (to ~-0.2 A for 5 loops) within the R_ds_on noise: small effect at this speed and current |
| #596 dc_current -> foc_current after 0.1 s, target 0.2 A | **PR: iq 1.79 A (8 ms above 0.5 A from the target) and -1.26 A the second time (19 ms, wrong direction)**; dev: at most 0.85 A for single samples |

## 2026-11-22 - simplefoc#596 v2: loop timestamp given to the filters / controllers

candas-agent perf/loopfoc-dt-once-v2 (2ef5f18, from Candas1 perf/loopfoc-dt-once): `LowPassFilter` / `PIDController`
get `operator()(x, now_us)` (dev code with now_us instead of _micros()), `operator()(x)` calls it with _micros();
loopFOC passes last_loopfoc_timestamp_us. On-target scenarios (equiv_foc_dev vs equiv_foc_v2, fake clock): **all 18
cases identical to dev bit for bit** (the first version differed in 9). Loop time (perf_run 2 rad/s, dev / v2
alternated twice, cloud run 45025816596; data/2026-11-22/perf_dtv2.csv): loopFOC 297.15 -> 290.45 us (**-6.7 us,
-2.3 %**; first version -6.1 %), total 431.6 -> 425.9 us. Flash +56 B, RAM same.

## 2026-11-22 - simplefoc#596 v3 (per-object times, one dt conversion per loop, dev rules)

candas-agent perf/loopfoc-dt-once-v3 (b7c0e17): each current filter / controller keeps the time of its last call
(also in dc_current, estimated_current and monitor()), the rules of operator() applied to the elapsed time, one
conversion to seconds shared when the elapsed microseconds are equal. On-target scenarios: **all 18 identical to
dev**. Loop time (cloud run 45027774655, dev / v2 / v3 alternated twice; data/2026-11-22/perf_dtv3.csv): loopFOC
dev 297.2, v2 290.25 (-2.3 %), **v3 289.8 (-2.5 %)**. Flash: v2 +56 B, v3 +424 B (RAM +24 B). Sharing the
conversion brings nothing measurable: the extra gain of the first version (279 us) came from skipping the per-object
rules, which is exactly what changed the behaviour.

## 2026-11-22 - setPwm with the overflow guard (Candas1#23, 15254e1)

perf_run 2 rad/s, dev / setPwm alternated twice (cloud run 44881317795; data/2026-11-22/perf_setpwm_guard.csv):
loopFOC 296.95 -> 289.45 us (**-7.5 us, -2.5 %**; -3.0 % before the guard). Flash -40 B. PR description updated.

## 2026-11-22 - Voltage feed-forward in foc_current (bench XK, tools/ff_test.py)

XK1 back-EMF (estimateBEMF from KV 17: 0.324 V per rad/s), XK2 + R * current setpoint, XK3 + dq decoupling
(L 0.37 mH); computed before each loopFOC into feed_forward_voltage. Cloud run 45031765116 (perf_dev_f103).
- Velocity steps 0 -> +-3 -> 0 rad/s (PI P 0.4 I 1 Tf 0.01, tuned without feed-forward), 2 rounds alternated;
  data/2026-11-22/ff.csv: **worse with feed-forward**: overshoot 58 % (off) -> 101 / 91 / 97 % (XK1/2/3), velocity
  not settled after 1 s (|end error| 0.02 -> 0.29 / 0.12 / 0.21 rad/s), iq tracking error RMS 0.22 -> 0.38 / 0.30 /
  0.36 A. The faster current response changes the plant seen by the velocity PI: it would need retuning.
- Back-EMF measured at steady speed (no feed-forward): (uq - R iq) / w = 0.46 (2 rad/s) / 0.40 (4 rad/s), slope
  0.354 V per rad/s + ~0.2 V fixed (dead time): the KV estimate (0.324) is ~9 % low, not the cause.
- Torque mode +-0.2 A steps: the motor does not move (static friction), back-EMF term 0; iq error RMS 0.153 (off) /
  0.144 / 0.139 A (within the R_ds_on noise). +-0.4 A accelerated past 9 rad/s in 0.45 s (aborted by the tool).

Velocity PI retuned with and without feed-forward (XK2 = back-EMF + R*i), steps 0 -> +-3 -> 0 rad/s, 2 rounds
alternated (data/2026-11-22/ff_tune.csv, ff_tune2.csv). Overshoot / rise 10-90 % / |velocity error| after 0.7 s:

| PI P, I | off | XK2 |
|---|---|---|
| 0.1, 0.25 | 8 % / 0.35 s / 0.25 | 5 % / 0.34 s / 0.12 |
| 0.1, 0.5 | 20 % / 0.11 s / 0.07 | 28 % / 0.14 s / 0.03 |
| 0.2, 0.5 | 9 % / 0.20 s / 0.08 | 22 % / 0.12 s / 0.09 |
| 0.2, 1 | 29 % / 0.03 s / 0.02 | 43 % / 0.03 s / 0.01 |
| 0.4, 1 (bench default) | 43-58 % / 0.01 s / 0.02 | 88-91 % / 0.01 s / 0.12-0.18 |

With the same gains the feed-forward acts like a higher gain (faster rise, more overshoot); after retuning, the best
compromises are similar with and without it (within the run-to-run spread). At <= 3 rad/s the back-EMF is ~1 V and
friction / hall resolution dominate: **no measurable benefit of the voltage feed-forward on this board at low speed**.
Very low gains (P 0.05) leave large errors (friction). Not tested: higher speeds, where the back-EMF is a larger part
of the voltage.

## 2026-11-22 - characteriseMotor on the STM32F103 board, with / without dead-time compensation

Firmware dt_f103 (dev + compensation d0f4aa3, cloud run 45033818929), bench command `XM<V>[,<comp>]` =
BLDCMotor::characteriseMotor(V) (correction factor 1.5, as the SimpleFOC command MFP), compensation forced along +d
(loopFOC does not run during the measurement). 0.8 V, motor in torque mode target 0, run twice each, alternated.

| Run | R [Ohm] | Ld [mH] | Lq [mH] | new el. zero (present 2.09) |
|---|---|---|---|---|
| no compensation | 2.57 | 0.0003 | 0.88 (warning Lq > 2 Ld) | NaN |
| compensation 0.38 V | 1.14 | 16.7 | 19.7 | 2.71 |
| no compensation | 1.34 | 0.91 | 1.27 | 1.72 |
| compensation 0.38 V | 1.19 | 3.74 | 21.1 (warning) | 2.83 |

Config values used by the bench: R 0.166 Ohm, L 0.37 mH (the dead-time calibration line fit gave 0.57 Ohm).
**Not usable on this board**: R spread x2 and 7-15x the config value, L spread x60. With R and L of run 3,
tuneCurrentController(100 Hz) would give P 0.8 / I 842 instead of 0.23 / 105. The compensation lowers R (1.14-1.19
vs 1.34-2.57) but makes L worse (forced along +d, it also acts at zero current, so the current between the
inductance pulses is not zero). Likely causes: R_ds_on current sense (valid only while the low side conducts, short
pulses), dead time (~0.35 V of 0.8 V lost), rotor moving during the measurement, the correction factor issue of
simplefoc#572 (point 4). Current PI from these values not tried (gains 4-8x higher, risk of instability).

## 2026-11-22: GD32 driver PR (Candas1/Arduino-FOC#25) on the GD32F130 split board, registers vs dev-gd32

Main supply off, board on the ST-Link 3.3 V, `tools/gd32_drv_regs.py` (TIMER0 + GPIOA/GPIOB configuration,
duty cycle registers excluded), envs `gd32drv_dev` (Candas1 dev-gd32 947a0a2) and `gd32drv_pr`, `DEAD_ZONE` 0.01335.

- **First PR version (34a7fae) disabled SWD**: the AF index was read with the core's `GD_PIN_AF_GET`, whose mask
  (`PIN_AF_MASK` 0xFF at bit 15) includes the channel field (bits 19:23): 18 / 34 instead of 2 for CH1 / CH2, then
  `pin_function` indexes `GD_GPIO_AF[]` (10 entries) out of bounds and `gpio_af_set` writes a garbage AF that spills
  over the next pins of AFSEL1, PA13/PA14 included -> SWD lost ~1 s after start-up (after the bench's 1 s delay).
  Recovery without NRST/BOOT0: pyOCD attach loop (`resume_on_disconnect` False) while the board 3.3 V was replugged,
  caught halted during the 1 s delay (attempt 274), then OpenOCD `halt` + `stm32f1x mass_erase 0` without reset
  (the `program` command failed after the catch: its reset let the firmware run).
- Fix (583bfb4): AF index from bits 15:18 only (checked in the disassembly: `and #0x78000` before `pin_function`).
- **Result: TIMER0 and GPIOB registers identical; GPIOA identical except AFSEL0** (PA4-PA7: 0x49000000 dev,
  0x0C000000 PR). Same core bug in `analogRead(PA4)` (Vbus at start-up): ADC channel 4 in bits 19:23 -> AF index 64,
  garbage AF written for PA4 and spilled over PA5-PA7; the value depends on the flash layout. Harmless here (PA4
  analog, PA5-PA7 inputs) but **any `analogRead` / `pinMode` through the core's pinmap can corrupt the AF of the
  next pins of the same AFSEL register**: worth reporting to CommunityGD32Cores (PinNames.h comment says AF = bits 15:18).
- Dead time read back: CCHP 0x983C (DTCFG 60 cycles) in both. Motor not run yet.
- **Motor running (26 V, Vbus 25.80 V read after a software reset, ST-Link 3.3 V disconnected), PR firmware
  `gd32drv_pr` 583bfb4**: open loop 1.2 V, 0.5 rad/s, 6 s: fundamental 0.679 / 0.706 A (18/11, same test:
  0.70 A), peak 1.44 A, sensor - open loop angle spread 7.3 deg el. Current mode 1 A, 2 rad/s, 6 s: iq 0.995 A,
  id 0.004 A, fundamental 1.000 / 0.969 A (18/11: iq 1.000 A, id ~0). No fault. Same behaviour as dev-gd32.
- **More tests (same day)**, open loop 1.2 V 0.5 rad/s 6 s, 26 V, direction from the slope of the hall angle
  (data/2026-11-22_gd32drv/*.csv, second half):

  | firmware | DRIVER_PINS | fundamental ia / ib [A] | sensor vs open loop |
  |---|---|---|---|
  | dev-gd32 | PA10,PB15, PA9,PB14, PA8,PB13 | 0.671 / 0.654 | same direction (spread 6.3 deg) |
  | dev-gd32 | PA8,PB13, PA9,PB14, PA10,PB15 (A and C swapped) | 0.644 / 0.641 | **same direction**: pin order ignored |
  | PR | normal | 0.648 / 0.670 | same direction (spread 7.4 deg) |
  | PR | A and C swapped | 0.705 / 0.659 | **opposite** (-7.49 vs +7.53 rad el/s): pin order followed |
  | PR, dead_zone 0.02 | normal | 0.424 / 0.331 | same direction, poor tracking (spread 75 deg) |

  dead_zone 0.02: CCHP 0x985A = **90 cycles** (1.25 us) as computed; 30 cycles more than 60 -> ~0.17 V more lost at
  26 V, the 1.2 V open loop barely turns the wheel. Sketches that leave dead_zone at the SimpleFOC default get less
  torque at low voltage than with the fixed 60 cycles.
- **Pin not on TIMER0** (C low = PA6, main supply off): start-up message `GD32-DRV: ERR: pin not on TIMER0: 37`
  (37 = Arduino pin number of PA6), then `ERR-MOT:Init not possible, driver not init`; TIMER0 left unconfigured
  (CTL0, CHCTL2, CCHP = 0), no fault (CFSR 0). The six pins stay GPIO outputs at 0 (BLDCDriver6PWM::init sets
  pinMode OUTPUT before _configure6PWM, all platforms): on this board the low sides are active low -> the low side
  MOSFETs of the valid pins would be ON (motor braked, high sides off, no shoot-through). Pre-existing SimpleFOC
  behaviour, to keep in mind.

## 2026-11-22: GD32F1x0 `_readRegularADCVoltage` (candas-agent feat/gd32-regular-adc), bus voltage at run time

Same API as SimpleFOC dev on STM32 (68120bf): one regular conversion started by software on the current sense ADC.
Bench: `BENCH_VBUS_RUNTIME_PIN` PA4 every 20 ms (gain 31), now also on GD32; `BENCH_VBUS_RUNTIME_ANALOGREAD` reads
with the core's analogRead instead (as GenericVoltageSense in Split_Hoverboard_SimpleFOC). Vbus at start-up still
read with analogRead. 26 V, current mode 1 A, 2 rad/s, 6 s (data/2026-11-22_gd32drv/*.csv):

| firmware | run time read | ADC CTL1.CTN | reads / errors | Vbus run time | iq | ia / ib fundamental |
|---|---|---|---|---|---|---|
| first version e01df09 (gd32drv_adc, run 45069616983) | `_readRegularADCVoltage` | 1 | 1103 / 0 | 25.82 V | **0.878 A** | **0.000** / 1.555 A, peak 2.48 A |
| same, CTN cleared by SWD before the run | `_readRegularADCVoltage` | 0 | 5144 / 0 | 25.90 V | 1.002 A | 1.022 / 0.983 A |
| dev-gd32 (gd32drv_ar_dev) | analogRead | 1 | 680 / 0 | 25.80 V | 1.007 A | 1.020 / 0.972 A |
| fixed 7dee891 (gd32drv_ar_fix) | analogRead | 0 | 691 / 0 | 25.80 V | 1.003 A | 1.002 / 0.975 A |
| fixed 7dee891 (gd32drv_adc) | `_readRegularADCVoltage` | 0 | 683 / 0 | 25.90 V | 1.000 A | 1.012 / 0.985 A |

- The core's first analogRead (start-up Vbus) enables the ADC continuous mode (CTL1 bit 1). With it, the 1.5 cycle
  regular conversion of `_readRegularADCVoltage` ran back to back and **IDATA0 (first injected channel, phase A)
  stayed at 0x800** while IDATA1 was updated. analogRead (7.5 cycles) with the continuous mode on was fine, so the
  Split_Hoverboard_SimpleFOC sketch (GenericVoltageSense at run time) is not affected. Exact mechanism not found.
- Fix: continuous mode off in `_configureADCLowSide` (as the STM32 current sense `_adc_init`) and before each
  `_readRegularADCVoltage` conversion. Cost: +8 B for the fix, `_readRegularADCVoltage` 284 B.
- Vbus: 25.90 V with the regular read (1.5 cycles), 25.80 V with analogRead (7.5 cycles), supply 26.0 V; the
  short sampling reads 0.4 % higher on this divider.
- STM32 for comparison (SimpleFOC dev): `_adc_init` (current sense ADC) sets ContinuousConvMode DISABLE, but
  `_adc_init_regular` (an ADC not used by the current sense) sets ENABLE.

### Later the same evening: stale values, continuous mode, final version based on the core's analogRead path

Stale value check (`BENCH_VBUS_RUNTIME_ALT_PIN` PB1, ~1.64 V, read right after Vbus PA4 at every loop,
`BENCH_VBUS_RUNTIME_MS` 0; a read counted wrong when Vbus is off by >10 % or the alt pin reads the Vbus value):

| version | reads, motor disabled | wrong | current mode 1 A, motor running | wrong | iq, ia / ib |
|---|---|---|---|---|---|
| own conversion, wait for EOC (8cffeb8) | 11874 | **11625** (values swapped) | | | |
| own conversion, EOC without EOIC, CTN off at init (f882ade) | 11810 | 0 | 53323 | 0 | 0.989, 0.999 / 1.000 A |
| analogRead on two pins, CTN off at init (f882ade) | 7350 | **7014** | | | |
| own conversion, CTN off/on around it (60224bb) | | | 11373 | 3 | 0.998, **0.929 / 1.191 A** |
| analogRead on two pins, dev-gd32 | 10046 | 0 | 8661 / 17317 | **3 / 8** | 1.006 / 1.002 A |
| final (fac18af / e988cf8): `get_adc_value` of the core | 6097 | 0 | 19841 | 4 | 0.998, 1.006 / 0.953 A |
| final, mixed with analogRead on PB1 | 6247 | 0 | 7009 | 0 | 1.002, 1.008 / 0.982 A |

- EOC is also set at the end of the injected group (with EOIC): waiting for EOC alone returns the previous regular
  result. The core's analogRead does not clear EOC before its trigger and relies on the continuous mode (set by its
  first call) to return the right channel; with the continuous mode off, analogRead of two pins swaps the values.
- Writing CTL1 while the ADC is on probably starts a conversion (as ADON on STM32F1): turning the continuous mode
  back on restarted 1.5 cycle continuous conversions and disturbed the currents (not proven).
- Final choice (with the user): `_readRegularADCVoltage` = the core's `get_adc_value` (analogRead path, 12 bit,
  without the analogRead resolution). Rules documented: first read (any pin) before `currentSense.init()`; rare wrong
  reads with the motor running (~0.02-0.05 %, same with analogRead on dev-gd32): filter before acting on a value.
- **To check on the STM32F103 (SimpleFOC dev)**: `_readRegularADCVoltage` waits with `HAL_ADC_PollForConversion`
  (EOC): same stale value test with two pins (the 19/11 test read only one pin at a constant voltage).
- Vbus: 25.92 V with the 12 bit value, 25.80 V with analogRead (the core's 12 -> 10 bit mapping rounds down).

### More tests of the two GD32 PRs (same evening, 26 V unless stated)

- **Loop time** (`tools/loop_time.py`, current mode 1 A 2 rad/s, XP x5, data/2026-11-22_gd32drv/loop_time.csv):
  dev-gd32 loopFOC 174.2 / total 218.7 us; PR #25 174.1 / 218.6 us (unchanged). PR #26 with one read every 20 ms:
  total avg 230.0, max 1209 us; two reads every loop: total avg 1793, max 9027 us. analogRead every 20 ms on
  dev-gd32: total avg 228.4, max 996 us. **One read (core path) ~0.8 ms**: PC sampling (two reads every loop, motor
  disabled): 83 % in the slow flash (>= 0x08008000), pin_function 35 % (pinMode at each read), pinmap_find_peripheral
  11 %, pinmap_find_function 11 %, get_adc_value 11 %, pin_in_pinmap 7 %. Split_Hoverboard_SimpleFOC calls
  battery.update() (GenericVoltageSense -> analogRead, not rate limited) at every loop: probably +0.8 ms per loop.
- **PWM 20 / 25 kHz** (DEAD_ZONE 0.01667 / 0.02084, motor disabled): CAR 0x708 / 0x5A0 = 20000 / 25000 Hz,
  dead time 60 cycles, CH3CV / CAR = 0.980 in both.
- **First regular read after currentSense.init(), motor running** (gd32drv_adc_late: no start-up analogRead, reads
  only while enabled): iq 1.000 / 0.997 A, ia / ib 1.007 / 0.978 and 1.015 / 0.995 A, 610 reads, 0 errors, no fault.
- **Bus voltage at 20 / 23 / 26 V** (reset after each change, motor disabled): analogRead at start-up 19.97 / 22.99 /
  25.82 V, `_readRegularADCVoltage` 20.08 / 23.08 / 25.95 V (12 bit; the core's 10 bit mapping rounds down).
- **dead_zone 0.01 = 45 cycles** (Split_Hoverboard_SimpleFOC value, CCHP 0x982D): supply current (supply display)
  0.04 A motor disabled, 0.08 A motor enabled at 0 V for 25 s with 60 cycles and the same 0.08 A with 45 cycles:
  no measurable shoot-through. Open loop 1.2 V 0.5 rad/s: fundamental 0.940 / 0.948 A (60 cycles: 0.648 / 0.670 A,
  90 cycles: 0.424 / 0.331 A). No fault.

## 2026-11-22: sampling point sweep, GD32F130 split board (R_ds_on low side current sense, PR #25 firmware)

`tools/sampling_sweep.py`: the driver's `sampling_point` (TIMER0 CH3, fraction of the PWM period, center-aligned
counting down) written through SWD, motor disabled, then open loop voltage mode 0.5 rad/s for 4 s (currents only
measured). 26 V, 16 kHz (CAR 2250 ticks at 72 MHz: 0.01 of the period = 0.3125 us), SVPWM, 60 cycles dead time.
data/2026-11-22_gd32drv/sampling_sweep*.csv. Reference: 0.65-0.70 A at 1.2 V, 1.31-1.39 A at 1.5 V.

- Coarse, 1.2 V: 0.40-0.55 wrong (up to 13 A read, bench overcurrent cut, fault 2); 0.60 to 0.995 all correct with the
  same noise (residual 0.15-0.23 A).
- Fine: first correct value ia / ib: 1.2 V: <= 0.555 / 0.565; 1.5 V: 0.565 / 0.575.
- SVPWM peak duty 0.5 + 0.866 U / Vbus = 0.540 (1.2 V), 0.550 (1.5 V): the limit moves with it. Time left before the
  low side MOSFET turns off, after the ADC trigger: **ia (first injected conversion) >= 0.47 us, ib (second) >= 0.78 us**
  (resolution 0.16 us).
- Split_Hoverboard_SimpleFOC: driver.voltage_limit 0.95 Vbus caps the duty at 0.95: with sampling_point 0.98, 0.94 us
  left > 0.78 us, the currents stay valid at full voltage (motor.voltage_limit 0.58 Vbus = SVPWM linear limit). The
  margin is in us: at 25 kHz 0.03 of the period is 0.6 us < 0.78 us (lower driver.voltage_limit or sampling_point
  0.99-0.995, both measured correct). Only tested at 0.7-1.3 A; ringing at higher currents not seen (oscilloscope).

## 2026-11-23: dev-gd32 merged with simplefoc dev (candas-agent update/dev-gd32-to-dev 98f890b), GD32F130 board

Merge of simplefoc dev ae7b3ad (56 commits) into Candas1 dev-gd32 fda8110: no conflict, the hardware API implemented
by the GD32 files is the same in master and dev; after the merge the branch = dev + the 4 GD32 files (+487 lines).
Envs gd32drv_head (fda8110) / gd32drv_up (98f890b), cloud run 45128358641, 26 V:
- Flash 47876 -> 47644 B (-232 B), RAM same. Registers: identical (TIMER0.CTL0 0xB1/0xA1 = DIR bit, read 6 times:
  changes with the counting direction).
- Open loop 1.2 V 0.5 rad/s: 0.657 / 0.656 A -> 0.673 / 0.659 A. Current mode 1 A 2 rad/s: iq 0.998 -> 0.999 A,
  id 0.007 -> -0.007 A. No fault.
- **Loop time: loopFOC 174.2 -> 163.3 us (-6.3 %), total 218.7 -> 207.8 us**: generic dev changes (double literals
  and conversions replaced by float, 0542fb7 / 4ed8c6e, probably).

## 2026-11-23: GD32 driver PHASE_OFF did not disable channel 0 (candas-agent fix/gd32-phase-off-ch0 2df1be4)

`_isChannelEnabled` returns the raw enable bit (1 / 0x10 / 0x100 for CH0 / CH1 / CH2) and the PHASE_OFF branch
tested `!= 1`. `tools/phase_state.py` (CHCTL2 motor disabled, enabled at 0 V, disabled again):
- dev-gd32 b44e45c (gd32drv_up): 0x188D / 0x1DDD / 0x188D: **CH0 + CH0N stay enabled when disabled** (phase C, PA8 /
  PB13 on the split board, keeps switching at 0 % = low side on), CH1 / CH2 off.
- fix `!= 0` (gd32drv_ch0, cloud 45130525364): 0x1888 / 0x1DDD / 0x1888: all three phase channels off when disabled
  (CH3 = ADC trigger stays on, no pin).
- Motor with the fix: current mode 1 A 2 rad/s iq 1.002 A, id 0.004 A, ia / ib 1.001 / 0.978 A; open loop 1.2 V
  0.691 / 0.686 A. Same as before. Trapezoidal modulation (PHASE_OFF per sector) not tested.
- **Wheel turned by hand, motor disabled** (user): with dev-gd32 one phase catches (low side of phase C on, current
  through it and the body diodes of the other low sides); with the fix the wheel turns freely.
- **Trapezoid_120, open loop 1.2 V 0.5 rad/s, 5 s** (`tools/trap_test.py`, bench overcurrent cut raised to 100 A for
  the run with the user's OK: the floating phase reads nonsense with R_ds_on sensing and tripped it at once; the
  script cuts on the driven phases only, 5 reads in a row; supply 1 A limit):
  | | CH0 (C) / CH1 (B) / CH2 (A) off | ic rms while C should float (sectors 1, 4) |
  |---|---|---|
  | dev-gd32 | **0 %** / 34 % / 34 % | **1.796 A** (442 reads) |
  | fix | 34 % / 33 % / 34 % | 0.471 A (400 reads; 0.286 A in a first shorter run: noise of ia + ib) |

## 2026-11-23: GD32 cleanup point 3, static functions and gd32_timer_sampling_point (candas-agent cleanup/gd32-static-sampling-point ddc2745)

Envs gd32cl_base (dev-gd32 02b2e02) / gd32cl_3, cloud runs 45135285658 / 45136186253, 26 V.
- Flash 48204 -> 48188 B (-16 B): `_findTimerPin` and `_deadTimeRegister` inlined in `_configure6PWM`; a clone of
  `_setSinglePhaseState` without the unused `channel2` argument (`_writeDutyCycle6PWM` 0xB4 -> 0xB0 B).
- Registers (`tools/gd32_drv_regs.py`, motor disabled): identical, except TIMER0.CTL0 0xB1 / 0xA1 = counting
  direction bit (6 reads of the same firmware: 5 x 0xB1, 1 x 0xA1).
- Loop time (`tools/loop_time.py`, current mode 1 A 2 rad/s, A/B alternated, local data/gd32cl3_loop.csv):
  loopFOC base 163.2 / 163.0 us, point 3 163.1 / 163.0 us; total 207.5 / 207.5 vs 207.7 / 207.3 us: no difference.
- Point 1 (cleanup/gd32-commented-code d1fc833): firmware.bin byte-identical to the base (48204 B).
- tools/sampling_sweep.py: writes gd32_timer_sampling_point, or sampling_point in older firmwares.

## 2026-11-23: GD32 cleanup point 4, SIMPLEFOC_GD32_ADC_INTERRUPT (candas-agent cleanup/gd32-adc-interrupt 9db70b8)

Envs gd32cl4_base (dev-gd32 f930f7d) / gd32cl_4 / gd32cl_4it (= gd32cl_4 + `-D SIMPLEFOC_GD32_ADC_INTERRUPT`),
cloud run 45138317372, 26 V. `ADC_USE_INTERRUPT` (commented define) becomes the build flag, filter
`PHASE_CURRENT_FILTER` removed (raw values, as STM32), `adc_val` static volatile [3], only with the flag.
- Without the flag: firmware.bin byte-identical to the base (48188 B). With the flag: 48404 B (+216 B), RAM +12 B;
  `ADC_CMP_IRQHandler` of the driver replaces the weak one of the core. First time this path was ever compiled.
- Motor disabled (pyOCD): ADC CTL0 0x180 (EOICIE set), NVIC ISER0 bit 12 set; `adc_val` follows IDATA0/1
  (e.g. 2320/2270), 3rd value 0 (2 current pins).
- Loop time (`tools/loop_time.py`, current mode 1 A 2 rad/s, alternated, local data/gd32cl4_loop.csv):
  loopFOC with interrupt 168.0 / 167.9 us, without 162.9 / 163.0 us (+5 us, +3 %: ~3.4 interrupts per loop at
  16 kHz); total 213.6 / 213.5 vs 207.2 / 207.5 us. Max loopFOC 217 / 239 us with vs 174.5 us without: not explained
  (an interrupt costs a few us; maybe micros() read across the interrupt), not investigated.
- Currents (`tools/ol_run.py`, same point): ia/ib fundamental 1.007/0.996 A without, 1.009/0.973 A with; residual
  0.197/0.224 vs 0.195/0.227 A; sensor - open loop angle 70.1 vs 70.0 deg el.: same.
- Board left on gd32cl_4it, 26 V, motor disabled. Merged as Candas1/Arduino-FOC#32 (a13d04d on dev-gd32).

## 2026-11-23: GD32 cleanup point 5, driver and current sense limited to the GD32F1x0 (candas-agent cleanup/gd32-f1x0-only 225b739)

Candas1/Arduino-FOC#33 (merged, 812ae3d on dev-gd32). Guards `_GD32_DEF_ && GD32F1x0` in driver gd32_mcu.h/.cpp and current sense gd32_mcu.h.
Cloud run 45141797236, no flashing:
- GD32F130 bench firmware: `gd32cl5_base` (dev-gd32 a13d04d) and `gd32cl_5`: firmware.bin byte-identical, 48 188 B.
- GD32F103RC check sketch (src/gd32check, envs `gd32chk_base` / `gd32chk_5`): dev-gd32 compiles on the F103 and links
  the GD32 `_configure6PWM` (T) and `timer_break_config` (text 30 308 B): init would succeed without PWM
  (PIN_MODE_AF = input on F10x). With the PR: generic weak `_configure6PWM` (W), no GD32 timer code (text 27 920 B).
  Not run (no GD32F103 board). `_readRegularADCVoltage` call on F103 not compiled.

## 23/11 — GD32 cleanup point 6 (readability), compile only
- candas-agent cleanup/gd32-readability: b3624d1 (A: local params variables, no break after return, `{0}`),
  6fc43e8 (B: `_driverSyncLowSide` fails if driver params are 0, SIMPLEFOC_DRIVER_INIT_FAILED or not 6PWM).
- Cloud 45144925553: gd32cl6_base = gd32cl_6a byte-identical (48 188 B); with SIMPLEFOC_GD32_ADC_INTERRUPT
  gd32cl6_base_it = gd32cl_6a_it (48 404 B). gd32cl_6 48 204 B (+16), only `_driverSyncLowSide` differs in the
  disassembly. Not flashed yet.
- C (e1f98f8: unused `resolution`/`channel2` parameters and `_UNUSED(phase_state)` removed, TIMER_CCX_ENABLE, comment),
  cloud 45145849155: gd32cl_6c byte-identical to gd32cl_6 (48 204 B); gd32cl_6c_it 48 420 B, disassembly vs
  gd32cl6_base_it: same instructions except `_driverSyncLowSide` (B) and the mangled name of `_setSinglePhaseState`.
- On the board (26 V, motor disabled, never run): flashed gd32cl6_base then gd32cl_6c (OpenOCD). `gd32_drv_regs.py
  --compare`: identical except TIMER0.CTL0 DIR bit (0xA1/0xB1, toggles by itself in center-aligned mode, re-read 6x).
  CHCTL2 0x1888 on both (CH3EN set: `_driverSyncLowSide` passed the new check); ADC IDATA0/1 converting
  (~0x90A / 0x8DF on both). Loop code still below 0x08008000 (`__fast_end` 0x080069A4, +16 B after
  `_driverSyncLowSide`). Failure path of B (other driver type) not run. loopFOC not measured (motor would run).
  Board left on gd32cl_6c.
- Loop time (`tools/loop_time.py`, motor run 1 A 2 rad/s with the user's go, alternated, data/gd32cl6_loop.csv):
  loopFOC base 163.3 / 162.9 us, gd32cl_6c 162.9 / 163.0 us; max 174.6 / 174.3 vs 174.9 / 174.7 us; total
  207.8 / 207.4 vs 207.2 / 207.6 us: no difference. Board left on gd32cl_6c, 26 V, motor disabled.
- Merged as Candas1/Arduino-FOC#34 (5b70df8 on dev-gd32). gd32cl_6c = same code as 5b70df8.

## 23/11 — GD32 cleanup point 2 (whitespace), compile only
- candas-agent cleanup/gd32-whitespace 13ddb4e, base Candas1 dev-gd32 5b70df8. Two .cpp files: tabs -> 2 spaces,
  one space before `=` (the user's choice, as in the STM32 F1 code), trailing spaces and doubled blank lines removed.
  `git diff -w` = 5 removed blank lines only.
- Cloud 45148693458: gd32cl2_base / gd32cl_2 48 204 B, _it 48 420 B, _pb12 (`TIMER_BLDC_EMERGENCY_SHUTDOWN_PIN=PB12`)
  48 236 B: all three pairs byte-identical (cmp). Not flashed (no need). Polarity flags not built.

## 23/11 : GD32 PHASE_HI / PHASE_LO (candas-agent fix/gd32-phase-hi-lo 3c8b8ef vs dev-gd32 952ebb5)
Firmware src/phasecheck (driver only), cloud run 45188355727, board on the ST-Link 3.3 V (main supply off, Vbus read
1.60 V), duty 0.3, tools/phase_hilo.py (CHCTL2 + 400 samples of the pin levels). Low side pins active low.
- dev-gd32: HI and LO = ON (CHCTL2 0x0DDD, both pins of each phase switching). ON 0x0DDD, OFF 0x0888 (pins 0 % / 100 %).
- PR: ON 0x0DDD and OFF 0x0888 unchanged. HI 0x0999: high pin ~31 % high, low pin 100 % high (low MOSFET off).
  LO 0x0CCC: high pin 0 %, low pin ~73 % high, i.e. low MOSFET on ~27 % = the duty cycle (not 1 - duty as in ON):
  with CHxEN = 0 and CHxNEN = 1 the timer outputs the reference without inversion (same timer bits as STM32 hardware
  6PWM, which also disables only the high channel). Mixed HI/LO/OFF and LO/OFF/HI: each phase as above.
- Flash 15 984 -> 15 972 bytes. Loop time not measured (2 CHCTL2 reads per phase instead of 1).
- Board back on gd32cl_6c (cloud 45145849155). Flash with OpenOCD (pyOCD -t cortex_m does not program the flash).

## 23/11 : GD32 PHASE_HI / PHASE_LO under power (26 V, firmware src/phasebrake, gd32_brake_pr, cloud 45189684826)
Fixed modes, the three phases in the same state, wheel turned by hand: OFF free; ON 0 % brakes very strongly; LO 0 %
free; LO 95 % brakes weakly at hand speed, more when faster (boost: brakes once the back-EMF exceeds ~5 % of the bus,
~4 rad/s), supply current dropped (energy back to the bus); HI 0 % free. LO 95 % pins under power: high sides 0 %,
low sides on 95.1 % (CH compare 2137 / 2250). Added to Candas1/Arduino-FOC#37.

## 23/11 : SimpleFOC example gd32_split_hoverboard on the board (env gd32_example_split, cloud 45191788080)
Example copied unchanged from dev-gd32 952ebb5 (flash 49 600 bytes, RAM 4 680), 26 V, driven over SWD
(tools/example_swd.py; its Commander is on Serial2, FTDI test left for later).
- Init: motor_ready, driver and current sense initialised, hall alignment moved the motor: zero angle 2.094 rad,
  direction CCW (= known 2.09 / CCW). Offsets ia 1.640 V, ib 1.610 V.
- Target 0: iq mean -0.003 A std 0.169 A (peaks -0.67 / +0.35), id std 0.094 A, uq mean +0.31 V std 0.11 V
  (R_ds_on sense noise -> audible hiss, loop in the audible range).
- Loop period (age of LPF_current_q.timestamp_prev, 59 halted snapshots): ~465 us = ~2.1 kHz (loopFOC + move +
  command.run, default flash placement, no fast math; bench loopFOC 163 us).
- Torque 0.3 A, 2 s: iq 0.27-0.33 A, uq 0.86 V, wheel did not move. 0.5 A: wheel started, 8.6 rad/s after 0.8 s
  (cut by the tool), uq 3.11 V (~0.36 V per rad/s, bench 0.354).
- Board back on gd32cl_6c (cloud 45145849155), 26 V.

## 23/11 : PHASE_HI / PHASE_LO fix in normal operation (envs gd32ph_dev 952ebb5 / gd32ph_pr 3c8b8ef, cloud 45194097902)
Bench firmware, 26 V, data/2026-11-23_phase. Flash 48 204 -> 48 196 B (-8), RAM same.
- Registers (phase_state.py): 0x1888 / 0x1DDD / 0x1888 on both.
- Current mode 1 A 2 rad/s (fundamental from ol_run CSV): ia 1.019 / 1.021 A, ib 0.964 / 0.990 A. No fault.
- Open loop 1.2 V 0.5 rad/s, 3 runs each, in run order: dev 0.697/0.675, 0.691/0.703, 0.705/0.713 A;
  PR 0.676/0.642, 0.676/0.679, 0.716/0.684 A (ia/ib): ranges overlap, run-to-run spread ~±3 % (as for #27).
- Loop time (loop_time.py, 1 A 2 rad/s, alternated dev/PR/dev/PR): loopFOC 162.9 / 163.0 vs 163.4 / 163.3 us
  (+0.4 us, +0.25 %), total 207.4 / 207.4 vs 207.9 / 207.8 us; max loopFOC 174.3-174.9 on both.
- Board back on gd32cl_6c, 26 V, motor disabled.

## 23/11 : GD32E230 build check (envs gd32e230_example_dev / gd32f130_example_dev, dev-gd32 66eb425, cloud 45249459019)
Example gd32_split_hoverboard (src/example_split), build only. Both builds succeed, but on genericGD32E230C8 the GD32
driver is not compiled (condition `GD32F1x0`): `_configure6PWM`, `_configureADCLowSide`, `_writeDutyCycle6PWM`,
`_driverSyncLowSide`, `_readADCVoltageLowSide` are the weak stubs (`W` in nm), so the firmware would do nothing.
- arm-none-eabi-size text/data/bss: E230 53 924 / 936 / 3 768, F130 48 644 / 940 / 3 768 (E230 +5.3 kB with
  the driver missing: Cortex-M23 code and core, to look at once the driver builds).
- Board photo (simplefoc-work #39): MCU marked E230K8 = GD32E230K8, QFN32. No PB2, PB8-PB15, PC13-15 on this
  package: none of RoboDurden's E230 layouts (2-3-1..3, 2-4-1, 2-x-y, all LQFP48) fits. TIMER0 complementary
  outputs in QFN32 (core variant): only PA7 / PB0 / PB1 (CH0N-CH2N, AF2), with PA8-PA10. ADC: PA0-PA7, PB0, PB1.

## 23/11 : pin scan validated on the known split board (env pinscan_f130c8, cloud 45268772654)
26 V off, ST-Link 3.3 V. Time base 71 999 450 Hz. Classification matches the known pinout (example_split):
PA8-10 (high side) held low, PB13-15 (low side, active low) held high, halls PB11 / PF1 / PC14 held high (board
pull-ups), PB0 / PB1 driven at 1.94 / 1.97 V (current op amp at rest), PB2 (self hold) low, PA4 (bus voltage) 58 mV,
PA5 high 3.29 V. The core analogRead returns the previous conversion: first build showed every ADC value one pin
late; fixed by reading twice. Toggle mode (--all) started, then single pins (--pin) through OpenOCD: no usable scope reading (Rigol DS1104Z
over USB, tools/scope_pin.py). Ground clip on the serial connector GND gave a noisy, offset reference (3.3 V pin
read 4.16 V, then 2.14 V with the clip on the ST-Link GND wire): the scope setup, not the scan, is in doubt.
Next: a "touch" mode (pin levels watched by the MCU, point touched through 1 kOhm to GND or 3.3 V), no scope.
Board back on gd32cl_6c (cloud 45145849155), 26 V off.

## 2026-11-24: GD32F130 `_readRegularADCVoltage`, cycles of each step (Candas1/simplefoc-work#22)

Envs `adcdiag_dev` (Candas1 dev-gd32 66eb425) and `adcdiag_fast` (same, with the core's analog.cpp, pinmap.c,
wiring_*.c and PinMap_ADC in the first 32 KB; the SPL gd32f1x0_adc/gpio stayed in the slow flash: archive members,
the linker pattern did not match them). Cloud run 45467965800. Command `XA`: each step called 64 times alone, DWT
cycles minus the timer overhead, motor disabled, current sense initialised, 3.3 V only (Vbus 1.79 V read before
flashing). 72 MHz. PA4. Two runs each, identical within 1 cycle. Raw output: data/2026-11-24_adcdiag/.

| step (mean cycles) | dev (core in slow flash) | fast (core in fast flash) |
|---|---|---|
| `_readRegularADCVoltage` | **44726 (621 us)** | **7250 (101 us)** |
| `analogRead` | 44595 | 7123 |
| DIGITAL_TO_PINNAME | 4 | 4 |
| pin_in_pinmap | 139 (min 66) | 66 |
| pinMode INPUT_ANALOG | 13030 | 540 |
| get_adc_value | 27407 | 371 |
| - pinmap_peripheral | 144 (min 71) | 71 |
| - get_adc_channel | 6394 | 93 |
| - conversion (SPL, slow flash in both) | 201 | 202 |
| float scale | 107 | 107 |
| `_readRegularADCVoltage`, irq off | 44777 | 7255 |

- The slow flash is 84 % of a read (44.7k -> 7.3k cycles when the core code is in the first 32 KB), as the PC
  sampling of 22/11 (83 %). The penalty depends on the code shape: the small search loops (pin_in_pinmap,
  pinmap_peripheral) and the SPL conversion cost the same in both, get_adc_channel (switch) is 69x slower, pinMode
  24x, the body of get_adc_value ~20k cycles. The 0.8 ms of 22/11 was a loop time difference with the motor running.
- The conversion itself (trigger, EOC wait, read) is ~200 cycles (2.8 us); interrupts are not the cause.
- Not explained: the full read costs ~6k cycles (fast) / ~4k (dev) more than the sum of its steps timed alone. Only
  in the sequence pinMode -> get_adc_value; candidate: the EOC wait (continuous mode on, CTL1 0x1E9003) when the pin
  was just reconfigured. To measure before choosing (pinMode + get_adc_value together, EOC wait loop count).
- ADC registers: CTL0 0x100, CTL1 0x1E9003 (ADCON, CTN, injected external trigger), SAMPT1 0x1000, RCU_CFG0 0x804800A.

### Same day, second XA run: sequences, alternated pins, EOC wait (cloud 45470046275, Vbus 1.80 V)

| mean cycles | dev | fast |
|---|---|---|
| `_readRegularADCVoltage` | 51047 | 5161 |
| pinMode alone / get_adc_value alone | 23462 / 21176 | 537 / 370 |
| pinMode + get_adc_value | 46678 | 4968 |
| pinMode + conversion | 31908 | 4805 |
| get_adc_value PA4 then PB1 (pair) | 42481 | 779 |
| conversion with EOC counters, alone / after pinMode | 8506 / 36107 | 231 / 4838 |

- **The EOC wait never waits**: EOC already set before the trigger 64/64 after pinMode (and 34-53/64 otherwise, even
  right after clearing it), 0 to 0.5 loop turns. With the continuous mode on, EOC is set by the continuous
  conversions (and the injected group): the read returns the last finished conversion, whatever its channel.
- **Stale values when the read is fast**: fast build, get_adc_value PA4 then PB1 gives 79 / 78 (PB1 ~2452 expected);
  with EOC cleared first 2454 / 78 (each read returns the other pin). The dev build gives 80 / 2452 (right) only
  because ~20k cycles of slow flash pass between the channel write and the read; with EOC cleared first it is also
  swapped (2451 / 78). So the current path is right only because it is slow; a faster read needs a real wait for a
  conversion of the new channel.
- The ~4.3k extra cycles of pinMode followed by a conversion (fast build) are not in the EOC loop (0 turns): one
  access stalls, not found which. Not needed for the plan (pinMode goes away).
- Slow flash timings depend on the layout: adding XA code moved dev from 44.7k to 51k cycles per read (pinMode
  13k -> 23k, pin_in_pinmap 139 -> 4279). Compare only builds with the same layout.
- First RTT attach after an OpenOCD flash can read dbg.magic 0 (start-up not finished): retry after ~3 s.

## 2026-11-24 evening: #22 branch logic tests (candas-agent issue-22-gd32-regular-adc-fast 8d3104e)

Cloud 45475203942 (dev) / 45475673639 (branch). 3.3 V only (Vbus 1.80-1.82 V), motor disabled. Same layout pairs:
dev / br (gd32f130_mcu.cpp in the fast flash as the bench, core + SPL in the slow flash), fast / br_fast (core code in
the fast flash too, SPL still slow). br_late = br without the start-up analogRead. Raw: data/2026-11-24_adcdiag/t_*.txt.

| mean cycles | dev | br | fast | br_fast | br_late |
|---|---|---|---|---|---|
| `_readRegularADCVoltage` (same pin) | 66412 | **9115** | 11392 | **860** | 26383 |
| pair PA4 + PB1 | 158496 | **22488** | 22946 | **1745** | 78354 |
| analogRead | 64454 | 42599 | 11253 | 9166 | 57603 |
| pin not analog: pinMode(INPUT) + read, minus pinMode | 70596 | 28396 | 13455 | 11701 | 43347 |
| first call | - | - | - | - | 121652 (CTL1 0x9001 -> 0x1E9003, 0.063 V) |

- **Our function: 0 wrong / 0 x -1 in 10000 alternated PA4/PB1 reads and in the series mixed with analogRead, in all
  layouts** (references 0.062 / 1.97-1.98 V). XA twice on br: identical.
- XAT (br, ADC off): -1 x3, one ERR message, 32118 cycles (446 us) per failing call; PB15 (not ADC): -1 + message.
- Registers after the reads: CTL1 0x1E9003 unchanged, SAMPT0 0, SAMPT1 0x8001000 (7.5 cycles on ch4 and ch9 only).
- Gain: 7.3x in the slow flash (127 us; plan expected < 1500 cycles), 13x when the core code is fast (12 us).
  The remaining cycles in br are the SPL calls (adc_flag_get in the wait loop, channel config) and pinmap_function,
  all in the slow flash; the layout still moves the numbers (br_late 26k).
- **Side effect: the core's analogRead(PB1) interleaved with our reads of PA4 returned the PA4 value 3170/3334
  (br_fast) and 3247/3334 (br_late) times, 0 in br, dev and fast.** analogRead reads RDATA right after its channel
  write (EOC is set by the running conversions): it is right only if enough time passes in between, which depends on
  the layout and on the ADC state left by the previous read. Same mechanism as get_adc_value pair 78/78 (fast builds).
- bench-runner flashed once after a `nan` Vbus read (board at 3.3 V, 1.80 V just before and after): rule tightened.

### XAR (cloud 45480983501): the core's analogRead alone, PA4 / PB1 alternated x10000, then the mixed series

| firmware | analogRead alone, wrong pin1 / pin2 | mixed: our reads wrong / analogRead wrong |
|---|---|---|
| adcdiag_br_fast | **9885 / 9848** | 0 / 3277 of 3334 |
| adcdiag_br_late | 0 / 0 | 0 / 0 (3247 in the previous layout) |

- **The core's analogRead is wrong on its own** when it runs fast enough (br_fast layout): the wrong reads of the
  mixed series are not caused by our function. Whether analogRead is right depends on the layout (br_late: 0 now,
  3247 before XAR moved the code).
- Stopped after 2 of 5: adcdiag_br_late has no start-up Vbus read (BENCH_NO_VBUS_STARTUP), so its `X` gives nan and
  bench-runner could not check the bus before the next flash (correct stop). Board left on adcdiag_br_late.

XAR, the other three firmwares (owner confirmed 26 V off for the flash from adcdiag_br_late; then 1.80 V reads):

| firmware | analogRead alone, wrong pin1 / pin2 | our function (dev: the core path) alternated, wrong | mixed: function / analogRead |
|---|---|---|---|
| adcdiag_br | 0 / 0 | 0 / 0 | 0 / 0 |
| adcdiag_fast (dev-gd32, core fast) | **9900 / 9863** | **9826 / 9824** | 9881+6565 / 3291 |
| adcdiag_dev | 0 / 0 | 0 / 0 | 0 / 0 |

- dev-gd32's `_readRegularADCVoltage` (core path) is wrong ~98 % of the time as soon as the core code runs fast,
  like analogRead: confirms the diagnostic (right only because slow). The branch: 0 wrong in all 5 layouts.
- Board back on gd32cl_6c (1.81 V).

## 2026-11-24 night: #22 power test, stopped on dev-gd32 (no current)

adcpwr_dev (Candas1 dev-gd32 66eb425, Vbus PA4 + PB1 read at every loop), 26 V (25.96 V at start-up),
`ol_run.py --mode current --current 1 --speed 2 --duration 6`: **iq 0.004 A, uq pinned at 4.0 V, ia/ib fundamental
0.002 / 0.009 A, sensor angle frozen (rotor did not turn)**. Vbus run time 26.07 V, 32074 reads, 0 errors, PB1 1.975 V,
0 stale. No fault. Raw: data/2026-11-24_adcpwr/. The branch was not run; board left on adcpwr_dev, motor disabled.
- Not caused by #22 (dev-gd32 baseline). The board's known-good firmware gd32cl_6c uses e1f98f8; 66eb425 adds 4
  commits incl. #37 (PHASE_HI / PHASE_LO drive one side only), never motor-tested here. To isolate before going on.
- Step 1, known-good gd32cl_6c (e1f98f8), same run: **same result** (iq -0.001 A, uq 4.0 V / ud 3.85 V, ia/ib 0.015 /
  0.011 A, sensor angle constant 5.764). So the bench, not the firmware: with ~5.5 V applied and ~0 A, the phases
  are not driven or not connected; the hall angle does not move either. Owner to check the motor and hall connectors.
- Motor connected, step 1 again on gd32cl_6c (Vbus 25.80 V): ol_run sent ME1, then the first dbg read failed with
  `pyOCD TransferError: STLink error (18): AP error`; ME0 failed too (link lost), the heartbeat stopped the motor;
  a new connection then read ME0 Status 0, no fault. First run tonight with real phase current: SWD disturbed by the
  motor currents? (ST-Link wiring changed for the E230 boards on 23/11.) No measurement.
- Retry (Vbus 25.81 V): same AP error during the ME1 drive, at the first dbg reads; heartbeat stopped the motor, ME0
  Status 0 after reconnecting. Two in a row: SWD link vs motor currents. Stopped for the night (power test of #22 to
  redo once the ST-Link wiring is checked).
- Last retry with the SWD clock at 1 MHz (BENCH_SWD_HZ, 7c6a775): same AP error at the first dbg read after ME1.
  Systematic, not random. Changes since the good runs of 22/11: ST-Link wiring (E230 work), ST-Link firmware V2J37,
  Rigol on USB (ground loop through its probe ground if still clipped on the board?). See simplefoc-work#47.
- **Other ST-Link** (pyOCD ID 54FF67065072495023531087): no AP error, the link held during the run -> #47 is the
  first probe (or its V2J37 firmware). But gd32cl_6c, same run: **iq 0.43 A, uq pinned at 4.0 V**, ia / ib
  fundamental 0.43 / 0.36 A, peak 1.57 A, no fault (reference 18/11: iq 0.999 A at uq 0.83 V). ~10x the normal
  voltage for half the current: a phase badly connected (motor just reconnected)? Stopped, owner to check.
- Phases reconnected, second ST-Link: **AP error again at the first dbg read after ME1** (ol_run.py line 88, so the
  motor was enabled; heartbeat stopped it; ME0 Status 0 after reconnecting). The second probe only held while a phase
  was badly connected (0.43 A): the link fails when the full phase current flows, with either probe -> not the probe.
- 22/11 firmware gd32drv_dev (947a0a2, iq 1.0 A on 22/11), same run: **the link held**, but **iq 0.42 A, uq 4.0 V**,
  ia / ib 0.42 / 0.35 A, peak 1.44 A, no fault: same as gd32cl_6c with the "bad phase" (0.43 A). Two firmwares give the
  same low iq at the voltage limit -> the bench changed since 22/11 (phase wiring or current sense), not the firmware.
  Link failures not reproduced with this firmware (one run). Motor runs stopped until the owner checks (supply current,
  wheel force, phase-phase resistance ~0.34 Ohm expected).
- Same 22/11 firmware again, owner watching the supply: **the supply was in current limit** during the run, and the
  link failed (AP error at line 88). Likely cause of the whole evening: the bus collapses in current limit (low iq at
  uq 4 V: the board assumes 26 V) and the ground/bus jumps break SWD. Current limit probably lowered on 23/11 (E230).

### #22 power test done (supply limit back to 1 A), cloud 45485161661, 25.8 V, ol_run 1 A 2 rad/s 6 s + loop_time

| | control (947a0a2) | adcpwr_dev (66eb425) | adcpwr_br (3d3dd26) |
|---|---|---|---|
| iq / id [A] | 1.000 / 0.002 | 1.000 / 0.000 | 1.004 / 0.004 |
| uq / ud [V] | 1.447 / -0.493 | 1.431 / -0.530 | 1.447 / -0.487 |
| ia / ib fundamental [A] | 1.003 / 0.982 | 1.017 / 0.932 | 1.022 / 0.957 |
| peak [A] | 1.76 | 1.70 | 1.95 |
| loopFOC avg / max [us] | - | 163.1 / 174.6 | 163.3 / 175.0 |
| **total loop avg / max [us]** (Vbus + PB1 read every loop) | - | **1483.9 / 8679** | **508.5 / 579** |
| Vbus run time, reads, errors | - | 25.90 V, 23822, 0 | 25.90 V, 65415, 0 |
| PB1 wrong (stale) reads | - | 3 (0.013 %) | 16 (0.024 %, cumulative incl. rest) |

- Currents identical. Loop 2.9x faster: ~145 us per read (br) vs ~630 us (dev), as the logic test.
- Stale reads while the motor runs remain (~0.01-0.02 %, the 22/11 range was 0.02-0.05 %): not 0 as at rest. The
  4-end argument assumes at most 2 injected groups and that an interrupted regular conversion restarts. Small counts.
- No link error, no fault. Board back on gd32cl_6c. uq 1.45 V (vs 0.83 V on 18/11) in all three: not firmware.

## 2026-11-25 - _isset overloads vs simplefoc#595 (code only, Candas1/simplefoc-work#3)
- Cloud build 45580002625, envs perf_dev_f103 (ae7b3ad) / perf_pins_f103 (d88e569) / perf_isset_f103 (61cfdbd).
- Flash 90300 / 90180 / 90020 B, RAM unchanged. getPhaseCurrents and _readADCInjectedChannelVoltage: same machine
  code in pins and isset. No __aeabi_i2f / fcmp left in _readRegularADCVoltage, BLDCDriver6PWM::enable / init,
  _configure6PWM (still there with #595). loopFOC / move unchanged (float parameters).
- Not measured on the bench yet.
