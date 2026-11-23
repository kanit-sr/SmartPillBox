#include "pulse.h"
#include "config.h"
#include <Arduino.h>
#include <Wire.h>
#include <math.h>

namespace {

// ---------- register ของ MAX30102 ----------
constexpr uint8_t ADDR            = 0x57;
constexpr uint8_t REG_INTR_EN1    = 0x02;
constexpr uint8_t REG_INTR_EN2    = 0x03;
constexpr uint8_t REG_FIFO_WR_PTR = 0x04;
constexpr uint8_t REG_OVF_COUNTER = 0x05;
constexpr uint8_t REG_FIFO_RD_PTR = 0x06;
constexpr uint8_t REG_FIFO_DATA   = 0x07;
constexpr uint8_t REG_FIFO_CONFIG = 0x08;
constexpr uint8_t REG_MODE_CONFIG = 0x09;
constexpr uint8_t REG_SPO2_CONFIG = 0x0A;
constexpr uint8_t REG_LED1_PA     = 0x0C;  // แดง
constexpr uint8_t REG_LED2_PA     = 0x0D;  // IR
constexpr uint8_t REG_PART_ID     = 0xFF;
constexpr uint8_t PART_ID         = 0x15;

constexpr uint8_t MODE_SPO2       = 0x03;  // แดง + IR
constexpr uint8_t MODE_SHDN       = 0x80;
constexpr uint8_t MODE_RESET      = 0x40;
// เฉลี่ย 4 sample (0b010 << 5) + rollover + almost-full = 15
constexpr uint8_t FIFO_CFG        = (0x2 << 5) | 0x10 | 0x0F;
// ADC 4096 nA (0b01 << 5) + 100 sps (0b001 << 2) + pulse width 411 us / 18 bit (0b11)
// 100 sps ÷ เฉลี่ย 4 = 25 Hz ตรงกับ PULSE_FS_HZ
constexpr uint8_t SPO2_CFG        = (0x1 << 5) | (0x1 << 2) | 0x3;
static_assert(PULSE_FS_HZ == 25, "ถ้าแก้ PULSE_FS_HZ ต้องแก้ SPO2_CFG / FIFO_CFG ให้ตรงด้วย");

constexpr int      BYTES_PER_SAMPLE = 6;     // แดง 3 bytes + IR 3 bytes
constexpr int      CHUNK            = 16;    // อ่านทีละ 16 sample = 96 bytes (buffer Wire ของ ESP32 = 128)
constexpr uint32_t STALL_MS         = 1000;  // ไม่มี sample ใหม่นานเกินนี้ = เซนเซอร์หลุด
constexpr int      MAX_PEAKS        = PULSE_WINDOW_SAMPLES / 4;

enum class Phase : uint8_t { IDLE, SETTLING, COLLECTING };

bool     sensor_ok = false;
Phase    phase = Phase::IDLE;
int      settle_left = 0;
int      count = 0;
uint32_t last_sample_ms = 0;
float    red_buf[PULSE_WINDOW_SAMPLES];
float    ir_buf[PULSE_WINDOW_SAMPLES];
float    tmp_buf[PULSE_WINDOW_SAMPLES];
PulseResult last = {0, 0, false};

bool write_reg(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(ADDR);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

bool read_regs(uint8_t reg, uint8_t *buf, uint8_t n) {
  Wire.beginTransmission(ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(ADDR, n) != n) return false;
  for (uint8_t i = 0; i < n; i++) buf[i] = Wire.read();
  return true;
}

uint32_t be18(const uint8_t *p) {
  return (((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | p[2]) & 0x3FFFF;
}

void clear_fifo() {
  write_reg(REG_FIFO_WR_PTR, 0);
  write_reg(REG_OVF_COUNTER, 0);
  write_reg(REG_FIFO_RD_PTR, 0);
}

void add_sample(uint32_t red, uint32_t ir) {
  if (phase == Phase::SETTLING) {
    if (--settle_left <= 0) phase = Phase::COLLECTING;
    return;
  }
  if (count < PULSE_WINDOW_SAMPLES) {
    red_buf[count] = (float)red;
    ir_buf[count] = (float)ir;
    count++;
  }
}

// อ่าน sample ทั้งหมดที่ค้างใน FIFO — คืนจำนวนที่อ่านได้, -1 = I2C error
int drain_fifo() {
  uint8_t ptr[3];  // WR_PTR, OVF_COUNTER, RD_PTR อยู่ติดกัน
  if (!read_regs(REG_FIFO_WR_PTR, ptr, 3)) return -1;
  // OVF_COUNTER ไม่เป็น 0 = FIFO เต็ม (sample เก่าบางส่วนหายไปแล้ว เพราะ loop ช้าเกิน ~1.3 วิ)
  int avail = ptr[1] ? 32 : ((ptr[0] - ptr[2]) & 0x1F);
  int got = 0;
  uint8_t b[CHUNK * BYTES_PER_SAMPLE];
  while (avail > 0) {
    const int n = avail < CHUNK ? avail : CHUNK;
    if (!read_regs(REG_FIFO_DATA, b, n * BYTES_PER_SAMPLE)) return -1;
    for (int i = 0; i < n; i++) add_sample(be18(b + i * 6), be18(b + i * 6 + 3));
    avail -= n;
    got += n;
  }
  return got;
}

// ลบ baseline ด้วย moving average 1 วิ (high-pass) แล้วเกลี่ย 4 sample (low-pass) เขียนทับที่เดิม
// คืนค่า DC (ค่าเฉลี่ย) ของสัญญาณเดิม
float to_ac(float *x, int n) {
  double sum = 0;
  for (int i = 0; i < n; i++) sum += x[i];
  const float dc = (float)(sum / n);

  const int half = PULSE_FS_HZ / 2;
  for (int i = 0; i < n; i++) {
    const int a = i - half < 0 ? 0 : i - half;
    const int b = i + half >= n ? n - 1 : i + half;
    double s = 0;
    for (int k = a; k <= b; k++) s += x[k];
    tmp_buf[i] = x[i] - (float)(s / (b - a + 1));
  }
  for (int i = 0; i < n; i++) {
    const int b = i + 3 < n ? i + 3 : n - 1;
    float s = 0;
    for (int k = i; k <= b; k++) s += tmp_buf[k];
    x[i] = s / (b - i + 1);
  }
  return dc;
}

float rms(const float *x, int n) {
  double s = 0;
  for (int i = 0; i < n; i++) s += (double)x[i] * x[i];
  return sqrtf((float)(s / n));
}

// หาจังหวะหัวใจจากสัญญาณ IR → bpm, 0 = สัญญาณไม่ดี
// กลับขั้วก่อนหายอด: ตอนหัวใจบีบ เลือดที่ผิวมากขึ้น แสงสะท้อนกลับน้อยลง
uint8_t heart_rate(const float *ac, int n) {
  const float thr = 0.5f * rms(ac, n);
  const int min_gap = PULSE_FS_HZ * 60 / 200;  // เร็วสุด 200 bpm
  int peaks[MAX_PEAKS];
  int np = 0;

  for (int i = 1; i < n - 1; i++) {
    const float v = -ac[i];
    if (v <= thr || v <= -ac[i - 1] || v < -ac[i + 1]) continue;
    if (np > 0 && i - peaks[np - 1] < min_gap) {
      if (v > -ac[peaks[np - 1]]) peaks[np - 1] = i;  // ใกล้กันเกิน: เก็บยอดที่สูงกว่า
      continue;
    }
    if (np < MAX_PEAKS) peaks[np++] = i;
  }
  if (np < 4) return 0;  // ต้องมีอย่างน้อย 3 ช่วง

  // ใช้ median ของช่วงห่างระหว่างยอด (ทนยอดหลอก 1–2 อัน) แล้วเช็คว่าจังหวะสม่ำเสมอพอ
  int gaps[MAX_PEAKS];
  const int ng = np - 1;
  for (int i = 0; i < ng; i++) gaps[i] = peaks[i + 1] - peaks[i];
  for (int i = 1; i < ng; i++) {  // insertion sort (ไม่กี่ตัว)
    const int g = gaps[i];
    int j = i - 1;
    while (j >= 0 && gaps[j] > g) { gaps[j + 1] = gaps[j]; j--; }
    gaps[j + 1] = g;
  }
  const float med = (ng % 2) ? gaps[ng / 2] : 0.5f * (gaps[ng / 2 - 1] + gaps[ng / 2]);
  int close = 0;
  for (int i = 0; i < ng; i++) close += fabsf(gaps[i] - med) <= 0.25f * med ? 1 : 0;
  if (close * 3 < ng * 2) return 0;  // ช่วงห่างกระจัดกระจาย (ขยับแขน) → ไม่เชื่อ

  const float bpm = 60.0f * PULSE_FS_HZ / med;
  if (bpm < 40 || bpm > 200) return 0;
  return (uint8_t)(bpm + 0.5f);
}

// SpO2 จาก ratio-of-ratios R = (AC_red/DC_red) / (AC_ir/DC_ir) + สูตรประมาณของ Maxim (ไม่ได้ calibrate)
uint8_t spo2_from(float ac_red, float dc_red, float ac_ir, float dc_ir) {
  if (dc_red <= 0 || dc_ir <= 0 || ac_ir <= 0) return 0;
  const float r = (ac_red / dc_red) / (ac_ir / dc_ir);
  if (r < 0.3f || r > 1.3f) return 0;  // นอกช่วงที่สูตรใช้ได้
  float s = -45.060f * r * r + 30.354f * r + 94.845f;
  if (s > 100) s = 100;
  if (s < 70) return 0;
  return (uint8_t)(s + 0.5f);
}

void finish() {
  const int n = count;
  last = {0, 0, false};
  if (n >= PULSE_WINDOW_SAMPLES / 2) {
    const float dc_ir = to_ac(ir_buf, n);
    const float dc_red = to_ac(red_buf, n);
    last.contact = dc_ir >= PULSE_IR_MIN;
    if (last.contact) {
      last.hr = heart_rate(ir_buf, n);
      // SpO2 เชื่อได้เฉพาะตอนจับจังหวะหัวใจได้ (สัญญาณดีพอ)
      if (last.hr) last.spo2 = spo2_from(rms(red_buf, n), dc_red, rms(ir_buf, n), dc_ir);
    }
  }
  phase = Phase::IDLE;
  write_reg(REG_MODE_CONFIG, MODE_SHDN | MODE_SPO2);
  LOG("[pulse] contact=%d hr=%u spo2=%u (n=%d)\n", last.contact, last.hr, last.spo2, n);
}

}  // namespace

bool pulse_begin() {
  phase = Phase::IDLE;
  uint8_t id = 0;
  sensor_ok = read_regs(REG_PART_ID, &id, 1) && id == PART_ID;
  if (!sensor_ok) return false;

  write_reg(REG_MODE_CONFIG, MODE_RESET);
  for (int i = 0; i < 20; i++) {  // รอ reset bit เคลียร์ตัวเอง
    uint8_t m = MODE_RESET;
    if (read_regs(REG_MODE_CONFIG, &m, 1) && !(m & MODE_RESET)) break;
    delay(5);
  }
  sensor_ok = write_reg(REG_INTR_EN1, 0)
           && write_reg(REG_INTR_EN2, 0)
           && write_reg(REG_FIFO_CONFIG, FIFO_CFG)
           && write_reg(REG_SPO2_CONFIG, SPO2_CFG)
           && write_reg(REG_LED1_PA, PULSE_LED_RED)
           && write_reg(REG_LED2_PA, PULSE_LED_IR)
           && write_reg(REG_MODE_CONFIG, MODE_SHDN | MODE_SPO2);  // ปิดไว้ก่อน เปิดตอนวัด
  return sensor_ok;
}

bool pulse_ok() { return sensor_ok; }

void pulse_start() {
  if (!sensor_ok || phase != Phase::IDLE) return;
  clear_fifo();
  if (!write_reg(REG_MODE_CONFIG, MODE_SPO2)) {
    sensor_ok = false;
    last = {0, 0, false};
    return;
  }
  phase = Phase::SETTLING;
  settle_left = PULSE_SETTLE_SAMPLES;
  count = 0;
  last_sample_ms = millis();
}

bool pulse_update() {
  if (phase == Phase::IDLE) return false;
  const int got = drain_fifo();
  const uint32_t now = millis();
  if (got > 0) last_sample_ms = now;

  if (got < 0 || now - last_sample_ms > STALL_MS) {  // หลุดกลางทาง
    LOG("[pulse] sensor lost\n");
    sensor_ok = false;
    phase = Phase::IDLE;
    last = {0, 0, false};
    return true;
  }
  if (phase == Phase::COLLECTING && count >= PULSE_WINDOW_SAMPLES) {
    finish();
    return true;
  }
  return false;
}

bool pulse_busy() { return phase != Phase::IDLE; }

PulseResult pulse_last() { return last; }

void pulse_sleep() {
  phase = Phase::IDLE;
  if (sensor_ok) write_reg(REG_MODE_CONFIG, MODE_SHDN | MODE_SPO2);
}
