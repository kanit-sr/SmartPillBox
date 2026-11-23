// line_notify.h — ส่งข้อความเข้า LINE ผ่าน LINE Messaging API
// (LINE Notify ปิดบริการไปแล้วตั้งแต่ 31 มี.ค. 2025 ใช้ไม่ได้)
//
// ถ้า LINE_TO ใน secrets.h ว่าง → broadcast ให้ทุกคนที่แอด LINE OA ของโปรเจกต์เป็นเพื่อน
//   (ง่ายสุด ไม่ต้องหา userId; นับโควตาตามจำนวนเพื่อน)
// ถ้าใส่ LINE_TO (userId "U..." หรือ groupId "C...") → push หาคน/กลุ่มนั้น
#pragma once
#include <stdint.h>
#include <time.h>

enum LineResult {
  LINE_OK = 0,        // LINE รับแล้ว (หรือรับไปแล้วจากรอบก่อน)
  LINE_RETRY,         // เน็ต/เซิร์ฟเวอร์มีปัญหา ลองใหม่ทีหลัง
  LINE_DROP           // ผิดถาวร (token ผิด, โควตาหมด ฯลฯ) อย่าลองซ้ำ ดู Serial
};

// ส่งข้อความ text
// retryKey = รหัสประจำข้อความ (เช่น "1763170980_42") ส่งซ้ำด้วย key เดิม LINE จะไม่ส่งซ้ำให้ญาติ
//            (ใช้ header X-Line-Retry-Key) ใส่ nullptr ถ้าไม่ต้องการ
LineResult lineSend(const char* text, const char* retryKey = nullptr);

// สร้างข้อความภาษาไทยของ event ตามตารางใน netpie-schema.md
// คืน false ถ้า event นี้ไม่ต้องแจ้ง LINE (pill_taken, fall_cancelled, node_power_off)
// now = เวลาตอนส่ง ใช้ต่อท้าย "(ส่งช้า เกิดเมื่อ HH:MM)" ถ้าส่งช้ากว่า 2 นาที
// d   = JSON ของ field "d"
// node = node_id ของกำไล (> 0 จะใส่ "กำไล #n" ในข้อความ)
bool lineTextForEvent(const char* ev, time_t ts, const char* d, time_t now, uint8_t node,
                      char* out, int outLen);
