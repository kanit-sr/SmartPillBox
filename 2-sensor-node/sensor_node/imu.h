// imu.h — MPU6050 50 Hz + ring buffer 100 ช่อง (2 วิ)
#pragma once
#include <stdint.h>
#include "config.h"

// ตั้ง MPU6050: accel ±8 g, gyro ±2000 dps, DLPF ~21 Hz (ให้ตรงกับ dataset ที่ใช้เทรน)
// คืน false ถ้าไม่เจอเซนเซอร์บนบัส I2C
bool imu_begin();

// เรียกใน loop() ให้บ่อยที่สุด — อ่าน 1 sample ทุก 20 ms แล้วเขียนลง ring buffer
void imu_update();

// มีข้อมูลครบ 100 sample (2 วิ) แล้วหรือยัง
bool imu_window_ready();

// copy ring buffer ออกมาเรียงจากเก่าไปใหม่
// out ต้องมีขนาด IMU_WINDOW_SAMPLES * IMU_AXES (600 float)
// รูปแบบ: [accX, accY, accZ, gyrX, gyrY, gyrZ] x 100 sample
void imu_copy_window(float *out);

// สั่ง MPU6050 เข้า sleep (~5 µA) ก่อนกำไล deep sleep — ตื่นใหม่ด้วย imu_begin()
void imu_sleep();

uint32_t imu_sample_count();    // จำนวน sample ที่อ่านได้ทั้งหมด
uint32_t imu_missed_samples();  // จำนวนรอบที่อ่านไม่ทัน/อ่านไม่สำเร็จ (ถ้าเพิ่มเรื่อยๆ แปลว่า loop ช้าเกิน 20 ms)
