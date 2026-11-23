// battery.h — แรงดันแบต LiPo 1 cell → %
#pragma once
#include <stdint.h>

void battery_begin();

// แรงดันแบต (mV) เฉลี่ย BATT_SAMPLES ครั้ง คูณ BATT_DIVIDER แล้ว — 0 ถ้า BATT_ENABLED = 0
// อ่านตอนมอเตอร์ไม่สั่น (มอเตอร์ดึงกระแสทำให้แรงดันตกชั่วคราว)
uint16_t battery_read_mv();

// แปลง mV → 0–100 % ตามกราฟ discharge ของ LiPo (ประมาณ) — 100 ถ้า BATT_ENABLED = 0
uint8_t battery_percent(uint16_t mv);
