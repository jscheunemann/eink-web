// eink-web: browser entry point for the TRMNL firmware.
//
// Restores RTC memory saved before the last deep sleep, then runs Arduino's
// setup() and loop() in a fiber driven by the browser's frame callback, as
// the CrossPoint adapter does.
#include <emscripten.h>

#include "sim_coop.h"

extern void setup();
extern void loop();
extern "C" void eink_web_restore_rtc(void);

namespace {

void firmwareTask(void *) {
  setup();
  for (;;) {
    loop();
    simcoop::sleepMs(1);
  }
}

}  // namespace

int main() {
  eink_web_restore_rtc();
  simcoop::spawn(firmwareTask, nullptr, "loopTask", 2 * 1024 * 1024);
  emscripten_set_main_loop(simcoop::tick, 0, false);
  return 0;
}
