// GD32F130 split board: PhaseState test UNDER POWER (26 V), motor not driven: the wheel is turned by hand and
// brakes (phases shorted) or turns freely depending on the mode. tools/phase_brake.py writes req_mode over SWD.
// SAFETY: only fixed modes where the three phases are in the SAME state with the SAME duty cycle: the motor
// phases are never connected to different potentials, so no current flows from the supply. Unknown mode = all OFF.
// Starts in mode 0 (all OFF, free wheel). Duty cycle at most 0.95 (bootstrap capacitors of the high side drivers).
#include <Arduino.h>
#include <SimpleFOC.h>

#define SELF_HOLD_PIN PB2
#define VBUS_PIN PA4
#define VBUS_GAIN 31.0f
#define VBUS_MAX 30.0f

BLDCDriver6PWM driver = BLDCDriver6PWM(PA10, PB15, PA9, PB14, PA8, PB13);

struct Mode { PhaseState state; float duty; };
static const Mode modes[] = {
  {PhaseState::PHASE_OFF, 0.0f},   // 0: all off: free wheel
  {PhaseState::PHASE_ON, 0.0f},    // 1: ON at 0 %: low sides always on: brake (reference)
  {PhaseState::PHASE_LO, 0.0f},    // 2: LO at 0 %
  {PhaseState::PHASE_LO, 0.95f},   // 3: LO at 95 %
  {PhaseState::PHASE_HI, 0.0f},    // 4: HI at 0 %
};
static const uint32_t mode_count = sizeof(modes) / sizeof(modes[0]);

// written by the tool
volatile uint32_t req_mode = 0;
// read by the tool: 1 running, 2 refused (bus voltage too high), 3 driver init failed; mode applied, loop counter
volatile uint32_t status = 0;
volatile float vbus_measured = 0;
volatile uint32_t mode_applied = 0;
volatile uint32_t applied = 0;

void setup() {
  pinMode(SELF_HOLD_PIN, OUTPUT);
  digitalWrite(SELF_HOLD_PIN, HIGH);
  vbus_measured = analogRead(VBUS_PIN) * 3.3f / 1023.0f * VBUS_GAIN;
  if (vbus_measured > VBUS_MAX) {
    status = 2;
    return;
  }
  driver.voltage_power_supply = 1.0f;  // duty cycle = voltage
  driver.pwm_frequency = 16000;
  driver.dead_zone = 0.01335f;
  if (!driver.init()) {
    status = 3;
    return;
  }
  driver.enable();
  status = 1;
}

void loop() {
  if (status != 1) return;
  uint32_t m = req_mode < mode_count ? req_mode : 0;
  driver.setPhaseState(modes[m].state, modes[m].state, modes[m].state);
  driver.setPwm(modes[m].duty, modes[m].duty, modes[m].duty);
  mode_applied = m;
  applied++;
  delay(1);
}
