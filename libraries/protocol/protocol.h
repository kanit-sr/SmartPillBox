// protocol.h — ข้อตกลงการสื่อสารระหว่างกำไลกับกล่องยา
// เจ้าของ: คณิต | ใช้โดย: sensor_node.ino, gateway.ino
// spec: 1-system-architect/docs/message-format.md (ชื่อทุกตัวตามเอกสาร ห้ามเปลี่ยน)
#pragma once
#include <stdint.h>

// ---------- เวอร์ชัน protocol ----------
#define PROTO_VER 1   // กล่องทิ้ง packet ที่ ver ไม่ตรง

// ---------- ชนิด packet (ค่า type ใน header) ----------
enum MsgType : uint8_t {
  STATUS         = 1,   // ทุก 60 วิ
  FALL_CONFIRMED = 2,   // นับถอยหลัง 15 วิแล้วไม่มีใครกด, ส่งซ้ำจนกว่าจะได้ ACK
  FALL_CANCELLED = 3,   // กดยกเลิกระหว่างนับถอยหลัง, retry สูงสุด 5 ครั้ง
  SOS            = 4,   // กดค้าง 3 วิ, ส่งซ้ำจนกว่าจะได้ ACK
  POWER_OFF      = 5,   // กดค้าง 10 วิ, retry สูงสุด 5 ครั้งแล้ว deep sleep
  MOTION         = 6,   // ค่าดิบ MPU6050 สำหรับเดโม/ดีบัก ส่งครั้งเดียว ไม่ต้อง retry
  ACK            = 10   // กล่อง → กำไล
};

// ---------- ค่าคงที่ร่วม (หน่วย ms ทั้งหมด ยกเว้น LOW_BATT_PERCENT) ----------
#define STATUS_INTERVAL   60000UL   // 60 วิ
#define NODE_TIMEOUT      180000UL  // 180 วิ ไม่ได้ STATUS เกินนี้ = ขาดการติดต่อ
#define LOW_BATT_PERCENT  20        // %
#define FALL_COUNTDOWN    15000UL   // 15 วิ
#define SOS_HOLD          3000UL    // 3 วิ
#define POWER_OFF_HOLD    10000UL   // 10 วิ
#define ACK_TIMEOUT       200UL     // 200 ms
#define MAX_NODES         8         // ขนาดตาราง node ในกล่อง (ไม่ใช่ช่วงของ id, id ใช้ได้ 1–254)

// ---------- bit ของ flags (ใน STATUS) ----------
#define FLAG_MAX30102_OK  (1 << 0)  // bit0 = MAX30102 ใช้ได้
#define FLAG_MPU6050_OK   (1 << 1)  // bit1 = MPU6050 ใช้ได้
#define FLAG_MODEL_OK     (1 << 2)  // bit2 = โมเดล AI โหลดได้

// ---------- bit ของ cmd (ใน ACK) ----------
#define CMD_PILL_REMIND   (1 << 0)  // bit0 = สั่นเตือนยา (สั่น 3 ครั้ง)
#define CMD_FALL_CLEARED  (1 << 1)  // bit1 = alarm ล้มถูกเคลียร์แล้ว

// ---------- packet (packed: ขนาดตรงตาม spec ไม่มี padding) ----------

// Header ร่วม 5 bytes อยู่หน้าทุก packet (espnow_link กรอก ver/type/seq ให้เอง)
struct __attribute__((packed)) PacketHeader {
  uint8_t  ver;      // = PROTO_VER
  uint8_t  type;     // MsgType
  uint8_t  node_id;  // หมายเลขกำไล 1–254 (0, 255 สงวนไว้) ใน ACK = node ปลายทาง
  uint16_t seq;      // เลขลำดับของผู้ส่ง
};

struct __attribute__((packed)) StatusPacket {
  PacketHeader hdr;
  uint8_t  hr;        // bpm, 0 = อ่านไม่ได้
  uint8_t  spo2;      // %, 0 = อ่านไม่ได้
  uint8_t  batt_pct;  // %
  uint16_t batt_mv;   // แรงดันดิบ
  uint8_t  flags;     // FLAG_*
};

struct __attribute__((packed)) FallConfirmedPacket {
  PacketHeader hdr;
  uint8_t  hr;
  uint8_t  spo2;
  uint8_t  conf;      // ความมั่นใจโมเดล 0–100
  uint32_t age_ms;    // ล้มมาแล้วกี่ ms (อัปเดตทุกครั้งที่ส่ง)
  uint8_t  batt_pct;
};

struct __attribute__((packed)) FallCancelledPacket {
  PacketHeader hdr;
  uint8_t  conf;
  uint32_t age_ms;
};

struct __attribute__((packed)) SosPacket {
  PacketHeader hdr;
  uint8_t  hr;
  uint8_t  spo2;
  uint32_t age_ms;
  uint8_t  batt_pct;
};

struct __attribute__((packed)) PowerOffPacket {
  PacketHeader hdr;
  uint8_t  batt_pct;
};

struct __attribute__((packed)) MotionPacket {
  PacketHeader hdr;
  int16_t  ax, ay, az;  // ความเร่ง หน่วย mg (1000 = 1g)
  int16_t  gx, gy, gz;  // การหมุน หน่วย deg/s
};

struct __attribute__((packed)) AckPacket {
  PacketHeader hdr;   // type = ACK, seq = ของกล่องเอง
  uint16_t ack_seq;   // seq ของ packet ที่ตอบ
  uint8_t  ack_type;  // type ของ packet ที่ตอบ
  uint8_t  cmd;       // CMD_*
};

// เช็คขนาดตอน compile (ถ้าใครแก้ struct แล้วขนาดเพี้ยนจะ compile ไม่ผ่าน)
static_assert(sizeof(PacketHeader) == 5, "header must be 5 bytes");
static_assert(sizeof(StatusPacket) == 11, "STATUS size");
static_assert(sizeof(FallConfirmedPacket) == 13, "FALL_CONFIRMED size");
static_assert(sizeof(FallCancelledPacket) == 10, "FALL_CANCELLED size");
static_assert(sizeof(SosPacket) == 12, "SOS size");
static_assert(sizeof(PowerOffPacket) == 6, "POWER_OFF size");
static_assert(sizeof(MotionPacket) == 17, "MOTION size");
static_assert(sizeof(AckPacket) == 9, "ACK size");
