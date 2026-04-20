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

An IoT-based Smart Access Control System. The Arduino UNO reads UIDs from an RFID-RC522 reader, forwards them over HC-05 Bluetooth to a Raspberry Pi gateway (Session 2), which publishes them to an MQTT broker. A Node-RED flow compares each UID against a configurable whitelist and publishes `open` or `alarm` back to the gateway, which forwards the command to the Arduino over Bluetooth. The Arduino drives an SG90 servo (door lock) or a 3-pin active-low buzzer module (alarm).

**Compressed session plan (2 × 2h):**
- **Session 1 (DONE, 2026-04-20):** Arduino side fully working — wiring, sketch, standalone USB test, HC-05 configured and paired over Bluetooth. Bluetooth command round-trip verified.
- **Session 2:** Raspberry Pi gateway (`Gateway.py`), Mosquitto MQTT broker, Node-RED flow and dashboard, end-to-end demo.

## 2. Bill of materials

| Component | Model / variant | Notes |
|---|---|---|
| Microcontroller | Arduino UNO R3 (or clone) | |
| RFID reader | MFRC522 / RC522 module | **3.3 V ONLY** — 5 V destroys the chip |
| RFID tags | 2 × MIFARE Classic 1K | 1 authorized, 1 unauthorized |
| Servo | Tower Pro SG90 9g | 200–400 mA current spikes |
| Buzzer | 3-pin active buzzer module (VCC/GND/IO) | **Active-LOW** on our unit |
| Bluetooth | HC-05 ZS-040 breakout | Default 9600 baud, PIN 1234 |
| Jumpers | ~20 M-M + M-F | |
| Breadboard | 400-point | |
| USB cable | USB-A to USB-B | |
| FTDI (optional) | Any USB-TTL 5 V FTDI | Used only to configure HC-05 |

## 3. Pin map (Arduino UNO)

| UNO pin | Signal | Connects to |
|---|---|---|
| 3.3 V | Power (RC522 ONLY) | RC522 VCC ⚠️ |
| 5 V | Power bus | Servo VCC, Buzzer VCC, HC-05 VCC |
| GND | Ground bus | All module GNDs |
| D2 | SoftwareSerial RX | HC-05 TXD |
| D3 | SoftwareSerial TX | HC-05 RXD |
| D6 | PWM (Servo signal) | SG90 signal (orange/yellow) |
| D7 | Digital out (Buzzer) | Buzzer module IO |
| D9 | RC522 RST | RC522 RST |
| D10 | SPI SS | RC522 SDA/SS |
| D11 | SPI MOSI | RC522 MOSI |
| D12 | SPI MISO | RC522 MISO |
| D13 | SPI SCK | RC522 SCK |

**Why these choices**
- RC522 uses hardware SPI → D10–D13 are fixed.
- HC-05 cannot sit on D10/D11 (common tutorial pins) because of the SPI conflict. We put it on D2/D3 which support pin-change interrupts (needed for SoftwareSerial RX).
- D6 is PWM-capable for the Servo library.
- D7 is a plain digital pin; enough for the active buzzer's IO.

## 4. Wiring — step by step

Assemble **with USB unplugged**. Double-check the RC522 power rail before first power-up.

### 4.1 RC522 (⚠️ 3.3 V)
```
RC522 SDA  → UNO D10
RC522 SCK  → UNO D13
RC522 MOSI → UNO D11
RC522 MISO → UNO D12
RC522 IRQ  → (not connected)
RC522 GND  → UNO GND
RC522 RST  → UNO D9
RC522 3.3V → UNO 3.3V      ← NOT 5 V
```

### 4.2 SG90 servo
```
Signal (orange/yellow) → UNO D6
VCC    (red)           → UNO 5V        (consider external 5 V if UNO browns out)
GND    (brown/black)   → UNO GND
```

### 4.3 3-pin active-LOW buzzer module
```
VCC → UNO 5V
GND → UNO GND
IO  → UNO D7
```
This module sounds when IO is **LOW** and is silent when IO is **HIGH**. The sketch drives the pin HIGH as its very first action in `setup()` to prevent the boot-beep before `Serial.begin()` even runs.

### 4.4 HC-05 ZS-040
```
VCC → UNO 5V
GND → UNO GND
TXD → UNO D2       (HC-05 transmits, Arduino receives)
RXD → UNO D3       (Arduino transmits, HC-05 receives)
EN / KEY → (not connected — only needed for AT mode via FTDI)
```
ZS-040 is 5 V-tolerant on RXD thanks to the onboard divider — no external resistor needed.

## 5. HC-05 configuration (done via FTDI)

Renamed the module and changed PIN so the phone/Pi sees a named device.

**FTDI ↔ HC-05 wiring (power off first)**
```
FTDI VCC (5V) → HC-05 VCC
FTDI GND      → HC-05 GND
FTDI TX       → HC-05 RX
FTDI RX       → HC-05 TX
```

**Enter AT mode**
1. Hold the HC-05 button down.
2. Plug FTDI into USB while holding. Release after ~2 sec.
3. LED now blinks slowly (on 2 s, off 2 s) = AT mode.

**PuTTY**: serial, FTDI COM, **38400 baud**, 8-N-1. Terminal settings: *Implicit CR in every LF* ON, *Local echo* ON.

**Commands sent**
```
AT                         → OK
AT+NAME=SmartLock          → OK (module renamed)
AT+PSWD=4242               → OK (PIN changed)
AT+UART=9600,0,0           → OK (comm baud locked)
AT+NAME?                   → +NAME:SmartLock
AT+PSWD?                   → +PSWD:4242
AT+UART?                   → +UART:9600,0,0
```

**Exit AT mode:** unplug FTDI, release button, re-plug → fast blink = comm mode, ready to pair.

## 6. PlatformIO project

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

## 7. Source code

The full sketch lives in [`src/main.cpp`](src/main.cpp). Highlights:

- Active-low buzzer inversion: `BUZZ_OFF = HIGH`, `BUZZ_ON = LOW`.
- Boot-beep prevention: `digitalWrite(BUZZER_PIN, BUZZ_OFF)` is the first statement in `setup()`.
- RFID UID → uppercase hex, printed as `UID:XXXXXXXX`.
- Three door states: `LOCKED`, `OPEN_HOLD`, `ALARMING`.
- Alarm pattern: 5 pulses of 100 ms on / 100 ms off over exactly 1 second, driven non-blocking in `loop()`.
- Commands `open`, `alarm`, `lock` accepted from either USB `Serial` or HC-05 `SoftwareSerial`. Separate line buffers per stream prevent interleaving corruption.

## 8. Session 1 — test protocol (what we ran)

### 8.1 Standalone USB test
1. Upload sketch via PlatformIO (Ctrl+Alt+U).
2. Open Serial Monitor at 9600 baud → `READY` printed.
3. Scan authorized badge near RC522 → `UID:4C8A285D` printed.
4. Scan second badge → UID recorded for Session 2 unauthorized test.
5. Type `open` + Enter → servo rotates to 90°, returns to 0° after 3 s. `STATE:OPEN` → `STATE:LOCKED`.
6. Type `alarm` + Enter → 5 short beeps over 1 s. `STATE:ALARM` → `STATE:LOCKED`.
7. Type `lock` + Enter → servo returns immediately.

### 8.2 Bluetooth round-trip test
1. HC-05 paired with Windows laptop as `SmartLock`, PIN `4242`.
2. Noted **Outgoing** COM port in Windows Bluetooth → COM Ports tab.
3. Opened PuTTY on that COM at 9600 baud.
4. LED switched to slow double-blink (connected).
5. Commands from PuTTY produced identical state transitions as the USB test.
6. Badge scans printed `UID:...` inside PuTTY.

**Status:** all Session 1 objectives verified and passing.

## 9. Gotchas and fixes (from this build)

| Symptom | Root cause | Fix |
|---|---|---|
| Buzzer beeps continuously at power-on | Module is active-LOW; pin floats/reads LOW pre-setup | First two lines of `setup()` pin D7 HIGH |
| `#include <Servo.h>` shows red squiggle | PlatformIO IntelliSense didn't index framework lib | Add `arduino-libraries/Servo` to `lib_deps`, rebuild index |
| RC522 silent (no UIDs) | Powered from 5 V (fries it) or SS/RST swapped | Confirm 3.3 V, verify D9=RST / D10=SS |
| Servo doesn't move after adding HC-05 | Wiring bumped loose when adding HC-05 (in our case) | Re-check D6 signal wire and 5 V rail. If brown-outs persist: external 5 V supply for servo with shared GND, or a 100–470 µF cap across 5 V/GND at the servo |
| HC-05 pairs but PuTTY shows nothing | Wrong COM (used Incoming instead of Outgoing) | Use the **Outgoing** COM from Bluetooth → More Bluetooth settings → COM Ports |
| HC-05 AT commands fail | Not in AT mode, or wrong baud | Hold button while plugging FTDI; baud 38400 |

## 10. Whitelist / tag registry

| Role | UID (hex, uppercase) |
|---|---|
| Authorized (Hamza's tag) | `4C8A285D` |
| Unauthorized (test tag)  | *TBD — scan second badge and fill in* |

## 11. Session 2 — plan

### Goal
End-to-end flow: badge scan → HC-05 → Pi → MQTT → Node-RED whitelist check → MQTT → HC-05 → Arduino actuation → dashboard update.

### Tasks
1. Raspberry Pi setup: enable Bluetooth, pair with `SmartLock` (PIN `4242`), bind RFCOMM device `/dev/rfcomm0`.
2. Install Mosquitto MQTT broker on the Pi (or laptop — either works for the demo).
3. Write `gateway.py`:
   - Open `/dev/rfcomm0` at 9600 baud (pyserial).
   - Read lines, parse `UID:<hex>` → publish `{"uid":"<hex>"}` on topic `GUID/telemetry`.
   - Subscribe to `GUID/commands` → write command string + `\r\n` to serial.
4. Node-RED flow:
   - `mqtt in` on `GUID/telemetry` → JSON parse → function node checks UID against whitelist `["4C8A285D", <badge2>]` → branches into `{"command":"open"}` or `{"command":"alarm"}` → `mqtt out` on `GUID/commands`.
   - Dashboard widgets: live access log (timestamp + UID + status), door status indicator, manual Lock/Unlock buttons, whitelist editor (text field + add/remove buttons).
5. End-to-end test: scan authorized → door opens + dashboard logs green. Scan unauthorized → alarm beeps + dashboard logs red. Press manual Unlock → servo moves.
6. Report: architecture diagram, data formats, MQTT topics, all source code.

### Required by start of Session 2
- Raspberry Pi boots and connects to the same network as the laptop.
- Mosquitto and Node-RED installable (apt-get mosquitto, npm install -g node-red) — download beforehand if venue Wi-Fi is weak.
- HC-05 still named `SmartLock`, PIN `4242`.
- This repo cloned onto the Pi for the `src/main.cpp` reference and (soon) `gateway/gateway.py`.

## 12. References

- Proposal PDF: `IoT_Project_Proposal_Final.pdf`
- Build-guide PDF (printable): `IoT_Smart_Access_Build_Guide.pdf` on Desktop
- MFRC522 library: <https://github.com/miguelbalboa/rfid>
- Node-RED: <https://nodered.org/>
- Mosquitto: <https://mosquitto.org/>
