// Minimal Arduino.h for host (PC) tests of SimpleFOC pure functions
#pragma once
#include <stdint.h>
#include <math.h>
#include <string.h>
#ifndef min
#define min(a,b) ((a)<(b)?(a):(b))
#endif
#ifndef max
#define max(a,b) ((a)>(b)?(a):(b))
#endif
