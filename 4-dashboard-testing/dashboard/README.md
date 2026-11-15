# Dashboard

## Widget
- สถานะกำไล: online, แบต, HR/SpO2
- สถานะกล่อง: อุณหภูมิ/ความชื้น, เน็ต
- ประวัติ event, กราฟ HR/SpO2, อัตรากินยาตรงเวลา
- หน้าตั้งตารางยา (เขียนลง shadow)
- ปุ่มเคลียร์ alarm

// อ่านค่าชีพจรในช่อง VALUE ของ widget
#["spb_box"]["shadow"]["hr"]

// Action ของปุ่มเคลียร์ alarm
#["spb_box"].publishMsg(
  "spb/cmd",
  JSON.stringify({ cmd: "clear_alarm" })
);

// เขียนตารางยา
#["spb_box"].writeShadow(
  "schedule",
  ["08:00", "12:00", "19:00"]
);