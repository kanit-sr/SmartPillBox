#!/usr/bin/env python3
"""กล่องยาจำลอง: ส่งข้อมูลปลอมเข้า NETPIE ให้ทดสอบ NETPIE dashboard ได้โดยไม่ต้องมีฮาร์ดแวร์
ไม่ส่ง LINE (ทดสอบ LINE ด้วย curl ใน 3-gateway-cloud/SETUP-cloud.md)

  pip install paho-mqtt
  export NETPIE_CLIENT_ID=... NETPIE_TOKEN=... NETPIE_SECRET=...
  python3 mock_box.py          # ส่งค่าปกติทุก 5 วิ + ตอบปุ่มเคลียร์ alarm + พิมพ์ตารางยาที่ dashboard เขียน
  python3 mock_box.py --fall   # จำลองล้ม (box_state = fall_alarm + event fall_confirmed)
ห้ามรันพร้อมกล่องจริงที่ใช้ Client ID เดียวกัน
"""
import json, os, random, sys, time
import paho.mqtt.client as mqtt

CID, TOKEN, SECRET = os.environ["NETPIE_CLIENT_ID"], os.environ["NETPIE_TOKEN"], os.environ.get("NETPIE_SECRET", "")
state = {"fall_at": None, "id": 0}


def shadow(data):
    c.publish("@shadow/data/update", json.dumps({"data": data}))


def event(ev, d):
    state["id"] += 1
    ts = int(time.time())
    c.publish("@msg/spb/event", json.dumps({"ev": ev, "ts": ts, "id": state["id"], "d": d}))
    shadow({"last_ev": ev, "last_ev_ts": ts})
    print("event", ev, d)


def on_connect(cl, userdata, flags, rc, *args):
    print("ต่อ NETPIE แล้ว" if rc == 0 else f"ต่อไม่ได้ rc={rc}")
    cl.subscribe("@msg/spb/cmd")
    cl.subscribe("@shadow/data/updated")


def on_message(cl, userdata, m):
    msg = json.loads(m.payload or b"{}")
    if m.topic == "@msg/spb/cmd" and msg.get("cmd") == "clear_alarm":
        print("ได้คำสั่ง clear_alarm")
        if state["fall_at"]:
            event("fall_acknowledged", {"by": "dashboard", "after_s": int(time.time() - state["fall_at"])})
            state["fall_at"] = None
        shadow({"box_state": "idle"})
    elif m.topic == "@shadow/data/updated" and "schedule" in msg.get("data", {}):
        print("ตารางยาจาก dashboard:", msg["data"]["schedule"])


try:
    c = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id=CID)
except AttributeError:  # paho-mqtt 1.x
    c = mqtt.Client(client_id=CID)
c.username_pw_set(TOKEN, SECRET)
c.on_connect, c.on_message = on_connect, on_message
c.connect("mqtt.netpie.io", 1883, 30)
c.loop_start()
time.sleep(2)

if "--fall" in sys.argv:
    event("fall_confirmed", {"hr": 112, "spo2": 94, "conf": 91})
    state["fall_at"] = time.time()
    shadow({"box_state": "fall_alarm"})

print("กำลังจำลองกล่องยา… (Ctrl+C เพื่อหยุด)")
while True:
    t = int(time.time())
    shadow({"hr": random.randint(70, 85), "spo2": random.randint(95, 99), "batt": 64, "node_online": True,
            "node_last_seen": t, "temp": round(29 + random.random(), 1), "hum": random.randint(58, 66),
            "lid_open": False, "person": True, "next_pill": "19:00", "queue": 0})
    time.sleep(5)
