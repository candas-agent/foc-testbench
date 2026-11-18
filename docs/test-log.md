# Test log

One entry per test session. Keep the CSV exports in `data/<date>/` (ignored by git by default, add them explicitly if you want to share them).

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
