// กล่องปลอม: รับ packet จากกำไล พิมพ์ออก Serial และตอบ ACK อัตโนมัติ
// พิมพ์ 'p' ใน Serial Monitor = ใส่คำสั่งสั่นเตือนยาไปกับ ACK ถัดไป
#include <WiFi.h>
#include "protocol.h"
#include "espnow_link.h"

void onPacket(uint8_t type, const uint8_t* data, int len, bool isDup) {
  if (type == STATUS) {
    const StatusPacket* s = (const StatusPacket*)data;
    Serial.printf("STATUS seq=%u hr=%u spo2=%u batt=%u%% %s\n",
                  s->hdr.seq, s->hr, s->spo2, s->batt_pct, isDup ? "(dup)" : "");
  } else {
    Serial.printf("type=%u len=%d %s\n", type, len, isDup ? "(dup)" : "");
  }
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  Serial.print("Box MAC: ");
  Serial.println(WiFi.macAddress());
  linkBegin(nullptr);
  linkOnReceive(onPacket);
}

void loop() {
  linkLoop();
  if (Serial.available() && Serial.read() == 'p') linkSetAckCmd(CMD_PILL_REMIND);
}
