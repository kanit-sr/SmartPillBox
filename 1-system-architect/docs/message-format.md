# ESP-NOW Message Format

| type | ทิศทาง | field | หมายเหตุ |
|---|---|---|---|
| STATUS | กำไล → กล่อง | | ทุก ~1 นาที |
| FALL_CONFIRMED | กำไล → กล่อง | | ส่งซ้ำจนได้ ACK |
| FALL_CANCELLED | กำไล → กล่อง | | |
| SOS | กำไล → กล่อง | | |
| POWER_OFF | กำไล → กล่อง | | |
| ACK | กล่อง → กำไล | | มี flag สั่งเตือนยา |
