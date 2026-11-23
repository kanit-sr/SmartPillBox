# การทดสอบโค้ดและวิธีรัน

## 1. ทดสอบโค้ดอยู่ส่วนไหน?

ชุดทดสอบอยู่ใน `4-dashboard-testing/test/` แต่โค้ดที่ถูกทดสอบเป็นของ sensor node, gateway และ libraries ตามตารางด้านล่าง การดูข้อความบน dashboard อย่างเดียวไม่ยืนยัน logic ภายในทุกส่วน

| ระดับ | วิธี / ตำแหน่ง | สถานะที่ตรวจพบ |
|---|---|---|
| Build check | Arduino IDE Verify สเก็ตช์ของทั้งสองบอร์ดตาม README หลัก | ยังไม่รัน; compile ผ่านอย่างเดียวไม่พิสูจน์พฤติกรรม |
| Firmware unit test | เรียกฟังก์ชันจริงพร้อม fake clock/sensor/transport แล้วเทียบผล | ยังไม่มี เนื่องจากโมดูลเป็น TODO และยังไม่มี interface ที่เรียกได้ |
| Data contract check | `tools/validate_payloads.py` ตรวจ mock หรือ output log จริง | รันบนคอมได้ ไม่เรียก ESP32 firmware |
| Checker regression test | `automated/test_validate_payloads.py` | ทดสอบตัว checker ด้วยข้อมูลถูก/ผิดและ CLI exit code |
| Integration / end-to-end | T01–T18 ใน `test-plan.md` | ต้องรัน firmware จริงหรือ test build ที่ระบุสิ่งจำลอง และกรอกผลจริง |

## 2. Test ID เชื่อมกับไฟล์โค้ด

| ไฟล์โค้ดที่เกี่ยวข้อง (เทียบจาก root repo) | สิ่งที่ต้องพิสูจน์ | Test ID |
|---|---|---|
| `2-sensor-node/sensor_node/fall_detector.*`, `button.*`, `motor.*` | นับถอยหลัง ยกเลิก SOS ปิดเครื่อง และการสั่น | T01–T03, T09, T17 |
| `2-sensor-node/sensor_node/imu.*`, `pulse.*`, `battery.*` | อ่าน sensor, ค่าอ่านไม่ได้, แบต | T07, T11 และ metrics AI/แบต |
| `3-gateway-cloud/gateway/pill_schedule.*`, `sensors.*` | หลายมื้อ เงื่อนไขพบคน/เปิดฝา และเวลา 10/30 นาที | T04–T05, T10–T12, T15–T17 |
| `3-gateway-cloud/gateway/fall_alarm.*`, `indicator.*`, `button.*` | alarm/เคลียร์, mute ยา, ลำดับความสำคัญ | T01, T03, T12, T16–T17 |
| `3-gateway-cloud/gateway/node_monitor.*` | last_seen ทุก packet, offline, power-off, เตือนแบตครั้งเดียว | T07–T09, T14 |
| `3-gateway-cloud/gateway/netpie.*`, `line_notify.*` | Shadow, event, schedule, คำสั่ง, LINE และ reconnect | T01–T15, T18 |
| `libraries/protocol/`, `libraries/espnow_link/` | format, ACK/seq, retry, channel recovery | T01–T03, T06, T09, T14, T17 |
| `libraries/store_forward/` | คิวระหว่าง cloud ขาด, ส่งซ้ำและคงเวลาจริง | T06, T12–T14 |
| `sensor_node.ino`, `gateway.ino` ในโฟลเดอร์ของแต่ละบอร์ด | การเรียกโมดูลร่วมกันใน setup/loop | T01–T18 |

เมื่อเจ้าของ firmware เพิ่ม logic แล้ว ให้เขียน unit test ที่เรียก implementation นั้นโดยตรง เช่น ส่ง fake time คร่อม 10/30 นาทีและตรวจ event หรือทำให้ transport ส่งไม่ผ่านแล้วตรวจคิว/การ retry ห้ามสร้าง logic จำลองอีกชุดแล้วเรียกว่าทดสอบ firmware จริง

## 3. คำสั่งที่รันได้ทันที

รันจาก root repository ใช้ Python 3.10+ ไม่มี dependency เพิ่ม:

```powershell
python -B 4-dashboard-testing/test/tools/validate_payloads.py --mock 4-dashboard-testing/dashboard/mock-data.json
python -B -m unittest discover -s 4-dashboard-testing/test/automated -p "test_*.py" -v
```

ตรวจข้อมูลจริงหลังบันทึก log ตาม [evidence/README.md](evidence/README.md) โดยเปลี่ยน RUN-ID เป็นรอบที่มีไฟล์จริง:

```powershell
python -B 4-dashboard-testing/test/tools/validate_payloads.py --log 4-dashboard-testing/test/evidence/RUN-ID/mqtt.jsonl
python -B 4-dashboard-testing/test/tools/validate_payloads.py --log 4-dashboard-testing/test/evidence/RUN-ID/mqtt.jsonl --expected-events 4-dashboard-testing/test/evidence/RUN-ID/queued-events.json
```

`queued-events.json` เป็น array ของ event ที่ได้จากการบันทึกเข้าคิวฝั่ง gateway อย่างอิสระ ไม่คัดลอกจาก log ปลายทางที่กำลังตรวจ เพราะจะตรวจเหตุที่หายไม่ได้ ใช้คิวและ received log จาก gateway/boot เดียวกันและช่วงเวลาที่ครอบคลุมการ flush

Exit code: `0` = รูปแบบข้อมูลและเงื่อนไขที่ร้องขอผ่าน, `1` = ข้อมูลผิด/เหตุที่คาดหาย, `2` = ใช้คำสั่งผิดหรืออ่านไฟล์ไม่ได้ ไม่ใช้ `0` เป็นผล PASS ของ T01–T18 โดยอัตโนมัติ

## 4. สิ่งที่ checker ตรวจและข้อจำกัด

- Shadow: ชื่อ field, ชนิดข้อมูล, HH:MM, ช่วงเปอร์เซ็นต์, box_state; mock ต้องมีครบ ส่วน log ยอมให้ partial update
- Event: `ev/ts/id/d` และ field ของ event ครบทั้ง 10 ชนิด; ตรวจ boolean ที่ผิดมาในช่องตัวเลข, JSON เสีย, key ซ้ำ และค่าที่ไม่ใช่ตัวเลข finite
- Retry: ID เดิมและ payload เดิมนับ duplicate แยก; ID เดิมแต่ `ev/ts/d` เปลี่ยนถือว่าขัดแย้ง ยอมให้ backlog มาถึงไม่เรียงตามเวลา
- การส่งครบ: ตรวจได้เฉพาะเมื่อส่ง `--expected-events`; log ปลายทางลำพังพิสูจน์ว่าไม่มีเหตุหายไม่ได้
- ใช้ log ของหนึ่ง gateway หนึ่ง boot/session ต่อไฟล์ ถ้า ID reset/wrap ให้แยกไฟล์และเก็บจุดเชื่อม session ก่อนวิเคราะห์ต่อ ไม่ได้แก้ protocol ของทีมให้มี node ID
- ไม่ตรวจว่าค่าตรง sensor จริง, UI render ถูก, เวลา HH:MM ตรงนาฬิกา, firmware สร้าง event ในเงื่อนไขถูกต้อง, LINE ถึงจริง หรือ alarm หยุดจริง
- ไม่คำนวณ packet delivery rate จาก MQTT event; ต้องใช้ log TX/RX ของ ESP-NOW ตาม metrics
- ยังไม่ตัดสินนโยบายตารางว่าง/ซ้ำ และ `next_pill` ที่ยังไม่มีมื้อถัดไป ซึ่งต้องตกลงใน T15

## 5. วิธีเก็บผลการรันทดสอบโค้ด

เก็บคำสั่ง, stdout/stderr, exit code, Python/board version และ commit ของ source ที่ใช้ในโฟลเดอร์รอบทดสอบ แยกหัวข้อ “checker บนคอม”, “build”, “firmware unit test” และ “hardware end-to-end” ชัดเจน

รอบตรวจเครื่องมือบนคอมวันที่ 2026-11-21 ใช้ Python 3.13.7 ดู [บันทึกรอบและ hash ของไฟล์ที่ตรวจ](evidence/2026-11-21-checker-local/run.md) และ [ผลคำสั่งจริง](evidence/2026-11-21-checker-local/checker-output.txt)

| รายการ | ผลการตรวจล่าสุด |
|---|---|
| ตรวจ fixture ด้วย checker | ผ่าน: 1 full shadow, 10 event ไม่ซ้ำ; exit 0 |
| Automated regression ของ checker | ผ่าน 18 tests; exit 0 |
| Build firmware กำไล / กล่อง | NOT RUN |
| Firmware unit tests | ยังไม่มี implementation ที่นำมาทดสอบได้ |
| T01–T18 / D01–D06 บนอุปกรณ์และ NETPIE | NOT RUN |
