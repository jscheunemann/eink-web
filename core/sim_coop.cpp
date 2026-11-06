#include "sim_coop.h"

#include <emscripten.h>
#include <emscripten/fiber.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "freertos/FreeRTOS.h"

namespace {

struct CoopFiber {
  emscripten_fiber_t ctx;
  SimTaskHandle *handle = nullptr;
  void (*fn)(void *) = nullptr;
  void *arg = nullptr;
  std::function<bool()> pred;
  double deadline = 0;  // 0 = runnable now
  bool waiting = false;
  bool finished = false;
};

emscripten_fiber_t schedCtx;
alignas(16) char schedAsyncStack[256 * 1024];
std::vector<CoopFiber *> fibers;
CoopFiber *cur = nullptr;

double nowMs() { return emscripten_get_now(); }

void fiberEntry(void *p) {
  auto *f = static_cast<CoopFiber *>(p);
  f->fn(f->arg);
  f->finished = true;
  for (;;) emscripten_fiber_swap(&f->ctx, &schedCtx);
}

bool runnable(CoopFiber *f) {
  if (f->finished) return false;
  if (!f->waiting) return true;
  if (f->pred && f->pred()) return true;
  return nowMs() >= f->deadline;
}

void *alignedAlloc(size_t bytes) {
  void *p = nullptr;
  if (posix_memalign(&p, 16, bytes) != 0) {
    std::fprintf(stderr, "simcoop: out of memory for a %zu-byte fiber stack\n", bytes);
    std::abort();
  }
  return p;
}

}  // namespace

namespace simcoop {

SimTaskHandle *spawn(void (*fn)(void *), void *arg, const char *name, uint32_t stackBytes) {
  auto *f = new CoopFiber();
  f->handle = new SimTaskHandle();
  f->handle->name = name ? name : "sim-task";
  f->handle->coop = f;
  f->fn = fn;
  f->arg = arg;
  const size_t cstack = stackBytes < 512 * 1024 ? 512 * 1024 : stackBytes * 4;
  const size_t astack = 1024 * 1024;
  emscripten_fiber_init(&f->ctx, fiberEntry, f, alignedAlloc(cstack), cstack, alignedAlloc(astack), astack);
  fibers.push_back(f);
  return f->handle;
}

SimTaskHandle *currentTask() { return cur ? cur->handle : nullptr; }

bool waitUntil(const std::function<bool()> &pred, uint32_t timeoutMs) {
  if (pred && pred()) return true;
  if (!cur) {
    // Not inside a task (static initialisation, or a JS callback): nothing
    // can make progress while we wait, so report the current state.
    return pred ? pred() : true;
  }
  CoopFiber *self = cur;
  self->pred = pred;
  self->deadline = timeoutMs == kForever ? INFINITY : nowMs() + timeoutMs;
  self->waiting = true;
  emscripten_fiber_swap(&self->ctx, &schedCtx);
  self->waiting = false;
  const bool ok = pred ? pred() : true;
  self->pred = nullptr;
  return ok;
}

void sleepMs(uint32_t ms) { waitUntil(nullptr, ms); }

void yieldNow() {
  if (!cur) return;
  CoopFiber *self = cur;
  self->pred = nullptr;
  self->deadline = 0;
  self->waiting = true;
  emscripten_fiber_swap(&self->ctx, &schedCtx);
  self->waiting = false;
}

void finishCurrent() {
  if (!cur) return;
  cur->finished = true;
  for (;;) emscripten_fiber_swap(&cur->ctx, &schedCtx);
}

void tick() {
  emscripten_fiber_init_from_current_context(&schedCtx, schedAsyncStack, sizeof(schedAsyncStack));
  const double start = nowMs();
  // A few passes per frame let a notify/wait hand-off between the main loop
  // and the render task finish within one frame, while the frame budget keeps
  // the page responsive.
  for (int pass = 0; pass < 8; pass++) {
    bool ran = false;
    for (size_t i = 0; i < fibers.size(); i++) {
      CoopFiber *f = fibers[i];
      if (!runnable(f)) continue;
      cur = f;
      emscripten_fiber_swap(&schedCtx, &f->ctx);
      cur = nullptr;
      ran = true;
    }
    if (!ran || nowMs() - start > 12) break;
  }
}

}  // namespace simcoop

void simcoopSleepMs(unsigned long ms) { simcoop::sleepMs(static_cast<uint32_t>(ms)); }
void simcoopYield() { simcoop::yieldNow(); }
