#include "hall_table.h"

HallTableSensor::HallTableSensor(HallSensorTs& hall, const FOCMotor& motor, int8_t hall_dir)
    : _hall(hall), _motor(motor), _hall_dir(hall_dir) {}

void HallTableSensor::setTable(const float lower_edge_deg[8], const float width_deg[8]) {
  for (int s = 0; s < 8; s++) {
    _lower[s] = lower_edge_deg[s] * _PI / 180.0f;
    _width[s] = width_deg[s] * _PI / 180.0f;
  }
  _last_state = -1;
  _prev_edge_us = 0;
  _ring_n = 0;
  _v1 = _v6 = _vh_edge = 0;
}

void HallTableSensor::update() {
  _hall.update();
  int8_t s = _hall.hall_state;
  if (s < 1 || s > 6) return;  // invalid state: keep the last angle
  if (s != _last_state) {
    if (_last_state < 0) {
      _lower_cont = _lower[s];
    } else {
      // follow continuously: shortest step between the two lower edges
      float step = _normalizeAngle(_lower[s] - _lower[_last_state] + _PI) - _PI;
      _lower_cont += step;
      long now_edge = _hall.lastTransitionUs();
      long dt_us = now_edge - _prev_edge_us;
      if (_prev_edge_us != 0 && dt_us > 0) {
        float w = (step > 0 ? 1.0f : -1.0f) * _width[_last_state], dt = dt_us * 1e-6f;
        _v1 = w / dt;                                  // V2: last sector, measured width / time spent in it
        _ring_w[_ring_i] = w; _ring_t[_ring_i] = dt;
        _ring_i = (_ring_i + 1) % 6;
        if (_ring_n < 6) _ring_n++;
        float sw = 0, stt = 0;
        for (int k = 0; k < _ring_n; k++) { sw += _ring_w[k]; stt += _ring_t[k]; }
        _v6 = (stt > 0) ? sw / stt : _v1;              // V3: last 6 sectors (one electrical turn)
      }
      _prev_edge_us = now_edge;
      _vh_edge = _hall_dir * _hall.getVelocity() * _motor.pole_pairs;  // V1: HallSensor value at the edge
    }
    _last_state = s;
  }
  float w = _width[s];
  float vel_el = _motor.shaft_velocity * _motor.pole_pairs;  // rotor frame (sensor_direction CW)
  float el = _lower_cont + 0.5f * w;                          // standstill: middle of the sector
  if (vel_el > 0.05f || vel_el < -0.05f) {
    float dt = (_micros() - _hall.lastTransitionUs()) * 1e-6f;  // time since the last transition
    float adv = _constrain(vel_el * dt, -w, w);
    el = (vel_el > 0) ? _lower_cont + adv : _lower_cont + w + adv;  // entry edge + advance
  }
  last_el = el;
  float mech = el / _motor.pole_pairs;
  full_rotations = (int32_t)floorf(mech / _2PI);
  angle_prev = mech - full_rotations * _2PI;
  angle_prev_ts = _micros();
}

// between edges the rotor cannot be faster than the current sector width / time since the last edge
float HallTableSensor::decay(float v_el) {
  float t = (_micros() - _prev_edge_us) * 1e-6f;
  if (_prev_edge_us == 0 || t <= 0) return v_el;
  float bound = _width[_last_state] / t;
  return _constrain(v_el, -bound, bound);
}

float HallTableSensor::getVelocity() {
  float vh = _hall_dir * _hall.getVelocity();               // HallSensor velocity, mechanical, rotor frame
  if (vel_mode == 0 || _last_state < 1) return vh;          // V0: HallSensor as is
  float pp = _motor.pole_pairs;
  switch (vel_mode) {
    case 1: return decay(_vh_edge) / pp;                    // V1: HallSensor value at the edge + decay
    case 2: return decay(_v1) / pp;                         // V2: last sector, measured width + decay
    case 3: return decay(_v6) / pp;                         // V3: 6 sector average + decay
    default: {                                              // V4: V3 below 2.5 rad/s, HallSensor above 4
      float v = decay(_v6) / pp;
      float a = _constrain((fabsf(v) - 2.5f) / 1.5f, 0.0f, 1.0f);
      return (1.0f - a) * v + a * vh;
    }
  }
}
