/**
 * Static tests: the motor does NOT move (driver disabled, phases off).
 *   pio test -e esc1 -f test_static
 */
#include <stdio.h>
#include <unity.h>
#include "bench.h"

void setUp() {}
void tearDown() {}

// current sense offsets: with the driver disabled the measured currents must be close to 0
void test_current_offset() {
  float sa = 0, sb = 0, sc = 0;
  const int n = 200;
  for (int k = 0; k < n; k++) {
    PhaseCurrent_s i = currentSense.getPhaseCurrents();
    sa += i.a; sb += i.b; sc += i.c;
    _delay(1);
  }
  char msg[80];
  snprintf(msg, sizeof(msg), "offsets [mA]: a=%d b=%d c=%d", (int)(1000 * sa / n), (int)(1000 * sb / n), (int)(1000 * sc / n));
  TEST_MESSAGE(msg);
  TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, sa / n);
  TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, sb / n);
  TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, sc / n);
}

// dead-time compensation: the voltage added on each phase must follow the phase current direction,
// with the linear ramp below the threshold. setPhaseVoltage is called with the driver disabled
// (phases stay off in SinePWM/SVPWM), only the computed Ua/Ub/Uc are compared.
void test_compensation_sign() {
  TEST_ASSERT_FALSE(motor.enabled);
  TEST_ASSERT_EQUAL(FOCMotorStatus::motor_ready, motor.motor_status);

  // save the state
  FOCModulationType mod = motor.foc_modulation;
  int8_t centered = motor.modulation_centered;
  TorqueControlType tc = motor.torque_controller;
  float comp = motor.deadtime_compensation, th = motor.deadtime_comp_threshold;
  DQCurrent_s ic = motor.deadtime_comp_current;

  // sine PWM centered: fixed common mode, the differences are exactly the compensation
  motor.foc_modulation = FOCModulationType::SinePWM;
  motor.modulation_centered = 1;
  motor.torque_controller = TorqueControlType::foc_current;  // gain without the resistance
  motor.deadtime_comp_threshold = 0.3f;
  const float dc = 0.2f, iq = 1.0f;

  for (int k = 0; k < 24; k++) {
    float angle = k * _2PI / 24;
    motor.deadtime_comp_current.d = 0;
    motor.deadtime_comp_current.q = iq;

    motor.deadtime_compensation = 0;
    motor.setPhaseVoltage(0.5f, 0, angle);
    float a0 = motor.Ua, b0 = motor.Ub, c0 = motor.Uc;

    motor.deadtime_compensation = dc;
    motor.setPhaseVoltage(0.5f, 0, angle);

    // expected phase currents (inverse Park + Clarke of iq)
    float Ia = -sinf(angle) * iq;
    float Ib = -sinf(angle - _2PI / 3) * iq;
    float Ic = -sinf(angle + _2PI / 3) * iq;
    float t = motor.deadtime_comp_threshold;
    TEST_ASSERT_FLOAT_WITHIN(0.02f, dc * _constrain(Ia / t, -1.0f, 1.0f), motor.Ua - a0);
    TEST_ASSERT_FLOAT_WITHIN(0.02f, dc * _constrain(Ib / t, -1.0f, 1.0f), motor.Ub - b0);
    TEST_ASSERT_FLOAT_WITHIN(0.02f, dc * _constrain(Ic / t, -1.0f, 1.0f), motor.Uc - c0);
  }

  // zero current setpoint -> no compensation
  motor.deadtime_comp_current.q = 0;
  motor.deadtime_compensation = 0;
  motor.setPhaseVoltage(0.5f, 0, 1.0f);
  float a0 = motor.Ua;
  motor.deadtime_compensation = dc;
  motor.setPhaseVoltage(0.5f, 0, 1.0f);
  TEST_ASSERT_FLOAT_WITHIN(1e-5f, a0, motor.Ua);

  // restore
  motor.foc_modulation = mod;
  motor.modulation_centered = centered;
  motor.torque_controller = tc;
  motor.deadtime_compensation = comp;
  motor.deadtime_comp_threshold = th;
  motor.deadtime_comp_current = ic;
  driver.setPwm(0, 0, 0);
}

void setup() {
  _delay(2000);
  bench_setup();
  UNITY_BEGIN();
  RUN_TEST(test_current_offset);
  RUN_TEST(test_compensation_sign);
  UNITY_END();
}

void loop() {}
