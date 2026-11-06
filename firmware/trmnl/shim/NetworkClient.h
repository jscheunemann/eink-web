#pragma once
// eink-web: network client as a stream over a response body.
//
// On the device this is a TCP (or TLS) socket. In the browser, HTTPClient
// performs each request with fetch and loads the body into the client passed
// to begin(), which the firmware then reads like a socket: available(),
// read(), readBytes(), connected().
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <string>

#include "IPAddress.h"
#include "Stream.h"

class NetworkClient : public Stream {
 public:
  virtual ~NetworkClient() {}

  // Socket-style API the firmware may call; there is no socket here.
  virtual int connect(IPAddress, uint16_t) { return 1; }
  virtual int connect(const char *, uint16_t) { return 1; }
  virtual int connect(const char *host, uint16_t port, int32_t) { return connect(host, port); }
  virtual void stop() {
    body_.clear();
    pos_ = 0;
    open_ = false;
  }
  virtual uint8_t connected() { return open_ || available() > 0; }
  operator bool() { return connected(); }
  void setTimeout(uint32_t) {}
  void setNoDelay(bool) {}
  virtual void clear() {}

  // Stream: reads come from the loaded response body.
  int available() override { return static_cast<int>(body_.size() - pos_); }
  int read() override { return pos_ < body_.size() ? static_cast<uint8_t>(body_[pos_++]) : -1; }
  int peek() override { return pos_ < body_.size() ? static_cast<uint8_t>(body_[pos_]) : -1; }
  int read(uint8_t *buf, size_t size) {
    const size_t n = size < body_.size() - pos_ ? size : body_.size() - pos_;
    if (n) memcpy(buf, body_.data() + pos_, n);
    pos_ += n;
    return static_cast<int>(n);
  }
  size_t readBytes(uint8_t *buf, size_t size) { return static_cast<size_t>(read(buf, size)); }
  size_t readBytes(char *buf, size_t size) { return readBytes(reinterpret_cast<uint8_t *>(buf), size); }

  using Print::write;
  // Print: request bodies go through HTTPClient, so writes are discarded.
  size_t write(uint8_t) override { return 1; }
  size_t write(const uint8_t *, size_t size) override { return size; }

  // Called by HTTPClient once the fetch completes.
  void eink_load(std::string body) {
    body_ = std::move(body);
    pos_ = 0;
    open_ = false;  // the whole body is buffered; "connected" while unread
  }

 private:
  std::string body_;
  size_t pos_ = 0;
  bool open_ = false;
};

class NetworkClientSecure : public NetworkClient {
 public:
  void setInsecure() {}
  void setCACert(const char *) {}
  void setCACertBundle(const uint8_t *) {}
  void setCertificate(const char *) {}
  void setPrivateKey(const char *) {}
  void setHandshakeTimeout(unsigned long) {}
};
