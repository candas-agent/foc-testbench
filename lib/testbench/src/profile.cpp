#include "profile.h"

#ifdef BENCH_PROFILE
#include "bench.h"

extern "C" volatile ProfData prof;
volatile ProfData prof;
extern "C" uint32_t SystemCoreClock;

static uint32_t sum[P_N], mx[P_N], loops = 0;

void prof_init() {
  *(volatile uint32_t*)0xE000EDFC |= (1u << 24);  // DEMCR.TRCENA
  *(volatile uint32_t*)0xE0001004 = 0;            // DWT_CYCCNT
  *(volatile uint32_t*)0xE0001000 |= 1u;          // DWT_CTRL.CYCCNTENA
  prof.magic = PROF_MAGIC;
  prof.cpu_hz = SystemCoreClock;
}

void prof_add(int part, uint32_t cycles) {
  sum[part] += cycles;
  if (cycles > mx[part]) mx[part] = cycles;
}

void prof_end_loop() {
  static uint32_t last_us = 0;
  uint32_t now = _micros();
  if ((int32_t)(now - last_us) < 0) {
    prof.micros_back = prof.micros_back + 1;
    if (last_us - now > prof.micros_back_us) prof.micros_back_us = last_us - now;
  }
  last_us = now;
  if (++loops < PROF_WINDOW) return;
  for (int k = 0; k < P_N; k++) {
    prof.avg[k] = sum[k] / PROF_WINDOW;
    prof.max[k] = mx[k];
    sum[k] = mx[k] = 0;
  }
  loops = 0;
  prof.windows = prof.windows + 1;
}

static const char* const names[P_N] = {"observer", "loopFOC", "move", "debug", "safety", "recorder", "commands", "TOTAL"};

void prof_print(Print& out) {
  float mhz = prof.cpu_hz / 1e6f;
  out.print(F("CPU [MHz]: ")); out.println(mhz, 1);
  out.print(F("micros backwards: ")); out.print(prof.micros_back); out.print(F(" times, max [us]: ")); out.println(prof.micros_back_us);
  out.println(F("part: avg us / max us"));
  for (int k = 0; k < P_N; k++) {
    out.print(names[k]); out.print(F(": "));
    out.print(prof.avg[k] / mhz, 1); out.print(F(" / "));
    out.println(prof.max[k] / mhz, 1);
  }
}

#ifdef BENCH_MICROBENCH
// ---------------------------------------------------------------------------
// Micro benchmarks: mean cycles per call over N calls (loop overhead removed)
// ---------------------------------------------------------------------------
static volatile float vin = 1.2345f, vin2 = 0.789f;
static volatile float vout;
#define N_BENCH 64

// each call timed alone: min (no interrupt) and mean (with the interrupts), minus the timer overhead
#define BENCH(label, ...) do {                                    \
    uint32_t mn = 0xFFFFFFFFu, sm = 0;                            \
    for (int k = 0; k < N_BENCH; k++) {                           \
      uint32_t t0 = prof_cycles();                                \
      __VA_ARGS__;                                                \
      uint32_t t = prof_cycles() - t0;                            \
      if (t < mn) mn = t;                                         \
      sm += t;                                                    \
    }                                                             \
    out.print(F(label ": min "));                                 \
    out.print((int32_t)(mn - empty));                             \
    out.print(F(" mean "));                                       \
    out.println((int32_t)(sm / N_BENCH - empty));                 \
    delay(20);  /* RTT up buffer of 512 bytes: let the probe read it */ \
  } while (0)


void prof_bench(Print& out) {
  if (motor.enabled) { out.println(F("Disable the motor first (ME0)")); return; }
  uint32_t empty = 0xFFFFFFFFu;
  for (int k = 0; k < N_BENCH; k++) { uint32_t t0 = prof_cycles(); uint32_t t = prof_cycles() - t0; if (t < empty) empty = t; }
  out.print(F("cycles per call, CPU [MHz]: ")); out.println(SystemCoreClock / 1e6f, 1);
  out.print(F("empty loop cycles: ")); out.println(empty);
  BENCH("fadd", vout = vin + vin2);
  BENCH("fmul", vout = vin * vin2);
  BENCH("fdiv", vout = vin / vin2);
  BENCH("i2f", vout = (float)(int32_t)k);
  BENCH("f2i", vout = (float)(int32_t)(vin * 1000.0f));
  BENCH("sqrtf", vout = sqrtf(vin));
  BENCH("_sincos", { float s, c; _sincos(vin, &s, &c); vout = s + c; });
  BENCH("_atan2", vout = _atan2(vin, vin2));
  BENCH("atan2f", vout = atan2f(vin, vin2));

  BENCH("_normalizeAngle", vout = _normalizeAngle(vin * 10.0f));
  BENCH("getPhaseCurrents", { PhaseCurrent_s i = currentSense.getPhaseCurrents(); vout = i.a; });
  {
    CurrentSense* volatile csp = &currentSense;
    BENCH("virtual getPhaseCurrents", { PhaseCurrent_s i = csp->getPhaseCurrents(); vout = i.a; });
    PhaseCurrent_s pc = currentSense.getPhaseCurrents();
    BENCH("getABCurrents", { ABCurrent_s ab = csp->getABCurrents(pc); vout = ab.alpha; });
    ABCurrent_s ab = csp->getABCurrents(pc);
    BENCH("getDQCurrents", { DQCurrent_s dq = csp->getDQCurrents(ab, vin); vout = dq.q; });
  }
  {
    CurrentSense* volatile csp = &currentSense;
    ABCurrent_s ab1; ab1.alpha = 1.0f; ab1.beta = 0.5f;
    BENCH("getDQCurrents const", { DQCurrent_s dq = csp->getDQCurrents(ab1, vin); vout = dq.q; });
    PhaseCurrent_s pc = currentSense.getPhaseCurrents();
    out.print(F("pc: ")); out.print(pc.a, 6); out.print(' '); out.print(pc.b, 6); out.print(' '); out.println(pc.c, 6);
    volatile float va = pc.a;
    BENCH("fmul pc.a", vout = va * vin2);
    BENCH("fadd pc.a", vout = va + vin2);
  }
  BENCH("getFOCCurrents", { DQCurrent_s i = currentSense.getFOCCurrents(vin); vout = i.q; });
  PIDController pid(1.0f, 100.0f, 0, NOT_SET, 4.0f);
  BENCH("PID", vout = pid(vin));
  LowPassFilter lpf(0.002f);
  BENCH("LPF", vout = lpf(vin));
  BENCH("_micros", vout = (float)_micros());
  BENCH("observer.update", observer.update());
  BENCH("sensor update", if (motor.sensor) motor.sensor->update());
  {
    // motor stopped: shaft_velocity ~0 skips the SmoothingSensor phase correction branch.
    // Fake velocities (motor disabled) to time every branch, then restore the real value.
    float v = motor.shaft_velocity;
    motor.shaft_velocity = 1.0f;
    BENCH("sensor update v=+1", if (motor.sensor) motor.sensor->update());
    motor.shaft_velocity = -1.0f;
    BENCH("sensor update v=-1", if (motor.sensor) motor.sensor->update());
    motor.shaft_velocity = v;
  }
  BENCH("electricalAngle", vout = motor.electricalAngle());
  BENCH("computeCompensation", { float a, b, c; computeCompensation(a, b, c); vout = a; });
}
#endif  // BENCH_MICROBENCH
#endif
