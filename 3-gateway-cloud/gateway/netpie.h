// netpie.h — กล่องยา ↔ NETPIE2020 (MQTT) + ส่ง LINE ตาม event
// spec: 1-system-architect/docs/netpie-schema.md
//
// งาน network ทั้งหมด (Wi-Fi, NTP, MQTT, LINE) รันใน FreeRTOS task แยกบน core 0
// loop() ของกล่องจึงไม่ค้างตอนต่อเน็ต/ส่ง LINE → buzzer/LED/ESP-NOW ทำงานต่อได้ตลอด
// ทุกฟังก์ชันข้างล่างเรียกจาก loop() ได้เลย (thread-safe) และคืนค่าทันที
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

// เริ่ม Wi-Fi + task ของ NETPIE
// เรียกใน setup() "ก่อน" linkBegin(nullptr) เพราะ ESP-NOW ต้องเปิด Wi-Fi โหมด STA ก่อน
// และต้องเรียกก่อนฟังก์ชันอื่นในไฟล์นี้
void netpieBegin();

// Wi-Fi + MQTT ต่ออยู่ไหม (ใช้ทำ LED เหลืองตอนเน็ตหลุด)
bool netpieOnline();

// เวลาปัจจุบันจาก NTP (epoch วินาที), 0 = ยังไม่ได้ sync
time_t netpieNow();

// ---------- Shadow (@shadow/data/update) ----------
// เรียกได้บ่อยเท่าไรก็ได้ จะส่งเฉพาะ field ที่ค่าเปลี่ยน รวมเป็น update เดียวทุก ~2 วิ
// field ที่เปิด store ใน Device Schema (hr, spo2, batt, temp, hum) จะเข้า Feed ให้เอง
void netpieSet(const char* key, int value);
void netpieSet(const char* key, float value);
void netpieSet(const char* key, bool value);
void netpieSet(const char* key, const char* value);

// ---------- Event (@msg/spb/event + LINE) ----------
// ev     = ชื่อ event ตาม netpie-schema.md เช่น "fall_confirmed"
// dJson  = object ของ field "d" เป็น JSON string เช่น "{\"hr\":110,\"spo2\":95,\"conf\":91}"
//          (ใส่ nullptr ถ้าไม่มี)
// ts     = เวลาที่เกิดจริง (epoch) ใส่ 0 = ตอนนี้
//          ถ้าเป็น event จากกำไล ให้ใส่ netpieNow() - age_ms/1000
// event เข้าคิว store_forward ก่อนเสมอ เน็ตหลุดก็ไม่หาย (เก็บได้ SF_MAX อัน) แล้วส่งตามทีหลัง
// คืน false ถ้าคิวภายในเต็มหรือ JSON ยาวเกิน
bool netpieEvent(const char* ev, const char* dJson = nullptr, time_t ts = 0);

// ---------- คำสั่งจาก dashboard ----------
// คืน true ครั้งเดียวต่อการกดปุ่ม "เคลียร์ alarm" บน dashboard (@msg/spb/cmd {"cmd":"clear_alarm"})
bool netpieTakeClearAlarm();

// ตารางยาจาก shadow field "schedule" ("HH:MM" หลายมื้อ)
// คืนจำนวนมื้อ (0..maxSlots), *changed = true ถ้าเปลี่ยนตั้งแต่เรียกครั้งก่อน
// ตารางล่าสุดถูกจำไว้ใน flash (Preferences) เปิดเครื่องตอนเน็ตหลุดก็ยังมีตารางยา
int netpieSchedule(char out[][6], int maxSlots, bool* changed);
