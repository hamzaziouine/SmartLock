# SmartLock — IoT Smart Access Control System

Université Internationale de Rabat — IoT Module 2025/2026
Group: Box 10 / TPE Groupe E — Supervisor: Prof. Ouassim Karrakchou

RFID-based door access control with Node-RED dashboard and MQTT backbone.

```
[RFID tag] --> [RC522] --> [Arduino UNO] --(HC-05 Bluetooth)--> [Raspberry Pi gateway]
                              |                                         |
                              v                                         v
                       [Servo + Buzzer]                         [Mosquitto MQTT broker]
                                                                        |
                                                                        v
                                                              [Node-RED flow + dashboard]
```

## Status

| Session | Scope | Status |
|---|---|---|
| 1 | Arduino sketch, wiring, HC-05 config, Bluetooth round-trip test | ✅ Done (2026-04-20) |
| 2 | Raspberry Pi gateway (`gateway.py`), Mosquitto, Node-RED flow + dashboard, end-to-end demo | ⏳ Pending |

## Quick start

1. Open this folder in VSCode with the PlatformIO extension installed.
2. Wire the hardware as described in [`BUILD_GUIDE.md`](BUILD_GUIDE.md) — follow section 3 (pin map) and section 4 (step by step).
3. Click **Upload** in PlatformIO. Open Serial Monitor at 9600 baud.
4. Scan a tag. Type `open`, `alarm`, or `lock` to control the actuators.

## Full build guide

Everything — bill of materials, pin-by-pin wiring, HC-05 AT configuration, test protocol, troubleshooting — is in **[BUILD_GUIDE.md](BUILD_GUIDE.md)**. A printable PDF version sits on the team's shared Desktop.

## Repository layout

```
.
├── BUILD_GUIDE.md       # Full build + wiring + test guide (THIS is the doc to read)
├── README.md            # You are here
├── platformio.ini       # PlatformIO build config
├── src/
│   └── main.cpp         # Arduino firmware
├── include/             # (header files, empty)
├── lib/                 # (local libs, empty)
└── test/                # (unit tests, empty)
```

Session 2 will add:
```
└── gateway/
    └── gateway.py       # Raspberry Pi Bluetooth <-> MQTT bridge
```

## Whitelist

- Authorized UID: `4C8A285D`
- Unauthorized UID: *scan second tag in Session 2*

## License

Academic project — not for redistribution outside UIR IoT 2025/2026.
