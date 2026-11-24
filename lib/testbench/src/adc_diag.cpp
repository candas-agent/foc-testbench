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

void adc_diag(Print& out) {
  if (motor.enabled) { out.println(F("Disable the motor first (ME0)")); return; }
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
}
#endif
