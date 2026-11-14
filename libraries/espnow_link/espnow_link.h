// espnow_link.h — ESP-NOW ส่ง/รับ + ACK/retry + sequence number + channel scan
// เจ้าของ: คณิต | ใช้โดย: sensor_node.ino (กำไล), gateway.ino (กล่องยา)
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "protocol.h"

// callback ฝั่งกล่อง: ได้ packet หนึ่งอันจากกำไล (ACK ถูกส่งกลับไปแล้ว)
//   type  = MsgType ของ packet
//   data  = ทั้ง packet รวม header (cast เป็น StatusPacket* ฯลฯ ตาม type)
//   isDup = true ถ้า seq+type ซ้ำกับ packet ก่อนหน้า (อย่าเริ่ม alarm ใหม่)
typedef void (*LinkRecvCallback)(uint8_t type, const uint8_t* data, int len, bool isDup);

// เริ่ม ESP-NOW (เรียกใน setup หลัง WiFi.mode / WiFi.begin)
//   กำไล: ใส่ MAC ของกล่อง
//   กล่อง: ใส่ nullptr (รับจากใครก็ได้ แล้วตอบ ACK กลับไปที่ผู้ส่ง)
bool linkBegin(const uint8_t* peerMac);

// ---------- ฝั่งกำไล ----------

// ส่ง packet แล้วรอ ACK (ACK_TIMEOUT ต่อรอบ) ส่งได้สูงสุด tries รอบ
//   pkt     = struct จาก protocol.h ที่กรอก payload แล้ว (header กรอกให้เอง)
//   sameSeq = true ใช้ seq เดิมของครั้งก่อน (ใช้ตอนส่ง FALL_CONFIRMED/SOS ซ้ำใน loop
//             เพื่อให้กล่องรู้ว่าเป็นเหตุการณ์เดิม)
// คืน true ถ้าได้ ACK; cmd ที่ได้มาอ่านด้วย linkLastCmd()
// ถ้าไม่ได้ ACK ติดกัน 3 ครั้ง จะไล่ scan channel 1–13 ให้เอง
bool linkSend(uint8_t type, void* pkt, size_t len, uint8_t tries, bool sameSeq = false);

// cmd จาก ACK ล่าสุด (CMD_PILL_REMIND, CMD_FALL_CLEARED)
uint8_t linkLastCmd();

// ---------- ฝั่งกล่อง ----------

// ตั้ง callback รับ packet (เรียกจาก linkLoop ไม่ใช่จาก WiFi task)
void linkOnReceive(LinkRecvCallback cb);

// ใส่ cmd ไปกับ ACK ถัดไป "ครั้งเดียว" แล้วล้างเอง
// เช่น linkSetAckCmd(CMD_PILL_REMIND) → STATUS รอบหน้าได้ ACK ที่สั่งสั่น
void linkSetAckCmd(uint8_t cmd);

// ต้องเรียกใน loop() ของกล่อง เพื่อส่ง packet ที่รับมาเข้า callback
void linkLoop();

// channel ที่ใช้อยู่ตอนนี้ (ไว้ debug)
uint8_t linkChannel();
