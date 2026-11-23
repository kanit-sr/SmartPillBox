// กำไลปลอม: ส่ง STATUS ทุก 5 วิ แล้วพิมพ์ผล ACK (เทสคู่กับ fake_box)
// ใส่ MAC ของกล่อง (ดูจาก Serial ของ fake_box) ใน BOX_MAC
#include <WiFi.h>
#include "protocol.h"
#include "espnow_link.h"

uint8_t BOX_MAC[6] = {0x24, 0x6F, 0x28, 0x00, 0x00, 0x00};  // TODO: แก้เป็น MAC กล่องจริง

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  if (!linkBegin(BOX_MAC)) Serial.println("ESP-NOW init failed");
}

void loop() {
  StatusPacket p = {};
  p.hr = 75;
  p.spo2 = 98;
  p.batt_pct = 80;
  p.batt_mv = 3950;
  p.flags = FLAG_MAX30102_OK | FLAG_MPU6050_OK;

  bool ok = linkSend(STATUS, &p, sizeof(p), 1);  // STATUS ส่งครั้งเดียวตาม spec
  Serial.printf("STATUS seq=%u ack=%s ch=%u\n", p.hdr.seq, ok ? "yes" : "no", linkChannel());
  if (ok && (linkLastCmd() & CMD_PILL_REMIND)) Serial.println("-> สั่นเตือนยา");
  delay(5000);
}
