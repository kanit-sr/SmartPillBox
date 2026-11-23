// copy ไฟล์นี้เป็น secrets.h แล้วใส่ค่าจริง (secrets.h ไม่ขึ้น git)
#pragma once

// Wi-Fi (ESP32 ใช้ได้แค่ 2.4 GHz)
#define WIFI_SSID        ""
#define WIFI_PASS        ""

// NETPIE2020 → Device ของกล่องยา (alias "pillbox") → Key
#define NETPIE_CLIENT_ID ""
#define NETPIE_TOKEN     ""
#define NETPIE_SECRET    ""

// LINE Developers → channel Messaging API → Messaging API → Channel access token (long-lived)
#define LINE_TOKEN       ""
// เว้นว่าง = broadcast ถึงทุกคนที่แอด LINE OA เป็นเพื่อน (ง่ายสุด)
// หรือใส่ userId "U..." / groupId "C..." เพื่อส่งหาคน/กลุ่มเดียว
#define LINE_TO          ""
