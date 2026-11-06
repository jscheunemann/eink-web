#pragma once
#include <cstdint>
#include <cstdio>
#include <cstdlib>
inline void esp_restart() {}
inline uint32_t esp_get_free_heap_size() { return 1000000; }
// eink-web: reset reasons. A page load is a power-on; a wake from the page's
// deep sleep is reported by the sleep API, as on the device.
typedef enum {
  ESP_RST_UNKNOWN,
  ESP_RST_POWERON,
  ESP_RST_EXT,
  ESP_RST_SW,
  ESP_RST_PANIC,
  ESP_RST_INT_WDT,
  ESP_RST_TASK_WDT,
  ESP_RST_WDT,
  ESP_RST_DEEPSLEEP,
  ESP_RST_BROWNOUT,
  ESP_RST_SDIO,
} esp_reset_reason_t;
extern "C" int eink_web_wake_cause(void);
inline esp_reset_reason_t esp_reset_reason(void) { return eink_web_wake_cause() ? ESP_RST_DEEPSLEEP : ESP_RST_POWERON; }
[[noreturn]] inline void esp_system_abort(const char *details) {
  fprintf(stderr, "esp_system_abort: %s\n", details ? details : "");
  abort();
}
