#include "button.h"
#include "config.h"
#include "protocol.h"
#include <Arduino.h>
#include <esp_sleep.h>
#include <driver/gpio.h>
#include <soc/soc_caps.h>

#if !SOC_GPIO_SUPPORT_DEEPSLEEP_WAKEUP
#error "ชิปนี้ปลุก deep sleep ด้วย GPIO ไม่ได้ (โค้ดนี้เขียนสำหรับ XIAO ESP32-C3)"
#endif

namespace {

constexpr uint32_t DEBOUNCE_MS = 30;

bool     raw_last = false;     // ค่าดิบรอบก่อน
uint32_t raw_changed = 0;      // เวลาที่ค่าดิบเปลี่ยนล่าสุด
bool     down = false;         // สถานะหลัง debounce
uint32_t down_since = 0;
bool     sos_ready_sent = false;
bool     consumed = false;     // กดครั้งนี้ถูกใช้ไปแล้ว (ปิดเครื่อง / กดค้างตอนบูต) → ปล่อยแล้วไม่ต้องมี event

bool read_raw() { return digitalRead(PIN_BUTTON) == LOW; }  // ต่อลง GND = กด

}  // namespace

void button_begin(bool ignore_until_release) {
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  delay(2);
  raw_last = down = read_raw();
  raw_changed = down_since = millis();
  sos_ready_sent = false;
  consumed = down && ignore_until_release;
}

ButtonEvent button_update() {
  const uint32_t now = millis();
  const bool raw = read_raw();
  if (raw != raw_last) {
    raw_last = raw;
    raw_changed = now;
  }

  // เปลี่ยนสถานะเมื่อค่าดิบนิ่งครบ DEBOUNCE_MS
  if (raw != down && now - raw_changed >= DEBOUNCE_MS) {
    down = raw;
    if (down) {
      down_since = now;
      sos_ready_sent = false;
      consumed = false;
      return BTN_NONE;
    }
    // ปล่อยปุ่ม
    const uint32_t held = now - down_since;
    const bool was_consumed = consumed;
    consumed = false;
    if (was_consumed) return BTN_NONE;
    return held >= SOS_HOLD ? BTN_SOS : BTN_SHORT;
  }

  if (down && !consumed) {
    const uint32_t held = now - down_since;
    if (!sos_ready_sent && held >= SOS_HOLD) {
      sos_ready_sent = true;
      return BTN_SOS_READY;
    }
    if (held >= POWER_OFF_HOLD) {
      consumed = true;
      return BTN_POWER_OFF;
    }
  }
  return BTN_NONE;
}

bool button_is_down() { return down; }

void button_wait_release() {
  uint32_t up_since = millis();
  while (true) {
    if (read_raw()) up_since = millis();
    else if (millis() - up_since >= DEBOUNCE_MS) break;
    delay(5);
  }
  down = false;
  consumed = false;
}

void button_enable_wakeup() {
  // GPIO0–5 ของ C3 อยู่ใน power domain ที่ยังมีไฟตอน deep sleep จึงใช้ pull-up ภายในต่อได้
  // (ถ้าสายยาว/มีสัญญาณรบกวนจนตื่นเอง ให้ใส่ R pull-up 10k ภายนอกเพิ่ม)
  gpio_pullup_en((gpio_num_t)PIN_BUTTON);
  gpio_pulldown_dis((gpio_num_t)PIN_BUTTON);
  esp_deep_sleep_enable_gpio_wakeup(1ULL << PIN_BUTTON, ESP_GPIO_WAKEUP_GPIO_LOW);
}
