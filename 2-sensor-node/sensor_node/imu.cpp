#include "imu.h"
#include <Arduino.h>
#include <Wire.h>

namespace {

constexpr uint8_t MPU_ADDR         = 0x68;  // AD0 ต่อ GND
constexpr uint8_t REG_SMPLRT_DIV   = 0x19;
constexpr uint8_t REG_CONFIG       = 0x1A;
constexpr uint8_t REG_GYRO_CONFIG  = 0x1B;
constexpr uint8_t REG_ACCEL_CONFIG = 0x1C;
constexpr uint8_t REG_ACCEL_XOUT_H = 0x3B;
constexpr uint8_t REG_PWR_MGMT_1   = 0x6B;
constexpr uint8_t REG_WHO_AM_I     = 0x75;

constexpr float ACC_LSB_PER_G   = 4096.0f;  // ±8 g
constexpr float GYR_LSB_PER_DPS = 16.4f;    // ±2000 dps
constexpr uint32_t PERIOD_US    = 1000000UL / IMU_SAMPLE_HZ;

float    ring[IMU_WINDOW_SAMPLES][IMU_AXES];
uint16_t idx     = 0;  // ช่องที่จะเขียนถัดไป (เมื่อเต็มแล้ว = ช่องที่เก่าที่สุด)
uint32_t total   = 0;
uint32_t missed  = 0;
uint32_t next_us = 0;

bool write_reg(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

bool read_regs(uint8_t reg, uint8_t *buf, uint8_t n) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(MPU_ADDR, n) != n) return false;
  for (uint8_t i = 0; i < n; i++) buf[i] = Wire.read();
  return true;
}

int16_t be16(const uint8_t *p) {
  return (int16_t)((p[0] << 8) | p[1]);
}

}  // namespace

bool imu_begin() {
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, I2C_FREQ_HZ);

  uint8_t who = 0;
  if (!read_regs(REG_WHO_AM_I, &who, 1)) return false;  // ไม่มี ACK = สายหลวม/ไม่มีไฟ/address ผิด
  // โมดูล clone บางตัวตอบค่าอื่นที่ไม่ใช่ 0x68 แต่ใช้ register ชุดเดียวกัน จึงไม่ถือเป็น error

  if (!write_reg(REG_PWR_MGMT_1, 0x80)) return false;   // reset
  delay(100);

  bool ok = write_reg(REG_PWR_MGMT_1, 0x01)     // ปลุก + ใช้ clock จาก gyro X (นิ่งกว่า clock ภายใน)
         && write_reg(REG_CONFIG, 0x04)         // DLPF ~21 Hz กัน aliasing ตอนเก็บที่ 50 Hz
         && write_reg(REG_SMPLRT_DIV, 19)       // 1 kHz / (1 + 19) = 50 Hz
         && write_reg(REG_GYRO_CONFIG, 0x18)    // ±2000 dps
         && write_reg(REG_ACCEL_CONFIG, 0x10);  // ±8 g
  delay(50);

  idx = 0;
  total = 0;
  missed = 0;
  next_us = micros();
  return ok;
}

void imu_update() {
  const uint32_t now = micros();
  if ((int32_t)(now - next_us) < 0) return;  // ยังไม่ถึงรอบ

  // loop ช้าจนเลยไปหลายรอบ: นับเป็น missed แล้วตั้งเวลาใหม่ (ไม่อ่านย้อนหลัง)
  const uint32_t late = (now - next_us) / PERIOD_US;
  if (late > 0) {
    missed += late;
    next_us += late * PERIOD_US;
  }
  next_us += PERIOD_US;

  uint8_t b[14];  // ax ay az temp gx gy gz (big-endian 16 bit)
  if (!read_regs(REG_ACCEL_XOUT_H, b, sizeof(b))) {
    missed++;
    return;
  }

  float *s = ring[idx];
  s[0] = be16(b + 0)  / ACC_LSB_PER_G;
  s[1] = be16(b + 2)  / ACC_LSB_PER_G;
  s[2] = be16(b + 4)  / ACC_LSB_PER_G;
  s[3] = be16(b + 8)  / GYR_LSB_PER_DPS;
  s[4] = be16(b + 10) / GYR_LSB_PER_DPS;
  s[5] = be16(b + 12) / GYR_LSB_PER_DPS;

  idx = (idx + 1) % IMU_WINDOW_SAMPLES;
  total++;
}

bool imu_window_ready() {
  return total >= IMU_WINDOW_SAMPLES;
}

void imu_copy_window(float *out) {
  // ข้อมูลในแรมไม่ได้เรียงเก่า->ใหม่เสมอ: เริ่มอ่านที่ idx (ค่าเก่าสุด) แล้ววนจนครบ 100 ช่อง
  // imu_update() กับฟังก์ชันนี้ถูกเรียกจาก loop() เดียวกัน จึงไม่มีการเขียนทับระหว่าง copy
  // ถ้าย้ายการอ่าน IMU ไปไว้ใน FreeRTOS task หรือ timer อื่น ต้องใส่ mutex ครอบตรงนี้
  for (uint16_t i = 0; i < IMU_WINDOW_SAMPLES; i++) {
    const float *s = ring[(idx + i) % IMU_WINDOW_SAMPLES];
    for (uint8_t a = 0; a < IMU_AXES; a++) *out++ = s[a];
  }
}

void imu_sleep() {
  write_reg(REG_PWR_MGMT_1, 0x40);  // SLEEP bit
}

uint32_t imu_sample_count()   { return total; }
uint32_t imu_missed_samples() { return missed; }
