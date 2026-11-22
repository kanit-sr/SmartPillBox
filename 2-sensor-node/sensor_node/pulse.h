// pulse.h — MAX30102 ชีพจร (bpm) + SpO2 (%) แบบ non-blocking
// ใช้ register ตรงๆ (ไม่ต้องลง library เพิ่ม) อยู่บนบัส I2C เดียวกับ MPU6050 — เรียก imu_begin() ก่อน (เป็นคน Wire.begin)
//
// วิธีใช้: pulse_start() → เรียก pulse_update() ทุกรอบ loop → คืน true ครั้งเดียวตอนวัดเสร็จ (~7 วิ)
//          แล้วอ่านผลจาก pulse_last()
// ค่าที่ได้เป็นการประมาณจาก PPG ที่ข้อมือ ไม่ได้ calibrate ทางการแพทย์
#pragma once
#include <stdint.h>

struct PulseResult {
  uint8_t hr;       // bpm, 0 = อ่านไม่ได้ (ไม่แนบผิว / สัญญาณไม่ดี / เซนเซอร์เสีย)
  uint8_t spo2;     // %,  0 = อ่านไม่ได้
  bool    contact;  // ค่า IR สูงพอว่าแนบผิวอยู่
};

bool pulse_begin();        // หาเซนเซอร์ + ตั้งค่า แล้ว shutdown ไว้ — คืน false ถ้าไม่เจอ
bool pulse_ok();           // เซนเซอร์ตอบอยู่ (ใช้ทำ FLAG_MAX30102_OK)

void pulse_start();        // เริ่มวัด 1 รอบ (ถ้ากำลังวัดอยู่จะไม่เริ่มใหม่)
bool pulse_update();       // เรียกใน loop() — true = เพิ่งวัดเสร็จ (หรือล้มเหลว) รอบนี้
bool pulse_busy();         // กำลังวัดอยู่
PulseResult pulse_last();  // ผลรอบล่าสุด

void pulse_sleep();        // shutdown (ก่อน deep sleep)

// ต้องเรียก pulse_update() อย่างน้อยทุก ~1 วิ (FIFO ในเซนเซอร์เก็บได้ 32 sample = 1.28 วิ ที่ 25 Hz)
