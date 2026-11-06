#pragma once
// Cooperative task scheduler for the browser build.
//
// The browser has no threads unless the page is cross-origin isolated, so
// every FreeRTOS task (the firmware's main loop and the render task) runs as
// an Emscripten fiber. A task gives up the CPU only when it waits: delay(),
// vTaskDelay(), a semaphore or a task notification. The scheduler runs from
// the browser's frame callback and resumes whichever fibers can make progress.
#include <cstdint>
#include <functional>

struct SimTaskHandle;

namespace simcoop {

constexpr uint32_t kForever = 0xFFFFFFFFu;

// Wait until pred() is true or timeoutMs passes. Returns pred()'s final value
// (true when no predicate is given and the timeout simply elapsed).
bool waitUntil(const std::function<bool()> &pred, uint32_t timeoutMs);
void sleepMs(uint32_t ms);
void yieldNow();

// Start fn(arg) as a new task. stackBytes is a lower bound; the fiber gets at
// least 512KB because the host build's frames are larger than Xtensa's.
SimTaskHandle *spawn(void (*fn)(void *), void *arg, const char *name, uint32_t stackBytes);
SimTaskHandle *currentTask();
void finishCurrent();  // vTaskDelete(nullptr)

// Browser frame callback: run every fiber that can make progress.
void tick();

}  // namespace simcoop

void simcoopSleepMs(unsigned long ms);
void simcoopYield();
