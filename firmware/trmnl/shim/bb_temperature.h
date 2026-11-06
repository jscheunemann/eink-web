#pragma once
// eink-web: the optional temperature sensor is not present in the browser.
#include <stdint.h>
#define TEMP_SUCCESS 0
#define TEMP_ERROR -1
class BBTemp {
 public:
  int init(int = -1, int = -1, int = 0, uint32_t = 100000) { return TEMP_ERROR; }
  int type() { return -1; }
  int temperature() { return 220; }  // tenths of a degree C
  int humidity() { return 0; }
  int start() { return TEMP_ERROR; }
  int stop() { return TEMP_ERROR; }
  int getSample() { return TEMP_ERROR; }
};
