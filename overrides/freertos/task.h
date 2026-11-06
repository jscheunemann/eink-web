#pragma once
#include <cstdint>

#include "FreeRTOS.h"
#include "sim_coop.h"

inline SimTaskHandle *simMainTaskHandle() {
  static SimTaskHandle mainTask{0, "main", nullptr};
  return &mainTask;
}

inline TaskHandle_t xTaskGetCurrentTaskHandle() {
  SimTaskHandle *h = simcoop::currentTask();
  return h ? h : simMainTaskHandle();
}

inline BaseType_t xTaskCreate(void (*fn)(void *), const char *name, uint32_t stackDepth, void *param,
                              BaseType_t /*priority*/, TaskHandle_t *handle) {
  TaskHandle_t h = simcoop::spawn(fn, param, name, stackDepth);
  if (handle) *handle = h;
  return pdPASS;
}

inline BaseType_t xTaskCreatePinnedToCore(void (*fn)(void *), const char *name, uint32_t stackDepth, void *param,
                                          BaseType_t priority, TaskHandle_t *handle, BaseType_t /*coreId*/) {
  return xTaskCreate(fn, name, stackDepth, param, priority, handle);
}

inline uint32_t ulTaskNotifyTake(int /*clearOnExit*/, uint32_t ticksToWait) {
  TaskHandle_t h = xTaskGetCurrentTaskHandle();
  if (!simcoop::waitUntil([h] { return h->notifyCount > 0; }, ticksToWait * portTICK_PERIOD_MS)) return 0;
  h->notifyCount--;
  return 1;
}

inline void xTaskNotify(TaskHandle_t handle, uint32_t /*value*/, int /*action*/) {
  if (handle) handle->notifyCount++;
}

inline const char *pcTaskGetName(TaskHandle_t h) {
  if (!h) h = xTaskGetCurrentTaskHandle();
  return h ? h->name : "main";
}

inline void vTaskDelete(TaskHandle_t h) {
  if (!h || h == simcoop::currentTask()) simcoop::finishCurrent();
}

inline unsigned int uxTaskGetStackHighWaterMark(TaskHandle_t) { return 2048; }
inline void vTaskList(char *) {}

inline void vTaskDelay(uint32_t ticks) {
  if (ticks == 0) {
    simcoop::yieldNow();
    return;
  }
  simcoop::sleepMs(ticks * portTICK_PERIOD_MS);
}
