// GD32F130 split board: PhaseState check of the GD32 6PWM driver (PHASE_ON / OFF / HI / LO per phase).
// No motor control: the driver only. tools/phase_hilo.py writes the requested states into req_state / req_duty
// over SWD and reads back the TIMER0 output enable bits (CHCTL2) and the pin levels.
// SAFETY: only with the main supply OFF (board on the ST-Link 3.3 V, gate drivers unpowered). The driver is never
// initialised if the bus voltage is above 10 V.
#include <Arduino.h>
#include <SimpleFOC.h>

#define SELF_HOLD_PIN PB2
#define VBUS_PIN PA4
#define VBUS_GAIN 31.0f
#define VBUS_MAX 10.0f

BLDCDriver6PWM driver = BLDCDriver6PWM(PA10, PB15, PA9, PB14, PA8, PB13);

// written by the tool: PhaseState values (0 OFF, 1 ON, 2 HI, 3 LO) and duty cycle [0..1]
volatile uint8_t req_state[3] = {1, 1, 1};
volatile float req_duty = 0.3f;
// read by the tool: 1 running, 2 refused (bus voltage too high), 3 driver init failed; applied counter
volatile uint32_t status = 0;
volatile float vbus_measured = 0;
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
  driver.setPhaseState((PhaseState)req_state[0], (PhaseState)req_state[1], (PhaseState)req_state[2]);
  driver.setPwm(req_duty, req_duty, req_duty);
  applied++;
  delay(1);
}
