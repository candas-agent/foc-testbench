#pragma once
// ---------------------------------------------------------------------------
// Hoverboard split board TT-SD2.2 "master" (GD32F130C8, single motor)
// Pins from Candas1/Split_Hoverboard_SimpleFOC include/defines_2-0.h
// No ST-Link virtual COM port: commands and messages go through SEGGER RTT (pyOCD, tools --port rtt)
// ---------------------------------------------------------------------------
#define BOARD_NAME          "GD32F130 split board"

// Power stage
#define SUPPLY_VOLTAGE      26.0f   // [V] set the REAL measured supply voltage, the compensation volts are scaled with it
#define BENCH_PWM_FREQUENCY 16000   // [Hz] GD32 driver default
// The GD32 driver ignores dead_zone and sets a fixed DEAD_TIME of 60 timer cycles (gd32_mcu.h):
// 60 / 72 MHz = 0.83 us = 1.33 % of the 62.5 us PWM period. DEAD_ZONE only serves the expected value
// printed by the bench (Vbus * DEAD_ZONE), keep it in sync with DEAD_TIME and the PWM frequency.
#define DEAD_ZONE           0.0133f

// Driver: 6PWM on TIMER0 (channels are hard-coded in the GD32 driver)
//                          phase G (A)   phase B (B)   phase Y (C)
#define DRIVER_PINS         PA10, PB15,   PA9, PB14,    PA8, PB13

// Low side current sense on the R_ds_on of the low side MOSFETs, 2 phases (the 3rd from Kirchhoff)
// Less accurate than a shunt and temperature dependent.
#define CS_SHUNT            0.008f  // [Ohm] R_ds_on of the low side MOSFETs
#define CS_GAIN             (-2.5f)
#define CS_PINS             PB1, PB0, _NC
// R_ds_on sensing is only valid while the low side MOSFET conducts: the driver must be enabled
// (all phases at 50 %, 0 V on the motor) while the current sense measures its offsets.
// Driver disabled during the offset calibration -> offsets ~0.2 V too high -> ~10 A error.
#define CS_CALIBRATE_DRIVER_ENABLED

// Power latch: must be held high or the board switches itself off (bridge the switch connector on the bench)
#define SELF_HOLD_PIN       PB2

// Bus voltage measurement, read ONCE at start-up only: analogRead at run time would share the ADC
// used by the current sense. Gain from the split board sketch (BATTERY_GAIN).
#define VBUS_PIN            PA4
#define VBUS_GAIN           31.0f   // Vbus = ADC voltage * VBUS_GAIN
#define VBUS_TOLERANCE      0.15f   // warn if the measured Vbus differs from SUPPLY_VOLTAGE by more than this

// Commands and messages: SEGGER RTT through the ST-Link
#define BENCH_USE_RTT

// Recorder (R command): 8 KB of RAM only, 16 bytes per sample
#define RECORD_SAMPLES      100
