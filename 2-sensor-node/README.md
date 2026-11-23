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
| `packets.h` | ส่ง packet ผ่าน `espnow_link` + ใส่ `node_id` ให้เมื่อ `protocol.h` มี field นี้ |
| `sensor_node.ino` | setup/loop + state machine ของกำไล |

## การต่อสาย (XIAO ESP32-C3)

| อุปกรณ์ | ขา | หมายเหตุ |
|---|---|---|
| MPU6050 + MAX30102 | SDA = D4 (GPIO6), SCL = D5 (GPIO7) | บัส I2C เดียวกัน, address 0x68 / 0x57 |
| ปุ่ม | D1 (GPIO3) → GND | pull-up ภายใน, ใช้ปลุก deep sleep ได้ (C3 ปลุกได้เฉพาะ GPIO0–5) |
| มอเตอร์สั่น | D10 (GPIO10) → base/gate ทรานซิสเตอร์ | ใส่ diode กลับขั้วคร่อมมอเตอร์ + R pull-down ที่ base |
| แบต | D0 (GPIO2) ← divider 1:2 จาก BAT+ | เช่น 220k + 220k, ยังไม่ต่อให้ตั้ง `BATT_ENABLED 0` |

ไม่ต้องลง library เพิ่ม (MAX30102 อ่าน register ตรงๆ) ใช้แค่ `protocol`, `espnow_link` และ `chawissdev-project-1_inferencing` ใน `libraries/`
ก่อนอัปโหลดแก้ `GATEWAY_MAC` ใน `config.h` เป็น MAC ของกล่องยา
| `ml/` | dataset และโน้ตการเทรน |
| `pm/` | timeline, บันทึกประชุม, เรื่องที่รอคำตอบอาจารย์ (งาน PM) |

> ESP32-C3 ปลุกจาก deep sleep ได้เฉพาะบาง GPIO เช็คก่อนเลือกขาปุ่ม

ไฟล์ทั้งหมดอยู่ใน `sensor_node/` (folder เดียวกับ .ino) — Arduino IDE จะเปิดเป็น tab ให้เอง

## Edge Impulse
Export เป็น **Arduino library (.zip)** → Sketch → Include Library → Add .ZIP Library
lib จะไปอยู่ใน `libraries/` ของ repo ถ้าต้องการให้ทีมใช้ร่วม (เช่นกรณีย้าย AI ไป gateway)
ให้เพิ่มบรรทัด `!libraries/<ชื่อ lib>/` ใน `.gitignore`
