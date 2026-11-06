#pragma once
// eink-web: I2C bus with nothing attached. Every transfer reports "no device",
// which the firmware treats as an absent sensor.
#include <stddef.h>
#include <stdint.h>

class TwoWire {
 public:
  bool begin(int = -1, int = -1, uint32_t = 0) { return true; }
  bool end() { return true; }
  bool setClock(uint32_t) { return true; }
  void beginTransmission(uint16_t) {}
  uint8_t endTransmission(bool = true) { return 2; }  // 2 = address NACK
  size_t requestFrom(uint16_t, size_t, bool = true) { return 0; }
  size_t write(uint8_t) { return 1; }
  size_t write(const uint8_t *, size_t n) { return n; }
  int available() { return 0; }
  int read() { return -1; }
};
inline TwoWire Wire;
inline TwoWire Wire1;
