# ข้อกำหนดกับหลักฐาน

อ้างอิงเอกสารโจทย์ของผู้ใช้: *Embedded System (CEDT) - Final Project 2026 (1).pdf* หน้า 2–8 ตารางนี้ใช้วางแผนและตรวจความครบ ไม่ใช่ผลรับรองคะแนนหรือคำสั่งให้แก้ firmware นอกส่วนที่รับผิดชอบ

## ส่วน Dashboard + Test Lead

| เกณฑ์ | หลักฐานที่ต้องเตรียม | แหล่งในโฟลเดอร์นี้ | สถานะ |
|---|---|---|---|
| M5 Raw sensor data บน cloud พร้อม dashboard | ข้อมูลดิบอ่านย้อนหลังได้ ภาพหน้าจอ และการตั้งค่า | dashboard/README.md, T18/D05, evidence | ยังไม่มีหลักฐานจริง |
| M6 Notification | event ตรงกับข้อความและเวลาได้รับ LINE | T01, T03, T05–T08, T10 | NOT RUN |
| UX 3 คะแนน | การจัดหน้า อ่านสถานะง่าย คำแจ้งเตือนชัด และ feedback | D01–D06, field-test.md | NOT RUN |
| Test plan/procedure 3 คะแนน | ขั้นตอนและผล normal, edge, failure | T01–T18, test-results.md | มีแผน ยังไม่รัน |
| Quantitative metrics 4 คะแนน | อย่างน้อย 3 ประเภทวัดจริง วิธีวัด และการวัดซ้ำพร้อม variation | metrics.md | ยังไม่วัด |
| Analysis/comparison 3 คะแนน | เทียบเป้าหมายและระบบใกล้เคียงด้วยผลวัดจริง | analysis.md | ยังไม่มีผลวิเคราะห์ |
| Field Testing ระดับ 2/3 คะแนน | ≥24 ชั่วโมงพร้อมข้อมูลต่อเนื่อง / ผู้ใช้จริงหรือสถานที่จริง | field-test.md | NOT RUN |
| Robustness 3 คะแนน | กู้คืนได้เองหลังไฟหรือ Wi-Fi หายภายใต้เงื่อนไขที่ทดสอบ | T11–T14, recovery log | NOT RUN |
| Demo / รายงาน / contribution | ขั้นตอนเดโม ผลทดสอบ บทวิเคราะห์ commit ของผู้ทำ | demo-checklist.md, evidence, Git | รอหลักฐานจริง |

## รายการที่ต้องประสานทีม

| เรื่อง | สิ่งที่ตรวจพบ / สิ่งที่ต้องตัดสินใจ | ผู้รับผิดชอบร่วม |
|---|---|---|
| M1 sensor อย่างน้อย 5 ประเภท รวม ≥2 ต่อ node/gateway | ตาม BOM ใน system-flow.md มี MPU6050, MAX30102, DHT11, ultrasonic และ reed; โจทย์ไม่นับ switch เป็น sensor จึงมีเพียง 4 ประเภทที่นับได้ตามรายการปัจจุบัน ให้ตรวจ BOM ล่าสุดและเพิ่ม/ปรับให้ครบ | Architect + เจ้าของบอร์ด |
| M2 actuator อย่างน้อย 1 | LED/buzzer ไม่นับ และ vibration motor ที่แจ้งเตือนอย่างเดียวไม่นับตามนิยามหน้า 2; ต้องมี actuator ทำ physical action ที่เข้าเกณฑ์ พร้อมโค้ดควบคุมและทดสอบ | Architect + เจ้าของบอร์ด |
| M4 AI closed loop | ใช้ผลโมเดลจริงในการตัดสินใจ ไม่ใช้ threshold-only แล้วเรียกว่า AI; เก็บ model version และผลทดสอบ | Sensor + Test lead |
| M7 safety ตามอุปกรณ์จริง | ตรวจวงจรป้องกันแบต; หากเพิ่มชิ้นส่วนเคลื่อนที่ต้องมีวิธีหยุด; เงื่อนไขไฟบ้าน/ของเหลวใช้เมื่ออุปกรณ์ที่เลือกมีส่วนดังกล่าว | เจ้าของ hardware |
| M8 / contribution | Git เข้าถึงผ่านอินเทอร์เน็ตได้ สมาชิกมี commit ของตนและอธิบายงานได้ | ทุกคน |
| Sensor error / ตารางยา / reboot policy | ตกลงสิ่งที่ระบุใน T10–T12 และ T15 ก่อนตัดสิน PASS | Architect + Gateway + Sensor |
| Event history / adherence | เลือกที่เก็บ event ถาวรและนิยามมื้อตรงเวลา; MQTT สดกับ Shadow ล่าสุดยังไม่พอ | Gateway + Dashboard |

## เกณฑ์เพิ่มคะแนนที่ทดสอบเมื่อทีมเลือกทำ

- E3: ต้องวัด current ของ sleep/awake และประมาณแบต ไม่ใช่เพียงเรียก deep sleep
- E4: พิสูจน์ทั้ง ACK/retry และ buffering เมื่อ internet/cloud ขาด ใช้ T06/T13/T14/T17
- E5: ถ้าเลือก ให้เจ้าของ gateway แสดง TLS/authentication และตรวจว่าไม่มี secrets ใน source
- E7: ถ้าเลือก ให้มีผล inference ของโมเดลบน MCU จริง พร้อม RAM/latency/model version
- Scalability ระดับ 2 คะแนน: ต้องมี 1 physical node + อย่างน้อย 2 simulated nodes ทำงานพร้อมกัน และ gateway/dashboard แยกได้ถูก ปัจจุบัน message-format ระบุกำไลเดียวและไม่มี node ID จึงยังไม่อ้างว่าครบเกณฑ์นี้

ไม่จำเป็นต้องเพิ่มทุก E-item; ทีมเลือกได้ตาม proposal และสาธิต/อธิบายจริง คะแนนรวมหมวด Embedded Engineering Depth สูงสุด 10 ตามโจทย์
