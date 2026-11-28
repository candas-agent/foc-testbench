// stm32duino PR #3117 check: size of a bare Blink
#include <Arduino.h>
void setup() { pinMode(PC13, OUTPUT); }
void loop() { digitalWrite(PC13, !digitalRead(PC13)); delay(500); }
