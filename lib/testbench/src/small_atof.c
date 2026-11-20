/**
 * Small atof() / atoi() replacing the newlib ones (GD32F130: 64 KB flash, 8 KB RAM).
 *
 * Commander only calls atof() and atoi(), but newlib's atof() is strtod(): double precision, big numbers,
 * hexadecimal, locale... ~8 KB of flash and ~460 bytes of RAM (locale, malloc). These versions parse
 * "[spaces][+|-]digits[.digits][e|E[+|-]digits]" in single precision, plenty for typed commands
 * (7 significant digits). No "inf"/"nan"/hexadecimal.
 * Being defined here, the linker does not pull the newlib versions from libc.
 */
#if defined(ARDUINO_ARCH_GD32) || defined(BENCH_SMALL_ATOF)
#include <stdlib.h>

static const char* skip_spaces(const char* s) {
  while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r' || *s == '\f' || *s == '\v') s++;
  return s;
}

double atof(const char* s) {
  s = skip_spaces(s);
  int neg = 0;
  if (*s == '+' || *s == '-') neg = (*s++ == '-');
  // up to 9 significant digits in an integer (exact), the others only move the decimal exponent
  unsigned long mant = 0;
  int digits = 0, exp10 = 0;
  for (; *s >= '0' && *s <= '9'; s++) {
    if (digits < 9) { mant = mant * 10 + (unsigned long)(*s - '0'); if (mant) digits++; }
    else exp10++;
  }
  if (*s == '.') {
    for (s++; *s >= '0' && *s <= '9'; s++) {
      if (digits < 9) { mant = mant * 10 + (unsigned long)(*s - '0'); if (mant) digits++; exp10--; }
    }
  }
  if (*s == 'e' || *s == 'E') {
    const char* e = s + 1;
    int eneg = 0, ev = 0;
    if (*e == '+' || *e == '-') eneg = (*e++ == '-');
    if (*e >= '0' && *e <= '9') {
      for (; *e >= '0' && *e <= '9'; e++) if (ev < 100) ev = ev * 10 + (*e - '0');
      exp10 += eneg ? -ev : ev;
    }
  }
  float v = (float)mant;
  // scale by 10^exp10 (multiplying by 10 is exact up to the float rounding; divide for negative exponents)
  float p = 1.0f;
  int n = exp10 < 0 ? -exp10 : exp10;
  if (n > 45) n = 45;  // beyond the float range: gives 0 or inf
  for (float base = 10.0f; n; n >>= 1, base *= base) if (n & 1) p *= base;
  v = exp10 < 0 ? v / p : v * p;
  return neg ? -v : v;
}

int atoi(const char* s) {
  s = skip_spaces(s);
  int neg = 0;
  if (*s == '+' || *s == '-') neg = (*s++ == '-');
  int v = 0;
  for (; *s >= '0' && *s <= '9'; s++) v = v * 10 + (*s - '0');
  return neg ? -v : v;
}

long atol(const char* s) {
  return atoi(s);
}
#endif
