#pragma once
/*
 * Test of a calibrated hall table (bench only, not a library change).
 *
 * Wraps a HallSensor and gives the rotor angle from the measured edges of each hall state (tools/hall_calib.py):
 *  - standstill: middle of the sector
 *  - moving: the transition happened on the entry edge (lower edge forward, upper edge backward), then advance with
 *    velocity * time since the transition, limited to the measured sector width.
 * The table is in the ROTOR frame (SimpleFOC convention), so use motor.zero_electric_angle = 0 and
 * motor.sensor_direction = Direction::CW with this sensor. hall_dir = the direction found by the alignment of the
 * plain hall sensor (its sector count runs this way relative to the rotor).
 */
#include <SimpleFOC.h>

// HallSensor keeps the transition timestamp protected (only SmoothingSensor is a friend): expose it for the test
class HallSensorTs : public HallSensor {
  public:
    using HallSensor::HallSensor;
    long lastTransitionUs() { return angle_prev_ts; }  // set to the last transition time by HallSensor::update()
};

class HallTableSensor : public Sensor {
  public:
    // lower_edge / width: rotor electrical angle [deg] of the lower edge and width of each hall state, index 1..6
    HallTableSensor(HallSensorTs& hall, const FOCMotor& motor, int8_t hall_dir);
    void setTable(const float lower_edge_deg[8], const float width_deg[8]);
    void update() override;
    float getVelocity() override;
    int needsSearch() override { return 0; }
    void init() override {}             // the hall sensor is initialised by the bench
    float last_el = 0;                  // last electrical angle given [rad] (debug)
    // velocity estimate: 0 HallSensor, 1 HallSensor at the edge + decay, 2 last sector (measured width) + decay,
    // 3 last 6 sectors + decay, 4 = 3 below 2.5 rad/s blended to 0 above 4 rad/s. Decay: never above the current
    // sector width / time since the last edge (instead of holding the value then dropping to 0)
    uint8_t vel_mode = 0;
  protected:
    float getSensorAngle() override { return angle_prev; }
    HallSensorTs& _hall;
    const FOCMotor& _motor;
    int8_t _hall_dir;
    float _lower[8] = {0}, _width[8] = {0};  // [rad]
    int8_t _last_state = -1;
    float _lower_cont = 0;             // continuous electrical angle of the lower edge of the current state
    long _prev_edge_us = 0;            // time of the previous transition
    float _v1 = 0, _v6 = 0, _vh_edge = 0;  // electrical rad/s, rotor frame
    float decay(float v_el);
    float _ring_w[6] = {0};            // signed widths [rad] and durations [s] of the last 6 sectors left
    float _ring_t[6] = {0};
    uint8_t _ring_i = 0, _ring_n = 0;
};
