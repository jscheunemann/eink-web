#pragma once
// Browser build: FreeRTOS shim backed by cooperative fibers (sim_coop.h).
// Same API surface as the desktop simulator's thread-based shim.
#include <cstdint>
#include <mutex>

#define pdTRUE 1
#define pdFALSE 0
#define pdPASS 1
#define portMAX_DELAY 0xFFFFFFFF
#define eIncrement 1
#define portTICK_PERIOD_MS 1
#define pdMS_TO_TICKS(xTimeInMs) ((uint32_t)((uint32_t)(xTimeInMs) / (uint32_t)portTICK_PERIOD_MS))

using BaseType_t = int;

// Fibers never preempt each other, so a critical section needs no lock.
struct SimPortMux {};
typedef SimPortMux portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED \
  {}

inline void taskENTER_CRITICAL(portMUX_TYPE *) {}
inline void taskEXIT_CRITICAL(portMUX_TYPE *) {}
#define portENTER_CRITICAL(mux) taskENTER_CRITICAL(mux)
#define portEXIT_CRITICAL(mux) taskEXIT_CRITICAL(mux)

struct SimTaskHandle {
  uint32_t notifyCount = 0;
  const char *name = "sim-task";
  void *coop = nullptr;  // scheduler-owned fiber record
};
typedef SimTaskHandle *TaskHandle_t;
