/**
 * On-target equivalence tests (STM32F103 hoverboard board): the PR code of the SimpleFOC library linked in this
 * firmware against a verbatim copy of the dev code (ae7b3ad), compiled by the same compiler, run with the real
 * libgcc soft float. Results in the global `equiv` structs, read with tools/equiv_read.py (pyOCD).
 *
 *  -D EQUIV_PID : PIDController::operator() (no peripheral at all: safe with the board normally supplied)
 *  -D EQUIV_PWM : BLDCDriver6PWM::setPwm (TIM1, the motor pins) and BLDCDriver3PWM::setPwm (PC6/PC7/PC8, high side
 *                 inputs of the unused left motor), duty cycles read back from the timer registers.
 *                 THE MAIN SUPPLY MUST BE OFF (board fed by the ST-Link 3.3 V, gate drivers unpowered): the firmware
 *                 refuses to start the PWM if the bus voltage reads above 10 V.
 *
 * Build the same firmware with the dev library: every count must then be "identical" (checks the copies below).
 */
#include <Arduino.h>
#include <SimpleFOC.h>

#define EQUIV_MAGIC 0xE0E0A002u
#define SELF_HOLD_PIN PA5

struct EquivResult {
  uint32_t magic;
  uint32_t state;          // 0 running, 1 done, 2 refused (bus voltage), 3 init failed
  uint32_t cases;
  uint32_t identical;      // same result bit for bit (PID: output and state; PWM: dc and timer register)
  uint32_t expected;       // differences listed in the PR (PID: D == 0 and NaN / signed zero; PWM: see below)
  uint32_t unexpected;     // any other difference: must be 0
  uint32_t new_out_of_range; // PWM: new duty cycle NaN or outside [0, 1], or register above ARR + 1
  uint32_t dev_out_of_range; // PWM: same for the dev code
  uint32_t max_count_diff; // PWM: largest register difference with a valid configuration (supply > 0)
  uint32_t one_count;      // PWM: cases with one register count of difference
  uint32_t reg_mismatch;   // PWM: register != (uint32_t)(dc * (ARR + 1)) for the new code
  float vbus;              // PWM: bus voltage read before starting the PWM
  float bad[10];           // first unexpected case: inputs then outputs
};

volatile uint32_t equiv_progress = 0;

// ---------------------------------------------------------------- random inputs
static uint32_t rng = 0x12345678u;
static uint32_t rnd() { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }
static float urand(float lo, float hi) { return lo + (hi - lo) * (float)(rnd() >> 8) * (1.0f / 16777216.0f); }
static float bits(uint32_t b) { float f; memcpy(&f, &b, 4); return f; }
static uint32_t fbits(float f) { uint32_t b; memcpy(&b, &f, 4); return b; }
static bool same(float a, float b) { return fbits(a) == fbits(b) || (isnan(a) && isnan(b)); }

static const float specials[] = {0.0f, -0.0f, 1e-6f, -1e-6f, 1.0f, -1.0f, 1e6f, -1e6f, NOT_SET, 1e-38f,
                                 INFINITY, -INFINITY, NAN};
static const int n_specials = sizeof(specials) / sizeof(specials[0]);
// 70 %: value in [lo, hi], 15 %: special value, 15 %: any bit pattern
static float pick(float lo, float hi) {
  uint32_t r = rnd() % 100;
  if (r < 70) return urand(lo, hi);
  if (r < 85) return specials[rnd() % n_specials];
  return bits(rnd());
}

#if defined(EQUIV_PID)
// ---------------------------------------------------------------- PID
EquivResult equiv_pid;

// dev ae7b3ad PIDController::operator(), verbatim, with dt = Ts (Ts always set here)
struct DevPid { float P, I, D, output_ramp, limit, Ts, error_prev, output_prev, integral_prev; };
static float dev_pid(DevPid& s, float error) {
    float dt = s.Ts;
    float proportional = s.P * error;
    float integral = s.integral_prev + s.I*dt*0.5f*(error + s.error_prev);
    if(_isset(s.limit)) integral = _constrain(integral, -s.limit, s.limit);
    float derivative = s.D*(error - s.error_prev)/dt;
    float output = proportional + integral + derivative;
    if(_isset(s.limit)) output = _constrain(output, -s.limit, s.limit);
    if(_isset(s.output_ramp) && s.output_ramp > 0){
        float output_rate = (output - s.output_prev)/dt;
        if (output_rate > s.output_ramp)
            output = s.output_prev + s.output_ramp*dt;
        else if (output_rate < -s.output_ramp)
            output = s.output_prev - s.output_ramp*dt;
    }
    s.integral_prev = integral;
    s.output_prev = output;
    s.error_prev = error;
    return output;
}

// the library class, with access to its state
class PidProbe : public PIDController {
public:
  PidProbe() : PIDController(0, 0, 0) {}
  void set_state(float e, float o, float i) { error_prev = e; output_prev = o; integral_prev = i; }
  float e() { return error_prev; }
  float o() { return output_prev; }
  float i() { return integral_prev; }
};

static void run_pid(uint32_t n) {
  EquivResult& r = equiv_pid;
  PidProbe pid;
  for (uint32_t k = 0; k < n; k++) {
    DevPid s;
    s.P = pick(0, 5); s.I = pick(0, 100);
    s.D = (rnd() & 1) ? ((rnd() & 1) ? 0.0f : -0.0f) : pick(-0.1f, 0.1f);
    s.limit = (rnd() % 3 == 0) ? NOT_SET : pick(0.1f, 30);
    s.output_ramp = (rnd() & 1) ? NOT_SET : pick(0, 1000);
    s.Ts = pick(1e-5f, 0.5f);
    if (!_isset(s.Ts)) s.Ts = 1e-3f;  // NOT_SET would read the clock: not comparable
    s.error_prev = pick(-30, 30); s.output_prev = pick(-30, 30); s.integral_prev = pick(-30, 30);
    float error = pick(-30, 30);

    pid.P = s.P; pid.I = s.I; pid.D = s.D; pid.limit = s.limit; pid.output_ramp = s.output_ramp; pid.Ts = s.Ts;
    pid.set_state(s.error_prev, s.output_prev, s.integral_prev);
    float in_D = s.D, in_e = error, in_ep = s.error_prev;

    float out_new = pid(error);
    float out_dev = dev_pid(s, error);

    bool all_same = same(out_new, out_dev) && same(pid.o(), s.output_prev) && same(pid.i(), s.integral_prev)
                    && same(pid.e(), s.error_prev);
    r.cases++;
    if (all_same) r.identical++;
    else if (in_D == 0.0f && same(pid.i(), s.integral_prev) && same(pid.e(), s.error_prev)
             && (isnan(out_dev) || (out_dev == 0.0f && out_new == 0.0f))) r.expected++;
    else {
      if (r.unexpected == 0) {
        float b[10] = {s.P, s.I, in_D, in_e, in_ep, s.limit, s.output_ramp, s.Ts, out_dev, out_new};
        memcpy(r.bad, b, sizeof(b));
      }
      r.unexpected++;
    }
    equiv_progress = k;
  }
}
#endif

#if defined(EQUIV_PWM)
// ---------------------------------------------------------------- setPwm
#include "drivers/hardware_specific/stm32/stm32_mcu.h"
EquivResult equiv_pwm6, equiv_pwm3;

BLDCDriver6PWM driver6 = BLDCDriver6PWM(PA10, PB15, PA9, PB14, PA8, PB13);
BLDCDriver3PWM driver3 = BLDCDriver3PWM(PC6, PC7, PC8);

// dev ae7b3ad duty cycle of one phase, verbatim
static float dev_dc(float U, float voltage_limit, float voltage_power_supply) {
  U = _constrain(U, 0, voltage_limit);
  return _constrain(U / voltage_power_supply, 0.0f , 1.0f );
}

static void run_pwm(EquivResult& r, BLDCDriver* drv, float* dc[3], int handle_idx[3], uint32_t n) {
  STM32DriverParams* p = (STM32DriverParams*)drv->params;
  for (uint32_t k = 0; k < n; k++) {
    float vps = (rnd() % 10 == 0) ? pick(-10, 10) : urand(1, 60);
    float vl = (rnd() % 10 == 0) ? pick(-10, 100) : vps * urand(0.1f, 1.5f);
    float U[3];
    for (int j = 0; j < 3; j++) {
      uint32_t c = rnd() % 8;
      U[j] = c == 0 ? vl : c == 1 ? vps : pick(-5, 70);   // a quarter exactly at the limit or the supply
    }
    drv->voltage_power_supply = vps;
    drv->voltage_limit = vl;
    drv->setPwm(U[0], U[1], U[2]);
    r.cases++;
    bool all_same = true, bad = false, recorded = r.unexpected != 0;
    for (int j = 0; j < 3; j++) {
      TIM_TypeDef* tim = p->timers_handle[handle_idx[j]]->Instance;
      uint32_t arr1 = tim->ARR + 1;
      uint32_t ch = p->channels[handle_idx[j]];
      uint32_t reg = ch == 1 ? tim->CCR1 : ch == 2 ? tim->CCR2 : ch == 3 ? tim->CCR3 : tim->CCR4;
      float d_new = *dc[j];
      float d_dev = dev_dc(U[j], vl, vps);
      uint32_t reg_dev = (uint32_t)(d_dev * arr1);
      if (reg != (uint32_t)(d_new * arr1)) r.reg_mismatch++;
      if (isnan(d_new) || d_new < 0.0f || d_new > 1.0f || reg > arr1) r.new_out_of_range++;
      if (isnan(d_dev) || d_dev < 0.0f || d_dev > 1.0f || reg_dev > arr1) r.dev_out_of_range++;
      if (!same(d_new, d_dev) || reg != reg_dev) all_same = false;
      uint32_t diff = reg > reg_dev ? reg - reg_dev : reg_dev - reg;
      bool valid = vps > 0.0f;   // supply set and positive (NaN, 0, negative, NOT_SET: invalid)
      if (valid) {
        if (diff > r.max_count_diff) r.max_count_diff = diff;
        if (diff == 1) r.one_count++;
        if (diff > 1) bad = true;
      } else if (reg != 0 || d_new != 0.0f) {
        bad = true;  // invalid configuration: the new code must give 0
      }
      if (bad && !recorded) {
        recorded = true;
        float b[10] = {vps, vl, U[j], d_dev, d_new, (float)reg_dev, (float)reg, (float)arr1, (float)j, 0};
        memcpy(r.bad, b, sizeof(b));
      }
    }
    if (all_same) r.identical++;
    else if (bad) r.unexpected++;
    else r.expected++;
    equiv_progress = k;
  }
}
#endif

void setup() {
  pinMode(SELF_HOLD_PIN, OUTPUT);
  digitalWrite(SELF_HOLD_PIN, HIGH);
#if defined(EQUIV_PID)
  equiv_pid.magic = EQUIV_MAGIC;
  run_pid(2000000);
  equiv_pid.state = 1;
#endif
#if defined(EQUIV_PWM)
  equiv_pwm6.magic = equiv_pwm3.magic = EQUIV_MAGIC;
  // bus voltage on PC2 (gain 29.3, see include/board_stm32f103_split.h): refuse to drive the PWM if supplied
  analogReadResolution(12);
  float vbus = 0;
  for (int k = 0; k < 16; k++) vbus += analogRead(PC2) * (3.3f / 4095.0f) * 29.3f / 16.0f;
  equiv_pwm6.vbus = equiv_pwm3.vbus = vbus;
  // supply off, board on the ST-Link 3.3 V: reads ~5.6 V (offset of the divider/ADC, measured 21/11); 26 V supply: ~26 V
  if (vbus > 10.0f) { equiv_pwm6.state = equiv_pwm3.state = 2; return; }

  driver6.voltage_power_supply = 26.0f;
  driver6.voltage_limit = 2.0f;
  driver6.pwm_frequency = 16000;
  driver6.dead_zone = 0.01f;
  driver3.voltage_power_supply = 26.0f;
  driver3.voltage_limit = 2.0f;
  driver3.pwm_frequency = 16000;
  if (!driver6.init()) { equiv_pwm6.state = equiv_pwm3.state = 3; return; }
  driver6.enable();
  {
    float* dc[3] = {&driver6.dc_a, &driver6.dc_b, &driver6.dc_c};
    int idx[3] = {0, 2, 4};   // hardware 6PWM: high side handles
    run_pwm(equiv_pwm6, &driver6, dc, idx, 500000);
  }
  equiv_pwm6.state = 1;
  driver6.setPwm(0, 0, 0);
  driver6.disable();
  if (!driver3.init()) { equiv_pwm3.state = 3; return; }
  driver3.enable();
  {
    float* dc[3] = {&driver3.dc_a, &driver3.dc_b, &driver3.dc_c};
    int idx[3] = {0, 1, 2};
    run_pwm(equiv_pwm3, &driver3, dc, idx, 500000);
  }
  equiv_pwm3.state = 1;
  driver3.setPwm(0, 0, 0);
  driver3.disable();
#endif
}

void loop() {}
