#pragma once
#include <cstdint>

#include "FreeRTOS.h"
#include "task.h"

// Same tagged-handle design as the desktop shim, with cooperative waits.
enum class SimSemaphoreKind { Mutex, Binary };

struct SimSemaphoreBase {
  const SimSemaphoreKind kind;
  explicit SimSemaphoreBase(SimSemaphoreKind k) : kind(k) {}
};

// Recursive mutex: the holder may take it again.
struct SimMutex : SimSemaphoreBase {
  SimMutex() : SimSemaphoreBase(SimSemaphoreKind::Mutex) {}
  TaskHandle_t holder = nullptr;
  uint16_t holdCount = 0;
};

struct SimBinarySemaphore : SimSemaphoreBase {
  SimBinarySemaphore() : SimSemaphoreBase(SimSemaphoreKind::Binary) {}
  bool available = false;
};

typedef SimSemaphoreBase *SemaphoreHandle_t;
using StaticSemaphore_t = SimMutex;

inline SemaphoreHandle_t xSemaphoreCreateMutex() { return new SimMutex(); }
inline SemaphoreHandle_t xSemaphoreCreateMutexStatic(StaticSemaphore_t *storage) { return storage; }
inline SemaphoreHandle_t xSemaphoreCreateBinary() { return new SimBinarySemaphore(); }

inline bool xSemaphoreTake(SemaphoreHandle_t sem, uint32_t ticksToWait) {
  if (!sem) return true;
  const uint32_t timeout = ticksToWait == portMAX_DELAY ? simcoop::kForever : ticksToWait * portTICK_PERIOD_MS;
  if (sem->kind == SimSemaphoreKind::Binary) {
    auto *bin = static_cast<SimBinarySemaphore *>(sem);
    if (!simcoop::waitUntil([bin] { return bin->available; }, timeout)) return false;
    bin->available = false;
    return true;
  }
  auto *mtx = static_cast<SimMutex *>(sem);
  TaskHandle_t me = xTaskGetCurrentTaskHandle();
  if (!simcoop::waitUntil([mtx, me] { return mtx->holder == nullptr || mtx->holder == me; }, timeout)) return false;
  mtx->holder = me;
  mtx->holdCount++;
  return true;
}

inline bool xSemaphoreGive(SemaphoreHandle_t sem) {
  if (!sem) return true;
  if (sem->kind == SimSemaphoreKind::Binary) {
    auto *bin = static_cast<SimBinarySemaphore *>(sem);
    const bool wasEmpty = !bin->available;
    bin->available = true;
    return wasEmpty;
  }
  auto *mtx = static_cast<SimMutex *>(sem);
  if (mtx->holdCount > 0) mtx->holdCount--;
  if (mtx->holdCount == 0) mtx->holder = nullptr;
  return true;
}

inline TaskHandle_t xSemaphoreGetMutexHolder(SemaphoreHandle_t sem) {
  if (!sem || sem->kind != SimSemaphoreKind::Mutex) return nullptr;
  return static_cast<SimMutex *>(sem)->holder;
}

inline int xQueuePeek(SemaphoreHandle_t sem, void *, uint32_t) {
  if (!sem) return pdTRUE;
  if (sem->kind == SimSemaphoreKind::Binary) return static_cast<SimBinarySemaphore *>(sem)->available ? pdTRUE : pdFALSE;
  // Matches try_lock on the desktop shim's recursive mutex: free, or held by us.
  TaskHandle_t holder = static_cast<SimMutex *>(sem)->holder;
  return holder == nullptr || holder == xTaskGetCurrentTaskHandle() ? pdTRUE : pdFALSE;
}
