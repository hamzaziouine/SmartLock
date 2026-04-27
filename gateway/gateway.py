"""
SmartLock gateway — bridges Arduino serial <-> MQTT.

Session 2 speedrun: runs on Windows laptop against the Arduino on COM16
(direct USB serial). Same protocol works unchanged when swapped onto a Pi
reading /dev/rfcomm0 over Bluetooth — only SERIAL_PORT changes.

Topics:
  GUID/telemetry  ->  JSON  {"type":"uid","uid":"4C8A285D"}
                   |  JSON  {"type":"state","state":"OPEN"}
  GUID/commands   <-  raw   "open" | "alarm" | "lock"
"""

import json
import os
import threading
import time

import paho.mqtt.client as mqtt
import serial

SERIAL_PORT  = os.environ.get("SMARTLOCK_PORT", "COM16")
SERIAL_BAUD  = 9600
MQTT_HOST    = os.environ.get("MQTT_HOST", "localhost")
MQTT_PORT    = int(os.environ.get("MQTT_PORT", "1883"))
TOPIC_TLM    = "GUID/telemetry"
TOPIC_CMD    = "GUID/commands"

ser = serial.Serial(SERIAL_PORT, SERIAL_BAUD, timeout=1)
time.sleep(2)  # Arduino auto-reset on open

mqttc = mqtt.Client(client_id="smartlock-gateway")


def publish(payload: dict) -> None:
    mqttc.publish(TOPIC_TLM, json.dumps(payload), qos=0)
    print(f"[telemetry] {payload}")


def on_connect(client, _userdata, _flags, rc):
    print(f"[mqtt] connected rc={rc}")
    client.subscribe(TOPIC_CMD, qos=0)


def reopen_serial():
    global ser
    try:
        ser.close()
    except Exception:
        pass
    time.sleep(1)
    for attempt in range(5):
        try:
            ser = serial.Serial(SERIAL_PORT, SERIAL_BAUD, timeout=1)
            time.sleep(2)
            print(f"[serial] reopened {SERIAL_PORT}")
            return True
        except serial.SerialException as exc:
            print(f"[serial] reopen attempt {attempt+1}/5 failed: {exc}")
            time.sleep(2)
    return False


def on_command(_client, _userdata, msg):
    cmd = msg.payload.decode("utf-8", errors="ignore").strip()
    if not cmd:
        return
    try:
        cmd = json.loads(cmd).get("command", cmd)
    except json.JSONDecodeError:
        pass
    if cmd not in {"open", "alarm", "lock"}:
        print(f"[cmd] reject {cmd!r}")
        return
    print(f"[cmd] -> arduino: {cmd}")
    try:
        ser.write((cmd + "\r\n").encode("ascii"))
    except (serial.SerialException, OSError) as exc:
        print(f"[serial] write failed: {exc} — attempting reopen")
        if reopen_serial():
            try:
                ser.write((cmd + "\r\n").encode("ascii"))
            except Exception as exc2:
                print(f"[serial] write retry failed: {exc2}")


def serial_reader():
    while True:
        try:
            line = ser.readline().decode("ascii", errors="ignore").strip()
        except (serial.SerialException, OSError) as exc:
            print(f"[serial] read error: {exc} — attempting reopen")
            reopen_serial()
            continue
        if not line:
            continue
        print(f"[arduino] {line}")
        if line.startswith("UID:"):
            publish({"type": "uid", "uid": line[4:].strip().upper()})
        elif line.startswith("STATE:"):
            publish({"type": "state", "state": line[6:].strip()})
        elif line == "READY":
            publish({"type": "ready"})


def main():
    mqttc.on_connect = on_connect
    mqttc.on_message = on_command
    mqttc.connect(MQTT_HOST, MQTT_PORT, keepalive=30)
    threading.Thread(target=serial_reader, daemon=True).start()
    print(f"[gateway] {SERIAL_PORT}@{SERIAL_BAUD} <-> {MQTT_HOST}:{MQTT_PORT}")
    mqttc.loop_forever()


if __name__ == "__main__":
    main()
