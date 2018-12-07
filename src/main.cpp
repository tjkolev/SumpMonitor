#include <Arduino.h>

#define FLOAT_UNKNOWN (-1)
#define FLOAT_NONE 0
#define FLOAT_SUMP 1
#define FLOAT_BACKUP 2
#define FLOAT_FAIL 3
#define FLOAT_COUNT (FLOAT_FAIL+1)

const char* floatNames[] = {
  "Level-Dry",
  "Level-SumpPump",
  "Level-BackupPump",
  "Level-Flood"
};

const char* getLevelName(int floatId) {
  if(FLOAT_NONE <= floatId && floatId <= FLOAT_FAIL) {
    return floatNames[floatId];
  }
  return "Level-Unknown";
}

unsigned char debounceMask = 0x07; // Successive positive readings as bits (111)
unsigned char floatDebounceBits[FLOAT_COUNT] = { 0x00, 0x00, 0x00, 0x00 };
int floatRangeValues[FLOAT_COUNT][2] = {
  { 0, 20 },
  { 80, 100 },
  { 300, 500 },
  { 800, 1024 }
};

int floatCheck() {
  int rawVal = analogRead(PIN_A0);
  Serial.print("\npin_A0: ");Serial.println(rawVal);

  int floatId = FLOAT_UNKNOWN;
  for(int floatNdx = 0; floatNdx < FLOAT_COUNT; floatNdx++) {
    floatDebounceBits[floatNdx] = (floatDebounceBits[floatNdx] << 1);
    if(floatRangeValues[floatNdx][0] <= rawVal && rawVal <= floatRangeValues[floatNdx][1]) {
      // Float activated.
      floatDebounceBits[floatNdx] += 1;
      if(debounceMask == (floatDebounceBits[floatNdx] & debounceMask)) {
        floatId = floatNdx;
      }
    }
  }

  return floatId;
}

int lastActivatedFloat = FLOAT_UNKNOWN;
unsigned int lastLevelActivationMs = 0;
unsigned int nextLevelCheckMs = 0;
unsigned int checkInMs = 5 * 1000; // every 5 seconds

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

  nextLevelCheckMs = millis() + checkInMs;
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);		 // Start the Serial communication to send messages to the computer
	delay(10);
	Serial.println('\n');
}

unsigned int nextLoopMs = 0;
unsigned int loopMs = 1 * 1000;
void loop() {
  // put your main code here, to run repeatedly:
  if(millis() >= nextLoopMs)
  {
    checkWaterLevel();
    nextLoopMs = millis() + loopMs;
  }

  yield();
}