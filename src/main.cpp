#include <Arduino.h>
  #include <SPI.h>
  #include <MFRC522.h>
  #include <Servo.h>
  #include <SoftwareSerial.h>

  constexpr uint8_t RST_PIN    = 9;
  constexpr uint8_t SS_PIN     = 10;
  constexpr uint8_t SERVO_PIN  = 6;
  constexpr uint8_t BUZZER_PIN = 7;
  constexpr uint8_t BT_RX_PIN  = 2;
  constexpr uint8_t BT_TX_PIN  = 3;

  // Active-LOW buzzer module: HIGH = silent, LOW = sound
  constexpr uint8_t BUZZ_OFF = HIGH;
  constexpr uint8_t BUZZ_ON  = LOW;

  constexpr int SERVO_LOCKED = 0;
  constexpr int SERVO_OPEN   = 90;

  constexpr unsigned long OPEN_MS    = 3000;
  constexpr unsigned long ALARM_MS   = 1000;  // 5 beeps total
  constexpr unsigned long BEEP_PULSE = 100;   // 100 ms on / 100 ms off

  MFRC522 rfid(SS_PIN, RST_PIN);
  Servo doorServo;
  SoftwareSerial btSerial(BT_RX_PIN, BT_TX_PIN);

  enum DoorState { LOCKED, OPEN_HOLD, ALARMING };
  DoorState state = LOCKED;
  unsigned long stateEnteredAt = 0;

  String serialBuf = "";
  String btBuf     = "";

  void enterState(DoorState s) {
    state = s;
    stateEnteredAt = millis();
    switch (s) {
      case LOCKED:
        doorServo.write(SERVO_LOCKED);
        digitalWrite(BUZZER_PIN, BUZZ_OFF);
        Serial.println(F("STATE:LOCKED"));
        btSerial.println(F("STATE:LOCKED"));
        break;
      case OPEN_HOLD:
        digitalWrite(BUZZER_PIN, BUZZ_OFF);
        doorServo.write(SERVO_OPEN);
        Serial.println(F("STATE:OPEN"));
        btSerial.println(F("STATE:OPEN"));
        break;
      case ALARMING:
        doorServo.write(SERVO_LOCKED);
        digitalWrite(BUZZER_PIN, BUZZ_ON);  // first beep starts immediately
        Serial.println(F("STATE:ALARM"));
        btSerial.println(F("STATE:ALARM"));
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

  void pollStream(Stream &s, String &buf) {
    while (s.available()) {
      char ch = (char)s.read();
      if (ch == '\n' || ch == '\r') {
        if (buf.length()) { handleCommand(buf); buf = ""; }
      } else {
        buf += ch;
        if (buf.length() > 24) buf = "";
      }
    }
  }

  void setup() {
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, BUZZ_OFF);

    Serial.begin(9600);
    btSerial.begin(9600);
    SPI.begin();
    rfid.PCD_Init();

    doorServo.attach(SERVO_PIN);
    doorServo.write(SERVO_LOCKED);

    Serial.println(F("READY"));
    btSerial.println(F("READY"));
  }

  void loop() {
    unsigned long now = millis();

    if (state == OPEN_HOLD && now - stateEnteredAt >= OPEN_MS) {
      enterState(LOCKED);
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
      Serial.print(F("UID:"));   Serial.println(uid);
      btSerial.print(F("UID:")); btSerial.println(uid);
      rfid.PICC_HaltA();
      rfid.PCD_StopCrypto1();
    }

    pollStream(Serial,   serialBuf);
    pollStream(btSerial, btBuf);
  }