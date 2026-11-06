// eink-web: the TRMNL firmware's link to the page.
//
// Everything here crosses into JavaScript through Module.eink, which the
// page's boot script (web/trmnl-boot.js) provides: panel frames, HTTP via
// fetch or the mock server, deep sleep and wake, and the battery reading.
#include <emscripten.h>
#include <stdint.h>
#include <string.h>

#include "eink_web.h"
#include "esp_sleep.h"
#include "sim_coop.h"

// clang-format off
EM_JS(void, eink_js_present, (const uint8_t *p0, const uint8_t *p1, int w, int h, int gray4), {
  const n = ((w + 7) >> 3) * h;
  Module.eink.present(HEAPU8.slice(p0, p0 + n), HEAPU8.slice(p1, p1 + n), w, h, !!gray4);
});

EM_JS(int, eink_js_fetch_start, (const char *method, const char *url, const char *headers, const uint8_t *body, int len), {
  return Module.eink.fetchStart(UTF8ToString(method), UTF8ToString(url), JSON.parse(UTF8ToString(headers)),
                                len > 0 ? HEAPU8.slice(body, body + len) : null);
});
EM_JS(int, eink_js_fetch_state, (int id), { return Module.eink.fetchState(id); });
EM_JS(int, eink_js_fetch_status, (int id), { return Module.eink.fetchStatus(id); });
EM_JS(int, eink_js_fetch_body_len, (int id), { return Module.eink.fetchBody(id).length; });
EM_JS(void, eink_js_fetch_body_copy, (int id, uint8_t *dst), { HEAPU8.set(Module.eink.fetchBody(id), dst); });
EM_JS(char *, eink_js_fetch_header, (int id, const char *name), {
  const v = Module.eink.fetchHeader(id, UTF8ToString(name));
  if (v === null || v === undefined) return 0;
  const n = lengthBytesUTF8(v) + 1;
  const p = _malloc(n);
  stringToUTF8(v, p, n);
  return p;
});
EM_JS(void, eink_js_fetch_free, (int id), { Module.eink.fetchFree(id); });

EM_JS(void, eink_js_deep_sleep, (const uint8_t *rtc, int len, double sleepMs), {
  Module.eink.deepSleep(HEAPU8.slice(rtc, rtc + len), sleepMs);
});
EM_JS(int, eink_js_rtc_len, (void), { const r = Module.eink.rtcData(); return r ? r.length : 0; });
EM_JS(void, eink_js_rtc_copy, (uint8_t *dst), { HEAPU8.set(Module.eink.rtcData(), dst); });
EM_JS(int, eink_js_wake_cause, (void), { return Module.eink.wakeCause(); });
EM_JS(int, eink_js_battery_mv, (void), { return Module.eink.batteryMv(); });
EM_JS(int, eink_js_pin_read, (int pin), { return Module.eink.pinRead(pin); });
EM_JS(void, eink_js_restart, (void), { Module.eink.restart(); });
// clang-format on

extern "C" {

void eink_web_present(const uint8_t *plane0, const uint8_t *plane1, int width, int height, int gray4) {
  eink_js_present(plane0, plane1, width, height, gray4);
}

int eink_fetch_start(const char *method, const char *url, const char *headersJson, const uint8_t *body, int bodyLen) {
  return eink_js_fetch_start(method, url, headersJson, body, bodyLen);
}
int eink_fetch_state(int id) { return eink_js_fetch_state(id); }
int eink_fetch_status(int id) { return eink_js_fetch_status(id); }
int eink_fetch_body_len(int id) { return eink_js_fetch_body_len(id); }
void eink_fetch_body_copy(int id, uint8_t *dst) { eink_js_fetch_body_copy(id, dst); }
char *eink_fetch_header(int id, const char *name) { return eink_js_fetch_header(id, name); }
void eink_fetch_free(int id) { eink_js_fetch_free(id); }

int eink_web_wake_cause(void) { return eink_js_wake_cause(); }
int eink_web_battery_mv(void) { return eink_js_battery_mv(); }
int eink_web_pin_read(int pin) { return eink_js_pin_read(pin); }

// ESP.restart(): the page saves storage and reboots the firmware.
void eink_web_restart(void) {
  eink_js_restart();
  simcoop::finishCurrent();
  for (;;) simcoop::sleepMs(60000);
}

// RTC memory: every RTC_DATA_ATTR variable lives in the "rtc_data" section
// (shim/esp_attr.h); the linker brackets it with these symbols.
extern char __start_rtc_data[];
extern char __stop_rtc_data[];

static uint64_t g_timerWakeUs = 0;

void eink_web_restore_rtc(void) {
  const int len = eink_js_rtc_len();
  const int size = static_cast<int>(__stop_rtc_data - __start_rtc_data);
  if (len == size) eink_js_rtc_copy(reinterpret_cast<uint8_t *>(__start_rtc_data));
}

void eink_web_deep_sleep(uint64_t sleepUs) {
  eink_js_deep_sleep(reinterpret_cast<const uint8_t *>(__start_rtc_data),
                     static_cast<int>(__stop_rtc_data - __start_rtc_data), static_cast<double>(sleepUs) / 1000.0);
  // The page saves storage and restarts the firmware; this task never resumes.
  simcoop::finishCurrent();
  for (;;) simcoop::sleepMs(60000);
}

esp_err_t esp_sleep_enable_timer_wakeup(uint64_t time_in_us) {
  g_timerWakeUs = time_in_us;
  return ESP_OK;
}

esp_err_t esp_sleep_disable_wakeup_source(esp_sleep_source_t source) {
  if (source == ESP_SLEEP_WAKEUP_TIMER || source == ESP_SLEEP_WAKEUP_ALL) g_timerWakeUs = 0;
  return ESP_OK;
}

esp_sleep_wakeup_cause_t esp_sleep_get_wakeup_cause(void) {
  return static_cast<esp_sleep_wakeup_cause_t>(eink_web_wake_cause());
}

void esp_deep_sleep_start(void) {
  eink_web_deep_sleep(g_timerWakeUs);
  __builtin_unreachable();
}

esp_err_t esp_light_sleep_start(void) {
  simcoop::sleepMs(static_cast<uint32_t>(g_timerWakeUs / 1000));
  return ESP_OK;
}

}  // extern "C"

// esp_timer: a one-shot timer is a cooperative task that sleeps, then runs
// the callback unless the timer was stopped or restarted in the meantime.
#include <chrono>

#include "esp_flash.h"
#include "esp_timer.h"

esp_flash_t *esp_flash_default_chip = nullptr;

int64_t esp_timer_get_time(void) {
  using namespace std::chrono;
  static const auto start = steady_clock::now();
  return duration_cast<microseconds>(steady_clock::now() - start).count();
}

namespace {
struct TimerRun {
  esp_timer_handle_t timer;
  unsigned generation;
};

void timerTask(void *arg) {
  TimerRun *run = static_cast<TimerRun *>(arg);
  simcoop::sleepMs(static_cast<uint32_t>(run->timer->timeoutUs / 1000));
  if (run->timer->armed && run->timer->generation == run->generation) {
    run->timer->armed = false;
    run->timer->cb(run->timer->arg);
  }
  delete run;
}
}  // namespace

esp_err_t esp_timer_start_once(esp_timer_handle_t t, uint64_t timeout_us) {
  if (!t) return ESP_ERR_INVALID_ARG;
  t->generation++;
  t->armed = true;
  t->timeoutUs = timeout_us;
  simcoop::spawn(timerTask, new TimerRun{t, t->generation}, "esp_timer", 64 * 1024);
  return ESP_OK;
}
