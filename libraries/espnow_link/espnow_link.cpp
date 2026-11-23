#include "espnow_link.h"
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#define MAX_PKT       32  // packet ใหญ่สุดใน protocol.h แค่ 17 bytes
#define RX_QUEUE_LEN  8
#define SCAN_AFTER    3   // ไม่ได้ ACK ติดกันกี่ครั้งถึงจะ scan channel

static const uint8_t BROADCAST[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

static uint8_t  peer[6];
static uint16_t mySeq;
static uint8_t  failCount = 0;

// ACK ที่รออยู่ (กำไล) — เขียนจาก WiFi task อ่านจาก loop
static volatile bool     ackGot = false;
static volatile uint16_t ackWaitSeq = 0;
static volatile uint8_t  ackWaitNode = 0;
static volatile uint8_t  lastCmd = 0;

// ฝั่งกล่อง
static LinkRecvCallback userCb = nullptr;
static QueueHandle_t    rxQueue = nullptr;
static volatile uint8_t pendingCmd = 0;
// packet ล่าสุดของแต่ละ node_id (กันรับซ้ำ แยกต่อ node) index = node_id
static uint16_t lastRxSeq[256];
static uint8_t  lastRxType[256];
static bool     haveLastRx[256];

struct RxItem {
  uint8_t len;
  bool    isDup;
  uint8_t data[MAX_PKT];
};

static void addPeer(const uint8_t* mac) {
  if (esp_now_is_peer_exist(mac)) return;
  esp_now_peer_info_t p = {};
  memcpy(p.peer_addr, mac, 6);
  p.channel = 0;  // 0 = channel ปัจจุบัน
  p.encrypt = false;
  esp_now_add_peer(&p);
}

static void sendAck(const uint8_t* mac, const PacketHeader* h) {
  AckPacket a;
  a.hdr.ver  = PROTO_VER;
  a.hdr.type = ACK;
  a.hdr.node_id = h->node_id;  // node ปลายทาง (บอร์ดจำลองหลาย node ใช้ MAC เดียวกัน)
  a.hdr.seq  = mySeq++;
  a.ack_seq  = h->seq;
  a.ack_type = h->type;
  a.cmd      = pendingCmd;
  pendingCmd = 0;  // ส่งครั้งเดียว
  addPeer(mac);
  esp_now_send(mac, (const uint8_t*)&a, sizeof(a));
}

static void handleRx(const uint8_t* mac, const uint8_t* data, int len) {
  if (len < (int)sizeof(PacketHeader) || len > MAX_PKT) return;
  const PacketHeader* h = (const PacketHeader*)data;
  if (h->ver != PROTO_VER) return;  // เวอร์ชันไม่ตรง ทิ้ง

  if (h->type == ACK) {  // กำไลได้ ACK
    if (len < (int)sizeof(AckPacket)) return;
    const AckPacket* a = (const AckPacket*)data;
    if (a->ack_seq == ackWaitSeq && a->hdr.node_id == ackWaitNode) {
      lastCmd = a->cmd;
      ackGot = true;
    }
    return;
  }

  // node_id 0 และ 255 สงวนไว้ ทิ้ง
  uint8_t n = h->node_id;
  if (n == 0 || n == 255) return;

  // กล่องได้ packet จากกำไล: ตอบ ACK ทุกครั้ง แม้เป็นตัวซ้ำ
  sendAck(mac, h);
  RxItem item;
  item.len = len;
  item.isDup = haveLastRx[n] && h->seq == lastRxSeq[n] && h->type == lastRxType[n];
  memcpy(item.data, data, len);
  lastRxSeq[n] = h->seq;
  lastRxType[n] = h->type;
  haveLastRx[n] = true;
  if (rxQueue) xQueueSend(rxQueue, &item, 0);
}

// signature ของ callback เปลี่ยนใน ESP32 core 3.x
#if ESP_ARDUINO_VERSION_MAJOR >= 3
static void onRecv(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
  handleRx(info->src_addr, data, len);
}
#else
static void onRecv(const uint8_t* mac, const uint8_t* data, int len) {
  handleRx(mac, data, len);
}
#endif

static void setChannel(uint8_t ch) {
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);
}

bool linkBegin(const uint8_t* peerMac) {
  if (WiFi.getMode() == WIFI_OFF) WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) return false;
  esp_now_register_recv_cb(onRecv);
  mySeq = (uint16_t)esp_random();  // สุ่มเลขเริ่ม กันชน seq เดิมหลังรีบูต
  rxQueue = xQueueCreate(RX_QUEUE_LEN, sizeof(RxItem));
  memcpy(peer, peerMac ? peerMac : BROADCAST, 6);
  if (peerMac) addPeer(peer);
  return true;
}

// ส่ง 1 รอบแล้วรอ ACK
static bool sendOnce(const uint8_t* pkt, size_t len, uint16_t seq) {
  ackWaitSeq = seq;
  ackGot = false;
  if (esp_now_send(peer, pkt, len) != ESP_OK) return false;
  unsigned long t0 = millis();
  while (millis() - t0 < ACK_TIMEOUT) {
    if (ackGot) return true;
    delay(2);
  }
  return false;
}

static bool scanChannels(const uint8_t* pkt, size_t len, uint16_t seq) {
  for (uint8_t ch = 1; ch <= 13; ch++) {
    setChannel(ch);
    if (sendOnce(pkt, len, seq)) return true;
  }
  return false;
}

bool linkSend(uint8_t type, void* pkt, size_t len, uint8_t tries, bool sameSeq) {
  if (len < sizeof(PacketHeader) || len > MAX_PKT) return false;
  PacketHeader* h = (PacketHeader*)pkt;
  if (!sameSeq) mySeq++;
  lastCmd = 0;  // ไม่ได้ ACK รอบนี้ = ไม่มีคำสั่ง (กันสั่นซ้ำจาก ACK เก่า)
  h->ver = PROTO_VER;
  h->type = type;
  h->seq = mySeq;
  if (h->node_id == 0) h->node_id = 1;  // ผู้ส่งไม่ได้ตั้ง node_id → ถือเป็นกำไลจริง (node 1)
  ackWaitNode = h->node_id;

  for (uint8_t i = 0; i < tries; i++) {
    if (sendOnce((uint8_t*)pkt, len, mySeq)) {
      failCount = 0;
      return true;
    }
  }

  // ไม่ได้ ACK: ถ้าติดกันหลายครั้ง กล่องอาจย้าย channel ตาม router → ไล่หา
  if (++failCount >= SCAN_AFTER) {
    failCount = 0;
    return scanChannels((uint8_t*)pkt, len, mySeq);
  }
  return false;
}

uint8_t linkLastCmd() { return lastCmd; }

void linkOnReceive(LinkRecvCallback cb) { userCb = cb; }

void linkSetAckCmd(uint8_t cmd) { pendingCmd |= cmd; }

void linkLoop() {
  if (!rxQueue) return;
  RxItem item;
  while (xQueueReceive(rxQueue, &item, 0) == pdTRUE) {
    if (userCb) userCb(item.data[1], item.data, item.len, item.isDup);
  }
}

uint8_t linkChannel() {
  uint8_t ch;
  wifi_second_chan_t second;
  esp_wifi_get_channel(&ch, &second);
  return ch;
}
