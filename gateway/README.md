# SmartLock — Session 2 gateway

Bridges the Arduino (USB or Bluetooth serial) to MQTT, with a Node-RED flow
that does whitelist matching and renders the dashboard.

## Architecture

```
[Arduino UNO + RC522 + servo + buzzer]
          │  USB serial @ 9600  (COM16 on this laptop)
          ▼
   gateway.py  ── MQTT ──▶  Mosquitto (localhost:1883)
                                 │
                                 ▼
                          Node-RED flow
                                 │
                                 ▼
                    Dashboard @ http://localhost:1880/ui
```

For the "real" Pi/Bluetooth deployment, the only change is
`SMARTLOCK_PORT=/dev/rfcomm0` after `rfcomm bind 0 <hc05_mac> 1`. Protocol is
identical.

## MQTT topics

| Topic            | Direction          | Payload                                    |
|------------------|--------------------|--------------------------------------------|
| `GUID/telemetry` | gateway → broker   | `{"type":"uid","uid":"4C8A285D"}`          |
|                  |                    | `{"type":"state","state":"OPEN"}`          |
|                  |                    | `{"type":"ready"}`                         |
| `GUID/commands`  | broker  → gateway  | `open` / `lock` / `alarm` (raw or `{"command":"..."}`) |

## Speedrun setup (Windows, ~10 minutes)

```powershell
# 1) install Mosquitto + Node-RED
winget install EclipseFoundation.Mosquitto
npm install -g --unsafe-perm node-red node-red-dashboard

# 2) start broker (allow anonymous on the loopback for the demo)
net start mosquitto

# 3) gateway
cd "gateway"
py -m pip install -r requirements.txt
$env:SMARTLOCK_PORT="COM16"
py gateway.py

# 4) Node-RED (separate terminal)
node-red
# Browse http://127.0.0.1:1880, hamburger -> Import -> select flows.json -> Deploy
# Dashboard: http://127.0.0.1:1880/ui
```

> If `net start mosquitto` says service not registered, run from the install
> dir: `mosquitto install` then `net start mosquitto`. Or run foreground:
> `mosquitto -v`.

## End-to-end test

1. Upload `src/main.cpp` to UNO. Close PlatformIO Serial Monitor (frees COM16).
2. `py gateway.py` → expect `[mqtt] connected rc=0` and `[arduino] READY`.
3. Open dashboard `http://127.0.0.1:1880/ui`.
4. Scan authorized tag (`4C8A285D`) → log shows `OK -> open`, servo opens 3 s,
   `Door state` reads `OPEN` then `LOCKED`.
5. Scan unauthorized tag → `DENY -> alarm`, 5 beeps.
6. Manual buttons (Open / Lock / Alarm) drive the door directly.
7. Whitelist editor: type a UID hex, **Add** / **Remove**, retest.

## Troubleshooting

| Symptom                        | Fix                                                    |
|--------------------------------|--------------------------------------------------------|
| `serial.SerialException: PermissionError` | PlatformIO Serial Monitor still open. Close it. |
| `[mqtt] connected rc=5`        | Mosquitto needs `allow_anonymous true` + a listener on 1883. |
| Dashboard buttons do nothing   | Node-RED not deployed, or broker config mismatch.      |
| UID prints in gateway but no command goes back | Check Node-RED debug pane; ensure flow Deploy succeeded. |
