#ifndef main_h
#define main_h

#include <sensitive.h>

#define SUMP_MONITOR_VERSION  __DATE__ " " __TIME__
#define DEVICE_ID       "sump"

// Events in order of severity
#define IOT_EVENT_NONE        0
#define IOT_EVENT_DRY         1
#define IOT_EVENT_RESET       2
#define IOT_EVENT_SUMP        3
#define IOT_EVENT_BAD_STATE   4
#define IOT_EVENT_BACKUP      5
#define IOT_EVENT_FLOOD       6

#define IOT_API_BASE_URL "http://" IOT_SERVICE_FQDN "/cgi-bin/luci/iot-helper/api"

struct ApplicationConfig {
  unsigned long MainLoopMs = 1 * 1000; // every second
  unsigned long UpdateConfigMs = 5 * 60 * 1000; // every 5 minutes
  byte DebounceMask = 0x07; // Successive readings as bits (111) or (000) to confirm float's state.
  unsigned long MinNotifyPeriodMs = 15 * 60 * 1000; // 15 minutes
  unsigned long DryAgeNotifyMs = 12 * 60 * 60 * 1000; // 12 hours
  unsigned long MaxPumpRunTimeMs = 2 * 60 * 1000; // 2 minutes
  unsigned long PumpTestRunMs = 3 * 1000; // 3 seconds
  unsigned long PumpTestRunMinIntervalMs = 30 * 60 * 1000; // 30 minutes
  bool DebugLog = true;
  bool PostLog = true;

  // evaluated fields
  byte inverseDebounceMask = ~DebounceMask;
};

extern ApplicationConfig AppConfig;

void log(const char* format, ...);
#define logd(...) {if(AppConfig.DebugLog) log(__VA_ARGS__);};
char* formatMillis(char* buff, unsigned long milliseconds);

bool ensureWiFi();
bool wifiConnected();
void updateConfig(bool force = false);
void soundAlarm(int alarmEvent = IOT_EVENT_NONE);
void stopAlarm();
bool checkAlarm();
void testAlarm();
bool sendNotification(int eventId, const char* msg = NULL, int msgLen = 0);
void postLog(const char* logMsg);

#endif // main_h