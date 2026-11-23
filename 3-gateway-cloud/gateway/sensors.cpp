#include "sensors.h"
#include <Arduino.h>
#include <DHT.h>
#include "config.h"

#define ECHO_TIMEOUT_US  25000UL  // ~4 m ไม่ได้ echo ภายในนี้ = ไม่เจออะไร

static DHT dht(PIN_DHT, DHT11);

static bool lidIsOpen = false;
static unsigned long lidOpenedAt = 0;

static float distCm = -1;
static unsigned long lastSonar = 0;

static float temp = NAN, hum = NAN;
static unsigned long lastDht = 0;
static unsigned long badSince = 0;  // 0 = ตอนนี้ไม่เกิน
static bool forcedBad = false;

void sensorsBegin() {
  pinMode(PIN_REED, INPUT);
  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  digitalWrite(PIN_TRIG, LOW);
  dht.begin();
}

static float readDistanceCm() {
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);
  unsigned long us = pulseIn(PIN_ECHO, HIGH, ECHO_TIMEOUT_US);
  return us ? us / 58.0 : -1;
}

void sensorsLoop() {
  unsigned long now = millis();

  // reed: จำเวลาที่เริ่มเปิด
  bool open = digitalRead(PIN_REED) == REED_OPEN_LEVEL;
  if (open && !lidIsOpen) lidOpenedAt = now;
  lidIsOpen = open;

  if (now - lastSonar >= SONAR_INTERVAL_MS) {
    lastSonar = now;
    distCm = readDistanceCm();
  }

  if (now - lastDht >= DHT_INTERVAL_MS) {
    lastDht = now;
    temp = dht.readTemperature();
    hum = dht.readHumidity();
    // NaN เทียบ > แล้วได้ false เสมอ → อ่านไม่ได้ = ไม่นับว่าเกิน
    bool bad = temp > STORAGE_TEMP_MAX || hum > STORAGE_HUM_MAX;
    if (!bad) {
      badSince = 0;
      forcedBad = false;
    } else if (badSince == 0) {
      badSince = now ? now : 1;
    }
  }
}

bool lidOpen() { return lidIsOpen; }
bool lidOpenHeld() { return lidIsOpen && millis() - lidOpenedAt >= LID_OPEN_HOLD_MS; }
bool personNear() { return distCm > 0 && distCm < PERSON_NEAR_CM; }
float sonarCm() { return distCm; }

float boxTemp() { return temp; }
float boxHum() { return hum; }

bool storageBad() {
  return forcedBad || (badSince != 0 && millis() - badSince >= STORAGE_WARN_HOLD_MS);
}

void sensorsForceStorageBad() { forcedBad = true; }
