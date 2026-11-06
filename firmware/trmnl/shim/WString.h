#pragma once

// ArduinoJson auto-detects Arduino's String class via the ARDUINO macro, which
// is not defined in the simulator's native build. Enable the support manually
// so .as<String>() and serialize-to-String resolve to ArduinoJson's built-in
// converter (matching firmware behaviour) instead of the std::string-only path.
#ifndef ARDUINOJSON_ENABLE_ARDUINO_STRING
#define ARDUINOJSON_ENABLE_ARDUINO_STRING 1
#endif

#include <cstdint>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <string>

class String {
public:
  std::string s;
  String() {}
  String(const char *str) : s(str ? str : "") {}
  explicit String(const std::string &str) : s(str) {}
  explicit String(char c) : s(1, c) {}
  explicit String(unsigned char n, unsigned char base = 10) : s(fromInt(n, base)) {}
  explicit String(int n, unsigned char base = 10) : s(fromInt(n, base)) {}
  explicit String(unsigned int n, unsigned char base = 10) : s(fromInt(n, base)) {}
  explicit String(long n, unsigned char base = 10) : s(fromInt(n, base)) {}
  explicit String(unsigned long n, unsigned char base = 10) : s(fromInt(n, base)) {}
  explicit String(long long n, unsigned char base = 10) : s(fromInt(n, base)) {}
  explicit String(unsigned long long n, unsigned char base = 10) : s(fromInt(n, base)) {}
  explicit String(float n, unsigned int decimals = 2) : s(fromFloat(n, decimals)) {}
  explicit String(double n, unsigned int decimals = 2) : s(fromFloat(n, decimals)) {}
  static std::string fromFloat(double n, unsigned int decimals) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%.*f", static_cast<int>(decimals), n);
    return buf;
  }
  template <typename T>
  static std::string fromInt(T n, unsigned char base) {
    if (base == 10) return std::to_string(n);
    if (base < 2 || base > 36) base = 10;
    const bool neg = n < 0;
    unsigned long long v = neg ? 0ULL - static_cast<unsigned long long>(n) : static_cast<unsigned long long>(n);
    std::string out;
    do {
      const int d = static_cast<int>(v % base);
      out.insert(out.begin(), static_cast<char>(d < 10 ? '0' + d : 'a' + d - 10));
      v /= base;
    } while (v);
    return neg ? "-" + out : out;
  }
  auto begin() const { return s.begin(); }
  auto end() const { return s.end(); }
  void toLowerCase() {
    for (char &c : s)
      c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }

  String &operator=(const char *str) {
    s = str ? str : "";
    return *this;
  }
  String &operator=(const std::string &str) {
    s = str;
    return *this;
  }
  String &operator+=(const char *str) {
    s += str;
    return *this;
  }
  String &operator+=(char c) {
    s += c;
    return *this;
  }
  String &operator+=(const String &other) {
    s += other.s;
    return *this;
  }

  bool startsWith(const String &prefix) const { return s.find(prefix.s) == 0; }
  bool startsWith(const char *prefix) const { return s.find(prefix) == 0; }
  String substring(unsigned int from,
                   unsigned int to = (unsigned int)-1) const {
    if (from >= s.length())
      return String();
    return String(
        s.substr(from, to == (unsigned int)-1 ? std::string::npos : to - from));
  }
  int lastIndexOf(char c) const { return s.rfind(c); }
  int lastIndexOf(char c, int from) const { return from < 0 ? -1 : static_cast<int>(s.rfind(c, static_cast<size_t>(from))); }
  int lastIndexOf(const char *str) const { return static_cast<int>(s.rfind(str)); }
  int lastIndexOf(const String &str) const { return static_cast<int>(s.rfind(str.s)); }
  void toCharArray(char *buf, unsigned int bufsize, unsigned int index = 0) const {
    if (!buf || !bufsize) return;
    const size_t n = index < s.size() ? std::min<size_t>(bufsize - 1, s.size() - index) : 0;
    if (n) memcpy(buf, s.data() + index, n);
    buf[n] = 0;
  }
  void getBytes(unsigned char *buf, unsigned int bufsize, unsigned int index = 0) const {
    toCharArray(reinterpret_cast<char *>(buf), bufsize, index);
  }
  bool equalsIgnoreCase(const String &o) const {
    if (s.size() != o.s.size()) return false;
    for (size_t i = 0; i < s.size(); i++)
      if (tolower(static_cast<unsigned char>(s[i])) != tolower(static_cast<unsigned char>(o.s[i]))) return false;
    return true;
  }
  void toUpperCase() { for (auto &c : s) c = static_cast<char>(toupper(static_cast<unsigned char>(c))); }
  float toFloat() const { return strtof(s.c_str(), nullptr); }
  double toDouble() const { return strtod(s.c_str(), nullptr); }
  int indexOf(char c) const { return s.find(c); }
  int indexOf(char c, size_t pos) const { return s.find(c, pos); }
  int indexOf(const char *substr) const { return s.find(substr); }
  int indexOf(const char *substr, size_t pos) const {
    return s.find(substr, pos);
  }
  int indexOf(const String &substr) const { return s.find(substr.s); }
  int indexOf(const String &substr, size_t pos) const {
    return s.find(substr.s, pos);
  }
  char charAt(size_t index) const {
    return index < s.length() ? s[index] : '\0';
  }
  long toInt() const { return std::stoi(s); }
  int read() const {
    if (readPos < s.length())
      return s[readPos++];
    return -1;
  }
  bool endsWith(const String &suffix) const {
    return s.size() >= suffix.s.size() &&
           s.substr(s.size() - suffix.s.size()) == suffix.s;
  }
  bool endsWith(const char *suffix) const {
    return suffix && s.size() >= strlen(suffix) &&
           s.substr(s.size() - strlen(suffix)) == suffix;
  }
  bool equals(const String &other) const { return s == other.s; }
  bool equals(const char *other) const { return s == (other ? other : ""); }
  void trim() {
    size_t first = s.find_first_not_of(" \t\n\r");
    if (first == std::string::npos) {
      s.clear();
      return;
    }
    size_t last = s.find_last_not_of(" \t\n\r");
    s = s.substr(first, (last - first + 1));
  }
  void replace(char find, char replaceWith) {
    for (char &c : s) {
      if (c == find)
        c = replaceWith;
    }
  }
  void replace(const char *find, const char *replaceWith) {
    if (!find || !*find)
      return;
    const std::string f(find);
    const std::string r(replaceWith ? replaceWith : "");
    size_t pos = 0;
    while ((pos = s.find(f, pos)) != std::string::npos) {
      s.replace(pos, f.size(), r);
      pos += r.size();
    }
  }
  void replace(const String &find, const String &replaceWith) {
    replace(find.s.c_str(), replaceWith.s.c_str());
  }
  bool isEmpty() const { return s.empty(); }
  size_t length() const { return s.length(); }
  void remove(size_t index) {
    if (index < s.size()) s.erase(index);
  }
  bool reserve(size_t capacity) {
    s.reserve(capacity);
    return true;
  }
  const char *c_str() const { return s.c_str(); }
  bool operator==(const char *other) const { return s == (other ? other : ""); }
  bool operator!=(const char *other) const { return !(*this == other); }
  bool operator==(const String &other) const { return s == other.s; }
  bool operator!=(const String &other) const { return s != other.s; }

  size_t write(uint8_t c) {
    s.push_back(c);
    return 1;
  }
  size_t write(const uint8_t *buffer, size_t size) {
    s.append((const char *)buffer, size);
    return size;
  }
  size_t write(const char *buffer, size_t size) {
    s.append(buffer, size);
    return size;
  }

  // ArduinoJson's writer for ::String calls concat() to append chunks.
  unsigned char concat(const char *str) {
    if (!str)
      return 0;
    s += str;
    return 1;
  }
  unsigned char concat(const char *str, unsigned int length) {
    if (!str)
      return 0;
    s.append(str, length);
    return 1;
  }
  unsigned char concat(const String &other) {
    s += other.s;
    return 1;
  }

  operator const char *() const { return c_str(); }

private:
  mutable size_t readPos = 0;
};

inline String operator+(const String &lhs, const String &rhs) {
  String res(lhs);
  res += rhs.c_str();
  return res;
}

inline String operator+(const String &lhs, const char *rhs) {
  String res(lhs);
  res += rhs;
  return res;
}

inline String operator+(const char *lhs, const String &rhs) {
  String res(lhs);
  res += rhs.c_str();
  return res;
}

// eink-web: Print::print(String), defined once String is complete.
#include "Print.h"
inline size_t Print::print(const String &s) { return write(s.c_str(), s.length()); }
