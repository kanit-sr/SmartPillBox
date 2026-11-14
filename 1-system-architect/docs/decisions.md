# Decisions

| วันที่ | เรื่อง | ตัดสินใจ | เหตุผล |
|---|---|---|---|
| 2026-11-15 | AI รันบน C3 หรือ gateway | **กำไล (XIAO C3)** | กำไลส่งแค่ผล "ล้ม" packet เล็ก ถ้าผลเทส Edge Impulse บน C3 ไม่ผ่าน ต้องเพิ่ม packet ส่ง IMU ดิบไป gateway (แก้ format เยอะ) |
| 2026-11-15 | LINE ยิงจาก gateway หรือ NETPIE trigger | **gateway (กล่องยา)** ยิงตรงไป LINE Messaging API | มี `line_notify.*` ในโครงกล่องแล้ว, ไม่ต้องตั้ง trigger ฝั่ง NETPIE, ตอนเน็ตหลุดก็คิวรวมกับ store-and-forward ได้ |
| 2026-11-15 | เวลาของ event มาจากไหน | **กล่องยา (NTP)** | กำไลไม่มีนาฬิกาจริง จึงส่งแค่ `age_ms` (เกิดมาแล้วกี่ ms) ให้กล่องคำนวณเวลาจริง |
