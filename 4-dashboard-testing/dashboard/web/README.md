# SmartPillBox HTML Dashboard

หน้า HTML/CSS/JavaScript ภาษาไทยสำหรับอ่านข้อมูลกล่องยาจาก NETPIE 2020 มี layout สำหรับจอคอมและมือถือ ใช้ Python standard library เป็นตัวกลางในเครื่อง ไม่ต้องติดตั้ง npm package หรือใช้ CDN

## 1. เปิดใช้งาน

จาก root ของ repository ใช้ **Python 3.10 ขึ้นไป**:

```powershell
python -B 4-dashboard-testing/dashboard/web/server.py
```

เปิด **http://127.0.0.1:8080** ใน Chrome / Edge / Firefox รุ่นปัจจุบัน หน้าเริ่มในโหมด **DEMO** ซึ่งตัวเลข กราฟ และเหตุการณ์ทั้งหมดเป็นข้อมูลจำลอง ปิดตัวรันด้วย `Ctrl+C`

ถ้า port ไม่ว่าง:

```powershell
python -B 4-dashboard-testing/dashboard/web/server.py --port 8081
```

แล้วเปิด http://127.0.0.1:8081 แทน

หากต้องการดูตัวอย่างอย่างเดียว เปิด `index.html` ได้โดยตรง แต่การเชื่อม NETPIE จริงต้องเปิดผ่านตัวรันด้านบน ไม่ใช้ `python -m http.server` เพราะไม่มี API bridge

## 2. เชื่อมต่อกล่องจริง

1. ให้กล่อง publish Shadow ตาม [schema ล่าสุด](../../../1-system-architect/docs/netpie-schema.md) และใส่ Device ไว้ใน Device Group สำหรับข้อความ `spb/cmd`
2. กด **เชื่อมต่อ NETPIE** กรอก **Client ID และ Token ของกล่องยา** จาก NETPIE Console ด้วยตัวเองในหน้าจอ ไม่ต้องใส่ Secret
3. เลือกรอบอ่าน เริ่มที่ **15 วินาที** แล้วเชื่อมต่อ แต่ละรอบอ่าน Shadow และสถานะกล่องรวม 2 API calls (ประมาณ 480 calls/ชั่วโมงที่ 15 วินาที; ไม่นับการตั้งค่าหรือคำสั่ง) ปรับช่วงตามโควตาของบัญชี
4. ตรวจว่าป้ายเปลี่ยนจาก DEMO เป็น NETPIE แล้วเลือกกำไล เปรียบเทียบค่ากับ Shadow ใน Console
5. เมื่อจบ กด **กลับโหมดจำลอง** เพื่อลบ session หรือหยุด Python ด้วย `Ctrl+C`

อ่านผ่าน REST API จึงไม่เปิด MQTT connection ด้วย Client ID ซ้ำกับกล่อง ตัวรัน bind เฉพาะ `127.0.0.1` หน้านี้จึงเปิดได้ในคอมเครื่องเดียว การใช้จากมือถือจริงผ่านเครือข่ายยังไม่รวมในเวอร์ชันนี้

รหัสเก็บในหน่วยความจำตัวรันเท่านั้น ใช้ session cookie แบบ HttpOnly / SameSite=Strict หมดอายุและถูกลบภายใน 8 ชั่วโมง หรือเมื่อ disconnect/ปิดตัวรัน ไม่เก็บ Token ใน localStorage, ไฟล์, log หรือไฟล์ส่งออก อย่าเพิ่มรหัสจริงลง source code

## 3. สิ่งที่แสดงและขอบเขต

| ส่วน | แหล่งข้อมูล / พฤติกรรม |
|---|---|
| เลือกกำไล | ค้นจาก `nodes` และ suffix `_<node_id>` อัตโนมัติ เช่น `hr_1`, `hr_2`; รองรับ legacy `hr`/`spo2` เฉพาะข้อมูลแบบกำไลเดียว |
| HR / SpO₂ / แบตเตอรี่ | HR/SpO₂ = 0 แสดง `—`; batt = 0 แสดง `0%`; ไม่แทน field ที่หายด้วยค่าปกติ |
| สถานะกำไล | ใช้ `node_online_<n>` และ `node_last_seen_<n>`; เก่ากว่า 90 วินาทีแสดงข้อมูลเก่า (เกณฑ์หน้าจอ ไม่ใช่ timeout ของ firmware); ไม่มี timestamp แสดง “รายงานว่าออนไลน์” |
| สถานะกล่อง | อ่าน NETPIE `/status` แยกจากสถานะกำไล หากอ่านไม่สำเร็จระบุว่าค่าบนหน้าจออาจเก่า |
| กล่องยา | `temp`, `hum`, `lid_open`, `person`, `queue`, `box_state`, `alarm_node`; ไม่มีเวลาวัดแยกจึงไม่ยืนยันความสดของ temp/hum จากเวลา polling |
| ตารางยา | `schedule`, `next_pill`, `last_pill_taken`; ไม่คำนวณอัตรากินยาตรงเวลาจากข้อมูลไม่ครบ |
| กราฟ HR | ไม่เกิน 120 จุดต่อกำไล เฉพาะค่าที่เปลี่ยน/เวลาต้นทางเปลี่ยนระหว่างเปิดหน้า แยกช่วงเมื่อ offline/ข้อมูลขาด ไม่ได้ดึง NETPIE Feed ย้อนหลัง |
| เหตุการณ์ | อ่าน `last_event` ในแต่ละรอบ เก็บไม่เกิน 500 รายการในหน่วยความจำ กรองและส่งออก JSON ได้ เปิดหน้าใหม่เริ่มประวัติใหม่ |

**ข้อจำกัด last_event:** ถ้าเกิดหลายเหตุระหว่างรอบ polling อาจเห็นเพียงเหตุล่าสุด จึงใช้หน้านี้พิสูจน์ว่าไม่มี event ตกหล่นไม่ได้ เหตุที่มี ID ซ้ำจะไม่ถูกนับเพิ่ม หาก payload ขัดแย้งจะมีข้อความกำกับ ส่วน event ที่ไม่มี `id`/`ts` แสดงเวลา “อ่านพบ” และตัดข้อความเหมือนกันซ้ำโดยไม่สมมติว่าเป็น event ใหม่ การ export เป็น JSON ของรายการที่หน้าอ่านพบ ไม่ใช่ MQTT JSONL สำหรับ `validate_payloads.py`

## 4. คำสั่งและการตรวจผล

**แก้ไขตารางยา:** กดแก้ไข ใส่ `08:00, 12:00, 19:00` ตรวจเวลา 00:00–23:59 ต้องมีอย่างน้อยหนึ่งมื้อและไม่มีเวลาซ้ำ (ข้อจำกัดของฟอร์มนี้) ใช้ `PUT` แบบ merge เฉพาะ `schedule` แล้ว GET กลับมาเทียบ การอ่านกลับตรงหมายถึง Cloud รับค่าแล้ว ยังต้องตรวจว่ากล่องนำไปใช้และคงอยู่หลัง reboot

**รับเหตุ / เคลียร์ alarm:** เปิดเมื่อกล่องรายงาน `fall_alarm` หรือ `sos` และ NETPIE รายงานว่ากล่องออนไลน์ มีหน้าต่างยืนยันก่อนส่ง `{"cmd":"clear_alarm"}` ไปที่ REST `/message/spb/cmd` ซึ่งตรงกับ MQTT `@msg/spb/cmd` ของกล่อง หน้าจอรอ event `fall_acknowledged` ใหม่ที่มี `d.by="dashboard"`, node ตรงกัน และ timestamp หลังส่งคำสั่ง ไม่ถือว่าการตอบรับ HTTP ยืนยันว่าเสียง/ไฟหยุดแล้ว ไม่ retry คำสั่งเองเมื่อผลไม่แน่นอน

**ข้อจำกัด firmware ปัจจุบันที่ตรวจ 2026-11-23:** `test/hw_netpie_box/hw_netpie_box.ino` ส่งค่า node 1 และ `last_event` แบบย่อ ไม่มี `id`, `ts`, `d.by` และไม่ได้อ่าน `schedule` จาก Shadow หน้านี้อ่านค่าจาก test sketch ได้ แต่ยังยืนยัน ACK ตาม schema หรือการใช้ตารางยาที่บันทึกไม่ได้ ต้องพัฒนา firmware ส่วนนี้ก่อนตัดสิน D03/D04 เป็น PASS ส่วน `3-gateway-cloud/gateway/netpie.cpp` ยังเป็นโครง

## 5. ทดสอบและบันทึกผล

Node.js 18 ขึ้นไปสำหรับ test ฝั่ง JavaScript; Python 3.10 ขึ้นไปสำหรับ bridge tests ไม่มี dependency เพิ่ม รันจาก root:

```powershell
node --test 4-dashboard-testing/dashboard/web/tests/core.test.cjs
python -B -m unittest discover -s 4-dashboard-testing/dashboard/web/tests -p "test_*.py" -v
```

ผลตรวจ 2026-11-23: JavaScript 12 tests **PASS**, Python HTTP bridge 10 tests **PASS** (NETPIE responses เป็น mock ทั้งหมด) และ JavaScript syntax check ผ่าน

สิ่งที่ยัง **NOT RUN**: ตรวจภาพหน้าจอจริง desktop/mobile, เชื่อมบัญชี NETPIE จริง, readback จาก cloud จริง, ส่งคำสั่งถึงบอร์ด, Feed history, firmware/hardware และ D01–D06 end-to-end ห้ามใช้ผล unit tests แทนผลเหล่านี้

ตรวจต่อโดยสร้าง Run-ID ใน `../../test/evidence/` และบันทึกผลใน [test-results.md](../../test/test-results.md):

1. เปิด desktop และ mobile viewport: ตรวจเมนู การเลือกกำไล ตารางยา และ popup ไม่ล้นจอ
2. เชื่อมจริงและเทียบค่า node 1/2; ทดสอบ HR/SpO₂=0, batt=0, field หาย และ sensor ไม่ส่งเวลา
3. ตัดเน็ตคอม กล่อง และกำไลแยกกัน ตรวจสถานะเก่าและการกลับมาอ่านข้อมูล
4. ส่ง clear_alarm เมื่อมีเหตุทดสอบ ตรวจ serial log, event ACK, LED และ buzzer
5. แก้ตาราง ทดสอบเวลาผิดและซ้ำ อ่านกลับ รีบูตกล่อง และตรวจว่ากล่องเตือนจริง
6. ส่ง event ซ้ำ/หลาย event ระหว่าง polling เทียบกับ log อิสระ และระบุรายการที่หน้าจอไม่ได้เห็น

## โครงสร้างและ API อ้างอิง

- `index.html` — โครงหน้าและฟอร์ม
- `styles.css` — ธีมสีเขียวและ responsive layout
- `core.js` — อ่าน schema, ตรวจค่า, กำไล, ตารางยา และ deduplication
- `app.js` — demo/live, polling, กราฟ, ฟอร์ม และ export
- `server.py` — static server และ NETPIE REST bridge เฉพาะเครื่องนี้
- `tests/` — regression tests ของ logic และ local HTTP API

เส้นทาง NETPIE ตรวจจาก [RESTful API ของ NETPIE](https://docs.netpie.io/device-api.html): `GET /shadow/data`, `GET /status`, `PUT /shadow/data` และ `PUT /message/spb/cmd` ภายใต้ `https://api.netpie.io/v2/device` ใช้ Authorization แบบ `Device ClientID:Token` อ้างอิงการส่งข้อความร่วมกับ [MQTT API](https://docs.netpie.io/mqtt-api.html)
