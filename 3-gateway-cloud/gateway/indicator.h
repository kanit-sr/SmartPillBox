// indicator.h — RGB LED + buzzer ของกล่องยา (ไม่ใช้ delay ต้องเรียก indicatorLoop() ใน loop)
#pragma once

enum LedMode {
  LED_OFF,
  LED_GREEN,       // ปกติ
  LED_YELLOW,      // ที่เก็บยาร้อน/ชื้นเกิน หรือเน็ตหลุด
  LED_BLUE,        // เตือนยาแบบปิดเสียงแล้ว (ค้าง)
  LED_BLUE_BLINK,  // ถึงเวลากินยา
  LED_RED_BLINK    // มีคนล้ม
};

enum BeepMode {
  BEEP_OFF,
  BEEP_PILL,       // เตือนยา: บี๊บสั้นทุก 5 วิ
  BEEP_PILL_FAST,  // เตือนยา (ไม่เจอคน 10 นาที): บี๊บสั้นทุก 1 วิ
  BEEP_FALL_FAST,  // ล้ม 0–2 นาทีแรก: ดัง/เงียบ ถี่ๆ
  BEEP_FALL_SLOW   // ล้มหลัง 2 นาที: บี๊บสั้นทุก FALL_SLOW_BEEP_MS
};

void indicatorBegin();
// ตั้งโหมด LED และเสียง (เรียกทุกรอบได้ เปลี่ยนเฉพาะตอนค่าต่าง)
void indicatorSet(LedMode led, BeepMode beep);
void indicatorLoop();
