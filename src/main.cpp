#include <Arduino.h>
#include <main.h>
#include <pins.h>

#define FLOAT_LEVEL_SUMP    0
#define FLOAT_LEVEL_BACKUP  1
#define FLOAT_LEVEL_FLOOD   2
#define FLOAT_LEVEL_COUNT   (FLOAT_LEVEL_FLOOD + 1)

enum ExecutionMode {
  Initializing,
  Monitoring,
  Pumping,
  Resting,
};
ExecutionMode execMode = Initializing;

bool testButtonPressed = false;
bool enableOnButtonPress = false;
IRAM_ATTR void onButtonPress() {
  if(enableOnButtonPress) {
    testButtonPressed = true;
  }
}

char textBuffer[TXT_BUFF_LEN];
void log(const char* format, ...)
{
  va_list args;
  va_start(args, format);

  snprintf(textBuffer, TXT_BUFF_LEN, "%lu ", millis());
  size_t txtLen = strlen(textBuffer);
  vsnprintf(textBuffer + txtLen, TXT_BUFF_LEN - txtLen, format, args);

  va_end(args);
  Serial.println(textBuffer);
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

struct FloatData {
  byte Pin;
  byte DebounceBits = 0x00;
  bool State = 0;
  bool LoggedState = 0;

  bool stateChanged() {
    return State != LoggedState;
  }

  void logState() {
    LoggedState = State;
  }
};

#define FLOAT_COUNT   3
FloatData floats[FLOAT_COUNT] = {
  { .Pin = FLOAT_SUMP_PIN },
  { .Pin = FLOAT_BACKUP_PIN },
  { .Pin = FLOAT_FLOOD_PIN },
};

#define TXT_FLOAT_STATE_LEN   12
char floatsState[TXT_FLOAT_STATE_LEN];
char* getFloatsState() {
  snprintf(floatsState, TXT_FLOAT_STATE_LEN, "[%d %d %d]",
      floats[FLOAT_LEVEL_SUMP].State, floats[FLOAT_LEVEL_BACKUP].State, floats[FLOAT_LEVEL_FLOOD].State);
  return floatsState;
}

byte inverseDebounceMask = ~AppConfig.DebounceMask;

void checkFloat(int floatLevel) {
  FloatData& fdata = floats[floatLevel];
  fdata.DebounceBits = fdata.DebounceBits << 1;
  int pinVal = digitalRead(fdata.Pin);
  if(pinVal == 0) {
    // Pin is pulled up. Will read 0 when float switch is on.
    fdata.DebounceBits += 1;
  }

  // So many consequtive times the switch was read as ON.
  if(AppConfig.DebounceMask == (fdata.DebounceBits & AppConfig.DebounceMask)) {
    fdata.State = true;
  }

  // So many consequtive times the switch was read as OFF.
  if(inverseDebounceMask == (fdata.DebounceBits | inverseDebounceMask)) {
    fdata.State = false;
  }

  return;
}

bool verifyFloatsState() {
  for(int lvl = FLOAT_LEVEL_COUNT - 1; lvl > 0; lvl--) {
    if(floats[lvl].State && !floats[lvl-1].State) {
      return false;
    }
  }
  return true;
}

unsigned long pumpStarted = 0;

void drivePump() {

  // Water too high. Need to start pumping.
  if(floats[FLOAT_LEVEL_BACKUP].State || floats[FLOAT_LEVEL_FLOOD].State) {
    digitalWrite(RELAY_PUMP_PIN, 1); // Doesn't hurt to assert we need to pump.
    if(execMode != Pumping) {
      pumpStarted = millis();
      int eventId = floats[FLOAT_LEVEL_BACKUP].State ? IOT_EVENT_BACKUP : IOT_EVENT_FLOOD;
      soundAlarm(eventId);
      sendNotification(eventId);
    }
    execMode = Pumping;
    return;
  }

  // Until the sump level float is off.
  if(!floats[FLOAT_LEVEL_SUMP].State) {
    if(execMode != Monitoring) {
      stopAlarm();
    }
    digitalWrite(RELAY_PUMP_PIN, 0);
    execMode = Monitoring;
    pumpStarted = 0;
    return;
  }

  // We end up here if the water leve is above sump but below backup level.
  // If we've been pumping for too long - give the pump a break. If the water
  // reaches backup level pump will start again unconditionally.
  if(execMode == Pumping) {
    if(millis() - pumpStarted >= AppConfig.MaxPumpRunTimeMs) {
      log("Giving the pump some rest.");
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
  return floats[FLOAT_LEVEL_SUMP].stateChanged()
    | floats[FLOAT_LEVEL_BACKUP].stateChanged()
    | floats[FLOAT_LEVEL_FLOOD].stateChanged();
}

void logFloatsState() {

  if(floatStateChanged()) {
    log("Floats state: %s", getFloatsState());

    floats[FLOAT_LEVEL_SUMP].logState();
    floats[FLOAT_LEVEL_BACKUP].logState();
    floats[FLOAT_LEVEL_FLOOD].logState();
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
    int msgLen = snprintf(textBuffer, TXT_BUFF_LEN, "Invalid floats state: %s.", getFloatsState());
    sendNotification(IOT_EVENT_BAD_STATE, textBuffer, msgLen);
    soundAlarm(IOT_EVENT_BAD_STATE);
  }

  unsigned long now = millis();
  if(floats[FLOAT_LEVEL_SUMP].State) {
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
    bool alarmOn = checkAlarm();
    log("Button was pressed - %s", (alarmOn ? "Stopping alarm." : "Running test."));
    if(alarmOn) {
      stopAlarm();
    }
    else {
      runTest();
    }
  }

  testButtonPressed = false;
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200); // Start the Serial communication to send messages to the computer
  delay(10);

  log("\nSetting up...");

  setupIO();
  ensureWiFi();

  updateConfig();

  execMode = Monitoring;

  log("Ready. Version: " SUMP_MONITOR_VERSION);
}

unsigned long lastLoopRun = 0;
bool resetNotificationSent = false;
bool flipBlueLed = false;
void loop() {

  if(millis() - lastLoopRun > AppConfig.MainLoopMs) {

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