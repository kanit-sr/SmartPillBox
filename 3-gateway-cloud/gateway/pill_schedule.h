// pill_schedule.h — เตือนกินยาตามตาราง "HH:MM"
// spec: 1-system-architect/docs/state-diagram.md ข้อ 2
//   ถึงเวลา → PILL_REMIND (ฟ้ากะพริบ + บี๊บ + สั่งกำไลสั่น)
//   ไม่เจอคนหน้ากล่องภายใน PILL_ESCALATE_MS → PILL_ESCALATE (บี๊บถี่)
//   กดปุ่ม → PILL_MUTED (เงียบ ฟ้าค้าง แต่ยังรอเปิดฝา)
//   เปิดฝาค้าง + เจอคน → pill_taken, ครบ PILL_MISSED_MS → pill_missed (+LINE)
#pragma once

enum PillState { PILL_IDLE, PILL_REMIND, PILL_ESCALATE, PILL_MUTED };

void pillBegin();
// โหลดตารางยาใหม่ (จาก netpieSchedule) n = จำนวนมื้อ
void pillSetSchedule(char slots[][6], int n);
// fallActive = กำลังมี alarm ล้ม (ระหว่างนั้นไม่สั่งกำไลสั่นเตือนยา)
void pillLoop(bool fallActive);
// ปุ่มกล่องตอนเตือนยา = ปิดเสียง (ไม่ใช่ "กินแล้ว")
void pillMute();
// เริ่มเตือนยาทันที (คำสั่งทดสอบทาง Serial)
void pillStartNow();
PillState pillState();
// พิมพ์สถานะออก Serial (คำสั่ง ?)
void pillPrint();
