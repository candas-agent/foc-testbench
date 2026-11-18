#pragma once
// ---------------------------------------------------------------------------
// Test bench configuration - adapt to your motor and power supply
// ---------------------------------------------------------------------------

// Board: power stage, pins, current sense, communication, recorder size (selected by the PlatformIO env)
#if defined(BOARD_GD32F130_SPLIT)
#include "board_gd32f130_split.h"
#else
#include "board_esc1.h"
#endif

// Motor (hoverboard motor, values from the MXLEMMING observer README example) - no position sensor
#define POLE_PAIRS          15
#define PHASE_RESISTANCE    0.1664f      // [Ohm] phase resistance
#define KV_RATING           17.0f        // [rpm/V]
#define PHASE_INDUCTANCE    0.00036858f  // [H]

// Limits - the phase resistance is very low: I ~ U / R, 1 V is already ~6 A !
#define ALIGN_VOLTAGE       0.5f    // [V] rotor alignment before the dead-time calibration (~3 A)
#define OPENLOOP_VOLTAGE    0.3f    // [V] voltage amplitude in open loop (~1.8 A at standstill)
// In open loop voltage mode the amplitude must exceed the back-EMF: ~0.32 V per rad/s with KV 17 and
// 15 pole pairs, so 0.3 V is only enough up to ~0.5 rad/s. Use the current mode (MT2) for higher speeds.
#define FOC_VOLTAGE_LIMIT   4.0f    // [V] voltage limit of the current controllers
#define CURRENT_LIMIT       2.0f    // [A] also the current amplitude in open loop with foc_current
#define VELOCITY_LIMIT      10.0f   // [rad/s]

// Controllers
#define CURRENT_BANDWIDTH   300.0f  // [Hz] current PI tuned from R and L (tuneCurrentController)
#define CURRENT_LPF_TF      0.002f  // [s] current low pass filter

// Dead-time compensation defaults (disabled at start-up)
#define DEADTIME_COMP       0.0f    // [V]
#define DEADTIME_THRESHOLD  0.2f    // [A]
#define CALIB_MAX_CURRENT   1.5f    // [A] default max current for the dead-time calibration

// Safety
#define RUN_TIMEOUT_S         30      // [s] the motor is disabled after this time enabled (0 = no timeout)
#define OVERCURRENT_A         4.0f    // [A] any phase above this for OVERCURRENT_SAMPLES consecutive loops -> disable
#define OVERCURRENT_SAMPLES   10
#define HEARTBEAT_TIMEOUT_MS  2000    // [ms] when the heartbeat is on (XH1), disable if no command received for this time
#define CALIB_MAX_CURRENT_LIMIT 2.0f  // [A] the calibration current is clamped to this value
