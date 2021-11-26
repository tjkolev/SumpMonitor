#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>
#include <main.h>

extern HTTPClient httpClient;
extern WiFiClient wifiClient;

ApplicationConfig AppConfig;

#define CONFIG_URL    IOT_API_BASE_URL "/config?deviceid=sump"

template <typename T>
void updateValue(const JsonObject &jconfig, const char* key, T &currentValue, int multiplier) {
  if (jconfig.containsKey(key)) {
    currentValue = jconfig[key].as<T>() * multiplier;
  }
}

template <typename T>
void updateValue(const JsonObject &jconfig, const char* key, T &currentValue) {
  if (jconfig.containsKey(key)) {
    currentValue = jconfig[key].as<T>();
  }
}

void parseConfig(const char* json) {
  StaticJsonBuffer<1024> jsonBuffer;
  JsonObject& config = jsonBuffer.parseObject(json);
  if (!config.success()) {
    log("Failed to parse json:\n%s", json);
  }

  updateValue(config, "MainLoopSec", AppConfig.MainLoopMs, 1000);
  updateValue(config, "UpdateConfigSec", AppConfig.UpdateConfigMs, 1000);
  updateValue(config, "DebounceMask", AppConfig.DebounceMask);
  updateValue(config, "MinNotifyPeriodSec", AppConfig.MinNotifyPeriodMs, 1000);
  updateValue(config, "DryAgeNotifySec", AppConfig.DryAgeNotifyMs, 1000);
  updateValue(config, "MaxPumpRunTimeSec", AppConfig.MaxPumpRunTimeMs, 1000);
  updateValue(config, "PumpTestRunSec", AppConfig.PumpTestRunMs, 1000);
  updateValue(config, "DebugLog", AppConfig.DebugLog);

  AppConfig.inverseDebounceMask = ~AppConfig.DebounceMask;

  log("Configuration pulled from %s", CONFIG_URL);
  log(json);
}

unsigned long lastConfigUpdate = 0;
void updateConfig(bool force) {
  unsigned long now = millis();
  if(!force && (now - lastConfigUpdate < AppConfig.UpdateConfigMs)) {
    return;
  }
  lastConfigUpdate = now;

  if(ensureWiFi()) {
    httpClient.setTimeout(10000);
    httpClient.begin(wifiClient, CONFIG_URL);
    int code = httpClient.GET();
    if(code == 200) {
      String body = httpClient.getString();
      parseConfig(body.c_str());
    }
    else {
      log("Cannot pull config. Http code %d", code);
    }
    httpClient.end();
  }
  else {
   log("Cannot pull config: no wifi.");
  }

}
