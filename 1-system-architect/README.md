# 1. System Architect & Integration — คณิต

- กำหนด message format ของ ESP-NOW และ NETPIE
- ทำ shared lib ที่กำไลกับกล่องยาใช้ร่วมกัน
- state diagram ของทั้งสองบอร์ด
- offline store-and-forward ของกล่องยา
- ประกอบระบบและทดสอบ end-to-end

## ไฟล์
- `docs/message-format.md` — รายละเอียด packet ทุกแบบ
- `docs/state-diagram.md` — state ของกำไลและกล่องยา
- `docs/netpie-schema.md` — topic / shadow / ชื่อ event
- `docs/decisions.md` — บันทึกการตัดสินใจ (LINE ยิงจากไหน, AI อยู่บอร์ดไหน)
- `../libraries/protocol/` — struct และค่าคงที่ร่วม
- `../libraries/espnow_link/` — ส่ง/รับ ESP-NOW + ACK/retry + channel scan
- `../libraries/store_forward/` — เก็บข้อมูลตอนเน็ตหลุด (กล่องยาใช้)
