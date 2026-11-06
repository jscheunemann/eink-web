#!/usr/bin/env python3
"""Prepare the TRMNL build's patched sources.

Copies bb_epaper's sources and points its I/O include at this adapter's
emulated panel controller (web_io.inl). Every patch must match exactly once;
a miss means upstream changed, and the build stops here.
"""
import os
import shutil
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "tools"))
import paths  # noqa: E402

OUT = os.path.join(paths.BUILD, "trmnl", "bb_epaper")
FW_OUT = os.path.join(paths.BUILD, "trmnl", "firmware")

# Firmware source fixes, applied to copies in build/trmnl/firmware/ that the
# build compiles in place of the originals. Keep each one small and explained.
FIRMWARE_PATCHES = [
    # Passes an Arduino String to a printf-style log call. GCC on the device
    # passes its bytes through (and the first field happens to be the text
    # pointer); Clang compiles it to a trap. Pass the C string instead.
    ("src/api-client/header.cpp",
     'Value - %s\\r\\n", __FILE__, __LINE__, PREFERENCES_FRIENDLY_ID, inputs.friendlyId);',
     'Value - %s\\r\\n", __FILE__, __LINE__, PREFERENCES_FRIENDLY_ID, inputs.friendlyId.c_str());'),
]


def patch(path, old, new):
    text = open(path).read()
    found = text.count(old)
    if found != 1:
        sys.exit(f"patch_trmnl: expected 1 match in {os.path.basename(path)}, found {found}:\n{old}")
    open(path, "w").write(text.replace(old, new))


def main():
    shutil.rmtree(OUT, ignore_errors=True)
    shutil.copytree(os.path.join(paths.DEPS, "bb_epaper", "src"), OUT)
    shutil.copy(os.path.join(HERE, "web_io.inl"), OUT)
    patch(os.path.join(OUT, "bb_epaper.cpp"),
          '#ifdef __MEM_ONLY__\n#include "mem_io.inl"\n#endif // __MEM_ONLY__\n\n'
          '#if defined( __LINUX__ ) && !defined( __MEM_ONLY__ )\n#include "rpi_io.inl"\n#else\n'
          '#ifdef ESPHOME_LOG_LEVEL\n#include "esphome_io.inl" // I/O for the ESPHome / HomeAssistant project\n'
          '#elif defined( ARDUINO )\n#include "arduino_io.inl" // I/O (non-portable) code is in here\n'
          '#elif !defined(__MACH__)\n#include "../esp_idf/esp_generic.inl" // ESP-IDF specific\n'
          '#endif // ARDUINO\n#endif // __LINUX__\n',
          '#include "web_io.inl" // eink-web: emulated panel controller\n')
    shutil.rmtree(FW_OUT, ignore_errors=True)
    for rel, old, new in FIRMWARE_PATCHES:
        dest = os.path.join(FW_OUT, rel)
        os.makedirs(os.path.dirname(dest), exist_ok=True)
        shutil.copy(os.path.join(paths.TRMNL_FW, rel), dest)
        patch(dest, old, new)
    print("patch_trmnl: ok")


if __name__ == "__main__":
    main()
