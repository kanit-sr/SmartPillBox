# Fall Detection Model

- window: 2 วิ (100 sample × 6 แกน)
- label: fall, normal (เดิน, นั่ง, ลุก, ยกแขน ...)
- บันทึก: จำนวน sample ต่อ label, accuracy, RAM/latency บน C3

## Dataset: FallAllD (ข้อมือ D2)
สคริปต์ `prepare_fallalld.py` แปลง FallAllD เป็น window สำหรับ Edge Impulse (ต้องมี `pip install numpy scipy`)

```
python prepare_fallalld.py --variant A    # ADL ทั้งหมด
python prepare_fallalld.py --variant B    # ตัดปรบมือ/โบกมือ/ทุบโต๊ะ (ID 1-7, 12)
```

- หน่วย: accX..Z เป็น g, gyrX..Z เป็น deg/s, 50 Hz, ลำดับ `accX,accY,accZ,gyrX,gyrY,gyrZ` (firmware ต้องส่งเข้าโมเดลลำดับเดียวกัน)
- MPU6050 ต้องตั้ง accel ±8 g, gyro ±2000 dps ให้ตรงกับ dataset
- fall = ID 101-135 ตัด 5 window รอบจุดกระแทก / normal = ID 1-44 สุ่มให้จำนวนเท่า fall
- แบ่งชุดตามคน: S13-S15 = testing
- ผลลัพธ์อยู่นอก repo: `Embedded System/FallAllD_wrist_50Hz/variant_A|B/` (training 1859+1859, testing 750+750)
- อ้างอิง: M. Saleh et al., "FallAllD", IEEE Sensors Journal, doi:10.1109/JSEN.2020.3018335 (CC BY 4.0)
