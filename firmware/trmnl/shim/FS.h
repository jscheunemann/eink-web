#pragma once
// eink-web: Arduino fs::FS over the page's persistent storage. The device's
// flash filesystem lives at /persist/flash; paths keep their device form.
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <memory>
#include <string>

#include "Stream.h"
#include "WString.h"

#define FILE_READ "r"
#define FILE_WRITE "w"
#define FILE_APPEND "a"

namespace fs {

enum SeekMode { SeekSet = 0, SeekCur = 1, SeekEnd = 2 };

inline std::string hostPath(const char *path) {
  std::string p = path ? path : "/";
  if (p.empty() || p[0] != '/') p = "/" + p;
  return "/persist/flash" + p;
}

class File : public Stream {
 public:
  File() {}
  File(const char *devicePath, const char *mode) : path_(devicePath ? devicePath : "/") {
    const std::string host = hostPath(devicePath);
    struct stat st;
    if (stat(host.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
      dir_ = opendir(host.c_str());
      return;
    }
    fp_ = fopen(host.c_str(), (mode && mode[0] == 'w') ? "wb" : (mode && mode[0] == 'a') ? "ab" : "rb");
  }
  File(File &&o) noexcept { *this = std::move(o); }
  File &operator=(File &&o) noexcept {
    close();
    fp_ = o.fp_;
    dir_ = o.dir_;
    path_ = std::move(o.path_);
    o.fp_ = nullptr;
    o.dir_ = nullptr;
    return *this;
  }
  File(const File &) = delete;
  File &operator=(const File &) = delete;
  ~File() override { close(); }

  explicit operator bool() const { return fp_ || dir_; }
  void close() {
    if (fp_) fclose(fp_);
    if (dir_) closedir(dir_);
    fp_ = nullptr;
    dir_ = nullptr;
  }
  bool isDirectory() const { return dir_ != nullptr; }
  const char *name() const {
    const size_t slash = path_.rfind('/');
    return slash == std::string::npos ? path_.c_str() : path_.c_str() + slash + 1;
  }
  const char *path() const { return path_.c_str(); }
  size_t size() const {
    if (!fp_) return 0;
    const long cur = ftell(fp_);
    fseek(fp_, 0, SEEK_END);
    const long end = ftell(fp_);
    fseek(fp_, cur, SEEK_SET);
    return end < 0 ? 0 : static_cast<size_t>(end);
  }
  size_t position() const { return fp_ ? static_cast<size_t>(ftell(fp_)) : 0; }
  bool seek(uint32_t pos, SeekMode mode = SeekSet) {
    return fp_ && fseek(fp_, pos, mode == SeekSet ? SEEK_SET : mode == SeekCur ? SEEK_CUR : SEEK_END) == 0;
  }
  File openNextFile(const char *mode = FILE_READ) {
    if (!dir_) return File();
    while (struct dirent *e = readdir(dir_)) {
      if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
      std::string child = path_ == "/" ? "/" + std::string(e->d_name) : path_ + "/" + e->d_name;
      return File(child.c_str(), mode);
    }
    return File();
  }

  int available() override { return fp_ ? static_cast<int>(size() - position()) : 0; }
  int read() override { return fp_ ? fgetc(fp_) : -1; }
  int peek() override {
    if (!fp_) return -1;
    const int c = fgetc(fp_);
    if (c != EOF) ungetc(c, fp_);
    return c;
  }
  size_t read(uint8_t *buf, size_t n) { return fp_ ? fread(buf, 1, n, fp_) : 0; }
  size_t readBytes(char *buf, size_t n) { return read(reinterpret_cast<uint8_t *>(buf), n); }
  size_t write(uint8_t c) override { return fp_ && fputc(c, fp_) != EOF ? 1 : 0; }
  size_t write(const uint8_t *buf, size_t n) override { return fp_ ? fwrite(buf, 1, n, fp_) : 0; }
  void flush() override {
    if (fp_) fflush(fp_);
  }

 private:
  FILE *fp_ = nullptr;
  DIR *dir_ = nullptr;
  std::string path_;
};

class FS {
 public:
  bool begin(bool = false, const char * = "/spiffs", uint8_t = 10, const char * = nullptr) {
    ::mkdir("/persist", 0777);
    ::mkdir("/persist/flash", 0777);
    return true;
  }
  void end() {}
  bool format() {
    removeTree(hostPath("/"));
    return begin();
  }
  File open(const char *path, const char *mode = FILE_READ, bool = false) { return File(path, mode); }
  File open(const String &path, const char *mode = FILE_READ, bool create = false) { return open(path.c_str(), mode, create); }
  bool exists(const char *path) {
    struct stat st;
    return stat(hostPath(path).c_str(), &st) == 0;
  }
  bool exists(const String &path) { return exists(path.c_str()); }
  bool remove(const char *path) { return ::remove(hostPath(path).c_str()) == 0; }
  bool remove(const String &path) { return remove(path.c_str()); }
  bool rename(const char *from, const char *to) { return ::rename(hostPath(from).c_str(), hostPath(to).c_str()) == 0; }
  bool rename(const String &from, const String &to) { return rename(from.c_str(), to.c_str()); }
  bool mkdir(const char *path) { return ::mkdir(hostPath(path).c_str(), 0777) == 0; }
  bool rmdir(const char *path) { return ::rmdir(hostPath(path).c_str()) == 0; }
  size_t totalBytes() { return 1536 * 1024; }  // the device's SPIFFS partition is about 1.5 MB
  size_t usedBytes() { return 0; }

 private:
  static void removeTree(const std::string &dir) {
    if (DIR *d = opendir(dir.c_str())) {
      while (struct dirent *e = readdir(d)) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        const std::string p = dir + "/" + e->d_name;
        struct stat st;
        if (stat(p.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
          removeTree(p);
          ::rmdir(p.c_str());
        } else {
          ::remove(p.c_str());
        }
      }
      closedir(d);
    }
  }
};

}  // namespace fs

using fs::File;
