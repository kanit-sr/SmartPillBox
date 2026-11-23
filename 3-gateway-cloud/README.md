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

## gateway.ino ทำงานยังไง

```
loop():
  linkLoop()          packet จากกำไล → node_monitor (shadow hr_<n>, แบตต่ำ, ขาดการติดต่อ)
                                     → fall_alarm (fall_confirmed / sos / fall_cancelled)
  ปุ่มกล่อง           มี alarm ล้ม → เคลียร์ (fall_acknowledged) / ไม่มี → ปิดเสียงเตือนยา
  netpieTakeClearAlarm()  ปุ่มเคลียร์บน dashboard
  netpieSchedule()    ตารางยาใหม่ → pill_schedule
  pillLoop()          เตือนยา 10/30 นาที, pill_taken / pill_missed
  updateOutputs()     เลือก LED / buzzer / box_state ตามลำดับความสำคัญ (state-diagram.md ข้อ 3)
```

ชื่อ field ใน shadow แยกต่อ node (`hr_1`, `spo2_1`, `batt_1`, `node_online_1`, ...) ตาม [netpie-schema.md](../1-system-architect/docs/netpie-schema.md)

### ทดสอบทาง Serial Monitor (115200)

| key | ทำอะไร |
|---|---|
| `p` | เริ่มเตือนยาทันที |
| `f` / `s` | จำลองล้ม / SOS จากกำไล #1 (ส่ง LINE จริง) |
| `b` | จำลอง STATUS แบต 15% จากกำไล #1 (ส่ง LINE จริงครั้งแรก) |
| `t` | บังคับที่เก็บยาร้อนเกิน (storage_warn) |
| `?` | พิมพ์สถานะทุกโมดูล |

เดโมในห้อง: ตั้ง `#define DEMO_FAST 1` ใน `config.h` เวลาที่เป็นนาที (เตือนยา 10/30, alarm 2/10, ที่เก็บยา 10) จะหดเป็นวินาที

ทดสอบ dashboard ก่อนมีฮาร์ดแวร์: `4-dashboard-testing/tools/mock_box.py` (กล่องจำลอง ส่งเข้า NETPIE ไม่ส่ง LINE)
