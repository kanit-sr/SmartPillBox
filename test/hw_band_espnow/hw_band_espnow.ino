// เดโมกำไล (XIAO ESP32-C3) ส่งค่าเซนเซอร์จริงผ่าน ESP-NOW ไปกล่อง (คู่กับ test/hw_netpie_box)
// MOTION (MPU6050) ทุก 0.5 วิ, STATUS (MAX30102) ทุก 5 วิ, ได้ CMD_PILL_REMIND = มอเตอร์สั่น
// MOCK_SENSORS 1 = ไม่อ่านเซนเซอร์ ส่งชีพจร/acc จำลองแทน (เทสเส้นทาง กำไล→กล่อง→NETPIE)
//   เลือกสถานการณ์โดยพิมพ์ใน Serial (COM4):
//     n = ปกติ นั่งนิ่ง   w = เดิน   h = ชีพจรสูง
//     f = ล้ม (ส่ง FALL_CONFIRMED แล้วนอนนิ่ง)   s = SOS (หรือกดปุ่มกำไล)
#define MOCK_SENSORS 1

#include <WiFi.h>
#include <Wire.h>
#include "MAX30105.h"
#include "heartRate.h"
#include "protocol.h"
#include "espnow_link.h"

#define PIN_SDA    D4
#define PIN_SCL    D5
#define PIN_MOTOR  D10
#define PIN_BUTTON D1
#define MPU_ADDR   0x68

// MAC ของกล่อง (DevKit) อ่านด้วย esptool read-mac
uint8_t BOX_MAC[6] = {0xA4, 0xF0, 0x0F, 0x65, 0x80, 0x6C};

MAX30105 pulse;
bool maxOk = false, mpuOk = false;
char scenario = 'n';           // สถานการณ์จำลองปัจจุบัน
unsigned long fallAt = 0;      // เวลาที่ล้ม (ไว้คำนวณ age_ms)
float bpm = 0;
unsigned long lastBeat = 0;
long lastIr = 0;

bool mpuBegin() {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);  // PWR_MGMT_1: ปลุกจาก sleep
  Wire.write(0);
  return Wire.endTransmission() == 0;
}

void mpuRead(int16_t* v) {  // ax ay az temp gx gy gz
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, 14);
  for (int i = 0; i < 7; i++) v[i] = (Wire.read() << 8) | Wire.read();
}

void vibrate(int ms) {
  digitalWrite(PIN_MOTOR, HIGH);
  delay(ms);
  digitalWrite(PIN_MOTOR, LOW);
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_MOTOR, OUTPUT);
#if MOCK_SENSORS
  mpuOk = true;
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  Serial.println("MOCK_SENSORS: พิมพ์ n=ปกติ w=เดิน h=ชีพจรสูง f=ล้ม s=SOS");
#else
  Wire.begin(PIN_SDA, PIN_SCL);
  mpuOk = mpuBegin();
  maxOk = pulse.begin(Wire, I2C_SPEED_FAST);
  if (maxOk) {
    pulse.setup();
    pulse.setPulseAmplitudeRed(0x0A);
  }
  Serial.printf("MPU6050 %s, MAX30102 %s\n", mpuOk ? "OK" : "NOT FOUND", maxOk ? "OK" : "NOT FOUND");
#endif

  WiFi.mode(WIFI_STA);
  if (!linkBegin(BOX_MAC)) Serial.println("ESP-NOW init failed");
}

uint8_t mockHr() {
  if (scenario == 'h') return 125 + random(0, 10);
  if (scenario == 'w') return 95 + random(0, 10);
  if (scenario == 'f' || scenario == 's') return 110 + random(0, 8);
  return 72 + random(0, 9);
}

// ส่งเหตุการณ์ด่วน: retry จนได้ ACK (สูงสุด 10 รอบ)
void sendEvent(char ev) {
  uint8_t hr = mockHr();
  bool ok;
  if (ev == 'f') {
    FallConfirmedPacket p = {};
    p.hr = hr;
    p.spo2 = 94;
    p.conf = 91;
    p.age_ms = millis() - fallAt;
    p.batt_pct = 100;
    ok = linkSend(FALL_CONFIRMED, &p, sizeof(p), 10);
  } else {
    SosPacket p = {};
    p.hr = hr;
    p.spo2 = 96;
    p.age_ms = 0;
    p.batt_pct = 100;
    ok = linkSend(SOS, &p, sizeof(p), 10);
  }
  Serial.printf("%s hr=%u ack=%s\n", ev == 'f' ? "FALL_CONFIRMED" : "SOS", hr, ok ? "yes" : "no");
}

void loop() {
#if MOCK_SENSORS
  // เลือกสถานการณ์จาก Serial หรือปุ่มกำไล (กด = SOS)
  char c = Serial.available() ? Serial.read() : 0;
  static bool lastBtn = false;
  bool btn = digitalRead(PIN_BUTTON) == LOW;
  if (btn && !lastBtn) c = 's';
  lastBtn = btn;
  if (c == 'n' || c == 'w' || c == 'h' || c == 'f' || c == 's') {
    scenario = c;
    Serial.printf("== scenario: %c ==\n", c);
    if (c == 'f') {
      fallAt = millis();
      sendEvent('f');
    } else if (c == 's') {
      sendEvent('s');
    }
  }
#endif

  // MAX30102 ต้องอ่านถี่ๆ ถึงจะจับจังหวะหัวใจได้
  if (maxOk) {
    lastIr = pulse.getIR();
    if (checkForBeat(lastIr)) {
      unsigned long now = millis();
      float b = 60000.0 / (now - lastBeat);
      lastBeat = now;
      if (b > 30 && b < 220) bpm = bpm ? bpm * 0.7 + b * 0.3 : b;
    }
  }

#if !MOCK_SENSORS
  // ขาเซนเซอร์ยังหลวม: ลองหาใหม่ทุก 2 วิ จนกว่าจะเจอ
  static unsigned long lastRetry = 0;
  if ((!mpuOk || !maxOk) && millis() - lastRetry >= 2000) {
    lastRetry = millis();
    if (!mpuOk && (mpuOk = mpuBegin())) Serial.println("MPU6050 เจอแล้ว");
    if (!maxOk && (maxOk = pulse.begin(Wire, I2C_SPEED_FAST))) {
      pulse.setup();
      pulse.setPulseAmplitudeRed(0x0A);
      Serial.println("MAX30102 เจอแล้ว");
    }
  }
#endif

  static unsigned long lastMotion = 0;
  if (mpuOk && millis() - lastMotion >= 500) {
    lastMotion = millis();
    int16_t v[7];
#if MOCK_SENSORS
    float t = millis() / 1000.0;
    float sway = random(-20, 21) / 1000.0;  // สั่นเล็กน้อยให้ดูเป็นค่าจริง
    if (scenario == 'w') {         // แกว่งแขนตามจังหวะเดิน ~1 ก้าว/วิ
      v[0] = 16384 * 0.5 * sin(6.3 * t);
      v[1] = 16384 * 0.3 * cos(6.3 * t);
      v[2] = 16384 * (0.9 + 0.2 * sin(12.6 * t));
      v[4] = 131 * 120 * cos(6.3 * t);
      v[5] = 131 * 40 * sin(6.3 * t);
      v[6] = 131 * 10;
    } else if (scenario == 'f' && millis() - fallAt < 1000) {  // จังหวะกระแทก
      v[0] = 16384 * 1.95;  // ช่วง ±2g ของ MPU6050 เกินนี้ int16 ล้น
      v[1] = 16384 * -1.8;
      v[2] = 16384 * 1.5;
      v[4] = 131 * 240;
      v[5] = 131 * -200;
      v[6] = 131 * 150;
    } else if (scenario == 'f') {  // นอนตะแคงนิ่ง: แรงโน้มถ่วงย้ายไปแกน X
      v[0] = 16384 * (0.98 + sway);
      v[1] = 16384 * sway;
      v[2] = 16384 * 0.1;
      v[4] = v[5] = v[6] = 0;
    } else {                       // นั่งนิ่ง ข้อมือขยับนิดหน่อย
      v[0] = 16384 * (0.05 * sin(t) + sway);
      v[1] = 16384 * (0.1 * cos(t) + sway);
      v[2] = 16384 * 0.98;
      v[4] = 131 * 5 * cos(t);
      v[5] = 131 * -3 * sin(t);
      v[6] = 0;
    }
#else
    mpuRead(v);
#endif
    MotionPacket m = {};
    m.ax = (int32_t)v[0] * 1000 / 16384;
    m.ay = (int32_t)v[1] * 1000 / 16384;
    m.az = (int32_t)v[2] * 1000 / 16384;
    m.gx = v[4] / 131;
    m.gy = v[5] / 131;
    m.gz = v[6] / 131;
    bool ok = linkSend(MOTION, &m, sizeof(m), 1);
    Serial.printf("MOTION acc=%d,%d,%dmg gyro=%d,%d,%d ack=%s ch=%u\n",
                  m.ax, m.ay, m.az, m.gx, m.gy, m.gz, ok ? "yes" : "no", linkChannel());
  }

  static unsigned long lastStatus = 0;
  if (millis() - lastStatus >= 5000) {
    lastStatus = millis();
    bool finger = lastIr > 50000;
    StatusPacket p = {};
    p.hr = finger ? (uint8_t)bpm : 0;
    p.spo2 = 0;  // ยังไม่ได้คำนวณ SpO2 จากเซนเซอร์จริงในเดโมนี้
#if MOCK_SENSORS
    p.hr = mockHr();
    p.spo2 = scenario == 'f' ? 94 : 97 + random(0, 3);
#endif
    p.batt_pct = 100;
    p.batt_mv = 0;
    p.flags = (maxOk ? FLAG_MAX30102_OK : 0) | (mpuOk ? FLAG_MPU6050_OK : 0);
    bool ok = linkSend(STATUS, &p, sizeof(p), 1);
    Serial.printf("STATUS hr=%u ack=%s\n", p.hr, ok ? "yes" : "no");
    if (ok && (linkLastCmd() & CMD_FALL_CLEARED)) {
      Serial.println("-> alarm ถูกเคลียร์จาก dashboard");
      scenario = 'n';
      vibrate(150);
    }
    if (ok && (linkLastCmd() & CMD_PILL_REMIND)) {
      Serial.println("-> สั่นเตือนยา");
      for (int i = 0; i < 3; i++) {
        vibrate(300);
        delay(200);
      }
    }
  }
}
