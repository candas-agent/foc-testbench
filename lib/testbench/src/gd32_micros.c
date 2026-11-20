/**
 * Monotonic micros() for the GD32 Arduino core (link with -Wl,--wrap=getCurrentMicros).
 *
 * The core reads the millisecond counter and SysTick->VAL without checking whether SysTick has just wrapped
 * with its interrupt still pending: micros() then goes back by up to 1 ms (measured on the GD32F130 bench:
 * 7 times in 10 s, up to 930 us). SimpleFOC computes its time steps with unsigned differences of _micros(),
 * so one such step gives a huge dt: the SmoothingSensor then jumps the hall angle by pi/3 electrical for a loop.
 */
#if defined(ARDUINO_ARCH_GD32)
#include <Arduino.h>

extern volatile uint32_t gd_ticks;

uint32_t __wrap_getCurrentMicros(void) {
  uint32_t load = SysTick->LOAD + 1;
  uint32_t ms, val, pending;
  do {
    ms = gd_ticks;
    val = SysTick->VAL;
    pending = SCB->ICSR & SCB_ICSR_PENDSTSET_Msk;
  } while (ms != gd_ticks);  // the tick interrupt ran in between: read again
  // wrapped (VAL reloaded, counting down from LOAD) but the tick interrupt has not run yet
  if (pending && val > load / 2) ms++;
  return ms * 1000 + ((load - val) * 1000) / load;
}
#endif
