// hw_line_box — ทดสอบส่ง LINE จากกล่องยา (ESP32 DevKit) ด้วยข้อมูลจำลองเท่านั้น
// ใช้ line_notify.cpp ชุดเดียวกับ 3-gateway-cloud/gateway (คัดลอกมา)
// secrets.h: คัดลอกจาก gateway/secrets.h (gitignored ห้าม commit)
//
// เปิด Serial Monitor 115200 แล้วพิมพ์ตัวอักษร 1 ตัว:
//   t = ข้อความทดสอบธรรมดา
//   f = fall_confirmed (ล้ม)      s = sos
//   p = pill_missed (ลืมกินยา)   b = low_battery
// ไม่ส่งอะไรเองตอนบูต กันสแปมเวลารีเซ็ตบอร์ด
#include <WiFi.h>
#include <time.h>
#include "secrets.h"
#include "line_notify.h"

static int counter = 0;

static void sendMock(const char* ev, const char* d) {
  time_t now = time(nullptr);
  char text[300];
  if (!lineTextForEvent(ev, now, d, now, text, sizeof(text))) {
    Serial.printf("event %s ไม่ต้องแจ้ง LINE\n", ev);
    return;
  }
  char full[340];
  snprintf(full, sizeof(full), "[ทดสอบ ข้อมูลจำลอง] %s", text);
  char key[32];
  snprintf(key, sizeof(key), "%ld_%d", (long)now, ++counter);
  Serial.printf("ส่ง: %s\n", full);
  LineResult r = lineSend(full, key);
  Serial.printf("ผล: %s\n", r == LINE_OK ? "OK" : r == LINE_RETRY ? "RETRY" : "DROP");
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n[hw_line_box] กำลังต่อ WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.printf("\nWiFi OK IP %s\n", WiFi.localIP().toString().c_str());
  configTime(7 * 3600, 0, "pool.ntp.org", "time.google.com");  // เวลาไทย
  while (time(nullptr) < 1700000000) delay(200);
  Serial.println("NTP OK  พิมพ์ t/f/s/p/b เพื่อส่ง LINE");
}

void loop() {
  if (!Serial.available()) return;
  char c = Serial.read();
  switch (c) {
    case 't': {
      LineResult r = lineSend("[ทดสอบ] สวัสดีจากกล่องยา SmartPillBox");
      Serial.printf("ผล: %s\n", r == LINE_OK ? "OK" : r == LINE_RETRY ? "RETRY" : "DROP");
      break;
    }
    case 'f': sendMock("fall_confirmed", "{\"hr\":112,\"spo2\":95}"); break;
    case 's': sendMock("sos", "{\"hr\":98,\"spo2\":97}"); break;
    case 'p': sendMock("pill_missed", "{\"slot\":\"เช้า\"}"); break;
    case 'b': sendMock("low_battery", "{\"batt\":15}"); break;
  }
}
