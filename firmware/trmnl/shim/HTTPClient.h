#pragma once
// eink-web: HTTPClient on top of the page's fetch.
//
// Each request is handed to the page (eink_fetch_start), which answers it with
// fetch or, in mock mode, with its built-in TRMNL server. The calling firmware
// task waits cooperatively until the reply arrives, then the body is loaded
// into the NetworkClient passed to begin() so the firmware can stream it.
#include <stdint.h>
#include <stdlib.h>

#include <string>
#include <utility>
#include <vector>

#include "NetworkClient.h"
#include "Stream.h"
#include "WString.h"
#include "eink_web.h"
#include "sim_coop.h"

#define HTTPC_ERROR_CONNECTION_REFUSED (-1)
#define HTTPC_ERROR_SEND_HEADER_FAILED (-2)
#define HTTPC_ERROR_SEND_PAYLOAD_FAILED (-3)
#define HTTPC_ERROR_NOT_CONNECTED (-4)
#define HTTPC_ERROR_CONNECTION_LOST (-5)
#define HTTPC_ERROR_NO_STREAM (-6)
#define HTTPC_ERROR_NO_HTTP_SERVER (-7)
#define HTTPC_ERROR_TOO_LESS_RAM (-8)
#define HTTPC_ERROR_ENCODING (-9)
#define HTTPC_ERROR_STREAM_WRITE (-10)
#define HTTPC_ERROR_READ_TIMEOUT (-11)

typedef enum {
  HTTP_CODE_CONTINUE = 100,
  HTTP_CODE_OK = 200,
  HTTP_CODE_CREATED = 201,
  HTTP_CODE_ACCEPTED = 202,
  HTTP_CODE_NO_CONTENT = 204,
  HTTP_CODE_MOVED_PERMANENTLY = 301,
  HTTP_CODE_FOUND = 302,
  HTTP_CODE_SEE_OTHER = 303,
  HTTP_CODE_NOT_MODIFIED = 304,
  HTTP_CODE_TEMPORARY_REDIRECT = 307,
  HTTP_CODE_PERMANENT_REDIRECT = 308,
  HTTP_CODE_BAD_REQUEST = 400,
  HTTP_CODE_UNAUTHORIZED = 401,
  HTTP_CODE_FORBIDDEN = 403,
  HTTP_CODE_NOT_FOUND = 404,
  HTTP_CODE_TOO_MANY_REQUESTS = 429,
  HTTP_CODE_INTERNAL_SERVER_ERROR = 500,
  HTTP_CODE_SERVICE_UNAVAILABLE = 503,
} t_http_codes;

typedef enum {
  HTTPC_DISABLE_FOLLOW_REDIRECTS,
  HTTPC_STRICT_FOLLOW_REDIRECTS,
  HTTPC_FORCE_FOLLOW_REDIRECTS,
} followRedirects_t;

class HTTPClient {
 public:
  ~HTTPClient() { end(); }

  bool begin(NetworkClient &client, const String &url) {
    client_ = &client;
    url_ = url.c_str();
    return true;
  }
  bool begin(NetworkClient &client, const char *url) { return begin(client, String(url)); }
  bool begin(const String &url) {
    client_ = &own_;
    url_ = url.c_str();
    return true;
  }
  bool begin(const char *url) { return begin(String(url)); }
  void end() {
    headers_.clear();
    body_.clear();
  }

  void addHeader(const String &name, const String &value, bool = false, bool = true) {
    headers_.emplace_back(name.c_str(), value.c_str());
  }
  void collectHeaders(const char *keys[], size_t count) {
    collect_.clear();
    for (size_t i = 0; i < count; i++) collect_.emplace_back(keys[i], "");
  }
  String header(const char *name) {
    for (auto &h : collect_)
      if (strcasecmp(h.first.c_str(), name) == 0) return String(h.second.c_str());
    return String();
  }
  bool hasHeader(const char *name) { return header(name).length() > 0; }
  String getLocation() { return String(location_.c_str()); }

  void setTimeout(uint16_t ms) { timeoutMs_ = ms ? ms : timeoutMs_; }
  void setConnectTimeout(int32_t) {}
  void setReuse(bool) {}
  void setUserAgent(const String &ua) { addHeader("User-Agent", ua); }
  void setFollowRedirects(followRedirects_t) {}
  void useHTTP10(bool = true) {}

  int GET() { return perform("GET", nullptr, 0); }
  int POST(const String &payload) { return perform("POST", payload.c_str(), payload.length()); }
  int POST(const char *payload) { return perform("POST", payload, payload ? strlen(payload) : 0); }
  int POST(uint8_t *payload, size_t size) { return perform("POST", reinterpret_cast<const char *>(payload), size); }
  int PUT(const String &payload) { return perform("PUT", payload.c_str(), payload.length()); }
  int sendRequest(const char *method, const String &payload) { return perform(method, payload.c_str(), payload.length()); }

  int getSize() { return static_cast<int>(body_.size()); }
  String getString() { return String(body_.c_str()); }
  NetworkClient *getStreamPtr() { return client_; }
  NetworkClient &getStream() { return *client_; }
  int writeToStream(Stream *stream) {
    if (!stream) return HTTPC_ERROR_NO_STREAM;
    const size_t n = stream->write(reinterpret_cast<const uint8_t *>(body_.data()), body_.size());
    return n == body_.size() ? static_cast<int>(n) : HTTPC_ERROR_STREAM_WRITE;
  }
  bool connected() { return client_ && client_->connected(); }

  static String errorToString(int error) {
    switch (error) {
      case HTTPC_ERROR_CONNECTION_REFUSED: return "connection refused";
      case HTTPC_ERROR_SEND_HEADER_FAILED: return "send header failed";
      case HTTPC_ERROR_SEND_PAYLOAD_FAILED: return "send payload failed";
      case HTTPC_ERROR_NOT_CONNECTED: return "not connected";
      case HTTPC_ERROR_CONNECTION_LOST: return "connection lost";
      case HTTPC_ERROR_NO_STREAM: return "no stream";
      case HTTPC_ERROR_NO_HTTP_SERVER: return "no HTTP server";
      case HTTPC_ERROR_TOO_LESS_RAM: return "too less ram";
      case HTTPC_ERROR_ENCODING: return "Transfer-Encoding not supported";
      case HTTPC_ERROR_STREAM_WRITE: return "Stream write error";
      case HTTPC_ERROR_READ_TIMEOUT: return "read Timeout";
      default: return String();
    }
  }

 private:
  static void appendJsonString(std::string &out, const std::string &s) {
    out += '"';
    for (char c : s) {
      if (c == '"' || c == '\\') {
        out += '\\';
        out += c;
      } else if (static_cast<unsigned char>(c) < 0x20) {
        char buf[8];
        snprintf(buf, sizeof(buf), "\\u%04x", c);
        out += buf;
      } else {
        out += c;
      }
    }
    out += '"';
  }

  int perform(const char *method, const char *payload, size_t len) {
    std::string json = "[";
    for (size_t i = 0; i < headers_.size(); i++) {
      if (i) json += ',';
      json += '[';
      appendJsonString(json, headers_[i].first);
      json += ',';
      appendJsonString(json, headers_[i].second);
      json += ']';
    }
    json += ']';
    const int id = eink_fetch_start(method, url_.c_str(), json.c_str(), reinterpret_cast<const uint8_t *>(payload),
                                    static_cast<int>(len));
    const bool replied = simcoop::waitUntil([id] { return eink_fetch_state(id) != 0; }, timeoutMs_);
    if (!replied) {
      eink_fetch_free(id);
      return HTTPC_ERROR_READ_TIMEOUT;
    }
    if (eink_fetch_state(id) < 0) {
      eink_fetch_free(id);
      return HTTPC_ERROR_CONNECTION_REFUSED;
    }
    const int status = eink_fetch_status(id);
    body_.assign(static_cast<size_t>(eink_fetch_body_len(id)), '\0');
    if (!body_.empty()) eink_fetch_body_copy(id, reinterpret_cast<uint8_t *>(&body_[0]));
    for (auto &h : collect_) h.second = takeHeader(id, h.first.c_str());
    location_ = takeHeader(id, "Location");
    eink_fetch_free(id);
    if (client_) client_->eink_load(body_);
    return status;
  }

  static std::string takeHeader(int id, const char *name) {
    char *v = eink_fetch_header(id, name);
    std::string out = v ? v : "";
    free(v);
    return out;
  }

  NetworkClient own_;
  NetworkClient *client_ = &own_;
  std::string url_;
  std::string body_;
  std::string location_;
  std::vector<std::pair<std::string, std::string>> headers_;
  std::vector<std::pair<std::string, std::string>> collect_;
  uint32_t timeoutMs_ = 15000;
};
