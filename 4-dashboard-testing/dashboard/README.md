# Dashboard

## HTML Dashboard ที่รันในเครื่อง

เปิดใช้หน้าเว็บที่ [web/index.html](web/index.html) หรือรันจาก root:

```powershell
python -B 4-dashboard-testing/dashboard/web/server.py
```

แล้วเปิด **http://127.0.0.1:8080** รองรับหลายกำไลตาม schema `hr_<n>` และมี DEMO แยกจากข้อมูลจริง ดู [วิธีเชื่อม NETPIE และทดสอบ](web/README.md)

ส่วนด้านล่างเป็นแนวทางสร้าง widget ใน **NETPIE Console Dashboard** เดิม ตาราง field ที่ไม่มี suffix เป็นตัวอย่าง single-node; โปรเจกต์ปัจจุบันให้ยึด [schema ล่าสุด](../../1-system-architect/docs/netpie-schema.md) เช่น `hr_1`, `spo2_1`, `batt_1`

เพิ่มภาพและ export หลังตรวจ D01–D06

## 1. สร้างและเชื่อม Dashboard

1. ใช้ Project และ gateway Device ที่ทีมสร้างไว้ บันทึกชื่ออ้างอิงในรอบทดสอบ
2. เข้า Console > Dashboard > Create ตั้งชื่อ SmartPillBox แล้ว Save
3. เข้า Setting > Add device เลือก gateway ตั้ง Alias เป็น `spb_box` ให้สิทธิ์ Read Shadow, Write Shadow, Read Feed, Subscribe Message และ Publish Message ตามงานที่ใช้
4. กด Edit แล้ว Add Panel/Add Widget ตามตารางด้านล่าง กด Save และเปิดหน้าใหม่เพื่อตรวจว่าค่าตั้งค่ายังอยู่
5. หลังทดสอบสำเร็จ ใช้ Export เก็บ configuration ที่ตรวจและนำข้อมูลลับออกแล้วใน `dashboard/exports/` พร้อมภาพใน `test/evidence/<Run-ID>/` ทดสอบ Import ลง dashboard ทดสอบอีกอันเพื่อยืนยันว่าทำซ้ำได้

รูปแบบ alias, การอ่าน Shadow/Feed และฟังก์ชันในตัวอย่างใช้ตาม [คู่มือ NETPIE Dashboard](https://docs.netpie.io/dashboard.html) ตรวจเอกสารวันที่ 2026-11-21

## 2. การจัดหน้าและ field mapping

จัดสถานะฉุกเฉินและปุ่มรับเหตุไว้ด้านบน ตามด้วยค่ากำไล/กล่อง ตารางยา และกราฟ ใช้ข้อความกำกับสถานะร่วมกับสี และแสดงหน่วยกับเวลา Asia/Bangkok

| Widget / ข้อมูล | แหล่งข้อมูล | การแสดงผล / สิ่งที่ตรวจ |
|---|---|---|
| กล่องเชื่อม NETPIE | `#["spb_box"]["status"]` | 1 = เชื่อมต่อ, 0 = ไม่เชื่อมต่อ; แยกจากสถานะ Agent ของ dashboard และสถานะกำไล |
| กำไลออนไลน์ | Shadow `node_online`, `node_last_seen` | ออนไลน์/ขาดการติดต่อ/ข้อมูลล่าสุดเมื่อใด; ขณะกล่อง offline ค่านี้อาจค้าง |
| HR / SpO2 | `hr`, `spo2` | bpm / %; 0 แสดง “ไม่มีข้อมูล” ตาม protocol |
| แบตเตอรี่ | `batt` | %; 0 คือแบต 0% ไม่ใช่ไม่มีข้อมูล |
| สภาพกล่อง | `temp`, `hum` | °C / %RH; ค่าไม่มีหรืออ่านไม่ได้ไม่แสดงเป็นค่าปกติ |
| ฝา / คนหน้ากล่อง | `lid_open`, `person` | เปิด/ปิด, พบ/ไม่พบ; ไม่ตีความค่าที่หายว่า false |
| สถานะหลัก | `box_state` | `idle`, `pill_remind`, `pill_escalate`, `fall_alarm`, `storage_warn` |
| มื้อถัดไป / ล่าสุด | `next_pill`, `last_pill_taken` | HH:MM / วันเวลา; กรณียังไม่เคยมีข้อมูลต้องตกลงค่าเริ่มต้นกับทีม |
| คิวส่งย้อนหลัง | `queue` | จำนวนค้างส่ง; ตอนเน็ตขาดค่าบน cloud ไม่ใช่จำนวนสด ใช้ serial log ประกอบ |
| ตารางยา | `schedule` | array ของ HH:MM; อ่านกลับหลังบันทึก และตรวจว่า gateway ใช้จริง |
| กราฟย้อนหลัง | Feed `hr`, `spo2`, `batt`, `temp`, `hum` | แกนเวลา หน่วย และช่วงข้อมูลขาดให้เห็นชัด |
| เหตุการณ์ | MQTT `@msg/spb/event` | แสดงเวลาเกิด `ts`, ชนิด `ev`, รายละเอียด `d`; ข้อความซ้ำ ID เดิมไม่เพิ่มจำนวนเหตุ |

ตัวอย่าง VALUE สำหรับ HR ใน Text widget:

```javascript
#["spb_box"]["shadow"]["hr"] === 0
  ? "ไม่มีข้อมูล"
  : (#["spb_box"]["shadow"]["hr"] == null ? "ยังไม่มีข้อมูล" : #["spb_box"]["shadow"]["hr"] + " bpm")
```

## 3. คำสั่งจาก Dashboard

ปุ่ม “รับเหตุ / เคลียร์ alarm ล้ม”:

```javascript
#["spb_box"].publishMsg("spb/cmd", JSON.stringify({ cmd: "clear_alarm" }));
```

`publishMsg` ของ Dashboard ใช้ `spb/cmd` ส่วน MQTT client โดยตรงใช้ `@msg/spb/cmd` ตรวจผลด้วย `fall_acknowledged` ที่ `by=dashboard` และผลหยุด alarm จริง ไม่ถือว่าคลิกปุ่มแล้วสำเร็จทันที

ตัวอย่าง action บันทึกตารางยา:

```javascript
#["spb_box"].writeShadow("schedule", ["08:00", "12:00", "19:00"]);
```

ช่องกรอกจริงต้องตรวจ HH:MM ในช่วง 00:00–23:59 ก่อนเขียน และอ่าน Shadow กลับหลังบันทึก กรณีตารางว่าง เวลาซ้ำ จำนวนมื้อสูงสุด และการแก้ระหว่างเตือนยาให้ใช้ข้อตกลงของทีมใน T15

สำหรับเชื่อมกับ gateway: ขอค่าแรกผ่าน `@shadow/data/get`, รับผ่าน `@private/shadow/data/get/response` และติดตามการแก้ด้วย `@shadow/data/updated` ตาม [MQTT API](https://docs.netpie.io/mqtt-api.html) ให้ผู้พัฒนา gateway ตรวจ payload ที่รับจริงก่อนเชื่อม callback

## 4. Feed, ประวัติเหตุการณ์ และอัตรากินยาตรงเวลา

ในหน้า Device > Feed เพิ่ม Datatag ของ `hr`, `spo2`, `batt`, `temp`, `hum` เลือก Data Path จาก Shadow ตั้งชนิดเป็นตัวเลข หน่วย และ TTL ให้ครอบคลุมช่วงทดสอบจนถึงส่งงาน โดยอ้างอิง [Device Feed/Datatag](https://docs.netpie.io/device-config.html#device-feed-datatag) ใช้ `#["spb_box"]["feed"]` กับ Chart และตรวจว่าค้นข้อมูลเดิมได้หลังปิด/เปิดหน้า

เก็บ raw Feed response พร้อมช่วงเวลาที่ query และจำนวนจุด หลีกเลี่ยงการใช้ค่าที่ aggregate แล้วแทน raw readings หากใช้ API ให้ทำตาม [Data Store API](https://docs.netpie.io/device-api.html#data-store-api) และเก็บผล JSON โดยไม่เก็บ Authorization header ค่าเวลา Feed เป็น milliseconds ส่วน `ts` ของ event ใน schema ทีมเป็น seconds

Shadow เก็บค่าล่าสุด และ MQTT event ที่เห็นสดไม่ใช่ฐานข้อมูลประวัติ ก่อนระบุว่ามีประวัติย้อนหลัง ต้องเลือกและทำที่เก็บ event ถาวรร่วมกับ gateway/cloud แล้วทดสอบเปิดหน้าใหม่ยังเห็นเหตุเดิม ระหว่างพัฒนาสามารถเก็บ MQTT JSONL เป็นหลักฐานและวิเคราะห์ได้ แต่ยังไม่ถือว่าฟีเจอร์ประวัติบน dashboard เสร็จ

อัตรากินยาตรงเวลาให้กำหนดก่อนวัด: เกณฑ์สายที่ยอมรับ, timezone, ตารางยาที่มีผลในแต่ละวัน และนโยบายมื้อที่ข้อมูลหาย คำนวณ `มื้อที่เข้าเกณฑ์ตรงเวลา / มื้อที่ถึงกำหนดและครบช่วงรอผล × 100` ตัด event ซ้ำและไม่นับมื้ออนาคต ข้อมูลไม่ครบให้แสดงจำนวนมื้อที่ไม่ทราบผลแยก ไม่คำนวณจาก `last_pill_taken` ตัวเดียว

## 5. ตรวจ Dashboard (D01–D06)

| ID | ขั้นตอน | ผลที่คาด | สถานะ |
|---|---|---|---|
| D01 | ป้อนค่าปกติ ค่า 0 และไม่มี field แล้วเทียบกับ Shadow | หน่วย/ชื่อ field ถูก; HR/SpO2=0 ไม่มีข้อมูล แต่ batt=0 เป็น 0%; ไม่แต่งค่าทดแทน | NOT RUN |
| D02 | ตัดการเชื่อมต่อแยก dashboard, gateway และกำไล | แสดงส่วนที่ขาดถูกต้อง ค่าเก่าไม่ถูกนำเสนอเป็นค่าปัจจุบัน | NOT RUN |
| D03 | เริ่ม alarm แล้วเคลียร์จากหน้า UI | ได้ command, `fall_acknowledged` และหยุด alarm จริง; ทดสอบใน T01/T18 | NOT RUN |
| D04 | แก้ตารางยา อ่านกลับ รีบูตกล่อง และทดสอบค่าผิด | gateway ใช้ตารางที่บันทึก; รายงานค่าผิดโดยไม่ทำข้อมูลเดิมเสีย; ดู T04/T15 | NOT RUN |
| D05 | อ่าน Feed ย้อนหลัง เปิดหน้าใหม่ และตรวจ event history | Feed มีข้อมูลดิบพร้อมเวลา; ประวัติ event ต้องอ่านจากที่เก็บจริง; เปรียบเทียบกับ log | NOT RUN |
| D06 | เปิดบนมือถือ ให้ผู้ทดลองหาสถานะและรับเหตุ แล้วทดสอบ Export/Import | อ่านหน่วย/ข้อความได้ ปุ่มใช้งานได้ ไม่มีส่วนถูกตัด; บันทึกปัญหาและภาพ | NOT RUN |

บันทึกผลจริง D01–D06 ใน [test-results.md](../test/test-results.md) และหลักฐานใน [evidence](../test/evidence/README.md) การรัน mock validator ไม่ใช่การตรวจการแสดงผลของ widget
