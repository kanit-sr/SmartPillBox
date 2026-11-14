#include "store_forward.h"
#include <string.h>

static char buf[SF_MAX][SF_LEN];
static int head = 0;   // ตำแหน่ง event เก่าสุด
static int count = 0;

bool sfPush(const char* json) {
  if (strlen(json) >= SF_LEN) return false;
  if (count == SF_MAX) {  // เต็ม: ทิ้งอันเก่าสุด
    head = (head + 1) % SF_MAX;
    count--;
  }
  int tail = (head + count) % SF_MAX;
  strcpy(buf[tail], json);
  count++;
  return true;
}

int sfFlush(SfPublishFn publish) {
  int sent = 0;
  while (count > 0) {
    if (!publish(buf[head])) break;  // ส่งไม่ได้ เก็บไว้ส่งรอบหน้า
    head = (head + 1) % SF_MAX;
    count--;
    sent++;
  }
  return sent;
}

int sfSize() { return count; }
