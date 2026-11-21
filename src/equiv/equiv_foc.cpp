/**
 * On-target scenario test of loopFOC / move (simplefoc#596: one dt for the current filters and controllers),
 * and of the pin test of simplefoc#595 (`_isset(pin)` -> `pin != _NC`).
 *
 * EQUIV_FOC: no peripheral at all. A fake driver (setPwm only stores), a fake current sense (currents computed from
 * the angle), a fake sensor (angle given), and a fake clock: every call of _micros() from the library returns
 * `fake_us` (link option -Wl,--wrap=_Z7_microsv), so dev and the PR see exactly the same times. The same scenarios
 * run in a firmware built with dev and one built with the PR; tools/equiv_foc_compare.py compares the traces
 * (voltage.q/d and filtered current.q/d after each loop) bit for bit.
 *
 * EQUIV_PINS: `_isset(pin)` and `pin != _NC` compared for every int in [-2^25, 2^25] and 2^20 random ints.
 */
#include <Arduino.h>
#include <SimpleFOC.h>
#include <new>

#if defined(EQUIV_FOC)

volatile uint32_t fake_us = 1000;
extern "C" unsigned long __wrap__Z7_microsv(void) { return fake_us; }

struct Rec { float a, b, c, d; };
#define FOC_TRACE_LEN 2100
Rec foc_trace[FOC_TRACE_LEN];
struct FocResult {
  uint32_t magic, state, n, scenario_start[20], init_status;
} foc_result = {0xE0E0F001u, 0, 0, {0}, 0};

static uint32_t rng = 0x2468ACE1u;
static uint32_t rnd() { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }
static float urand(float lo, float hi) { return lo + (hi - lo) * (float)(rnd() >> 8) * (1.0f / 16777216.0f); }

static float fake_angle = 0;
class FakeSensor : public Sensor {
public:
  float getSensorAngle() override { return fake_angle; }
  void init() override { Sensor::init(); }
};

class FakeDriver : public BLDCDriver {
public:
  int init() override { initialized = true; return 1; }
  void enable() override {}
  void disable() override {}
  void setPwm(float a, float b, float c) override { Ua = a; Ub = b; Uc = c; }
  void setPhaseState(PhaseState, PhaseState, PhaseState) override {}
  float Ua = 0, Ub = 0, Uc = 0;
};

static float fake_iq = 0.5f, fake_id = 0.0f;
class FakeCurrentSense : public CurrentSense {
public:
  int init() override { initialized = true; return 1; }
  PhaseCurrent_s getPhaseCurrents() override {
    // phase currents of (id, iq) at the electrical angle (15 pole pairs) plus a little noise
    float el = _normalizeAngle(fake_angle * 15);
    float s = _sin(el), c = _cos(el);
    float ia = fake_id * c - fake_iq * s;          // inverse Park, alpha
    float ib = fake_id * s + fake_iq * c;          // beta
    PhaseCurrent_s p;
    p.a = ia + urand(-0.02f, 0.02f);
    p.b = -0.5f * ia + _SQRT3_2 * ib + urand(-0.02f, 0.02f);
    p.c = -p.a - p.b;
    return p;
  }
};

// a fresh motor (and fake sensor / current sense) for each case: the differences of one case do not carry over
alignas(BLDCMotor) static uint8_t motor_buf[sizeof(BLDCMotor)];
alignas(FakeSensor) static uint8_t sensor_buf[sizeof(FakeSensor)];
alignas(FakeCurrentSense) static uint8_t cs_buf[sizeof(FakeCurrentSense)];
BLDCMotor* m;
FakeDriver fdriver;

static void step(uint32_t dt_us, float target) {
  fake_us += dt_us;
  fake_angle += 0.002f;
  fake_iq = 0.8f * fake_iq + 0.2f * (target * 0.9f);   // the "motor" follows the target a bit
  m->loopFOC();
  m->move(target);
  if (foc_result.n < FOC_TRACE_LEN)
    foc_trace[foc_result.n++] = {m->voltage.q, m->voltage.d, m->current.q, m->current.d};
}
static uint32_t jitter() { return 250 + rnd() % 101; }  // 250-350 us

static int n_cases = 0;
static void new_case() { foc_result.scenario_start[n_cases++] = foc_result.n; }

static void fresh() {
  new_case();
  fake_iq = 0.5f; fake_id = 0.0f;
  FakeSensor* sensor = new (sensor_buf) FakeSensor();
  FakeCurrentSense* cs = new (cs_buf) FakeCurrentSense();
  m = new (motor_buf) BLDCMotor(15);
  fdriver.voltage_power_supply = 12;
  fdriver.voltage_limit = 12;
  fdriver.init();
  sensor->init();
  m->linkSensor(sensor);
  m->linkDriver(&fdriver);
  cs->linkDriver(&fdriver);
  cs->init();
  cs->skip_align = true;
  m->linkCurrentSense(cs);
  m->torque_controller = TorqueControlType::foc_current;
  m->controller = MotionControlType::torque;
  m->voltage_limit = 6;
  m->current_limit = 2;
  m->phase_resistance = 0.2f;
  m->zero_electric_angle = 0;
  m->sensor_direction = Direction::CW;
  m->init();
  if (m->initFOC() != 1) foc_result.init_status++;
  for (int k = 0; k < 30; k++) step(jitter(), 0.5f);   // warm-up, recorded
}

static void run_foc() {
  // ---- case 0: a filter and a PID called directly (operator(), clock steps from 0 to 2 s)
  new_case();
  {
    LowPassFilter lpf(0.01f);
    PIDController pid(1.0f, 100.0f, 0.001f, 1000.0f, 10.0f);
    static const uint32_t steps[] = {0, 1, 50, 300, 1000, 200000, 290000, 310000, 450000, 600000, 2000000};
    for (int k = 0; k < 400; k++) {
      uint32_t r = rnd() % 21;
      uint32_t dt = r < 10 ? jitter() : steps[r - 10];
      fake_us += dt;
      float x = urand(-1, 1);
      float y = lpf(x);
      float u = pid(x);
      foc_trace[foc_result.n++] = {y, u, (float)dt, x};
    }
  }

  // ---- case 1: steady loop, 250-350 us
  fresh();
  for (int k = 0; k < 250; k++) step(jitter(), 0.5f + 0.3f * _sin(k * 0.05f));

  // ---- cases 2-7: motor disabled for a pause, then enabled again
  static const uint32_t pauses[] = {50000, 250000, 290000, 310000, 400000, 600000};
  for (int p = 0; p < 6; p++) {
    fresh();
    m->disable();
    fake_us += pauses[p];
    m->enable();
    for (int k = 0; k < 30; k++) step(jitter(), 0.6f);
  }

  // ---- case 8: Ts set by the user on some filters and controllers
  fresh();
  m->LPF_current_q.Ts = 1e-4f;
  m->PID_current_d.Ts = 2e-4f;
  for (int k = 0; k < 150; k++) step(jitter(), 0.4f);
  // ---- case 9: same, then Ts set back to NOT_SET
  fresh();
  m->LPF_current_q.Ts = 1e-4f;
  m->PID_current_d.Ts = 2e-4f;
  for (int k = 0; k < 30; k++) step(jitter(), 0.4f);
  m->LPF_current_q.Ts = NOT_SET;
  m->PID_current_d.Ts = NOT_SET;
  for (int k = 0; k < 30; k++) step(jitter(), 0.4f);

  // ---- cases 10-15: other torque modes for a while (not recorded), back to foc_current
  static const TorqueControlType modes[] = {TorqueControlType::voltage, TorqueControlType::dc_current,
                                            TorqueControlType::estimated_current};
  static const uint32_t absences[] = {100000, 400000};
  for (int k_m = 0; k_m < 3; k_m++)
    for (int a = 0; a < 2; a++) {
      fresh();
      m->torque_controller = modes[k_m];
      uint32_t t_end = fake_us + absences[a];
      while (fake_us < t_end) { fake_us += 20000; m->loopFOC(); m->move(0.5f); }
      m->torque_controller = TorqueControlType::foc_current;
      for (int k = 0; k < 30; k++) step(jitter(), 0.5f);
    }

  // ---- case 16: some loops in the same microsecond (clock not moving)
  fresh();
  for (int k = 0; k < 70; k++) step(k % 10 == 0 ? 0 : jitter(), 0.5f);

  // ---- case 17: _micros() wrapping around 2^32
  fake_us = 0xFFFFFFFFu - 40000;
  fresh();
  for (int k = 0; k < 170; k++) step(jitter(), 0.5f);

  new_case();
  foc_result.state = 1;
}
#endif

#if defined(EQUIV_PINS)
struct PinResult { uint32_t magic, state, cases, differ; int32_t first; } pin_result = {0xE0E0F002u, 0, 0, 0, 0};
volatile int pin_v;
static uint32_t prng = 0x13579BDFu;
static void check_pin(int v) {
  pin_v = v;
  int pin = pin_v;   // through a volatile: the compiler cannot fold the conditions
  bool old_set = _isset(pin);
  bool new_set = pin != _NC;
  pin_result.cases++;
  if (old_set != new_set) { if (!pin_result.differ) pin_result.first = v; pin_result.differ++; }
}
static void run_pins() {
  for (int v = -(1 << 25); v <= (1 << 25); v++) check_pin(v);
  check_pin(INT32_MIN); check_pin(INT32_MAX);
  for (int k = 0; k < (1 << 20); k++) { prng ^= prng << 13; prng ^= prng >> 17; prng ^= prng << 5; check_pin((int)prng); }
  pin_result.state = 1;
}
#endif

void equiv_foc_setup() {
#if defined(EQUIV_FOC)
  run_foc();
#endif
#if defined(EQUIV_PINS)
  run_pins();
#endif
}
