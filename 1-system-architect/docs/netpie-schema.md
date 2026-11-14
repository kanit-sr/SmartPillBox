# NETPIE Schema + LINE

> spec ที่ System Architect กำหนด ส่วน ESP-NOW อยู่ใน [message-format.md](message-format.md)

## กล่องยา → NETPIE (MQTT)

### Shadow (ค่าล่าสุด, dashboard อ่านจากตรงนี้)

กล่องอัปเดตด้วย `@shadow/data/update` → `{"data": {...}}` (ส่งเฉพาะ field ที่เปลี่ยนได้)

| field | ชนิด | มาจาก | ตัวอย่าง |
|---|---|---|---|
| `hr` | int | STATUS | 78 |
| `spo2` | int | STATUS | 97 |
| `batt` | int | STATUS | 64 |
| `node_online` | bool | กล่องคำนวณ | true |
| `node_last_seen` | int (epoch) | กล่อง | 1763170980 |
| `temp` | float | DHT11 | 29.5 |
| `hum` | float | DHT11 | 62 |
| `lid_open` | bool | reed | false |
| `person` | bool | ultrasonic | true |
| `box_state` | string | state กล่อง | `idle` / `pill_remind` / `pill_escalate` / `fall_alarm` / `storage_warn` |
| `next_pill` | string | ตารางยา | "12:00" |
| `last_pill_taken` | int (epoch) | | |
| `queue` | int | store_forward | จำนวน event ที่ค้างส่ง (โชว์ตอนเดโม E4) |
| `schedule` | array | **dashboard เขียน** | ["08:00","12:00","19:00"] |

เปิด **Feed** ของ device ไว้ ให้ `hr`, `spo2`, `batt`, `temp`, `hum` เก็บเป็นกราฟย้อนหลัง (ใช้ตอบข้อ M5 raw data)

### Event (เหตุการณ์ เกิดแล้วจบ)

topic: `@msg/spb/event` ทุก event ใช้รูปแบบเดียวกัน

```json
{"ev": "fall_confirmed", "ts": 1763170980, "id": 42, "d": {"hr": 110, "spo2": 95, "conf": 91}}
```

- `ts` = เวลาที่เกิดจริง (epoch, ไม่ใช่เวลาที่ส่ง เพื่อให้ถูกต้องแม้ส่งช้าจาก store_forward)
- `id` = เลขลำดับ event ของกล่อง กันซ้ำตอนส่งตาม

| ev | `d` |
|---|---|
| pill_taken | `slot` ("08:00"), `late_min` |
| pill_missed | `slot` |
| fall_confirmed | `hr`, `spo2`, `conf` |
| fall_cancelled | `conf` |
| fall_acknowledged | `by` ("button" / "dashboard"), `after_s` |
| sos | `hr`, `spo2` |
| low_battery | `batt` |
| node_offline | `last_seen` |
| node_power_off | `batt` **(เพิ่มใหม่ ยังไม่มีในรายชื่อเดิม)** |
| storage_warn | `temp`, `hum` |

---

## NETPIE → กล่องยา (คำสั่ง)

| เรื่อง | ช่องทาง | payload |
|---|---|---|
| เคลียร์ alarm ล้ม | ปุ่มบน dashboard publish ไป `@msg/spb/cmd` | `{"cmd": "clear_alarm"}` |
| ตารางยา | dashboard เขียน `schedule` ใน shadow | กล่องอ่านตอนเปิดเครื่อง (`@shadow/data/get`) และตอน shadow ถูกแก้ |

ตารางยาเก็บใน shadow ไม่ใช่ใน `@msg` เพราะกล่องรีบูตแล้วยังดึงกลับมาได้
(ชื่อ topic ฝั่ง subscribe ของ shadow ต้องเช็คกับเอกสาร NETPIE2020 อีกที)

---

## กล่องยา → LINE (HTTPS)

`POST https://api.line.me/v2/bot/message/push`
body: `{"to": LINE_TO, "messages": [{"type": "text", "text": "..."}]}`

| เมื่อ | ข้อความ |
|---|---|
| fall_confirmed | ⚠️ มีคนล้ม เวลา 08:03 ชีพจร 110 SpO2 95% กรุณาตรวจสอบด่วน |
| sos | 🆘 มีการกดขอความช่วยเหลือ (SOS) เวลา 08:03 |
| alarm ล้มครบ 10 นาที | ⚠️ ยังไม่มีคนรับเหตุล้ม (ผ่านไป 10 นาที) |
| fall_acknowledged | ✅ มีคนรับเหตุแล้ว เวลา 08:07 |
| pill_missed | 💊 ยังไม่ได้กินยามื้อ 08:00 (เลยมา 30 นาที) |
| low_battery | 🔋 แบตกำไลเหลือ 18% กรุณาชาร์จ |
| node_offline | 📡 กำไลขาดการติดต่อตั้งแต่ 14:20 |
| storage_warn | 🌡️ ที่เก็บยาร้อน/ชื้นเกิน 32°C ความชื้น 85% |

- ไม่ส่ง LINE: pill_taken, fall_cancelled, node_power_off (ดูใน dashboard พอ)
- ถ้าเน็ตหลุดแล้วส่งตามทีหลัง ต่อท้ายว่า "(ส่งช้า เกิดเมื่อ 08:03)"
- LINE Notify ปิดไปแล้ว ต้องใช้ Messaging API เท่านั้น และแพ็กเกจฟรีจำกัดจำนวนข้อความต่อเดือน ระวังตอนเทสซ้ำๆ

