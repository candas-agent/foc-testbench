// Host (PC) audit of the pure math functions of SimpleFOC (src/common/foc_utils.cpp), float32 like on the MCU.
// Reference: double precision libm. Prints the largest errors and the special cases.
// Note: float -> unsigned conversion of a negative or too large value is undefined in C++ (x86 and ARM differ):
// those cases of _sin / _cos are reported separately and must be checked on the target.
//   g++ -O1 -ffp-contract=off -I stub -I <simplefoc>/src/common audit/math_audit.cpp <simplefoc>/src/common/foc_utils.cpp
#include <stdio.h>
#include <math.h>
#include <float.h>
#include <stdlib.h>
#include "foc_utils.h"

static double PI2 = 2 * M_PI;

static void sin_cos() {
  double max_s = 0, max_c = 0, arg_s = 0, arg_c = 0;
  const int N = 2000000;
  for (int k = 0; k <= N; k++) {
    float a = (float)(PI2 * k / N);
    double es = fabs(_sin(a) - sin((double)a)), ec = fabs(_cos(a) - cos((double)a));
    if (es > max_s) { max_s = es; arg_s = a; }
    if (ec > max_c) { max_c = ec; arg_c = a; }
  }
  printf("_sin on [0, 2PI]: max error %.3g at %.7g\n", max_s, arg_s);
  printf("_cos on [0, 2PI]: max error %.3g at %.7g\n", max_c, arg_c);
  // a bit above 2PI and large positive angles (wrap of the table index)
  double m = 0, am = 0;
  for (int k = 0; k <= N; k++) {
    float a = (float)(PI2 + 400.0 * k / N);
    double e = fabs(_sin(a) - sin((double)a));
    if (e > m) { m = e; am = a; }
  }
  printf("_sin on [2PI, 2PI + 400]: max error %.3g at %.7g\n", m, am);
  float edges[] = {0.0f, -0.0f, _2PI, nextafterf(_2PI, 0), nextafterf(_2PI, 10), _PI_2, _PI, _3PI_2};
  for (float a : edges) printf("   _sin(%.9g) = %.7g (sin %.7g), _cos = %.7g (cos %.7g)\n", a, _sin(a), sin(a), _cos(a), cos(a));
}

static void atan2_audit() {
  double m = 0; float mx = 0, my = 0;
  srand(1);
  for (int k = 0; k < 4000000; k++) {
    float x = (float)(rand() / (double)RAND_MAX * 2 - 1) * powf(10, (float)(rand() % 7 - 3));
    float y = (float)(rand() / (double)RAND_MAX * 2 - 1) * powf(10, (float)(rand() % 7 - 3));
    double e = fabs(_atan2(y, x) - atan2((double)y, (double)x));
    if (e > M_PI) e = fabs(e - PI2);  // +PI / -PI on the negative x axis
    if (e > m) { m = e; mx = x; my = y; }
  }
  printf("_atan2 random: max error %.3g rad at (y %.4g, x %.4g)\n", m, my, mx);
  float sp[] = {0.0f, -0.0f, 1.0f, -1.0f, INFINITY, -INFINITY, NAN, FLT_MIN, 1e-40f};
  for (float y : sp) for (float x : sp) {
    float r = _atan2(y, x); double ref = atan2((double)y, (double)x);
    bool bad = (isnan(r) != isnan(ref)) || (!isnan(r) && fabs(r - ref) > 1e-3 && fabs(fabs(r - ref) - PI2) > 1e-3);
    if (bad) printf("   _atan2(%g, %g) = %g, atan2 = %g\n", y, x, r, ref);
  }
}

static void normalize_audit() {
  long out = 0, at2pi = 0, n = 0;
  float worst = 0;
  srand(2);
  for (int k = 0; k < 4000000; k++) {
    float a = (float)((rand() / (double)RAND_MAX * 2 - 1) * pow(10, rand() % 8 - 2));
    float r = _normalizeAngle(a);
    n++;
    if (!(r >= 0 && r < _2PI)) { out++; if (r == _2PI) at2pi++; worst = a; }
  }
  printf("_normalizeAngle: %ld random angles (|a| up to 1e5), outside [0, 2PI): %ld (equal to 2PI: %ld, e.g. %.9g)\n",
         n, out, at2pi, worst);
  float sp[] = {NAN, INFINITY, -INFINITY, -0.0f, -1e-30f};
  for (float a : sp) printf("   _normalizeAngle(%g) = %g\n", a, _normalizeAngle(a));
}

static void sqrt_audit() {
  double m = 0; float am = 0;
  for (int e = -30; e <= 30; e++)
    for (int k = 0; k < 10000; k++) {
      float x = ldexpf(1.0f + k / 10000.0f, e);
      double rel = fabs(_sqrtApprox(x) / sqrt((double)x) - 1);
      if (rel > m) { m = rel; am = x; }
    }
  printf("_sqrtApprox: max relative error %.3g %% at %g (used by CurrentSense::getDCCurrent)\n", m * 100, am);
  float sp[] = {0.0f, -0.0f, -1.0f, 1e-40f, FLT_MAX, INFINITY, NAN};
  for (float x : sp) printf("   _sqrtApprox(%g) = %g\n", x, _sqrtApprox(x));
}

int main() {
  sin_cos();
  atan2_audit();
  normalize_audit();
  sqrt_audit();
  return 0;
}
