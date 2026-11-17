#pragma once
// ---------------------------------------------------------------------------
// Test bench configuration - adapt to your motor and power supply
// ---------------------------------------------------------------------------

// Motor (hoverboard motor, values from the MXLEMMING observer README example) - no position sensor
#define POLE_PAIRS          15
#define PHASE_RESISTANCE    0.1664f      // [Ohm] phase resistance
#define KV_RATING           17.0f        // [rpm/V]
#define PHASE_INDUCTANCE    0.00036858f  // [H]

// Power stage
#define SUPPLY_VOLTAGE      12.0f   // [V] set the REAL measured supply voltage, the compensation volts are scaled with it
#define PWM_FREQUENCY       25000   // [Hz]
#define DEAD_ZONE           0.02f   // dead-time as a fraction of the PWM period (SimpleFOC default)

// Limits - the phase resistance is very low: I ~ U / R, 1 V is already ~6 A !
#define ALIGN_VOLTAGE       0.5f    // [V] rotor alignment before the dead-time calibration (~3 A)
#define OPENLOOP_VOLTAGE    0.3f    // [V] voltage amplitude in open loop (~1.8 A)
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
