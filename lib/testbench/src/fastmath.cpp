/**
 * Faster replacements of the weak SimpleFOC math functions, for MCUs without FPU (GD32F130: soft float).
 * Same results as SimpleFOC foc_utils.cpp (same table and interpolation), less float work:
 *  - _normalizeAngle: no fmodf (libm, slow in soft float), one multiply and one truncation
 *  - _sin/_cos/_sincos: angle scaled once to an integer index (65536 per turn), cos = index + 1/4 turn,
 *    so _sincos does one float multiply instead of two plus the float add of _cos.
 * Enabled with -D BENCH_FAST_MATH (strong symbols override the weak ones of the library).
 */
#ifdef BENCH_FAST_MATH
#include <Arduino.h>
#include "common/foc_utils.h"

static const uint16_t sine_array[65] = {0,804,1608,2411,3212,4011,4808,5602,6393,7180,7962,8740,9512,10279,11039,11793,12540,13279,14010,14733,15447,16151,16846,17531,18205,18868,19520,20160,20788,21403,22006,22595,23170,23732,24279,24812,25330,25833,26320,26791,27246,27684,28106,28511,28899,29269,29622,29957,30274,30572,30853,31114,31357,31581,31786,31972,32138,32286,32413,32522,32610,32679,32729,32758,32768};

// sine of a 16 bit angle index (65536 per turn), scaled by 32768, interpolated like SimpleFOC _sin
static inline int32_t sin_index(uint32_t i) {
  int32_t frac = i & 0xff;
  i = (i >> 8) & 0xff;
  int32_t t1, t2;
  if (i < 64)       { t1 = sine_array[i];        t2 = sine_array[i + 1]; }
  else if (i < 128) { t1 = sine_array[128 - i];  t2 = sine_array[127 - i]; }
  else if (i < 192) { t1 = -sine_array[i - 128]; t2 = -sine_array[i - 127]; }
  else              { t1 = -sine_array[256 - i]; t2 = -sine_array[255 - i]; }
  return t1 + (((t2 - t1) * frac) >> 8);
}

// angle [rad] to index; through int32 so negative angles wrap too (|a| < 200000 rad)
static inline uint32_t angle_index(float a) {
  return (uint32_t)(int32_t)(a * (64 * 4 * 256.0f / _2PI));
}

float _sin(float a) {
  return (1.0f / 32768.0f) * sin_index(angle_index(a));
}

float _cos(float a) {
  return (1.0f / 32768.0f) * sin_index(angle_index(a) + 64 * 256);
}

// The loop calls _sincos several times with the same electrical angle (Park in getFOCCurrents, inverse
// Park in setPhaseVoltage, dead-time compensation): the last result is kept and reused.
// Compared bit for bit (no float comparison); main loop only (not reentrant from interrupts).
void _sincos(float a, float* s, float* c) {
#ifdef BENCH_SINCOS_CACHE
  static uint32_t last_bits = 0x7FC00000u;  // NaN: never equal to a real angle
  static float last_s, last_c;
  uint32_t bits;
  memcpy(&bits, &a, sizeof(bits));
  if (bits == last_bits) { *s = last_s; *c = last_c; return; }
#endif
  uint32_t i = angle_index(a);
  *s = (1.0f / 32768.0f) * sin_index(i);
  *c = (1.0f / 32768.0f) * sin_index(i + 64 * 256);
#ifdef BENCH_SINCOS_CACHE
  last_bits = bits; last_s = *s; last_c = *c;
#endif
}

float _normalizeAngle(float angle) {
  float a = angle - _2PI * (float)(int32_t)(angle * (1.0f / _2PI));
  if (a < 0) a += _2PI;
  if (a >= _2PI) a -= _2PI;
  return a;
}
#endif
