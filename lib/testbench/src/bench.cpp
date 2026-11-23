#include "bench.h"
#include "profile.h"

#ifdef BENCH_USE_RTT
#include <RTTStream.h>
static RTTStream rtt;
Stream& bench_io = rtt;
#else
Stream& bench_io = Serial;
#endif

// Ld must be given: with 4 arguments only Lq is set, Ld stays NOT_SET (-12345) and tuneCurrentController()
// then computes PID_current_d.P = -12345 * 2*pi*BW (positive feedback, instant overcurrent in foc_current)
BLDCMotor motor = BLDCMotor(POLE_PAIRS, PHASE_RESISTANCE, KV_RATING, PHASE_INDUCTANCE, PHASE_INDUCTANCE);
BLDCDriver6PWM driver = BLDCDriver6PWM(DRIVER_PINS);
LowsideCurrentSense currentSense = LowsideCurrentSense(CS_SHUNT, CS_GAIN, CS_PINS);
Commander command = Commander(bench_io);
MXLEMMINGObserverSensor observer = MXLEMMINGObserverSensor(motor);  // after motor: reads its KV and pole pairs
#ifdef HALL_A_PIN
#ifdef BENCH_HALL_TABLE_LOWER
#include "hall_table.h"
static HallSensorTs hall = HallSensorTs(HALL_A_PIN, HALL_B_PIN, HALL_C_PIN, POLE_PAIRS);
#else
static HallSensor hall = HallSensor(HALL_A_PIN, HALL_B_PIN, HALL_C_PIN, POLE_PAIRS);
#endif
// interpolates between the 6 hall states per electrical revolution (sets its own phase correction)
static SmoothingSensor smooth = SmoothingSensor(hall, motor);
#ifdef BENCH_HALL_TABLE_LOWER
static HallTableSensor hall_table = HallTableSensor(hall, motor, (int8_t)SENSOR_DIRECTION);
static const float hall_table_lower[8] = BENCH_HALL_TABLE_LOWER, hall_table_width[8] = BENCH_HALL_TABLE_WIDTH;
static const float hall_ideal_lower[8] = BENCH_HALL_IDEAL_LOWER, hall_ideal_width[8] = BENCH_HALL_IDEAL_WIDTH;
static int hall_mode = 0;  // 0 SmoothingSensor, 1 measured table, 2 ideal grid (same class as 1)
#endif
// Hall edge log: every sector change timestamped in the interrupt (micros()), read by pyOCD (tools/board_mem.py
// read_hall_edges). Exact edge times, independent of how fast the PC polls dbg (~370 reads/s through the ST-Link).
// Ring buffer: entry k is in hall_edges[k % HALL_EDGE_N], hall_edge_n counts all edges since boot.
#ifdef HALL_EDGE_N
extern "C" {
HallEdge hall_edges[HALL_EDGE_N];
volatile uint32_t hall_edge_n = 0;
}
static void hallLog() {
  static int32_t last = 0x7FFFFFFF;
  int32_t c = (int32_t)(hall.electric_rotations * 6 + hall.electric_sector);
  if (c == last) return;              // glitch: interrupt without a sector change
  last = c;
  uint32_t n = hall_edge_n;
  hall_edges[n % HALL_EDGE_N].t_us = micros();
  hall_edges[n % HALL_EDGE_N].count = c;
  hall_edge_n = n + 1;                // published last: the PC never reads a half written entry as new
}
#else
static inline void hallLog() {}
#endif
static void hallA() { hall.handleA(); hallLog(); }
static void hallB() { hall.handleB(); hallLog(); }
static void hallC() { hall.handleC(); hallLog(); }
#endif
int sensor_aligned = 0;
#ifdef HALL_A_PIN
// XG<state>[,<loops>]: invalid hall state injected for some loops (hall_glitch.cpp, see docs/equiv-tests.md)
void hall_glitch_set(HallSensor& h, int state);
void hall_glitch_restore(HallSensor& h);
static volatile int glitch_pending = 0, glitch_state = 0, glitch_loops = 1, glitch_left = 0;
#endif

volatile DebugVars dbg;
float openloop_voltage = OPENLOOP_VOLTAGE;
int equal_amplitude = 0;
int observer_on = OBSERVER_DEFAULT_ON;

uint32_t run_timeout_s = RUN_TIMEOUT_S;
float overcurrent_a = OVERCURRENT_A;
int heartbeat_on = 0;
volatile int fault = 0;

static uint32_t enable_ms = 0;
static int8_t was_enabled = 0;
static int overcurrent_cnt = 0;
static uint32_t last_rx_ms = 0;

// ---------------------------------------------------------------------------
// Dead-time compensation
// ---------------------------------------------------------------------------
// BENCH_NO_DEADTIME: plain SimpleFOC (master/dev) without the dead-time compensation API,
// e.g. to measure a library change on its own. The D commands then only print/set the bench settings.
void computeCompensation(float& ca, float& cb, float& cc) {
  ca = cb = cc = 0;
#ifndef BENCH_NO_DEADTIME
  if (motor.deadtime_compensation <= 0 || motor.motor_status != FOCMotorStatus::motor_ready) return;
  if (motor.foc_modulation != FOCModulationType::SinePWM && motor.foc_modulation != FOCModulationType::SpaceVectorPWM) return;
  float r = (motor.torque_controller == TorqueControlType::voltage && _isset(motor.phase_resistance) && motor.phase_resistance > 0) ? motor.phase_resistance : 1.0f;
  float th = motor.deadtime_comp_threshold > 0 ? motor.deadtime_comp_threshold : 1e-3f;
  float gain = motor.deadtime_compensation / (th * r);
  float s, c;
  _sincos(motor.electrical_angle, &s, &c);
  float Ia = c * motor.deadtime_comp_current.d - s * motor.deadtime_comp_current.q;
  float Ibeta = s * motor.deadtime_comp_current.d + c * motor.deadtime_comp_current.q;
  float Ib = -0.5f * Ia + _SQRT3_2 * Ibeta;
  float Ic = -Ia - Ib;
  float dc = motor.deadtime_compensation;
  ca = _constrain(Ia * gain, -dc, dc);
  cb = _constrain(Ib * gain, -dc, dc);
  cc = _constrain(Ic * gain, -dc, dc);
#endif
}

static bool isOpenLoop() {
  return motor.controller == MotionControlType::velocity_openloop || motor.controller == MotionControlType::angle_openloop;
}

// In open loop (voltage mode) the voltage amplitude is the voltage limit: it is set from openloop_voltage,
// optionally lowered to keep the same fundamental amplitude with and without compensation.
// In the other modes the voltage limit is FOC_VOLTAGE_LIMIT.
// XR1: plain SimpleFOC behaviour for bug reproduction (docs/equiv-tests.md): the bench no longer sets the voltage
// limit every loop nor forces foc_current in closed loop. XR0 (default) restores the bench behaviour.
static int raw_simplefoc = 0;
// XK<n>: voltage feed-forward in foc_current, computed before each loopFOC (feed_forward_voltage, SimpleFOC dev):
// 0 off, 1 back-EMF (estimateBEMF, from KV), 2 + R * current setpoint, 3 + dq decoupling (L * electrical speed)
static int ff_mode = 0;

static void updateVoltageLimit() {
  if (raw_simplefoc) return;
  if (isOpenLoop() && motor.torque_controller == TorqueControlType::voltage) {
    float u = openloop_voltage;
#ifndef BENCH_NO_DEADTIME
    if (equal_amplitude) u -= (4.0f / _PI) * motor.deadtime_compensation;
#endif
    motor.updateVoltageLimit(u > 0 ? u : 0);
  } else {
    // updateVoltageLimit() and not voltage_limit = : the current PI limits must follow. SimpleFOC copies
    // voltage_limit into them on every motion/torque mode change (MC/MT commands), so after a voltage mode
    // run they kept the small open loop voltage and capped the current controllers.
    motor.updateVoltageLimit(FOC_VOLTAGE_LIMIT);
  }
}

float bench_calibrate(float max_current) {
#ifdef BENCH_NO_DEADTIME
  (void)max_current;
  bench_io.println(F("No dead-time compensation in this build"));
  return NOT_SET;
#else
  if (max_current <= 0) max_current = CALIB_MAX_CURRENT;
  if (max_current > CALIB_MAX_CURRENT_LIMIT) max_current = CALIB_MAX_CURRENT_LIMIT;
  bench_io.print(F("Dead-time calibration, max current [A]: "));
  bench_io.println(max_current, 3);
  bool en = motor.enabled;
  if (!en) motor.enable();
  motor.updateVoltageLimit(FOC_VOLTAGE_LIMIT);
  float udt = motor.calibrateDeadtimeCompensation(max_current);
  updateVoltageLimit();
  if (!en) motor.disable();
  if (_isset(udt)) {
    bench_io.print(F("Measured compensation [V]: "));
    bench_io.println(udt, 4);
    bench_io.print(F("Ratio to Vbus*dead_zone: "));
    bench_io.println(udt / (SUPPLY_VOLTAGE * DEAD_ZONE), 3);
  } else {
    bench_io.println(F("Calibration failed"));
  }
  return udt;
#endif
}

// ---------------------------------------------------------------------------
// Recorder
// ---------------------------------------------------------------------------
extern "C" {
Sample rec_buf[RECORD_SAMPLES];
volatile int32_t rec_n = 0;
volatile int32_t rec_running = 0;
}
static bool rec_auto_dump = false;
static uint32_t rec_period_us = 0, rec_t0 = 0, rec_last = 0;

void rec_start(uint32_t period_us, bool auto_dump) {
  rec_period_us = period_us;
  rec_auto_dump = auto_dump;
  rec_n = 0;
  rec_t0 = rec_last = _micros();
  rec_running = 1;
}
bool rec_done() { return !rec_running && rec_n > 0; }
int rec_count() { return rec_n; }
const Sample* rec_data() { return rec_buf; }

void rec_dump(Print& out) {
  out.println(F("REC_BEGIN"));
  out.println(F("t_us,ia,comp_a,angle_err"));
  for (int k = 0; k < rec_n; k++) {
    out.print(rec_buf[k].t_us);
    out.print(',');
    out.print(rec_buf[k].ia, 4);
    out.print(',');
    out.print(rec_buf[k].comp_a, 4);
    out.print(',');
    out.println(rec_buf[k].angle_err, 4);
  }
  out.println(F("REC_END"));
}

static void rec_update() {
  if (!rec_running) return;
  uint32_t now = _micros();
  if (rec_n > 0 && now - rec_last < rec_period_us) return;
  rec_last = now;
  rec_buf[rec_n].t_us = now - rec_t0;
  rec_buf[rec_n].ia = dbg.ia;
#if defined(BENCH_NO_DEADTIME) && defined(HALL_A_PIN)
  rec_buf[rec_n].comp_a = (float)hall.hall_state;   // no compensation in this build: the hall state instead (XG tests)
#else
  rec_buf[rec_n].comp_a = dbg.comp_a;
#endif
  rec_buf[rec_n].angle_err = observer_on ? dbg.angle_err : motor.current_sp;  // observer off: current setpoint
  rec_buf[rec_n].iq = motor.current.q;
  rec_buf[rec_n].en = motor.enabled ? 1.0f : 0.0f;
  if (++rec_n >= RECORD_SAMPLES) {
    rec_running = 0;
    if (rec_auto_dump) {
      // dumping blocks the loop: stop the motor first (open loop would stall)
      bench_disable(F("recording done"));
      rec_dump(bench_io);
    }
  }
}

float zero_crossing_flatness(const Sample* s, int n) {
  if (n < 10) return NAN;
  float mean = 0;
  for (int k = 0; k < n; k++) mean += s[k].ia;
  mean /= n;
  float sq = 0;
  for (int k = 0; k < n; k++) sq += (s[k].ia - mean) * (s[k].ia - mean);
  float peak = sqrtf(2.0f * sq / n);
  if (peak <= 0) return NAN;
  int small = 0;
  for (int k = 0; k < n; k++) if (fabsf(s[k].ia - mean) < 0.1f * peak) small++;
  return (float)small / n;
}

float angle_error_ripple(const Sample* s, int n) {
  if (n < 10) return NAN;
  float sc = 0, cc = 0;
  for (int k = 0; k < n; k++) { sc += sinf(s[k].angle_err); cc += cosf(s[k].angle_err); }
  float mean = atan2f(sc, cc);
  float sq = 0;
  for (int k = 0; k < n; k++) {
    float d = s[k].angle_err - mean;
    while (d > _PI) d -= _2PI;
    while (d < -_PI) d += _2PI;
    sq += d * d;
  }
  return sqrtf(sq / n);
}

// ---------------------------------------------------------------------------
// Safety and debug
// ---------------------------------------------------------------------------
void bench_disable(const __FlashStringHelper* reason) {
  motor.disable();
  was_enabled = 0;
  if (reason) {
    bench_io.print(F("MOTOR DISABLED: "));
    bench_io.println(reason);
  }
}

static void safety() {
  if (motor.enabled && !was_enabled) enable_ms = millis();
  was_enabled = motor.enabled;
  if (!motor.enabled) { overcurrent_cnt = 0; return; }

  if (run_timeout_s && millis() - enable_ms > run_timeout_s * 1000UL) {
    fault = 1;
    bench_disable(F("run timeout"));
    return;
  }
  float imax = max(fabsf(dbg.ia), max(fabsf(dbg.ib), fabsf(dbg.ic)));
  overcurrent_cnt = imax > overcurrent_a ? overcurrent_cnt + 1 : 0;
  if (overcurrent_cnt > OVERCURRENT_SAMPLES) {
    fault = 2;
    bench_disable(F("overcurrent"));
    return;
  }
  if (heartbeat_on && millis() - last_rx_ms > HEARTBEAT_TIMEOUT_MS) {
    fault = 3;
    bench_disable(F("heartbeat lost"));
  }
}

static void updateDebug() {
  PhaseCurrent_s i = currentSense.getPhaseCurrents();
  dbg.ia = i.a;
  dbg.ib = i.b;
  dbg.ic = i.c;
  float ca, cb, cc;
  computeCompensation(ca, cb, cc);
  dbg.comp_a = ca;
  dbg.comp_b = cb;
  dbg.comp_c = cc;
  dbg.iq = motor.current.q;
  dbg.id = motor.current.d;
  dbg.uq = motor.voltage.q;
  dbg.ud = motor.voltage.d;
  dbg.electrical_angle = motor.electrical_angle;
  dbg.velocity = motor.shaft_velocity;
  dbg.loop_us = (float)motor.loopfoc_time_us;
  float e = 0;
  dbg.obs_angle = observer_on ? observer.electrical_angle : 0;
  if (observer_on) e = observer.electrical_angle - _normalizeAngle(motor.electrical_angle);
  if (e > _PI) e -= _2PI;
  if (e < -_PI) e += _2PI;
  dbg.angle_err = e;
  dbg.fault = (uint32_t)fault;
  dbg.enabled = (uint32_t)motor.enabled;
#ifdef HALL_A_PIN
  dbg.hall = (uint32_t)hall.hall_state;
  // sensor electrical angle (with the alignment), also in open loop where motor.electrical_angle is the open loop angle
  dbg.sensor_angle = sensor_aligned ? motor.electricalAngle() : 0;
  dbg.hall_count = (int32_t)(hall.electric_rotations * 6 + hall.electric_sector);
#else
  dbg.hall = 0;
  dbg.sensor_angle = 0;
  dbg.hall_count = 0;
#endif
}

// ---------------------------------------------------------------------------
// Serial commands
// ---------------------------------------------------------------------------
static bool hasArg(const char* p) { return *p != '\0' && *p != '\n' && *p != '\r'; }

static void printDeadtime() {
#ifndef BENCH_NO_DEADTIME
  bench_io.print(F("comp [V]: "));       bench_io.println(motor.deadtime_compensation, 4);
  bench_io.print(F("threshold: "));      bench_io.println(motor.deadtime_comp_threshold, 4);
  bench_io.print(F("use measured: "));   bench_io.println(motor.deadtime_comp_use_measured);
#endif
  bench_io.print(F("equal ampl.: "));    bench_io.println(equal_amplitude);
  bench_io.print(F("observer: "));       bench_io.println(observer_on);
  bench_io.print(F("openloop U [V]: ")); bench_io.println(openloop_voltage, 4);
  bench_io.print(F("expected Vbus*dead_zone [V]: ")); bench_io.println(SUPPLY_VOLTAGE * DEAD_ZONE, 4);
}

static void doDeadtime(char* cmd) {
  switch (cmd[0]) {
    case '\0': case '\n': case '\r':
      printDeadtime();
      break;
#ifndef BENCH_NO_DEADTIME
    case 'T':  // threshold
      command.scalar(&motor.deadtime_comp_threshold, cmd + 1);
      break;
    case 'M':  // use measured currents (foc_current)
      if (hasArg(cmd + 1)) motor.deadtime_comp_use_measured = atoi(cmd + 1) ? 1 : 0;
      bench_io.println(motor.deadtime_comp_use_measured);
      break;
#endif
    case 'E':  // equal amplitude in open loop
      if (hasArg(cmd + 1)) equal_amplitude = atoi(cmd + 1) ? 1 : 0;
      bench_io.println(equal_amplitude);
      break;
    case 'B':  // open loop voltage amplitude
      command.scalar(&openloop_voltage, cmd + 1);
      break;
    case 'O':  // MXLEMMING observer on/off (heavy on MCUs without FPU)
      if (hasArg(cmd + 1)) observer_on = atoi(cmd + 1) ? 1 : 0;
      bench_io.println(observer_on);
      break;
    case 'C':  // calibration
      bench_calibrate((float)atof(cmd + 1));
      break;
    default:   // compensation value
#ifndef BENCH_NO_DEADTIME
      command.scalar(&motor.deadtime_compensation, cmd);
#else
      printDeadtime();
#endif
      break;
  }
}

static void doMotor(char* cmd) {
  command.motor(&motor, cmd);
  if (!isOpenLoop()) {
    if (!sensor_aligned) {
      // no aligned position sensor: only the open loop motion controls are possible
      motor.controller = MotionControlType::velocity_openloop;
      bench_io.println(F("No aligned sensor: only open loop allowed (MC3 velocity, MC4 angle), back to velocity open loop"));
    } else if (!raw_simplefoc && motor.torque_controller != TorqueControlType::foc_current) {
      // closed loop with current control only: in voltage mode the torque target is in volts (1 V ~ 2 A)
      motor.torque_controller = TorqueControlType::foc_current;
      bench_io.println(F("Closed loop: torque control forced to foc_current (MT2)"));
    }
  }
}

#ifdef HALL_A_PIN
static void printSensor() {
  bench_io.print(F("hall state: "));      bench_io.println(hall.hall_state);
  bench_io.print(F("hall sector: "));     bench_io.println(hall.electric_sector);
  bench_io.print(F("aligned: "));         bench_io.println(sensor_aligned);
  bench_io.print(F("zero el. angle: "));  bench_io.println(motor.zero_electric_angle, 4);
  bench_io.print(F("direction: "));       bench_io.println((int)motor.sensor_direction);
#ifdef BENCH_HALL_TABLE_LOWER
  bench_io.print(F("hall mode: "));       bench_io.println(hall_mode);
  bench_io.print(F("velocity mode: "));   bench_io.println(hall_table.vel_mode);
  bench_io.print(F("el. angle [deg]: ")); bench_io.println(motor.electricalAngle() * 180.0f / _PI, 1);
#endif
}

// S: sensor state, SA: link the hall sensors and run the alignment (THE MOTOR MOVES, motor disabled after)
static void doSensor(char* cmd) {
  if (cmd[0] == 'A') {
    if (motor.enabled) { bench_io.println(F("Disable the motor first (ME0)")); return; }
    bench_io.println(F("Automatic sensor alignment, the motor moves..."));
    motor.zero_electric_angle = NOT_SET;
    motor.sensor_direction = Direction::UNKNOWN;
    motor.enable();
    motor.updateVoltageLimit(FOC_VOLTAGE_LIMIT);
    int ok = motor.initFOC();
    motor.disable();
    sensor_aligned = ok ? 1 : 0;
    updateVoltageLimit();
    bench_io.println(ok ? F("Alignment done") : F("ALIGNMENT FAILED"));
  }
#ifdef BENCH_HALL_TABLE_LOWER
  // ST0 SmoothingSensor (known alignment), ST1 measured hall table, ST2 ideal 60 deg grid (rotor frame, CW, zero 0)
  if (cmd[0] == 'V' && hasArg(cmd + 1)) hall_table.vel_mode = (uint8_t)atoi(cmd + 1);  // SV0..4 velocity estimate
  if (cmd[0] == 'T' && hasArg(cmd + 1)) {
    if (motor.enabled) { bench_io.println(F("Disable the motor first (ME0)")); return; }
    int m = atoi(cmd + 1);
    if (m >= 1 && m <= 4) {
      // 1 measured table, 2 ideal grid; 3 / 4 kept as aliases of 2 / 1 (velocity estimate chosen with SV)
      bool table = (m == 1 || m == 4);
      hall_table.setTable(table ? hall_table_lower : hall_ideal_lower, table ? hall_table_width : hall_ideal_width);
      hall_table.update();
      motor.linkSensor(&hall_table);
      motor.zero_electric_angle = 0;
      motor.sensor_direction = Direction::CW;
      hall_mode = m;
    } else {
      smooth.update();
      motor.linkSensor(&smooth);
      motor.zero_electric_angle = SENSOR_ZERO_ANGLE;
      motor.sensor_direction = SENSOR_DIRECTION;
      hall_mode = 0;
    }
  }
#endif
  printSensor();
}
#endif

#ifdef BENCH_VBUS_RUNTIME_PIN
static float vbus_rt = 0;  // run time bus voltage [V], see readVbusRuntime()
static uint32_t vbus_rt_reads = 0, vbus_rt_errors = 0, vbus_rt_last_ms = 0;
#ifndef BENCH_VBUS_RUNTIME_MS
#define BENCH_VBUS_RUNTIME_MS 20  // [ms] period of the run time reads, 0: every loop
#endif
#ifdef BENCH_VBUS_RUNTIME_ALT_PIN
// stale value check: reads alternate with another ADC pin at a different voltage, a Vbus read more than 10 % away
// from the start-up value is counted (a stale result would give the value of the other pin)
static float vbus_alt = 0;
static uint32_t vbus_alt_bad = 0;
#endif
#endif

float friction_ff_a = 0;  // [A] friction feed-forward in velocity mode (XF command)
// Velocity through a position ramp (V command): angle mode, target advanced by ramp_v * dt each loop, ramp_v also
// given as velocity feed-forward. The angle loop catches up what is lost while the wheel sticks.
static int8_t ramp_on = 0;
static float ramp_v = 0;
static uint32_t ramp_us = 0;
static void doRamp(char* cmd) {
  if (cmd[0] == 'O' || !hasArg(cmd)) {  // VO: stop, back to velocity mode at 0
    ramp_on = 0;
    motor.feed_forward_velocity = 0;
    motor.target = 0;
    motor.controller = MotionControlType::velocity;
  } else {
    ramp_v = (float)atof(cmd);
    if (!ramp_on) motor.target = motor.shaft_angle;
    motor.controller = MotionControlType::angle;
    ramp_on = 1;
    ramp_us = micros();
  }
  bench_io.print(F("ramp: ")); bench_io.print(ramp_on); bench_io.print(' '); bench_io.println(ramp_v, 3);
}

static void printSafety() {
  bench_io.print(F("timeout [s]: "));     bench_io.println(run_timeout_s);
  bench_io.print(F("overcurrent [A]: ")); bench_io.println(overcurrent_a, 2);
  bench_io.print(F("heartbeat: "));       bench_io.println(heartbeat_on);
  bench_io.print(F("last fault: "));      bench_io.println(fault);
  bench_io.print(F("Vbus at start-up [V]: ")); bench_io.println(dbg.vbus, 2);
#ifdef BENCH_VBUS_RUNTIME_PIN
  bench_io.print(F("Vbus run time [V]: ")); bench_io.print(vbus_rt, 2);
  bench_io.print(F(" reads ")); bench_io.print(vbus_rt_reads);
  bench_io.print(F(" errors ")); bench_io.println(vbus_rt_errors);
#ifdef BENCH_VBUS_RUNTIME_ALT_PIN
  bench_io.print(F("alt pin [V]: ")); bench_io.print(vbus_alt, 3);
  bench_io.print(F(" stale reads (Vbus off by >10 % or alt pin = Vbus): ")); bench_io.println(vbus_alt_bad);
#endif
#endif
  bench_io.print(F("friction ff [A]: ")); bench_io.println(friction_ff_a, 3);
}

// bus voltage, measured once before the current sense takes the ADC
static void measureVbus() {
#if defined(VBUS_PIN) && !defined(BENCH_NO_VBUS_STARTUP)
  pinMode(VBUS_PIN, INPUT_ANALOG);
  float sum = 0;
  const int n = 16;
  for (int k = 0; k < n; k++) sum += analogRead(VBUS_PIN);
  // Arduino analogRead: 10 bit by default
  dbg.vbus = sum / n / 1023.0f * 3.3f * VBUS_GAIN;
  bench_io.print(F("Vbus measured [V]: "));
  bench_io.println(dbg.vbus, 2);
  if (fabsf(dbg.vbus - SUPPLY_VOLTAGE) > VBUS_TOLERANCE * SUPPLY_VOLTAGE) {
    bench_io.print(F("WARNING: Vbus differs from SUPPLY_VOLTAGE = "));
    bench_io.println(SUPPLY_VOLTAGE, 1);
  }
#else
  dbg.vbus = NAN;
#endif
}

// XO: re-centre the current sense offsets while the motor is enabled in open loop at 0 V (DB0, MC4, MT0, ME1):
// the real current is 0 there, so the mean measured current is the offset error (drift since start-up).
static void trimOffsets() {
  if (!(motor.enabled && isOpenLoop() && motor.voltage_limit <= 0.0f)) {
    bench_io.println(F("XO: enable the motor in open loop at 0 V first (DB0, MC4, MT0, ME1)"));
    return;
  }
  const int n = 2000;
  float sa = 0, sb = 0;
  for (int k = 0; k < n; k++) {
    PhaseCurrent_s c = currentSense.getPhaseCurrents();
    sa += c.a; sb += c.b;
    delayMicroseconds(100);
  }
  sa /= n; sb /= n;
  currentSense.offset_ia += sa / currentSense.gain_a;
  currentSense.offset_ib += sb / currentSense.gain_b;
  bench_io.print(F("offset error removed [A]: a ")); bench_io.print(sa, 4);
  bench_io.print(F(" b ")); bench_io.println(sb, 4);
}

static void doSafety(char* cmd) {
  switch (cmd[0]) {
    case 'T': if (hasArg(cmd + 1)) run_timeout_s = (uint32_t)atol(cmd + 1); break;
    case 'I': if (hasArg(cmd + 1)) overcurrent_a = (float)atof(cmd + 1); break;
    case 'H': if (hasArg(cmd + 1)) heartbeat_on = atoi(cmd + 1) ? 1 : 0; break;
    case 'C': fault = 0; break;
    case 'F': if (hasArg(cmd + 1)) friction_ff_a = (float)atof(cmd + 1); break;  // XF0.3: friction feed-forward [A]
    case 'M': {  // XM<volt>[,<comp V>]: characteriseMotor(volt) as the SimpleFOC command MFP, optionally with the
                 // dead-time compensation forced along +d (the measurement only applies positive d voltages; loopFOC,
                 // which normally gives the current direction, does not run during the measurement)
      float v = hasArg(cmd + 1) ? (float)atof(cmd + 1) : 0.8f;
      const char* c2 = strchr(cmd + 1, ',');
      float comp = c2 ? (float)atof(c2 + 1) : 0.0f;
#ifndef BENCH_NO_DEADTIME
      float comp_prev = motor.deadtime_compensation;
      motor.deadtime_compensation = comp;
      motor.deadtime_comp_current = {10.0f, 0.0f};
#else
      if (comp != 0.0f) bench_io.println(F("no dead-time compensation in this build: measured without"));
#endif
      int res = motor.characteriseMotor(v);   // BLDCMotor: correction factor 1.5, as MFP
#ifndef BENCH_NO_DEADTIME
      motor.deadtime_compensation = comp_prev;
#endif
      bench_io.print(F("characterise: result ")); bench_io.print(res);
      bench_io.print(F(", R [Ohm] ")); bench_io.print(motor.phase_resistance, 4);
      bench_io.print(F(", Ld [mH] ")); bench_io.print(motor.axis_inductance.d * 1000.0f, 4);
      bench_io.print(F(", Lq [mH] ")); bench_io.println(motor.axis_inductance.q * 1000.0f, 4);
      return;
    }
    case 'K':  // XK0..XK3: voltage feed-forward mode (see ff_mode)
      if (hasArg(cmd + 1)) ff_mode = atoi(cmd + 1);
      if (ff_mode < 0 || ff_mode > 3) ff_mode = 0;
      if (ff_mode == 0) motor.feed_forward_voltage = {0.0f, 0.0f};
      bench_io.print(F("voltage feed-forward mode: ")); bench_io.println(ff_mode);
      return;
    case 'R':  // XR1 / XR0: plain SimpleFOC limits and torque modes (bug reproduction) / bench behaviour
      if (hasArg(cmd + 1)) raw_simplefoc = atoi(cmd + 1) ? 1 : 0;
      bench_io.print(F("plain SimpleFOC mode: ")); bench_io.println(raw_simplefoc);
      return;
#ifdef HALL_A_PIN
    case 'G': {  // XG0 / XG7[,loops]: invalid hall state 000 / 111 for 1 (or <loops>) control loops
      int st = hasArg(cmd + 1) ? atoi(cmd + 1) : 0;
      const char* c2 = strchr(cmd + 1, ',');
      int n = c2 ? atoi(c2 + 1) : 1;
      if (st != 0 && st != 7) st = 0;
      glitch_state = st; glitch_loops = n < 1 ? 1 : (n > 100 ? 100 : n); glitch_pending = 1;
      bench_io.print(F("hall glitch: state ")); bench_io.print(st); bench_io.print(F(", loops ")); bench_io.println(glitch_loops);
      return;
    }
#endif
    case 'O':  // XO: re-centre the offsets (open loop, 0 V, enabled); XO<da>,<db>: shift the measured ia, ib by da, db [A]
      if (hasArg(cmd + 1)) {
        float da = (float)atof(cmd + 1);
        const char* c2 = strchr(cmd + 1, ',');
        float db = c2 ? (float)atof(c2 + 1) : 0.0f;
        currentSense.offset_ia -= da / currentSense.gain_a;  // i = (v - offset) * gain
        currentSense.offset_ib -= db / currentSense.gain_b;
        bench_io.print(F("measured currents shifted [A]: a ")); bench_io.print(da, 4);
        bench_io.print(F(" b ")); bench_io.println(db, 4);
      } else {
        trimOffsets();
      }
      return;
#ifdef BENCH_PROFILE
    case 'P': prof_print(bench_io); return;   // XP: loop profile
#ifdef BENCH_MICROBENCH
    case 'B': prof_bench(bench_io); return;   // XB: micro benchmarks (motor disabled)
#endif
#endif
    default: break;
  }
  printSafety();
}

static void doPing(char*) {}  // heartbeat, silent

static void doRecord(char* cmd) {
  if (cmd[0] == 'D') { rec_dump(bench_io); return; }  // RD: dump the last recording
  bool dump = true;
  if (cmd[0] == 'N') { dump = false; cmd++; }       // RN: no serial dump (read with pyOCD)
  uint32_t period = hasArg(cmd) ? (uint32_t)atol(cmd) : 1000;
  bench_io.print(F("Recording, period [us]: "));
  bench_io.println(period);
  rec_start(period, dump);
}

// ---------------------------------------------------------------------------
void bench_setup() {
#ifdef SELF_HOLD_PIN
  // power latch first: the board switches itself off when the power button is released
  pinMode(SELF_HOLD_PIN, OUTPUT);
  digitalWrite(SELF_HOLD_PIN, HIGH);
#endif
#ifndef BENCH_USE_RTT
  Serial.begin(BENCH_SERIAL_BAUD);
#endif
  SimpleFOCDebug::enable(&bench_io);
  _delay(1000);
  bench_io.println(F("FOC test bench, board: " BENCH_BOARD_NAME));
  measureVbus();

  // no position sensor linked: open loop only

  driver.voltage_power_supply = SUPPLY_VOLTAGE;
  driver.pwm_frequency = BENCH_PWM_FREQUENCY;
  driver.dead_zone = DEAD_ZONE;
#ifdef DRIVER_VOLTAGE_LIMIT_RATIO
  driver.voltage_limit = SUPPLY_VOLTAGE * DRIVER_VOLTAGE_LIMIT_RATIO;
#endif
  driver.init();
  motor.linkDriver(&driver);

  // current sense: the phases are wired to known pins, no alignment needed
  currentSense.linkDriver(&driver);
#ifdef CS_CALIBRATE_DRIVER_ENABLED
  // offsets measured with the low side MOSFETs switching (current sense init sets all phases to 50 %: 0 V)
  driver.enable();
#endif
  currentSense.init();
#ifdef CS_CALIBRATE_DRIVER_ENABLED
  driver.disable();
#endif
  currentSense.skip_align = true;
  motor.linkCurrentSense(&currentSense);

  motor.voltage_sensor_align = ALIGN_VOLTAGE;  // used by the dead-time calibration to align the rotor
  motor.voltage_limit = FOC_VOLTAGE_LIMIT;     // used for the current PI limits in init()
  motor.current_limit = CURRENT_LIMIT;
  motor.velocity_limit = VELOCITY_LIMIT;

  motor.foc_modulation = FOCModulationType::SpaceVectorPWM;
  motor.torque_controller = TorqueControlType::voltage;
  motor.controller = MotionControlType::velocity_openloop;
#ifdef MOTION_DOWNSAMPLE
  // move() (angle/velocity reading, outer loops) only every MOTION_DOWNSAMPLE+1 loopFOC
  motor.motion_downsample = MOTION_DOWNSAMPLE;
#endif
  motor.LPF_current_q.Tf = CURRENT_LPF_TF;
  motor.LPF_current_d.Tf = CURRENT_LPF_TF;

#ifndef BENCH_NO_DEADTIME
  motor.deadtime_compensation = DEADTIME_COMP;
  motor.deadtime_comp_threshold = DEADTIME_THRESHOLD;
  motor.deadtime_comp_use_measured = 0;
#endif

  dbg.magic = DBG_MAGIC;
#ifdef BENCH_PROFILE
  prof_init();
#endif
  // observer in parallel (not linked): no alignment needed, angle of the flux = rotor d axis
  observer.init();
#ifdef HALL_A_PIN
  // hall sensors linked at start-up with the known alignment: initFOC does not move the motor
  hall.init();
  hall.enableInterrupts(hallA, hallB, hallC);
  motor.linkSensor(&smooth);
  motor.zero_electric_angle = SENSOR_ZERO_ANGLE;
  motor.sensor_direction = SENSOR_DIRECTION;
#endif

  motor.useMonitoring(bench_io);
  motor.monitor_variables = 0;  // nothing streamed by default, read dbg with pyOCD

  motor.init();
  motor.tuneCurrentController(CURRENT_BANDWIDTH);
  // current sense alignment skipped, sensor alignment known (hall board) or no sensor: the motor does not move here
  motor.initFOC();
#ifdef HALL_A_PIN
  sensor_aligned = motor.motor_status == FOCMotorStatus::motor_ready ? 1 : 0;
#endif

  motor.target = 0;
  updateVoltageLimit();
  // start disabled: in open loop the voltage/current is applied even at target 0 (heating)
  motor.disable();
  was_enabled = 0;

  command.add('M', doMotor, "motor");
  command.add('D', doDeadtime, "deadtime comp");
  command.add('X', doSafety, "safety");
  command.add('R', doRecord, "record");
  command.add('P', doPing, "ping");
#ifdef HALL_A_PIN
  command.add('S', doSensor, "sensor");
  command.add('V', doRamp, "velocity by position ramp");
#endif
  command.verbose = VerboseMode::user_friendly;
  last_rx_ms = millis();
}

// the profiled loop is bench_loop, bench_step alone (pio tests) is not profiled
static void bench_step_impl(bool profiled);
void bench_step() { bench_step_impl(false); }

static void bench_step_impl(bool profiled) {
#ifdef BENCH_PROFILE
  uint32_t _pt = prof_cycles();
#define LAP(part) do { if (profiled) { uint32_t _n = prof_cycles(); prof_add(part, _n - _pt); _pt = _n; } } while (0)
#else
#define LAP(part)
#endif
  updateVoltageLimit();
  // observer before loopFOC: it integrates the voltage applied during the last period (motor.Ualpha/Ubeta)
  if (observer_on) observer.update();
  LAP(P_OBS);
#ifdef HALL_A_PIN
  if (!sensor_aligned) hall.update();  // not linked to the motor yet: update it here for the S command and dbg
#endif
#ifdef TORQUE_TARGET_MAX
  if (motor.controller == MotionControlType::torque) motor.target = _constrain(motor.target, -TORQUE_TARGET_MAX, TORQUE_TARGET_MAX);
#endif
  // friction feed-forward in velocity mode: friction_ff_a with the sign of the velocity target (linear below
  // 0.5 rad/s so that it goes smoothly to 0 at standstill); the velocity PI only handles what is left
  if (ramp_on) {
    uint32_t now = micros();
    if (motor.enabled) {
      motor.target += ramp_v * (now - ramp_us) * 1e-6f;
      motor.feed_forward_velocity = ramp_v;
    } else {
      motor.target = motor.shaft_angle;  // keep in step while disabled: no jump when enabled again
    }
    ramp_us = now;
  }
  motor.feed_forward_current.q = (motor.controller == MotionControlType::velocity)
      ? friction_ff_a * _constrain(motor.target / 0.5f, -1.0f, 1.0f) : 0.0f;
  if (ff_mode && motor.torque_controller == TorqueControlType::foc_current && !isOpenLoop()) {
    float w = motor.shaft_velocity;
    float uq = motor.estimateBEMF(w);
    float ud = 0.0f;
    if (ff_mode >= 2) uq += motor.phase_resistance * motor.current_sp;
    if (ff_mode >= 3) {
      float we_l = w * motor.pole_pairs * PHASE_INDUCTANCE;
      ud = -we_l * motor.current.q;
      uq += we_l * motor.current.d;
    }
    motor.feed_forward_voltage.q = uq;
    motor.feed_forward_voltage.d = ud;
  }
#ifdef HALL_A_PIN
  if (glitch_pending) { hall_glitch_set(hall, glitch_state); glitch_left = glitch_loops; glitch_pending = 0; }
  else if (glitch_left && --glitch_left == 0) hall_glitch_restore(hall);
#endif
  motor.loopFOC();
  LAP(P_FOC);
  motor.move();
  LAP(P_MOVE);
  updateDebug();
  LAP(P_DBG);
  safety();
  LAP(P_SAFETY);
  rec_update();
  LAP(P_REC);
#undef LAP
}

#ifdef BENCH_VBUS_RUNTIME_PIN
// Run time bus voltage through the regular conversion on the current sense ADC (STM32: SimpleFOC dev;
// GD32F1x0: candas-agent feat/gd32-regular-adc)
float _readRegularADCVoltage(const int pin);
static void readVbusRuntime() {
#ifdef BENCH_VBUS_RUNTIME_WHEN_ENABLED
  // first read only once the motor runs (test of a first ADC read after the current sense init)
  if (!motor.enabled) return;
#endif
  if (millis() - vbus_rt_last_ms < BENCH_VBUS_RUNTIME_MS) return;
  vbus_rt_last_ms = millis();
#ifdef BENCH_VBUS_RUNTIME_ANALOGREAD
  // as GenericVoltageSense: Arduino analogRead (10 bit by default) at run time
  float v = analogRead(BENCH_VBUS_RUNTIME_PIN) / 1023.0f * 3.3f;
#else
  float v = _readRegularADCVoltage(BENCH_VBUS_RUNTIME_PIN);
#endif
  vbus_rt_reads++;
  if (v < 0) vbus_rt_errors++;
  else vbus_rt = v * BENCH_VBUS_RUNTIME_GAIN;
#ifdef BENCH_VBUS_RUNTIME_ALT_PIN
  if (v >= 0 && fabsf(v * BENCH_VBUS_RUNTIME_GAIN - dbg.vbus) > 0.1f * dbg.vbus) vbus_alt_bad++;
#if defined(BENCH_VBUS_RUNTIME_ANALOGREAD) || defined(BENCH_VBUS_RUNTIME_ALT_ANALOGREAD)
  vbus_alt = analogRead(BENCH_VBUS_RUNTIME_ALT_PIN) / 1023.0f * 3.3f;
#else
  vbus_alt = _readRegularADCVoltage(BENCH_VBUS_RUNTIME_ALT_PIN);
#endif
  if (vbus_alt < 0) vbus_rt_errors++;
  // the other pin read with the Vbus value (stale result) is counted too
  else if (fabsf(vbus_alt * BENCH_VBUS_RUNTIME_GAIN - dbg.vbus) < 0.1f * dbg.vbus) vbus_alt_bad++;
#endif
}
#endif

void bench_loop() {
  PROF_T0();
  bench_step_impl(true);
#ifdef BENCH_VBUS_RUNTIME_PIN
  readVbusRuntime();
#endif
#ifdef BENCH_PROFILE
  _pt = prof_cycles();
#endif
  if (bench_io.available()) last_rx_ms = millis();
  command.run();
  PROF_LAP(P_CMD);
  PROF_TOTAL();
}
