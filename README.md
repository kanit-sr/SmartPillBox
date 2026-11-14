# Smart Pill Box + Fall Detection

IoT medication reminder and fall detection system for elderly care.

A pill-box gateway reminds the user at the scheduled time, detects whether the pill was taken, and sends alerts to family caregivers via LINE. A wristband sensor node monitors heart rate and detects falls with on-device TinyML. Data is synced to NETPIE2020 and shown on a dashboard.

## How It Works

```
Wristband (sensor node) ──ESP-NOW──► Pill box (gateway) ──MQTT──► NETPIE2020 ──► Dashboard
                                            └──HTTPS──► LINE ──► Caregiver's phone
```

## Repository Structure

```
smart-pillbox/
├── libraries/              Shared code used by both boards (protocol, ESP-NOW link, store-and-forward)
├── 1-system-architect/     Docs: message format, NETPIE schema, state diagrams, decisions
├── 2-sensor-node/          Wristband firmware (XIAO ESP32-C3) + ML dataset + PM notes
├── 3-gateway-cloud/        Pill box firmware (ESP32 DevKit) + cloud sync
├── 4-dashboard-testing/    NETPIE dashboard setup + test plan and results
├── system-flow.md          Overall system flow
├── .github/CODEOWNERS     Review ownership per folder
├── .gitignore
└── README.md
```

## Team

| Folder | Owner | Role |
|---|---|---|
| `libraries/`, `1-system-architect/` | Kanit | System Architect & Integration |
| `2-sensor-node/` | Chawiss | Embedded Dev (Sensor Node), PM |
| `3-gateway-cloud/` | Pasit | Embedded Dev (Gateway & Cloud) |
| `4-dashboard-testing/` | Pakorn | Dashboard UI/UX, Test Lead |

## Setup (Arduino IDE)

1. Clone the repository.
2. Set **File → Preferences → Sketchbook location** to the repo folder and restart the IDE, so it can see `libraries/`.
3. Install **esp32 by Espressif Systems** from Boards Manager.
4. Install the libraries listed for your part from Library Manager.
5. Open your sketch:
   - Sensor node: `2-sensor-node/sensor_node/sensor_node.ino`, board **XIAO_ESP32C3**
   - Gateway: `3-gateway-cloud/gateway/gateway.ino`, board **ESP32 Dev Module**
6. Gateway only: copy `secrets.example.h` to `secrets.h` and fill in your own values. Never commit `secrets.h`.

## Workflow

- Create a branch per part, for example `sensor/imu`, `gateway/pill-schedule`, `arch/protocol`.
- Edit only your own folder. For changes elsewhere, open a Pull Request.
- Changes under `libraries/` must be reviewed by the System Architect. Do not copy `protocol.h` into your own sketch.
- Commit with your own GitHub account.
- Do not commit build output (`.pio/`, `build/`, `*.bin`, `*.elf`) or `secrets.h`.
