# Gateway (กล่องยา) Firmware Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** เขียนเฟิร์มแวร์กล่องยา (ESP32 DevKit) ให้ครบตาม system-flow.md: รับ packet จากกำไล, เตือนยา, alarm ล้ม, เฝ้ากำไล, เฝ้าที่เก็บยา แล้วส่งขึ้น NETPIE + LINE

**Architecture:** แต่ละเรื่องเป็นไฟล์ `.h/.cpp` คู่ของตัวเอง (ประกาศใน .h, เขียนใน .cpp) ทุกโมดูลเป็น non-blocking ใช้ `millis()` ไม่มี `delay()` ใน loop. `gateway.ino` แค่เรียก `xxxBegin()` / `xxxLoop()` แล้วเลือก LED/buzzer/`box_state` ตามตารางลำดับความสำคัญ. งาน network ใช้ `netpie.*` / `line_notify.*` ที่มีอยู่แล้ว (รันบน core 0)

**Tech Stack:** Arduino core esp32 4.0.0 (arduino-cli), PubSubClient, ArduinoJson v7, DHT sensor library, lib ของทีม `protocol` / `espnow_link` / `store_forward`

**Spec:** `system-flow.md`, `1-system-architect/docs/state-diagram.md`, `1-system-architect/docs/message-format.md`, `1-system-architect/docs/netpie-schema.md`

## สิ่งที่ต้องให้ Kanit ตัดสินใจ (ก่อนเริ่ม)

1. **ชื่อ field ใน NETPIE** → เสนอใช้แบบ **แยกต่อ node (`hr_1`, `spo2_1`, `batt_1`, `node_online_1`, ...)** ตาม `1-system-architect/docs/netpie-device-schema.json`
   เหตุผล: ตรงกับ decisions.md (2026-11-20), dashboard README ใช้ `#["pillbox"]["shadow"]["hr_1"]` อยู่แล้ว, test/hw_netpie_box ก็ส่ง `hr_1` และเทสผ่านแล้ว.
   ไฟล์ `3-gateway-cloud/netpie-device-schema.json` (แบบ `hr`) จะลบทิ้ง แล้วให้ SETUP-cloud.md ชี้ไปไฟล์ของ architect ไฟล์เดียว และแก้ alias `spb_box` → `pillbox` ในเอกสาร
2. **`protocol.h` ยังไม่มี `node_id` ใน header** (spec บอก 5 bytes แต่โค้ดจริง 4 bytes) → เสนอ **เพิ่ม `uint8_t node_id`** ใน `PacketHeader` ตอนนี้เลย
   ผลกระทบ: ต้อง flash กำไลใหม่ด้วย (โค้ดกำไล `packets.h` ใส่ `NODE_ID` ให้เองอัตโนมัติเมื่อมี field นี้ ไม่ต้องแก้โค้ดกำไล), test sketch เก่าใน `test/` ต้อง compile ใหม่
   ทางเลือก: ยังไม่เพิ่ม แล้วกล่องถือว่าทุก packet เป็น node 1 (ง่ายกว่า แต่เสียคะแนน Scalability)

## Global Constraints

- ประกาศใน `.h`, เขียนใน `.cpp`, เรียกจาก `.ino` · โค้ดเรียบง่าย อ่านง่าย (งานส่งในคลาส) · คอมเมนต์ภาษาไทย ตามสไตล์ไฟล์เดิม
- ไม่มี `delay()` ใน `loop()` (ยกเว้น `pulseIn` ของ HC-SR04 ซึ่ง timeout 25 ms และอ่านแค่ทุก 200 ms)
- ชื่อ event / field / ค่าคงที่ ตาม netpie-schema.md และ protocol.h เป๊ะ
- ขา GPIO (จาก `test/hw_check_box` ที่เทสแล้ว): REED 4, BUTTON 16 (INPUT_PULLUP, กด = LOW), BUZZER 17, TRIG 5, ECHO 18 (ผ่าน divider), DHT 19, LED R/G/B 21/22/23
- ห้าม commit `secrets.h` · commit แบบ conventional commits · Kanit push เอง
- compile: `arduino-cli compile --fqbn esp32:esp32:esp32:UploadSpeed=115200 3-gateway-cloud/gateway` · upload: Kanit กด BOOT เอง ที่ COM3

## Review Focus

1. **บูตแล้วยังไม่ได้เวลา NTP** (`netpieNow()==0`) → ห้ามเตือนยา ห้ามถือว่าเป็น 00:00 (pill_schedule ต้องข้ามจนกว่าจะได้เวลา)
2. **มื้อเดียวกันต้องเตือนครั้งเดียวต่อวัน** แม้ loop วนหลายรอบในนาทีนั้น หรือ dashboard แก้ตารางระหว่างเตือน
3. **packet ซ้ำ (`isDup`) จาก FALL_CONFIRMED/SOS ที่กำไลส่งซ้ำ** → ไม่เริ่ม alarm ใหม่ ไม่ส่ง LINE ซ้ำ
4. **เน็ตหลุด** → alarm/เตือนยาที่กล่องทำงานปกติ, LED เหลืองเมื่อไม่มีอะไรสำคัญกว่า, event เข้าคิวส่งทีหลัง
5. **เซนเซอร์อ่านไม่ได้** (DHT ได้ NaN, HC-SR04 ไม่มี echo) → ถือว่า "ไม่รู้" ไม่ใช่ "ร้อนเกิน" หรือ "มีคน"

แต่ละข้อมีขั้นเทสอยู่ใน task ที่เป็นเจ้าของโค้ด

## วิธีเทส (Arduino ไม่มี unit test)

แต่ละ task เทสด้วย (1) compile ผ่าน และ (2) Kanit upload แล้วดู Serial 115200. `gateway.ino` มีคำสั่งทดสอบทาง Serial ให้จำลองเหตุการณ์โดยไม่ต้องรอเวลาจริง:

| key | ทำอะไร |
|---|---|
| `p` | เริ่มเตือนยาทันที (slot = เวลาปัจจุบัน) |
| `f` | จำลอง FALL_CONFIRMED จาก node 1 |
| `s` | จำลอง SOS จาก node 1 |
| `b` | จำลอง STATUS แบต 15% จาก node 1 |
| `t` | บังคับ storage_warn |
| `?` | พิมพ์สถานะทุกโมดูล |

กำไลจริง / `test/hw_band_espnow` (MOCK_SENSORS=1, key n/w/h/f/s) ใช้เทสเส้นทาง ESP-NOW จริง
config.h มี `#define DEMO_FAST 0` ถ้าตั้ง 1 เวลา 10/30 นาทีของยา และ 2/10 นาทีของ alarm หด 60 เท่า (นาที → วินาที) ไว้เดโมในห้อง

---

### Task 0: เตรียม branch, secrets, เอกสาร schema

**Files:**
- Restore: `3-gateway-cloud/gateway/secrets.example.h` (ใน working copy ถูกลบไว้ เอากลับมาแล้ว) แก้คอมเมนต์ alias เป็น `pillbox`
- Delete: `3-gateway-cloud/netpie-device-schema.json`
- Modify: `3-gateway-cloud/SETUP-cloud.md`, `3-gateway-cloud/README.md` (alias `pillbox`, ลิงก์ schema ไป `1-system-architect/docs/netpie-device-schema.json`, ตัวอย่างโค้ดใช้ `hr_1`)

- [ ] **Step 1:** `git status` ต้องไม่เห็น `secrets.h` · `secrets.h` ของ gateway ตอนนี้ Wi-Fi/NETPIE/LINE ตรงกับ `test/hw_line_box/secrets.h` แล้ว (เช็ค hash แล้ว ไม่ต้องแก้)
- [ ] **Step 2:** แก้เอกสาร 3 ไฟล์ข้างบน
- [ ] **Step 3:** Commit `docs(gateway): use per-node NETPIE schema and pillbox alias`

### Task 1: `node_id` ใน protocol + espnow_link (ถ้า Kanit เลือกข้อ 2 = เพิ่ม)

**Files:**
- Modify: `libraries/protocol/protocol.h` — `PacketHeader { ver; type; node_id; seq; }` ตามลำดับใน message-format.md, แก้ static_assert ทุกตัว +1 (header 5, STATUS 11, FALL_CONFIRMED 13, FALL_CANCELLED 10, SOS 12, POWER_OFF 6, MOTION 17, ACK 9)
- Modify: `libraries/espnow_link/espnow_link.cpp` — dedup แยกต่อ node (`lastRxSeq[256]`, `lastRxType[256]`, `haveLastRx[256]` หรือ array ขนาด `MAX_NODES` ก็ได้), `sendAck` ใส่ `a.hdr.node_id = h->node_id`, ทิ้ง packet ที่ `node_id` เป็น 0 หรือ 255
- Modify: ฝั่งกำไลใน `linkSend` รอ ACK: เช็คว่า `ack.hdr.node_id` ตรงกับของตัวเอง (ใช้ `node_id` ที่อยู่ใน pkt ที่ส่ง)
- Modify: `libraries/protocol/protocol.h` — เพิ่ม `#define MAX_NODES 8`
- Modify: `libraries/espnow_link/examples/fake_box`, `fake_wristband` ให้ใส่ node_id

- [ ] **Step 1:** แก้ตามข้างบน
- [ ] **Step 2:** compile `libraries/protocol/examples/print_sizes` (ต้องพิมพ์ขนาดใหม่ตามข้างบน), `fake_box`, และกำไล `2-sensor-node/sensor_node` ต้องผ่านทั้งหมด
- [ ] **Step 3:** เทสจริง: flash `fake_box` + กำไล → Serial ของ fake_box ต้องเห็น `node=1`
- [ ] **Step 4:** Commit `feat(protocol): add node_id to packet header`

### Task 2: netpie / line_notify รองรับ node

**Files:**
- Modify: `3-gateway-cloud/gateway/netpie.h/.cpp`, `line_notify.h/.cpp`

**Interfaces (Produces):**
- `bool netpieEvent(const char* ev, const char* dJson = nullptr, time_t ts = 0, uint8_t node = 0);` — ใส่ `"node": node` ลง JSON event
- `void netpieSetJson(const char* key, const char* json);` — สำหรับ `nodes` (`[1,2]`) และ `last_event`
- `bool lineTextForEvent(const char* ev, time_t ts, const char* d, time_t now, uint8_t node, char* out, int outLen);` — ข้อความมี "(กำไล #n)" ตามตารางใน netpie-schema.md เมื่อ node > 0
- `publishEvent` เขียน `last_event` (payload เต็ม) ลง shadow ด้วย ตาม schema

- [ ] **Step 1:** แก้ตามข้างบน และแก้ comment ใน netpie.cpp จาก `spb_box` เป็น `pillbox`
- [ ] **Step 2:** compile gateway ผ่าน
- [ ] **Step 3:** Commit `feat(gateway): tag NETPIE events and LINE text with node id`

### Task 3: config + indicator + button + sensors (ฮาร์ดแวร์พื้นฐาน)

**Files:** `config.h`, `indicator.h/.cpp`, `button.h/.cpp`, `sensors.h/.cpp`, `gateway.ino` (ชั่วคราว: พิมพ์ค่าเซนเซอร์ทุก 1 วิ + ปุ่มสลับโหมด LED)

**config.h ค่าที่กำหนด:**
- ขาตาม Global Constraints, `REED_OPEN_LEVEL HIGH` (ถ้าเทสแล้วกลับด้านให้แก้ค่านี้ที่เดียว)
- `LID_OPEN_HOLD_MS 2000`, `PERSON_NEAR_CM 60`, `SONAR_INTERVAL_MS 200`, `DHT_INTERVAL_MS 5000`
- `STORAGE_TEMP_MAX 30.0`, `STORAGE_HUM_MAX 75.0`, `STORAGE_WARN_HOLD_MS (10 นาที)`
- `PILL_ESCALATE_MS (10 นาที)`, `PILL_MISSED_MS (30 นาที)`, `FALL_FAST_MS (2 นาที)`, `FALL_SLOW_BEEP_MS 20000`, `FALL_UNANSWERED_MS (10 นาที)`, ทุกตัวหาร 60 เมื่อ `DEMO_FAST 1`
- `SHADOW_SENSOR_MS 30000` (ส่ง temp/hum/lid/person ขึ้น shadow)

**Interfaces (Produces):**
- `indicator.h`: `enum LedMode { LED_OFF, LED_GREEN, LED_YELLOW, LED_BLUE, LED_BLUE_BLINK, LED_RED_BLINK };` `enum BeepMode { BEEP_OFF, BEEP_PILL, BEEP_PILL_FAST, BEEP_FALL_FAST, BEEP_FALL_SLOW };` `void indicatorBegin(); void indicatorSet(LedMode led, BeepMode beep); void indicatorLoop();`
  รูปแบบเสียง: PILL = บี๊บ 150 ms ทุก 5 วิ, PILL_FAST = บี๊บ 150 ms ทุก 1 วิ, FALL_FAST = ติด/ดับ 200/200 ms, FALL_SLOW = บี๊บ 300 ms ทุก `FALL_SLOW_BEEP_MS`. กะพริบ LED 500/500 ms
- `button.h`: `void buttonBegin(); bool buttonPressed();` — debounce 50 ms, คืน true ครั้งเดียวต่อการกด
- `sensors.h`: `void sensorsBegin(); void sensorsLoop(); bool lidOpen(); bool lidOpenHeld();` (เปิดค้าง ≥ `LID_OPEN_HOLD_MS`) `bool personNear();` (no echo = false) `float boxTemp(); float boxHum();` (NAN = อ่านไม่ได้) `bool storageBad();` (ร้อน/ชื้นเกินต่อเนื่อง ≥ `STORAGE_WARN_HOLD_MS`, NaN ไม่นับว่าเกิน)

- [ ] **Step 1:** เขียนไฟล์ทั้งหมด + gateway.ino ชั่วคราว
- [ ] **Step 2:** compile ผ่าน
- [ ] **Step 3:** เทสจริง: เปิด/ปิดฝา → `lid=1` แล้ว `lidHeld=1` หลัง 2 วิ, เอามือบังหน้า HC-SR04 → `person=1`, DHT ได้ค่าไม่ใช่ -1, กดปุ่ม → LED เปลี่ยนโหมดทีละแบบ + ได้ยินเสียงแต่ละแบบ (Review Focus 5: ถอดสาย DHT → `temp=nan` และ `storageBad=0`)
- [ ] **Step 4:** Commit `feat(gateway): add sensors, indicator and button modules`

### Task 4: node_monitor (ตาราง node + STATUS + แบตต่ำ + ขาดการติดต่อ)

**Files:** `node_monitor.h/.cpp`

**Interfaces:**
- Consumes: `netpieSet`, `netpieSetJson`, `netpieEvent(ev, d, ts, node)`, `netpieNow()` (Task 2)
- Produces: `void nodeMonitorBegin(); void nodeOnPacket(uint8_t node, uint8_t type, const uint8_t* data, int len); void nodeMonitorLoop();`

พฤติกรรม (ตาม state-diagram.md ข้อ 4): ตาราง `MAX_NODES` ช่อง, node ใหม่ลงทะเบียนเอง (เต็ม → log แล้วทิ้ง) และอัปเดต `nodes`; packet ทุกชนิดอัปเดต `node_last_seen_<n>` + `node_online_<n>=true`; STATUS → `hr_<n>`, `spo2_<n>`, `batt_<n>`; `low_battery` ครั้งเดียวเมื่อ < `LOW_BATT_PERCENT` แล้ว re-arm เมื่อ ≥; POWER_OFF → `node_power_off` และไม่แจ้ง offline; เกิน `NODE_TIMEOUT` → `node_offline` (`d.last_seen`) ครั้งเดียวจนกว่าจะเห็นอีก

- [ ] **Step 1:** เขียน
- [ ] **Step 2:** compile ผ่าน
- [ ] **Step 3:** เทสจริงกับกำไล/hw_band_espnow: ใน NETPIE shadow เห็น `hr_1`, `nodes=[1]`; Serial `b` → LINE แบตต่ำ 1 ครั้ง กด `b` ซ้ำไม่ส่งซ้ำ; ถอดไฟกำไล 3 นาที → LINE ขาดการติดต่อ 1 ครั้ง
- [ ] **Step 4:** Commit `feat(gateway): track wristband nodes, battery and timeouts`

### Task 5: fall_alarm

**Files:** `fall_alarm.h/.cpp`

**Interfaces (Produces):** `void fallAlarmBegin(); void fallOnPacket(uint8_t node, uint8_t type, const uint8_t* data, int len, bool isDup); void fallAlarmLoop(); bool fallActive(); uint8_t fallNode(); BeepMode fallBeep(); void fallClear(const char* by);`

พฤติกรรม (state-diagram.md ข้อ 1): FALL_CONFIRMED/SOS ที่ไม่ใช่ dup → event `fall_confirmed` / `sos` (ts = `netpieNow() - age_ms/1000` ถ้ามีเวลา) และเข้า alarm (ถ้า alarm อยู่แล้วไม่รีเซ็ตเวลา); FALL_CANCELLED → event `fall_cancelled` อย่างเดียว; `fallBeep()` = FAST ช่วง `FALL_FAST_MS` แรก แล้ว SLOW; ครบ `FALL_UNANSWERED_MS` → `fall_unanswered` ครั้งเดียว; `fallClear(by)` → `fall_acknowledged` (`by`, `after_s`, node) + `linkSetAckCmd(CMD_FALL_CLEARED)`; shadow `alarm_node`

- [ ] **Step 1:** เขียน
- [ ] **Step 2:** compile ผ่าน
- [ ] **Step 3:** เทสจริง: Serial `f` → แดงกะพริบ + buzzer ถี่ + LINE ล้ม; กำไลส่ง FALL_CONFIRMED ซ้ำ seq เดิม → ไม่มี LINE เพิ่ม (Review Focus 3); กดปุ่มกล่อง → หยุด + LINE "มีคนรับเหตุแล้ว"; ปุ่ม clear บน dashboard ก็หยุดได้; `DEMO_FAST 1` → ครบ 10 วิได้ LINE เตือนซ้ำ
- [ ] **Step 4:** Commit `feat(gateway): add fall/SOS alarm state machine`

### Task 6: pill_schedule

**Files:** `pill_schedule.h/.cpp`

**Interfaces (Produces):** `enum PillState { PILL_IDLE, PILL_REMIND, PILL_ESCALATE, PILL_MUTED };` `void pillBegin(); void pillSetSchedule(char slots[][6], int n); void pillLoop(bool fallActive); void pillMute(); void pillStartNow(); PillState pillState();`

พฤติกรรม (state-diagram.md ข้อ 2): เทียบ "HH:MM" ปัจจุบัน (localtime จาก `netpieNow()`, ข้ามถ้า 0 — Review Focus 1) กับตาราง, จำ `lastFiredSlot` + วันที่ กันเตือนซ้ำ (Review Focus 2); เข้า REMIND → `linkSetAckCmd(CMD_PILL_REMIND)` ถ้าไม่ได้อยู่ใน fall alarm; ไม่เจอคน `PILL_ESCALATE_MS` → ESCALATE (+ส่ง cmd อีกครั้ง); `lidOpenHeld() && personNear()` → `pill_taken` (`slot`, `late_min`) + `last_pill_taken`; ครบ `PILL_MISSED_MS` → `pill_missed`; `pillMute()` → MUTED; shadow `next_pill` อัปเดตเมื่อตาราง/มื้อเปลี่ยน

- [ ] **Step 1:** เขียน
- [ ] **Step 2:** compile ผ่าน
- [ ] **Step 3:** เทสจริง (`DEMO_FAST 1`): ตั้ง schedule บน dashboard เป็นนาทีถัดไป → ฟ้ากะพริบ + กำไลสั่นใน ≤60 วิ; ไม่ยืนหน้ากล่อง 10 วิ → buzzer ถี่; เปิดฝาค้าง 2 วิ + ยืนหน้ากล่อง → หยุด + `pill_taken`; อีกรอบไม่ทำอะไร 30 วิ → LINE ลืมกินยา; รีบูตแบบไม่มีเน็ต → ไม่มีการเตือนจนกว่าจะได้ NTP
- [ ] **Step 4:** Commit `feat(gateway): add medication schedule reminders`

### Task 7: gateway.ino รวมทุกอย่าง

**Files:** `gateway.ino`, `3-gateway-cloud/README.md` (ตาราง Serial test keys + ตัวอย่างใช้งานใหม่)

**Interfaces:** Consumes ทุกอย่างจาก Task 2–6

- `setup()`: `Serial.begin(115200)` → `indicatorBegin/buttonBegin/sensorsBegin` → `netpieBegin()` → `linkBegin(nullptr)` + `linkOnReceive(onPacket)` → `nodeMonitorBegin/fallAlarmBegin/pillBegin`
- `onPacket()`: `nodeOnPacket(...)` ทุก packet แล้ว `fallOnPacket(...)`
- `loop()`: `linkLoop`, ทุก `xxxLoop`; ปุ่ม: ถ้า `fallActive()` → `fallClear("button")` ไม่งั้นถ้าเตือนยาอยู่ → `pillMute()`; `netpieTakeClearAlarm()` → `fallClear("dashboard")`; `netpieSchedule(..., &changed)` → `pillSetSchedule`; เลือก LED/beep/`box_state` ตามตารางลำดับความสำคัญใน state-diagram.md ข้อ 3 (เน็ตหลุด = `!netpieOnline()` → เหลือง); storage_warn event ครั้งเดียวต่อรอบที่ `storageBad()` เปลี่ยนเป็น true; shadow `temp/hum/lid_open/person/queue` ทุก `SHADOW_SENSOR_MS`; Serial test keys ตามตารางข้างบน

- [ ] **Step 1:** เขียน
- [ ] **Step 2:** compile ผ่าน, ดู % flash/RAM
- [ ] **Step 3:** เทส end-to-end: ปกติ → เขียว; ปิด hotspot → เหลือง และ Serial `f` ยัง alarm ได้, เปิด hotspot คืน → LINE ส่งตามพร้อม "(ส่งช้า ...)" (Review Focus 4); เตือนยาแล้ว `f` → แดงชนะ, กดปุ่ม 1 ครั้งเคลียร์ล้ม กลับมาฟ้า, กดอีกครั้ง mute
- [ ] **Step 4:** Commit `feat(gateway): wire all modules in gateway.ino`
