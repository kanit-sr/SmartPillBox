// พิมพ์ขนาด packet ทุกชนิด (เช็คว่าตรงกับ message-format.md)
#include "protocol.h"

void setup() {
  Serial.begin(115200);
  Serial.printf("STATUS=%u FALL_CONFIRMED=%u FALL_CANCELLED=%u SOS=%u POWER_OFF=%u ACK=%u\n",
                sizeof(StatusPacket), sizeof(FallConfirmedPacket), sizeof(FallCancelledPacket),
                sizeof(SosPacket), sizeof(PowerOffPacket), sizeof(AckPacket));
}

void loop() {}
