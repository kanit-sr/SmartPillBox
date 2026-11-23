// เทสคิว: พิมพ์ 'o' = เน็ตหลุด/กลับ (สลับ), 'e' = เพิ่ม event
#include "store_forward.h"

bool online = true;
int nextId = 1;

bool fakePublish(const char* json) {
  if (!online) return false;
  Serial.print("publish: ");
  Serial.println(json);
  return true;
}

void setup() { Serial.begin(115200); }

void loop() {
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 'o') { online = !online; Serial.println(online ? "online" : "offline"); }
    if (c == 'e') {
      char json[SF_LEN];
      snprintf(json, sizeof(json), "{\"ev\":\"sos\",\"ts\":%lu,\"id\":%d,\"d\":{\"hr\":90,\"spo2\":97}}",
               millis() / 1000, nextId++);
      sfPush(json);
      Serial.printf("queued, queue=%d\n", sfSize());
    }
  }
  sfFlush(fakePublish);
}
