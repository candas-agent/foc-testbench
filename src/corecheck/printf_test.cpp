// stm32duino PR #3117 check: printf() without Serial must still link (_write now lives in the core archive)
#include <Arduino.h>
#include <stdio.h>
void setup() {}
void loop() { printf("hello %d\n", (int)millis()); delay(500); }
