#include "line_notify.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "secrets.h"

// ---------- สร้าง UUID คงที่จาก retryKey (key เดิม → UUID เดิม) ----------
static uint64_t fnv1a64(const char* s, uint64_t seed) {
  uint64_t h = seed;
  while (*s) { h ^= (uint8_t)*s++; h *= 0x100000001b3ULL; }
  return h;
}

static void uuidFromKey(const char* key, char out[37]) {
  uint64_t a = fnv1a64(key, 0xcbf29ce484222325ULL);
  uint64_t b = fnv1a64(key, 0x84222325cbf29ce4ULL);
  a = (a & ~0xF000ULL) | 0x4000ULL;                       // version 4
  b = (b & 0x3FFFFFFFFFFFFFFFULL) | 0x8000000000000000ULL; // variant
  snprintf(out, 37, "%08lx-%04lx-%04lx-%04lx-%012llx",
           (unsigned long)(a >> 32), (unsigned long)((a >> 16) & 0xFFFF),
           (unsigned long)(a & 0xFFFF), (unsigned long)(b >> 48),
           (unsigned long long)(b & 0xFFFFFFFFFFFFULL));
}

LineResult lineSend(const char* text, const char* retryKey) {
  if (WiFi.status() != WL_CONNECTED) return LINE_RETRY;
  if (strlen(LINE_TOKEN) == 0) {
    Serial.println("[LINE] ยังไม่ได้ใส่ LINE_TOKEN ใน secrets.h");
    return LINE_DROP;
  }

  JsonDocument doc;
  bool broadcast = strlen(LINE_TO) == 0;
  if (!broadcast) doc["to"] = LINE_TO;
  JsonObject m = doc["messages"].add<JsonObject>();
  m["type"] = "text";
  m["text"] = text;
  String body;
  serializeJson(doc, body);

  WiFiClientSecure tls;
  tls.setInsecure();  // โปรโตไทป์: ไม่ตรวจ cert (ถ้าจะใช้จริงให้ใส่ root CA ด้วย setCACert)
  HTTPClient http;
  http.setTimeout(8000);
  const char* url = broadcast ? "https://api.line.me/v2/bot/message/broadcast"
                              : "https://api.line.me/v2/bot/message/push";
  if (!http.begin(tls, url)) return LINE_RETRY;
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", String("Bearer ") + LINE_TOKEN);
  if (retryKey) {
    char uuid[37];
    uuidFromKey(retryKey, uuid);
    http.addHeader("X-Line-Retry-Key", uuid);
  }

  int code = http.POST(body);
  String resp = code > 0 ? http.getString() : "";
  http.end();

  if (code == 200 || code == 409) return LINE_OK;  // 409 = key นี้ LINE รับไปแล้ว
  Serial.printf("[LINE] HTTP %d %s\n", code, resp.c_str());
  if (code <= 0 || code >= 500) return LINE_RETRY;  // เน็ตหลุด / LINE ล่มชั่วคราว
  // 400 ข้อความผิด, 401 token ผิด, 403 สิทธิ์ไม่พอ, 429 โควตาเดือนนี้หมด/ยิงถี่เกิน
  return LINE_DROP;
}

// ---------- ข้อความของแต่ละ event ----------
static void hhmm(time_t t, char out[6]) {
  struct tm tmv;
  localtime_r(&t, &tmv);
  strftime(out, 6, "%H:%M", &tmv);
}

static void numOrDash(int v, char out[8]) {
  if (v > 0) snprintf(out, 8, "%d", v);
  else strcpy(out, "-");
}

bool lineTextForEvent(const char* ev, time_t ts, const char* d, time_t now, char* out, int outLen) {
  JsonDocument doc;
  if (d && *d) deserializeJson(doc, d);
  char t[6], hr[8], sp[8];
  hhmm(ts, t);
  numOrDash(doc["hr"] | 0, hr);
  numOrDash(doc["spo2"] | 0, sp);

  if (!strcmp(ev, "fall_confirmed")) {
    snprintf(out, outLen, "⚠️ มีคนล้ม เวลา %s ชีพจร %s SpO2 %s%% กรุณาตรวจสอบด่วน", t, hr, sp);
  } else if (!strcmp(ev, "sos")) {
    snprintf(out, outLen, "🆘 มีการกดขอความช่วยเหลือ (SOS) เวลา %s ชีพจร %s SpO2 %s%%", t, hr, sp);
  } else if (!strcmp(ev, "fall_unanswered")) {
    snprintf(out, outLen, "⚠️ ยังไม่มีคนรับเหตุล้ม (ผ่านไป %d นาที)", (int)(doc["after_s"] | 600) / 60);
  } else if (!strcmp(ev, "fall_acknowledged")) {
    const char* by = doc["by"] | "button";
    snprintf(out, outLen, "✅ มีคนรับเหตุแล้ว เวลา %s (%s)", t,
             strcmp(by, "dashboard") == 0 ? "กดจาก dashboard" : "กดปุ่มที่กล่องยา");
  } else if (!strcmp(ev, "pill_missed")) {
    snprintf(out, outLen, "💊 ยังไม่ได้กินยามื้อ %s (เลยมา 30 นาที)", (const char*)(doc["slot"] | "?"));
  } else if (!strcmp(ev, "low_battery")) {
    snprintf(out, outLen, "🔋 แบตกำไลเหลือ %d%% กรุณาชาร์จ", (int)(doc["batt"] | 0));
  } else if (!strcmp(ev, "node_offline")) {
    char ls[6];
    hhmm((time_t)(doc["last_seen"] | (long)ts), ls);
    snprintf(out, outLen, "📡 กำไลขาดการติดต่อตั้งแต่ %s", ls);
  } else if (!strcmp(ev, "storage_warn")) {
    snprintf(out, outLen, "🌡️ ที่เก็บยาร้อน/ชื้นเกิน %.1f°C ความชื้น %.0f%%",
             (float)(doc["temp"] | 0.0f), (float)(doc["hum"] | 0.0f));
  } else {
    return false;  // pill_taken, fall_cancelled, node_power_off ฯลฯ ดูใน dashboard พอ
  }

  if (now > 0 && ts > 0 && now - ts > 120) {  // ส่งช้าจากคิว store_forward
    int n = strlen(out);
    snprintf(out + n, outLen - n, " (ส่งช้า เกิดเมื่อ %s)", t);
  }
  return true;
}
