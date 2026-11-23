#include "motor.h"
#include "config.h"
#include <Arduino.h>

namespace {

bool     continuous = false;
uint8_t  left = 0;        // จำนวนครั้งที่ยังต้องสั่น (รวมครั้งที่กำลังสั่นอยู่)
uint16_t on_ms = 0, off_ms = 0;
bool     phase_on = false;
uint32_t phase_start = 0;

void drive(bool on) { digitalWrite(PIN_MOTOR, on ? HIGH : LOW); }

}  // namespace

void motor_begin() {
  pinMode(PIN_MOTOR, OUTPUT);
  drive(false);
}

void motor_on() {
  left = 0;
  continuous = true;
  drive(true);
}

void motor_off() {
  continuous = false;
  left = 0;
  phase_on = false;
  drive(false);
}

void motor_pulse(uint8_t count, uint16_t on, uint16_t off) {
  continuous = false;
  if (count == 0 || on == 0) { motor_off(); return; }
  left = count;
  on_ms = on;
  off_ms = off;
  phase_on = true;
  phase_start = millis();
  drive(true);
}

void motor_update() {
  if (continuous || left == 0) return;
  const uint32_t el = millis() - phase_start;
  if (phase_on) {
    if (el < on_ms) return;
    drive(false);
    phase_on = false;
    phase_start += on_ms;
    if (--left == 0) return;  // ครบแล้ว ไม่ต้องรอช่วงพักสุดท้าย
  } else if (el >= off_ms) {
    drive(true);
    phase_on = true;
    phase_start += off_ms;
  }
}

bool motor_busy() { return continuous || left > 0; }
