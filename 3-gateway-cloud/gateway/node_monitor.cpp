#include "node_monitor.h"
#include <Arduino.h>
#include "protocol.h"
#include "netpie.h"

enum NodeLink { NODE_ONLINE, NODE_OFFLINE, NODE_POWER_OFF };

struct Node {
  uint8_t  id;           // 0 = ช่องว่าง
  NodeLink link;
  unsigned long lastSeenMs;
  bool     lowBattSent;  // ส่ง low_battery ไปแล้ว รอแบตกลับมา ≥ LOW_BATT_PERCENT ก่อนส่งได้อีก
};

static Node nodes[MAX_NODES];

// ชื่อ field ของ node เช่น key("hr", 1) → "hr_1"
static const char* key(const char* name, uint8_t id) {
  static char buf[24];
  snprintf(buf, sizeof(buf), "%s_%u", name, id);
  return buf;
}

// อัปเดต shadow "nodes" = [1, 2, ...]
static void publishNodeList() {
  char json[64] = "[";
  for (int i = 0; i < MAX_NODES; i++) {
    if (!nodes[i].id) continue;
    char n[6];
    snprintf(n, sizeof(n), "%s%u", strlen(json) > 1 ? "," : "", nodes[i].id);
    strcat(json, n);
  }
  strcat(json, "]");
  netpieSetJson("nodes", json);
}

// หา node ในตาราง ถ้าไม่เจอลงทะเบียนใหม่ (ตารางเต็ม → nullptr)
static Node* findOrAdd(uint8_t id) {
  Node* empty = nullptr;
  for (int i = 0; i < MAX_NODES; i++) {
    if (nodes[i].id == id) return &nodes[i];
    if (!nodes[i].id && !empty) empty = &nodes[i];
  }
  if (!empty) {
    Serial.printf("[node] ตาราง node เต็ม (%d) ทิ้ง packet ของ node %u\n", MAX_NODES, id);
    return nullptr;
  }
  *empty = {id, NODE_ONLINE, millis(), false};
  Serial.printf("[node] ลงทะเบียน node %u\n", id);
  publishNodeList();
  return empty;
}

static void sendEvent(const char* ev, const char* d, uint8_t id) {
  netpieEvent(ev, d, 0, id);
  Serial.printf("[node] event %s node %u %s\n", ev, id, d);
}

void nodeMonitorBegin() {
  memset(nodes, 0, sizeof(nodes));
}

void nodeOnPacket(uint8_t id, uint8_t type, const uint8_t* data, int len) {
  Node* n = findOrAdd(id);
  if (!n) return;

  // packet ทุกชนิดนับว่า "เห็น"
  n->lastSeenMs = millis();
  if (n->link != NODE_ONLINE) Serial.printf("[node] node %u กลับมา online\n", id);
  n->link = NODE_ONLINE;
  netpieSet(key("node_online", id), true);
  if (netpieNow()) netpieSet(key("node_last_seen", id), (int)netpieNow());

  char d[32];
  if (type == STATUS && len >= (int)sizeof(StatusPacket)) {
    const StatusPacket* p = (const StatusPacket*)data;
    netpieSet(key("hr", id), (int)p->hr);
    netpieSet(key("spo2", id), (int)p->spo2);
    netpieSet(key("batt", id), (int)p->batt_pct);

    if (p->batt_pct < LOW_BATT_PERCENT && !n->lowBattSent) {
      snprintf(d, sizeof(d), "{\"batt\":%u}", p->batt_pct);
      sendEvent("low_battery", d, id);
      n->lowBattSent = true;
    } else if (p->batt_pct >= LOW_BATT_PERCENT) {
      n->lowBattSent = false;
    }
  } else if (type == POWER_OFF && len >= (int)sizeof(PowerOffPacket)) {
    const PowerOffPacket* p = (const PowerOffPacket*)data;
    n->link = NODE_POWER_OFF;  // จะไม่แจ้ง "ขาดการติดต่อ"
    netpieSet(key("node_online", id), false);
    snprintf(d, sizeof(d), "{\"batt\":%u}", p->batt_pct);
    sendEvent("node_power_off", d, id);
  }
}

void nodeMonitorLoop() {
  for (int i = 0; i < MAX_NODES; i++) {
    Node& n = nodes[i];
    if (!n.id || n.link != NODE_ONLINE) continue;
    if (millis() - n.lastSeenMs < NODE_TIMEOUT) continue;

    n.link = NODE_OFFLINE;  // แจ้งครั้งเดียว จนกว่าจะได้ packet ใหม่
    netpieSet(key("node_online", n.id), false);
    // last_seen เป็น epoch ถ้ามีเวลา NTP แล้ว
    time_t now = netpieNow();
    long lastSeen = now ? (long)(now - (millis() - n.lastSeenMs) / 1000) : 0;
    char d[32];
    snprintf(d, sizeof(d), "{\"last_seen\":%ld}", lastSeen);
    sendEvent("node_offline", d, n.id);
  }
}

void nodeMonitorPrint() {
  static const char* names[] = {"online", "offline", "power_off"};
  for (int i = 0; i < MAX_NODES; i++) {
    if (!nodes[i].id) continue;
    Serial.printf("  node %u: %s, เห็นล่าสุด %lu วิที่แล้ว, แบตต่ำแจ้งแล้ว=%d\n", nodes[i].id,
                  names[nodes[i].link], (millis() - nodes[i].lastSeenMs) / 1000, nodes[i].lowBattSent);
  }
}
