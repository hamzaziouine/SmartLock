# SmartLock — Revised Project Proposal

**Université Internationale de Rabat · Génie Informatique — IA & Big Data**
**IoT Module — 2025/2026 · Supervisor: Prof. Ouassim Karrakchou**
**Group: Box 10 / TPE Groupe E**
**Date of revision:** 2026-04-27

This document supersedes the original `IoT_Project_Proposal_Final.pdf`.
Two architectural changes have been made; everything else (objectives,
deliverables, evaluation criteria) is unchanged.

## 1. Team

| Name                        | Student ID |
|-----------------------------|------------|
| Zine Elabidine ES-SABBAR    | 126674     |
| Abdelmoati ELKOUFACHY       | 123495     |
| Hamza ZIOUINE               | 120072     |
| Leonce THEUREAU             | 120874     |

## 2. Goal (unchanged)

Build a working IoT smart access control system: an RFID reader authenticates
users, a microcontroller drives the lock and an alarm, and a network gateway
ships every event to an MQTT broker so a Node-RED dashboard can monitor the
door, override it manually, and edit the whitelist.

## 3. Architectural changes vs. the original proposal

### 3.1 Bluetooth link removed

**Before:** Arduino ↔ HC-05 Bluetooth ↔ Raspberry Pi gateway ↔ MQTT.

**Now:** Arduino ↔ USB serial ↔ laptop gateway (`gateway.py`) ↔ MQTT.

**Why:** The HC-05 ZS-040 module would not pair reliably with the available
Windows laptop. Rather than burn Session 2 chasing pairing/driver issues, we
deleted the Bluetooth tier altogether. The protocol on either side is
identical (newline-delimited ASCII at 9600 baud), so the swap is purely a
transport change — `gateway.py` can later open `/dev/rfcomm0` instead of
`COM16` with no other code changes if the BT path is needed.

### 3.2 "Manual override" is now a dashboard button

**Before:** the HC-05 channel doubled as a manual override — sending `open`,
`lock`, or `alarm` from a paired phone forced a state transition.

**Now:** the Node-RED dashboard exposes three buttons (Open / Lock / Alarm)
that publish the same commands on `GUID/commands`. The gateway forwards them
to the Arduino over USB. End user experience is the same (one tap to
override), but the operator now sees the door state, the access log, and the
whitelist in the same UI as the override buttons.

### 3.3 Net effect

- One fewer hardware component (HC-05) and one fewer host (Raspberry Pi).
- All software runs on a single laptop for the demo.
- The MQTT contract is unchanged from the original proposal, so the
  evaluation grid (data formats, topic design, dashboard, whitelist logic)
  applies as-is.

## 4. Architecture (revised)

```
[RFID tag] -> [RC522] -> [Arduino UNO] --USB serial--> [Laptop: gateway.py]
                              |                                |
                              v                                v
                       [Servo + Buzzer]                [Mosquitto broker]
                                                              |
                                                              v
                                                    [Node-RED flow + dashboard]
                                                              |
                                                              v
                                              http://127.0.0.1:1880/ui
                                              - access log (UID + verdict)
                                              - door state indicator
                                              - manual Open / Lock / Alarm
                                              - whitelist editor
```

## 5. MQTT contract

| Topic            | Direction          | Payload                                                |
|------------------|--------------------|--------------------------------------------------------|
| `GUID/telemetry` | gateway → broker   | `{"type":"uid","uid":"4C8A285D"}`                      |
|                  |                    | `{"type":"state","state":"OPEN"\|"LOCKED"\|"ALARM"}`   |
|                  |                    | `{"type":"ready"}`                                     |
| `GUID/commands`  | broker → gateway   | `open` / `lock` / `alarm`                              |

## 6. Hardware (revised)

| Component       | Model / variant                      | Notes                                    |
|-----------------|--------------------------------------|------------------------------------------|
| Microcontroller | Arduino UNO R3                       | USB-B cable to laptop                    |
| RFID reader     | MFRC522 / RC522                      | **3.3 V only**                           |
| RFID tags       | 2 × MIFARE Classic 1K                | Authorized + test                        |
| Servo           | Tower Pro SG90                       | Models the door actuator                 |
| Buzzer          | 3-pin active-LOW module              | Models the alarm                         |
| Host            | Windows laptop                       | Runs gateway, Mosquitto, Node-RED        |
| ~~Bluetooth~~   | ~~HC-05 ZS-040~~                     | **Removed** — see §3.1                   |
| ~~Gateway HW~~  | ~~Raspberry Pi~~                     | **Removed** — laptop hosts the gateway   |

## 7. Sessions (revised)

- **Session 1 — DONE 2026-04-20.** Wiring, Arduino sketch, RFID + servo +
  buzzer working from USB serial. Authorized tag UID captured.
- **Session 2 — DONE 2026-04-27.** `gateway.py` + `flows.json`, Mosquitto on
  loopback, Node-RED dashboard with whitelist editor and manual override.

## 8. Deliverables

- `src/main.cpp` — Arduino firmware (USB serial)
- `gateway/gateway.py` — pyserial + paho-mqtt bridge
- `gateway/flows.json` — Node-RED flow (whitelist + dashboard)
- `gateway/mosquitto.conf` — broker config
- `BUILD_GUIDE.md` — full wiring, build, and test guide
- `README.md` — quick start
- `PROPOSAL.md` — this document
- Demo video (recorded during defense)
