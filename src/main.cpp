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
};
ExecutionMode execMode = Initializing;

bool testButtonPressed = false;
bool enableOnButtonPress = false;
IRAM_ATTR void onButtonPress() {
  Serial.println("onButtonPress invoked.");
  if(enableOnButtonPress) {
    testButtonPressed = true;
  }
}

char* formatMillis(char* buff, unsigned long milliseconds) {
  // returns the millisconds formatted as d.hh:mm:ss.lll
  unsigned long tmillis = milliseconds;
  int msecs = (int) (tmillis % 1000);

  unsigned long tsecs;
  int secs = (int)((tsecs = tmillis / 1000) % 60);

  unsigned long tmins;
  int mins = (int)((tmins = tsecs / 60) % 60);

  unsigned long thours;
  int hours = (int)((thours = tmins / 60) % 24);

  int days = (int)(thours / 24);

  sprintf(buff, "%d.%02d:%02d:%02d.%03d", days, hours, mins, secs, msecs);
  return buff;
}

#define LOG_BUFF_LEN 600
char logMsgBuffer[LOG_BUFF_LEN];
char millisFmtBuffer[24];
void log(const char* format, ...)
{
  va_list args;
  va_start(args, format);

  size_t txtLen;

  txtLen = snprintf(logMsgBuffer, LOG_BUFF_LEN, "%s ", formatMillis(millisFmtBuffer, millis()));
  vsnprintf(logMsgBuffer + txtLen, LOG_BUFF_LEN - txtLen, format, args);

  va_end(args);
  Serial.println(logMsgBuffer);
  postLog(logMsgBuffer);
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
  //attachInterrupt(digitalPinToInterrupt(BUTTON_TEST_PIN), onButtonPress, RISING);
}

struct FloatData {
  byte Pin;
  byte DebounceBits = 0x00;
  bool On = 0;
  bool LoggedState = 0;

  bool stateChanged() {
    return On != LoggedState;
  }

  void logState() {
    LoggedState = On;
  }
};

#define FLOAT_COUNT   3
FloatData floats[FLOAT_COUNT] = {
  { .Pin = FLOAT_SUMP_PIN },
  { .Pin = FLOAT_BACKUP_PIN },
  { .Pin = FLOAT_FLOOD_PIN },
};

#define TXT_FLOAT_STATE_LEN   16
char floatsState[TXT_FLOAT_STATE_LEN];
const char* getFloatsState() {
  snprintf(floatsState, TXT_FLOAT_STATE_LEN, "[%d %d %d]",
      floats[FLOAT_LEVEL_SUMP].On, floats[FLOAT_LEVEL_BACKUP].On, floats[FLOAT_LEVEL_FLOOD].On);
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
    fdata.On = true;
  }

  // So many consequtive times the switch was read as OFF.
  if(inverseDebounceMask == (fdata.DebounceBits | inverseDebounceMask)) {
    fdata.On = false;
  }

  return;
}

bool verifyFloatsState() {
  snprintf(floatsState, TXT_FLOAT_STATE_LEN, "[%d %d %d]",
    floats[FLOAT_LEVEL_SUMP].DebounceBits, floats[FLOAT_LEVEL_BACKUP].DebounceBits, floats[FLOAT_LEVEL_FLOOD].DebounceBits);
  logd("Debounce bits: %s", floatsState);

  for(int lvl = FLOAT_LEVEL_COUNT - 1; lvl > 0; lvl--) {
    if(floats[lvl].On && !floats[lvl-1].On) {
      return false;
    }
  }
  return true;
}

unsigned long pumpStarted = 0;

void drivePump() {

  // Water too high. Need to start pumping.
  if(floats[FLOAT_LEVEL_BACKUP].On || floats[FLOAT_LEVEL_FLOOD].On) {
    digitalWrite(RELAY_PUMP_PIN, 1); // Doesn't hurt to assert we need to pump.
    if(execMode != Pumping) {
      pumpStarted = millis();
      int eventId = floats[FLOAT_LEVEL_BACKUP].On ? IOT_EVENT_BACKUP : IOT_EVENT_FLOOD;
      soundAlarm(eventId);
      sendNotification(eventId);
    }
    execMode = Pumping;
    return;
  }

  // Until the sump level float is off.
  if(!floats[FLOAT_LEVEL_SUMP].On) {
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

// Testing pump only so often
unsigned long lastPumpTest = 0;
void testPump() {
  if(millis() - lastPumpTest > AppConfig.PumpTestRunMinIntervalMs) {
    digitalWrite(RELAY_PUMP_PIN, 1);
    delay(AppConfig.PumpTestRunMs);
    digitalWrite(RELAY_PUMP_PIN, 0);

    lastPumpTest = millis();
  }
}

bool floatStateChanged() {
  return floats[FLOAT_LEVEL_SUMP].stateChanged()
    | floats[FLOAT_LEVEL_BACKUP].stateChanged()
    | floats[FLOAT_LEVEL_FLOOD].stateChanged();
}

void logFloatsState() {

  if(floatStateChanged()) {
    logd("Floats state: %s", getFloatsState());

    floats[FLOAT_LEVEL_SUMP].logState();
    floats[FLOAT_LEVEL_BACKUP].logState();
    floats[FLOAT_LEVEL_FLOOD].logState();
  }
}

unsigned long sumpLevel_LastOffTime = 0;
bool sumpConsideredDry = true;

void checkAllFloats() {

  for(int lvl = 0; lvl < FLOAT_LEVEL_COUNT; lvl++) {
    checkFloat(lvl);
  }

  if(!verifyFloatsState()) {
    const char* floatStates = getFloatsState();
    log("Invalid floats state: %s.", floatStates);
    sendNotification(IOT_EVENT_BAD_STATE, floatStates, -1);
    soundAlarm(IOT_EVENT_BAD_STATE);
  }

  // figure out when to call the sump dry
  if(sumpConsideredDry && floats[FLOAT_LEVEL_SUMP].On && floats[FLOAT_LEVEL_SUMP].stateChanged()) {
    sumpConsideredDry = false;
    sendNotification(IOT_EVENT_SUMP);
  }
  if(!sumpConsideredDry && !floats[FLOAT_LEVEL_SUMP].On && !floats[FLOAT_LEVEL_SUMP].stateChanged() && (millis() - sumpLevel_LastOffTime > AppConfig.DryAgeNotifyMs)) {
    sumpConsideredDry = true;
    sendNotification(IOT_EVENT_DRY);
  }
  if(!sumpConsideredDry && !floats[FLOAT_LEVEL_SUMP].On && floats[FLOAT_LEVEL_SUMP].stateChanged()) {
    sumpLevel_LastOffTime = millis();
  }

  drivePump();

  logFloatsState();
}

void runTest() {
  testAlarm();
  testPump();
}

void checkButtonPress() {

  enableOnButtonPress = false;

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
  enableOnButtonPress = true;
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200); // Start the Serial communication to send messages to the computer
  delay(100);

  log("\nSetting up...");

  setupIO();
  ensureWiFi();

  updateConfig(true);

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

    updateConfig();

    checkButtonPress();

    checkAllFloats(); // Gist of the work.

    soundAlarm(); // Keeps the alarms going on if needed.

    if(wifiConnected()) {
      digitalWrite(LED_BLUE_PIN, flipBlueLed ? 0 : 1);
      flipBlueLed = !flipBlueLed;
    }
    else {
      digitalWrite(LED_BLUE_PIN, 1); //off
    }

    lastLoopRun = millis();
  }

  yield();
}