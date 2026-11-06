#pragma once
// eink-web: StreamString is a String that can also be written and read as a
// Stream, as in the ESP32 Arduino core (the firmware casts it to String&).
#include "Stream.h"
#include "WString.h"

class StreamString : public String, public Stream {
 public:
  StreamString() {}
  explicit StreamString(const String &str) : String(str) {}

  using Print::write;
  size_t write(uint8_t c) override {
    *static_cast<String *>(this) += static_cast<char>(c);
    return 1;
  }
  size_t write(const uint8_t *buf, size_t size) override {
    for (size_t i = 0; i < size; i++) write(buf[i]);
    return size;
  }
  int available() override { return static_cast<int>(length() - pos_); }
  int read() override { return pos_ < length() ? static_cast<uint8_t>(c_str()[pos_++]) : -1; }
  int peek() override { return pos_ < length() ? static_cast<uint8_t>(c_str()[pos_]) : -1; }
  void flush() override {}

 private:
  size_t pos_ = 0;
};
