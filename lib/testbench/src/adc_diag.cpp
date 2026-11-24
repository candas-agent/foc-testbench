/**
 * XA (-D BENCH_ADC_DIAG): CPU cycles of each step of _readRegularADCVoltage on the GD32F130
 * (Candas1/simplefoc-work#22). Motor disabled, current sense initialised (injected group running).
 * Each step is timed alone, N_DIAG times: min (no interrupt) and mean, minus the timer overhead.
 * Env adcdiag_fast places this file and the core's ADC / pinmap / GPIO code in the fast flash
 * (first 32 KB): the difference with adcdiag_dev is the cost of the slow flash.
 */
#if defined(BENCH_ADC_DIAG) && defined(BENCH_PROFILE)
#include "profile.h"
#include "bench.h"
#include "analog.h"
float _readRegularADCVoltage(const int pin);  // gd32f130_mcu.cpp (dev-gd32)

#ifndef BENCH_ADC_DIAG_PIN
#define BENCH_ADC_DIAG_PIN PA4
#endif
#ifndef BENCH_ADC_DIAG_PIN2
#define BENCH_ADC_DIAG_PIN2 PB1  // second ADC pin for the alternated reads
#endif
#define N_DIAG 64

static volatile uint32_t sink;
static volatile float fsink;

#define DIAG(label, ...) do {                                     \
    uint32_t mn = 0xFFFFFFFFu, mx = 0, sm = 0;                    \
    for (int k = 0; k < N_DIAG; k++) {                            \
      uint32_t t0 = prof_cycles();                                \
      __VA_ARGS__;                                                \
      uint32_t t = prof_cycles() - t0 - empty;                    \
      if (t < mn) mn = t;                                         \
      if (t > mx) mx = t;                                         \
      sm += t;                                                    \
    }                                                             \
    out.print(F(label ": min ")); out.print(mn);                  \
    out.print(F(" mean ")); out.print(sm / N_DIAG);               \
    out.print(F(" max ")); out.println(mx);                       \
    delay(20);  /* RTT up buffer of 512 bytes */                  \
  } while (0)

// register-only conversion (floor): same sequence as get_adc_value after its first call
static inline uint16_t raw_conversion(uint8_t channel) {
  adc_regular_channel_config(0U, channel, ADC_SAMPLETIME_7POINT5);
  adc_software_trigger_enable(ADC_REGULAR_CHANNEL);
  while (!adc_flag_get(ADC_FLAG_EOC));
  adc_flag_clear(ADC_FLAG_EOC);
  return adc_regular_data_read();
}

// same, counting: EOC already set before the trigger, and the turns of the EOC wait loop
static uint32_t eoc_before, wait_turns;
static inline uint16_t counted_conversion(uint8_t channel, bool clear_first) {
  if (clear_first) adc_flag_clear(ADC_FLAG_EOC);
  adc_regular_channel_config(0U, channel, ADC_SAMPLETIME_7POINT5);
  if (adc_flag_get(ADC_FLAG_EOC)) eoc_before++;
  adc_software_trigger_enable(ADC_REGULAR_CHANNEL);
  while (!adc_flag_get(ADC_FLAG_EOC)) wait_turns++;
  adc_flag_clear(ADC_FLAG_EOC);
  return adc_regular_data_read();
}

// (d) first call after reset: with BENCH_NO_VBUS_STARTUP nothing read the ADC before, the core init runs here
static void first_call(Print& out) {
  static bool done = false;
  if (done) return;
  done = true;
  uint32_t ctl1 = ADC_CTL1;
  uint32_t t0 = prof_cycles();
  float v = _readRegularADCVoltage(BENCH_ADC_DIAG_PIN);
  uint32_t t = prof_cycles() - t0;
  out.print(F("first call: cycles ")); out.print(t); out.print(F(" value ")); out.print(v, 3);
  out.print(F(" CTL1 before 0x")); out.print(ctl1, HEX); out.print(F(" after 0x")); out.println(ADC_CTL1, HEX);
  delay(20);
}

// wrong read: more than 0.3 V from the value of the pin read alone (3 reads in a row, the last one kept)
static float ref_value(int pin) {
  float v = 0;
  for (int i = 0; i < 3; i++) v = _readRegularADCVoltage(pin);
  return v;
}

static void stale_check(Print& out) {
  const int p1 = BENCH_ADC_DIAG_PIN, p2 = BENCH_ADC_DIAG_PIN2;
  float r1 = ref_value(p1), r2 = ref_value(p2);
  out.print(F("stale check, references [V] pin1 ")); out.print(r1, 3); out.print(F(" pin2 ")); out.println(r2, 3);
  const uint32_t n = 10000;
  uint32_t bad1 = 0, bad2 = 0, neg = 0;
  for (uint32_t i = 0; i < n; i++) {
    float v1 = _readRegularADCVoltage(p1), v2 = _readRegularADCVoltage(p2);
    if (v1 < 0 || v2 < 0) neg++;
    if (fabsf(v1 - r1) > 0.3f) bad1++;
    if (fabsf(v2 - r2) > 0.3f) bad2++;
  }
  out.print(F("alternated x")); out.print(n); out.print(F(": wrong pin1 ")); out.print(bad1);
  out.print(F(" pin2 ")); out.print(bad2); out.print(F(" -1 ")); out.println(neg);
  delay(20);
  // mixed with the core's analogRead (10 bit) of pin2 at every third step
  bad1 = bad2 = neg = 0;
  uint32_t bad_ar = 0;
  for (uint32_t i = 0; i < n; i++) {
    float v1 = _readRegularADCVoltage(p1);
    if (v1 < 0) neg++;
    if (fabsf(v1 - r1) > 0.3f) bad1++;
    if (i % 3 == 0) {
      float a = analogRead(p2) * 3.3f / 1024.0f;
      if (fabsf(a - r2) > 0.3f) bad_ar++;
    } else {
      float v2 = _readRegularADCVoltage(p2);
      if (v2 < 0) neg++;
      if (fabsf(v2 - r2) > 0.3f) bad2++;
    }
  }
  out.print(F("mixed with analogRead x")); out.print(n); out.print(F(": wrong pin1 ")); out.print(bad1);
  out.print(F(" pin2 ")); out.print(bad2); out.print(F(" analogRead pin2 ")); out.print(bad_ar);
  out.print(F(" -1 ")); out.println(neg);
  out.print(F("registers: CTL1 0x")); out.print(ADC_CTL1, HEX); out.print(F(" SAMPT0 0x")); out.print(ADC_SAMPT0, HEX);
  out.print(F(" SAMPT1 0x")); out.println(ADC_SAMPT1, HEX);
  delay(20);
}

// XAR: the core's analogRead alone, PA4 / PB1 alternated, then mixed with our reads (same firmware, same layout)
static void analogread_check(Print& out) {
  const int p1 = BENCH_ADC_DIAG_PIN, p2 = BENCH_ADC_DIAG_PIN2;
  int r1 = 0, r2 = 0;
  for (int i = 0; i < 3; i++) r1 = analogRead(p1);
  for (int i = 0; i < 3; i++) r2 = analogRead(p2);
  out.print(F("analogRead check, references [10 bit] pin1 ")); out.print(r1); out.print(F(" pin2 ")); out.println(r2);
  const uint32_t n = 10000;
  uint32_t bad1 = 0, bad2 = 0;
  for (uint32_t i = 0; i < n; i++) {
    if (abs(analogRead(p1) - r1) > 93) bad1++;  // 93 = 0.3 V in 10 bit
    if (abs(analogRead(p2) - r2) > 93) bad2++;
  }
  out.print(F("analogRead alone alternated x")); out.print(n); out.print(F(": wrong pin1 ")); out.print(bad1);
  out.print(F(" pin2 ")); out.println(bad2);
  delay(20);
}

static void timeout_check(Print& out) {
  out.println(F("timeout check: ADC off, 3 reads (expect -1 x3 and one ERR message). Reset the board after."));
  delay(20);
  adc_disable();
  for (int i = 0; i < 3; i++) {
    uint32_t t0 = prof_cycles();
    float v = _readRegularADCVoltage(BENCH_ADC_DIAG_PIN);
    uint32_t t = prof_cycles() - t0;
    out.print(F("read ")); out.print(i); out.print(F(": ")); out.print(v, 3);
    out.print(F(" cycles ")); out.println(t);
    delay(20);
  }
  out.print(F("not an ADC pin (PB15): ")); out.println(_readRegularADCVoltage(PB15), 3);
}

#define COUNTS(label) do {                                                       \
    out.print(F("  " label ": EOC set before trigger ")); out.print(eoc_before);  \
    out.print(F("/" "64, wait turns per call ")); out.println(wait_turns / (float)N_DIAG, 1); \
    eoc_before = 0; wait_turns = 0; delay(20);                                   \
  } while (0)

// XA: everything below; XAS: stale value check only; XAR: analogRead alone, then XAS; XAT: timeout check (turns the ADC off: reset after; branch only,
// dev-gd32 waits forever)
void adc_diag(Print& out, char sub) {
  if (motor.enabled) { out.println(F("Disable the motor first (ME0)")); return; }
  first_call(out);
  if (sub == 'S') { stale_check(out); return; }
  if (sub == 'T') { timeout_check(out); return; }
  if (sub == 'R') { analogread_check(out); stale_check(out); return; }
  uint32_t empty = 0xFFFFFFFFu;
  for (int k = 0; k < N_DIAG; k++) { uint32_t t0 = prof_cycles(); uint32_t t = prof_cycles() - t0; if (t < empty) empty = t; }
  const int pin = BENCH_ADC_DIAG_PIN;
  PinName pn = DIGITAL_TO_PINNAME(pin);
  uint8_t channel = get_adc_channel(pn);
  extern uint32_t __fast_end;
  out.print(F("cycles, CPU [MHz]: ")); out.print(SystemCoreClock / 1e6f, 1);
  out.print(F(" empty ")); out.print(empty);
  out.print(F(" adc_diag @0x")); out.print((uint32_t)&adc_diag, HEX);
  out.print(F(" get_adc_value @0x")); out.print((uint32_t)&get_adc_value, HEX);
  out.print(F(" PinMap_ADC @0x")); out.print((uint32_t)PinMap_ADC, HEX);
  out.print(F(" __fast_end 0x")); out.println((uint32_t)&__fast_end, HEX);
  out.print(F("ADC CTL0 0x")); out.print(ADC_CTL0, HEX); out.print(F(" CTL1 0x")); out.print(ADC_CTL1, HEX);
  out.print(F(" SAMPT1 0x")); out.print(ADC_SAMPT1, HEX); out.print(F(" RCU_CFG0 0x")); out.println(RCU_CFG0, HEX);
  DIAG("_readRegularADCVoltage", fsink = _readRegularADCVoltage(pin));
  DIAG("analogRead", sink = analogRead(pin));
  DIAG("DIGITAL_TO_PINNAME", sink = DIGITAL_TO_PINNAME(pin));
  DIAG("pin_in_pinmap", sink = pin_in_pinmap(pn, PinMap_ADC));
  DIAG("pinMode INPUT_ANALOG", pinMode(pin, INPUT_ANALOG));
  DIAG("get_adc_value", sink = get_adc_value(pn));
  DIAG(" pinmap_peripheral", sink = pinmap_peripheral(pn, PinMap_ADC));
  DIAG(" get_adc_channel", sink = get_adc_channel(pn));
  DIAG(" conversion (SPL)", sink = raw_conversion(channel));
  DIAG(" conversion, irq off", { __disable_irq(); sink = raw_conversion(channel); __enable_irq(); });
  DIAG("float scale", fsink = (float)sink * 3.3f / 4096.0f);
  DIAG("_readRegularADCVoltage, irq off", { __disable_irq(); fsink = _readRegularADCVoltage(pin); __enable_irq(); });

  // where do the extra cycles of the full sequence come from (simplefoc-work#22)
  const int pin2 = BENCH_ADC_DIAG_PIN2;
  PinName pn2 = DIGITAL_TO_PINNAME(pin2);
  uint8_t channel2 = get_adc_channel(pn2);
  out.println(F("-- sequences"));
  DIAG("(c) pinMode INPUT alone", pinMode(pin, INPUT));
  DIAG("(c) pin not analog: pinMode INPUT + read", { pinMode(pin, INPUT); fsink = _readRegularADCVoltage(pin); });
  DIAG("pinMode + get_adc_value", { pinMode(pin, INPUT_ANALOG); sink = get_adc_value(pn); });
  DIAG("pinMode + conversion", { pinMode(pin, INPUT_ANALOG); sink = raw_conversion(channel); });
  DIAG("get_adc_value pin1, pin2 (pair)", { sink = get_adc_value(pn); sink = get_adc_value(pn2); });
  DIAG("_readRegularADCVoltage pin1, pin2 (pair)", { fsink = _readRegularADCVoltage(pin); fsink = _readRegularADCVoltage(pin2); });
  eoc_before = 0; wait_turns = 0;
  DIAG("counted conversion", sink = counted_conversion(channel, false));
  COUNTS("same channel");
  DIAG("counted conversion after pinMode", { pinMode(pin, INPUT_ANALOG); sink = counted_conversion(channel, false); });
  COUNTS("after pinMode");
  DIAG("counted conversion, channel 1/2 alternated", { sink = counted_conversion((k & 1) ? channel2 : channel, false); });
  COUNTS("alternated");
  DIAG("counted conversion, EOC cleared first", sink = counted_conversion(channel, true));
  COUNTS("EOC cleared first");
  // values: does each read return its own pin (PA4 ~Vbus divider, PB1 ~1.64 V)
  uint32_t a = get_adc_value(pn), b = get_adc_value(pn2), c = counted_conversion(channel, true), d = counted_conversion(channel2, true);
  out.print(F("values pin1 ")); out.print(a); out.print(F(" pin2 ")); out.print(b);
  out.print(F(" | EOC cleared first: pin1 ")); out.print(c); out.print(F(" pin2 ")); out.println(d);
  eoc_before = 0; wait_turns = 0;
  stale_check(out);
}
#endif
