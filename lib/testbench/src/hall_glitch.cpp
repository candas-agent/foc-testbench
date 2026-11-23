// Test only (command XG): forces an invalid hall state (000 or 111) as if the sensors glitched, then restores the
// real pin states, to reproduce the dev hall glitch bug on the board (docs/equiv-tests.md). The HallSensor state is private: this file
// (and only this one) opens it.
#include <Arduino.h>
#define private public
#include <SimpleFOC.h>
#undef private

void hall_glitch_set(HallSensor& h, int state) {
  noInterrupts();
  h.A_active = (state >> 2) & 1;
  h.B_active = (state >> 1) & 1;
  h.C_active = state & 1;
  h.updateState();
  interrupts();
}

void hall_glitch_restore(HallSensor& h) {
  noInterrupts();
  h.A_active = digitalRead(h.pinA);
  h.B_active = digitalRead(h.pinB);
  h.C_active = digitalRead(h.pinC);
  h.updateState();
  interrupts();
}
