#pragma once
// eink-web: ESP-IDF GPIO driver. Pin configuration is accepted and ignored;
// levels the firmware reads come from the page (see Arduino.h digitalRead).
#include <stdint.h>

#include "esp_err.h"

using gpio_num_t = int;
constexpr gpio_num_t GPIO_NUM_NC = -1;
constexpr gpio_num_t GPIO_NUM_0 = 0, GPIO_NUM_1 = 1, GPIO_NUM_2 = 2, GPIO_NUM_3 = 3, GPIO_NUM_4 = 4, GPIO_NUM_5 = 5,
                     GPIO_NUM_6 = 6, GPIO_NUM_7 = 7, GPIO_NUM_8 = 8, GPIO_NUM_9 = 9, GPIO_NUM_10 = 10, GPIO_NUM_13 = 13,
                     GPIO_NUM_20 = 20, GPIO_NUM_21 = 21;

enum gpio_mode_t { GPIO_MODE_DISABLE = 0, GPIO_MODE_INPUT = 1, GPIO_MODE_OUTPUT = 2, GPIO_MODE_INPUT_OUTPUT = 3 };
enum gpio_pullup_t { GPIO_PULLUP_DISABLE = 0, GPIO_PULLUP_ENABLE = 1 };
enum gpio_pulldown_t { GPIO_PULLDOWN_DISABLE = 0, GPIO_PULLDOWN_ENABLE = 1 };
enum gpio_int_type_t { GPIO_INTR_DISABLE = 0, GPIO_INTR_POSEDGE, GPIO_INTR_NEGEDGE, GPIO_INTR_ANYEDGE, GPIO_INTR_LOW_LEVEL, GPIO_INTR_HIGH_LEVEL };

struct gpio_config_t {
  uint64_t pin_bit_mask;
  gpio_mode_t mode;
  gpio_pullup_t pull_up_en;
  gpio_pulldown_t pull_down_en;
  gpio_int_type_t intr_type;
};

inline esp_err_t gpio_config(const gpio_config_t *) { return ESP_OK; }
inline esp_err_t gpio_hold_en(gpio_num_t) { return ESP_OK; }
inline esp_err_t gpio_hold_dis(gpio_num_t) { return ESP_OK; }
inline esp_err_t gpio_set_direction(gpio_num_t, gpio_mode_t) { return ESP_OK; }
inline esp_err_t gpio_set_level(gpio_num_t, uint32_t) { return ESP_OK; }
inline esp_err_t gpio_reset_pin(gpio_num_t) { return ESP_OK; }
inline esp_err_t gpio_pullup_en(gpio_num_t) { return ESP_OK; }
inline esp_err_t gpio_pulldown_dis(gpio_num_t) { return ESP_OK; }
