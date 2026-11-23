// config.h — ขา GPIO และค่าตั้งของกำไล
#pragma once

// ---------- ตัวตนของ node (message-format.md) ----------
#define NODE_ID              1      // กำไลจริง = 1, node จำลองตั้งเลขอื่น (1–254 ห้ามซ้ำ)

// MAC ของกล่องยา (ESP32 DevKit) ดูได้จาก Serial ตอนกล่องบูต หรือจาก fake_box
// กล่องที่ใช้จริง (DevKit ตัวเดียวกับที่เทส) a4:f0:0f:65:80:6c
#define GATEWAY_MAC          {0xA4, 0xF0, 0x0F, 0x65, 0x80, 0x6C}

#define DEBUG_LOG            1      // 1 = พิมพ์ log ทาง Serial 115200

// ---------- ขา GPIO (XIAO ESP32-C3, ตรงกับ test/hw_check_wristband) ----------
// ปุ่ม: D1 = GPIO3 ต่อลง GND (ใช้ pull-up ภายใน) — ESP32-C3 ปลุกจาก deep sleep ได้เฉพาะ GPIO0–5
#define PIN_BUTTON           3
// มอเตอร์: D10 = GPIO10 → ขา base/gate ของทรานซิสเตอร์ (ใส่ R pull-down ที่ base กันมอเตอร์หมุนตอน deep sleep)
#define PIN_MOTOR            10
// แบต: D0 = GPIO2 (ADC1) ผ่าน voltage divider 1:2 จาก BAT+ (เช่น 2 × 220k) XIAO ไม่มี divider ในตัว
#define PIN_BATT_ADC         2

// ---------- แบตเตอรี่ ----------
#define BATT_ENABLED         1      // 0 = ยังไม่ได้ต่อ divider → ส่ง batt_mv = 0, batt_pct = 100 (กันกล่องเตือนแบตต่ำผิดๆ)
#define BATT_DIVIDER         2.0f   // แรงดันจริง = แรงดันที่ขา ADC × ค่านี้
#define BATT_SAMPLES         16     // อ่านกี่ครั้งแล้วเฉลี่ย

// ---------- MAX30102 (ชีพจร / SpO2) ----------
// sensor ตั้งที่ 100 Hz เฉลี่ย 4 → ได้ 25 sample/วิ, ปิดเครื่อง (shutdown) ไว้ระหว่างรอบเพื่อประหยัดไฟ
#define PULSE_FS_HZ          25
#define PULSE_SETTLE_SAMPLES 25     // ทิ้ง 1 วิแรกหลังเปิด LED (สัญญาณยังไม่นิ่ง)
#define PULSE_WINDOW_SAMPLES 150    // เก็บ 6 วิ แล้วคำนวณ
#define PULSE_IR_MIN         50000  // ค่า IR เฉลี่ยต่ำกว่านี้ = ไม่ได้แนบผิว → hr = spo2 = 0
#define PULSE_LED_RED        0x1F   // กระแส LED แดง (0x1F ≈ 6.4 mA)
#define PULSE_LED_IR         0x1F   // กระแส LED IR

// ---------- จังหวะการทำงาน ----------
#define FIRST_STATUS_DELAY_MS   2000   // ส่ง STATUS แรกหลังบูตเร็วๆ ให้กล่องลงทะเบียน node
#define SENSOR_RETRY_MS         5000   // เซนเซอร์หาไม่เจอ → ลองใหม่ทุกกี่ ms (สายหลวม)
#define FALL_CANCELLED_TRIES    5      // spec: retry สูงสุด 5 ครั้ง
#define POWER_OFF_TRIES         5      // spec: retry สูงสุด 5 ครั้ง แล้ว deep sleep

// ---------- log ----------
#if DEBUG_LOG
#define LOG(...) Serial.printf(__VA_ARGS__)
#else
#define LOG(...) do {} while (0)
#endif

// ---------- I2C (MPU6050 + MAX30102 ใช้บัสเดียวกัน) ----------
// XIAO ESP32-C3: D4 = GPIO6 (SDA), D5 = GPIO7 (SCL)
#define PIN_I2C_SDA          6
#define PIN_I2C_SCL          7
#define I2C_FREQ_HZ          400000

// ---------- IMU (ต้องตรงกับข้อมูลที่ใช้เทรน FallAllD_wrist_50Hz) ----------
#define IMU_SAMPLE_HZ        50     // 1 sample ทุก 20 ms
#define IMU_WINDOW_SAMPLES   100    // ring buffer 2 วิ
#define IMU_AXES             6      // accX, accY, accZ (g), gyrX, gyrY, gyrZ (deg/s)

// ---------- Fall detector ----------
#define FALL_INFER_INTERVAL_MS  500    // รันโมเดลทุก 0.5 วิ (เท่ากับ window increase ตอนเทรน)
#define FALL_PROB_THRESHOLD     0.60f  // p(fall) ขั้นต่ำที่นับเป็น "โหวตล้ม" 1 ครั้ง
#define FALL_VOTE_WINDOW        3      // ดูผล 3 ครั้งล่าสุด (~1.5 วิ)
#define FALL_VOTE_NEEDED        2      // ต้องโหวตล้มอย่างน้อย 2 ใน 3 ถึงจะยืนยัน

// เช็คว่าแขนนิ่งหลังกระแทกก่อนยืนยัน (ลดการเตือนผิดจากปรบมือ/ทุบโต๊ะ)
// ปิดไว้ก่อนจนกว่าจะปรับค่ากับข้อมูลจากกำไลจริง — ถ้าตั้งผิดจะทำให้พลาดการล้มจริง
#define FALL_REQUIRE_STILLNESS  0
#define FALL_STILL_STD_G        0.15f  // ส่วนเบี่ยงเบนของ |a| ใน 1 วิล่าสุดต้องต่ำกว่านี้
#define FALL_STILL_TIMEOUT_MS   3000   // ไม่นิ่งภายในเวลานี้ = ไม่ใช่การล้ม
