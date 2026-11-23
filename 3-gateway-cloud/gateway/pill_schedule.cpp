#include "pill_schedule.h"
#include <Arduino.h>
#include <time.h>
#include "protocol.h"
#include "espnow_link.h"
#include "netpie.h"
#include "sensors.h"
#include "config.h"

#define PERSON_RECENT_MS  5000UL  // เปิดฝาแล้วตัวบังเซนเซอร์ได้ เลยนับคนที่เห็นภายใน 5 วิล่าสุด

static char slots[MAX_PILL_SLOTS][6];
static int slotCount = 0;

static PillState state = PILL_IDLE;
static char curSlot[6] = "";          // มื้อที่กำลังเตือน
static unsigned long startMs = 0;      // เริ่มเตือนเมื่อไร (นับ 10/30 นาทีจากตรงนี้)
static unsigned long lastPersonMs = 0; // เห็นคนล่าสุดเมื่อไร (0 = ยังไม่เห็นในรอบนี้)

// กันเตือนมื้อเดิมซ้ำในวันเดียวกัน
static char lastFiredSlot[6] = "";
static int lastFiredDay = -1;

static int lastMinute = -1;            // ไว้อัปเดต next_pill ทุกนาที

// เวลาท้องถิ่นตอนนี้ คืน false ถ้ายังไม่ได้เวลาจาก NTP
static bool localNow(struct tm* t) {
  time_t now = netpieNow();
  if (!now) return false;
  localtime_r(&now, t);
  return true;
}

static void updateNextPill() {
  struct tm t;
  if (!localNow(&t) || slotCount == 0) {
    netpieSet("next_pill", "");
    return;
  }
  char hhmm[6];
  snprintf(hhmm, sizeof(hhmm), "%02d:%02d", t.tm_hour, t.tm_min);
  // มื้อถัดไป = มื้อที่น้อยที่สุดที่ยังไม่ถึง ถ้าไม่มี = มื้อแรกของพรุ่งนี้
  const char* next = nullptr;
  const char* first = nullptr;
  for (int i = 0; i < slotCount; i++) {
    if (!first || strcmp(slots[i], first) < 0) first = slots[i];
    if (strcmp(slots[i], hhmm) > 0 && (!next || strcmp(slots[i], next) < 0)) next = slots[i];
  }
  netpieSet("next_pill", next ? next : first);
}

static void startRemind(const char* slot, bool fallActive) {
  strncpy(curSlot, slot, sizeof(curSlot) - 1);
  curSlot[sizeof(curSlot) - 1] = '\0';
  state = PILL_REMIND;
  startMs = millis();
  lastPersonMs = 0;
  if (!fallActive) linkSetAckCmd(CMD_PILL_REMIND);  // กำไลสั่นตอน ACK ของ STATUS ถัดไป
  Serial.printf("[pill] ถึงเวลากินยามื้อ %s\n", curSlot);
}

static void finish(const char* ev) {
  char d[40];
  if (!strcmp(ev, "pill_taken")) {
    unsigned long lateMin = (millis() - startMs) / MINUTE_MS;
    snprintf(d, sizeof(d), "{\"slot\":\"%s\",\"late_min\":%lu}", curSlot, lateMin);
    if (netpieNow()) netpieSet("last_pill_taken", (int)netpieNow());
  } else {
    snprintf(d, sizeof(d), "{\"slot\":\"%s\"}", curSlot);
  }
  netpieEvent(ev, d);
  Serial.printf("[pill] %s %s\n", ev, d);
  state = PILL_IDLE;
}

void pillBegin() {}

void pillSetSchedule(char newSlots[][6], int n) {
  slotCount = min(n, MAX_PILL_SLOTS);
  for (int i = 0; i < slotCount; i++) strcpy(slots[i], newSlots[i]);
  Serial.printf("[pill] ตารางยา %d มื้อ\n", slotCount);
  updateNextPill();
}

void pillLoop(bool fallActive) {
  struct tm t;
  bool haveTime = localNow(&t);  // ยังไม่ได้ NTP = ยังเตือนตามตารางไม่ได้

  if (haveTime && t.tm_min != lastMinute) {
    lastMinute = t.tm_min;
    updateNextPill();
  }

  if (state == PILL_IDLE) {
    if (!haveTime) return;
    char hhmm[6];
    snprintf(hhmm, sizeof(hhmm), "%02d:%02d", t.tm_hour, t.tm_min);
    for (int i = 0; i < slotCount; i++) {
      if (strcmp(slots[i], hhmm) != 0) continue;
      if (!strcmp(lastFiredSlot, hhmm) && lastFiredDay == t.tm_yday) return;  // มื้อนี้เตือนไปแล้ว
      strcpy(lastFiredSlot, hhmm);
      lastFiredDay = t.tm_yday;
      startRemind(hhmm, fallActive);
      return;
    }
    return;
  }

  // กำลังเตือนอยู่ (REMIND / ESCALATE / MUTED)
  if (personNear()) lastPersonMs = millis() ? millis() : 1;
  bool personRecent = lastPersonMs && millis() - lastPersonMs < PERSON_RECENT_MS;

  if (lidOpenHeld() && personRecent) {
    finish("pill_taken");
  } else if (millis() - startMs >= PILL_MISSED_MS) {
    finish("pill_missed");
  } else if (state == PILL_REMIND && lastPersonMs == 0 && millis() - startMs >= PILL_ESCALATE_MS) {
    state = PILL_ESCALATE;
    if (!fallActive) linkSetAckCmd(CMD_PILL_REMIND);  // สั่งกำไลสั่นอีกรอบ
    Serial.println("[pill] ไม่เจอคนหน้ากล่อง เตือนถี่ขึ้น");
  }
}

void pillMute() {
  if (state == PILL_REMIND || state == PILL_ESCALATE) {
    state = PILL_MUTED;
    Serial.println("[pill] ปิดเสียงเตือนยา (ยังรอเปิดฝา)");
  }
}

void pillStartNow() {
  if (state != PILL_IDLE) return;
  struct tm t;
  char hhmm[6] = "test";
  if (localNow(&t)) snprintf(hhmm, sizeof(hhmm), "%02d:%02d", t.tm_hour, t.tm_min);
  startRemind(hhmm, false);
}

PillState pillState() { return state; }

void pillPrint() {
  static const char* names[] = {"IDLE", "REMIND", "ESCALATE", "MUTED"};
  Serial.printf("  pill: %s มื้อ=%s ผ่านไป %lu วิ, ตาราง:", names[state], curSlot,
                state == PILL_IDLE ? 0 : (millis() - startMs) / 1000);
  for (int i = 0; i < slotCount; i++) Serial.printf(" %s", slots[i]);
  Serial.println();
}
