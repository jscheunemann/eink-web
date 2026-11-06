#pragma once
// eink-web: the optional SCD41 CO2 sensor is not present in the browser.
#include <stdint.h>
class SCD41 {
 public:
  int init(int = -1, int = -1, int = 0, uint32_t = 100000) { return -1; }
  int start() { return -1; }
  int stop() { return -1; }
  int getSample() { return -1; }
  int co2() { return 0; }
  int temperature() { return 0; }
  int humidity() { return 0; }
};
#define SCD41_SUCCESS 0
