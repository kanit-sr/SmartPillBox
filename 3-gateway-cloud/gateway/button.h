// button.h — ปุ่มเคลียร์ alarm บนกล่องยา
#pragma once

void buttonBegin();
// คืน true "ครั้งเดียว" ต่อการกด 1 ครั้ง (debounce แล้ว) ต้องเรียกทุกรอบ loop
bool buttonPressed();
