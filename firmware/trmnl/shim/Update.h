#pragma once
// eink-web: over-the-air firmware updates are refused in the browser.
#include <stddef.h>
#include <stdint.h>

#include "Stream.h"
#define UPDATE_SIZE_UNKNOWN 0xFFFFFFFF
class UpdateClass {
 public:
  bool begin(size_t = UPDATE_SIZE_UNKNOWN, int = 0, int = -1, uint8_t = 0, const char * = nullptr) { return false; }
  size_t writeStream(Stream &) { return 0; }
  size_t write(uint8_t *, size_t) { return 0; }
  bool end(bool = false) { return false; }
  bool isFinished() { return false; }
  bool hasError() { return true; }
  const char *errorString() { return "firmware updates are not available in the browser"; }
  void printError(Stream &) {}
  void abort() {}
};
inline UpdateClass Update;
