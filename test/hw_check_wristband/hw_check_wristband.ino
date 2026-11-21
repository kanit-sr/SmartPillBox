// เทสฮาร์ดแวร์กำไล (XIAO ESP32-C3) — แยกจาก sensor_node.ino ใช้เช็คว่าต่อสายถูก
// พิมพ์ค่า MPU6050 + MAX30102 + ปุ่ม ทุก 0.5 วิ / กดปุ่มหรือพิมพ์ m = มอเตอร์สั่น 300ms
#include <Wire.h>
#include "MAX30105.h"
#include "heartRate.h"

#define PIN_SDA    D4
#define PIN_SCL    D5
#define PIN_BUTTON D1
#define PIN_MOTOR  D10
#define MPU_ADDR   0x68

MAX30105 pulse;
bool maxOk = false, mpuOk = false;
float bpm = 0;
unsigned long lastBeat = 0;

int i2cScan(int sda, int scl, const char* label) {
  Wire.end();
  Wire.begin(sda, scl);
  int found = 0;
  Serial.printf("I2C scan (%s):", label);
  for (uint8_t a = 1; a < 127; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      Serial.printf(" 0x%02X", a);
      found++;
    }
  }
  Serial.println(found ? "  (MPU6050=0x68, MAX30102=0x57)" : " none");
  return found;
}

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
  delay(1500);  // รอ USB CDC ให้ monitor ต่อทัน
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  pinMode(PIN_MOTOR, OUTPUT);
  Serial.println("\n== hw_check_wristband ==");
  // ไม่เจออะไรเลย ลองสลับ SDA/SCL ดูเผื่อต่อกลับด้าน
  if (i2cScan(PIN_SDA, PIN_SCL, "SDA=D4 SCL=D5") == 0 &&
      i2cScan(PIN_SCL, PIN_SDA, "SWAPPED SDA=D5 SCL=D4") > 0) {
    Serial.println("!! SDA/SCL ต่อสลับกัน ใช้แบบสลับไปก่อน");
  } else {
    Wire.end();
    Wire.begin(PIN_SDA, PIN_SCL);
  }

  mpuOk = mpuBegin();
  maxOk = pulse.begin(Wire, I2C_SPEED_FAST);
  if (maxOk) {
    pulse.setup();  // ค่า default ของ SparkFun
    pulse.setPulseAmplitudeRed(0x0A);
  }
  Serial.printf("MPU6050 %s, MAX30102 %s\n", mpuOk ? "OK" : "NOT FOUND", maxOk ? "OK" : "NOT FOUND");
  vibrate(300);
}

void loop() {
  // ยังไม่เจอเซนเซอร์: สแกนซ้ำทุก 1 วิ ให้ขยับ/กดสายแล้วดูผลสดได้
  if (!mpuOk || !maxOk) {
    static unsigned long lastScan = 0;
    if (millis() - lastScan < 1000) return;
    lastScan = millis();
    i2cScan(PIN_SDA, PIN_SCL, "rescan");
    if (!mpuOk) mpuOk = mpuBegin();
    if (!maxOk && (maxOk = pulse.begin(Wire, I2C_SPEED_FAST))) {
      pulse.setup();
      pulse.setPulseAmplitudeRed(0x0A);
    }
    if (mpuOk && maxOk) Serial.println("== เจอครบทั้งสองตัวแล้ว ==");
    return;
  }

  // MAX30102 ต้องอ่านถี่ๆ ถึงจะจับจังหวะหัวใจได้
  long ir = 0, red = 0;
  if (maxOk) {
    ir = pulse.getIR();
    red = pulse.getRed();
    if (checkForBeat(ir)) {
      unsigned long now = millis();
      float b = 60000.0 / (now - lastBeat);
      lastBeat = now;
      if (b > 30 && b < 220) bpm = bpm ? bpm * 0.7 + b * 0.3 : b;
    }
  }

  static bool lastBtn = false;
  bool btn = digitalRead(PIN_BUTTON) == LOW;
  if (btn && !lastBtn) {
    Serial.println("button PRESSED -> vibrate");
    vibrate(300);
  }
  lastBtn = btn;
  if (Serial.available() && Serial.read() == 'm') vibrate(300);

  static unsigned long last = 0;
  if (millis() - last < 500) return;
  last = millis();

  int16_t v[7] = {0};
  if (mpuOk) mpuRead(v);
  Serial.printf("acc=%.2f,%.2f,%.2fg gyro=%d,%d,%d | ir=%ld red=%ld finger=%s bpm=%.0f | button=%s\n",
                v[0] / 16384.0, v[1] / 16384.0, v[2] / 16384.0,
                v[4] / 131, v[5] / 131, v[6] / 131,
                ir, red, ir > 50000 ? "yes" : "no", ir > 50000 ? bpm : 0.0,
                btn ? "PRESSED" : "up");
}
