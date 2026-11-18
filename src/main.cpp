/**
 * FOC test bench - interactive firmware
 *
 * PWM dead-time compensation with a hoverboard motor on a B-G431B-ESC1 or a GD32F130 split board,
 * no position sensor used (open loop only).
 * Starts with the motor DISABLED, velocity open loop, voltage mode, target 0, compensation disabled.
 * Commands (serial on the ESC1, RTT on the GD32 board): see README. Hardware setup and safety: lib/testbench.
 */
#include "bench.h"

void setup() {
  bench_setup();
  bench_io.println(F("Ready, motor DISABLED (ME1 to enable). Velocity open loop, target 0. 'D' prints the dead-time settings."));
}

void loop() {
  bench_loop();
}
