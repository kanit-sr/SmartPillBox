"""
prepare_fallalld.py — แปลง FallAllD (ข้อมือ) ให้พร้อมเทรนบน Edge Impulse

ทำอะไรบ้าง
  1. เลือกเฉพาะอุปกรณ์ข้อมือ (D2) และไฟล์ accel (_A) + gyro (_G)
  2. แปลงหน่วย raw -> g และ deg/s (LSM9DS1: ±8 g, ±2000 dps)
  3. resample 238 Hz -> 50 Hz (มี anti-alias filter)
  4. ตัด window 2 วิ = 100 sample x 6 แกน
       - fall  (ActivityID 101-135): หาจุดกระแทก (|a| สูงสุด) แล้วตัด window รอบจุดนั้น
                                     เลื่อนตำแหน่งจุดกระแทกใน window หลายแบบ ส่วนที่เหลือของไฟล์ทิ้ง
       - normal (ActivityID 1-44):   ตัด window ต่อกันทั้งไฟล์ แล้วสุ่มเลือกให้จำนวนใกล้ fall
                                     (เลือกวนทีละกิจกรรม ทุกกิจกรรมจึงมีตัวแทน)
  5. แบ่ง training / testing ตามคน (ไม่ใช่ตาม window)
  6. ส่งออกไฟล์ละ 1 window ชื่อ <label>.<id>.csv (Edge Impulse อ่าน label จากชื่อไฟล์)

ลำดับคอลัมน์ต้องตรงกับ firmware ตอน inference:  accX, accY, accZ (g), gyrX, gyrY, gyrZ (deg/s)

ตัวอย่าง
  python prepare_fallalld.py --variant A
  python prepare_fallalld.py --variant B                  # ตัดปรบมือ/โบกมือ/ทุบโต๊ะ
  python prepare_fallalld.py --variant A --exclude 25,27  # ตัดวิ่งเร็ว/กระโดดแรงเพิ่ม

ต้องมี: pip install numpy scipy
"""

import argparse
import csv
import json
import os
import random
import re
import sys
from collections import defaultdict
from pathlib import Path

import numpy as np
from scipy.signal import resample_poly

# ---------------- ค่าคงที่ของ dataset ----------------
FS_IN = 238                 # Hz (FallAllD)
FS_OUT = 50                 # Hz (MPU6050 บนกำไล)
UP, DOWN = 25, 119          # 50/238 = 25/119
WIN = 100                   # 2 วิ x 50 Hz
ACC_SCALE = 0.244e-3        # g per LSB      (LSM9DS1 ±8 g)
GYR_SCALE = 0.070           # deg/s per LSB  (LSM9DS1 ±2000 dps)
WRIST_DEVICE = 2            # D1=Neck, D2=Wrist, D3=Waist (จาก FallAllD_2_PYTHON_Structure.py)
AXES = ["accX", "accY", "accZ", "gyrX", "gyrY", "gyrZ"]
UNITS = ["g", "g", "g", "dps", "dps", "dps"]

# variant B: ตัดท่าแขนที่ใกล้การล้ม (ปรบมือ 1-4, โบกมือ 5-7, ทุบโต๊ะ 12)
VARIANT_B_EXCLUDE = {1, 2, 3, 4, 5, 6, 7, 12}

NAME_RE = re.compile(r"^S(\d{2})_D(\d)_A(\d{3})_T(\d{2})_A\.dat$")

SCRIPT_DIR = Path(__file__).resolve().parent
EMBEDDED_DIR = SCRIPT_DIR.parents[2]   # .../Embedded System


def is_fall(activity_id):
    return 101 <= activity_id <= 135


def load_recording(data_dir, stem):
    """อ่าน accel+gyro ของการทดลองหนึ่ง แปลงหน่วย และ resample เป็น 50 Hz -> (N, 6)"""
    acc = np.loadtxt(data_dir / f"{stem}_A.dat", delimiter=",", ndmin=2) * ACC_SCALE
    gyr = np.loadtxt(data_dir / f"{stem}_G.dat", delimiter=",", ndmin=2) * GYR_SCALE
    n = min(len(acc), len(gyr))
    x = np.hstack([acc[:n], gyr[:n]])
    return resample_poly(x, UP, DOWN, axis=0)


def fall_windows(x, shifts):
    """window รอบจุดกระแทก: จุดกระแทกอยู่ที่ตำแหน่ง 30..70 ใน window (กระจายตาม shifts)"""
    mag = np.linalg.norm(x[:, :3], axis=1)
    peak = int(np.argmax(mag))
    positions = np.unique(np.linspace(30, 70, shifts).round().astype(int))
    out = []
    for pos in positions:
        start = peak - pos
        if 0 <= start and start + WIN <= len(x):
            out.append((f"p{pos}", x[start:start + WIN]))
    return out, float(mag[peak])


def adl_windows(x, stride):
    return [(f"w{s}", x[s:s + WIN]) for s in range(0, len(x) - WIN + 1, stride)]


def pick_balanced(adl_by_activity, quota, rng):
    """เลือก ADL window วนทีละกิจกรรม จนได้ตาม quota"""
    pools = {a: rng.sample(ws, len(ws)) for a, ws in adl_by_activity.items()}
    picked = []
    while len(picked) < quota and any(pools.values()):
        for a in sorted(pools):
            if pools[a] and len(picked) < quota:
                picked.append(pools[a].pop())
    return picked


def write_sample(path, w, fmt):
    if fmt == "csv":
        with open(path, "w", newline="") as f:
            wr = csv.writer(f)
            wr.writerow(["timestamp"] + AXES)
            for i, row in enumerate(w):
                wr.writerow([i * 1000 // FS_OUT] + [f"{v:.5f}" for v in row])
    else:  # Edge Impulse data acquisition format (อัปโหลดได้โดยไม่ต้องตั้ง CSV Wizard)
        doc = {
            "protected": {"ver": "v1", "alg": "none"},
            "signature": "0" * 64,
            "payload": {
                "device_type": "FallAllD_wrist",
                "interval_ms": 1000 / FS_OUT,
                "sensors": [{"name": a, "units": u} for a, u in zip(AXES, UNITS)],
                "values": np.round(w, 5).tolist(),
            },
        }
        with open(path, "w") as f:
            json.dump(doc, f)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--data-dir", type=Path, default=EMBEDDED_DIR / "FallAllD__zip" / "FallAllD",
                    help="โฟลเดอร์ที่มีไฟล์ .dat ของ FallAllD")
    ap.add_argument("--out-dir", type=Path, default=EMBEDDED_DIR / "FallAllD_wrist_50Hz",
                    help="โฟลเดอร์ผลลัพธ์ (ควรอยู่นอก git repo)")
    ap.add_argument("--variant", choices=["A", "B"], default="A",
                    help="A = ADL ทั้งหมด, B = ตัดปรบมือ/โบกมือ/ทุบโต๊ะ")
    ap.add_argument("--exclude", default="", help="ActivityID ที่ตัดเพิ่ม คั่นด้วยคอมมา เช่น 25,27")
    ap.add_argument("--test-subjects", default="13,14,15", help="คนที่เก็บไว้เป็นชุดทดสอบ")
    ap.add_argument("--shifts", type=int, default=5, help="จำนวน window ต่อการล้ม 1 ครั้ง")
    ap.add_argument("--adl-stride", type=int, default=100, help="ระยะเลื่อน window ของ ADL (sample)")
    ap.add_argument("--adl-ratio", type=float, default=1.0, help="จำนวน normal ต่อ fall ในแต่ละ split")
    ap.add_argument("--format", choices=["csv", "json"], default="csv")
    ap.add_argument("--seed", type=int, default=42)
    args = ap.parse_args()

    if not args.data_dir.is_dir():
        sys.exit(f"ไม่พบโฟลเดอร์ข้อมูล: {args.data_dir}")

    exclude = {int(s) for s in args.exclude.split(",") if s.strip()}
    if args.variant == "B":
        exclude |= VARIANT_B_EXCLUDE
    test_subjects = {int(s) for s in args.test_subjects.split(",") if s.strip()}

    tag = f"variant_{args.variant}" + (f"_ex{'-'.join(map(str, sorted(exclude - VARIANT_B_EXCLUDE)))}"
                                       if exclude - VARIANT_B_EXCLUDE else "")
    out_root = args.out_dir / tag
    if out_root.exists() and any(out_root.rglob("*.*")):
        sys.exit(f"{out_root} มีไฟล์อยู่แล้ว ย้ายหรือลบก่อนรันใหม่ (กันข้อมูลคนละรอบปนกัน)")

    # ---------- อ่านไฟล์ข้อมือ ----------
    stems = []
    for name in sorted(os.listdir(args.data_dir)):
        m = NAME_RE.match(name)
        if m and int(m.group(2)) == WRIST_DEVICE:
            subj, act = int(m.group(1)), int(m.group(3))
            if act not in exclude:
                stems.append((name[:-6], subj, act, int(m.group(4))))
    if not stems:
        sys.exit("ไม่พบไฟล์ข้อมือ (D2) ในโฟลเดอร์ข้อมูล")
    print(f"ไฟล์ข้อมือที่ใช้: {len(stems)} การทดลอง | ตัดกิจกรรม: {sorted(exclude) or '-'}")

    rng = random.Random(args.seed)
    falls = {"training": [], "testing": []}
    adls = {"training": defaultdict(list), "testing": defaultdict(list)}
    peaks = []

    for i, (stem, subj, act, trial) in enumerate(stems, 1):
        split = "testing" if subj in test_subjects else "training"
        x = load_recording(args.data_dir, stem)
        meta = dict(subject=subj, activity=act, trial=trial, split=split)
        if is_fall(act):
            ws, peak_g = fall_windows(x, args.shifts)
            peaks.append(peak_g)
            falls[split] += [(dict(meta, label="fall", window=k), w) for k, w in ws]
        else:
            adls[split][act] += [(dict(meta, label="normal", window=k), w)
                                 for k, w in adl_windows(x, args.adl_stride)]
        if i % 250 == 0:
            print(f"  อ่านแล้ว {i}/{len(stems)}")

    # ---------- เขียนไฟล์ ----------
    manifest = []
    for split in ("training", "testing"):
        quota = round(len(falls[split]) * args.adl_ratio)
        normals = pick_balanced(adls[split], quota, rng)
        d = out_root / split
        d.mkdir(parents=True, exist_ok=True)
        for meta, w in falls[split] + normals:
            sid = f"S{meta['subject']:02d}_A{meta['activity']:03d}_T{meta['trial']:02d}_{meta['window']}"
            fname = f"{meta['label']}.{sid}.{args.format}"
            write_sample(d / fname, w, args.format)
            manifest.append(dict(meta, file=f"{split}/{fname}"))
        n_adl_total = sum(len(v) for v in adls[split].values())
        print(f"{split:9s}: fall {len(falls[split]):5d} | normal {len(normals):5d} "
              f"(จาก {n_adl_total} window ที่มี)")

    with open(out_root / "manifest.csv", "w", newline="") as f:
        wr = csv.DictWriter(f, fieldnames=["file", "label", "split", "subject", "activity", "trial", "window"])
        wr.writeheader()
        wr.writerows(manifest)

    p = np.array(peaks)
    print(f"แรงกระแทกตอนล้ม |a|: median {np.median(p):.2f} g, max {p.max():.2f} g "
          f"(ตั้ง MPU6050 เป็น ±8 g)")
    print(f"เสร็จ -> {out_root}")


if __name__ == "__main__":
    main()
