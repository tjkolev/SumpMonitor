#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>
#include <main.h>

HTTPClient httpClient;
WiFiClient wifiClient;

ApplicationConfig AppConfig;

// const char* floatNames[] = {
//   "Level-Dry",
//   "Level-SumpPump",
//   "Level-BackupPump",
//   "Level-Flood"
// };

// const char* getLevelName(int floatId) {
//   if(FLOAT_NONE <= floatId && floatId <= FLOAT_FAIL) {
//     return floatNames[floatId];
//   }
//   return "Level-Unknown";
// }

void sendNotification(int eventId) {
  if(!ensureWiFi()) {
    Serial.println("Cannot notify: no wifi.");
    return;
  }

  httpClient.setTimeout(10000);
  switch(eventId){
    case IOT_EVENT_DRY:
      httpClient.begin(wifiClient, IOT_API_BASE_URL "/notify?eventid=sump_dry");
      break;
    case IOT_EVENT_SUMP:
      httpClient.begin(wifiClient, IOT_API_BASE_URL "/notify?eventid=sump_sump");
      break;
    case IOT_EVENT_BACKUP:
      httpClient.begin(wifiClient, IOT_API_BASE_URL "/notify?eventid=sump_backup");
      break;
    case IOT_EVENT_FLOOD:
      httpClient.begin(wifiClient, IOT_API_BASE_URL "/notify?eventid=sump_flood");
      break;
    case IOT_EVENT_RESET:
      httpClient.begin(wifiClient, IOT_API_BASE_URL "/notify?eventid=sump_reset");
      break;
    
    default:
      return;
  }

  int code = httpClient.POST(NULL, 0);
  if(code == 200){
    Serial.println("Notification sent.");
  }
  else {
    Serial.print("Failed to send notification. Http code ");Serial.println(code);
  }
  httpClient.end();
}

void parseConfig(const char* json) {
  StaticJsonBuffer<1024> jsonBuffer;
  JsonObject& config = jsonBuffer.parseObject(json);
  if (!config.success()) {
    Serial.println("Failed to parse json.");
    return;
  }

  unsigned int pval;
  if ((pval = config["MainLoopSec"])) AppConfig.MainLoopMs = pval * 1000;
  if ((pval = config["UpdateConfigSec"])) AppConfig.UpdateConfigMs = pval * 1000;
  if ((pval = config["LevelCheckSec"])) AppConfig.LevelCheckMs = pval * 1000;
  if ((pval = config["DebounceMask"])) AppConfig.DebounceMask = (unsigned char) pval;
  if ((pval = config["FloatBackupNotifyPeriodSec"])) AppConfig.FloatBackupNotifyPeriodMs = pval * 1000;
  if ((pval = config["FloatFailNotifyPeriodSec"])) AppConfig.FloatFailNotifyPeriodMs = pval * 1000;
  if ((pval = config["SumpThresholdNotifySec"])) AppConfig.SumpThresholdNotifyMs = pval * 1000;
  if ((pval = config["DryAgeNotifySec"])) AppConfig.DryAgeNotifyMs = pval * 1000;

  char lvlx[] = "Level_";
  for(int fl = FLOAT_NONE; fl < FLOAT_COUNT; fl++) {
    lvlx[5] = (char) (fl + 48);
    if (config.containsKey(lvlx)) {
      AppConfig.FloatRangeValues[fl][0] = config[lvlx][0];
      AppConfig.FloatRangeValues[fl][1] = config[lvlx][1];
    }
  }

  Serial.println("Configuration updated from json.");
}

unsigned long nextUpdateConfigMs = 0;
void updateConfig() {
  if(millis() < nextUpdateConfigMs) {
    return;
  }

  if(ensureWiFi()) {
    httpClient.setTimeout(10000);
    httpClient.begin(wifiClient, IOT_API_BASE_URL "/config?deviceid=sump");
    int code = httpClient.GET();
    if(code == 200) {
      String body = httpClient.getString();
      parseConfig(body.c_str());
    }
    else {
      Serial.print("Cannot pull config. Http code ");Serial.println(code);
    }
    httpClient.end();
  }
  else {
    Serial.println("Cannot pull config: no wifi.");
  }

  nextUpdateConfigMs = millis() + AppConfig.UpdateConfigMs;
  Serial.print("Next config update: ");Serial.println(nextUpdateConfigMs / 1000);
}
