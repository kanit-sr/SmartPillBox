#include "fall_alarm.h"
#include <Arduino.h>
#include "protocol.h"
#include "espnow_link.h"
#include "netpie.h"
#include "config.h"

static bool active = false;
static uint8_t alarmNode = 0;
static unsigned long startMs = 0;
static bool unansweredSent = false;

// เวลาที่เกิดจริง = ตอนนี้ - age_ms (0 = ยังไม่มีเวลา NTP ให้ netpie ใส่ตอนส่งเอง)
static time_t eventTime(uint32_t ageMs) {
  time_t now = netpieNow();
  return now ? now - ageMs / 1000 : 0;
}

static void startAlarm(uint8_t node, const char* ev, const char* d, time_t ts) {
  netpieEvent(ev, d, ts, node);  // → cloud + LINE
  Serial.printf("[fall] %s node %u %s\n", ev, node, d);
  if (active) return;            // alarm อยู่แล้ว ไม่รีเซ็ตเวลา
  active = true;
  alarmNode = node;
  startMs = millis();
  unansweredSent = false;
  netpieSet("alarm_node", (int)node);
}

void fallAlarmBegin() {
  netpieSet("alarm_node", 0);
}

void fallOnPacket(uint8_t node, uint8_t type, const uint8_t* data, int len, bool isDup) {
  if (isDup) return;  // กำไลส่งซ้ำ (seq เดิม) ไม่เริ่ม alarm ใหม่ ไม่ส่ง LINE ซ้ำ
  char d[48];

  if (type == FALL_CONFIRMED && len >= (int)sizeof(FallConfirmedPacket)) {
    const FallConfirmedPacket* p = (const FallConfirmedPacket*)data;
    snprintf(d, sizeof(d), "{\"hr\":%u,\"spo2\":%u,\"conf\":%u}", p->hr, p->spo2, p->conf);
    startAlarm(node, "fall_confirmed", d, eventTime(p->age_ms));
  } else if (type == SOS && len >= (int)sizeof(SosPacket)) {
    const SosPacket* p = (const SosPacket*)data;
    snprintf(d, sizeof(d), "{\"hr\":%u,\"spo2\":%u}", p->hr, p->spo2);
    startAlarm(node, "sos", d, eventTime(p->age_ms));
  } else if (type == FALL_CANCELLED && len >= (int)sizeof(FallCancelledPacket)) {
    const FallCancelledPacket* p = (const FallCancelledPacket*)data;
    snprintf(d, sizeof(d), "{\"conf\":%u}", p->conf);
    netpieEvent("fall_cancelled", d, eventTime(p->age_ms), node);  // แค่บันทึก ไม่เปลี่ยน state
    Serial.printf("[fall] fall_cancelled node %u\n", node);
  }
}

void fallAlarmLoop() {
  if (!active || unansweredSent) return;
  if (millis() - startMs >= FALL_UNANSWERED_MS) {
    char d[24];
    snprintf(d, sizeof(d), "{\"after_s\":%lu}", (millis() - startMs) / 1000);
    netpieEvent("fall_unanswered", d, 0, alarmNode);
    unansweredSent = true;
    Serial.println("[fall] ยังไม่มีคนรับเหตุ ส่ง LINE เตือนซ้ำ");
  }
}

bool fallActive() { return active; }
uint8_t fallNode() { return active ? alarmNode : 0; }

BeepMode fallBeep() {
  if (!active) return BEEP_OFF;
  return millis() - startMs < FALL_FAST_MS ? BEEP_FALL_FAST : BEEP_FALL_SLOW;
}

void fallClear(const char* by) {
  if (!active) return;
  char d[48];
  snprintf(d, sizeof(d), "{\"by\":\"%s\",\"after_s\":%lu}", by, (millis() - startMs) / 1000);
  netpieEvent("fall_acknowledged", d, 0, alarmNode);
  linkSetAckCmd(CMD_FALL_CLEARED);  // บอกกำไลใน ACK ถัดไป (ครั้งเดียว)
  Serial.printf("[fall] เคลียร์ alarm โดย %s\n", by);
  active = false;
  alarmNode = 0;
  netpieSet("alarm_node", 0);
}

void fallSimulate(uint8_t node, uint8_t type) {
  if (type == SOS) startAlarm(node, "sos", "{\"hr\":0,\"spo2\":0}", 0);
  else startAlarm(node, "fall_confirmed", "{\"hr\":0,\"spo2\":0,\"conf\":99}", 0);
}
