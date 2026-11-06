#pragma once
// eink-web: ESP32 Preferences (NVS) kept in files on the page's persistent
// storage, one file per namespace under /persist/nvs/. Every write saves the
// namespace, so values survive deep sleep (a page reload) like real NVS.
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include <map>
#include <string>

#include "WString.h"

namespace eink_web_nvs {

using Namespace = std::map<std::string, std::string>;  // key -> raw bytes

inline std::string path(const std::string &ns) { return "/persist/nvs/" + ns + ".kv"; }

inline std::string hex(const std::string &s) {
  static const char *d = "0123456789abcdef";
  std::string out;
  for (unsigned char c : s) {
    out += d[c >> 4];
    out += d[c & 15];
  }
  return out;
}

inline std::string unhex(const std::string &s) {
  auto v = [](char c) { return c <= '9' ? c - '0' : c - 'a' + 10; };
  std::string out;
  for (size_t i = 0; i + 1 < s.size(); i += 2) out += static_cast<char>((v(s[i]) << 4) | v(s[i + 1]));
  return out;
}

inline Namespace &load(const std::string &ns) {
  static std::map<std::string, Namespace> all;
  auto it = all.find(ns);
  if (it != all.end()) return it->second;
  Namespace &n = all[ns];
  if (FILE *f = fopen(path(ns).c_str(), "r")) {
    char line[8192];
    while (fgets(line, sizeof(line), f)) {
      std::string l(line);
      while (!l.empty() && (l.back() == '\n' || l.back() == '\r')) l.pop_back();
      const size_t tab = l.find('\t');
      if (tab != std::string::npos) n[unhex(l.substr(0, tab))] = unhex(l.substr(tab + 1));
    }
    fclose(f);
  }
  return n;
}

inline void save(const std::string &ns) {
  mkdir("/persist", 0777);
  mkdir("/persist/nvs", 0777);
  if (FILE *f = fopen(path(ns).c_str(), "w")) {
    for (auto &kv : load(ns)) fprintf(f, "%s\t%s\n", hex(kv.first).c_str(), hex(kv.second).c_str());
    fclose(f);
  }
}

}  // namespace eink_web_nvs

class Preferences {
 public:
  bool begin(const char *name, bool readOnly = false, const char * = nullptr) {
    ns_ = name ? name : "";
    readOnly_ = readOnly;
    open_ = true;
    eink_web_nvs::load(ns_);
    return true;
  }
  void end() { open_ = false; }
  bool clear() {
    if (!writable()) return false;
    data().clear();
    eink_web_nvs::save(ns_);
    return true;
  }
  bool remove(const char *key) {
    if (!writable()) return false;
    data().erase(key);
    eink_web_nvs::save(ns_);
    return true;
  }
  bool isKey(const char *key) { return open_ && data().count(key) > 0; }
  size_t freeEntries() { return 500 - data().size(); }

  size_t putString(const char *key, const char *value) { return put(key, std::string(value ? value : "")); }
  size_t putString(const char *key, const String &value) { return put(key, std::string(value.c_str())); }
  String getString(const char *key, const String &def = String()) {
    const std::string *v = get(key);
    return v ? String(v->c_str()) : def;
  }
  size_t getString(const char *key, char *out, size_t max) {
    const std::string *v = get(key);
    if (!v || !out || max == 0) return 0;
    const size_t n = v->size() < max - 1 ? v->size() : max - 1;
    memcpy(out, v->data(), n);
    out[n] = 0;
    return n;
  }

  size_t putBytes(const char *key, const void *value, size_t len) {
    return put(key, std::string(static_cast<const char *>(value), len));
  }
  size_t getBytesLength(const char *key) {
    const std::string *v = get(key);
    return v ? v->size() : 0;
  }
  size_t getBytes(const char *key, void *buf, size_t max) {
    const std::string *v = get(key);
    if (!v) return 0;
    const size_t n = v->size() < max ? v->size() : max;
    memcpy(buf, v->data(), n);
    return n;
  }

  size_t putBool(const char *key, bool v) { return putNum(key, v ? 1 : 0); }
  bool getBool(const char *key, bool def = false) { return getNum(key, def ? 1 : 0) != 0; }
  size_t putChar(const char *key, int8_t v) { return putNum(key, v); }
  int8_t getChar(const char *key, int8_t def = 0) { return static_cast<int8_t>(getNum(key, def)); }
  size_t putUChar(const char *key, uint8_t v) { return putNum(key, v); }
  uint8_t getUChar(const char *key, uint8_t def = 0) { return static_cast<uint8_t>(getNum(key, def)); }
  size_t putShort(const char *key, int16_t v) { return putNum(key, v); }
  int16_t getShort(const char *key, int16_t def = 0) { return static_cast<int16_t>(getNum(key, def)); }
  size_t putUShort(const char *key, uint16_t v) { return putNum(key, v); }
  uint16_t getUShort(const char *key, uint16_t def = 0) { return static_cast<uint16_t>(getNum(key, def)); }
  size_t putInt(const char *key, int32_t v) { return putNum(key, v); }
  int32_t getInt(const char *key, int32_t def = 0) { return static_cast<int32_t>(getNum(key, def)); }
  size_t putUInt(const char *key, uint32_t v) { return putNum(key, v); }
  uint32_t getUInt(const char *key, uint32_t def = 0) { return static_cast<uint32_t>(getNum(key, def)); }
  size_t putLong(const char *key, int32_t v) { return putNum(key, v); }
  int32_t getLong(const char *key, int32_t def = 0) { return static_cast<int32_t>(getNum(key, def)); }
  size_t putULong(const char *key, uint32_t v) { return putNum(key, v); }
  uint32_t getULong(const char *key, uint32_t def = 0) { return static_cast<uint32_t>(getNum(key, def)); }
  size_t putLong64(const char *key, int64_t v) { return putNum(key, v); }
  int64_t getLong64(const char *key, int64_t def = 0) { return getNum(key, def); }
  size_t putULong64(const char *key, uint64_t v) { return putNum(key, static_cast<int64_t>(v)); }
  uint64_t getULong64(const char *key, uint64_t def = 0) { return static_cast<uint64_t>(getNum(key, static_cast<int64_t>(def))); }
  size_t putFloat(const char *key, float v) { return putBytes(key, &v, sizeof(v)); }
  float getFloat(const char *key, float def = 0) {
    float v = def;
    return getBytes(key, &v, sizeof(v)) == sizeof(v) ? v : def;
  }

 private:
  eink_web_nvs::Namespace &data() { return eink_web_nvs::load(ns_); }
  bool writable() const { return open_ && !readOnly_; }
  const std::string *get(const char *key) {
    if (!open_ || !key) return nullptr;
    auto it = data().find(key);
    return it == data().end() ? nullptr : &it->second;
  }
  size_t put(const char *key, std::string value) {
    if (!writable() || !key) return 0;
    const size_t n = value.size() ? value.size() : 1;
    data()[key] = std::move(value);
    eink_web_nvs::save(ns_);
    return n;
  }
  size_t putNum(const char *key, int64_t v) { return put(key, std::to_string(v)) ? sizeof(v) : 0; }
  int64_t getNum(const char *key, int64_t def) {
    const std::string *v = get(key);
    return v ? strtoll(v->c_str(), nullptr, 10) : def;
  }

  std::string ns_;
  bool readOnly_ = false;
  bool open_ = false;
};
