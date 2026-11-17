/**
 * FOC test bench: PWM dead-time compensation on a B-G431B-ESC1 with a hoverboard motor (hall sensors)
 *
 * - starts in velocity open loop (voltage mode), target 0, compensation disabled
 * - everything can be changed at runtime through the serial Commander (see README)
 * - the variables to watch with MCUViewer are grouped in the global `dbg` structure
 */
#include <Arduino.h>
#include <SimpleFOC.h>
#ifdef USE_SMOOTHING_SENSOR
#include "encoders/smoothing/SmoothingSensor.h"
#endif
#include "config.h"

BLDCMotor motor = BLDCMotor(POLE_PAIRS, PHASE_RESISTANCE, KV_RATING, PHASE_INDUCTANCE);
BLDCDriver6PWM driver = BLDCDriver6PWM(A_PHASE_UH, A_PHASE_UL, A_PHASE_VH, A_PHASE_VL, A_PHASE_WH, A_PHASE_WL);
// gain calculation: https://community.simplefoc.com/t/b-g431b-esc1-current-control/521/21
LowsideCurrentSense currentSense = LowsideCurrentSense(0.003f, -64.0f / 7.0f, A_OP1_OUT, A_OP2_OUT, A_OP3_OUT);
HallSensor hall = HallSensor(A_HALL1, A_HALL2, A_HALL3, POLE_PAIRS);
#ifdef USE_SMOOTHING_SENSOR
SmoothingSensor smooth = SmoothingSensor(hall, motor);
#endif

void doA() { hall.handleA(); }
void doB() { hall.handleB(); }
void doC() { hall.handleC(); }

Commander command = Commander(Serial);

// ---------------------------------------------------------------------------
// Variables for MCUViewer (global, volatile so they are never optimised away)
// ---------------------------------------------------------------------------
struct DebugVars {
  float ia, ib, ic;              // measured phase currents [A]
  float comp_a, comp_b, comp_c;  // dead-time compensation added on each phase [V]
  float iq, id;                  // filtered dq currents [A] (foc_current mode)
  float uq, ud;                  // dq voltages [V]
  float electrical_angle;        // [rad]
  float velocity;                // [rad/s]
  float loop_us;                 // loopFOC period [us]
};
volatile DebugVars dbg;

// test bench state
float openloop_voltage = OPENLOOP_VOLTAGE;  // open loop voltage amplitude [V]
int equal_amplitude = 0;  // 1: lower the open loop voltage by 4/pi*compensation to keep the same fundamental amplitude

// ---------------------------------------------------------------------------
// Dead-time compensation actually applied, same formula as BLDCMotor::setPhaseVoltage
// (keep in sync with the library, only used for display)
// ---------------------------------------------------------------------------
static void computeCompensation(float& ca, float& cb, float& cc) {
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

static bool isOpenLoop() {
  return motor.controller == MotionControlType::velocity_openloop || motor.controller == MotionControlType::angle_openloop;
}

// In open loop (voltage mode) the voltage amplitude is the voltage limit: it is set from openloop_voltage,
// optionally lowered to keep the same fundamental amplitude with and without compensation.
// In the other modes the voltage limit is restored to FOC_VOLTAGE_LIMIT.
static void updateVoltageLimit() {
  if (isOpenLoop() && motor.torque_controller == TorqueControlType::voltage) {
    float u = openloop_voltage;
    if (equal_amplitude) u -= (4.0f / _PI) * motor.deadtime_compensation;
    motor.voltage_limit = u > 0 ? u : 0;
  } else {
    motor.voltage_limit = FOC_VOLTAGE_LIMIT;
  }
}

// ---------------------------------------------------------------------------
// 'D' command: dead-time compensation
// ---------------------------------------------------------------------------
static void printDeadtime() {
  Serial.print(F("comp [V]: "));       Serial.println(motor.deadtime_compensation, 4);
  Serial.print(F("threshold: "));      Serial.println(motor.deadtime_comp_threshold, 4);
  Serial.print(F("use measured: "));   Serial.println(motor.deadtime_comp_use_measured);
  Serial.print(F("equal ampl.: "));    Serial.println(equal_amplitude);
  Serial.print(F("openloop U [V]: ")); Serial.println(openloop_voltage, 4);
  Serial.print(F("expected Vbus*dead_zone [V]: ")); Serial.println(SUPPLY_VOLTAGE * DEAD_ZONE, 4);
}

static void calibrate(float max_current) {
  if (max_current <= 0) max_current = CALIB_MAX_CURRENT;
  Serial.print(F("Dead-time calibration, max current [A]: "));
  Serial.println(max_current, 3);
  // calibration needs the driver enabled and a usable voltage range
  bool was_enabled = motor.enabled;
  if (!was_enabled) motor.enable();
  motor.voltage_limit = FOC_VOLTAGE_LIMIT;
  float udt = motor.calibrateDeadtimeCompensation(max_current);
  updateVoltageLimit();
  if (!was_enabled) motor.disable();
  if (_isset(udt)) {
    Serial.print(F("Measured compensation [V]: "));
    Serial.println(udt, 4);
    Serial.print(F("Ratio to Vbus*dead_zone: "));
    Serial.println(udt / (SUPPLY_VOLTAGE * DEAD_ZONE), 3);
  } else {
    Serial.println(F("Calibration failed"));
  }
}

void doDeadtime(char* cmd) {
  char c = cmd[0];
  switch (c) {
    case '\0':
    case '\n':
    case '\r':
      printDeadtime();
      break;
    case 'T':  // threshold
      command.scalar(&motor.deadtime_comp_threshold, cmd + 1);
      break;
    case 'M':  // use measured currents (foc_current)
      if (cmd[1] != '\0' && cmd[1] != '\n') motor.deadtime_comp_use_measured = atoi(cmd + 1) ? 1 : 0;
      Serial.println(motor.deadtime_comp_use_measured);
      break;
    case 'E':  // equal amplitude in open loop
      if (cmd[1] != '\0' && cmd[1] != '\n') equal_amplitude = atoi(cmd + 1) ? 1 : 0;
      Serial.println(equal_amplitude);
      break;
    case 'B':  // open loop voltage amplitude
      command.scalar(&openloop_voltage, cmd + 1);
      break;
    case 'C':  // calibration
      calibrate((float)atof(cmd + 1));
      break;
    default:   // compensation value
      command.scalar(&motor.deadtime_compensation, cmd);
      break;
  }
}

void doMotor(char* cmd) { command.motor(&motor, cmd); }

void setup() {
  Serial.begin(115200);
  SimpleFOCDebug::enable(&Serial);
  _delay(1000);

  // hall sensors
  hall.pullup = Pullup::USE_INTERN;
  hall.init();
  hall.enableInterrupts(doA, doB, doC);
#ifdef USE_SMOOTHING_SENSOR
  motor.linkSensor(&smooth);
#else
  motor.linkSensor(&hall);
#endif

  // driver
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

  // limits
  motor.voltage_sensor_align = ALIGN_VOLTAGE;
  motor.voltage_limit = FOC_VOLTAGE_LIMIT;  // used for the current PI limits in init()
  motor.current_limit = CURRENT_LIMIT;
  motor.velocity_limit = VELOCITY_LIMIT;

  // controllers
  motor.foc_modulation = FOCModulationType::SpaceVectorPWM;
  motor.torque_controller = TorqueControlType::voltage;
  motor.controller = MotionControlType::velocity_openloop;
  motor.PID_velocity.P = VELOCITY_P;
  motor.PID_velocity.I = VELOCITY_I;
  motor.LPF_velocity.Tf = VELOCITY_LPF_TF;
  motor.LPF_current_q.Tf = CURRENT_LPF_TF;
  motor.LPF_current_d.Tf = CURRENT_LPF_TF;

  // dead-time compensation (disabled at start-up)
  motor.deadtime_compensation = DEADTIME_COMP;
  motor.deadtime_comp_threshold = DEADTIME_THRESHOLD;
  motor.deadtime_comp_use_measured = 0;

  motor.useMonitoring(Serial);
  motor.monitor_variables = 0;  // nothing streamed by default, use MCUViewer

  motor.init();
  motor.tuneCurrentController(CURRENT_BANDWIDTH);
  // aligns the hall sensors (the motor moves)
  motor.initFOC();

  motor.target = 0;
  updateVoltageLimit();

  command.add('M', doMotor, "motor");
  command.add('D', doDeadtime, "deadtime comp");
  command.verbose = VerboseMode::user_friendly;

  Serial.println(F("Ready. Open loop, target 0. 'D' prints the dead-time settings."));
}

void loop() {
  updateVoltageLimit();
  motor.loopFOC();
  motor.move();
  updateDebug();
  command.run();
}
