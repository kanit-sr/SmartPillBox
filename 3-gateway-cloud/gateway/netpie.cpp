#include "netpie.h"
#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include "store_forward.h"
#include "line_notify.h"
#include "secrets.h"

// ---------- ค่าตั้ง ----------
#define NETPIE_HOST        "mqtt.netpie.io"
#define NETPIE_PORT        1883
#define TOPIC_EVENT        "@msg/spb/event"
#define TOPIC_CMD          "@msg/spb/cmd"
#define TOPIC_SHADOW_UPD   "@shadow/data/update"
#define TOPIC_SHADOW_GET   "@shadow/data/get"
#define TOPIC_SHADOW_CHG   "@shadow/data/updated"
#define TOPIC_SHADOW_RESP  "@private/shadow/data/get/response"
#define LOCAL_TZ           "ICT-7"     // เวลาไทย
#define SHADOW_PUSH_MS     2000UL      // รวม field ที่เปลี่ยนแล้วส่งทุกเท่านี้
#define RECONNECT_MS       5000UL
#define FLUSH_BACKOFF_MS   5000UL      // ส่ง event ไม่ผ่าน รอเท่านี้ก่อนลองใหม่
#define MAX_SLOTS          8

// ---------- state ที่ loop() กับ task ใช้ร่วมกัน (ป้องกันด้วย mtx) ----------
static SemaphoreHandle_t mtx;
static QueueHandle_t evQueue;          // loop() → task (task เป็นคนเดียวที่แตะ store_forward)
struct EvItem { char json[SF_LEN]; };

static JsonDocument pending;           // field ที่รอส่ง
static JsonDocument last;              // ค่าล่าสุดทุก field (ไว้เทียบ + ส่งใหม่ตอนต่อเน็ตได้)
static char sched[MAX_SLOTS][6];
static int schedN = 0;
static bool schedChanged = false;
static bool clearAlarm = false;
static volatile bool online = false;
static uint32_t evId = 0;

// ---------- ฝั่ง task เท่านั้น ----------
static WiFiClient net;
static PubSubClient mqtt(net);

static bool timeValid() { return time(nullptr) > 1700000000; }
time_t netpieNow() { return timeValid() ? time(nullptr) : 0; }
bool netpieOnline() { return online; }

// ---------- Shadow ----------
template <typename T>
static void setField(const char* key, T value) {
  xSemaphoreTake(mtx, portMAX_DELAY);
  String k(key);
  if (last[k].isNull() || last[k].as<T>() != value) {
    last[k] = value;
    pending[k] = value;
  }
  xSemaphoreGive(mtx);
}
void netpieSet(const char* key, int value)   { setField<int>(key, value); }
void netpieSet(const char* key, float value) { setField<float>(key, roundf(value * 10) / 10); }
void netpieSet(const char* key, bool value)  { setField<bool>(key, value); }
void netpieSet(const char* key, const char* value) {
  xSemaphoreTake(mtx, portMAX_DELAY);
  String k(key), v(value);
  if (last[k].isNull() || v != last[k].as<const char*>()) {
    last[k] = v;
    pending[k] = v;
  }
  xSemaphoreGive(mtx);
}

// ---------- Event ----------
bool netpieEvent(const char* ev, const char* dJson, time_t ts) {
  EvItem it;
  if (ts == 0) ts = netpieNow();
  int n;
  if (ts > 0) {
    n = snprintf(it.json, SF_LEN, "{\"ev\":\"%s\",\"ts\":%ld,\"id\":%lu,\"d\":%s}",
                 ev, (long)ts, (unsigned long)++evId, dJson ? dJson : "{}");
  } else {
    // ยังไม่ได้เวลาจาก NTP: จำ millis ไว้ แล้วคำนวณ ts ตอนส่ง
    n = snprintf(it.json, SF_LEN, "{\"ev\":\"%s\",\"ts\":0,\"ms\":%lu,\"id\":%lu,\"d\":%s}",
                 ev, (unsigned long)millis(), (unsigned long)++evId, dJson ? dJson : "{}");
  }
  if (n >= SF_LEN) return false;
  return xQueueSend(evQueue, &it, 0) == pdTRUE;
}

// ---------- คำสั่งจาก dashboard ----------
bool netpieTakeClearAlarm() {
  xSemaphoreTake(mtx, portMAX_DELAY);
  bool v = clearAlarm;
  clearAlarm = false;
  xSemaphoreGive(mtx);
  return v;
}

int netpieSchedule(char out[][6], int maxSlots, bool* changed) {
  xSemaphoreTake(mtx, portMAX_DELAY);
  int n = min(schedN, maxSlots);
  for (int i = 0; i < n; i++) strcpy(out[i], sched[i]);
  if (changed) *changed = schedChanged;
  schedChanged = false;
  xSemaphoreGive(mtx);
  return n;
}

static bool validHHMM(const char* s) {
  return s && strlen(s) == 5 && isdigit(s[0]) && isdigit(s[1]) && s[2] == ':' &&
         isdigit(s[3]) && isdigit(s[4]) && atoi(s) < 24 && atoi(s + 3) < 60;
}

static void saveSchedule() {  // เรียกตอนถือ mtx อยู่
  String csv;
  for (int i = 0; i < schedN; i++) { if (i) csv += ','; csv += sched[i]; }
  Preferences p;
  p.begin("spb", false);
  p.putString("sched", csv);
  p.end();
}

static void loadSchedule() {
  Preferences p;
  p.begin("spb", true);
  String csv = p.getString("sched", "");
  p.end();
  schedN = 0;
  int start = 0;
  while (start < (int)csv.length() && schedN < MAX_SLOTS) {
    int comma = csv.indexOf(',', start);
    if (comma < 0) comma = csv.length();
    String s = csv.substring(start, comma);
    if (validHHMM(s.c_str())) strcpy(sched[schedN++], s.c_str());
    start = comma + 1;
  }
  schedChanged = schedN > 0;
}

static void applySchedule(JsonArrayConst arr) {
  char tmp[MAX_SLOTS][6];
  int n = 0;
  for (JsonVariantConst v : arr) {
    const char* s = v.as<const char*>();
    if (n < MAX_SLOTS && validHHMM(s)) strcpy(tmp[n++], s);
  }
  xSemaphoreTake(mtx, portMAX_DELAY);
  bool diff = n != schedN;
  for (int i = 0; !diff && i < n; i++) diff = strcmp(tmp[i], sched[i]) != 0;
  if (diff) {
    memcpy(sched, tmp, sizeof(tmp));
    schedN = n;
    schedChanged = true;
    saveSchedule();
    Serial.printf("[netpie] ตารางยาใหม่ %d มื้อ\n", n);
  }
  xSemaphoreGive(mtx);
}

// ---------- MQTT ----------
static void onMessage(char* topic, byte* payload, unsigned int len) {
  JsonDocument doc;
  if (deserializeJson(doc, payload, len)) return;

  if (!strcmp(topic, TOPIC_CMD)) {
    // dashboard: #["pillbox"].publishMsg("spb/cmd", JSON.stringify({cmd:"clear_alarm"}))
    if (doc["cmd"] == "clear_alarm") {
      xSemaphoreTake(mtx, portMAX_DELAY);
      clearAlarm = true;
      xSemaphoreGive(mtx);
      Serial.println("[netpie] ได้คำสั่ง clear_alarm จาก dashboard");
    }
  } else if (!strcmp(topic, TOPIC_SHADOW_RESP) || !strcmp(topic, TOPIC_SHADOW_CHG)) {
    // ตอนเปิดเครื่อง (get response) และตอน dashboard writeShadow("schedule", [...]) (updated)
    JsonArrayConst arr = doc["data"]["schedule"].as<JsonArrayConst>();
    if (!arr.isNull()) applySchedule(arr);
  }
}

static bool connectMqtt() {
  if (!mqtt.connect(NETPIE_CLIENT_ID, NETPIE_TOKEN, NETPIE_SECRET)) {
    Serial.printf("[netpie] ต่อ MQTT ไม่ได้ state=%d\n", mqtt.state());
    return false;
  }
  mqtt.subscribe(TOPIC_CMD);
  mqtt.subscribe(TOPIC_SHADOW_CHG);
  mqtt.subscribe("@private/#");
  mqtt.publish(TOPIC_SHADOW_GET, "");  // ขอ shadow ทั้งก้อน เพื่ออ่าน schedule ตอนเปิดเครื่อง
  // หลังต่อใหม่ ส่งค่าล่าสุดทุก field ขึ้นไปอีกรอบ
  xSemaphoreTake(mtx, portMAX_DELAY);
  for (JsonPair kv : last.as<JsonObject>()) pending[kv.key()] = kv.value();
  xSemaphoreGive(mtx);
  Serial.println("[netpie] ต่อ NETPIE แล้ว");
  return true;
}

static void pushShadow() {
  xSemaphoreTake(mtx, portMAX_DELAY);
  if (pending.size() == 0) { xSemaphoreGive(mtx); return; }
  JsonDocument msg;
  msg["data"] = pending;
  String body;
  serializeJson(msg, body);
  xSemaphoreGive(mtx);

  if (mqtt.publish(TOPIC_SHADOW_UPD, body.c_str())) {
    // ล้างเฉพาะ field ที่ยังเป็นค่าเดิม (ระหว่างส่ง loop() อาจแก้ field ใหม่เข้ามา)
    xSemaphoreTake(mtx, portMAX_DELAY);
    for (JsonPair kv : msg["data"].as<JsonObject>()) {
      if (pending[kv.key()] == kv.value()) pending.remove(kv.key());
    }
    xSemaphoreGive(mtx);
  }
}

// callback ของ sfFlush: publish event ไป @msg/spb/event แล้วส่ง LINE
static bool publishEvent(const char* json) {
  JsonDocument e;
  if (deserializeJson(e, json)) return true;  // JSON เสีย ทิ้งไปเลย
  time_t now = netpieNow();
  time_t ts = e["ts"] | (time_t)0;
  if (ts == 0) {
    if (!now) return false;  // รอ NTP ก่อน
    ts = now - (time_t)((millis() - (uint32_t)(e["ms"] | 0UL)) / 1000UL);
    e["ts"] = ts;
    e.remove("ms");
  }
  String body;
  serializeJson(e, body);
  if (!mqtt.publish(TOPIC_EVENT, body.c_str())) return false;

  // เก็บ event ล่าสุดไว้ใน shadow ด้วย ให้ dashboard โชว์ได้ (@msg ไม่ถูกเก็บในฐานข้อมูล)
  const char* ev = e["ev"] | "";
  netpieSet("last_ev", ev);
  netpieSet("last_ev_ts", (int)ts);

  char key[32], text[300];
  snprintf(key, sizeof(key), "%ld_%lu", (long)ts, (unsigned long)(e["id"] | 0UL));
  String d;
  serializeJson(e["d"], d);
  if (lineTextForEvent(ev, ts, d.c_str(), now, text, sizeof(text))) {
    // LINE ไม่ผ่านชั่วคราว → ส่งทั้ง event ใหม่ (retry key เดิม ญาติไม่ได้ LINE ซ้ำ)
    if (lineSend(text, key) == LINE_RETRY) return false;
  }
  return true;
}

// ---------- task (core 0) ----------
static void netpieTask(void*) {
  uint32_t tShadow = 0, tReconnect = 0, tFlushRetry = 0;
  bool ntpStarted = false;
  int lastQueue = -1;

  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(50));

    // ย้าย event ที่ loop() ส่งมา เข้าคิว store_forward (เน็ตหลุดก็เก็บไว้ได้)
    EvItem it;
    while (xQueueReceive(evQueue, &it, 0) == pdTRUE) sfPush(it.json);
    if (sfSize() != lastQueue) { lastQueue = sfSize(); netpieSet("queue", lastQueue); }

    if (WiFi.status() != WL_CONNECTED) { online = false; continue; }  // Wi-Fi ต่อใหม่เองอัตโนมัติ
    if (!ntpStarted) { configTzTime(LOCAL_TZ, "pool.ntp.org", "time.google.com"); ntpStarted = true; }

    if (!mqtt.connected()) {
      online = false;
      if (millis() - tReconnect < RECONNECT_MS) continue;
      tReconnect = millis();
      if (!connectMqtt()) continue;
    }
    online = true;
    mqtt.loop();

    uint32_t now = millis();
    if (now - tShadow >= SHADOW_PUSH_MS) { tShadow = now; pushShadow(); }

    if (sfSize() > 0 && (int32_t)(now - tFlushRetry) >= 0) {
      sfFlush(publishEvent);
      if (sfSize() > 0) tFlushRetry = millis() + FLUSH_BACKOFF_MS;
    }
  }
}

void netpieBegin() {
  mtx = xSemaphoreCreateMutex();
  evQueue = xQueueCreate(8, sizeof(EvItem));
  loadSchedule();

  // ESP-NOW ต้องใช้ WIFI_STA และ channel เดียวกับ router
  // → เรียก netpieBegin() ก่อน linkBegin() (espnow_link จัดการไล่ channel ฝั่งกำไลให้)
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  mqtt.setServer(NETPIE_HOST, NETPIE_PORT);
  mqtt.setCallback(onMessage);
  mqtt.setBufferSize(1024);  // shadow ทั้งก้อนตอน get ใหญ่กว่า 256 bytes ค่าเริ่มต้น
  mqtt.setKeepAlive(30);

  xTaskCreatePinnedToCore(netpieTask, "netpie", 12288, nullptr, 1, nullptr, 0);
}
