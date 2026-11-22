// button.h — ปุ่มบนกำไล: กดสั้น / ค้าง 3 วิ (SOS) / ค้าง 10 วิ (ปิดเครื่อง) / ปลุกจาก deep sleep
// กฎตาม state-diagram.md: ตัดสินตอน "ปล่อยปุ่ม" ยกเว้นกดค้างครบ 10 วิ
#pragma once
#include <stdint.h>

enum ButtonEvent : uint8_t {
  BTN_NONE = 0,
  BTN_SHORT,      // ปล่อยก่อน SOS_HOLD
  BTN_SOS_READY,  // ยังกดอยู่ และเพิ่งครบ SOS_HOLD (สั่นสั้นบอกว่า "ปล่อยได้แล้ว")
  BTN_SOS,        // ปล่อยหลัง SOS_HOLD แต่ก่อน POWER_OFF_HOLD
  BTN_POWER_OFF,  // กดค้างครบ POWER_OFF_HOLD (ไม่ต้องปล่อย) — ปล่อยทีหลังจะไม่ได้ event อื่นอีก
};

// ignore_until_release = true: ถ้าปุ่มถูกกดอยู่ตอนเริ่ม (เช่นกดเพื่อปลุกจาก deep sleep) ไม่นับครั้งนั้น
void button_begin(bool ignore_until_release);

// เรียกใน loop() ทุกรอบ คืน event ได้ทีละ 1 ตัว
ButtonEvent button_update();

bool button_is_down();           // สถานะหลัง debounce
void button_wait_release();      // blocking: รอจนปล่อยปุ่ม (ใช้ก่อน deep sleep ไม่งั้นจะตื่นทันที)
void button_enable_wakeup();     // ตั้งให้กดปุ่ม (LOW) ปลุกจาก deep sleep
