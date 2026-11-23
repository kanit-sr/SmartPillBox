// node_monitor.h — ตาราง node (กำไล) ในกล่องยา แยกต่อ node_id
// ลงทะเบียน node ใหม่เอง, อัปเดต shadow (hr_<n>, spo2_<n>, batt_<n>, node_online_<n>, node_last_seen_<n>, nodes)
// และส่ง event low_battery / node_power_off / node_offline
// spec: 1-system-architect/docs/state-diagram.md ข้อ 4
#pragma once
#include <stdint.h>

void nodeMonitorBegin();
// เรียกทุก packet ที่ได้จากกำไล (ก่อนส่งต่อให้ fall_alarm)
void nodeOnPacket(uint8_t node, uint8_t type, const uint8_t* data, int len);
// เช็ค node ที่เงียบเกิน NODE_TIMEOUT
void nodeMonitorLoop();
// พิมพ์ตาราง node ออก Serial (คำสั่ง ?)
void nodeMonitorPrint();
