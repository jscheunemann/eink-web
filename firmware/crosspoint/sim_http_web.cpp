// eink-web: CrossPoint's HTTP in the browser.
//
// The desktop simulator sends every firmware HTTP request (plugins, OPDS,
// KOSync, downloads) through sim_http_fetch::fetch(), which runs `curl`. A web
// page can't run programs or open sockets, so in the browser build that last
// step comes here instead and uses the page's fetch(). The request runs in
// JavaScript while the calling firmware task sleeps on the cooperative
// scheduler, so the firmware sees an ordinary blocking call.
//
// What the browser allows is what works: the server must allow cross-origin
// requests (or serve this page itself), an https page can't reach an http
// server, and browsers drop "forbidden" request headers such as Host.
#include <emscripten.h>

#include <cstdlib>
#include <map>
#include <string>
#include <vector>

#include "SimHttpFetch.h"
#include "sim_coop.h"

// clang-format off
EM_JS(int, simhttp_js_start, (const char *method, const char *url, const char *headersJson, const char *body,
                              int hasBody, int follow, int timeoutMs), {
  const S = Module.simHttp || (Module.simHttp = { next: 1, reqs: new Map() });
  const id = S.next++;
  const r = { state: 0, status: 0, body: new Uint8Array(0), headers: new Uint8Array(0) };
  S.reqs.set(id, r);
  const m = UTF8ToString(method), u = UTF8ToString(url);
  const opts = { method: m, headers: JSON.parse(UTF8ToString(headersJson)), credentials: "omit",
                 redirect: follow ? "follow" : "manual" };
  if (hasBody && m !== "GET" && m !== "HEAD") opts.body = UTF8ToString(body);
  // The firmware's connect + read timeouts bound the wait for a response.
  const ctl = new AbortController();
  opts.signal = ctl.signal;
  const timer = setTimeout(() => ctl.abort(), timeoutMs);
  fetch(u, opts).then(async (res) => {
    clearTimeout(timer);
    r.status = res.status;
    const lines = [];
    res.headers.forEach((v, k) => lines.push(k + ": " + v));
    r.headers = new TextEncoder().encode(lines.join("\n"));
    r.body = m === "HEAD" ? new Uint8Array(0) : new Uint8Array(await res.arrayBuffer());
    r.state = 1;
  }).catch((e) => {
    clearTimeout(timer);
    console.warn("[sim-http] " + m + " " + u + " failed: " + e);
    r.state = -1;
  });
  return id;
});
EM_JS(int, simhttp_js_state, (int id), { return Module.simHttp.reqs.get(id).state; });
EM_JS(int, simhttp_js_status, (int id), { return Module.simHttp.reqs.get(id).status; });
EM_JS(int, simhttp_js_body_len, (int id), { return Module.simHttp.reqs.get(id).body.length; });
EM_JS(void, simhttp_js_body_copy, (int id, char *dst), { HEAPU8.set(Module.simHttp.reqs.get(id).body, dst); });
EM_JS(int, simhttp_js_headers_len, (int id), { return Module.simHttp.reqs.get(id).headers.length; });
EM_JS(void, simhttp_js_headers_copy, (int id, char *dst), { HEAPU8.set(Module.simHttp.reqs.get(id).headers, dst); });
EM_JS(void, simhttp_js_free, (int id), { Module.simHttp.reqs.delete(id); });
// clang-format on

namespace {

std::string jsonString(const std::string& s) {
  std::string out = "\"";
  for (unsigned char c : s) {
    if (c == '"' || c == '\\') {
      out += '\\';
      out += static_cast<char>(c);
    } else if (c < 0x20) {
      char buf[8];
      snprintf(buf, sizeof(buf), "\\u%04x", c);
      out += buf;
    } else {
      out += static_cast<char>(c);
    }
  }
  return out + "\"";
}

const char kBase64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string base64(const std::string& in) {
  std::string out;
  int val = 0, bits = -6;
  for (unsigned char c : in) {
    val = (val << 8) + c;
    bits += 8;
    while (bits >= 0) {
      out += kBase64[(val >> bits) & 0x3F];
      bits -= 6;
    }
  }
  if (bits > -6) out += kBase64[((val << 8) >> (bits + 8)) & 0x3F];
  while (out.size() % 4) out += '=';
  return out;
}

}  // namespace

bool sim_http_web_fetch(const std::string& url, const char* method, const std::map<std::string, std::string>& headers,
                        const std::string& basicAuth, const char* body, sim_http_fetch::Response& out,
                        bool followRedirects, const sim_http_fetch::Timeouts& timeouts) {
  std::string json = "{";
  bool first = true;
  for (const auto& h : headers) {
    json += (first ? "" : ",") + jsonString(h.first) + ":" + jsonString(h.second);
    first = false;
  }
  if (!basicAuth.empty()) json += (first ? "" : ",") + jsonString("Authorization") + ":" + jsonString("Basic " + base64(basicAuth));
  json += "}";

  const int id = simhttp_js_start(method ? method : "GET", url.c_str(), json.c_str(), body ? body : "", body ? 1 : 0,
                                  followRedirects ? 1 : 0, static_cast<int>(timeouts.connectMs + timeouts.readMs));
  int state;
  while ((state = simhttp_js_state(id)) == 0) simcoopSleepMs(5);
  if (state < 0) {
    simhttp_js_free(id);
    return false;
  }

  out.statusCode = simhttp_js_status(id);
  const int bodyLen = simhttp_js_body_len(id);
  out.body.assign(static_cast<size_t>(bodyLen), '\0');
  if (bodyLen > 0) simhttp_js_body_copy(id, out.body.data());

  const int headersLen = simhttp_js_headers_len(id);
  std::string text(static_cast<size_t>(headersLen), '\0');
  if (headersLen > 0) simhttp_js_headers_copy(id, text.data());
  simhttp_js_free(id);

  // fetch() already lowercases header names, as the curl path does.
  size_t start = 0;
  while (start < text.size()) {
    const size_t end = text.find('\n', start);
    const std::string line = text.substr(start, end == std::string::npos ? std::string::npos : end - start);
    const size_t colon = line.find(": ");
    if (colon != std::string::npos) out.headers[line.substr(0, colon)] = line.substr(colon + 2);
    if (end == std::string::npos) break;
    start = end + 1;
  }
  // A manual redirect is opaque to a page (status 0): report it as a failure.
  return out.statusCode > 0;
}
