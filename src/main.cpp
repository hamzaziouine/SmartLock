#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Servo.h>

// SmartLock — USB-serial firmware.
// Bluetooth was dropped from the architecture: the gateway runs on the laptop
// over the same USB cable that powers the UNO. Manual override happens
// through the Node-RED dashboard, which publishes commands to MQTT, which the
// gateway forwards over this same Serial link.

constexpr uint8_t RST_PIN    = 9;
constexpr uint8_t SS_PIN     = 10;
constexpr uint8_t SERVO_PIN  = 6;
constexpr uint8_t BUZZER_PIN = 7;

// Active-LOW buzzer module: HIGH = silent, LOW = sound.
constexpr uint8_t BUZZ_OFF = HIGH;
constexpr uint8_t BUZZ_ON  = LOW;

constexpr int SERVO_LOCKED = 0;
constexpr int SERVO_OPEN   = 90;

constexpr unsigned long OPEN_MS         = 3000;
constexpr unsigned long OPEN_BEEP_MS    = 300;   // single 300 ms beep on approval
constexpr unsigned long ALARM_MS        = 1000;  // 5 beeps total
constexpr unsigned long BEEP_PULSE      = 100;   // 100 ms on / 100 ms off
constexpr unsigned long SERVO_SETTLE_MS = 700;   // detach after this to stop holding-current draw
constexpr unsigned long RC_CHECK_MS     = 500;   // health-check the RC522 every 500 ms

MFRC522 rfid(SS_PIN, RST_PIN);
Servo doorServo;

enum DoorState { LOCKED, OPEN_HOLD, ALARMING };
DoorState state = LOCKED;
unsigned long stateEnteredAt = 0;
String serialBuf = "";

bool servoAttached = false;
unsigned long servoActivatedAt = 0;
unsigned long lastRcCheck = 0;

void rcInit() {
  rfid.PCD_Init();
  rfid.PCD_AntennaOff();
  rfid.PCD_AntennaOn();
  rfid.PCD_SetAntennaGain(rfid.RxGain_max);
}

void rcHealthCheck() {
  if (millis() - lastRcCheck < RC_CHECK_MS) return;
  lastRcCheck = millis();
  byte v = rfid.PCD_ReadRegister(MFRC522::VersionReg);
  if (v == 0x00 || v == 0xFF) {
    Serial.println(F("RC522:RESET"));
    rcInit();
  }
}

void servoMove(int angle) {
  if (!servoAttached) {
    doorServo.attach(SERVO_PIN);
    servoAttached = true;
  }
  doorServo.write(angle);
  servoActivatedAt = millis();
}

void servoMaintain() {
  if (servoAttached && millis() - servoActivatedAt >= SERVO_SETTLE_MS) {
    doorServo.detach();
    servoAttached = false;
  }
}

void enterState(DoorState s) {
  state = s;
  stateEnteredAt = millis();
  switch (s) {
    case LOCKED:
      servoMove(SERVO_LOCKED);
      digitalWrite(BUZZER_PIN, BUZZ_OFF);
      Serial.println(F("STATE:LOCKED"));
      break;
    case OPEN_HOLD:
      servoMove(SERVO_OPEN);
      digitalWrite(BUZZER_PIN, BUZZ_ON);   // single 300 ms approval beep, cut off in loop()
      Serial.println(F("STATE:OPEN"));
      break;
    case ALARMING:
      servoMove(SERVO_LOCKED);
      digitalWrite(BUZZER_PIN, BUZZ_ON);
      Serial.println(F("STATE:ALARM"));
      break;
  }
}

String readUidHex() {
  String s;
  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) s += "0";
    s += String(rfid.uid.uidByte[i], HEX);
  }
  s.toUpperCase();
  return s;
}

void handleCommand(String cmd) {
  cmd.trim(); cmd.toLowerCase();
  if (cmd.length() == 0) return;
  if      (cmd == "open")  enterState(OPEN_HOLD);
  else if (cmd == "alarm") enterState(ALARMING);
  else if (cmd == "lock")  enterState(LOCKED);
  else { Serial.print(F("UNKNOWN:")); Serial.println(cmd); }
}

void setup() {
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, BUZZ_OFF);

  Serial.begin(9600);
  SPI.begin();
  rcInit();

  servoMove(SERVO_LOCKED);

  // Diagnostic: read the RC522 chip version. 0x91/0x92 = v1.0/v2.0,
  // 0x88 = clone. 0x00 or 0xFF = SPI dead (wiring or power).
  byte rcVer = rfid.PCD_ReadRegister(MFRC522::VersionReg);
  Serial.print(F("RC522:0x"));
  if (rcVer < 0x10) Serial.print(F("0"));
  Serial.println(rcVer, HEX);

  Serial.println(F("READY"));
}

void loop() {
  unsigned long now = millis();
  servoMaintain();
  rcHealthCheck();

  if (state == OPEN_HOLD) {
    unsigned long elapsed = now - stateEnteredAt;
    if (elapsed >= OPEN_BEEP_MS) digitalWrite(BUZZER_PIN, BUZZ_OFF);
    if (elapsed >= OPEN_MS) enterState(LOCKED);
  }

  if (state == ALARMING) {
    unsigned long elapsed = now - stateEnteredAt;
    if (elapsed >= ALARM_MS) {
      enterState(LOCKED);
    } else {
      bool beepOn = ((elapsed / BEEP_PULSE) % 2) == 0;
      digitalWrite(BUZZER_PIN, beepOn ? BUZZ_ON : BUZZ_OFF);
    }
  }

  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    String uid = readUidHex();
    Serial.print(F("UID:")); Serial.println(uid);
    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
  }

  while (Serial.available()) {
    char ch = (char)Serial.read();
    if (ch == '\n' || ch == '\r') {
      if (serialBuf.length()) { handleCommand(serialBuf); serialBuf = ""; }
    } else {
      serialBuf += ch;
      if (serialBuf.length() > 24) serialBuf = "";
    }
  }
}
