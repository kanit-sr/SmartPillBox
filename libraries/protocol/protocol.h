// protocol.h — ข้อตกลงการสื่อสารระหว่างกำไลกับกล่องยา
// เจ้าของ: คณิต | ใช้โดย: sensor_node.ino, gateway.ino
#pragma once
#include <stdint.h>

// TODO: enum MsgType (STATUS, FALL_CONFIRMED, FALL_CANCELLED, SOS, POWER_OFF, ACK)
// TODO: struct แต่ละ packet (ใส่ __attribute__((packed)))
// TODO: ค่าคงที่ร่วม เช่น NODE_TIMEOUT_MS, LOW_BATT_PERCENT, FALL_COUNTDOWN_MS
