#pragma once
// ---------------------------------------------------------------------------
// B-G431B-ESC1 (STM32G431, ST-Link V3 on board with virtual COM port)
// ---------------------------------------------------------------------------
#define BENCH_BOARD_NAME          "B-G431B-ESC1"

// Power stage
#define SUPPLY_VOLTAGE      12.0f   // [V] set the REAL measured supply voltage, the compensation volts are scaled with it
#define BENCH_PWM_FREQUENCY 25000   // [Hz]
#define DEAD_ZONE           0.02f   // dead-time as a fraction of the PWM period (SimpleFOC default)

// Driver: 6PWM, pins from the board variant
#define DRIVER_PINS         A_PHASE_UH, A_PHASE_UL, A_PHASE_VH, A_PHASE_VL, A_PHASE_WH, A_PHASE_WL

// Low side current sense with the STM32G4 op-amps
// gain calculation: https://community.simplefoc.com/t/b-g431b-esc1-current-control/521/21
#define CS_SHUNT            0.003f
#define CS_GAIN             (-64.0f / 7.0f)
#define CS_PINS             A_OP1_OUT, A_OP2_OUT, A_OP3_OUT

// Commands and messages: ST-Link virtual COM port
#define BENCH_SERIAL_BAUD   115200

// Recorder (R command): samples stored in RAM, 16 bytes each
#define RECORD_SAMPLES      800
