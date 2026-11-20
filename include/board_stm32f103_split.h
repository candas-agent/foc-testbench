#pragma once
// ---------------------------------------------------------------------------
// Hoverboard board gen 1 (STM32F103RC, single motor used: the "right" one)
// Pins from Candas1/Split_Hoverboard_SimpleFOC include/defines_1-0.h, settings from its src/main.cpp
// Commands and messages: SEGGER RTT through the ST-Link (pyOCD, tools --port rtt)
// NOT TESTED ON HARDWARE YET
// ---------------------------------------------------------------------------
#define BENCH_BOARD_NAME          "STM32F103 hoverboard"

// Power stage
#define SUPPLY_VOLTAGE      26.0f   // [V] set the REAL measured supply voltage, the compensation volts are scaled with it
#define BENCH_PWM_FREQUENCY 16000   // [Hz] as in the hoverboard sketch
#define DEAD_ZONE           0.01f   // dead-time as a fraction of the PWM period (hoverboard sketch value)

// Driver: 6PWM on TIM1
//                          phase G (A)   phase B (B)   phase Y (C)
#define DRIVER_PINS         PA10, PB15,   PA9, PB14,    PA8, PB13

// Low side current sense on the R_ds_on of the low side MOSFETs, 2 phases (the 3rd from Kirchhoff)
#define CS_SHUNT            0.008f  // [Ohm] R_ds_on of the low side MOSFETs
#define CS_GAIN             (-2.5f)
#define CS_PINS             PC5, PC4, _NC
// R_ds_on sensing is only valid while the low side MOSFET conducts: driver enabled during the offset calibration
#define CS_CALIBRATE_DRIVER_ENABLED

// Power latch: must be held high or the board switches itself off
#define SELF_HOLD_PIN       PA5

// Bus voltage: not read at start-up (VBUS_PIN), but at run time every 20 ms through SimpleFOC dev's
// _readRegularADCVoltage() (regular conversion on the current sense ADC). PC2 = ADC12_IN12 and gain 33 from the
// commented VBAT / BATTERY_GAIN of defines_1-0.h: to check against the real supply voltage.
#define BENCH_VBUS_RUNTIME_PIN  PC2
// gain 33 (EFeru default BAT_CALIB 3970/1492) read 29.2-29.5 V motor stopped for 26.0 V on the supply display
// (STM32F103 board, 2026-11-19): 33 * 26.0 / 29.3 = 29.3
#define BENCH_VBUS_RUNTIME_GAIN 29.3f

// No FPU (software floats), same choices as the GD32F130 board
#define OBSERVER_DEFAULT_ON 0
#define CURRENT_BANDWIDTH   100.0f  // [Hz]
// measured 2026-11-19 (static vector, phase B current): 0.5 V -> 0.42 A, 0.7 V -> 0.89 A, 0.9 V -> 1.54 A,
// i.e. ~0.35 V lost then ~3.2 A/V: 0.8 V gives ~1.5 A (1.5 V would give ~3.7 A, near the 4 A cut)
#define ALIGN_VOLTAGE       0.8f    // [V]

// Hall sensors of the motor (defines_1-0.h), interpolated by the SmoothingSensor of SimpleFOC drivers.
// Alignment from the hoverboard sketch config.h (STM32F103RC): initFOC does not move the motor.
// To be checked on this board with SA (THE MOTOR MOVES).
#define HALL_A_PIN          PC10
#define HALL_B_PIN          PC11
#define HALL_C_PIN          PC12
#define SENSOR_ZERO_ANGLE   2.09f   // [rad] MOTOR_zero_electric_offset
#define SENSOR_DIRECTION    Direction::CCW
#define TORQUE_TARGET_MAX   1.0f    // [A] closed loop torque mode: target clamped (a free wheel accelerates)
// Hall table test (lib/testbench/src/hall_table.*, command ST): rotor electrical angle [deg] of the lower edge and
// width of each hall state (index = state 1..6), measured 2026-11-20 with tools/hall_calib.py on the STM32F103 board,
// and the ideal 60 deg grid fitted to the same edges (offset 28 deg) for comparison.
#define BENCH_HALL_TABLE_LOWER  {0, 208.8f, 322.9f, 266.3f,  92.8f, 150.3f,  26.9f, 0}
#define BENCH_HALL_TABLE_WIDTH  {0,  57.5f,  64.0f,  56.6f,  57.5f,  58.5f,  65.9f, 0}
#define BENCH_HALL_IDEAL_LOWER  {0, 208.0f, 328.0f, 268.0f,  88.0f, 148.0f,  28.0f, 0}
#define BENCH_HALL_IDEAL_WIDTH  {0,  60.0f,  60.0f,  60.0f,  60.0f,  60.0f,  60.0f, 0}
#define DRIVER_VOLTAGE_LIMIT_RATIO 0.95f

// Commands and messages: SEGGER RTT through the ST-Link
#define BENCH_USE_RTT

// Recorder (R command): 48 KB of RAM, 16 bytes per sample
#define RECORD_SAMPLES      500
