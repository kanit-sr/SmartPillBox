// config.h — ขา GPIO และค่าตั้งของกล่องยา
// ขาตาม test/hw_check_box (เทสกับบอร์ดจริงแล้ว)
#pragma once

// ---------- ขา GPIO (ESP32 DevKit) ----------
#define PIN_REED     4    // mini reed switch (ฝากล่อง)
#define PIN_BUTTON   16   // ปุ่มเคลียร์ alarm (INPUT_PULLUP กด = LOW)
#define PIN_BUZZER   17   // active buzzer (HIGH = ดัง)
#define PIN_TRIG     5    // HC-SR04 Trig
#define PIN_ECHO     18   // HC-SR04 Echo (ผ่าน voltage divider 5V → 3.3V)
#define PIN_DHT      19   // DHT11
#define PIN_LED_R    21   // RGB LED (HIGH = ติด)
#define PIN_LED_G    22
#define PIN_LED_B    23

// ระดับของ reed ตอน "ฝาเปิด" ถ้าเทสแล้วกลับด้านให้แก้ตรงนี้ที่เดียว
#define REED_OPEN_LEVEL  HIGH

// ---------- เดโมในห้อง ----------
// 1 = เวลาที่เป็น "นาที" ข้างล่างหดเป็น "วินาที" (เตือนยา 10/30 นาที → 10/30 วิ ฯลฯ)
#define DEMO_FAST  0

#if DEMO_FAST
#define MINUTE_MS  1000UL
#else
#define MINUTE_MS  60000UL
#endif

// ---------- เซนเซอร์ ----------
#define LID_OPEN_HOLD_MS     2000UL   // เปิดฝาค้างเกินนี้ = เปิดจริง
#define PERSON_NEAR_CM       60       // ใกล้กว่านี้ = มีคนหน้ากล่อง
#define SONAR_INTERVAL_MS    200UL    // อ่าน HC-SR04 ทุกเท่านี้
#define DHT_INTERVAL_MS      5000UL   // อ่าน DHT11 ทุกเท่านี้ (DHT11 อ่านถี่กว่า 1 วิไม่ได้)

// ---------- ที่เก็บยา ----------
#define STORAGE_TEMP_MAX     30.0     // °C
#define STORAGE_HUM_MAX      75.0     // %
#define STORAGE_WARN_HOLD_MS (10 * MINUTE_MS)  // ร้อน/ชื้นเกินต่อเนื่องนานเท่านี้ถึงเตือน

// ---------- เตือนยา ----------
#define PILL_ESCALATE_MS     (10 * MINUTE_MS)  // ไม่เจอคนหน้ากล่อง → buzzer ถี่ขึ้น
#define PILL_MISSED_MS       (30 * MINUTE_MS)  // ยังไม่เปิดฝา → ลืมกินยา
#define MAX_PILL_SLOTS       8                 // มื้อยาสูงสุดต่อวัน

// ---------- alarm ล้ม ----------
#define FALL_FAST_MS         (2 * MINUTE_MS)   // ช่วงแรก buzzer ถี่
#define FALL_SLOW_BEEP_MS    20000UL           // หลังจากนั้นบี๊บสั้นทุกเท่านี้
#define FALL_UNANSWERED_MS   (10 * MINUTE_MS)  // ยังไม่มีคนรับ → LINE เตือนซ้ำ

// ---------- cloud ----------
#define SHADOW_SENSOR_MS     30000UL  // ส่ง temp/hum/lid/person ขึ้น shadow ทุกเท่านี้
