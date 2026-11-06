#pragma once
// eink-web: Arduino Print, Printable and F() strings for the TRMNL build.
// Covers the Arduino API the firmware and its logging library use: text,
// characters, integers in any base, floating point and printf.
#include <stdarg.h>
#include <stdio.h>

#include <cstddef>
#include <cstdint>
#include <cstring>

#define DEC 10
#define HEX 16
#define OCT 8
#define BIN 2

class __FlashStringHelper;
#define FPSTR(p) (reinterpret_cast<const __FlashStringHelper *>(p))
#define F(s) FPSTR(s)

class String;
class Print;

class Printable {
 public:
  virtual ~Printable() = default;
  virtual size_t printTo(Print &p) const = 0;
};

class Print {
 public:
  virtual ~Print() = default;
  virtual size_t write(uint8_t) = 0;
  virtual size_t write(const uint8_t *buffer, size_t size) {
    size_t n = 0;
    while (size--) n += write(*buffer++);
    return n;
  }
  size_t write(const char *s) { return s ? write(reinterpret_cast<const uint8_t *>(s), strlen(s)) : 0; }
  size_t write(const char *buf, size_t size) { return write(reinterpret_cast<const uint8_t *>(buf), size); }
  virtual int availableForWrite() { return 0; }
  virtual void flush() {}

  size_t print(const __FlashStringHelper *s) { return write(reinterpret_cast<const char *>(s)); }
  size_t print(const String &s);
  size_t print(const char *s) { return write(s); }
  size_t print(char c) { return write(static_cast<uint8_t>(c)); }
  size_t print(unsigned char n, int base = DEC) { return printNumber(n, base); }
  size_t print(int n, int base = DEC) { return printSigned(n, base); }
  size_t print(unsigned int n, int base = DEC) { return printNumber(n, base); }
  size_t print(long n, int base = DEC) { return printSigned(n, base); }
  size_t print(unsigned long n, int base = DEC) { return printNumber(n, base); }
  size_t print(long long n, int base = DEC) { return printSigned(n, base); }
  size_t print(unsigned long long n, int base = DEC) { return printNumber(n, base); }
  size_t print(double n, int digits = 2) { return printFloat(n, digits); }
  size_t print(const Printable &p) { return p.printTo(*this); }

  size_t println() { return write("\r\n"); }
  template <typename T>
  size_t println(const T &v) {
    const size_t n = print(v);
    return n + println();
  }
  template <typename T>
  size_t println(const T &v, int fmt) {
    const size_t n = print(v, fmt);
    return n + println();
  }

  size_t printf(const char *format, ...) __attribute__((format(printf, 2, 3))) {
    char small[256];
    va_list args;
    va_start(args, format);
    const int len = vsnprintf(small, sizeof(small), format, args);
    va_end(args);
    if (len < 0) return 0;
    if (static_cast<size_t>(len) < sizeof(small)) return write(small, static_cast<size_t>(len));
    char *big = new char[len + 1];
    va_start(args, format);
    vsnprintf(big, len + 1, format, args);
    va_end(args);
    const size_t n = write(big, static_cast<size_t>(len));
    delete[] big;
    return n;
  }

 private:
  size_t printSigned(long long n, int base) {
    if (base == DEC && n < 0) return print('-') + printNumber(static_cast<unsigned long long>(-n), DEC);
    return printNumber(static_cast<unsigned long long>(n), base);
  }
  size_t printNumber(unsigned long long n, int base) {
    if (base < 2) base = DEC;
    char buf[8 * sizeof(n) + 1];
    char *p = &buf[sizeof(buf) - 1];
    *p = '\0';
    do {
      const int d = static_cast<int>(n % base);
      *--p = static_cast<char>(d < 10 ? '0' + d : 'A' + d - 10);
      n /= base;
    } while (n);
    return write(p);
  }
  size_t printFloat(double n, int digits) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%.*f", digits < 0 ? 0 : digits, n);
    return write(buf);
  }
};
