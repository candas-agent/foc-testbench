// Pin scan of an unknown GD32F130 QFN32 board (simplefoc-work #39): find which MCU pin goes where with a scope,
// probing only easy points (gate driver inputs, op amp outputs, connectors), never the QFN pads.
// SAFETY: main supply and battery DISCONNECTED, board on the ST-Link 3.3 V only (gate drivers unpowered).
// No pin is ever an output: the firmware only switches the internal pull-up / pull-down (~40 kOhm), so a pin
// tied to anything (op amp output, unpowered driver input) carries less than 100 uA. SWD pins PA13 / PA14 untouched.
// Read and driven over SWD by tools/pin_scan.py through the struct `scan`:
//   at start-up and mode 3: classify every pin (pull-down read, pull-up read, ADC voltage without pull); mode 0: idle
//   mode 1: all pins toggle their pull at their own frequency, f = (base + n) * 100 Hz, base A=10, B=30, F=50
//           (PA7 = 1.7 kHz, PB1 = 3.1 kHz, PF0 = 5.0 kHz): the frequency seen on the scope gives the pin
//   mode 2: only pin index `only` toggles (confirmation), the others stay on pull-down
#include <Arduino.h>

struct PinDef {
  uint32_t arduino;
  uint32_t port;   // GPIOA / GPIOB / GPIOF base address
  uint8_t bit;
  char name;       // 'A', 'B', 'F'
  bool adc;
};

static const PinDef PINS[] = {
  {PA0, GPIOA, 0, 'A', true},  {PA1, GPIOA, 1, 'A', true},  {PA2, GPIOA, 2, 'A', true},
  {PA3, GPIOA, 3, 'A', true},  {PA4, GPIOA, 4, 'A', true},  {PA5, GPIOA, 5, 'A', true},
  {PA6, GPIOA, 6, 'A', true},  {PA7, GPIOA, 7, 'A', true},  {PA8, GPIOA, 8, 'A', false},
  {PA9, GPIOA, 9, 'A', false}, {PA10, GPIOA, 10, 'A', false}, {PA11, GPIOA, 11, 'A', false},
  {PA12, GPIOA, 12, 'A', false}, {PA15, GPIOA, 15, 'A', false},
  {PB0, GPIOB, 0, 'B', true},  {PB1, GPIOB, 1, 'B', true},  {PB2, GPIOB, 2, 'B', false},
  {PB3, GPIOB, 3, 'B', false}, {PB4, GPIOB, 4, 'B', false}, {PB5, GPIOB, 5, 'B', false},
  {PB6, GPIOB, 6, 'B', false}, {PB7, GPIOB, 7, 'B', false}, {PB8, GPIOB, 8, 'B', false},
  {PF0, GPIOF, 0, 'F', false}, {PF1, GPIOF, 1, 'F', false},
};
static const uint32_t N = sizeof(PINS) / sizeof(PINS[0]);
static const uint32_t MAX = 32;

// layout mirrored in tools/pin_scan.py
struct Scan {
  uint32_t magic;           // 0x5C4A0001
  volatile uint32_t mode;   // written by the tool
  volatile uint32_t only;   // pin index for mode 2
  volatile uint32_t done;   // incremented after each classification, mode copied when a mode starts
  uint32_t count;
  char port[MAX];
  uint8_t bit[MAX];
  uint8_t rd_down[MAX];     // level read with pull-down
  uint8_t rd_up[MAX];       // level read with pull-up
  uint16_t adc_mv[MAX];     // 0xFFFF: no ADC on this pin
  uint16_t freq_hz[MAX];    // toggle frequency in mode 1
};
static_assert(sizeof(Scan) == 276, "layout mirrored in tools/pin_scan.py");
volatile Scan scan;

static void set_pull(const PinDef &p, bool up) {
  uint32_t v = GPIO_PUD(p.port);
  v &= ~(3u << (2 * p.bit));
  v |= (up ? 1u : 2u) << (2 * p.bit);
  GPIO_PUD(p.port) = v;
}

static void all_inputs_pulldown() {
  for (uint32_t i = 0; i < N; i++) pinMode(PINS[i].arduino, INPUT_PULLDOWN);
}

static void classify() {
  all_inputs_pulldown();
  delay(5);
  for (uint32_t i = 0; i < N; i++) scan.rd_down[i] = digitalRead(PINS[i].arduino);
  for (uint32_t i = 0; i < N; i++) set_pull(PINS[i], true);
  delay(5);
  for (uint32_t i = 0; i < N; i++) scan.rd_up[i] = digitalRead(PINS[i].arduino);
  for (uint32_t i = 0; i < N; i++) {
    if (PINS[i].adc) {
      pinMode(PINS[i].arduino, INPUT);  // no pull for the ADC reading
      delay(2);
      scan.adc_mv[i] = (uint16_t)(analogRead(PINS[i].arduino) * 3300UL / 1023UL);
    } else {
      scan.adc_mv[i] = 0xFFFF;
    }
  }
  all_inputs_pulldown();
  scan.done++;
}

static uint32_t half_period[MAX];
static uint32_t next_edge[MAX];
static bool level_up[MAX];

static void toggle_loop(uint32_t mode) {
  all_inputs_pulldown();
  for (uint32_t i = 0; i < N; i++) {
    half_period[i] = SystemCoreClock / (2u * scan.freq_hz[i]);
    level_up[i] = false;
  }
  uint32_t now = DWT->CYCCNT;
  for (uint32_t i = 0; i < N; i++) next_edge[i] = now + half_period[i];
  while (scan.mode == mode) {
    now = DWT->CYCCNT;
    for (uint32_t i = 0; i < N; i++) {
      if ((int32_t)(now - next_edge[i]) >= 0) {
        next_edge[i] += half_period[i];
        if (mode == 1 || i == scan.only) {
          level_up[i] = !level_up[i];
          set_pull(PINS[i], level_up[i]);
        }
      }
    }
  }
  all_inputs_pulldown();
}

void setup() {
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
  scan.count = N;
  for (uint32_t i = 0; i < N; i++) {
    scan.port[i] = PINS[i].name;
    scan.bit[i] = PINS[i].bit;
    uint32_t base = PINS[i].name == 'A' ? 10 : PINS[i].name == 'B' ? 30 : 50;
    scan.freq_hz[i] = (uint16_t)((base + PINS[i].bit) * 100);
  }
  scan.mode = 0;
  scan.only = 0xFFFFFFFF;
  scan.magic = 0x5C4A0001;
  classify();
}

void loop() {
  uint32_t m = scan.mode;
  if (m == 1 || m == 2) {
    toggle_loop(m);
  } else if (m == 3) {  // re-classify on request, then back to idle
    classify();
    scan.mode = 0;
  }
}
