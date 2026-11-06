#pragma once
// eink-web: the Arduino core for the TRMNL build.
//
// Time and delays run on the cooperative scheduler (core/sim_coop.h); the
// button pin and battery ADC read what the page reports; other pins and
// peripherals accept writes and do nothing.
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <string>
#include <type_traits>

#include "driver/gpio.h"
#include "esp_arduino_version.h"
#include "esp_attr.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sim_coop.h"

#define PROGMEM
#define ICACHE_RODATA_ATTR
#define PGM_P const char *
#define PSTR(s) (s)
#define pgm_read_byte(addr) (*reinterpret_cast<const uint8_t *>(addr))
#define pgm_read_word(addr) (*reinterpret_cast<const uint16_t *>(addr))
#define pgm_read_dword(addr) (*reinterpret_cast<const uint32_t *>(addr))
#define pgm_read_ptr(addr) (*(void *const *)(addr))
#define SET_LOOP_TASK_STACK_SIZE(bytes)

#define HIGH 0x1
#define LOW 0x0
#define INPUT 0x01
#define OUTPUT 0x03
#define PULLUP 0x04
#define INPUT_PULLUP 0x05
#define PULLDOWN 0x08
#define INPUT_PULLDOWN 0x09
#define OPEN_DRAIN 0x10
#define OUTPUT_OPEN_DRAIN 0x13
#define RISING 0x01
#define FALLING 0x02
#define CHANGE 0x03

typedef uint8_t byte;
typedef bool boolean;
typedef unsigned int word;

#define _min(a, b) ((a) < (b) ? (a) : (b))
#define _max(a, b) ((a) > (b) ? (a) : (b))
#define constrain(amt, low, high) ((amt) < (low) ? (low) : ((amt) > (high) ? (high) : (amt)))
#define bitRead(value, bit) (((value) >> (bit)) & 0x01)
#define bitSet(value, bit) ((value) |= (1UL << (bit)))
#define bitClear(value, bit) ((value) &= ~(1UL << (bit)))
#define bitWrite(value, bit, bitvalue) ((bitvalue) ? bitSet(value, bit) : bitClear(value, bit))
#define bit(b) (1UL << (b))

// By value: decltype of a conditional on two lvalues is a reference, which
// would return a reference to an argument.
template <typename A, typename B>
constexpr typename std::common_type<A, B>::type max(A a, B b) {
  return a > b ? a : b;
}
template <typename A, typename B>
constexpr typename std::common_type<A, B>::type min(A a, B b) {
  return a < b ? a : b;
}

extern "C" int eink_web_pin_read(int pin);
extern "C" int eink_web_battery_mv(void);
extern "C" void eink_web_restart(void);

inline unsigned long millis() {
  using namespace std::chrono;
  static const auto start = steady_clock::now();
  return static_cast<unsigned long>(duration_cast<milliseconds>(steady_clock::now() - start).count());
}
inline unsigned long micros() {
  using namespace std::chrono;
  static const auto start = steady_clock::now();
  return static_cast<unsigned long>(duration_cast<microseconds>(steady_clock::now() - start).count());
}
inline void delay(unsigned long ms) { simcoopSleepMs(ms); }
inline void delayMicroseconds(unsigned int us) {
  if (us >= 1000) simcoopSleepMs(us / 1000);
}
inline void yield() { simcoopYield(); }

inline void pinMode(int, int) {}
inline void digitalWrite(int, int) {}
inline int digitalRead(int pin) { return eink_web_pin_read(pin); }
inline void attachInterrupt(int, void (*)(void), int) {}
inline void detachInterrupt(int) {}
inline int digitalPinToInterrupt(int pin) { return pin; }
inline void interrupts() {}
inline void noInterrupts() {}

// The firmware reads the battery through a 2:1 divider and doubles the
// reading, so the ADC sees half of what the page reports.
inline uint32_t analogReadMilliVolts(int = 0) { return static_cast<uint32_t>(eink_web_battery_mv() / 2); }
inline uint16_t analogRead(int pin) { return static_cast<uint16_t>(analogReadMilliVolts(pin) * 4095 / 3300); }
inline void analogReadResolution(int) {}
inline void analogSetAttenuation(int) {}
inline void analogSetPinAttenuation(int, int) {}
inline float temperatureRead() { return 35.0f; }

// LED PWM (used for the status LED and buzzer on some boards).
inline uint32_t ledcSetup(uint8_t, uint32_t freq, uint8_t) { return freq; }
inline void ledcAttachPin(uint8_t, uint8_t) {}
inline void ledcDetachPin(uint8_t) {}
inline bool ledcAttach(uint8_t, uint32_t, uint8_t) { return true; }
inline bool ledcDetach(uint8_t) { return true; }
inline bool ledcWrite(uint8_t, uint32_t) { return true; }
inline uint32_t ledcWriteTone(uint8_t, uint32_t freq) { return freq; }

inline long random(long howbig) { return howbig > 0 ? std::rand() % howbig : 0; }
inline long random(long howsmall, long howbig) { return howsmall >= howbig ? howsmall : howsmall + random(howbig - howsmall); }
inline void randomSeed(unsigned long seed) { std::srand(static_cast<unsigned>(seed)); }
inline long map(long x, long in_min, long in_max, long out_min, long out_max) {
  return in_max == in_min ? out_min : (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

inline char *dtostrf(double val, signed char width, unsigned char prec, char *sout) {
  sprintf(sout, "%*.*f", width, prec, val);
  return sout;
}
inline char *itoa(int value, char *str, int base) {
  if (base == 16) sprintf(str, "%x", value);
  else if (base == 8) sprintf(str, "%o", value);
  else sprintf(str, "%d", value);
  return str;
}
inline char *ltoa(long value, char *str, int base) { return itoa(static_cast<int>(value), str, base); }
inline char *utoa(unsigned value, char *str, int base) {
  if (base == 16) sprintf(str, "%x", value);
  else sprintf(str, "%u", value);
  return str;
}
inline char *ultoa(unsigned long value, char *str, int base) { return utoa(static_cast<unsigned>(value), str, base); }

// Time: the page's clock is always set, so SNTP is a no-op and getLocalTime()
// returns local wall-clock time.
inline void configTime(long, int, const char *, const char * = nullptr, const char * = nullptr) {}
inline void configTzTime(const char *tz, const char *, const char * = nullptr, const char * = nullptr) {
  if (tz) {
    setenv("TZ", tz, 1);
    tzset();
  }
}
inline bool getLocalTime(struct tm *info, uint32_t = 5000) {
  const time_t now = time(nullptr);
  localtime_r(&now, info);
  return true;
}

struct EspClass {
  void restart() { eink_web_restart(); }
  uint32_t getFreeHeap() { return 180 * 1024; }  // in the range an ESP32-C3 reports
  uint32_t getMinFreeHeap() { return getFreeHeap(); }
  uint32_t getHeapSize() { return 320 * 1024; }
  uint32_t getMaxAllocHeap() { return 110 * 1024; }
  uint32_t getFreePsram() { return 0; }
  uint32_t getPsramSize() { return 0; }
  uint32_t getFlashChipSize() { return 4 * 1024 * 1024; }
  const char *getChipModel() { return "ESP32-C3 (browser)"; }
  uint8_t getChipRevision() { return 4; }
  uint32_t getCpuFreqMHz() { return 80; }
  const char *getSdkVersion() { return "eink-web"; }
  uint64_t getEfuseMac() { return 0x0100000000AEULL; }
};
inline EspClass ESP;

#include "HardwareSerial.h"
#include "Print.h"
#include "Stream.h"
#include "WString.h"
