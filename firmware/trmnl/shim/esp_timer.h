#pragma once
// eink-web: esp_timer one-shot timers, run as cooperative tasks.
#include <stdint.h>

#include "esp_err.h"
#include "sim_coop.h"

typedef void (*esp_timer_cb_t)(void *arg);
typedef enum { ESP_TIMER_TASK = 0 } esp_timer_dispatch_t;
typedef struct {
  esp_timer_cb_t callback;
  void *arg;
  esp_timer_dispatch_t dispatch_method;
  const char *name;
  bool skip_unhandled_events;
} esp_timer_create_args_t;

struct eink_web_timer {
  esp_timer_cb_t cb;
  void *arg;
  uint64_t timeoutUs;
  unsigned generation;  // bumped by stop/start so a stale wait does not fire
  bool armed;
};
typedef eink_web_timer *esp_timer_handle_t;

int64_t esp_timer_get_time(void);
inline esp_err_t esp_timer_create(const esp_timer_create_args_t *args, esp_timer_handle_t *out) {
  *out = new eink_web_timer{args->callback, args->arg, 0, 0, false};
  return ESP_OK;
}
esp_err_t esp_timer_start_once(esp_timer_handle_t t, uint64_t timeout_us);
inline esp_err_t esp_timer_stop(esp_timer_handle_t t) {
  if (!t) return ESP_ERR_INVALID_ARG;
  t->armed = false;
  t->generation++;
  return ESP_OK;
}
inline esp_err_t esp_timer_delete(esp_timer_handle_t t) {
  if (t) t->armed = false;  // leaked on purpose: a waiting task may still hold it
  return ESP_OK;
}
