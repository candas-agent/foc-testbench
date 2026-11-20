// Minimal SimpleFOC project on the B-G431B-ESC1, no lib_archive line in platformio.ini
#include <SimpleFOC.h>
BLDCMotor motor(7);
BLDCDriver6PWM driver(A_PHASE_UH, A_PHASE_UL, A_PHASE_VH, A_PHASE_VL, A_PHASE_WH, A_PHASE_WL);
LowsideCurrentSense cs(0.003f, -64.0f / 7.0f, A_OP1_OUT, A_OP2_OUT, A_OP3_OUT);
void setup() {
  driver.voltage_power_supply = 12;
  driver.init();
  motor.linkDriver(&driver);
  cs.linkDriver(&driver);
  cs.init();
  motor.linkCurrentSense(&cs);
  motor.init();
  motor.initFOC();
}
void loop() {
  motor.loopFOC();
  motor.move();
}
