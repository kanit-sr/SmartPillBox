# NETPIE Schema + LINE

> spec ที่ System Architect กำหนด ส่วน ESP-NOW อยู่ใน [message-format.md](message-format.md)

## ตั้งค่าใน NETPIE (ทำครั้งเดียว)

1. สร้าง Project `SmartPillBox`
2. สร้าง **Device `pillbox` ตัวเดียว** = กล่องยา (gateway) กำไลและ node จำลองไม่ต้องมี device เพราะมาทาง ESP-NOW
   - กล่องต่อ `mqtt.netpie.io` port 1883 (หรือ 1884 = TLS), Client ID = ClientID ของ device, Username = Token
   - dashboard **ใช้ device เดียวกันนี้** (Dashboard > Setting > Add device แล้วเลือก `pillbox`) dashboard ต่อผ่าน agent ของ NETPIE เอง ไม่ได้ใช้ Client ID ของกล่อง ส่วนสิทธิ์ให้ติ๊ก Subscribe/Publish Message, Read/Write Shadow, Read Feed
3. สร้าง **Device Group** แล้วใส่ `pillbox` (`@msg` ใช้ได้เฉพาะ device ที่อยู่ใน group)
4. วาง Data Schema จาก [netpie-device-schema.json](netpie-device-schema.json)
   - `additionalProperties: true` เพื่อให้ field ของ node ที่ 4 ขึ้นไปยังเกิดใน shadow ได้ (แค่ไม่ถูกตรวจชนิด)
5. Feed: ใน schema ใส่ `operation.store.ttl = "30d"` ไว้ให้ 5 field คือ `hr_1`, `spo2_1`, `batt_1`, `temp`, `hum` (`store` 1 อัน = Datatag 1 อัน)
   - NETPIE จำกัดจำนวน Datatag ต่อ device (ลองใส่ 11 field แล้วขึ้น "Data tag per device limit is exceeded") จึงเก็บกราฟย้อนหลังเฉพาะกำไลจริง (node 1) กับกล่อง
   - node จำลอง (2, 3, ...) ยังโชว์ค่าล่าสุดจาก shadow ได้ปกติ แค่ไม่มีกราฟย้อนหลัง
   - ถ้าโควตายังเหลือ ค่อยเพิ่ม `store` ให้ `hr_2` ฯลฯ ทีละตัว
6. เขียน `schedule` เริ่มต้นลง shadow ก่อนเปิดกล่องครั้งแรก
7. **เทสก่อนเลย**: กดปุ่ม `publishMsg("spb/cmd", ...)` บน dashboard แล้วดูว่ากล่องได้รับไหม (เอกสารไม่ได้บอกตรงๆ ว่า device ได้รับ msg ที่ dashboard ส่งในนามตัวเองหรือเปล่า) ถ้าไม่ได้รับ ให้เปลี่ยนไปใช้ทางสำรอง: ปุ่มเขียน shadow `clear_alarm_req` (epoch) แล้วกล่องจับจาก `@shadow/data/updated`

## กล่องยา → NETPIE (MQTT)

### Shadow (ค่าล่าสุด, dashboard อ่านจากตรงนี้)

กล่องอัปเดตด้วย `@shadow/data/update` → `{"data": {...}}` (ส่งเฉพาะ field ที่เปลี่ยนได้)

**field ของกำไลแยกต่อ node** ด้วย suffix `_<node_id>` (เช่น `hr_1`, `hr_2`, `hr_3`) ทำ widget ใน NETPIE ง่ายกว่า object ซ้อน

| field | ชนิด | มาจาก | ตัวอย่าง |
|---|---|---|---|
| `hr_<n>` | int | STATUS | `hr_1`: 78 |
| `spo2_<n>` | int | STATUS | `spo2_1`: 97 |
| `batt_<n>` | int | STATUS | `batt_1`: 64 |
| `node_online_<n>` | bool | กล่องคำนวณ | `node_online_1`: true |
| `node_last_seen_<n>` | int (epoch) | กล่อง | `node_last_seen_1`: 1763170980 |
| `nodes` | array | กล่อง | รายชื่อ node ที่ลงทะเบียนแล้ว `[1, 2, 3]` |

node ใหม่ลงทะเบียนเมื่อไร field `_<n>` ของมันก็เกิดใน shadow เอง แต่ widget บน NETPIE dashboard ต้องเพิ่มเองต่อ node

**field ของกล่อง**

| field | ชนิด | มาจาก | ตัวอย่าง |
|---|---|---|---|
| `temp` | float | DHT11 | 29.5 |
| `hum` | float | DHT11 | 62 |
| `lid_open` | bool | reed | false |
| `person` | bool | ultrasonic | true |
| `box_state` | string | state กล่อง | `idle` / `pill_remind` / `pill_escalate` / `fall_alarm` / `storage_warn` |
| `alarm_node` | int | กล่อง | node ที่ทำให้เกิด `fall_alarm` (0 = ไม่มี alarm) |
| `next_pill` | string | ตารางยา | "12:00" |
| `last_pill_taken` | int (epoch) | | |
| `queue` | int | store_forward | จำนวน event ที่ค้างส่ง (โชว์ตอนเดโม E4) |
| `schedule` | array | **dashboard เขียน** | ["08:00","12:00","19:00"] |
| `last_event` | object | กล่อง | event ล่าสุดที่ส่ง `@msg/spb/event` (payload เดียวกันเป๊ะ) ไว้ดูย้อนหลังตอนเทส เพราะ `@msg` ไม่ถูกเก็บ |

เปิด **Feed** ของ device ไว้ ให้ `hr_<n>`, `spo2_<n>`, `batt_<n>`, `temp`, `hum` เก็บเป็นกราฟย้อนหลัง (ใช้ตอบข้อ M5 raw data)

**อ้างอิงใน dashboard** (ไวยากรณ์ของ NETPIE ใช้ `#` แทนชื่อ datasource)
- อ่าน shadow: `#["pillbox"]["shadow"]["hr_1"]`
- อ่าน event: `#["pillbox"]["msg"]["spb"]["event"]`
- ปุ่มเคลียร์ alarm: `#["pillbox"].publishMsg("spb/cmd", '{"cmd":"clear_alarm"}')` (ไม่ต้องใส่ `@msg/`)
- แก้ตารางยา: `#["pillbox"].writeShadow("schedule", ["08:00","12:00","19:00"])` (ใช้ HTML widget ทำช่องกรอก)

### Event (เหตุการณ์ เกิดแล้วจบ)

topic: `@msg/spb/event` ทุก event ใช้รูปแบบเดียวกัน

```json
{"ev": "fall_confirmed", "ts": 1763170980, "id": 42, "node": 1, "d": {"hr": 110, "spo2": 95, "conf": 91}}
```

- `ts` = เวลาที่เกิดจริง (epoch, ไม่ใช่เวลาที่ส่ง เพื่อให้ถูกต้องแม้ส่งช้าจาก store_forward)
- `id` = เลขลำดับ event ของกล่อง กันซ้ำตอนส่งตาม
- `node` = `node_id` ของกำไลที่เกี่ยวข้อง, event ของกล่องเอง (pill_taken, pill_missed, storage_warn) ใส่ 0

| ev | `d` |
|---|---|
| pill_taken | `slot` ("08:00"), `late_min` |
| pill_missed | `slot` |
| fall_confirmed | `hr`, `spo2`, `conf` |
| fall_cancelled | `conf` |
| fall_acknowledged | `by` ("button" / "dashboard"), `after_s` (`node` = node ที่เกิดเหตุ) |
| sos | `hr`, `spo2` |
| low_battery | `batt` |
| node_offline | `last_seen` |
| node_power_off | `batt` **(เพิ่มใหม่ ยังไม่มีในรายชื่อเดิม)** |
| storage_warn | `temp`, `hum` |

---

## NETPIE → กล่องยา (คำสั่ง)

| เรื่อง | ช่องทาง | payload |
|---|---|---|
| เคลียร์ alarm ล้ม | ปุ่มบน dashboard publish ไป `@msg/spb/cmd` | `{"cmd": "clear_alarm"}` (เคลียร์ alarm ที่มีอยู่ ของ node ไหนก็ได้) |
| ตารางยา | dashboard เขียน `schedule` ใน shadow | กล่องอ่านตอนเปิดเครื่อง (`@shadow/data/get`) และตอน shadow ถูกแก้ |

ตารางยาเก็บใน shadow ไม่ใช่ใน `@msg` เพราะกล่องรีบูตแล้วยังดึงกลับมาได้
topic ของ shadow ตามเอกสาร NETPIE2020: ขอค่า publish `@shadow/data/get` → คำตอบมาที่ `@private/shadow/data/get/response`, รู้ว่า shadow ถูกแก้ subscribe `@shadow/data/updated`
(subscribe จะได้แค่ข้อความหลังจาก subscribe แล้ว จึงต้อง `get` ตอนบูตเสมอ)

---

## กล่องยา → LINE (HTTPS)

`POST https://api.line.me/v2/bot/message/push`
body: `{"to": LINE_TO, "messages": [{"type": "text", "text": "..."}]}`

| เมื่อ | ข้อความ |
|---|---|
| fall_confirmed | ⚠️ มีคนล้ม (กำไล #1) เวลา 08:03 ชีพจร 110 SpO2 95% กรุณาตรวจสอบด่วน |
| sos | 🆘 มีการกดขอความช่วยเหลือ (SOS) จากกำไล #1 เวลา 08:03 |
| alarm ล้มครบ 10 นาที | ⚠️ ยังไม่มีคนรับเหตุล้ม (ผ่านไป 10 นาที) |
| fall_acknowledged | ✅ มีคนรับเหตุแล้ว เวลา 08:07 |
| pill_missed | 💊 ยังไม่ได้กินยามื้อ 08:00 (เลยมา 30 นาที) |
| low_battery | 🔋 แบตกำไล #1 เหลือ 18% กรุณาชาร์จ |
| node_offline | 📡 กำไล #1 ขาดการติดต่อตั้งแต่ 14:20 |
| storage_warn | 🌡️ ที่เก็บยาร้อน/ชื้นเกิน 32°C ความชื้น 85% |

- ไม่ส่ง LINE: pill_taken, fall_cancelled, node_power_off (ดูใน dashboard พอ)
- ถ้าเน็ตหลุดแล้วส่งตามทีหลัง ต่อท้ายว่า "(ส่งช้า เกิดเมื่อ 08:03)"
- LINE Notify ปิดไปแล้ว ต้องใช้ Messaging API เท่านั้น และแพ็กเกจฟรีจำกัดจำนวนข้อความต่อเดือน ระวังตอนเทสซ้ำๆ

