// sensors.h — reed switch (ฝา), HC-SR04 (คนหน้ากล่อง), DHT11 (ที่เก็บยา)
// ต้องเรียก sensorsLoop() ทุกรอบ loop ฟังก์ชันอื่นแค่คืนค่าล่าสุด
#pragma once

void sensorsBegin();
void sensorsLoop();

bool lidOpen();       // ฝาเปิดอยู่ตอนนี้
bool lidOpenHeld();   // ฝาเปิดค้างนานกว่า LID_OPEN_HOLD_MS
bool personNear();    // มีคนใกล้กว่า PERSON_NEAR_CM (ไม่ได้ echo = false)
float sonarCm();      // ระยะล่าสุด (cm), -1 = ไม่ได้ echo

float boxTemp();      // °C, NAN = อ่านไม่ได้
float boxHum();       // %,  NAN = อ่านไม่ได้
// ร้อน/ชื้นเกินต่อเนื่องนานกว่า STORAGE_WARN_HOLD_MS (อ่านไม่ได้ไม่นับว่าเกิน)
bool storageBad();
// บังคับให้ storageBad() เป็น true (คำสั่งทดสอบทาง Serial) จนกว่าค่าจริงจะปกติ
void sensorsForceStorageBad();
