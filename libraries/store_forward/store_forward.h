// store_forward.h — เก็บ event ตอนเน็ตหลุด แล้วส่งตามเมื่อเน็ตกลับ (กล่องยาใช้)
// เจ้าของ: คณิต | ใช้โดย: gateway.ino
// คิวอยู่ใน RAM (ไฟดับ/รีบูตแล้วหาย) เก็บได้ SF_MAX event, เต็มแล้วทิ้งอันเก่าสุด
#pragma once
#include <stdbool.h>

#define SF_MAX  20    // จำนวน event สูงสุดในคิว
#define SF_LEN  200   // ความยาว JSON สูงสุดต่อ event

// ฟังก์ชันส่งจริง (ize เขียน เช่น publish ไป @msg/spb/event) คืน true ถ้าส่งสำเร็จ
typedef bool (*SfPublishFn)(const char* json);

// ใส่ event ที่สร้าง JSON แล้ว เช่น
// {"ev":"fall_confirmed","ts":1763170980,"id":42,"d":{"hr":110,"spo2":95,"conf":91}}
// คืน false ถ้า JSON ยาวเกิน SF_LEN
bool sfPush(const char* json);

// เรียกใน loop(): ส่ง event จากเก่าไปใหม่จนคิวว่าง หรือจน publish คืน false (เน็ตหลุด)
// คืนจำนวน event ที่ส่งได้รอบนี้
int sfFlush(SfPublishFn publish);

// จำนวน event ที่ค้างส่ง (ใช้กับ field "queue" ใน shadow)
int sfSize();
