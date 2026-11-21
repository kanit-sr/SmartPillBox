// เดโมกล่องยา (ESP32 DevKit) ส่งค่าเซนเซอร์จริงขึ้น NETPIE shadow ทุก 1 วิ
// รวมค่าจากกำไล (test/hw_band_espnow) ที่ส่งมาทาง ESP-NOW: hr_1, acc_*_1, gyro_*_1
// copy secrets.example.h เป็น secrets.h แล้วใส่ Wi-Fi + NETPIE (secrets.h ไม่ขึ้น git)
// คำสั่งจาก dashboard (@msg/spb/cmd):
//   clear_alarm = buzzer บี๊บ + ไฟเขียว 1 วิ
//   pill_remind = ฝากคำสั่งสั่นเตือนยาไปกับ ACK ถัดไปของกำไล (พิมพ์ p ใน Serial ก็ได้)
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include "protocol.h"
#include "espnow_link.h"
#include "secrets.h"

#define PIN_REED    4
#define PIN_BUZZER  17  // TX2
#define PIN_TRIG    5
#define PIN_ECHO    18
#define PIN_DHT     19
#define PIN_LED_G   22
#define PERSON_CM   50  // ใกล้กว่านี้ = มีคนหน้ากล่อง

DHT dht(PIN_DHT, DHT11);
WiFiClient net;
PubSubClient mqtt(net);

// ค่าล่าสุดจากกำไล (เขียนจาก linkLoop อ่านตอน publish)
MotionPacket band = {};
uint8_t bandHr = 0;
unsigned long bandLastSeen = 0;
bool haveMotion = false;

void onBandPacket(uint8_t type, const uint8_t* data, int len, bool isDup) {
  bandLastSeen = millis();
  if (type == MOTION && len >= (int)sizeof(MotionPacket)) {
    memcpy(&band, data, sizeof(MotionPacket));
    haveMotion = true;
  } else if (type == STATUS && len >= (int)sizeof(StatusPacket)) {
    bandHr = ((const StatusPacket*)data)->hr;
  }
}

float readDistanceCm() {
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);
  unsigned long us = pulseIn(PIN_ECHO, HIGH, 30000);
  return us ? us / 58.0 : -1;
}

void onMessage(char* topic, byte* payload, unsigned int len) {
  JsonDocument doc;
  if (deserializeJson(doc, payload, len)) return;
  if (String(topic) == "@msg/spb/cmd" && doc["cmd"] == "clear_alarm") {
    Serial.println("[netpie] ได้คำสั่ง clear_alarm");
    digitalWrite(PIN_LED_G, HIGH);
    digitalWrite(PIN_BUZZER, HIGH);
    delay(200);
    digitalWrite(PIN_BUZZER, LOW);
    delay(800);
    digitalWrite(PIN_LED_G, LOW);
  } else if (String(topic) == "@msg/spb/cmd" && doc["cmd"] == "pill_remind") {
    Serial.println("[netpie] ได้คำสั่ง pill_remind");
    linkSetAckCmd(CMD_PILL_REMIND);
  }
}

void connectAll() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.printf("[wifi] ต่อ %s", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    Serial.printf(" OK ip=%s\n", WiFi.localIP().toString().c_str());
  }
  while (!mqtt.connected()) {
    if (mqtt.connect(NETPIE_CLIENT_ID, NETPIE_TOKEN, NETPIE_SECRET)) {
      Serial.println("[netpie] ต่อ NETPIE แล้ว");
      mqtt.subscribe("@msg/spb/cmd");
    } else {
      Serial.printf("[netpie] ต่อ MQTT ไม่ได้ state=%d\n", mqtt.state());
      delay(3000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_REED, INPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  pinMode(PIN_LED_G, OUTPUT);
  dht.begin();
  WiFi.mode(WIFI_STA);
  mqtt.setServer("mqtt.netpie.io", 1883);
  mqtt.setCallback(onMessage);
  mqtt.setBufferSize(512);
  connectAll();  // ต่อ Wi-Fi ก่อน ESP-NOW จะได้อยู่ channel เดียวกับ router (กำไล scan หาเอง)
  linkBegin(nullptr);
  linkOnReceive(onBandPacket);
  Serial.printf("[espnow] รอกำไลที่ channel %u\n", linkChannel());
}

void loop() {
  connectAll();
  mqtt.loop();
  linkLoop();
  if (Serial.available() && Serial.read() == 'p') linkSetAckCmd(CMD_PILL_REMIND);

  static unsigned long last = 0;
  if (millis() - last < 1000) return;
  last = millis();

  JsonDocument doc;
  JsonObject d = doc["data"].to<JsonObject>();
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (!isnan(t)) d["temp"] = round(t * 10) / 10.0;
  if (!isnan(h)) d["hum"] = round(h);
  float dist = readDistanceCm();
  d["lid_open"] = digitalRead(PIN_REED) == HIGH;  // ไม่มีแม่เหล็ก = ฝาเปิด
  d["person"] = dist > 0 && dist < PERSON_CM;
  d["box_state"] = "idle";

  bool bandOnline = bandLastSeen && millis() - bandLastSeen < 10000;
  d["node_online_1"] = bandOnline;
  if (bandOnline) {
    d["hr_1"] = bandHr;
    if (haveMotion) {
      d["acc_x_1"] = band.ax / 1000.0;
      d["acc_y_1"] = band.ay / 1000.0;
      d["acc_z_1"] = band.az / 1000.0;
      d["gyro_x_1"] = band.gx;
      d["gyro_y_1"] = band.gy;
      d["gyro_z_1"] = band.gz;
    }
  }

  char buf[384];
  size_t n = serializeJson(doc, buf);
  bool ok = mqtt.publish("@shadow/data/update", buf, n);
  Serial.printf("%s %s\n", ok ? "sent" : "FAILED", buf);
}
