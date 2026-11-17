/**
 * FOC test bench - interactive firmware
 *
 * PWM dead-time compensation on a B-G431B-ESC1 with a hoverboard motor, no position sensor (open loop only).
 * Starts with the motor DISABLED, velocity open loop, voltage mode, target 0, compensation disabled.
 * Serial commands: see README. Hardware setup and safety: lib/testbench.
 */
#include "bench.h"

void setup() {
  bench_setup();
  Serial.println(F("Ready, motor DISABLED (ME1 to enable). Velocity open loop, target 0. 'D' prints the dead-time settings."));
}

void loop() {
  bench_loop();
}
