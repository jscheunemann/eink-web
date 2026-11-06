// Browser entry point. Replaces the simulator's simulator_main.cpp: the
// firmware's setup()/loop() runs inside a fiber, and the browser's frame
// callback drives the cooperative scheduler.
#include <emscripten.h>

#include "Arduino.h"
#include "HalDisplay.h"
#include "HalGPIO.h"
#include "SimulatorLifecycle.h"
#include "sim_coop.h"

extern void setup();
extern void loop();
extern HalDisplay display;

namespace {

char arg0[] = "picco";
char *webArgv[] = {arg0, nullptr};

void firmwareTask(void *) {
  SimulatorLifecycle::initProcessArgs(webArgv);
  setup();
  while (!display.shouldQuit()) {
    // Same frame structure as the desktop simulator's main loop.
    gpio.beginFrame();
    loop();
    display.presentIfNeeded();
    simcoop::sleepMs(1);
  }
  EM_ASM({ if (Module.onQuit) Module.onQuit(); });
  simcoop::finishCurrent();
}

}  // namespace

int main() {
  // The firmware's loop task: deep EPUB layout and FreeType rendering need a
  // larger stack than the device's, because host frames are bigger.
  simcoop::spawn(firmwareTask, nullptr, "loopTask", 2 * 1024 * 1024);
  emscripten_set_main_loop(simcoop::tick, 0, false);
  return 0;
}
