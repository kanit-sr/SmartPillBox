#include "indicator.h"
#include <Arduino.h>
#include "config.h"

#define BLINK_MS  500UL  // LED กะพริบ ติด/ดับ อย่างละเท่านี้

static LedMode  led = LED_OFF;
static BeepMode beep = BEEP_OFF;
static unsigned long beepStart = 0;  // เวลาที่เริ่มโหมดเสียงปัจจุบัน

static void setRgb(bool r, bool g, bool b) {
  digitalWrite(PIN_LED_R, r);
  digitalWrite(PIN_LED_G, g);
  digitalWrite(PIN_LED_B, b);
}

void indicatorBegin() {
  pinMode(PIN_LED_R, OUTPUT);
  pinMode(PIN_LED_G, OUTPUT);
  pinMode(PIN_LED_B, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  setRgb(false, false, false);
  digitalWrite(PIN_BUZZER, LOW);
}

void indicatorSet(LedMode l, BeepMode b) {
  led = l;
  if (b != beep) {
    beep = b;
    beepStart = millis();  // เริ่มรูปแบบเสียงใหม่จากต้นรอบ (บี๊บแรกดังทันที)
  }
}

// ดังช่วง onMs แรกของทุกๆ periodMs
static bool pulse(unsigned long t, unsigned long periodMs, unsigned long onMs) {
  return (t % periodMs) < onMs;
}

void indicatorLoop() {
  unsigned long now = millis();
  bool blinkOn = (now / BLINK_MS) % 2 == 0;

  switch (led) {
    case LED_OFF:        setRgb(false, false, false); break;
    case LED_GREEN:      setRgb(false, true, false); break;
    case LED_YELLOW:     setRgb(true, true, false); break;
    case LED_BLUE:       setRgb(false, false, true); break;
    case LED_BLUE_BLINK: setRgb(false, false, blinkOn); break;
    case LED_RED_BLINK:  setRgb(blinkOn, false, false); break;
  }

  unsigned long t = now - beepStart;
  bool on = false;
  switch (beep) {
    case BEEP_OFF:       on = false; break;
    case BEEP_PILL:      on = pulse(t, 5000, 150); break;
    case BEEP_PILL_FAST: on = pulse(t, 1000, 150); break;
    case BEEP_FALL_FAST: on = pulse(t, 400, 200); break;
    case BEEP_FALL_SLOW: on = pulse(t, FALL_SLOW_BEEP_MS, 300); break;
  }
  digitalWrite(PIN_BUZZER, on ? HIGH : LOW);
}
