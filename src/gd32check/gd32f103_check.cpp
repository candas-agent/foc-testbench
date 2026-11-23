// GD32 cleanup point 5: SimpleFOC on a GD32 family other than the GD32F1x0 (here GD32F103RC).
// Compile check only (no GD32F103 board on the bench): with the GD32 driver limited to the GD32F1x0,
// driver.init() must use the generic 6PWM function and fail (driver.initialized == 0).
// The GD32 timer functions of the driver (timer_break_config...) must not be in the ELF.
#include <Arduino.h>
#include <SimpleFOC.h>

BLDCMotor motor(15);
BLDCDriver6PWM driver(PA8, PB13, PA9, PB14, PA10, PB15);
LowsideCurrentSense current_sense(0.0035f, -8.0f, PA0, PA1, _NC);

void setup() {
  Serial.begin(115200);
  SimpleFOCDebug::enable(&Serial);
  driver.voltage_power_supply = 26;
  driver.init();
  current_sense.linkDriver(&driver);
  current_sense.init();
  Serial.print("driver.initialized=");
  Serial.println(driver.initialized);
  Serial.print("current_sense.initialized=");
  Serial.println(current_sense.initialized);
}

void loop() {}
