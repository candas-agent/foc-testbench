#pragma once
/**
 * Test bench core, shared by the interactive firmware (src/main.cpp) and the pio tests (test/).
 *
 * Hardware: hoverboard motor on a B-G431B-ESC1 or a GD32F130 split board (include/board_*.h),
 * no position sensor used (open loop only).
 * Safety: run timeout, overcurrent cut, optional heartbeat (see config.h).
 */
#include <Arduino.h>
#include <SimpleFOC.h>
#include "config.h"
#include "encoders/MXLEMMING_observer/MXLEMMINGObserverSensor.h"
#ifdef HALL_A_PIN
#include "encoders/smoothing/SmoothingSensor.h"
#endif

extern BLDCMotor motor;
extern BLDCDriver6PWM driver;
extern LowsideCurrentSense currentSense;
extern Commander command;
// commands and messages: Serial (ESC1 virtual COM port) or RTT (GD32 board, through the ST-Link)
extern Stream& bench_io;
// MXLEMMING sensorless observer, NOT linked to the motor: runs in parallel to the open loop
extern MXLEMMINGObserverSensor observer;

// Variables read with pyOCD (global, volatile so they are never optimised away)
// The layout is mirrored in tools/board_mem.py: update both together (DBG_MAGIC checks the version).
#define DBG_MAGIC 0xF0CDB005u
struct DebugVars {
  uint32_t magic;                // DBG_MAGIC
  float ia, ib, ic;              // measured phase currents [A]
  float comp_a, comp_b, comp_c;  // dead-time compensation added on each phase [V]
  float iq, id;                  // filtered dq currents [A] (foc_current mode)
  float uq, ud;                  // dq voltages [V]
  float electrical_angle;        // [rad]
  float velocity;                // [rad/s]
  float loop_us;                 // loopFOC period [us]
  float obs_angle;               // observer electrical angle [rad]
  float angle_err;               // observer angle - open loop angle, wrapped to [-pi, pi] [rad]
  uint32_t fault;                // last safety fault (0 none, 1 timeout, 2 overcurrent, 3 heartbeat)
  uint32_t enabled;              // motor enabled
  float vbus;                    // bus voltage measured at start-up [V], NAN if the board has no measurement
  uint32_t hall;                 // hall sensor state (0..7), 0 if the board has no hall sensors
  float sensor_angle;            // electrical angle from the sensor (0 if none) [rad]
  int32_t hall_count;            // hall sectors counted since boot (electric_rotations*6 + sector): position mod 6*pp
};
extern volatile DebugVars dbg;

// test bench state
extern float openloop_voltage;   // open loop voltage amplitude [V]
extern int equal_amplitude;      // 1: open loop voltage lowered by 4/pi*compensation
extern int observer_on;          // MXLEMMING observer updated every loop (DO0/DO1)
extern int sensor_aligned;       // 1 once the hall sensors are linked and aligned (SA): closed loop allowed

// --- main functions ---
void bench_setup();              // hardware init, motor DISABLED at the end
void bench_step();               // loopFOC + move + safety + debug + recorder (no serial commands)
void bench_loop();               // bench_step + serial commands
void bench_disable(const __FlashStringHelper* reason = nullptr);

// --- dead-time compensation ---
// compensation actually applied on each phase, same formula as BLDCMotor::setPhaseVoltage
void computeCompensation(float& ca, float& cb, float& cc);
// runs motor.calibrateDeadtimeCompensation, returns NOT_SET on failure
float bench_calibrate(float max_current);

// --- safety ---
extern uint32_t run_timeout_s;   // 0 = no timeout
extern float overcurrent_a;
extern int heartbeat_on;
extern volatile int fault;       // last fault: 0 none, 1 timeout, 2 overcurrent, 3 heartbeat

// --- recorder ---
// The layout is mirrored in tools/board_mem.py: update both together.
struct Sample {
  uint32_t t_us;     // time since the start of the recording
  float ia;          // measured phase A current [A]
  float comp_a;      // compensation added on phase A [V]
  float angle_err;   // observer angle error [rad]
};
// recorder state, C linkage so tools/board_mem.py finds the symbols in the ELF
extern "C" {
extern Sample rec_buf[RECORD_SAMPLES];
extern volatile int32_t rec_n;
extern volatile int32_t rec_running;
}
void rec_start(uint32_t period_us, bool auto_dump);
bool rec_done();
int rec_count();
const Sample* rec_data();
void rec_dump(Print& out);

// share of the samples where |ia| < 10% of the peak (peak estimated as sqrt(2)*rms, robust to noise)
// a pure sine gives ~0.064, the dead-time plateau around zero makes it larger
float zero_crossing_flatness(const Sample* s, int n);

// ripple (standard deviation) of the observer angle error [rad], around its circular mean
float angle_error_ripple(const Sample* s, int n);
