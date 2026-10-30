# SmartPillBox

IoT medication dispenser and fall detection system for elderly care.

A smart pill-box gateway rotates the correct medication compartment at the scheduled time and verifies pill intake. A wristband sensor node monitors heart rate and detects falls with on-device TinyML. Family caregivers are notified via LINE when a dose is missed or a fall is detected.

## Repository Structure

```
smartpillbox/
├── node/        # เฟิร์มแวร์สายรัดข้อมือ (Sky)
├── gateway/     # เฟิร์มแวร์กล่องยา + cloud sync (Ice)
├── dashboard/   # หน้า Dashboard (Ohm)
├── docs/        # interface spec, block diagram, รายงาน (Kanit)
├── .gitignore
└── README.md
```

## Team

| Folder | Owner | Role |
|---|---|---|
| `node/` | Sky | Embedded Dev (Sensor Node) |
| `gateway/` | Ice | Embedded Dev (Gateway & Cloud) |
| `dashboard/` | Ohm | UI/UX & Dashboard + Project Manager & Test Lead |
| `docs/` | Kanit | System Architect & Integration |

## Workflow

- สร้าง Branch ใหม่เวลาจะทำ Feature
- Commit with your own GitHub account.
- Do not commit build output (`.pio/`, `build/`).
