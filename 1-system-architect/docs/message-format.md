# Message Format (ESP-NOW)

> spec ที่ System Architect กำหนด ทุกคนเขียนโค้ดตามนี้ ส่วน NETPIE และ LINE อยู่ใน [netpie-schema.md](netpie-schema.md)

```
กำไล ──(1) ESP-NOW──► กล่องยา ──(3) MQTT──► NETPIE ──► Dashboard
     ◄──(2) ACK──────        ◄──(4) MQTT───
                              └──(5) HTTPS──► LINE
```

## 1. กำไล → กล่องยา (ESP-NOW)

### Header ร่วม (ทุก packet, 4 bytes)

| field | ชนิด | ความหมาย |
|---|---|---|
| `ver` | uint8 | เวอร์ชัน protocol (เริ่ม 1) กล่องทิ้ง packet ที่เวอร์ชันไม่ตรง |
| `type` | uint8 | ชนิด packet (ตารางล่าง) |
| `seq` | uint16 | เลขลำดับของผู้ส่ง เพิ่มทีละ 1 ใช้จับคู่ ACK และกันการรับซ้ำ |

ไม่มี node id เพราะมีกำไลเครื่องเดียว ใช้ MAC address แยกแทน

### ชนิด packet

| type | ค่า | ส่งเมื่อ | ต้องได้ ACK? |
|---|---|---|---|
| STATUS | 1 | ทุก 60 วิ | ส่งครั้งเดียว (ACK ใช้ส่งคำสั่งเตือนยากลับ) |
| FALL_CONFIRMED | 2 | นับถอยหลัง 15 วิแล้วไม่มีใครกด | ส่งซ้ำจนกว่าจะได้ ACK |
| FALL_CANCELLED | 3 | กดยกเลิกระหว่างนับถอยหลัง | retry สูงสุด 5 ครั้ง |
| SOS | 4 | กดค้าง 3 วิ | ส่งซ้ำจนกว่าจะได้ ACK |
| POWER_OFF | 5 | กดค้าง 10 วิ | retry สูงสุด 5 ครั้ง แล้วเข้า deep sleep เลย |
| ACK | 10 | กล่อง → กำไล | — |

### Payload

| type | field | ชนิด | หมายเหตุ |
|---|---|---|---|
| STATUS | `hr` | uint8 | ชีพจร bpm, 0 = อ่านไม่ได้ |
| | `spo2` | uint8 | %, 0 = อ่านไม่ได้ |
| | `batt_pct` | uint8 | % (กำไลแปลงจากแรงดันเอง) |
| | `batt_mv` | uint16 | แรงดันดิบ ไว้ปรับเทียบ |
| | `flags` | uint8 | bit0 = MAX30102 ใช้ได้, bit1 = MPU6050 ใช้ได้, bit2 = โมเดล AI โหลดได้ |
| FALL_CONFIRMED | `hr`, `spo2` | uint8 ×2 | อ่านทันทีหลังยืนยันล้ม |
| | `conf` | uint8 | ความมั่นใจของโมเดล 0–100 |
| | `age_ms` | uint32 | ล้มมาแล้วกี่ ms (นับตอนส่งแต่ละรอบ) |
| | `batt_pct` | uint8 | |
| FALL_CANCELLED | `conf` | uint8 | ไว้ดูว่าโมเดลจับผิดที่ความมั่นใจเท่าไร |
| | `age_ms` | uint32 | |
| SOS | `hr`, `spo2`, `age_ms`, `batt_pct` | | เหมือน FALL_CONFIRMED แต่ไม่มี `conf` |
| POWER_OFF | `batt_pct` | uint8 | กล่องจะไม่แจ้ง "ขาดการติดต่อ" หลังได้ packet นี้ |

ขนาดใหญ่สุดประมาณ 12 bytes (ESP-NOW รับได้ 250)

---

## 2. กล่องยา → กำไล (ACK)

| field | ชนิด | ความหมาย |
|---|---|---|
| header | 4 bytes | `type` = ACK, `seq` = ของกล่องเอง |
| `ack_seq` | uint16 | seq ของ packet ที่ตอบ |
| `ack_type` | uint8 | type ของ packet ที่ตอบ |
| `cmd` | uint8 | bit0 = **สั่นเตือนยา** (สั่น 3 ครั้ง), bit1 = alarm ล้มถูกเคลียร์แล้ว |

**กฎ**
- กล่องตอบ ACK ทุก packet แม้เป็น packet ซ้ำ แต่ถ้า `seq` ซ้ำกับครั้งก่อน จะไม่เริ่ม alarm ใหม่
- เตือนยาไปถึงกำไลผ่าน ACK ของ STATUS จึงช้าได้สูงสุด ~60 วิ (ยอมรับได้ เพราะกล่องเตือนเองทันที)
- กำไลรอ ACK ~200 ms ถ้าไม่ได้ ACK ติดกันหลายรอบ แปลว่ากล่องอาจเปลี่ยน Wi-Fi channel → กำไลไล่ scan channel 1–13 (ทำใน `espnow_link`)

### ค่าคงที่ร่วม (`protocol.h`)

| ชื่อ | ค่า |
|---|---|
| STATUS_INTERVAL | 60 วิ |
| NODE_TIMEOUT | 180 วิ (ไม่ได้ STATUS เกินนี้ = ขาดการติดต่อ) |
| LOW_BATT_PERCENT | 20 |
| FALL_COUNTDOWN | 15 วิ |
| SOS_HOLD / POWER_OFF_HOLD | 3 วิ / 10 วิ |
| ACK_TIMEOUT | 200 ms |

---

## แต่ละคนต้องทำตามนี้

- **sky (กำไล)**: ส่ง packet ในหัวข้อกำไล → กล่องยา ผ่าน `espnow_link` เท่านั้น ห้ามประกาศ struct เอง (ใช้จาก `protocol.h`), อ่าน SpO2 ไม่ได้ให้ส่ง 0, เอา `conf` จากผลของ Edge Impulse (คูณ 100), สั่นเตือนยาเมื่อ ACK มี bit0 ของ `cmd`
- **ize (กล่องยา)**: รับ packet ในหัวข้อกำไล → กล่องยา และตอบ ACK ตามหัวข้อ ACK, อัปเดต shadow และส่ง event ตาม netpie-schema.md, รับคำสั่งตาม netpie-schema.md, ส่ง LINE ตามข้อความใน netpie-schema.md, ตารางยาเป็น "HH:MM" หลายมื้อต่อวัน, event ทุกตัวผ่าน `store_forward`
- **paksstt (dashboard)**: widget อ่านจาก field ใน shadow ในหัวข้อ Shadow ของ netpie-schema.md ชื่อตามนี้เป๊ะ, ปุ่มเคลียร์ alarm publish ตาม netpie-schema.md, ช่องแก้ตารางยาเขียนลง `schedule`, test plan T01–T10 เช็คตาม event ในหัวข้อ Event ของ netpie-schema.md
