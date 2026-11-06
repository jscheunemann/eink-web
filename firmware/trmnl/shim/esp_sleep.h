#pragma once
// eink-web: ESP-IDF sleep API. Deep sleep hands over to the page, which saves
// RTC memory and reboots the firmware when the timer expires or a button is
// pressed (see eink_web.h).
#include <stdint.h>

#include "eink_web.h"
#include "esp_err.h"

typedef enum {
  ESP_SLEEP_WAKEUP_UNDEFINED = 0,
  ESP_SLEEP_WAKEUP_ALL,
  ESP_SLEEP_WAKEUP_EXT0,
  ESP_SLEEP_WAKEUP_EXT1,
  ESP_SLEEP_WAKEUP_TIMER,
  ESP_SLEEP_WAKEUP_TOUCHPAD,
  ESP_SLEEP_WAKEUP_ULP,
  ESP_SLEEP_WAKEUP_GPIO,
  ESP_SLEEP_WAKEUP_UART,
  ESP_SLEEP_WAKEUP_WIFI,
  ESP_SLEEP_WAKEUP_COCPU,
  ESP_SLEEP_WAKEUP_COCPU_TRAP_TRIG,
  ESP_SLEEP_WAKEUP_BT,
} esp_sleep_source_t;
typedef esp_sleep_source_t esp_sleep_wakeup_cause_t;
#define ESP_SLEEP_WAKEUP_EXT ESP_SLEEP_WAKEUP_EXT1

typedef enum { ESP_GPIO_WAKEUP_GPIO_LOW = 0, ESP_GPIO_WAKEUP_GPIO_HIGH = 1 } esp_deepsleep_gpio_wake_up_mode_t;
typedef enum { ESP_EXT1_WAKEUP_ANY_LOW = 0, ESP_EXT1_WAKEUP_ANY_HIGH = 1, ESP_EXT1_WAKEUP_ALL_LOW = 2 } esp_sleep_ext1_wakeup_mode_t;

#ifdef __cplusplus
extern "C" {
#endif
esp_err_t esp_sleep_enable_timer_wakeup(uint64_t time_in_us);
esp_err_t esp_sleep_disable_wakeup_source(esp_sleep_source_t source);
esp_sleep_wakeup_cause_t esp_sleep_get_wakeup_cause(void);
__attribute__((noreturn)) void esp_deep_sleep_start(void);
esp_err_t esp_light_sleep_start(void);
#ifdef __cplusplus
}
#endif

inline esp_err_t esp_deep_sleep_enable_gpio_wakeup(uint64_t, esp_deepsleep_gpio_wake_up_mode_t) { return ESP_OK; }
inline esp_err_t esp_sleep_enable_gpio_wakeup(void) { return ESP_OK; }
inline esp_err_t esp_sleep_enable_ext0_wakeup(int, int) { return ESP_OK; }
inline esp_err_t esp_sleep_enable_ext1_wakeup(uint64_t, esp_sleep_ext1_wakeup_mode_t) { return ESP_OK; }
inline esp_err_t esp_sleep_enable_ext1_wakeup_io(uint64_t, esp_sleep_ext1_wakeup_mode_t) { return ESP_OK; }
inline esp_err_t gpio_deep_sleep_hold_en(void) { return ESP_OK; }
inline esp_err_t gpio_deep_sleep_hold_dis(void) { return ESP_OK; }
inline uint64_t esp_sleep_get_ext1_wakeup_status(void) { return 0; }
inline uint64_t esp_sleep_get_gpio_wakeup_status(void) { return 0; }
