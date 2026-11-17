#include "bench.h"

BLDCMotor motor = BLDCMotor(POLE_PAIRS, PHASE_RESISTANCE, KV_RATING, PHASE_INDUCTANCE);
BLDCDriver6PWM driver = BLDCDriver6PWM(A_PHASE_UH, A_PHASE_UL, A_PHASE_VH, A_PHASE_VL, A_PHASE_WH, A_PHASE_WL);
// gain calculation: https://community.simplefoc.com/t/b-g431b-esc1-current-control/521/21
LowsideCurrentSense currentSense = LowsideCurrentSense(0.003f, -64.0f / 7.0f, A_OP1_OUT, A_OP2_OUT, A_OP3_OUT);
Commander command = Commander(Serial);

volatile DebugVars dbg;
float openloop_voltage = OPENLOOP_VOLTAGE;
int equal_amplitude = 0;

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
void computeCompensation(float& ca, float& cb, float& cc) {
  ca = cb = cc = 0;
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
}

static bool isOpenLoop() {
  return motor.controller == MotionControlType::velocity_openloop || motor.controller == MotionControlType::angle_openloop;
}

// In open loop (voltage mode) the voltage amplitude is the voltage limit: it is set from openloop_voltage,
// optionally lowered to keep the same fundamental amplitude with and without compensation.
// In the other modes the voltage limit is FOC_VOLTAGE_LIMIT.
static void updateVoltageLimit() {
  if (isOpenLoop() && motor.torque_controller == TorqueControlType::voltage) {
    float u = openloop_voltage;
    if (equal_amplitude) u -= (4.0f / _PI) * motor.deadtime_compensation;
    motor.voltage_limit = u > 0 ? u : 0;
  } else {
    motor.voltage_limit = FOC_VOLTAGE_LIMIT;
  }
}

float bench_calibrate(float max_current) {
  if (max_current <= 0) max_current = CALIB_MAX_CURRENT;
  if (max_current > CALIB_MAX_CURRENT_LIMIT) max_current = CALIB_MAX_CURRENT_LIMIT;
  Serial.print(F("Dead-time calibration, max current [A]: "));
  Serial.println(max_current, 3);
  bool en = motor.enabled;
  if (!en) motor.enable();
  motor.voltage_limit = FOC_VOLTAGE_LIMIT;
  float udt = motor.calibrateDeadtimeCompensation(max_current);
  updateVoltageLimit();
  if (!en) motor.disable();
  if (_isset(udt)) {
    Serial.print(F("Measured compensation [V]: "));
    Serial.println(udt, 4);
    Serial.print(F("Ratio to Vbus*dead_zone: "));
    Serial.println(udt / (SUPPLY_VOLTAGE * DEAD_ZONE), 3);
  } else {
    Serial.println(F("Calibration failed"));
  }
  return udt;
}

// ---------------------------------------------------------------------------
// Recorder
// ---------------------------------------------------------------------------
static Sample rec_buf[RECORD_SAMPLES];
static int rec_n = 0;
static bool rec_running = false;
static bool rec_auto_dump = false;
static uint32_t rec_period_us = 0, rec_t0 = 0, rec_last = 0;

void rec_start(uint32_t period_us, bool auto_dump) {
  rec_period_us = period_us;
  rec_auto_dump = auto_dump;
  rec_n = 0;
  rec_t0 = rec_last = _micros();
  rec_running = true;
}
bool rec_done() { return !rec_running && rec_n > 0; }
int rec_count() { return rec_n; }
const Sample* rec_data() { return rec_buf; }

void rec_dump(Print& out) {
  out.println(F("REC_BEGIN"));
  out.println(F("t_us,ia,comp_a"));
  for (int k = 0; k < rec_n; k++) {
    out.print(rec_buf[k].t_us);
    out.print(',');
    out.print(rec_buf[k].ia, 4);
    out.print(',');
    out.println(rec_buf[k].comp_a, 4);
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
  rec_buf[rec_n].comp_a = dbg.comp_a;
  if (++rec_n >= RECORD_SAMPLES) {
    rec_running = false;
    if (rec_auto_dump) {
      // dumping blocks the loop: stop the motor first (open loop would stall)
      bench_disable(F("recording done"));
      rec_dump(Serial);
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

// ---------------------------------------------------------------------------
// Safety and debug
// ---------------------------------------------------------------------------
void bench_disable(const __FlashStringHelper* reason) {
  motor.disable();
  was_enabled = 0;
  if (reason) {
    Serial.print(F("MOTOR DISABLED: "));
    Serial.println(reason);
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
}

// ---------------------------------------------------------------------------
// Serial commands
// ---------------------------------------------------------------------------
static bool hasArg(const char* p) { return *p != '\0' && *p != '\n' && *p != '\r'; }

static void printDeadtime() {
  Serial.print(F("comp [V]: "));       Serial.println(motor.deadtime_compensation, 4);
  Serial.print(F("threshold: "));      Serial.println(motor.deadtime_comp_threshold, 4);
  Serial.print(F("use measured: "));   Serial.println(motor.deadtime_comp_use_measured);
  Serial.print(F("equal ampl.: "));    Serial.println(equal_amplitude);
  Serial.print(F("openloop U [V]: ")); Serial.println(openloop_voltage, 4);
  Serial.print(F("expected Vbus*dead_zone [V]: ")); Serial.println(SUPPLY_VOLTAGE * DEAD_ZONE, 4);
}

static void doDeadtime(char* cmd) {
  switch (cmd[0]) {
    case '\0': case '\n': case '\r':
      printDeadtime();
      break;
    case 'T':  // threshold
      command.scalar(&motor.deadtime_comp_threshold, cmd + 1);
      break;
    case 'M':  // use measured currents (foc_current)
      if (hasArg(cmd + 1)) motor.deadtime_comp_use_measured = atoi(cmd + 1) ? 1 : 0;
      Serial.println(motor.deadtime_comp_use_measured);
      break;
    case 'E':  // equal amplitude in open loop
      if (hasArg(cmd + 1)) equal_amplitude = atoi(cmd + 1) ? 1 : 0;
      Serial.println(equal_amplitude);
      break;
    case 'B':  // open loop voltage amplitude
      command.scalar(&openloop_voltage, cmd + 1);
      break;
    case 'C':  // calibration
      bench_calibrate((float)atof(cmd + 1));
      break;
    default:   // compensation value
      command.scalar(&motor.deadtime_compensation, cmd);
      break;
  }
}

static void doMotor(char* cmd) {
  command.motor(&motor, cmd);
  // no position sensor: only the open loop motion controls are possible
  if (!isOpenLoop()) {
    motor.controller = MotionControlType::velocity_openloop;
    Serial.println(F("No sensor: only open loop allowed (MC3 velocity, MC4 angle), back to velocity open loop"));
  }
}

static void printSafety() {
  Serial.print(F("timeout [s]: "));     Serial.println(run_timeout_s);
  Serial.print(F("overcurrent [A]: ")); Serial.println(overcurrent_a, 2);
  Serial.print(F("heartbeat: "));       Serial.println(heartbeat_on);
  Serial.print(F("last fault: "));      Serial.println(fault);
}

static void doSafety(char* cmd) {
  switch (cmd[0]) {
    case 'T': if (hasArg(cmd + 1)) run_timeout_s = (uint32_t)atol(cmd + 1); break;
    case 'I': if (hasArg(cmd + 1)) overcurrent_a = (float)atof(cmd + 1); break;
    case 'H': if (hasArg(cmd + 1)) heartbeat_on = atoi(cmd + 1) ? 1 : 0; break;
    case 'C': fault = 0; break;
    default: break;
  }
  printSafety();
}

static void doPing(char*) {}  // heartbeat, silent

static void doRecord(char* cmd) {
  if (cmd[0] == 'D') { rec_dump(Serial); return; }  // RD: dump the last recording
  uint32_t period = hasArg(cmd) ? (uint32_t)atol(cmd) : 1000;
  Serial.print(F("Recording, period [us]: "));
  Serial.println(period);
  rec_start(period, true);
}

// ---------------------------------------------------------------------------
void bench_setup() {
  Serial.begin(115200);
  SimpleFOCDebug::enable(&Serial);
  _delay(1000);

  // no position sensor linked: open loop only

  driver.voltage_power_supply = SUPPLY_VOLTAGE;
  driver.pwm_frequency = PWM_FREQUENCY;
  driver.dead_zone = DEAD_ZONE;
  driver.init();
  motor.linkDriver(&driver);

  // current sense (B-G431B-ESC1: phases are known, no alignment needed)
  currentSense.linkDriver(&driver);
  currentSense.init();
  currentSense.skip_align = true;
  motor.linkCurrentSense(&currentSense);

  motor.voltage_sensor_align = ALIGN_VOLTAGE;  // used by the dead-time calibration to align the rotor
  motor.voltage_limit = FOC_VOLTAGE_LIMIT;     // used for the current PI limits in init()
  motor.current_limit = CURRENT_LIMIT;
  motor.velocity_limit = VELOCITY_LIMIT;

  motor.foc_modulation = FOCModulationType::SpaceVectorPWM;
  motor.torque_controller = TorqueControlType::voltage;
  motor.controller = MotionControlType::velocity_openloop;
  motor.LPF_current_q.Tf = CURRENT_LPF_TF;
  motor.LPF_current_d.Tf = CURRENT_LPF_TF;

  motor.deadtime_compensation = DEADTIME_COMP;
  motor.deadtime_comp_threshold = DEADTIME_THRESHOLD;
  motor.deadtime_comp_use_measured = 0;

  motor.useMonitoring(Serial);
  motor.monitor_variables = 0;  // nothing streamed by default, use MCUViewer

  motor.init();
  motor.tuneCurrentController(CURRENT_BANDWIDTH);
  // no sensor and current sense alignment skipped: the motor does not move here
  motor.initFOC();

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
  command.verbose = VerboseMode::user_friendly;
  last_rx_ms = millis();
}

void bench_step() {
  updateVoltageLimit();
  motor.loopFOC();
  motor.move();
  updateDebug();
  safety();
  rec_update();
}

void bench_loop() {
  bench_step();
  if (Serial.available()) last_rx_ms = millis();
  command.run();
}
