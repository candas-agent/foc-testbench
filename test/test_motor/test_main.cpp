/**
 * Motor tests: THE MOTOR MOVES. Current-limited supply, motor fixed, someone next to the bench.
 *   pio test -e esc1 -f test_motor
 *
 * The run timeout and the overcurrent cut of the test bench stay active.
 */
#include <stdio.h>
#include <unity.h>
#include "bench.h"

void setUp() {}
void tearDown() { bench_disable(); }

static float calibrated = NAN;  // result of the calibration test, used by the open loop tests

static void run_ms(uint32_t ms) {
  uint32_t t0 = millis();
  while (millis() - t0 < ms && motor.enabled) bench_step();
}

// records the phase A current over ~4 electrical periods at the given speed, returns the zero crossing flatness
static float measure_flatness(float velocity, float comp) {
  motor.deadtime_compensation = comp;
  motor.target = velocity;
  motor.enable();
  run_ms(2000);  // settle
  TEST_ASSERT_TRUE_MESSAGE(motor.enabled, "motor disabled by the safety during settling");
  float f_el = velocity * POLE_PAIRS / _2PI;
  uint32_t period_us = (uint32_t)(4.0f / f_el * 1e6f / RECORD_SAMPLES);
  rec_start(period_us, false);
  while (!rec_done() && motor.enabled) bench_step();
  bench_disable();
  TEST_ASSERT_TRUE_MESSAGE(rec_done(), "recording interrupted by the safety");
  motor.target = 0;
  return zero_crossing_flatness(rec_data(), rec_count());
}

// calibration repeatable and plausible
void test_calibration() {
  float v[3];
  for (int k = 0; k < 3; k++) {
    v[k] = bench_calibrate(CALIB_MAX_CURRENT);
    TEST_ASSERT_TRUE_MESSAGE(_isset(v[k]) && v[k] > 0, "calibration failed");
    _delay(1000);
  }
  float mean = (v[0] + v[1] + v[2]) / 3;
  float spread = max(v[0], max(v[1], v[2])) - min(v[0], min(v[1], v[2]));
  char msg[100];
  snprintf(msg, sizeof(msg), "calibration [mV]: %d %d %d, ratio to Vbus*dead_zone: %d%%",
           (int)(1000 * v[0]), (int)(1000 * v[1]), (int)(1000 * v[2]), (int)(100 * mean / (SUPPLY_VOLTAGE * DEAD_ZONE)));
  TEST_MESSAGE(msg);
  TEST_ASSERT_TRUE_MESSAGE(spread < 0.15f * mean, "calibration not repeatable (spread > 15%)");
  float ratio = mean / (SUPPLY_VOLTAGE * DEAD_ZONE);
  TEST_ASSERT_TRUE_MESSAGE(ratio > 0.25f && ratio < 4.0f, "calibration far from Vbus*dead_zone");
  calibrated = mean;
}

static void compare(const char* name) {
  if (isnan(calibrated)) TEST_IGNORE_MESSAGE("needs test_calibration");
  float off = measure_flatness(2.0f, 0);
  _delay(1000);
  float on = measure_flatness(2.0f, calibrated);
  char msg[100];
  snprintf(msg, sizeof(msg), "%s |i|<10%% of peak: off %d.%d%%, on %d.%d%% (pure sine 6.4%%)", name,
           (int)(100 * off), (int)(1000 * off) % 10, (int)(100 * on), (int)(1000 * on) % 10);
  TEST_MESSAGE(msg);
  TEST_ASSERT_TRUE_MESSAGE(on < off, "compensation does not reduce the zero crossing plateau");
  motor.deadtime_compensation = 0;
}

// open loop, voltage mode, same fundamental amplitude with and without compensation
void test_openloop_voltage() {
  motor.torque_controller = TorqueControlType::voltage;
  motor.controller = MotionControlType::velocity_openloop;
  equal_amplitude = 1;
  compare("voltage");
  equal_amplitude = 0;
}

// open loop with current control (foc_current in the open loop frame), compensation from the setpoints
void test_openloop_current() {
  motor.torque_controller = TorqueControlType::foc_current;
  motor.controller = MotionControlType::velocity_openloop;
  motor.deadtime_comp_use_measured = 0;
  compare("current");
  motor.torque_controller = TorqueControlType::voltage;
}

void setup() {
  _delay(2000);
  bench_setup();
  UNITY_BEGIN();
  RUN_TEST(test_calibration);
  RUN_TEST(test_openloop_voltage);
  RUN_TEST(test_openloop_current);
  UNITY_END();
}

void loop() {}
