# การจัดเก็บหลักฐาน

สร้างโฟลเดอร์ต่อรอบ เช่น `2026-11-21-bench-01/` และคัดลอก [RUN_TEMPLATE.md](RUN_TEMPLATE.md) ไปเป็น `run.md` ภายใน เก็บ build/commit และสิ่งที่จำลองก่อนเริ่ม

```text
<Run-ID>/
  run.md
  mqtt-raw.log          ต้นฉบับจากเครื่องมือบันทึก
  mqtt.jsonl            ข้อมูลที่จัดรูปแบบเพื่อให้ checker อ่านได้
  queued-events.json    event ที่บันทึกเข้าคิวจาก gateway เป็นแหล่งอิสระ
  sensor-serial.log
  gateway-serial.log
  feed-raw.json         ผล query พร้อมช่วงเวลา/จำนวนจุดใน run.md
  metrics-raw.jsonl     ค่าที่วัดจริงทีละตัวอย่าง ไม่ใช่เฉพาะค่าเฉลี่ย
  screenshots/
  checker-output.txt
```

รายการนี้เป็นรูปแบบที่แนะนำ ยังไม่ได้สร้างหรืออ้างว่ามีหลักฐานดังกล่าวจริง เก็บภาพ/คลิปโดยตั้งชื่อ Test ID และเวลา ใส่ลิงก์ relative ใน `test-results.md` และ `metrics.md` ข้อมูลที่เปลี่ยนหน่วยหรือจัดรูปแบบต้องเก็บต้นฉบับไว้คู่กัน และบันทึกวิธีแปลง

## MQTT log สำหรับ checker

UTF-8 JSON Lines หนึ่ง JSON object ต่อบรรทัด ใช้หนึ่ง gateway และหนึ่ง boot/session ต่อไฟล์:

```json
{"received_at":"2026-11-21T08:00:00+07:00","topic":"@shadow/data/update","payload":{"data":{"hr":78,"spo2":97,"batt":64}}}
{"received_at":"2026-11-21T08:00:01+07:00","topic":"@msg/spb/event","payload":{"ev":"sos","ts":1795222800,"id":1,"d":{"hr":92,"spo2":97}}}
```

นี่คือตัวอย่าง MOCK ไม่ใช่ผลจริง `received_at` เป็นเวลาเครื่องบันทึกรับข้อความพร้อม timezone; `payload.ts` เป็นเวลาที่เกิดเหตุจริงจาก gateway หน่วย seconds ไม่แทนกัน `payload` เป็น object หรือ JSON string ได้

checker รับเฉพาะ `@msg/spb/event` และ `@shadow/data/update` เก็บ topic อื่นใน raw log และคัดเฉพาะสองชนิดนี้ลงไฟล์ที่ใช้ตรวจ ข้อมูล Shadow update อาจต้องเก็บจาก MQTT publish log ฝั่ง gateway เพราะการ subscribe ฝั่ง cloud ไม่จำเป็นต้องเห็น topic ที่ส่งเข้าระบบโดยตรง ห้ามเปลี่ยน response topic ให้ดูเหมือนเป็น publish ดิบโดยไม่บันทึกการแปลง

## Event ที่คาดว่าจะส่งย้อนหลัง

`queued-events.json` ใช้ JSON array ของ `ev/ts/id/d` ที่ gateway บันทึกก่อนส่ง จาก serial/debug queue export ที่คงต้นฉบับไว้ ตัวอย่างรูปแบบ:

```json
[{"ev":"sos","ts":1795222800,"id":1,"d":{"hr":92,"spo2":97}}]
```

รายการคาดหมายต้องสร้างจากฝั่งต้นทางก่อนตรวจ log ปลายทาง สำหรับ T06/T13 ให้บันทึกเวลาตัด/คืนเครือข่าย คิวต้น/ปลาย และจำนวนเหตุที่ต้องได้ ห้ามใช้ mock ในไฟล์นี้แล้วอ้างว่าเป็นหลักฐาน HARDWARE

## ข้อมูลที่ใช้ประกอบตัวชี้วัด

- Latency: sample ID, จุดเริ่ม/สิ้นสุด, เวลา/หน่วย, วิธี sync และ error ของนาฬิกา
- Packet delivery: source MAC, boot/session, seq, type, เวลา TX/RX, จำนวน retry และ channel
- AI: sample/session ID, ground truth, prediction, model version; แยกข้อมูลฝึก/ทดสอบ
- Power: mode, กระแส/แรงดัน, เครื่องมือ, sampling interval, เวลาเริ่ม/จบและความจุแบต
- Field test: start/end, sample interval, outage/unknown interval, recovery และผู้บันทึก

เก็บเฉพาะข้อมูลที่ใช้ตรวจงาน หลีกเลี่ยง token, secret, Wi-Fi password และข้อมูลผู้ทดลองที่ระบุตัวตนใน repo ตามข้อกำหนดการส่งงาน
