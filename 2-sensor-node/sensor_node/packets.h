// packets.h — ส่ง packet ผ่าน espnow_link (ห้ามประกาศ struct เอง ใช้จาก protocol.h เท่านั้น)
//
// spec (decisions.md 2026-11-20) ให้มี node_id ใน header แต่ protocol.h ตอนนี้ยังไม่มี field นี้
// send_packet() จึงใส่ NODE_ID ให้เอง "ถ้า" PacketHeader มี node_id แล้ว — พอคณิตอัปเดต protocol.h
// โค้ดกำไลไม่ต้องแก้อะไร (ถ้ายังไม่มี field ก็ compile ผ่านเหมือนเดิม)
#pragma once
#include "protocol.h"
#include "espnow_link.h"
#include "config.h"

// ชื่อ STATUS ใน protocol.h ชนกับ `typedef enum {...} STATUS` ใน ROM header ของ ESP32-C3 (rom/ets_sys.h)
// ซึ่ง Arduino.h include มาเสมอ → โค้ดกำไลจึงไม่เรียกชื่อ STATUS ตรงๆ แต่ใช้ค่า 1 ตาม message-format.md
// (จะ compile ได้ต้องให้ protocol.h เลิกประกาศชื่อ STATUS ก่อน — งานของ System Architect)
constexpr MsgType PKT_STATUS = static_cast<MsgType>(1);

namespace packet_detail {
template <typename P>
auto stamp_node_id(P &p, int) -> decltype(p.hdr.node_id = 0, void()) { p.hdr.node_id = NODE_ID; }
template <typename P>
void stamp_node_id(P &, long) {}
}  // namespace packet_detail

// ส่ง packet แล้วรอ ACK สูงสุด tries รอบ (รอบละ ACK_TIMEOUT) — คืน true ถ้าได้ ACK, cmd อ่านจาก linkLastCmd()
// same_seq = true ใช้ seq เดิม (ส่ง FALL_CONFIRMED/SOS ซ้ำ ให้กล่องรู้ว่าเป็นเหตุการณ์เดิม)
template <typename P>
bool send_packet(MsgType type, P &p, uint8_t tries, bool same_seq = false) {
  packet_detail::stamp_node_id(p, 0);
  return linkSend(type, &p, sizeof(p), tries, same_seq);
}
