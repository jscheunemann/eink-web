#pragma once
// eink-web: partition table queries find nothing in the browser.
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
typedef enum { ESP_PARTITION_TYPE_APP = 0, ESP_PARTITION_TYPE_DATA = 1 } esp_partition_type_t;
typedef int esp_partition_subtype_t;
#define ESP_PARTITION_SUBTYPE_ANY 0xff
typedef struct {
  esp_partition_type_t type;
  esp_partition_subtype_t subtype;
  uint32_t address;
  uint32_t size;
  char label[17];
} esp_partition_t;
inline const esp_partition_t *esp_partition_find_first(esp_partition_type_t, esp_partition_subtype_t, const char *) { return nullptr; }
inline esp_err_t esp_partition_read(const esp_partition_t *, size_t, void *, size_t) { return ESP_FAIL; }
