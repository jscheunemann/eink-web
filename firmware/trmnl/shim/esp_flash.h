#pragma once
// eink-web: the device's raw flash, as erased flash: every read returns 0xFF.
// (The firmware checks the top of flash for custom logos; none are present.)
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "esp_err.h"
typedef struct esp_flash_t esp_flash_t;
extern esp_flash_t *esp_flash_default_chip;
inline esp_err_t esp_flash_init(esp_flash_t *) { return ESP_OK; }
inline esp_err_t esp_flash_read(esp_flash_t *, void *buf, uint32_t, uint32_t len) {
  if (buf) memset(buf, 0xff, len);
  return ESP_OK;
}
inline esp_err_t esp_flash_get_size(esp_flash_t *, uint32_t *size) {
  if (size) *size = 4 * 1024 * 1024;
  return ESP_OK;
}
