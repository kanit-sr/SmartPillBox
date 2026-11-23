# Run record template

แบบฟอร์มเท่านั้น ยังไม่มีผลทดสอบ ห้ามใช้ค่าตัวอย่างเป็นผลจริง

| รายการ | ค่า |
|---|---|
| Run ID / ผู้ทดสอบ | รอกรอก |
| เริ่ม–สิ้นสุด / timezone | รอกรอก |
| Mode / จุดที่จำลอง | HARDWARE หรือ MOCK; รอกรอก |
| Sensor firmware / commit / boot ID | รอกรอก |
| Gateway firmware / commit / boot ID | รอกรอก |
| Dashboard export / revision | รอกรอก |
| Model / dataset version | รอกรอก |
| Board, sensor, แบต, เครื่องมือวัด | รอกรอก |
| NETPIE alias / เครือข่าย / channel / ระยะ | รอกรอก; ไม่ใส่ secret |
| Test IDs / เกณฑ์ที่ตกลง | รอกรอก |
| การ sync เวลา / ความคลาดเคลื่อน / ค่าที่เร่งใน test build | รอกรอก |

| เวลา | Test ID / Sample ID | สิ่งที่ทำ | ผลจริง / ค่าที่วัด | หลักฐาน |
|---|---|---|---|---|
| รอกรอก | — | — | — | — |

## คำสั่งที่รัน

บันทึกคำสั่งจริง, exit code และลิงก์ stdout/stderr ระบุว่าทดสอบ checker, build หรือ firmware การผ่าน checker ไม่ยืนยัน firmware

## การแปลงข้อมูล

ระบุ raw file → ไฟล์ที่จัดรูปแบบ, วิธีแปลง, หน่วยเวลา และจำนวนรายการก่อน/หลัง ถ้าไม่ได้แปลงให้ระบุว่าไม่ได้แปลง

## ผลและข้อจำกัด

- PASS / FAIL / BLOCKED / NOT RUN ของแต่ละ Test ID: รอกรอก
- ข้อมูลที่ขาด / ช่วงเวลาไม่ทราบสถานะ: รอกรอก
- Issue ID / ผู้รับผิดชอบ / ทดสอบซ้ำ: รอกรอก
- ลิงก์ metrics, ภาพ และคลิป: รอกรอก
