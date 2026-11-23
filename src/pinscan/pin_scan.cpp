// Pin scan of an unknown GD32F130 / GD32E230 QFN32 board (simplefoc-work #39): find which MCU pin goes where with a
// scope, probing only easy points (gate driver inputs, op amp outputs, connectors), never the QFN pads.
// SAFETY: main supply and battery DISCONNECTED, board on the ST-Link 3.3 V only (gate drivers unpowered).
// No pin is ever an output: the firmware only switches the internal pull-up / pull-down (~40 kOhm), so a pin
// tied to anything (op amp output, unpowered driver input) carries less than 100 uA. SWD pins PA13 / PA14 untouched.
// Read and driven over SWD by tools/pin_scan.py through the struct `scan`:
//   at start-up and mode 3: classify every pin (pull-down read, pull-up read, ADC voltage without pull); mode 0: idle
//   mode 1: all pins toggle their pull at their own frequency, f = (base + n) * 100 Hz, base A=10, B=30, F=50, C=60
//           (PA7 = 1.7 kHz, PB1 = 3.1 kHz, PF0 = 5.0 kHz): the frequency seen on the scope gives the pin
//   mode 2: only pin index `only` toggles (confirmation), the others stay on pull-down
#include <Arduino.h>

struct PinDef {
  uint32_t arduino;
  uint32_t port;   // GPIOA / GPIOB / GPIOF base address
  uint8_t bit;
  char name;       // 'A', 'B', 'C', 'F'
  bool adc;
};

static const PinDef PINS[] = {
  {PA0, GPIOA, 0, 'A', true},  {PA1, GPIOA, 1, 'A', true},  {PA2, GPIOA, 2, 'A', true},
  {PA3, GPIOA, 3, 'A', true},  {PA4, GPIOA, 4, 'A', true},  {PA5, GPIOA, 5, 'A', true},
  {PA6, GPIOA, 6, 'A', true},  {PA7, GPIOA, 7, 'A', true},  {PA8, GPIOA, 8, 'A', false},
  {PA9, GPIOA, 9, 'A', false}, {PA10, GPIOA, 10, 'A', false}, {PA11, GPIOA, 11, 'A', false},
  {PA12, GPIOA, 12, 'A', false}, {PA15, GPIOA, 15, 'A', false},
  {PB0, GPIOB, 0, 'B', true},  {PB1, GPIOB, 1, 'B', true},
#ifdef PB2
  {PB2, GPIOB, 2, 'B', false},
#endif
  {PB3, GPIOB, 3, 'B', false}, {PB4, GPIOB, 4, 'B', false}, {PB5, GPIOB, 5, 'B', false},
  {PB6, GPIOB, 6, 'B', false}, {PB7, GPIOB, 7, 'B', false},
#ifdef PB8
  {PB8, GPIOB, 8, 'B', false},
#endif
#ifdef PB15  // LQFP48 only (validation on the known split board)
  {PB9, GPIOB, 9, 'B', false},  {PB10, GPIOB, 10, 'B', false}, {PB11, GPIOB, 11, 'B', false},
  {PB12, GPIOB, 12, 'B', false}, {PB13, GPIOB, 13, 'B', false}, {PB14, GPIOB, 14, 'B', false},
  {PB15, GPIOB, 15, 'B', false},
  {PC13, GPIOC, 13, 'C', false}, {PC14, GPIOC, 14, 'C', false}, {PC15, GPIOC, 15, 'C', false},
  {PF4, GPIOF, 4, 'F', false},  {PF5, GPIOF, 5, 'F', false},  {PF6, GPIOF, 6, 'F', false},
  {PF7, GPIOF, 7, 'F', false},
#endif
  {PF0, GPIOF, 0, 'F', false}, {PF1, GPIOF, 1, 'F', false},
};
static const uint32_t N = sizeof(PINS) / sizeof(PINS[0]);
static const uint32_t MAX = 40;
static_assert(sizeof(PINS) / sizeof(PINS[0]) <= MAX, "too many pins");

// layout mirrored in tools/pin_scan.py
struct Scan {
  uint32_t magic;           // 0x5C4A0001
  volatile uint32_t mode;   // written by the tool
  volatile uint32_t only;   // pin index for mode 2
  volatile uint32_t done;   // incremented after each classification
  uint32_t count;
  char port[MAX];
  uint8_t bit[MAX];
  uint8_t rd_down[MAX];     // level read with pull-down
  uint8_t rd_up[MAX];       // level read with pull-up
  uint16_t adc_mv[MAX];     // 0xFFFF: no ADC on this pin
  uint16_t freq_hz[MAX];    // toggle frequency in modes 1 and 2
  uint32_t tick_hz;         // time base rate measured against delay(): the toggle periods are computed from it
};
static_assert(sizeof(Scan) == 344, "layout mirrored in tools/pin_scan.py");
volatile Scan scan;

// Time base: DWT cycle counter on the Cortex-M3 (F130); the Cortex-M23 (E230) has none: TIMER2 free-running, 16 bit
#if defined(GD32E23x)
static void timebase_init() {
  rcu_periph_clock_enable(RCU_TIMER2);
  TIMER_CTL0(TIMER2) = 0;
  TIMER_PSC(TIMER2) = SystemCoreClock / 1000000u - 1u;  // ~1 MHz, the real rate is measured into tick_hz
  TIMER_CAR(TIMER2) = 0xFFFF;
  TIMER_SWEVG(TIMER2) = TIMER_SWEVG_UPG;
  TIMER_CTL0(TIMER2) = TIMER_CTL0_CEN;
}
static inline uint32_t now_ticks() { return TIMER_CNT(TIMER2) & 0xFFFFu; }
static inline uint32_t ticks_add(uint32_t t, uint32_t d) { return (t + d) & 0xFFFFu; }
static inline uint32_t ticks_diff(uint32_t a, uint32_t b) { return (a - b) & 0xFFFFu; }
static inline bool reached(uint32_t now, uint32_t t) { return (int16_t)(uint16_t)(now - t) >= 0; }
#else
static void timebase_init() {
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}
static inline uint32_t now_ticks() { return DWT->CYCCNT; }
static inline uint32_t ticks_add(uint32_t t, uint32_t d) { return t + d; }
static inline uint32_t ticks_diff(uint32_t a, uint32_t b) { return a - b; }
static inline bool reached(uint32_t now, uint32_t t) { return (int32_t)(now - t) >= 0; }
#endif

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
    half_period[i] = scan.tick_hz / (2u * scan.freq_hz[i]);
    level_up[i] = false;
  }
  uint32_t now = now_ticks();
  for (uint32_t i = 0; i < N; i++) next_edge[i] = ticks_add(now, half_period[i]);
  while (scan.mode == mode) {
    now = now_ticks();
    for (uint32_t i = 0; i < N; i++) {
      if (reached(now, next_edge[i])) {
        next_edge[i] = ticks_add(next_edge[i], half_period[i]);
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
  timebase_init();
  uint32_t t0 = now_ticks();
  delay(20);
  scan.tick_hz = ticks_diff(now_ticks(), t0) * 50u;
  scan.count = N;
  for (uint32_t i = 0; i < N; i++) {
    scan.port[i] = PINS[i].name;
    scan.bit[i] = PINS[i].bit;
    uint32_t base = PINS[i].name == 'A' ? 10 : PINS[i].name == 'B' ? 30 : PINS[i].name == 'F' ? 50 : 60;
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
  } else if (m == 3) {  // classify again on request, then back to idle
    classify();
    scan.mode = 0;
  }
}
