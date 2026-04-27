# SmartLock — IoT Smart Access Control System

Université Internationale de Rabat — IoT Module 2025/2026
Group: Box 10 / TPE Groupe E — Supervisor: Prof. Ouassim Karrakchou

RFID-based door access control with an MQTT backbone and a Node-RED
dashboard for live monitoring, manual override, and whitelist management.

```
[RFID tag] -> [RC522] -> [Arduino UNO] --(USB serial)--> [Laptop gateway.py]
                              |                                   |
                              v                                   v
                       [Servo + Buzzer]                  [Mosquitto MQTT broker]
                                                                  |
                                                                  v
                                                       [Node-RED flow + dashboard]
```

## Status

| Session | Scope                                                                  | Status                |
|---------|------------------------------------------------------------------------|-----------------------|
| 1       | Arduino sketch, wiring, standalone USB test                            | ✅ Done (2026-04-20)  |
| 2       | `gateway.py`, Mosquitto, Node-RED flow + dashboard, end-to-end demo    | ✅ Done (2026-04-27)  |

> Architecture change vs. the original proposal: the HC-05 Bluetooth link and
> Raspberry Pi gateway have been replaced by a laptop-hosted gateway over the
> same USB cable that powers the UNO. Manual override (the role HC-05 was
> meant to serve) is now a **dashboard button** that publishes to
> `GUID/commands` over MQTT. See [`PROPOSAL.md`](PROPOSAL.md) for the
> rationale and updated architecture, and [`BUILD_GUIDE.md`](BUILD_GUIDE.md)
> for the full build.

## Quick start

1. Open this folder in VSCode with PlatformIO. Wire per
   [`BUILD_GUIDE.md`](BUILD_GUIDE.md) §3–4. **Upload** to the UNO.
2. Close PlatformIO Serial Monitor (frees `COM16`).
3. From `gateway/` follow [`gateway/README.md`](gateway/README.md):
   - install Mosquitto + Node-RED
   - `pip install -r requirements.txt`
   - `py gateway.py`
   - `node-red`, import `flows.json`, deploy
4. Open dashboard at <http://127.0.0.1:1880/ui>.
5. Scan the authorized tag (`4C8A285D`) → door opens. Scan an unknown tag →
   alarm. Click dashboard buttons to override.

## Repository layout

```
.
├── BUILD_GUIDE.md          # Full build + wiring + test guide
├── PROPOSAL.md             # Revised project proposal (supersedes PDF)
├── README.md               # You are here
├── platformio.ini          # PlatformIO build config
├── src/
│   └── main.cpp            # Arduino firmware (USB serial only)
├── include/                # (header files, empty)
├── lib/                    # (local libs, empty)
├── test/                   # (unit tests, empty)
└── gateway/
    ├── gateway.py          # Serial <-> MQTT bridge (runs on laptop)
    ├── flows.json          # Node-RED flow (import in editor)
    ├── mosquitto.conf      # Minimal broker config (loopback, anonymous)
    ├── requirements.txt    # pyserial + paho-mqtt
    └── README.md           # Speedrun setup + troubleshooting
```

## Whitelist

- Authorized UID: `4C8A285D` (Hamza's tag)
- Unknown UIDs trigger the alarm. Add/remove UIDs live from the dashboard.

## License

Academic project — not for redistribution outside UIR IoT 2025/2026.
