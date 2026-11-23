#include "button.h"
#include <Arduino.h>
#include "config.h"

#define DEBOUNCE_MS  50UL

static bool stable = false;        // สถานะที่นิ่งแล้ว (true = กดอยู่)
static bool lastRead = false;
static unsigned long changedAt = 0;

void buttonBegin() {
  pinMode(PIN_BUTTON, INPUT_PULLUP);
}

bool buttonPressed() {
  bool down = digitalRead(PIN_BUTTON) == LOW;
  if (down != lastRead) {          // ค่ายังเด้งอยู่ เริ่มจับเวลาใหม่
    lastRead = down;
    changedAt = millis();
  }
  if (millis() - changedAt >= DEBOUNCE_MS && down != stable) {
    stable = down;
    return stable;                 // true เฉพาะตอนเพิ่งกดลง
  }
  return false;
}
