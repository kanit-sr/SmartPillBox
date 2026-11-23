// sensor_node.ino — กำไลข้อมือ (XIAO ESP32-C3 + MPU6050 + MAX30102 + มอเตอร์สั่น + ปุ่ม)
// state machine ตาม 1-system-architect/docs/state-diagram.md
//
//   NORMAL ──AI ตอบล้ม──► FALL_COUNTDOWN ──ครบ 15 วิ──► ALERT_SENDING ──ได้ ACK──► NORMAL
//     │                     │ กดสั้น → ส่ง FALL_CANCELLED → NORMAL
//     │                     └ กดค้าง 3 วิ → SOS ─────────► ALERT_SENDING
//     ├ กดค้าง 3 วิ (ปล่อยก่อน 10 วิ) → SOS ─────────────► ALERT_SENDING
//     └ กดค้าง 10 วิ → ส่ง POWER_OFF → deep sleep (OFF) ── กดปุ่ม → บูตใหม่ → NORMAL
//
// loop() ห้าม block นาน: IMU ต้องอ่านทุก 20 ms — ที่ block มีแค่ linkSend (รอ ACK 200 ms ต่อรอบ,
// ถ้าไม่ได้ ACK ติดกัน 3 ครั้ง espnow_link จะ scan channel 1–13 ใช้ ~2.6 วิ)
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_sleep.h>
#include <esp_now.h>
#include "protocol.h"
#include "espnow_link.h"
#include "config.h"
#include "packets.h"
#include "imu.h"
#include "pulse.h"
#include "battery.h"
#include "button.h"
#include "motor.h"
#include "fall_detector.h"

enum class State : uint8_t { ST_NORMAL, ST_FALL_COUNTDOWN, ST_ALERT_SENDING };  // ST_ กันชนกับ #define FALL_COUNTDOWN ใน protocol.h

// ---------- สถานะรวม ----------
State   state = State::ST_NORMAL;
bool    imu_ok = false;
bool    model_ok = false;        // โมเดลพร้อมใช้ (begin สำเร็จ)
bool    model_last_ok = true;    // รันโมเดลรอบล่าสุดสำเร็จ (ใช้ทำ FLAG_MODEL_OK)
uint8_t gateway_mac[6] = GATEWAY_MAC;

// channel ล่าสุดที่คุยกับกล่องได้ — อยู่ใน RTC memory จึงจำข้าม deep sleep ได้ ไม่ต้อง scan ใหม่ทุกครั้งที่เปิดเครื่อง
RTC_DATA_ATTR uint8_t saved_channel = 0;

// ---------- AI ----------
float    window_buf[IMU_WINDOW_SAMPLES * IMU_AXES];  // สำเนา ring buffer เรียงเก่า→ใหม่ ให้โมเดล
uint32_t last_infer_ms = 0;
uint32_t infer_after_sample = 0;  // รันโมเดลได้เมื่อ IMU เก็บ sample ถึงเลขนี้ (กันจับการล้มเดิมซ้ำหลังกลับ NORMAL)
uint32_t infer_runs = 0;

// ---------- STATUS ----------
uint32_t next_status_ms = 0;
bool     status_waiting_pulse = false;  // ถึงรอบ STATUS แล้ว รอวัดชีพจรเสร็จก่อนส่ง

// ---------- ล้ม / SOS ----------
uint8_t  fall_conf = 0;          // ความมั่นใจของโมเดล 0–100
uint32_t event_ms = 0;           // เวลาเกิดเหตุ (ล้ม = ตอน AI ตรวจเจอ, SOS = ตอนปล่อยปุ่ม) ใช้คำนวณ age_ms
MsgType  alert_type = FALL_CONFIRMED;
bool     alert_ready = false;    // วัดชีพจรเสร็จแล้ว เริ่มส่งได้
bool     alert_first = true;     // รอบแรกใช้ seq ใหม่ รอบต่อไปใช้ seq เดิม
uint8_t  alert_batt = 0;
uint32_t alert_tries = 0;

// ---------- sensor retry ----------
uint32_t last_imu_retry = 0;
uint32_t last_pulse_retry = 0;

// =====================================================================
// helpers
// =====================================================================

void set_wifi_channel(uint8_t ch) {
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);
}

void remember_channel() { saved_channel = linkChannel(); }

uint8_t status_flags() {
  uint8_t f = 0;
  if (pulse_ok()) f |= FLAG_MAX30102_OK;
  if (imu_ok) f |= FLAG_MPU6050_OK;
  if (model_ok && model_last_ok) f |= FLAG_MODEL_OK;
  return f;
}

// คำสั่งที่กล่องฝากมากับ ACK
void handle_ack_cmd() {
  const uint8_t cmd = linkLastCmd();
  if (cmd & CMD_PILL_REMIND) {
    LOG("[cmd] pill remind -> vibrate x3\n");
    motor_pill_remind();
  } else if (cmd & CMD_FALL_CLEARED) {
    LOG("[cmd] fall alarm cleared at box -> long vibrate\n");
    motor_fall_cleared();
  }
}

void enter_normal() {
  state = State::ST_NORMAL;
  fall_detector_reset();
  // ข้อมูลการล้ม (หรือการกระแทก) ยังค้างใน ring buffer → รอให้เก็บใหม่ครบ 1 window ก่อนรันโมเดลอีก
  infer_after_sample = imu_sample_count() + IMU_WINDOW_SAMPLES;
  LOG("[state] NORMAL\n");
}

void start_countdown(uint8_t conf) {
  state = State::ST_FALL_COUNTDOWN;
  fall_conf = conf;
  event_ms = millis();
  motor_on();  // สั่นตลอดช่วงนับถอยหลัง
  LOG("[state] FALL_COUNTDOWN conf=%u (กดปุ่มภายใน %lu วิ เพื่อยกเลิก)\n", conf, FALL_COUNTDOWN / 1000);
}

void start_alert(MsgType type) {
  state = State::ST_ALERT_SENDING;
  alert_type = type;
  alert_ready = false;
  alert_first = true;
  alert_tries = 0;
  if (type == SOS) event_ms = millis();
  motor_off();
  pulse_start();  // อ่านชีพจรครั้งเดียวก่อนส่ง (ถ้ากำลังวัดของ STATUS อยู่ ใช้ผลนั้นเลย)
  LOG("[state] ALERT_SENDING %s\n", type == SOS ? "SOS" : "FALL_CONFIRMED");
}

// =====================================================================
// ส่ง packet
// =====================================================================

void send_status() {
  const PulseResult pr = pulse_last();
  const uint16_t mv = battery_read_mv();
  StatusPacket p = {};
  p.hr = pr.hr;
  p.spo2 = pr.spo2;
  p.batt_pct = battery_percent(mv);
  p.batt_mv = mv;
  p.flags = status_flags();

  const bool ok = send_packet(PKT_STATUS, p, 1);  // spec: STATUS ส่งครั้งเดียว ไม่ retry
  LOG("[tx] STATUS seq=%u hr=%u spo2=%u batt=%u%% (%umV) flags=0x%02X ack=%s ch=%u\n",
      p.hdr.seq, p.hr, p.spo2, p.batt_pct, p.batt_mv, p.flags, ok ? "yes" : "no", linkChannel());
  if (ok) {
    remember_channel();
    handle_ack_cmd();
  }
}

void send_fall_cancelled() {
  FallCancelledPacket p = {};
  p.conf = fall_conf;
  p.age_ms = millis() - event_ms;
  const bool ok = send_packet(FALL_CANCELLED, p, FALL_CANCELLED_TRIES);
  LOG("[tx] FALL_CANCELLED conf=%u age=%lums ack=%s\n", p.conf, (unsigned long)p.age_ms, ok ? "yes" : "no");
  if (ok) remember_channel();
}

// ส่ง FALL_CONFIRMED / SOS 1 รอบ (เรียกซ้ำทุกรอบ loop จนได้ ACK) — คืน true ถ้าได้ ACK
bool send_alert_once() {
  const PulseResult pr = pulse_last();
  const uint32_t age = millis() - event_ms;
  const bool same_seq = !alert_first;
  bool ok;
  if (alert_type == FALL_CONFIRMED) {
    FallConfirmedPacket p = {};
    p.hr = pr.hr;
    p.spo2 = pr.spo2;
    p.conf = fall_conf;
    p.age_ms = age;
    p.batt_pct = alert_batt;
    ok = send_packet(FALL_CONFIRMED, p, 1, same_seq);
  } else {
    SosPacket p = {};
    p.hr = pr.hr;
    p.spo2 = pr.spo2;
    p.age_ms = age;
    p.batt_pct = alert_batt;
    ok = send_packet(SOS, p, 1, same_seq);
  }
  alert_first = false;
  alert_tries++;
  if (ok || alert_tries == 1 || alert_tries % 10 == 0) {
    LOG("[tx] %s try=%lu age=%lums ack=%s ch=%u\n", alert_type == SOS ? "SOS" : "FALL_CONFIRMED",
        (unsigned long)alert_tries, (unsigned long)age, ok ? "yes" : "no", linkChannel());
  }
  return ok;
}

// กดค้าง 10 วิ: ส่ง POWER_OFF (retry ≤ 5) แล้ว deep sleep — ไม่ return
void power_off() {
  LOG("[state] OFF\n");
  motor_off();
  PowerOffPacket p = {};
  p.batt_pct = battery_percent(battery_read_mv());
  const bool ok = send_packet(POWER_OFF, p, POWER_OFF_TRIES);
  LOG("[tx] POWER_OFF batt=%u%% ack=%s\n", p.batt_pct, ok ? "yes" : "no");
  if (ok) remember_channel();

  motor_power_off();
  while (motor_busy()) {
    motor_update();
    delay(5);
  }
  button_wait_release();  // ยังกดค้างอยู่ตอนเข้า sleep = ตื่นทันที

  pulse_sleep();
  imu_sleep();
  motor_off();
  esp_now_deinit();
  WiFi.mode(WIFI_OFF);

  button_enable_wakeup();
  LOG("deep sleep (กดปุ่มเพื่อเปิดเครื่อง)\n");
  Serial.flush();
  esp_deep_sleep_start();
}

// =====================================================================
// setup / loop
// =====================================================================

void setup() {
  Serial.begin(115200);
  const bool woke_by_button = esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_GPIO;
#if DEBUG_LOG
  delay(1000);  // รอ USB CDC ให้ monitor ต่อทัน
#endif

  motor_begin();
  button_begin(true);  // ปุ่มที่กดค้างไว้ตอนบูต (กดปลุก) ไม่นับเป็นการกด
  battery_begin();

  imu_ok = imu_begin();  // Wire.begin อยู่ในนี้ → ต้องมาก่อน pulse_begin
  pulse_begin();
  model_ok = fall_detector_begin();

  WiFi.mode(WIFI_STA);
  const bool link_ok = linkBegin(gateway_mac);
  if (link_ok && saved_channel >= 1 && saved_channel <= 13) set_wifi_channel(saved_channel);

  LOG("\n== sensor_node (node %d) ==\n", NODE_ID);
  LOG("wake=%s mac=%s ch=%u\n", woke_by_button ? "button" : "power-on", WiFi.macAddress().c_str(), linkChannel());
  LOG("gateway=%02X:%02X:%02X:%02X:%02X:%02X link=%s\n", gateway_mac[0], gateway_mac[1], gateway_mac[2],
      gateway_mac[3], gateway_mac[4], gateway_mac[5], link_ok ? "OK" : "FAIL");
  LOG("MPU6050=%s MAX30102=%s model=%s\n", imu_ok ? "OK" : "NOT FOUND", pulse_ok() ? "OK" : "NOT FOUND",
      model_ok ? "OK" : "FAIL");

  motor_boot();
  next_status_ms = millis() + FIRST_STATUS_DELAY_MS;
  enter_normal();
}

void loop() {
  const uint32_t now = millis();

  imu_update();
  motor_update();
  pulse_update();
  const ButtonEvent ev = button_update();

  // เซนเซอร์หาย (สายหลวม) → ลองใหม่เป็นระยะ
  if (!imu_ok && now - last_imu_retry >= SENSOR_RETRY_MS) {
    last_imu_retry = now;
    imu_ok = imu_begin();
    if (imu_ok) LOG("[imu] MPU6050 found\n");
  }
  if (!pulse_ok() && !pulse_busy() && now - last_pulse_retry >= SENSOR_RETRY_MS) {
    last_pulse_retry = now;
    if (pulse_begin()) LOG("[pulse] MAX30102 found\n");
  }

  switch (state) {
    case State::ST_NORMAL: {
      if (ev == BTN_SOS_READY) motor_sos_ready();  // ครบ 3 วิ: สั่นสั้นบอกว่าปล่อยได้แล้ว
      if (ev == BTN_SOS) { start_alert(SOS); break; }
      if (ev == BTN_POWER_OFF) power_off();

      // AI: รันทุก FALL_INFER_INTERVAL_MS เมื่อมีข้อมูลครบ 2 วิ
      if (model_ok && imu_ok && imu_window_ready() && imu_sample_count() >= infer_after_sample &&
          now - last_infer_ms >= FALL_INFER_INTERVAL_MS) {
        last_infer_ms = now;
        imu_copy_window(window_buf);
        const FallResult r = fall_detector_run(window_buf);
        model_last_ok = r.ok;
        infer_runs++;
        if (r.ok && (r.vote || infer_runs % 20 == 0)) {
          LOG("[ai] p_fall=%.2f vote=%d dsp=%dms nn=%dms imu_missed=%lu\n", r.p_fall, r.vote, r.dsp_ms, r.nn_ms,
              (unsigned long)imu_missed_samples());
        }
        if (r.detected) {
          const int conf = (int)(r.p_fall * 100 + 0.5f);
          start_countdown(conf > 100 ? 100 : conf);
          break;
        }
      }

      // STATUS ทุก STATUS_INTERVAL: วัดชีพจร (~7 วิ แบบ non-blocking) แล้วค่อยส่ง
      if (!status_waiting_pulse && (int32_t)(now - next_status_ms) >= 0) {
        next_status_ms = now + STATUS_INTERVAL;
        status_waiting_pulse = true;
        pulse_start();
      }
      if (status_waiting_pulse && !pulse_busy()) {
        status_waiting_pulse = false;
        send_status();
      }
      break;
    }

    case State::ST_FALL_COUNTDOWN: {
      // ไม่สนผล AI ใหม่ / คำสั่งเตือนยา
      if (ev == BTN_SHORT) {  // ผู้สวมบอกว่าไม่ได้ล้ม (ระบบจับผิด)
        motor_off();
        send_fall_cancelled();
        enter_normal();
        break;
      }
      // กดค้าง ≥ 3 วิระหว่างนับถอยหลัง = SOS ทันที (ไม่ส่ง FALL_CANCELLED)
      // ค้างถึง 10 วิก็นับเป็น SOS เพราะช่วงนี้ห้ามปิดเครื่อง
      if (ev == BTN_SOS || ev == BTN_POWER_OFF) {
        start_alert(SOS);
        break;
      }
      if (now - event_ms >= FALL_COUNTDOWN) start_alert(FALL_CONFIRMED);  // ไม่มีใครกด = ยืนยันล้ม
      break;
    }

    case State::ST_ALERT_SENDING: {
      // ไม่สนปุ่มทุกแบบ (ยกเลิกจากกำไลไม่ได้แล้ว ต้องเคลียร์ที่กล่อง)
      if (!alert_ready) {
        if (pulse_busy()) break;  // รอวัดชีพจรเสร็จ
        alert_ready = true;
        alert_batt = battery_percent(battery_read_mv());
        LOG("[alert] hr=%u spo2=%u batt=%u%%\n", pulse_last().hr, pulse_last().spo2, alert_batt);
      }
      if (send_alert_once()) {  // ส่งซ้ำทุก ACK_TIMEOUT จนกว่าจะได้ ACK
        remember_channel();
        motor_alert_sent();  // สั่น 2 ทีสั้นๆ บอกผู้สวมว่ากล่องได้รับแล้ว
        handle_ack_cmd();
        enter_normal();
      }
      break;
    }
  }
}
