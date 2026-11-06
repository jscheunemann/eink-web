#pragma once
// IPAddress (from the crosspoint-simulator WiFi shim).
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "WString.h"

class IPAddress {
  uint8_t bytes[4] = {0, 0, 0, 0};

public:
  IPAddress() {}
  IPAddress(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
    bytes[0] = a;
    bytes[1] = b;
    bytes[2] = c;
    bytes[3] = d;
  }
  String toString() const {
    char buf[16];
    snprintf(buf, sizeof(buf), "%u.%u.%u.%u", bytes[0], bytes[1], bytes[2],
             bytes[3]);
    return String(buf);
  }
  explicit operator String() const { return toString(); }
  uint8_t operator[](int i) const { return bytes[i % 4]; }
  uint8_t &operator[](int i) { return bytes[i % 4]; }
  bool operator==(const IPAddress &o) const {
    return memcmp(bytes, o.bytes, sizeof(bytes)) == 0;
  }
  bool operator!=(const IPAddress &o) const { return !(*this == o); }
};
