#include <Arduino.h>
#include <main.h>
#include <pins.h>

#define FLOAT_LEVEL_SUMP    0
#define FLOAT_LEVEL_BACKUP  1
#define FLOAT_LEVEL_FLOOD   2
#define FLOAT_LEVEL_COUNT   (FLOAT_LEVEL_FLOOD + 1)

char textBuffer[TXT_BUFF_LEN];

enum ExecutionMode {
  Initializing,
  Monitoring,
  Pumping,
};
ExecutionMode execMode = Initializing;

bool testButtonPressed = false;
bool enableOnButtonPress = false;
IRAM_ATTR void onButtonPress() {
  if(enableOnButtonPress) {
    testButtonPressed = true;
  }
}

void setupIO() {

  pinMode(FLOAT_SUMP_PIN, INPUT_PULLUP);
  pinMode(FLOAT_BACKUP_PIN, INPUT_PULLUP);
  pinMode(FLOAT_FLOOD_PIN, INPUT_PULLUP);

  pinMode(RELAY_PUMP_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  pinMode(LED_BLUE_PIN, OUTPUT);
  digitalWrite(LED_BLUE_PIN, 1); // off
  pinMode(LED_RED_PIN, OUTPUT);
  digitalWrite(LED_RED_PIN, 1); // off

  pinMode(BUTTON_TEST_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(BUTTON_TEST_PIN), onButtonPress, RISING);
}

byte floatPin[FLOAT_LEVEL_COUNT] = { FLOAT_SUMP_PIN, FLOAT_BACKUP_PIN, FLOAT_FLOOD_PIN };
byte floatDebounceBits[FLOAT_LEVEL_COUNT] = { 0x00, 0x00, 0x00};
bool floatState[FLOAT_LEVEL_COUNT] = { false, false, false };
bool loggedFloatState[FLOAT_LEVEL_COUNT] = { false, false, false };
byte inverseDebounceMask = ~AppConfig.DebounceMask;

void checkFloat(int floatLevel) {
  floatDebounceBits[floatLevel] = floatDebounceBits[floatLevel] << 1;
  int pinVal = digitalRead(floatPin[floatLevel]);

  if(pinVal == 0) {
    // Pin is pulled up. Will read 0 when float switch is on.
    floatDebounceBits[floatLevel] += 1;
  }

  // So many consequtive times the switch was read as ON.
  if(AppConfig.DebounceMask == (floatDebounceBits[floatLevel] & AppConfig.DebounceMask)) {
    floatState[floatLevel] = true;
  }

  // So many consequtive times the switch was read as OFF.
  if(inverseDebounceMask == (floatDebounceBits[floatLevel] | inverseDebounceMask)) {
    floatState[floatLevel] = false;
  }

  return;
}

bool verifyFloatsState() {
  for(int lvl = FLOAT_LEVEL_COUNT - 1; lvl > 0; lvl--) {
    if(floatState[lvl] && !floatState[lvl-1]) {
      return false;
    }
  }
  return true;
}

unsigned long pumpStarted;
void drivePump() {

  // Water too high. Need to start pumping.
  if(floatState[FLOAT_LEVEL_BACKUP] || floatState[FLOAT_LEVEL_FLOOD]) {
    digitalWrite(RELAY_PUMP_PIN, 1); // Doesn't hurt to assert we need to pump.
    if(execMode != Pumping) {
      execMode = Pumping;
      pumpStarted = millis();
      int eventId = floatState[FLOAT_LEVEL_BACKUP] ? IOT_EVENT_BACKUP : IOT_EVENT_FLOOD;
      soundAlarm(eventId);
      sendNotification(eventId);
    }
    return;
  }

  // Until the sump pump float is off.
  if(!floatState[FLOAT_LEVEL_SUMP]) {
    digitalWrite(RELAY_PUMP_PIN, 0);
    if(execMode == Pumping) {
      execMode = Monitoring;
      pumpStarted = 0;
      stopAlarm();
    }
    return;
  }

  // But if we've been pumping for too long - stop even if water level is higher.
  if(execMode == Pumping) {
    if(pumpStarted + AppConfig.MaxPumpRunTimeMs > millis()) {
      Serial.println("Giving the pump some rest.");
      digitalWrite(RELAY_PUMP_PIN, 0);
      execMode = Monitoring;
      pumpStarted = 0;
    }
  }

}

void testPump() {
  digitalWrite(RELAY_PUMP_PIN, 1);
  delay(AppConfig.PumpTestRunMs);
  digitalWrite(RELAY_PUMP_PIN, 0);
}

bool floatStateChanged() {
  return (floatState[FLOAT_LEVEL_SUMP] ^ loggedFloatState[FLOAT_LEVEL_SUMP])
    | (floatState[FLOAT_LEVEL_BACKUP] ^ loggedFloatState[FLOAT_LEVEL_BACKUP])
    | (floatState[FLOAT_LEVEL_FLOOD] ^ loggedFloatState[FLOAT_LEVEL_FLOOD]);
}

void logFloatsState() {

  if(floatStateChanged()) {
    snprintf(textBuffer, TXT_BUFF_LEN, "Floats state: [%d %d %d].",
          floatState[FLOAT_LEVEL_SUMP], floatState[FLOAT_LEVEL_BACKUP], floatState[FLOAT_LEVEL_FLOOD]);
    Serial.println(textBuffer);

    loggedFloatState[FLOAT_LEVEL_SUMP] = floatState[FLOAT_LEVEL_SUMP];
    loggedFloatState[FLOAT_LEVEL_BACKUP] = floatState[FLOAT_LEVEL_BACKUP];
    loggedFloatState[FLOAT_LEVEL_FLOOD] = floatState[FLOAT_LEVEL_FLOOD];
  }
}

unsigned long sumpLevel_LastOnTime = 0;
unsigned long sumpLevel_LastOffTime = 0;
bool trackDryPeriod = false;

void checkAllFloats() {

  for(int lvl = 0; lvl < FLOAT_LEVEL_COUNT; lvl++) {
    checkFloat(lvl);
  }

  if(!verifyFloatsState()) {
    int msgLen = snprintf(textBuffer, TXT_BUFF_LEN, "Invalid floats state: [%d %d %d].",
        floatState[FLOAT_LEVEL_SUMP], floatState[FLOAT_LEVEL_BACKUP], floatState[FLOAT_LEVEL_FLOOD]);
    sendNotification(IOT_EVENT_BAD_STATE, textBuffer, msgLen);
    soundAlarm(IOT_EVENT_BAD_STATE);
  }

  unsigned long now = millis();
  if(floatState[FLOAT_LEVEL_SUMP]) {
    sumpLevel_LastOnTime =
    sumpLevel_LastOffTime = now; // Move up (reset) the off time to avoid millis overflow issues.
    if(!trackDryPeriod) {
      trackDryPeriod = true;
      sendNotification(IOT_EVENT_SUMP);
    }
  }
  else {
    sumpLevel_LastOffTime = now;
  }

  if(trackDryPeriod && (sumpLevel_LastOnTime + AppConfig.DryAgeNotifyMs < sumpLevel_LastOffTime)) {
    sendNotification(IOT_EVENT_DRY);
    trackDryPeriod = false;
  }

  drivePump();

  logFloatsState();
}

void runTest() {
  testAlarm();
  testPump();
}

void checkButtonPress() {

  if(testButtonPressed) {
    Serial.print("Button was pressed! ");
    if(checkAlarm()) {
      Serial.println("Stopping alarm.");
      stopAlarm();
    }
    else {
      Serial.println("Running test.");
      runTest();
    }
  }

  testButtonPressed = false;
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);		 // Start the Serial communication to send messages to the computer
	delay(10);
	Serial.println("\nSetting up...");

  setupIO();
  ensureWiFi();

  updateConfig();

  execMode = Monitoring;
}

unsigned long lastLoopRun = 0;
bool resetNotificationSent = false;
bool flipBlueLed = false;
void loop() {

  if(lastLoopRun + AppConfig.MainLoopMs < millis()) {

    if(!resetNotificationSent) {
      // Here because sometimes wifi is not ready in startup.
      resetNotificationSent = sendNotification(IOT_EVENT_RESET);
    }

    if(updateConfig()) {
      inverseDebounceMask = ~AppConfig.DebounceMask;
    }

    checkButtonPress();

    checkAllFloats(); // Gist of the work.

    soundAlarm(); // Keeps the alarms going on if needed.

    enableOnButtonPress = true;

    if(wifiConnected()) {
      digitalWrite(LED_BLUE_PIN, flipBlueLed ? 0 : 1);
      flipBlueLed = !flipBlueLed;
    }

    lastLoopRun = millis();
  }

  yield();
}