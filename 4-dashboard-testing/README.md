# 4. Dashboard + Test Lead — ปกรณ์

ส่วนนี้รับผิดชอบ dashboard, การทดสอบระบบ และการรวบรวมผลวัดสำหรับรายงาน ไม่ต้องรอ firmware เสร็จจึงเริ่มเตรียมข้อมูลและตรวจรูปแบบ payload ได้

## เปิด HTML Dashboard

มีหน้าเว็บภาษาไทยพร้อมโหมด DEMO และเชื่อมข้อมูลกล่องผ่าน NETPIE REST API:

```powershell
python -B 4-dashboard-testing/dashboard/web/server.py
```

เปิด **http://127.0.0.1:8080** แล้วกด **เชื่อมต่อ NETPIE** กรอก Client ID และ Token ของกล่องยา ดู [คู่มือ HTML Dashboard](dashboard/web/README.md) สำหรับวิธีใช้ ข้อจำกัดของ firmware และคำสั่งทดสอบ

## การทดสอบการทำงานของโค้ดอยู่ตรงไหน?

| สิ่งที่จะตรวจ | อยู่ที่ | ขอบเขต |
|---|---|---|
| ตรวจข้อมูลจากโค้ด เช่น Shadow, event, ข้อความซ้ำ/ตกหล่น | [test/tools/validate_payloads.py](test/tools/validate_payloads.py) | รันบนคอม ตรวจ mock หรือ log จริง; ตรวจตกหล่นได้เมื่อมีรายการ event ที่ควรได้รับจากแหล่งอิสระ |
| Automated tests ของเครื่องมือตรวจข้อมูล | [test/automated/test_validate_payloads.py](test/automated/test_validate_payloads.py) | ตรวจว่าเครื่องมือรับข้อมูลถูกและปฏิเสธข้อมูลผิด ไม่ใช่ unit test ของ firmware |
| ทดสอบพฤติกรรม firmware ตั้งแต่รับข้อมูลจนถึงผลลัพธ์ | [test/test-plan.md](test/test-plan.md) | T01–T18 ใช้บอร์ดจริงหรือ test build ที่ระบุจุดจำลอง แล้วเก็บผลใน [test-results.md](test/test-results.md) |
| ความเชื่อมโยง Test ID กับไฟล์ firmware และวิธีรัน | [test/code-testing.md](test/code-testing.md) | แยก build check, unit test, integration และ end-to-end |
| ทดสอบหน้าจอ คำสั่ง และ Feed | [dashboard/README.md](dashboard/README.md) | การตั้งค่า, widget mapping, หลักฐาน และ D01–D06 |

**สถานะ firmware ตอนตรวจ repo:** `setup()/loop()` ของทั้งสองบอร์ดยังว่าง และโมดูลหลักยังเป็นโครง/TODO จึงยังไม่มี firmware unit test ที่เรียก logic จริง และยังไม่ยืนยันผลการทำงานของบอร์ด การผ่านสคริปต์ตรวจข้อมูลไม่เปลี่ยน T01–T18 เป็น PASS

## เริ่มตรวจบนคอม

ใช้ Python 3.10 ขึ้นไปและ standard library ไม่ต้องติดตั้ง package เพิ่ม รันจาก root ของ repository:

```powershell
python -B 4-dashboard-testing/test/tools/validate_payloads.py --mock 4-dashboard-testing/dashboard/mock-data.json
python -B -m unittest discover -s 4-dashboard-testing/test/automated -p "test_*.py" -v
```

คำสั่งแรกตรวจ fixture; คำสั่งที่สองทดสอบเครื่องมือตรวจข้อมูล วิธีตรวจ log จริงและข้อจำกัดอยู่ใน [code-testing.md](test/code-testing.md)

## เอกสารสำหรับทำงานและส่งรายงาน

- [Dashboard](dashboard/README.md): ตั้งค่า NETPIE, ข้อมูลที่แสดง, คำสั่ง และการตรวจ UI
- [Test plan](test/test-plan.md) / [Test results](test/test-results.md): ขั้นตอนและผลจริง T01–T18
- [Metrics](test/metrics.md): วิธีวัด จำนวนรอบ และตารางสรุปค่าจริง
- [Field test](test/field-test.md): ทดสอบต่อเนื่องอย่างน้อย 24 ชั่วโมงและการใช้งานจริง
- [Analysis](test/analysis.md): เปรียบเทียบเป้าหมายและระบบใกล้เคียง
- [Evidence](test/evidence/README.md): รูปแบบ log และการจัดเก็บหลักฐาน
- [Demo checklist](test/demo-checklist.md): ขั้นตอนเดโมและหลักฐานสำรอง
- [Requirement coverage](test/requirement-coverage.md): เชื่อมเกณฑ์โจทย์กับหลักฐาน รวมข้อที่ต้องให้ทีมตัดสินใจ

ทุกตัวอย่างที่ติดป้าย MOCK ใช้เตรียมทดสอบเท่านั้น กรอกผลจริงพร้อม build/commit, Run ID และหลักฐานก่อนตัดสิน PASS
