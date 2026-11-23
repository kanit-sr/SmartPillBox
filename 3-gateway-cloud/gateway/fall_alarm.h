// fall_alarm.h — alarm ล้ม / SOS ของกล่องยา
// spec: 1-system-architect/docs/state-diagram.md ข้อ 1
//   NO_ALARM → ได้ FALL_CONFIRMED / SOS (ไม่ใช่ dup) → ส่ง event + LINE, เข้า alarm
//   ระหว่าง alarm ได้อันใหม่ → ส่ง event + LINE แต่ไม่รีเซ็ตเวลา alarm
//   ครบ FALL_UNANSWERED_MS → fall_unanswered (LINE เตือนซ้ำ 1 ครั้ง)
//   fallClear() → fall_acknowledged + บอกกำไลผ่าน ACK (CMD_FALL_CLEARED)
#pragma once
#include <stdint.h>
#include "indicator.h"

void fallAlarmBegin();
void fallOnPacket(uint8_t node, uint8_t type, const uint8_t* data, int len, bool isDup);
void fallAlarmLoop();

bool fallActive();
uint8_t fallNode();          // node ที่ทำให้เกิด alarm (0 = ไม่มี)
BeepMode fallBeep();         // BEEP_FALL_FAST ช่วง FALL_FAST_MS แรก แล้ว BEEP_FALL_SLOW
// เคลียร์ alarm: by = "button" หรือ "dashboard" (ไม่มี alarm อยู่ = ไม่ทำอะไร)
void fallClear(const char* by);

// จำลองเหตุการณ์ (คำสั่งทดสอบทาง Serial) type = FALL_CONFIRMED หรือ SOS
void fallSimulate(uint8_t node, uint8_t type);
