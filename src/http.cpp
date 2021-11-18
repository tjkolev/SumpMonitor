#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>
#include <main.h>

HTTPClient httpClient;
WiFiClient wifiClient;

ApplicationConfig AppConfig;

unsigned long lastNotifyTime[IOT_EVENT_COUNT] = { 0, 0, 0, 0, 0, 0 };

bool sendNotification(int eventId, uint8_t* msg, int msgLen) {
  if(!ensureWiFi()) {
    Serial.println("Cannot notify: no wifi.");
    return false;
  }

  const char* url;
  switch(eventId) {
    case IOT_EVENT_DRY:
      url = IOT_API_BASE_URL "/notify?eventid=sump_dry";
      break;
    case IOT_EVENT_SUMP:
      url = IOT_API_BASE_URL "/notify?eventid=sump_sump";
      break;
    case IOT_EVENT_BACKUP:
      url = IOT_API_BASE_URL "/notify?eventid=sump_backup";
      break;
    case IOT_EVENT_FLOOD:
      url = IOT_API_BASE_URL "/notify?eventid=sump_flood";
      break;
    case IOT_EVENT_RESET:
      url = IOT_API_BASE_URL "/notify?eventid=sump_reset";
      break;
    case IOT_EVENT_BAD_STATE:
      url = IOT_API_BASE_URL "/notify?eventid=sump_badstate";
      break;

    default:
      Serial.print("No notification defined for ");Serial.println(eventId);
      return true;
  }

  unsigned long now = millis();
  if(0 != lastNotifyTime[eventId] && (lastNotifyTime[eventId] + AppConfig.MinNotifyPeriodMs > now)) {
    return true;
  }
  lastNotifyTime[eventId] = now;

  bool result = false;
  Serial.print(eventId);

  httpClient.begin(wifiClient, url);
  httpClient.setTimeout(10000);
  int code = httpClient.POST(msg, msgLen);
  if(code == 200){
    Serial.println(" - Notification sent.");
    result = true;
  }
  else {
    Serial.print(" - Failed to send notification. Http code ");Serial.println(code);
  }
  httpClient.end();

  return result;
}

template <typename T>
bool updateValue(unsigned int newValue, T &currentValue, int multiplier = 1000) {
  if (newValue) {
    T cfgValue = newValue * multiplier;
    if(cfgValue != currentValue) {
      currentValue = cfgValue;
      return true;
    }
  }
  return false;
}

bool parseConfig(const char* json) {
  StaticJsonBuffer<1024> jsonBuffer;
  JsonObject& config = jsonBuffer.parseObject(json);
  if (!config.success()) {
    Serial.println("Failed to parse json.");
    return false;
  }

  bool updated = updateValue(config["MainLoopSec"], AppConfig.MainLoopMs);
  updated |= updateValue(config["UpdateConfigSec"], AppConfig.UpdateConfigMs);
  updated |= updateValue(config["DebounceMask"], AppConfig.DebounceMask, 1);
  updated |= updateValue(config["MinNotifyPeriodSec"], AppConfig.MinNotifyPeriodMs);
  updated |= updateValue(config["DryAgeNotifySec"], AppConfig.DryAgeNotifyMs);
  updated |= updateValue(config["MaxPumpRunTimeSec"], AppConfig.MaxPumpRunTimeMs);
  updated |= updateValue(config["PumpTestRunSec"], AppConfig.PumpTestRunMs);

  Serial.print("Configuration read from json - "); updated ? Serial.println("updated:") : Serial.println("no changes.");
  if(updated) {
    Serial.println(json);
  }

  return updated;
}

unsigned long lastConfigUpdate = 0;
bool updateConfig() {
  unsigned long now = millis();
  if(lastConfigUpdate + AppConfig.UpdateConfigMs > now) {
    return false;
  }
  lastConfigUpdate = now;

  bool result = false;
  if(ensureWiFi()) {
    httpClient.setTimeout(10000);
    httpClient.begin(wifiClient, IOT_API_BASE_URL "/config?deviceid=sump");
    int code = httpClient.GET();
    if(code == 200) {
      String body = httpClient.getString();
      result = parseConfig(body.c_str());
    }
    else {
      Serial.print("Cannot pull config. Http code ");Serial.println(code);
    }
    httpClient.end();
  }
  else {
    Serial.println("Cannot pull config: no wifi.");
  }

  return result;
}
