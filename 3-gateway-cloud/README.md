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
| `netpie.*` | MQTT, shadow, NTP, คำสั่งเคลียร์จาก dashboard |
| `line_notify.*` | LINE Messaging API |
| `gateway.ino` | setup/loop + state machine ของกล่องยา |

ไฟล์ทั้งหมดอยู่ใน `gateway/` (folder เดียวกับ .ino) — Arduino IDE จะเปิดเป็น tab ให้เอง
