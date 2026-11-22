#include "fall_detector.h"
#include "config.h"
#include <Arduino.h>
#include <math.h>
#include <string.h>
#include <chawissdev-project-1_inferencing.h>

// ถ้าเทรนโมเดลใหม่แล้วรูปแบบ input ไม่ตรงกับ ring buffer จะ compile ไม่ผ่านตรงนี้
static_assert(EI_CLASSIFIER_RAW_SAMPLE_COUNT == IMU_WINDOW_SAMPLES,
              "โมเดลต้องการจำนวน sample ไม่ตรงกับ IMU_WINDOW_SAMPLES");
static_assert(EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME == IMU_AXES,
              "โมเดลต้องการจำนวนแกนไม่ตรงกับ IMU_AXES");
static_assert(EI_CLASSIFIER_FREQUENCY == IMU_SAMPLE_HZ,
              "โมเดลเทรนที่ความถี่ไม่ตรงกับ IMU_SAMPLE_HZ");
static_assert(FALL_VOTE_NEEDED <= FALL_VOTE_WINDOW, "FALL_VOTE_NEEDED ต้องไม่เกิน FALL_VOTE_WINDOW");

namespace {

int      fall_ix = -1;                 // index ของ label "fall" (หาจากชื่อ ไม่ fix ตัวเลข)
bool     votes[FALL_VOTE_WINDOW];      // ผลโหวตล่าสุดแบบวน
uint8_t  vote_pos = 0;
bool     pending = false;              // โหวตครบแล้ว รอเช็คว่าแขนนิ่ง (ถ้าเปิดใช้)
uint32_t pending_since = 0;

void clear_votes() {
  memset(votes, 0, sizeof(votes));
  vote_pos = 0;
}

int count_votes() {
  int n = 0;
  for (int i = 0; i < FALL_VOTE_WINDOW; i++) n += votes[i] ? 1 : 0;
  return n;
}

#if FALL_REQUIRE_STILLNESS
// ส่วนเบี่ยงเบนมาตรฐานของ |a| ใน 1 วินาทีล่าสุดของ window (หน่วย g)
// แขนนิ่ง (นอนอยู่หลังล้ม) ~0.0x g, ปรบมือ/เดิน สูงกว่านี้มาก
float recent_acc_std(const float *w) {
  const int n = IMU_SAMPLE_HZ;
  float sum = 0, sq = 0;
  for (int i = IMU_WINDOW_SAMPLES - n; i < IMU_WINDOW_SAMPLES; i++) {
    const float *s = w + i * IMU_AXES;
    const float m = sqrtf(s[0] * s[0] + s[1] * s[1] + s[2] * s[2]);
    sum += m;
    sq += m * m;
  }
  const float mean = sum / n;
  const float var = sq / n - mean * mean;
  return var > 0 ? sqrtf(var) : 0;
}
#endif

}  // namespace

bool fall_detector_begin() {
  fall_detector_reset();
  return true;
}

void fall_detector_reset() {
  clear_votes();
  pending = false;
}

FallResult fall_detector_run(float *window) {
  FallResult r = {};

  signal_t signal;
  if (numpy::signal_from_buffer(window, EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE, &signal) != 0) return r;

  ei_impulse_result_t res = {};
  if (run_classifier(&signal, &res, false) != EI_IMPULSE_OK) return r;

  if (fall_ix < 0) {
    for (int i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
      if (strcmp(res.classification[i].label, "fall") == 0) fall_ix = i;
    }
    if (fall_ix < 0) return r;  // โมเดลไม่มี label "fall"
  }

  r.ok = true;
  r.p_fall = res.classification[fall_ix].value;
  r.dsp_ms = res.timing.dsp;
  r.nn_ms = res.timing.classification;
  r.vote = r.p_fall >= FALL_PROB_THRESHOLD;

  // 1) เก็บผลโหวตของ window นี้
  votes[vote_pos] = r.vote;
  vote_pos = (vote_pos + 1) % FALL_VOTE_WINDOW;

  // 2) โหวตล้มครบ 2 ใน 3 -> เข้าสถานะรอยืนยัน
  const uint32_t now = millis();
  if (!pending && count_votes() >= FALL_VOTE_NEEDED) {
    pending = true;
    pending_since = now;
  }

  // 3) ยืนยัน
  if (pending) {
#if FALL_REQUIRE_STILLNESS
    // รออย่างน้อย 1 วิให้ช่วง 1 วิล่าสุดของ window เป็นช่วงหลังกระแทก แล้วดูว่าแขนนิ่งไหม
    if (now - pending_since >= 1000 && recent_acc_std(window) < FALL_STILL_STD_G) {
      r.detected = true;
    } else if (now - pending_since > FALL_STILL_TIMEOUT_MS) {
      fall_detector_reset();  // ไม่นิ่งภายในเวลา = ไม่ใช่การล้ม
    }
#else
    r.detected = true;
#endif
    if (r.detected) fall_detector_reset();  // ไม่ให้เหตุการณ์เดียวยืนยันซ้ำ
  }

  return r;
}
