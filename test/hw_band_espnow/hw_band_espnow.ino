// เดโมกำไล (XIAO ESP32-C3) ส่งค่าเซนเซอร์จริงผ่าน ESP-NOW ไปกล่อง (คู่กับ test/hw_netpie_box)
// MOTION (MPU6050) ทุก 0.5 วิ, STATUS (MAX30102) ทุก 5 วิ, ได้ CMD_PILL_REMIND = มอเตอร์สั่น
#include <WiFi.h>
#include <Wire.h>
#include "MAX30105.h"
#include "heartRate.h"
#include "protocol.h"
#include "espnow_link.h"

#define PIN_SDA    D4
#define PIN_SCL    D5
#define PIN_MOTOR  D10
#define MPU_ADDR   0x68

// MAC ของกล่อง (DevKit) อ่านด้วย esptool read-mac
uint8_t BOX_MAC[6] = {0xA4, 0xF0, 0x0F, 0x65, 0x80, 0x6C};

MAX30105 pulse;
bool maxOk = false, mpuOk = false;
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
  Wire.begin(PIN_SDA, PIN_SCL);
  mpuOk = mpuBegin();
  maxOk = pulse.begin(Wire, I2C_SPEED_FAST);
  if (maxOk) {
    pulse.setup();
    pulse.setPulseAmplitudeRed(0x0A);
  }
  Serial.printf("MPU6050 %s, MAX30102 %s\n", mpuOk ? "OK" : "NOT FOUND", maxOk ? "OK" : "NOT FOUND");

  WiFi.mode(WIFI_STA);
  if (!linkBegin(BOX_MAC)) Serial.println("ESP-NOW init failed");
}

void loop() {
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

  static unsigned long lastMotion = 0;
  if (mpuOk && millis() - lastMotion >= 500) {
    lastMotion = millis();
    int16_t v[7];
    mpuRead(v);
    MotionPacket m;
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
    StatusPacket p;
    p.hr = finger ? (uint8_t)bpm : 0;
    p.spo2 = 0;  // ยังไม่ได้คำนวณ SpO2 ในเดโมนี้
    p.batt_pct = 100;
    p.batt_mv = 0;
    p.flags = (maxOk ? FLAG_MAX30102_OK : 0) | (mpuOk ? FLAG_MPU6050_OK : 0);
    bool ok = linkSend(STATUS, &p, sizeof(p), 1);
    Serial.printf("STATUS hr=%u ack=%s\n", p.hr, ok ? "yes" : "no");
    if (ok && (linkLastCmd() & CMD_PILL_REMIND)) {
      Serial.println("-> สั่นเตือนยา");
      for (int i = 0; i < 3; i++) {
        vibrate(300);
        delay(200);
      }
    }
  }
}
