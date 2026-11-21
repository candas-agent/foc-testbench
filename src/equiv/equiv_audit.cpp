/**
 * On-target audit of SimpleFOC (dev) functions, no peripheral at all (fake driver / current sense / sensor / clock,
 * link option -Wl,--wrap=_Z7_microsv). Each check fills one AuditRec; read with tools/equiv_audit_read.py.
 * Things that differ between a PC and the MCU (float -> unsigned conversion of negative values, soft float) are
 * the reason to run this on the target.
 */
#include <Arduino.h>
#include <SimpleFOC.h>
#include <new>

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
#define N_AUDIT 40
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

void equiv_audit_setup() {
  memset(audit, 0, sizeof(audit));
  check_sincos();
  check_openloop();
  check_modulation();
  check_sector();
  check_current();
  check_sensor();
  audit_state.state = 1;
}
#else
void equiv_audit_setup() {}
#endif
