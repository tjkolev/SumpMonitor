#include <Arduino.h>
#include <main.h>

enum ValidFloatState {
  Dry = 0x0000,
  Water = 0x0001,
  SumpPump = 0x0011,
  BackupPump = 0x0111,
  Flood = 0x1111,
};

unsigned char floatDebounceBits[FLOAT_COUNT] = { 0x00, 0x00, 0x00, 0x00 };

int floatCheck() {
  int rawVal = analogRead(PIN_A0);
  Serial.print(millis()/1000);Serial.print(" pin_A0: ");Serial.println(rawVal);

  int floatId = FLOAT_UNKNOWN;
  bool rangeMatch = false;
  for(int floatNdx = 0; floatNdx < FLOAT_COUNT; floatNdx++) {
    floatDebounceBits[floatNdx] = (floatDebounceBits[floatNdx] << 1);
    if(AppConfig.FloatRangeValues[floatNdx][0] <= rawVal && rawVal <= AppConfig.FloatRangeValues[floatNdx][1]) {
      rangeMatch = true;
      floatDebounceBits[floatNdx] += 1;
      if(AppConfig.DebounceMask == (floatDebounceBits[floatNdx] & AppConfig.DebounceMask)) {
        floatId = floatNdx;
      }
    }
  }

  if(!rangeMatch) {
    Serial.print("No level range match for: "); Serial.println(rawVal);
  }

  return floatId;
}

int currentFloat = FLOAT_UNKNOWN;
unsigned long currentFloatSinceTime = 0;
unsigned long nextLevelCheckTime = 0;

unsigned long nextFloatFailNotify = 0;
unsigned long nextFloatBackupNotify = 0;
bool sumpNotifySuspended = true;
bool dryNotifySuspended = true;

void onFloatCheck(int topFloat) {
  // Serial.print("sec: ");Serial.println(millis()/1000);
  // Serial.print("topFloat: ");Serial.println(getLevelName(topFloat));
  // Serial.print("currentFloat: ");Serial.println(getLevelName(currentFloat));

  if(topFloat > FLOAT_NONE) {
    sumpNotifySuspended = dryNotifySuspended = false;
  }

  // Notifications from most to least critical.
  if(FLOAT_FAIL == topFloat) {
    // Flooding imminent
    if(millis() > nextFloatFailNotify) {
      sendNotification(IOT_EVENT_FLOOD);
      nextFloatFailNotify = millis() + AppConfig.FloatFailNotifyPeriodMs;
    }
    return;
  }

  if(FLOAT_BACKUP == topFloat) {
    // Backup activated.
    if(millis() > nextFloatBackupNotify) {
      sendNotification(IOT_EVENT_BACKUP);
      nextFloatBackupNotify = millis() + AppConfig.FloatBackupNotifyPeriodMs;
    }
    return;
  }

  // At dry level.
  if(FLOAT_NONE == currentFloat) {
    if(!sumpNotifySuspended && (FLOAT_SUMP == topFloat) && (millis() - currentFloatSinceTime > AppConfig.SumpThresholdNotifyMs)) {
      // been dry for some time, and now water at sump level
      sendNotification(IOT_EVENT_SUMP);
      sumpNotifySuspended = true;
      return;
    }
    if(!dryNotifySuspended && (millis() - currentFloatSinceTime > AppConfig.DryAgeNotifyMs)) {
      // notify it's considered dry
      sendNotification(IOT_EVENT_DRY);
      dryNotifySuspended = true;
      return;
    }
  }
}

void checkWaterLevel() {
  if(millis() < nextLevelCheckTime) {
    return;
  }

  int topFloat = floatCheck();
  onFloatCheck(topFloat);

  if(FLOAT_UNKNOWN != topFloat && topFloat != currentFloat) {
    currentFloat = topFloat;
    currentFloatSinceTime = millis();
  }

  nextLevelCheckTime = millis() + (dryNotifySuspended ? AppConfig.LevelCheckMs : AppConfig.MainLoopMs);
}


void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);		 // Start the Serial communication to send messages to the computer
	delay(10);
	Serial.println('\n');

  ensureWiFi();
}

bool resetNotificationSent = false;
unsigned long nextLoopMs = 0;
void loop() {
  // put your main code here, to run repeatedly:
  if(millis() >= nextLoopMs)
  {
    if(!resetNotificationSent) {
      sendNotification(IOT_EVENT_RESET);
      resetNotificationSent = true;
    }

    updateConfig();
    checkWaterLevel();
    nextLoopMs = millis() + AppConfig.MainLoopMs;
  }

  yield();
}