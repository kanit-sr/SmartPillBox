# ตั้งค่า NETPIE + LINE (ทำครั้งเดียว)

## 1. NETPIE2020 (portal.netpie.io)

1. ใช้ project และ device เดิมของทีม (alias **`spb_box`** ตามที่ dashboard ใช้ `#["spb_box"]`)
   - ถ้ายังไม่มี: สร้าง Project → Device Groups → สร้างกลุ่ม → Devices → สร้าง device ใส่ alias `spb_box` แล้วย้ายเข้ากลุ่ม
2. เปิด device → **Key** → copy Client ID / Token / Secret ไปใส่ `secrets.h`
3. แท็บ **Schema** → วางเนื้อหา [netpie-device-schema.json](netpie-device-schema.json) → Save
   - field ที่มี `store` (hr, spo2, batt, temp, hum) จะเก็บลง **Feed** เป็นกราฟย้อนหลัง 30 วัน
   - schema ตั้ง `additionalProperties: false` ถ้าจะเพิ่ม field ใหม่ใน shadow ต้องเพิ่มใน schema ด้วย ไม่งั้น NETPIE จะไม่รับ
4. เปิด **Feed** ของ device (สวิตช์ Store / Feed เปิดอยู่)
5. dashboard ต้องอยู่ใน project เดียวกัน และ device ต้องอยู่ใน group ที่ dashboard ผูกไว้

## 2. LINE Messaging API (developers.line.biz)

1. login → สร้าง **Provider** → **Create a Messaging API channel** (ได้ LINE Official Account มาด้วย)
2. แท็บ **Messaging API**
   - ล่างสุด **Channel access token (long-lived) → Issue** → copy ใส่ `LINE_TOKEN`
   - ให้ญาติทุกคนสแกน **QR code** แอดเพื่อน
3. LINE Official Account Manager → ปิด **Auto-reply** และ **Greeting message**
4. `LINE_TO` เว้นว่าง = broadcast ถึงทุกคนที่แอดเพื่อน (userId ตัวเองดูได้ที่ Basic settings → Your user ID)

ทดสอบ token (ส่งจริง 1 ข้อความต่อเพื่อน 1 คน):

```bash
curl -X POST https://api.line.me/v2/bot/message/broadcast \
  -H "Authorization: Bearer $LINE_TOKEN" -H "Content-Type: application/json" \
  -d '{"messages":[{"type":"text","text":"ทดสอบ Smart Pill Box"}]}'
# ได้ {} = สำเร็จ
# ดูโควตาที่ใช้ไปเดือนนี้
curl -H "Authorization: Bearer $LINE_TOKEN" https://api.line.me/v2/bot/message/quota/consumption
```

แพ็กเกจฟรีของ LINE OA จำกัดข้อความต่อเดือน (นับต่อผู้รับ) ตอนเทส dashboard ให้ใช้ `mock_box.py` ซึ่งไม่ส่ง LINE

## 3. กล่องยา (ESP32)

1. Library Manager → ลง **PubSubClient** และ **ArduinoJson** (v7)
2. copy `gateway/secrets.example.h` เป็น `gateway/secrets.h` ใส่ Wi-Fi, NETPIE, LINE
3. Upload → Serial Monitor (115200) ต้องเห็น `[netpie] ต่อ NETPIE แล้ว` และใน portal device ขึ้นสถานะ online

| Serial แสดง | สาเหตุ |
|---|---|
| `ต่อ MQTT ไม่ได้ state=5` | Client ID / Token / Secret ผิด |
| `ต่อ MQTT ไม่ได้ state=-2` | ยังต่อเน็ตไม่ได้ / ต่อ `mqtt.netpie.io:1883` ไม่ได้ (Wi-Fi บางที่บล็อกพอร์ตนี้) |
| ต่อได้แต่ shadow ไม่ขึ้นค่า | field ไม่อยู่ใน Device Schema หรือชนิดไม่ตรง |
| `[LINE] HTTP 401` | `LINE_TOKEN` ผิด / ถูก reissue |
| `[LINE] HTTP 429` | โควตาข้อความเดือนนี้หมด หรือยิงถี่เกิน |

## 4. ทดสอบ dashboard โดยไม่มีฮาร์ดแวร์

```bash
pip install paho-mqtt
export NETPIE_CLIENT_ID=... NETPIE_TOKEN=... NETPIE_SECRET=...
python3 4-dashboard-testing/tools/mock_box.py          # ส่งค่าปกติ + ตอบปุ่มเคลียร์
python3 4-dashboard-testing/tools/mock_box.py --fall   # จำลองล้ม
```

ใช้ device เดียวกับกล่องได้ แต่ห้ามเปิดพร้อมกล่องจริง (Client ID เดียวกันจะเตะกันหลุด) หรือสร้าง device ทดสอบแยกที่ alias `spb_box` ใน group อื่น
