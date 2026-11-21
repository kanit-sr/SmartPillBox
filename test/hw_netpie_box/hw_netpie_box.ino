// เดโมกล่องยา (ESP32 DevKit) ส่งค่าเซนเซอร์จริงขึ้น NETPIE shadow ของ spb_box ทุก 5 วิ
// copy secrets.example.h เป็น secrets.h แล้วใส่ Wi-Fi + NETPIE (secrets.h ไม่ขึ้น git)
// ปุ่มเคลียร์บน dashboard (@msg/spb/cmd clear_alarm) = buzzer บี๊บ + ไฟเขียว 1 วิ
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <DHT.h>
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
}

void loop() {
  connectAll();
  mqtt.loop();

  static unsigned long last = 0;
  if (millis() - last < 5000) return;
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

  char buf[256];
  size_t n = serializeJson(doc, buf);
  bool ok = mqtt.publish("@shadow/data/update", buf, n);
  Serial.printf("%s %s\n", ok ? "sent" : "FAILED", buf);
}
