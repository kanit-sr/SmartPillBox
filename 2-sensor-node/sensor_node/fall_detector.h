// fall_detector.h — เรียกโมเดล Edge Impulse แล้วตัดสินว่าล้มหรือไม่
#pragma once
#include <stdint.h>

struct FallResult {
  bool     ok;        // false = รันโมเดลไม่สำเร็จ (ค่าอื่นใช้ไม่ได้)
  float    p_fall;    // ความน่าจะเป็นว่า window นี้เป็นการล้ม (0-1)
  bool     vote;      // window นี้นับเป็น "โหวตล้ม" (p_fall >= FALL_PROB_THRESHOLD)
  bool     detected;  // ยืนยันว่าล้ม -> ให้ state machine สั่นมอเตอร์ + นับถอยหลัง 15 วิ
  int      dsp_ms;    // เวลาที่ใช้คำนวณ spectral features
  int      nn_ms;     // เวลาที่ใช้รันโมเดล
};

bool fall_detector_begin();

// เรียกทุก FALL_INFER_INTERVAL_MS ด้วย window จาก imu_copy_window()
// window: IMU_WINDOW_SAMPLES * IMU_AXES float เรียงเก่า->ใหม่, หน่วย g และ deg/s
// detected เป็น true แค่ครั้งเดียวต่อเหตุการณ์ แล้วผลโหวตจะถูกล้างเอง
FallResult fall_detector_run(float *window);

// ล้างผลโหวตทั้งหมด — เรียกตอนกลับเข้าโหมดปกติ (หลังยกเลิก/เคลียร์ alarm หรือตื่นจาก deep sleep)
void fall_detector_reset();
