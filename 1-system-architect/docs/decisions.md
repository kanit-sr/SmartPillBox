# Decisions

| วันที่ | เรื่อง | ตัดสินใจ | เหตุผล |
|---|---|---|---|
| 2026-11-15 | AI รันบน C3 หรือ gateway | **กำไล (XIAO C3)** | กำไลส่งแค่ผล "ล้ม" packet เล็ก ถ้าผลเทส Edge Impulse บน C3 ไม่ผ่าน ต้องเพิ่ม packet ส่ง IMU ดิบไป gateway (แก้ format เยอะ) |
| 2026-11-15 | LINE ยิงจาก gateway หรือ NETPIE trigger | **gateway (กล่องยา)** ยิงตรงไป LINE Messaging API | มี `line_notify.*` ในโครงกล่องแล้ว, ไม่ต้องตั้ง trigger ฝั่ง NETPIE, ตอนเน็ตหลุดก็คิวรวมกับ store-and-forward ได้ |
| 2026-11-15 | เวลาของ event มาจากไหน | **กล่องยา (NTP)** | กำไลไม่มีนาฬิกาจริง จึงส่งแค่ `age_ms` (เกิดมาแล้วกี่ ms) ให้กล่องคำนวณเวลาจริง |
| 2026-11-20 | แยก sensor node ยังไง | **`node_id` (uint8) ใน header ทุก packet** + field shadow แยกต่อ node (`hr_1`, `hr_2`, ...) | ทำ Scalability 2 คะแนน (กำไลจริง 1 + node จำลอง 2) บอร์ดจำลองตัวเดียวเล่นหลาย node ได้ (MAC ซ้ำ) และ dashboard ต้องแยก node ได้ ซึ่ง MAC ไม่ถูกส่งขึ้น cloud |
| 2026-11-20 | จำกัดจำนวน node ไหม | **ไม่จำกัด id** (1–254 ตั้งเองใน config) กล่องลงทะเบียน node ใหม่เอง, `MAX_NODES` = 8 แค่ขนาดตาราง | เพิ่ม node จำลองได้เรื่อยๆ โดยไม่ต้องแก้ spec หรือโค้ดกล่อง |
