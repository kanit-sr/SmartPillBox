# 3. Gateway & Cloud (กล่องยา) — พสิษฐ์


ESP32 DevKit + Reed + DHT11 + Ultrasonic + RGB LED + Buzzer + ปุ่มเคลียร์

| ไฟล์ | หน้าที่ |
|---|---|
| `config.h` | ขา GPIO, ค่าตั้งต่างๆ |
| `secrets.example.h` | ตัวอย่าง Wi-Fi/NETPIE/LINE (copy เป็น secrets.h) |
| `sensors.*` | Reed (เปิดฝาค้าง >2 วิ), Ultrasonic, DHT11 |
| `indicator.*` | RGB LED + รูปแบบเสียง buzzer |
| `button.*` | ปุ่มเคลียร์ alarm |
| `pill_schedule.*` | เตือนยา 10/30 นาที, pill_taken/missed |
| `fall_alarm.*` | alarm ล้ม 0–2 นาที / หลัง 2 นาที / 10 นาที |
| `node_monitor.*` | แบตต่ำ, กำไลขาดการติดต่อ 3 นาที |
| `netpie.*` | Wi-Fi, NTP, MQTT, shadow, event + store-and-forward, คำสั่งเคลียร์/ตารางยาจาก dashboard |
| `line_notify.*` | LINE Messaging API (push / broadcast) + ข้อความของแต่ละ event |
| `gateway.ino` | setup/loop + state machine ของกล่องยา |

ไฟล์ทั้งหมดอยู่ใน `gateway/` (folder เดียวกับ .ino) — Arduino IDE จะเปิดเป็น tab ให้เอง

Library ที่ต้องลงเพิ่ม (Library Manager): **PubSubClient** (Nick O'Leary), **ArduinoJson** v7 (Benoit Blanchon)

ตั้งค่า NETPIE + LINE ทีละขั้น: [SETUP-cloud.md](SETUP-cloud.md) · Device Schema: [netpie-device-schema.json](../1-system-architect/docs/netpie-device-schema.json)

## netpie.* ทำงานยังไง

```
กล่องยา ──MQTT──► NETPIE2020 (shadow + feed) ◄── NETPIE dashboard เดิม (4-dashboard-testing/dashboard)
   └──HTTPS──► LINE Messaging API ──► LINE ญาติ
```

- งาน network ทั้งหมดรันใน FreeRTOS task แยก (core 0) → `loop()` ไม่ค้างตอนต่อเน็ต/ส่ง LINE
- event ทุกตัวเข้า `store_forward` ก่อน เน็ตหลุดก็ไม่หาย พอเน็ตกลับค่อยส่งตาม + LINE ต่อท้าย "(ส่งช้า เกิดเมื่อ ...)"
- LINE ใช้ `X-Line-Retry-Key` → retry แล้วญาติไม่ได้ข้อความซ้ำ
- event ล่าสุดเขียนลง shadow (`last_ev`, `last_ev_ts`) ด้วย เพราะ `@msg` ไม่ถูกเก็บในฐานข้อมูลของ NETPIE

## ใช้ใน gateway.ino

```cpp
#include "netpie.h"

void setup() {
  Serial.begin(115200);
  netpieBegin();         // ก่อน linkBegin เสมอ
  linkBegin(nullptr);
  linkOnReceive(onPacket);
}

void onPacket(uint8_t type, const uint8_t* data, int len, bool isDup) {
  if (type == STATUS) {
    auto* p = (const StatusPacket*)data;
    netpieSet("hr", (int)p->hr);
    netpieSet("spo2", (int)p->spo2);
    netpieSet("batt", (int)p->batt_pct);
    netpieSet("node_online", true);
    netpieSet("node_last_seen", (int)netpieNow());
  } else if (type == FALL_CONFIRMED && !isDup) {
    auto* p = (const FallConfirmedPacket*)data;
    char d[64];
    snprintf(d, sizeof d, "{\"hr\":%u,\"spo2\":%u,\"conf\":%u}", p->hr, p->spo2, p->conf);
    time_t ts = netpieNow() ? netpieNow() - p->age_ms / 1000 : 0;
    netpieEvent("fall_confirmed", d, ts);   // → @msg/spb/event + LINE
    netpieSet("box_state", "fall_alarm");
  }
}

void loop() {
  linkLoop();
  if (netpieTakeClearAlarm()) { /* หยุด alarm เหมือนกดปุ่มที่กล่อง */ }

  bool changed;
  char slots[8][6];
  int n = netpieSchedule(slots, 8, &changed);
  if (changed) { /* โหลดตารางยาใหม่ลง pill_schedule */ }

  // alarm ล้มครบ 10 นาทียังไม่มีคนรับ → netpieEvent("fall_unanswered", "{\"after_s\":600}");
}
```

ทดสอบ dashboard ก่อนมีฮาร์ดแวร์: `4-dashboard-testing/tools/mock_box.py` (กล่องจำลอง ส่งเข้า NETPIE ไม่ส่ง LINE)
