#ifndef main_h
#define main_h

#include <sensitive.h>

#define FLOAT_UNKNOWN (-1)
#define FLOAT_NONE 0
#define FLOAT_SUMP 1
#define FLOAT_BACKUP 2
#define FLOAT_FAIL 3
#define FLOAT_COUNT (FLOAT_FAIL+1)

#define IOT_EVENT_DRY 0
#define IOT_EVENT_SUMP 1
#define IOT_EVENT_BACKUP 2
#define IOT_EVENT_FLOOD 3
#define IOT_EVENT_RESET 4

#define IOT_API_BASE_URL "http://" ROUTER_IP "/cgi-bin/luci/iot-helper/api"

struct ConfigParams {
  unsigned long MainLoopMs = 1 * 1000; // every second
  unsigned long UpdateConfigMs = 1 * 60 * 1000;
  unsigned long LevelCheckMs = 5 * 1000;
  unsigned char DebounceMask = 0x07; // Successive positive readings as bits (111)
  unsigned long FloatBackupNotifyPeriodMs = 20 * 60 * 1000; // 20 minutes
  unsigned long FloatFailNotifyPeriodMs = 5 * 60 * 1000; // 5 minutes
  unsigned long SumpThresholdNotifyMs = 6 * 60 * 60 * 1000; // 6 hours
  unsigned long DryAgeNotifyMs = 12 * 60 * 60 * 1000; // 12 hours

  int FloatRangeValues[FLOAT_COUNT][2] = {
    { 0, 60 },
    { 200, 400 },
    { 480, 600 },
    { 800, 1024 }
  };
};

#endif // main_h