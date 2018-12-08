#ifndef main_h
#define main_h

#define FLOAT_UNKNOWN (-1)
#define FLOAT_NONE 0
#define FLOAT_SUMP 1
#define FLOAT_BACKUP 2
#define FLOAT_FAIL 3
#define FLOAT_COUNT (FLOAT_FAIL+1)

struct ConfigParams {
  unsigned int MainLoopMs = 1 * 1000; // every second
  unsigned int UpdateConfigMs = 30 * 60 * 1000;
  unsigned int LevelCheckMs = 5 * 1000;
  unsigned char DebounceMask = 0x07; // Successive positive readings as bits (111)
  int FloatRangeValues[FLOAT_COUNT][2] = {
    { 0, 20 },
    { 300, 325 },
    { 535, 555 },
    { 1000, 1024 }
  };
};

#endif // main_h