#include <Arduino.h>
#include <main.h>
#include <pins.h>

#define BEEP_PERIOD_BAD_STATE   (1 * 60 * 1000) // one minute
#define BEEP_PERIOD_BACKUP      (15 * 1000) // 15 seconds
#define BEEP_PERIOD_FLOOD       (5 * 1000) // 5 seconds

int alarmEvent = 0;

// Using an active buzzer.
// Blocking beep. Mind the length.
void beepPattern(int beepCount, int beepLengthMs, int restMs) {

  digitalWrite(LED_RED_PIN, 0); //on

  for(int n = 0; n < beepCount - 1; n++) {
    if(n > 0) {
      delay(restMs);
    }
    digitalWrite(BUZZER_PIN, 1);
    delay(beepLengthMs);
    digitalWrite(BUZZER_PIN, 0);
  }
}

void beepBadState() {
  beepPattern(2, 400, 100);
}


void beepFlood() {
  beepPattern(3, 600, 100);
}


void beepBackupActivated() {
  beepPattern(3, 300, 100);
}

void beepAlarm(unsigned long beepInterval, unsigned long &lastBeep, std::function<void(void)> beepRoutine) {
  unsigned long now = millis();
  if(lastBeep + beepInterval > now)
  {
    return;
  }
  beepRoutine();
  lastBeep = now;
}

unsigned long lastBeepBadState = 0;
unsigned long lastBeepFlood = 0;
unsigned long lastBeepBackup = 0;

void soundAlarm(int alarmEventId) {

  if(alarmEventId > 0) {
      alarmEvent = alarmEventId;
  }

  if(alarmEvent <= 0) {
      return;
  }

  switch(alarmEvent) {

    case IOT_EVENT_BACKUP:
        beepAlarm(BEEP_PERIOD_BACKUP, lastBeepBackup, beepBackupActivated);
      break;
    case IOT_EVENT_FLOOD:
        beepAlarm(BEEP_PERIOD_FLOOD, lastBeepFlood, beepFlood);
      break;
    case IOT_EVENT_BAD_STATE:
        beepAlarm(BEEP_PERIOD_BAD_STATE, lastBeepBadState, beepBadState);
      break;
  }
}

void stopAlarm() {
    alarmEvent = 0;
    lastBeepBadState =
    lastBeepFlood =
    lastBeepBackup = 0;

    digitalWrite(BUZZER_PIN, 0);
    digitalWrite(LED_RED_PIN, 1); //off
}

bool checkAlarm() {
    return alarmEvent > 0;
}

void testAlarm() {
  digitalWrite(LED_RED_PIN, 0); // on
  digitalWrite(LED_BLUE_PIN, 0); // on

  beepBadState();

  delay(2000);
  beepBackupActivated();

  delay(2000);
  beepFlood();

  digitalWrite(LED_RED_PIN, 1); //off
  digitalWrite(LED_BLUE_PIN, 0); // on
}