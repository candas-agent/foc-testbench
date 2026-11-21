/**
 * On-target audit of SimpleFOC (dev) functions, no peripheral at all (fake driver / current sense / sensor / clock,
 * link option -Wl,--wrap=_Z7_microsv). Each check fills one AuditRec; read with tools/equiv_audit_read.py.
 * Things that differ between a PC and the MCU (float -> unsigned conversion of negative values, soft float) are
 * the reason to run this on the target.
 */
#include <Arduino.h>
#include <new>
#if defined(EQUIV_AUDIT)
// test code only: access to the private state of HallSensor (glitch / timing checks without pins)
#define private public
#endif
#include <SimpleFOC.h>
#if defined(EQUIV_AUDIT_DRIVERS)
#include "encoders/smoothing/SmoothingSensor.h"
#endif
#undef private

#if defined(EQUIV_AUDIT)

volatile uint32_t fake_us = 1000;
extern "C" unsigned long __wrap__Z7_microsv(void) { return fake_us; }

struct AuditRec {
  uint32_t id;      // check number (see tools/equiv_audit_read.py)
  uint32_t cases;
  uint32_t fails;   // cases outside the tolerance of the check
  float max_err;    // largest error found
  float worst[4];   // inputs / outputs of the worst case
};
#define N_AUDIT 64
AuditRec audit[N_AUDIT];
struct { uint32_t magic, state, n; } audit_state = {0xE0E0A0D1u, 0, 0};

static AuditRec& rec(uint32_t id) { AuditRec& r = audit[audit_state.n++]; r.id = id; return r; }
static void worse(AuditRec& r, float err, float w0, float w1 = 0, float w2 = 0, float w3 = 0) {
  if (err > r.max_err || isnan(err)) { r.max_err = isnan(err) ? INFINITY : err; r.worst[0] = w0; r.worst[1] = w1; r.worst[2] = w2; r.worst[3] = w3; }
}

// ---------------------------------------------------------------- fakes
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
  void setPhaseState(PhaseState sa, PhaseState sb, PhaseState sc) override { st[0] = sa; st[1] = sb; st[2] = sc; }
  float Ua = 0, Ub = 0, Uc = 0;
  PhaseState st[3];
};
static PhaseCurrent_s fake_i = {0, 0, 0};
class FakeCurrentSense : public CurrentSense {
public:
  int init() override { initialized = true; return 1; }
  PhaseCurrent_s getPhaseCurrents() override { return fake_i; }
};

alignas(BLDCMotor) static uint8_t motor_buf[sizeof(BLDCMotor)];
FakeDriver fdriver;
FakeSensor fsensor;
FakeCurrentSense fcs;

// ---------------------------------------------------------------- 1-3: _sin / _cos outside [0, 2PI]
static void check_sincos() {
  AuditRec& neg = rec(1);   // negative angles
  for (float a = -50.0f; a < 0.0f; a += 0.001f) {
    float e = fmaxf(fabsf(_sin(a) - (float)sin((double)a)), fabsf(_cos(a) - (float)cos((double)a)));
    neg.cases++; if (e > 1e-3f) neg.fails++;
    worse(neg, e, a, _sin(a), _cos(a));
  }
  AuditRec& big = rec(2);   // large positive angles (open loop angle mode, many electrical turns)
  for (float a = 4.0e5f; a < 4.3e5f; a += 1.0f) {
    float e = fabsf(_sin(a) - (float)sin((double)a));
    big.cases++; if (e > 1e-3f) big.fails++;
    worse(big, e, a, _sin(a));
  }
  AuditRec& in = rec(3);    // [0, 2PI]: reference accuracy on the target
  for (float a = 0.0f; a <= _2PI; a += 0.0001f) {
    float e = fmaxf(fabsf(_sin(a) - (float)sin((double)a)), fabsf(_cos(a) - (float)cos((double)a)));
    in.cases++; if (e > 1e-3f) in.fails++;
    worse(in, e, a);
  }
}

// ---------------------------------------------------------------- motor helpers
static BLDCMotor* m;
static BLDCMotor* new_motor(MotionControlType ctrl, TorqueControlType tc, bool with_sensor) {
  m = new (motor_buf) BLDCMotor(7);
  fdriver.voltage_power_supply = 12;
  fdriver.voltage_limit = 12;
  fdriver.init();
  m->linkDriver(&fdriver);
  if (with_sensor) { fake_angle = 1.0f; fsensor.init(); m->linkSensor(&fsensor); }
  fcs.linkDriver(&fdriver);
  fcs.init();
  fcs.skip_align = true;
  m->linkCurrentSense(&fcs);
  m->controller = ctrl;
  m->torque_controller = tc;
  m->voltage_limit = 2;
  m->velocity_limit = 20;
  m->current_limit = 2;
  m->phase_resistance = 1;
  m->zero_electric_angle = 0;
  m->sensor_direction = Direction::CW;
  m->init();
  m->initFOC();
  return m;
}
// amplitude and angle of the voltage vector applied by the driver (from the 3 phase voltages)
static void vec(float& amp, float& ang) {
  float al = (2.0f * fdriver.Ua - fdriver.Ub - fdriver.Uc) / 3.0f;
  float be = (fdriver.Ub - fdriver.Uc) * _1_SQRT3;
  amp = sqrtf(al * al + be * be);
  ang = atan2f(be, al);
}

// ---------------------------------------------------------------- 4-5: open loop angle mode, negative angles
struct Dbg { float shaft, el, vq, ua, ub, uc, amp, k; };
Dbg dbg[30];
uint32_t n_dbg = 0;
static void check_openloop() {
  AuditRec& neg = rec(4);   // voltage vector amplitude while the target angle is negative
  AuditRec& pos = rec(5);   // same, positive
  new_motor(MotionControlType::angle_openloop, TorqueControlType::voltage, false);
  m->foc_modulation = FOCModulationType::SpaceVectorPWM;
  for (int k = 0; k < 6000; k++) {
    fake_us += 1000;
    float target = (k < 3000) ? -5.0f : 5.0f;  // 20 rad/s: reaches -5 rad, then +5 rad
    m->loopFOC();
    m->move(target);
    float amp, ang;
    vec(amp, ang);
    if (k >= 2990 && k < 3020 && n_dbg < 30)
      dbg[n_dbg++] = {m->shaft_angle, m->electrical_angle, m->voltage.q, fdriver.Ua, fdriver.Ub, fdriver.Uc, amp, (float)k};
    AuditRec& r = m->shaft_angle < 0 ? neg : pos;
    float err = fabsf(amp - m->voltage_limit);
    r.cases++; if (err > 0.05f) r.fails++;
    worse(r, err, m->shaft_angle, amp, m->voltage_limit);
  }
}

// ---------------------------------------------------------------- 6-13: setPhaseVoltage, every modulation
static void check_modulation() {
  static const FOCModulationType mods[] = {FOCModulationType::SinePWM, FOCModulationType::SpaceVectorPWM,
                                           FOCModulationType::Trapezoid_120, FOCModulationType::Trapezoid_150};
  new_motor(MotionControlType::torque, TorqueControlType::voltage, false);
  for (int mi = 0; mi < 4; mi++)
    for (int cen = 0; cen < 2; cen++) {
      AuditRec& r = rec(6 + mi * 2 + cen);
      m->foc_modulation = mods[mi];
      m->modulation_centered = cen;
      const float Uqs[] = {0.0f, 0.5f, 2.0f, 12.0f * _1_SQRT3, 6.0f, -3.0f};
      for (float Uq : Uqs)
        for (int k = -20; k < 3620; k++) {
          float a = k * (_2PI / 3600.0f);
          fdriver.Ua = fdriver.Ub = fdriver.Uc = NAN;
          m->setPhaseVoltage(Uq, 0.0f, a);
          float lo = fminf(fdriver.Ua, fminf(fdriver.Ub, fdriver.Uc));
          float hi = fmaxf(fdriver.Ua, fmaxf(fdriver.Ub, fdriver.Uc));
          // range: phases must stay in [0, driver voltage_limit] when |Uq| fits the modulation
          float lim = (mi == 1) ? 12.0f * _1_SQRT3 : (mi == 0 ? 6.0f : 6.0f);
          float err = 0;
          if (isnan(lo) || isnan(hi)) err = INFINITY;
          else if (fabsf(Uq) <= lim) err = fmaxf(-lo, hi - 12.0f);
          if (mi < 2 && !isnan(lo)) {   // sine / SVPWM: the vector must be Uq at a + PI/2
            float amp, ang;
            vec(amp, ang);
            float exp_ang = a + _PI_2 + (Uq < 0 ? _PI : 0);
            float dang = fabsf(remainderf(ang - exp_ang, _2PI));
            if (fabsf(Uq) > 0.1f && (fabsf(amp - fabsf(Uq)) > 0.01f * fabsf(Uq) || dang > 0.01f))
              err = fmaxf(err, fmaxf(fabsf(amp - fabsf(Uq)), dang));
          }
          r.cases++; if (err > 1e-4f) r.fails++;
          worse(r, err, a, Uq, lo, hi);
        }
    }
}

// ---------------------------------------------------------------- 14: trapezoid sector index range
static void check_sector() {
  AuditRec& r = rec(14);
  for (int k = -2000; k < 2000; k++) {
    // around -PI/6 and 2PI - PI/6, where _normalizeAngle(angle + PI/6) is near 0 / 2PI
    float base = (k < 0) ? -_PI_6 : (_2PI - _PI_6);
    float a = base + (k % 1000) * 1e-7f;
    for (int s = 0; s < 2; s++) {
      a = s ? nextafterf(a, -10) : a;
      float n = _normalizeAngle(a + _PI_6);
      int sec6 = 6 * (n / _2PI), sec12 = 12 * (n / _2PI);
      r.cases++;
      if (sec6 < 0 || sec6 > 5 || sec12 < 0 || sec12 > 11) { r.fails++; worse(r, (float)sec12, a, n, (float)sec6, (float)sec12); }
    }
  }
}

// ---------------------------------------------------------------- 15-16: current sense transforms
static void check_current() {
  AuditRec& dq = rec(15);   // getFOCCurrents vs exact Park/Clarke, 2 and 3 measured phases
  AuditRec& dcs = rec(16);  // getDCCurrent magnitude
  AuditRec& dsg = rec(23);  // getDCCurrent sign (sign of iq when the angle is given)
  for (int k = 0; k < 20000; k++) {
    float ang = (k % 400) * (_2PI / 400.0f);
    float id = ((k * 7) % 21 - 10) * 0.1f, iq = ((k * 13) % 41 - 20) * 0.1f;
    double al = id * cos((double)ang) - iq * sin((double)ang), be = id * sin((double)ang) + iq * cos((double)ang);
    double ia = al, ib = -0.5 * al + 0.86602540378 * be, ic = -ia - ib;
    bool two = k & 1;
    fake_i.a = (float)ia; fake_i.b = (float)ib; fake_i.c = two ? 0.0f : (float)ic;
    DQCurrent_s c = fcs.getFOCCurrents(ang);
    float err = fmaxf(fabsf(c.d - id), fabsf(c.q - iq));
    dq.cases++; if (err > 2e-3f) dq.fails++;
    worse(dq, err, ang, id, iq, two);
    // dc current: magnitude sqrt(id^2 + iq^2), sign of iq (when the angle is given)
    float dc = fcs.getDCCurrent(ang);
    float mag = sqrtf(id * id + iq * iq);
    if (mag > 0.05f) {
      float derr = fabsf(fabsf(dc) - mag) / mag;
      dcs.cases++; if (derr > 0.01f) dcs.fails++;
      worse(dcs, derr, ang, id, iq, dc);
    }
    if (fabsf(iq) > 0.05f) {
      bool wrong = (dc > 0) != (iq > 0);
      dsg.cases++; if (wrong) { dsg.fails++; worse(dsg, 1.0f, ang, id, iq, dc); }
    }
  }
}

// ---------------------------------------------------------------- 17-22: Sensor velocity / angle tracking
static void check_sensor() {
  static const float speeds[] = {0.05f, -0.05f, 3.0f, -3.0f, 200.0f, -200.0f};
  for (int si = 0; si < 6; si++) {
    AuditRec& r = rec(17 + si);
    float w = speeds[si];
    // start 2 s before the 2^31 us signed wrap, run 4 s at 300 us: crosses 2^31 (long timestamps)
    fake_us = 0x80000000u - 2000000u;
    double total = 1.0;
    fake_angle = 1.0f;
    alignas(FakeSensor) static uint8_t sbuf[sizeof(FakeSensor)];
    FakeSensor* s = new (sbuf) FakeSensor();
    s->init();
    double angle0 = s->getAngle();
    for (int k = 0; k < 13333; k++) {
      fake_us += 300;
      total += w * 300e-6;
      double t = fmod(total, 2 * M_PI); if (t < 0) t += 2 * M_PI;
      fake_angle = (float)t;
      s->update();
      float v = s->getVelocity();
      if (k < 3) continue;
      float verr = fabsf(v - w) / fabsf(w);
      float aerr = fabsf((float)(s->getPreciseAngle() - angle0 - (total - 1.0)));
      float err = fmaxf(verr, aerr);
      r.cases++; if (verr > 0.02f || aerr > 1e-3f) r.fails++;
      worse(r, err, (float)(fake_us - 0x80000000u), v, w, aerr);
    }
  }
}


// ---------------------------------------------------------------- 30-45: closed loop with a simulated motor
// Motor model (true rotor): pp 7, R 1 Ohm, flux 0.01 Wb, J 1e-4, friction 1e-5; currents = steady state (L ~ 0).
// The fake sensor reads dir * theta (so sensor_direction = dir is right), true zero electric angle 0.7 rad.
static struct { float th, w, id, iq; int dir; bool quasi_static; float zero; } mot;
static const float M_R = 1.0f, M_FLUX = 0.01f, M_J = 1e-4f, M_B = 1e-5f;
static const int M_PP = 7;
static float true_el() { return M_PP * mot.th - mot.zero; }
static void applied_ab(float& al, float& be) {
  al = (2.0f * fdriver.Ua - fdriver.Ub - fdriver.Uc) / 3.0f;
  be = (fdriver.Ub - fdriver.Uc) * _1_SQRT3;
}
class ModelSensor : public Sensor {
public:
  float getSensorAngle() override {
    if (mot.quasi_static) {
      // alignment: the rotor flux follows the applied voltage vector (strong field, no load)
      float al, be;
      applied_ab(al, be);
      if (al * al + be * be > 1e-4f) {
        float el = atan2f(be, al);
        float th = (el + mot.zero) / M_PP;
        float k = roundf((mot.th - th) / (_2PI / M_PP));   // nearest mechanical solution
        mot.th = th + k * (_2PI / M_PP);
      }
    }
    float a = fmodf(mot.dir * mot.th, _2PI);
    return a < 0 ? a + _2PI : a;
  }
  void init() override { Sensor::init(); }
};
class ModelCurrentSense : public CurrentSense {
public:
  int init() override { initialized = true; return 1; }
  PhaseCurrent_s getPhaseCurrents() override {
    float e = true_el(), c = cosf(e), s = sinf(e);
    float al = mot.id * c - mot.iq * s, be = mot.id * s + mot.iq * c;
    PhaseCurrent_s p;
    p.a = al; p.b = -0.5f * al + _SQRT3_2 * be; p.c = -p.a - p.b;
    return p;
  }
};
alignas(ModelSensor) static uint8_t msensor_buf[sizeof(ModelSensor)];
alignas(ModelCurrentSense) static uint8_t mcs_buf[sizeof(ModelCurrentSense)];

static void model_step(float dt) {
  float al, be;
  applied_ab(al, be);
  float e = true_el(), c = cosf(e), s = sinf(e);
  float vd = al * c + be * s, vq = be * c - al * s;
  float we = M_PP * mot.w;
  mot.id = vd / M_R;
  mot.iq = (vq - M_FLUX * we) / M_R;
  float torque = 1.5f * M_PP * M_FLUX * mot.iq;
  mot.w += (torque - M_B * mot.w) / M_J * dt;
  mot.th += mot.w * dt;
}

static float vmax_seen;
static void sim_loop(float target, int n, float dt_s = 300e-6f) {
  for (int k = 0; k < n; k++) {
    fake_us += (uint32_t)(dt_s * 1e6f);
    model_step(dt_s);
    m->loopFOC();
    m->move(target);
    float amp, ang;
    vec(amp, ang);
    if (amp > vmax_seen) vmax_seen = amp;
  }
}

static bool model_motor(int dir, MotionControlType ctrl, TorqueControlType tc, bool align) {
  mot.th = 0.3f; mot.w = 0; mot.id = 0; mot.iq = 0; mot.dir = dir; mot.quasi_static = false; mot.zero = 0.7f;
  m = new (motor_buf) BLDCMotor(M_PP);
  fdriver.voltage_power_supply = 12;
  fdriver.voltage_limit = 12;
  fdriver.init();
  m->linkDriver(&fdriver);
  ModelSensor* ms = new (msensor_buf) ModelSensor();
  ms->init();
  m->linkSensor(ms);
  ModelCurrentSense* mc = new (mcs_buf) ModelCurrentSense();
  mc->linkDriver(&fdriver);
  mc->init();
  mc->skip_align = true;
  m->linkCurrentSense(mc);
  m->controller = ctrl;
  m->torque_controller = tc;
  m->voltage_limit = 6;
  m->voltage_sensor_align = 2;
  m->velocity_limit = 30;
  m->current_limit = 1;
  m->PID_velocity.P = 0.05f; m->PID_velocity.I = 1.0f; m->PID_velocity.D = 0;
  m->P_angle.P = 10;
  m->PID_current_q.P = 1; m->PID_current_q.I = 300;
  m->PID_current_d.P = 1; m->PID_current_d.I = 300;
  if (!align) { m->zero_electric_angle = 0.7f; m->sensor_direction = dir > 0 ? Direction::CW : Direction::CCW; }
  m->init();
  mot.quasi_static = align;
  int ok = m->initFOC();
  mot.quasi_static = false;
  mot.w = 0;
  vmax_seen = 0;
  return ok != 0;
}

static void check_closed_loop() {
  for (int dir = 1; dir >= -1; dir -= 2) {
    int base = dir > 0 ? 30 : 40;
    // velocity mode, voltage torque control: +20 then -20 rad/s
    AuditRec& v = rec(base + 0);
    model_motor(dir, MotionControlType::velocity, TorqueControlType::voltage, false);
    for (int ph = 0; ph < 2; ph++) {
      float tgt = ph ? -20.0f : 20.0f;
      sim_loop(tgt, 4000);   // 1.2 s
      // the true rotor turns dir * shaft velocity (the sensor is mounted the other way round for dir -1)
      float err = fabsf(mot.w - tgt) / 20.0f;   // shaft = dir * sensor = true rotor
      v.cases++; if (err > 0.03f) v.fails++;
      worse(v, err, tgt, mot.w, m->shaft_velocity);
    }
    float vmax_v = vmax_seen;
    // angle mode: +3 rad then -3 rad, velocity limit 10
    AuditRec& a = rec(base + 1);
    model_motor(dir, MotionControlType::angle, TorqueControlType::voltage, false);
    m->updateVelocityLimit(10);
    float wmax = 0;
    for (int ph = 0; ph < 2; ph++) {
      float tgt = ph ? -3.0f : 3.0f;
      for (int k = 0; k < 8000; k++) { sim_loop(tgt, 1); if (fabsf(mot.w) > wmax) wmax = fabsf(mot.w); }
      float err = fabsf(m->shaft_angle - tgt);
      a.cases++; if (err > 0.02f) a.fails++;
      worse(a, err, tgt, m->shaft_angle, wmax);
    }
    if (vmax_seen > vmax_v) vmax_v = vmax_seen;
    AuditRec& al = rec(base + 2);   // velocity limit respected in angle mode
    al.cases++; if (wmax > 10.0f * 1.15f) al.fails++;
    worse(al, wmax / 10.0f - 1.0f, wmax);
    // foc_current torque mode: +0.5 A, -0.5 A, then 2 A with current_limit 1 A (rotor held: w = 0)
    AuditRec& c = rec(base + 3);
    model_motor(dir, MotionControlType::torque, TorqueControlType::foc_current, false);
    const float tg[] = {0.5f, -0.5f, 2.0f};
    for (int ph = 0; ph < 3; ph++) {
      float iq_max = 0;
      for (int k = 0; k < 600; k++) { sim_loop(tg[ph], 1); mot.w = 0; if (fabsf(mot.iq) > iq_max) iq_max = fabsf(mot.iq); }
      float expect = _constrain(tg[ph], -1.0f, 1.0f);
      float err = fabsf(mot.iq - expect);
      c.cases++; if (err > 0.03f) c.fails++;
      worse(c, err, tg[ph], mot.iq, iq_max);
    }
    // voltage limit: largest applied vector over the voltage-mode runs above
    AuditRec& vl = rec(base + 4);
    vl.cases++; if (vmax_v > 6.0f * 1.001f) vl.fails++;
    worse(vl, vmax_v - 6.0f, vmax_v);
    // alignment (initFOC finds direction and zero angle), then velocity mode
    AuditRec& ag = rec(base + 5);
    bool ok = model_motor(dir, MotionControlType::velocity, TorqueControlType::voltage, true);
    float zerr = ok ? fabsf(remainderf(m->zero_electric_angle - 0.7f, _2PI)) : INFINITY;
    bool dir_ok = ok && ((dir > 0) == (m->sensor_direction == Direction::CW));
    sim_loop(15.0f, 4000);
    float verr = fabsf(mot.w - 15.0f) / 15.0f;
    float err = fmaxf(zerr, verr) + (dir_ok ? 0.0f : 10.0f);
    ag.cases++; if (!ok || !dir_ok || zerr > 0.05f || verr > 0.03f) ag.fails++;
    worse(ag, err, m->zero_electric_angle, (float)(int)m->sensor_direction, mot.w, (float)ok);
  }
}

// ---------------------------------------------------------------- 50-54: HallSensor (state set directly)
static void hall_set(HallSensor& h, int state) {
  h.A_active = (state >> 2) & 1; h.B_active = (state >> 1) & 1; h.C_active = state & 1;
  h.updateState();
}
static int hall_of_sector(int sec) {   // inverse of ELECTRIC_SECTORS
  for (int st = 1; st < 7; st++) if (ELECTRIC_SECTORS[st] == sec) return st;
  return 0;
}
alignas(HallSensor) static uint8_t hall_buf[sizeof(HallSensor)];
static HallSensor* new_hall(int sec) {
  memset(hall_buf, 0, sizeof(hall_buf));
  HallSensor* h = new (hall_buf) HallSensor(PC10, PC11, PC12, 15);
  h->use_interrupt = 1;   // update() then only copies the state (no digitalRead)
  h->hall_state = hall_of_sector(sec); h->electric_sector = sec; h->electric_rotations = 0;
  h->direction = Direction::CW; h->old_direction = Direction::CW;
  h->pulse_timestamp = fake_us;
  h->pulse_diff = 0;
  return h;
}
static int wrap6(int c) { return ((c % 6) + 6) % 6; }
static void check_hall() {
  // 50/51: steady rotation forward / backward, 1 sector every 2 ms, across the 2^31 us wrap
  for (int d = 0; d < 2; d++) {
    AuditRec& r = rec(50 + d);
    fake_us = 0x80000000u - 100000u;
    HallSensor* h = new_hall(0);
    int count = 0;
    for (int k = 0; k < 400; k++) {
      fake_us += 2000;
      count += d ? -1 : 1;
      hall_set(*h, hall_of_sector(wrap6(count)));
      h->update();
      float exp_angle = count * (_2PI / 90.0f);
      float exp_vel = (d ? -1.0f : 1.0f) * (_2PI / 90.0f) / 0.002f;
      float aerr = fabsf(h->getAngle() - exp_angle);
      float v = h->getVelocity();
      float verr = (k > 2) ? fabsf(v - exp_vel) / fabsf(exp_vel) : 0.0f;
      r.cases++; if (aerr > 1e-4f || verr > 0.01f) r.fails++;
      worse(r, fmaxf(aerr, verr), (float)count, h->getAngle(), v, exp_vel);
    }
  }
  // 52/53: glitch to an invalid state (000 or 111) then back
  {
    AuditRec& r = rec(52);
    AuditRec& rr = rec(53);
    for (int sec = 0; sec < 6; sec++)
      for (int bad = 0; bad < 2; bad++) {
        fake_us += 10000;
        HallSensor* h = new_hall(sec);
        h->update();
        float before = h->getAngle();
        fake_us += 50;
        hall_set(*h, bad ? 7 : 0);
        h->update();
        float during = h->getAngle();
        fake_us += 50;
        hall_set(*h, hall_of_sector(sec));
        h->update();
        float after = h->getAngle();
        float jump = fabsf(during - before) / (_2PI / 90.0f);   // in sectors
        r.cases++; if (jump > 1.01f) r.fails++;
        worse(r, jump, (float)sec, (float)(bad ? 7 : 0), before, during);
        float drift = fabsf(after - before) / (_2PI / 90.0f);
        rr.cases++; if (drift > 0.01f) rr.fails++;
        worse(rr, drift, (float)sec, (float)(bad ? 7 : 0), before, after);
      }
  }
  // 54: velocity right after a direction change, and after stopping
  {
    AuditRec& r = rec(54);
    HallSensor* h = new_hall(0);
    int count = 0;
    for (int k = 0; k < 20; k++) { fake_us += 2000; count++; hall_set(*h, hall_of_sector(wrap6(count))); h->update(); }
    fake_us += 2000; count--; hall_set(*h, hall_of_sector(wrap6(count))); h->update();
    float v_rev = h->getVelocity();
    r.cases++; if (v_rev > 0.0f) { r.fails++; worse(r, v_rev, v_rev); }
    fake_us += 500000; h->update();
    float v_stop = h->getVelocity();
    r.cases++; if (v_stop != 0.0f) { r.fails++; worse(r, fabsf(v_stop), v_rev, v_stop); }
  }
}



// ---------------------------------------------------------------- 60-61: dc_current and estimated_current torque modes
static void check_current_modes() {
  // 60: dc_current, rotor held: 0.5 A then -0.5 A (current magnitude from getDCCurrent)
  {
    AuditRec& r = rec(60);
    model_motor(1, MotionControlType::torque, TorqueControlType::dc_current, false);
    const float tg[] = {0.5f, -0.5f};
    for (int ph = 0; ph < 2; ph++) {
      for (int k = 0; k < 1500; k++) { sim_loop(tg[ph], 1); mot.w = 0; }
      float mag = sqrtf(mot.id * mot.id + mot.iq * mot.iq) * (mot.iq < 0 ? -1.0f : 1.0f);
      float err = fabsf(mag - tg[ph]) / 0.5f;
      r.cases++; if (err > 0.02f) r.fails++;
      worse(r, err, tg[ph], mag, mot.iq, mot.id);
    }
  }
  // 61: estimated_current with the right phase resistance, rotor held: 0.5 A then -0.5 A
  {
    AuditRec& r = rec(61);
    model_motor(1, MotionControlType::torque, TorqueControlType::estimated_current, false);
    m->phase_resistance = M_R;
    const float tg[] = {0.5f, -0.5f};
    for (int ph = 0; ph < 2; ph++) {
      for (int k = 0; k < 1500; k++) { sim_loop(tg[ph], 1); mot.w = 0; }
      float err = fabsf(mot.iq - tg[ph]) / 0.5f;
      r.cases++; if (err > 0.02f) r.fails++;
      worse(r, err, tg[ph], mot.iq, mot.id);
    }
  }
}

// ---------------------------------------------------------------- 62: velocity open loop, both directions
static void check_velocity_openloop() {
  AuditRec& r = rec(62);
  const float tg[] = {10.0f, -10.0f};
  for (int ph = 0; ph < 2; ph++) {
    new_motor(MotionControlType::velocity_openloop, TorqueControlType::voltage, false);
    float amp, prev, ang;
    float sum = 0; int n = 0;
    m->loopFOC(); m->move(tg[ph]);
    fake_us += 1000; m->loopFOC(); m->move(tg[ph]);
    vec(amp, prev);
    for (int k = 0; k < 500; k++) {
      fake_us += 1000;
      m->move(tg[ph]);     // the open loop angle is advanced in move(), applied by loopFOC()
      m->loopFOC();
      vec(amp, ang);
      sum += remainderf(ang - prev, _2PI); n++;
      prev = ang;
    }
    // electrical speed of the applied vector vs pole_pairs * target
    float w_el = sum / (n * 1e-3f);
    float err = fabsf(w_el - 7 * tg[ph]) / fabsf(7 * tg[ph]);
    r.cases++; if (err > 0.01f || fabsf(amp - 2.0f) > 0.05f) r.fails++;
    worse(r, fmaxf(err, fabsf(amp - 2.0f)), tg[ph], w_el, amp);
  }
}

// ---------------------------------------------------------------- 63-64: SmoothingSensor on a HallSensor
#if defined(EQUIV_AUDIT_DRIVERS)
static void check_smoothing() {
  for (int d = 0; d < 2; d++) {
    AuditRec& r = rec(63 + d);
    HallSensor* h = new_hall(0);
    BLDCMotor* mm = new (motor_buf) BLDCMotor(15);
    mm->sensor_direction = Direction::CW;
    alignas(SmoothingSensor) static uint8_t sbuf[sizeof(SmoothingSensor)];
    SmoothingSensor* s = new (sbuf) SmoothingSensor(*h, *mm);
    float w = (d ? -1.0f : 1.0f) * (_2PI / 90.0f) / 0.002f;   // 1 sector every 2 ms
    mm->shaft_velocity = w;
    int count = 0;
    uint32_t t_edge = fake_us;
    s->update();
    float prev = s->getAngle();
    for (int k = 0; k < 4000; k++) {
      fake_us += 100;
      if (fake_us - t_edge >= 2000) { t_edge += 2000; count += d ? -1 : 1; hall_set(*h, hall_of_sector(wrap6(count))); }
      s->update();
      float a = s->getAngle();
      if (k > 30) {
        // between two samples the smoothed angle must advance by w * 100 us, also across the hall edges
        float err = fabsf((a - prev) - w * 100e-6f) / (_2PI / 90.0f);   // in sectors
        r.cases++; if (err > 0.05f) r.fails++;
        worse(r, err, (float)count, a, prev, w * 100e-6f);
      }
      prev = a;
    }
  }
}
#endif

// ---------------------------------------------------------------- 65: stepper in angle open loop, negative angles
class FakeStepper : public StepperDriver {
public:
  int init() override { initialized = true; return 1; }
  void enable() override {}
  void disable() override {}
  void setPwm(float a, float b) override { Ua = a; Ub = b; }
  void setPhaseState(PhaseState, PhaseState) override {}
  float Ua = 0, Ub = 0;
};
FakeStepper fstepper;
alignas(StepperMotor) static uint8_t stepper_buf[sizeof(StepperMotor)];
static void check_stepper_openloop() {
  AuditRec& r = rec(65);
  StepperMotor* sm = new (stepper_buf) StepperMotor(50);
  fstepper.voltage_power_supply = 12; fstepper.voltage_limit = 12;
  fstepper.init();
  sm->linkDriver(&fstepper);
  sm->controller = MotionControlType::angle_openloop;
  sm->torque_controller = TorqueControlType::voltage;
  sm->voltage_limit = 2;
  sm->velocity_limit = 20;
  sm->init();
  sm->initFOC();
  for (int k = 0; k < 1000; k++) {
    fake_us += 1000;
    sm->loopFOC();
    sm->move(-2.0f);
    float amp = sqrtf(fstepper.Ua * fstepper.Ua + fstepper.Ub * fstepper.Ub);
    if (sm->shaft_angle < -0.1f) {
      float err = fabsf(amp - 2.0f);
      r.cases++; if (err > 0.05f) r.fails++;
      worse(r, err, sm->shaft_angle, amp);
    }
  }
}

// ---------------------------------------------------------------- 66-68: LPF and PID behaviour
static void check_lpf_pid() {
  // 66: LPF step response, Tf 10 ms, Ts 100 us: after 10 ms, 1 - (Tf/(Tf+Ts))^100 = 0.6303
  {
    AuditRec& r = rec(66);
    LowPassFilter f(0.01f);
    f(0.0f);
    float y = 0;
    for (int k = 0; k < 100; k++) { fake_us += 100; y = f(1.0f); }
    float err = fabsf(y - 0.63031f);
    r.cases++; if (err > 1e-3f) r.fails++;
    worse(r, err, y);
  }
  // 67: PID output ramp: P 1, ramp 100 /s, error step 10: output may rise by at most 100 * dt per call
  {
    AuditRec& r = rec(67);
    PIDController p(1.0f, 0.0f, 0.0f, 100.0f, 20.0f);
    p(0.0f);
    float prev = 0;
    for (int k = 0; k < 200; k++) {
      fake_us += 1000;
      float u = p(10.0f);
      float rate = (u - prev) / 1e-3f;
      r.cases++; if (rate > 100.0f * 1.01f) r.fails++;
      worse(r, rate - 100.0f, u, prev, rate);
      prev = u;
    }
  }
  // 68: PID anti-windup: I 100, limit 1, error +1 for 1 s then -1: the output must leave +1 at the first call
  {
    AuditRec& r = rec(68);
    PIDController p(0.0f, 100.0f, 0.0f, NOT_SET, 1.0f);
    for (int k = 0; k < 1000; k++) { fake_us += 1000; p(1.0f); }
    int calls = 0;
    float u = 1;
    while (u >= 0.999f && calls < 1000) { fake_us += 1000; u = p(-1.0f); calls++; }
    r.cases++; if (calls > 2) r.fails++;
    worse(r, (float)calls, u);
  }
}

// ---------------------------------------------------------------- 69: target when the motion control type changes
static void check_mode_change() {
  AuditRec& r = rec(69);
  model_motor(1, MotionControlType::velocity, TorqueControlType::voltage, false);
  sim_loop(10.0f, 1000);
  // velocity -> angle: the target becomes the present angle (no jump)
  m->updateMotionControlType(MotionControlType::angle);
  float e1 = fabsf(m->target - m->shaft_angle);
  // angle -> velocity: target 0
  m->target = 5;
  m->updateMotionControlType(MotionControlType::velocity);
  float e2 = fabsf(m->target);
  // velocity -> torque: target 0
  m->target = 5;
  m->updateMotionControlType(MotionControlType::torque);
  float e3 = fabsf(m->target);
  // torque -> angle_openloop: target = present angle
  m->updateMotionControlType(MotionControlType::angle_openloop);
  float e4 = fabsf(m->target - m->shaft_angle);
  float err = fmaxf(fmaxf(e1, e2), fmaxf(e3, e4));
  r.cases++; if (err > 1e-6f) r.fails++;
  worse(r, err, e1, e2, e3, e4);
}



// ---------------------------------------------------------------- 70-74: Commander values reaching the motor
// What the motor does after a mistyped / odd command (no validation in Commander::motor()).
static float applied_amp() { float amp, ang; vec(amp, ang); return amp; }
static void check_commander() {
  Commander cmd;   // no serial port: replies are not printed
  // 70: "LU-2" (voltage limit -2), torque voltage mode, target 0: the applied voltage must stay 0
  {
    AuditRec& r = rec(70);
    model_motor(1, MotionControlType::torque, TorqueControlType::voltage, false);
    cmd.motor(m, (char*)"LU-2");
    float amax = 0;
    for (int k = 0; k < 200; k++) { sim_loop(0.0f, 1); mot.w = 0; amax = fmaxf(amax, applied_amp()); }
    r.cases++; if (amax > 0.01f) r.fails++;
    worse(r, amax, m->voltage_limit, amax, m->voltage.q);
  }
  // 71: "LUnan" (atof gives NaN), target 10 V: the applied voltage must not exceed the previous limit 6 V
  {
    AuditRec& r = rec(71);
    model_motor(1, MotionControlType::torque, TorqueControlType::voltage, false);
    cmd.motor(m, (char*)"LUnan");
    float amax = 0;
    for (int k = 0; k < 200; k++) { sim_loop(10.0f, 1); mot.w = 0; amax = fmaxf(amax, applied_amp()); }
    r.cases++; if (amax > 6.01f) r.fails++;
    worse(r, amax - 6.0f, m->voltage_limit, amax, m->voltage.q);
  }
  // 72: "WT9" (modulation type 9, not a valid FOCModulationType), then target 2 V and back to 0
  {
    AuditRec& r = rec(72);
    model_motor(1, MotionControlType::torque, TorqueControlType::voltage, false);
    sim_loop(2.0f, 50);
    cmd.motor(m, (char*)"WT9");
    float amax = 0;
    for (int k = 0; k < 200; k++) { sim_loop(0.0f, 1); mot.w = 0; amax = fmaxf(amax, applied_amp()); }
    r.cases++; if (amax > 0.01f) r.fails++;
    worse(r, amax, (float)(int)m->foc_modulation, amax);
  }
  // 73: "LC-1" (current limit -1), foc_current torque mode, target 0: the true current must stay ~0
  {
    AuditRec& r = rec(73);
    model_motor(1, MotionControlType::torque, TorqueControlType::foc_current, false);
    cmd.motor(m, (char*)"LC-1");
    float imax = 0;
    for (int k = 0; k < 600; k++) { sim_loop(0.0f, 1); mot.w = 0; imax = fmaxf(imax, fabsf(mot.iq)); }
    r.cases++; if (imax > 0.05f) r.fails++;
    worse(r, imax, m->current_limit, imax, m->current_sp);
  }
  // 74: "LV-5" (velocity limit -5), angle mode, target +3 rad: must still reach the target
  {
    AuditRec& r = rec(74);
    model_motor(1, MotionControlType::angle, TorqueControlType::voltage, false);
    cmd.motor(m, (char*)"LV-5");
    sim_loop(3.0f, 8000);
    float err = fabsf(m->shaft_angle - 3.0f);
    r.cases++; if (err > 0.05f) r.fails++;
    worse(r, err, m->velocity_limit, m->shaft_angle, mot.w);
  }
}


void equiv_audit_setup() {
  memset(audit, 0, sizeof(audit));
  check_sincos();
  check_openloop();
  check_modulation();
  check_sector();
  check_current();
  check_sensor();
  check_closed_loop();
  check_hall();
  check_current_modes();
  check_velocity_openloop();
#if defined(EQUIV_AUDIT_DRIVERS)
  check_smoothing();
#endif
  check_stepper_openloop();
  check_lpf_pid();
  check_mode_change();
  check_commander();
  audit_state.state = 1;
}
#else
void equiv_audit_setup() {}
#endif
