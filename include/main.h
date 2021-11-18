#ifndef main_h
#define main_h

#include <sensitive.h>

#define IOT_EVENT_DRY 0
#define IOT_EVENT_SUMP 1
#define IOT_EVENT_BACKUP 2
#define IOT_EVENT_FLOOD 3
#define IOT_EVENT_RESET 4
#define IOT_EVENT_BAD_STATE 5
#define IOT_EVENT_COUNT (IOT_EVENT_BAD_STATE+1)

#define IOT_API_BASE_URL "http://" IOT_SERVICE_FQDN "/cgi-bin/luci/iot-helper/api"

struct ApplicationConfig {
  unsigned long MainLoopMs = 1 * 1000; // every second
  unsigned long UpdateConfigMs = 1 * 60 * 1000;
  byte DebounceMask = 0x07; // Successive readings as bits (111) or (000) to confirm float's state.
  unsigned long MinNotifyPeriodMs = 15 * 60 * 1000; // 15 minutes
  unsigned long DryAgeNotifyMs = 12 * 60 * 60 * 1000; // 12 hours
  unsigned long MaxPumpRunTimeMs = 5 * 60 * 1000; // 5 minutes
  unsigned long PumpTestRunMs = 3 * 1000; // 3 seconds
};

extern ApplicationConfig AppConfig;

bool ensureWiFi();
bool wifiConnected();
bool updateConfig();
void soundAlarm(int alarmEvent = 0);
void stopAlarm();
bool checkAlarm();
void testAlarm();
bool sendNotification(int eventId, uint8_t* msg = NULL, int msgLen = 0);

#endif // main_h