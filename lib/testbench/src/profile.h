#pragma once
/**
 * Loop profiler (compile with -D BENCH_PROFILE): CPU cycles of each part of bench_loop, averaged over
 * PROF_WINDOW loops, read with pyOCD (symbol prof) or printed by the XP command.
 * XB (-D BENCH_MICROBENCH, ~3 KB of flash): micro benchmarks of the float primitives and SimpleFOC blocks
 * (motor must be disabled). Only indicative: the benchmark loops run from the slow flash on the GD32F130.
 * Cycle counter: DWT CYCCNT of the Cortex-M3/M4 (72 MHz on the GD32F130, 170 MHz on the ESC1).
 */
#include <Arduino.h>

#ifdef BENCH_PROFILE
#define PROF_MAGIC  0x9F0F0003u
#define PROF_WINDOW 256
enum ProfPart { P_OBS, P_FOC, P_MOVE, P_DBG, P_SAFETY, P_REC, P_CMD, P_TOTAL, P_N };
struct ProfData {
  uint32_t magic;
  uint32_t windows;          // number of published windows
  uint32_t cpu_hz;           // SystemCoreClock
  uint32_t avg[P_N];         // mean cycles per loop of each part (last window)
  uint32_t max[P_N];         // max cycles of each part (last window)
  uint32_t micros_back;      // number of times _micros() went backwards between two loops
  uint32_t micros_back_us;   // largest backward step [us]
};
extern "C" volatile ProfData prof;

void prof_init();
static inline uint32_t prof_cycles() { return *(volatile uint32_t*)0xE0001004; }  // DWT_CYCCNT
void prof_add(int part, uint32_t cycles);
void prof_end_loop();
void prof_print(Print& out);
void prof_bench(Print& out);
void adc_diag(Print& out);  // XA, adc_diag.cpp (-D BENCH_ADC_DIAG)
#define PROF_T0()        uint32_t _pt = prof_cycles(), _pt0 = _pt
#define PROF_LAP(part)   do { uint32_t _n = prof_cycles(); prof_add(part, _n - _pt); _pt = _n; } while (0)
#define PROF_TOTAL()     do { prof_add(P_TOTAL, prof_cycles() - _pt0); prof_end_loop(); } while (0)
#else
#define PROF_T0()
#define PROF_LAP(part)
#define PROF_TOTAL()
#endif
