// เทสฮาร์ดแวร์กล่องยา (ESP32 DevKit) — แยกจาก gateway.ino ใช้เช็คว่าต่อสายถูก
// พิมพ์ค่าเซนเซอร์ทุก 1 วิ / คำสั่งใน Serial: r g b = สลับไฟ LED, z = buzzer 200ms
#include <DHT.h>

#define PIN_REED    4
#define PIN_BUTTON  16  // RX2
#define PIN_BUZZER  17  // TX2
#define PIN_TRIG    5
#define PIN_ECHO    18
#define PIN_DHT     19
#define PIN_LED_R   21
#define PIN_LED_G   22
#define PIN_LED_B   23

DHT dht(PIN_DHT, DHT11);
bool ledR = false, ledG = false, ledB = false;

float readDistanceCm() {
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);
  unsigned long us = pulseIn(PIN_ECHO, HIGH, 30000);
  return us ? us / 58.0 : -1;  // -1 = ไม่ได้ echo
}

void beep(int ms) {
  digitalWrite(PIN_BUZZER, HIGH);
  delay(ms);
  digitalWrite(PIN_BUZZER, LOW);
}

void showLed() {
  digitalWrite(PIN_LED_R, ledR);
  digitalWrite(PIN_LED_G, ledG);
  digitalWrite(PIN_LED_B, ledB);
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_REED, INPUT);
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  pinMode(PIN_LED_R, OUTPUT);
  pinMode(PIN_LED_G, OUTPUT);
  pinMode(PIN_LED_B, OUTPUT);
  dht.begin();

  // self-test: ไล่สี R G B แล้วบี๊บ 1 ที
  Serial.println("\n== hw_check_box: LED R -> G -> B, then beep ==");
  int pins[] = {PIN_LED_R, PIN_LED_G, PIN_LED_B};
  for (int p : pins) {
    digitalWrite(p, HIGH);
    delay(400);
    digitalWrite(p, LOW);
  }
  beep(200);
}

void loop() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == 'r') ledR = !ledR;
    if (c == 'g') ledG = !ledG;
    if (c == 'b') ledB = !ledB;
    if (c == 'z') beep(200);
    showLed();
  }

  static unsigned long last = 0;
  if (millis() - last < 1000) return;
  last = millis();

  float t = dht.readTemperature();
  float h = dht.readHumidity();
  Serial.printf("reed=%d button=%s dist=%.1fcm temp=%.1fC hum=%.0f%% led=%c%c%c\n",
                digitalRead(PIN_REED),
                digitalRead(PIN_BUTTON) == LOW ? "PRESSED" : "up",
                readDistanceCm(),
                isnan(t) ? -1.0 : t, isnan(h) ? -1.0 : h,
                ledR ? 'R' : '-', ledG ? 'G' : '-', ledB ? 'B' : '-');
}
