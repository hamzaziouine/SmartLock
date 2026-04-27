# Smart Access Control System — Build Guide

**Université Internationale de Rabat · Génie Informatique — IA & Big Data**
**IoT Module — 2025/2026 · Supervisor: Prof. Ouassim Karrakchou**
**Group: Box 10 / TPE Groupe E**

**Team**
- Zine Elabidine ES-SABBAR — 126674
- Abdelmoati ELKOUFACHY — 123495
- Hamza ZIOUINE — 120072
- Leonce THEUREAU — 120874

---

## 1. Project overview

An IoT smart access control system. The Arduino UNO reads UIDs from an
RFID-RC522 reader and drives a servo (door) and an active-LOW buzzer (alarm).
The Arduino is connected by USB serial to a laptop running `gateway.py`,
which publishes UID / state events to a Mosquitto MQTT broker. A Node-RED
flow checks each UID against a configurable whitelist and either publishes
`open` or `alarm` back to the gateway, which forwards the command to the
Arduino over the same USB serial link. The dashboard shows the live access
log, the current door state, three manual-override buttons, and a whitelist
editor.

> The original proposal used an HC-05 Bluetooth module + Raspberry Pi as the
> network tier. That tier was replaced with a laptop-hosted gateway (see
> [`PROPOSAL.md`](PROPOSAL.md) §3). The MQTT protocol is unchanged; only the
> transport between Arduino and gateway changed (Bluetooth → USB).

**Session plan (2 × 2 h):**
- **Session 1 — DONE 2026-04-20.** Wiring, Arduino sketch, USB test.
- **Session 2 — DONE 2026-04-27.** `gateway.py`, Mosquitto, Node-RED, end-to-end.

## 2. Bill of materials

| Component       | Model / variant                  | Notes                                    |
|-----------------|----------------------------------|------------------------------------------|
| Microcontroller | Arduino UNO R3                   |                                           |
| RFID reader     | MFRC522 / RC522 module           | **3.3 V ONLY** — 5 V destroys it          |
| RFID tags       | 2 × MIFARE Classic 1K            | 1 authorized, 1 test                      |
| Servo           | Tower Pro SG90 9 g               | 200–400 mA spikes                         |
| Buzzer          | 3-pin active buzzer module       | **Active-LOW**                            |
| Jumpers         | ~12 M-M + M-F                    |                                           |
| Breadboard      | 400-point                        |                                           |
| USB cable       | USB-A to USB-B                   | Powers the UNO and carries the data link  |
| Laptop          | Windows 10/11 with Python 3      | Runs gateway, Mosquitto, Node-RED         |

## 3. Pin map (Arduino UNO)

| UNO pin | Signal              | Connects to                              |
|---------|---------------------|------------------------------------------|
| 3.3 V   | Power (RC522 ONLY)  | RC522 VCC ⚠️                              |
| 5 V     | Power bus           | Servo VCC, Buzzer VCC                    |
| GND     | Ground bus          | All module GNDs                          |
| D6      | PWM (Servo signal)  | SG90 signal (orange/yellow)              |
| D7      | Digital out (Buzzer)| Buzzer module IO                         |
| D9      | RC522 RST           | RC522 RST                                |
| D10     | SPI SS              | RC522 SDA/SS                             |
| D11     | SPI MOSI            | RC522 MOSI                               |
| D12     | SPI MISO            | RC522 MISO                               |
| D13     | SPI SCK             | RC522 SCK                                |

**Why these choices**
- RC522 uses hardware SPI → D10–D13 are fixed.
- D6 is PWM-capable for the Servo library.
- D7 is a plain digital pin; enough for the active buzzer's IO.
- D2/D3 (formerly HC-05 SoftwareSerial) are now free — the USB CDC port on
  D0/D1 is the only host link.

## 4. Wiring — step by step

Assemble **with USB unplugged**. Double-check the RC522 power rail before
first power-up.

### 4.1 RC522 (⚠️ 3.3 V)
```
RC522 SDA  -> UNO D10
RC522 SCK  -> UNO D13
RC522 MOSI -> UNO D11
RC522 MISO -> UNO D12
RC522 IRQ  -> (not connected)
RC522 GND  -> UNO GND
RC522 RST  -> UNO D9
RC522 3.3V -> UNO 3.3V        <- NOT 5 V
```

### 4.2 SG90 servo
```
Signal (orange/yellow) -> UNO D6
VCC    (red)           -> UNO 5V    (external 5 V if UNO browns out)
GND    (brown/black)   -> UNO GND
```

### 4.3 3-pin active-LOW buzzer module
```
VCC -> UNO 5V
GND -> UNO GND
IO  -> UNO D7
```
This module sounds when IO is **LOW** and is silent when IO is **HIGH**. The
sketch drives the pin HIGH as its very first action in `setup()` to prevent
the boot-beep before `Serial.begin()` even runs.

### 4.4 Host link
USB-B cable from the UNO to the laptop. That's it — no Bluetooth module, no
SoftwareSerial, no level-shifters. The USB CDC port on D0/D1 is the only
host link, and the gateway opens `COM16` to talk to it.

## 5. PlatformIO project

`platformio.ini`:
```ini
[env:uno]
platform      = atmelavr
board         = uno
framework     = arduino
monitor_speed = 9600
lib_deps =
    miguelbalboa/MFRC522@^1.4.11
    arduino-libraries/Servo@^1.2.2
```

If VSCode IntelliSense reports a red squiggle on `#include <Servo.h>`:
1. Command palette → **PlatformIO: Rebuild IntelliSense Index**
2. Command palette → **Developer: Reload Window**

The compiler is the truth — if Build succeeds, ignore the squiggle.

## 6. Source code

The sketch lives in [`src/main.cpp`](src/main.cpp). Highlights:

- Active-low buzzer inversion: `BUZZ_OFF = HIGH`, `BUZZ_ON = LOW`.
- Boot-beep prevention: `digitalWrite(BUZZER_PIN, BUZZ_OFF)` is the first
  statement in `setup()`.
- RFID UID → uppercase hex, printed as `UID:XXXXXXXX`.
- Three door states: `LOCKED`, `OPEN_HOLD`, `ALARMING`.
- Alarm pattern: 5 pulses of 100 ms on / 100 ms off over 1 s, non-blocking.
- Commands `open`, `alarm`, `lock` accepted from `Serial` (USB).

## 7. Gateway, broker, dashboard

The Session 2 software lives in [`gateway/`](gateway/). It contains:

- `gateway.py` — opens `COM16` at 9600 baud, parses Arduino lines, publishes
  to `GUID/telemetry`, subscribes to `GUID/commands` and writes them back to
  the Arduino.
- `flows.json` — Node-RED flow with whitelist function, dashboard widgets,
  and the manual-override buttons.
- `mosquitto.conf` — minimal Mosquitto config (loopback, anonymous).
- `requirements.txt` — `pyserial`, `paho-mqtt`.

Setup commands and the end-to-end test protocol are in
[`gateway/README.md`](gateway/README.md).

## 8. Test protocol (Session 2 — what we run for the demo)

1. Upload `src/main.cpp` to the UNO. **Close PlatformIO Serial Monitor** so
   `COM16` is free for the gateway.
2. Start Mosquitto: `mosquitto -v -c gateway\mosquitto.conf` (or service).
3. Start the gateway: `set SMARTLOCK_PORT=COM16 && py gateway\gateway.py`.
   Expect `[mqtt] connected rc=0` and `[arduino] READY`.
4. Start Node-RED: `node-red`. In the editor, **Import** `gateway/flows.json`,
   then **Deploy**.
5. Open the dashboard: <http://127.0.0.1:1880/ui>.
6. Scan the authorized tag (`4C8A285D`) → log shows `OK -> open`, servo
   opens for 3 s, `Door state` reads `OPEN` then `LOCKED`.
7. Scan an unknown tag → `DENY -> alarm`, 5 short beeps.
8. Press the dashboard **Open / Lock / Alarm** buttons → state transitions
   match the button without any tag scan.
9. Whitelist editor → type a UID hex, **Add** / **Remove**, retest.

## 9. Gotchas and fixes

| Symptom                                    | Root cause                                | Fix                                                         |
|--------------------------------------------|-------------------------------------------|-------------------------------------------------------------|
| Buzzer beeps continuously at power-on      | Active-LOW module; pin floats LOW pre-setup | First two lines of `setup()` pin D7 HIGH                    |
| `#include <Servo.h>` red squiggle          | PlatformIO IntelliSense didn't index lib  | Add `arduino-libraries/Servo` to `lib_deps`, rebuild index   |
| RC522 silent (no UIDs)                     | Powered from 5 V (fries it) or SS/RST swapped | Confirm 3.3 V, verify D9=RST / D10=SS                       |
| Servo doesn't move                         | Bumped wire / brown-out                   | Re-check D6 signal wire and 5 V rail; external 5 V if needed |
| Gateway: `serial.SerialException` on open  | PlatformIO Serial Monitor still holds COM16 | Close it before starting the gateway                        |
| Dashboard buttons don't actuate            | Node-RED flow not deployed, or broker mismatch | Re-deploy after import; confirm broker host = `localhost`  |

## 10. Whitelist / tag registry

| Role                       | UID (hex, uppercase)            |
|----------------------------|---------------------------------|
| Authorized (Hamza's tag)   | `4C8A285D`                      |
| Test tag                   | scanned live during the demo    |

The dashboard's Whitelist editor mutates the in-memory list at runtime; for
persistence across Node-RED restarts, replace `flow.set('whitelist', wl)`
with `global.set` and enable the file-based context store.

## 11. References

- Original proposal PDF: `IoT_Project_Proposal_Final.pdf` (superseded by [`PROPOSAL.md`](PROPOSAL.md))
- MFRC522 library: <https://github.com/miguelbalboa/rfid>
- Node-RED: <https://nodered.org/>
- Mosquitto: <https://mosquitto.org/>
- paho-mqtt: <https://eclipse.dev/paho/index.php?page=clients/python/index.php>
- pyserial: <https://pyserial.readthedocs.io/>
