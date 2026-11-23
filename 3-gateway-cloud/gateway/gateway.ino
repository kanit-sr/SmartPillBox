// gateway.ino — กล่องยา (ESP32 DevKit)
// รับ packet จากกำไลทาง ESP-NOW, เตือนยา, alarm ล้ม, เฝ้าที่เก็บยา แล้วส่งขึ้น NETPIE + LINE
// spec: system-flow.md, 1-system-architect/docs/state-diagram.md
//
// คำสั่งทดสอบใน Serial Monitor (115200):
//   p = เริ่มเตือนยาทันที   f = จำลองล้ม (node 1)   s = จำลอง SOS (node 1)
//   b = จำลองแบตกำไล 15%   t = บังคับที่เก็บยาร้อนเกิน   ? = พิมพ์สถานะ
#include "protocol.h"
#include "espnow_link.h"
#include "config.h"
#include "netpie.h"
#include "sensors.h"
#include "indicator.h"
#include "button.h"
#include "node_monitor.h"
#include "fall_alarm.h"
#include "pill_schedule.h"

static bool storageWarned = false;  // ส่ง storage_warn ไปแล้ว (รอให้กลับมาปกติก่อนส่งอีก)

// ---------- packet จากกำไล ----------
void onPacket(uint8_t type, const uint8_t* data, int len, bool isDup) {
  uint8_t node = ((const PacketHeader*)data)->node_id;
  nodeOnPacket(node, type, data, len);
  fallOnPacket(node, type, data, len, isDup);
}

// ---------- เลือกไฟ / เสียง / box_state ตามลำดับความสำคัญ (state-diagram.md ข้อ 3) ----------
static void updateOutputs() {
  PillState pill = pillState();
  const char* boxState;
  LedMode led;
  BeepMode beep;

  if (fallActive()) {
    boxState = "fall_alarm";    led = LED_RED_BLINK;  beep = fallBeep();
  } else if (pill == PILL_ESCALATE) {
    boxState = "pill_escalate"; led = LED_BLUE_BLINK; beep = BEEP_PILL_FAST;
  } else if (pill == PILL_REMIND) {
    boxState = "pill_remind";   led = LED_BLUE_BLINK; beep = BEEP_PILL;
  } else if (pill == PILL_MUTED) {
    boxState = "pill_remind";   led = LED_BLUE;       beep = BEEP_OFF;
  } else if (storageBad()) {
    boxState = "storage_warn";  led = LED_YELLOW;     beep = BEEP_OFF;
  } else if (!netpieOnline()) {
    boxState = "idle";          led = LED_YELLOW;     beep = BEEP_OFF;
  } else {
    boxState = "idle";          led = LED_GREEN;      beep = BEEP_OFF;
  }

  indicatorSet(led, beep);
  netpieSet("box_state", boxState);  // ส่งเฉพาะตอนค่าเปลี่ยน
}

// ---------- ที่เก็บยาร้อน/ชื้นเกิน: แจ้งครั้งเดียวต่อรอบ ----------
static void checkStorage() {
  bool bad = storageBad();
  if (bad && !storageWarned) {
    char d[48];
    snprintf(d, sizeof(d), "{\"temp\":%.1f,\"hum\":%.0f}", isnan(boxTemp()) ? 0 : boxTemp(),
             isnan(boxHum()) ? 0 : boxHum());
    netpieEvent("storage_warn", d);
    Serial.printf("[box] storage_warn %s\n", d);
  }
  storageWarned = bad;
}

// ---------- ค่าเซนเซอร์ขึ้น shadow ----------
static void publishSensors() {
  static unsigned long last = 0;
  if (millis() - last < SHADOW_SENSOR_MS) return;
  last = millis();
  if (!isnan(boxTemp())) netpieSet("temp", boxTemp());
  if (!isnan(boxHum())) netpieSet("hum", boxHum());
  netpieSet("lid_open", lidOpen());
  netpieSet("person", personNear());
}

static void printStatus() {
  Serial.printf("[box] net=%s ch=%u time=%ld fall=%d(node %u) lid=%d held=%d person=%d (%.0fcm) "
                "temp=%.1f hum=%.0f storageBad=%d\n",
                netpieOnline() ? "online" : "offline", linkChannel(), (long)netpieNow(), fallActive(),
                fallNode(), lidOpen(), lidOpenHeld(), personNear(), sonarCm(), boxTemp(), boxHum(),
                storageBad());
  pillPrint();
  nodeMonitorPrint();
}

static void handleSerial() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == 'p') pillStartNow();
    if (c == 'f') fallSimulate(1, FALL_CONFIRMED);
    if (c == 's') fallSimulate(1, SOS);
    if (c == 't') sensorsForceStorageBad();
    if (c == '?') printStatus();
    if (c == 'b') {
      StatusPacket p = {};
      p.hdr.ver = PROTO_VER;
      p.hdr.type = STATUS;
      p.hdr.node_id = 1;
      p.hr = 75;
      p.spo2 = 97;
      p.batt_pct = 15;
      nodeOnPacket(1, STATUS, (const uint8_t*)&p, sizeof(p));
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n== SmartPillBox gateway ==");
  indicatorBegin();
  buttonBegin();
  sensorsBegin();

  netpieBegin();         // ต้องก่อน linkBegin (ESP-NOW ใช้ Wi-Fi STA + channel เดียวกับ router)
  linkBegin(nullptr);
  linkOnReceive(onPacket);

  nodeMonitorBegin();
  fallAlarmBegin();
  pillBegin();
}

void loop() {
  linkLoop();            // packet จากกำไล → onPacket
  sensorsLoop();
  handleSerial();

  // ตารางยาจาก dashboard (จำไว้ใน flash ด้วย)
  bool changed;
  char slots[MAX_PILL_SLOTS][6];
  int n = netpieSchedule(slots, MAX_PILL_SLOTS, &changed);
  if (changed) pillSetSchedule(slots, n);

  // ปุ่มกล่อง: เคลียร์ alarm ล้มก่อน ไม่มี alarm ถึงจะปิดเสียงเตือนยา
  if (buttonPressed()) {
    if (fallActive()) fallClear("button");
    else pillMute();
  }
  if (netpieTakeClearAlarm()) fallClear("dashboard");

  fallAlarmLoop();
  pillLoop(fallActive());
  nodeMonitorLoop();
  checkStorage();
  publishSensors();

  updateOutputs();
  indicatorLoop();
}
