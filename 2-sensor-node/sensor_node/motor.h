// motor.h — มอเตอร์สั่น (non-blocking: ตั้ง pattern แล้วเรียก motor_update() ใน loop)
#pragma once
#include <stdint.h>

void motor_begin();
void motor_update();                 // เรียกใน loop() ทุกรอบ

void motor_on();                     // สั่นค้างจนกว่าจะเรียก motor_off() (ใช้ตอนนับถอยหลังล้ม)
void motor_off();                    // หยุดทันที ยกเลิก pattern ที่ค้างอยู่
void motor_pulse(uint8_t count, uint16_t on_ms, uint16_t off_ms);  // สั่น count ครั้ง แทน pattern เดิม
bool motor_busy();                   // ยังสั่น / ยังเล่น pattern อยู่

// ---------- รูปแบบที่ใช้ในกำไล ----------
inline void motor_pill_remind()  { motor_pulse(3, 400, 300); }  // ACK มี CMD_PILL_REMIND: สั่น 3 ครั้ง
inline void motor_fall_cleared() { motor_pulse(1, 1500, 0); }   // ACK มี CMD_FALL_CLEARED: สั่นยาว 1 ครั้ง
inline void motor_sos_ready()    { motor_pulse(1, 150, 0); }    // กดค้างครบ 3 วิ: "ปล่อยได้แล้ว"
inline void motor_alert_sent()   { motor_pulse(2, 120, 120); }  // ส่ง FALL_CONFIRMED/SOS ถึงกล่องแล้ว
inline void motor_boot()         { motor_pulse(1, 200, 0); }    // เปิดเครื่อง
inline void motor_power_off()    { motor_pulse(1, 800, 0); }    // กำลังปิดเครื่อง
