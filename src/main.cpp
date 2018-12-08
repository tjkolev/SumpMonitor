#include <Arduino.h>
#include <ArduinoJson.h>
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
  Serial.print("\npin_A0: ");Serial.println(rawVal);

  int floatId = FLOAT_UNKNOWN;
  for(int floatNdx = 0; floatNdx < FLOAT_COUNT; floatNdx++) {
    floatDebounceBits[floatNdx] = (floatDebounceBits[floatNdx] << 1);
    if(configParams.FloatRangeValues[floatNdx][0] <= rawVal && rawVal <= configParams.FloatRangeValues[floatNdx][1]) {
      // Float activated.
      floatDebounceBits[floatNdx] += 1;
      if(configParams.DebounceMask == (floatDebounceBits[floatNdx] & configParams.DebounceMask)) {
        floatId = floatNdx;
      }
    }
  }

  return floatId;
}

int lastActivatedFloat = FLOAT_UNKNOWN;
unsigned int lastLevelActivationMs = 0;
unsigned int nextLevelCheckMs = 0;

void onFloatCheck(int topFloat) {
  Serial.print("sec: ");Serial.println(millis()/1000);
  Serial.print("topFloat: ");Serial.println(getLevelName(topFloat));
  Serial.print("lastFloat: ");Serial.println(getLevelName(lastActivatedFloat));
}

void checkWaterLevel() {
  if(millis() < nextLevelCheckMs) {
    return;
  }

  int topFloat = floatCheck();
  onFloatCheck(topFloat);

  if(FLOAT_UNKNOWN != topFloat) {
    lastActivatedFloat = topFloat;
    if(topFloat >= FLOAT_SUMP) {
      lastLevelActivationMs = millis();
    }
  }

  nextLevelCheckMs = millis() + configParams.LevelCheckMs;
}

unsigned int nextUpdateConfigMs = 0;
StaticJsonBuffer<1024> jsonBuffer;
void updateConfig() {
  if(millis() < nextUpdateConfigMs) {
    return;
  }

  char json[] = "{\"MainLoopMs\":1000,\"UpdateConfigMs\":1800000,\"LevelCheckMs\":6000,\"DebounceMask\":7,"
                 "\"Level0\":[0,20],\"Level1\":[80,100],\"Level2\":[300,500],\"Level3\":[800,1024]}";
  JsonObject& config = jsonBuffer.parseObject(json);
  if (config.success()) {
    configParams.MainLoopMs = config["MainLoopMs"];
    configParams.UpdateConfigMs = config["UpdateConfigMs"];
    configParams.LevelCheckMs = config["LevelCheckMs"];
    configParams.DebounceMask = config["DebounceMask"];

    configParams.FloatRangeValues[0][0] = config["Level0"][0];
    configParams.FloatRangeValues[0][1] = config["Level0"][1];
    configParams.FloatRangeValues[1][0] = config["Level1"][0];
    configParams.FloatRangeValues[1][1] = config["Level1"][1];
    configParams.FloatRangeValues[2][0] = config["Level2"][0];
    configParams.FloatRangeValues[2][1] = config["Level2"][1];
    configParams.FloatRangeValues[3][0] = config["Level3"][0];
    configParams.FloatRangeValues[3][1] = config["Level3"][1];
  }
  else {
    Serial.println("Failed to parse json.");
  }
  nextUpdateConfigMs = millis() + configParams.UpdateConfigMs;
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);		 // Start the Serial communication to send messages to the computer
	delay(10);
	Serial.println('\n');
}

unsigned int nextLoopMs = 0;
void loop() {
  // put your main code here, to run repeatedly:
  if(millis() >= nextLoopMs)
  {
    updateConfig();
    checkWaterLevel();
    nextLoopMs = millis() + configParams.MainLoopMs;
  }

  yield();
}