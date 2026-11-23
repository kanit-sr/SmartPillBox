// เดโมกล่องยา (ESP32 DevKit) ส่งค่าเซนเซอร์จริงขึ้น NETPIE shadow ของ pillbox ทุก 1 วิ
// รวมค่าจากกำไล (test/hw_band_espnow) ที่ส่งมาทาง ESP-NOW: hr_1, spo2_1, acc_*_1, gyro_*_1
// FALL_CONFIRMED / SOS จากกำไล = box_state fall_alarm / sos, ไฟแดง + buzzer ดังเป็นจังหวะ
// copy secrets.example.h เป็น secrets.h แล้วใส่ Wi-Fi + NETPIE (secrets.h ไม่ขึ้น git)
// คำสั่งจาก dashboard (@msg/spb/cmd):
//   clear_alarm = หยุด alarm, บี๊บ + ไฟเขียว 1 วิ, แจ้งกำไลด้วย CMD_FALL_CLEARED
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
#define PIN_LED_R   21
#define PIN_LED_G   22
#define PERSON_CM   50  // ใกล้กว่านี้ = มีคนหน้ากล่อง

DHT dht(PIN_DHT, DHT11);
WiFiClient net;
PubSubClient mqtt(net);

// ค่าล่าสุดจากกำไล (เขียนจาก linkLoop อ่านตอน publish)
MotionPacket band = {};
uint8_t bandHr = 0, bandSpo2 = 0;
unsigned long bandLastSeen = 0;
bool haveMotion = false;

// alarm จากกำไล: "" = ไม่มี, "fall_alarm", "sos"
String alarmState = "";
String pendingEvent = "";  // ส่งขึ้น last_event รอบ publish ถัดไป

void raiseAlarm(const char* state, const char* ev, uint8_t hr, uint8_t spo2) {
  bandHr = hr;
  bandSpo2 = spo2;
  if (alarmState == state) return;
  alarmState = state;
  pendingEvent = ev;
  Serial.printf("[band] %s hr=%u spo2=%u\n", ev, hr, spo2);
}

void onBandPacket(uint8_t type, const uint8_t* data, int len, bool isDup) {
  bandLastSeen = millis();
  if (type == MOTION && len >= (int)sizeof(MotionPacket)) {
    memcpy(&band, data, sizeof(MotionPacket));
    haveMotion = true;
  } else if (type == STATUS && len >= (int)sizeof(StatusPacket)) {
    bandHr = ((const StatusPacket*)data)->hr;
    bandSpo2 = ((const StatusPacket*)data)->spo2;
  } else if (type == FALL_CONFIRMED && !isDup && len >= (int)sizeof(FallConfirmedPacket)) {
    const FallConfirmedPacket* p = (const FallConfirmedPacket*)data;
    raiseAlarm("fall_alarm", "fall_confirmed", p->hr, p->spo2);
  } else if (type == SOS && !isDup && len >= (int)sizeof(SosPacket)) {
    const SosPacket* p = (const SosPacket*)data;
    raiseAlarm("sos", "sos", p->hr, p->spo2);
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
    if (alarmState.length()) {
      alarmState = "";
      pendingEvent = "fall_acknowledged";
      linkSetAckCmd(CMD_FALL_CLEARED);
    }
    digitalWrite(PIN_LED_R, LOW);
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
  pinMode(PIN_LED_R, OUTPUT);
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

  // alarm: ไฟแดงกะพริบ + buzzer ดังเป็นจังหวะ จนกว่าจะเคลียร์จาก dashboard
  bool on = alarmState.length() && (millis() / 500) % 2;
  digitalWrite(PIN_LED_R, on);
  digitalWrite(PIN_BUZZER, on);

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
  d["box_state"] = alarmState.length() ? alarmState : "idle";
  d["alarm_node"] = alarmState.length() ? 1 : 0;
  if (pendingEvent.length()) {
    JsonObject ev = d["last_event"].to<JsonObject>();
    ev["ev"] = pendingEvent;
    ev["node"] = 1;
    ev["hr"] = bandHr;
    pendingEvent = "";
  }

  bool bandOnline = bandLastSeen && millis() - bandLastSeen < 10000;
  d["node_online_1"] = bandOnline;
  if (bandOnline) {
    d["hr_1"] = bandHr;
    d["spo2_1"] = bandSpo2;
    if (haveMotion) {
      d["acc_x_1"] = band.ax / 1000.0;
      d["acc_y_1"] = band.ay / 1000.0;
      d["acc_z_1"] = band.az / 1000.0;
      d["gyro_x_1"] = band.gx;
      d["gyro_y_1"] = band.gy;
      d["gyro_z_1"] = band.gz;
    }
  }

  char buf[480];
  size_t n = serializeJson(doc, buf);
  bool ok = mqtt.publish("@shadow/data/update", buf, n);
  Serial.printf("%s %s\n", ok ? "sent" : "FAILED", buf);
}
