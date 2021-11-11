#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <main.h>

const char* floatNames[] = {
  "Level-Dry",
  "Level-SumpPump",
  "Level-BackupPump",
  "Level-Flood"
};

ConfigParams configParams;

const char* getLevelName(int floatId) {
  if(FLOAT_NONE <= floatId && floatId <= FLOAT_FAIL) {
    return floatNames[floatId];
  }
  return "Level-Unknown";
}

unsigned char floatDebounceBits[FLOAT_COUNT] = { 0x00, 0x00, 0x00, 0x00 };

int floatCheck() {
  int rawVal = analogRead(PIN_A0);
  Serial.print(millis()/1000);Serial.print(" pin_A0: ");Serial.println(rawVal);

  int floatId = FLOAT_UNKNOWN;
  bool rangeMatch = false;
  for(int floatNdx = 0; floatNdx < FLOAT_COUNT; floatNdx++) {
    floatDebounceBits[floatNdx] = (floatDebounceBits[floatNdx] << 1);
    if(configParams.FloatRangeValues[floatNdx][0] <= rawVal && rawVal <= configParams.FloatRangeValues[floatNdx][1]) {
      rangeMatch = true;
      floatDebounceBits[floatNdx] += 1;
      if(configParams.DebounceMask == (floatDebounceBits[floatNdx] & configParams.DebounceMask)) {
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

bool checkWifi() {
  return WiFi.status() == WL_CONNECTED;
}

HTTPClient httpClient;
WiFiClient wifiClient;

void sendNotification(int eventId) {
  if(!checkWifi()) {
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
  if ((pval = config["MainLoopSec"])) configParams.MainLoopMs = pval * 1000;
  if ((pval = config["UpdateConfigSec"])) configParams.UpdateConfigMs = pval * 1000;
  if ((pval = config["LevelCheckSec"])) configParams.LevelCheckMs = pval * 1000;
  if ((pval = config["DebounceMask"])) configParams.DebounceMask = (unsigned char) pval;
  if ((pval = config["FloatBackupNotifyPeriodSec"])) configParams.FloatBackupNotifyPeriodMs = pval * 1000;
  if ((pval = config["FloatFailNotifyPeriodSec"])) configParams.FloatFailNotifyPeriodMs = pval * 1000;
  if ((pval = config["SumpThresholdNotifySec"])) configParams.SumpThresholdNotifyMs = pval * 1000;
  if ((pval = config["DryAgeNotifySec"])) configParams.DryAgeNotifyMs = pval * 1000;

  char lvlx[] = "Level_";
  for(int fl = FLOAT_NONE; fl < FLOAT_COUNT; fl++) {
    lvlx[5] = (char) (fl + 48);
    if (config.containsKey(lvlx)) {
      configParams.FloatRangeValues[fl][0] = config[lvlx][0];
      configParams.FloatRangeValues[fl][1] = config[lvlx][1];
    }
  }

  Serial.println("Configuration updated from json.");
}

unsigned long nextUpdateConfigMs = 0;
void updateConfig() {
  if(millis() < nextUpdateConfigMs) {
    return;
  }

  if(checkWifi()) {
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

  nextUpdateConfigMs = millis() + configParams.UpdateConfigMs;
  Serial.print("Next config update: ");Serial.println(nextUpdateConfigMs / 1000);
}

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
      nextFloatFailNotify = millis() + configParams.FloatFailNotifyPeriodMs;
    }
    return;
  }

  if(FLOAT_BACKUP == topFloat) {
    // Backup activated.
    if(millis() > nextFloatBackupNotify) {
      sendNotification(IOT_EVENT_BACKUP);
      nextFloatBackupNotify = millis() + configParams.FloatBackupNotifyPeriodMs;
    }
    return;
  }

  // At dry level.
  if(FLOAT_NONE == currentFloat) {
    if(!sumpNotifySuspended && (FLOAT_SUMP == topFloat) && (millis() - currentFloatSinceTime > configParams.SumpThresholdNotifyMs)) {
      // been dry for some time, and now water at sump level
      sendNotification(IOT_EVENT_SUMP);
      sumpNotifySuspended = true;
      return;
    }
    if(!dryNotifySuspended && (millis() - currentFloatSinceTime > configParams.DryAgeNotifyMs)) {
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

  nextLevelCheckTime = millis() + (dryNotifySuspended ? configParams.LevelCheckMs : configParams.MainLoopMs);
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);		 // Start the Serial communication to send messages to the computer
	delay(10);
	Serial.println('\n');

  Serial.println("Setting up Wifi.");
  WiFi.persistent(false);
  WiFi.mode(WIFI_OFF);
  WiFi.mode(WIFI_STA);
  IPAddress ip(NETWORK_IP);
  IPAddress gateway(NETWORK_GATEWAY);
  IPAddress subnet(NETWORK_SUBNET);
  WiFi.config(ip, gateway, subnet);
  WiFi.begin(WIFI_NETWORK, WIFI_PASSWORD);
  delay(60 * 1000);
  Serial.println("Setup done.");
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
    nextLoopMs = millis() + configParams.MainLoopMs;
  }

  yield();
}