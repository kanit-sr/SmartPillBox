# 2. Sensor Node (กำไล) + PM — ชวิศ

XIAO ESP32-C3 + MPU6050 + MAX30102 + มอเตอร์สั่น + ปุ่ม

| ไฟล์ | หน้าที่ |
|---|---|
| `config.h` | ขา GPIO, ค่าตั้งต่างๆ |
| `imu.*` | MPU6050 50 Hz + ring buffer 100 ช่อง |
| `pulse.*` | MAX30102 ชีพจร/SpO2 |
| `battery.*` | แรงดันแบต → % |
| `button.*` | กดสั้น/ค้าง 3 วิ/ค้าง 10 วิ/ปลุกจาก deep sleep |
| `motor.*` | รูปแบบการสั่น |
| `fall_detector.*` | เรียกโมเดล Edge Impulse |
| `sensor_node.ino` | setup/loop + state machine ของกำไล |
| `ml/` | dataset และโน้ตการเทรน |
| `pm/` | timeline, บันทึกประชุม, เรื่องที่รอคำตอบอาจารย์ (งาน PM) |

> ESP32-C3 ปลุกจาก deep sleep ได้เฉพาะบาง GPIO เช็คก่อนเลือกขาปุ่ม

ไฟล์ทั้งหมดอยู่ใน `sensor_node/` (folder เดียวกับ .ino) — Arduino IDE จะเปิดเป็น tab ให้เอง

## Edge Impulse
Export เป็น **Arduino library (.zip)** → Sketch → Include Library → Add .ZIP Library
lib จะไปอยู่ใน `libraries/` ของ repo ถ้าต้องการให้ทีมใช้ร่วม (เช่นกรณีย้าย AI ไป gateway)
ให้เพิ่มบรรทัด `!libraries/<ชื่อ lib>/` ใน `.gitignore`
