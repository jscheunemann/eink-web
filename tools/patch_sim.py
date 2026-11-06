#!/usr/bin/env python3
"""Copy the desktop simulator's sources and adapt them for the browser.

Every patch must match exactly once; a miss means upstream changed and the
build stops here instead of producing a subtly broken binary.
"""
import re
import shutil
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import paths  # noqa: E402

SIM_SRC = paths.SIM_SRC
OUT = paths.SIM_PATCHED


def patch(rel, old, new, count=1):
    path = os.path.join(OUT, rel)
    text = open(path).read()
    found = text.count(old)
    if found != count:
        sys.exit(f"patch_sim: expected {count} match(es) in {rel}, found {found}:\n{old}")
    open(path, "w").write(text.replace(old, new))


def main():
    shutil.rmtree(OUT, ignore_errors=True)
    shutil.copytree(SIM_SRC, OUT)

    # Replaced by overrides/: the fiber scheduler entry point and FreeRTOS.
    os.remove(os.path.join(OUT, "simulator_main.cpp"))
    shutil.rmtree(os.path.join(OUT, "freertos"))
    # Self-contained MD5 instead of OpenSSL/CommonCrypto.
    for f in ("MD5Builder_linux.h", "MD5Builder_mac.h"):
        os.remove(os.path.join(OUT, f))
    open(os.path.join(OUT, "MD5Builder.h"), "w").write('#pragma once\n#include "MD5Builder_web.h"\n')

    # delay()/yield() hand control to the scheduler instead of blocking.
    patch("Arduino.h",
          "inline void delay(unsigned long ms) {\n  std::this_thread::sleep_for(std::chrono::milliseconds(ms));\n}\n"
          "inline void yield() { std::this_thread::yield(); }",
          '#include "sim_coop.h"\n'
          "inline void delay(unsigned long ms) { simcoopSleepMs(ms); }\n"
          "inline void yield() { simcoopYield(); }")

    # SDL_Delay busy-waits under Emscripten without ASYNCIFY sleeps; yield instead.
    for rel in ("HalGPIO.cpp",):
        path = os.path.join(OUT, rel)
        text = open(path).read()
        n = len(re.findall(r"\bSDL_Delay\(", text))
        if n == 0:
            sys.exit(f"patch_sim: no SDL_Delay in {rel}")
        text = re.sub(r"\bSDL_Delay\(", "simcoopSleepMs(", text)
        open(path, "w").write('#include "sim_coop.h"\n' + text)

    # Deep-sleep wake relaunches the process on the desktop. In the browser,
    # hand the wake to the page, which persists the SD card and restarts.
    patch("SimulatorLifecycle.cpp",
          "  execvp(gArgv[0], gArgv);\n",
          "#ifdef __EMSCRIPTEN__\n"
          "  EM_ASM({ Module.picco.reboot(UTF8ToString($0)); },\n"
          "         gpio.hasToggleSwitch() && gpio.isToggleSwitchOn() ? \"on\" : \"off\");\n"
          "  for (;;) simcoopSleepMs(1000);  // the page reloads; never return\n"
          "#endif\n"
          "  execvp(gArgv[0], gArgv);\n")
    patch("SimulatorLifecycle.cpp",
          '#include <unistd.h>\n',
          '#include <unistd.h>\n#ifdef __EMSCRIPTEN__\n#include <emscripten.h>\n#include "sim_coop.h"\n#endif\n')
    # Firmware newer than this simulator revision asks the display for its
    # controller name (About screen). Picco reports SSD1677 in the SDK.
    patch("HalDisplay.h", "class HalDisplay {\n", "class HalDisplay {\n public:\n  const char *controllerName() const { return \"SSD1677\"; }\n\n")
    print("patch_sim: ok")


if __name__ == "__main__":
    main()
