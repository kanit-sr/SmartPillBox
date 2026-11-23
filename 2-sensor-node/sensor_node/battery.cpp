#include "battery.h"
#include "config.h"
#include <Arduino.h>

namespace {

struct Point { uint16_t mv; uint8_t pct; };

// กราฟ LiPo 3.7 V ทั่วไป (ไม่มีโหลดหนัก) เรียงจากมากไปน้อย ระหว่างจุดใช้ linear interpolation
const Point CURVE[] = {
  {4200, 100}, {4150, 95}, {4110, 90}, {4080, 85}, {4020, 80}, {3980, 75},
  {3950, 70},  {3910, 65}, {3870, 60}, {3850, 55}, {3840, 50}, {3820, 45},
  {3800, 40},  {3790, 35}, {3770, 30}, {3750, 25}, {3730, 20}, {3710, 15},
  {3690, 10},  {3610, 5},  {3270, 0},
};
constexpr int N = sizeof(CURVE) / sizeof(CURVE[0]);

}  // namespace

void battery_begin() {
#if BATT_ENABLED
  analogReadResolution(12);
  analogSetPinAttenuation(PIN_BATT_ADC, ADC_11db);  // วัดได้ถึง ~2.5 V ที่ขา (แบตเต็ม 4.2 V / 2 = 2.1 V)
#endif
}

uint16_t battery_read_mv() {
#if BATT_ENABLED
  uint32_t sum = 0;
  for (int i = 0; i < BATT_SAMPLES; i++) sum += analogReadMilliVolts(PIN_BATT_ADC);  // ใช้ค่า calibration จาก eFuse
  return (uint16_t)((float)sum / BATT_SAMPLES * BATT_DIVIDER + 0.5f);
#else
  return 0;
#endif
}

uint8_t battery_percent(uint16_t mv) {
#if BATT_ENABLED
  if (mv >= CURVE[0].mv) return 100;
  if (mv <= CURVE[N - 1].mv) return 0;
  for (int i = 1; i < N; i++) {
    if (mv >= CURVE[i].mv) {
      const Point &hi = CURVE[i - 1], &lo = CURVE[i];
      return lo.pct + (uint32_t)(mv - lo.mv) * (hi.pct - lo.pct) / (hi.mv - lo.mv);
    }
  }
  return 0;
#else
  (void)mv;
  return 100;
#endif
}
