#!/usr/bin/env python3
"""Compile the TRMNL firmware (usetrmnl/firmware) to WebAssembly.

Usage: build.py [--model og] [--only SUBSTRING]

Builds the firmware's own sources with bb_epaper in its memory-only mode,
against the shared browser layer (core/), this adapter's shims (shim/) and the
generic Arduino shims from the crosspoint-simulator (build/simsrc).
"""
import argparse
import glob
import hashlib
import os
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "tools"))
import paths  # noqa: E402

os.environ["EM_CONFIG"] = paths.EM_CONFIG
os.environ["EMCC_LOCAL_PORTS"] = "sdl2=" + paths.SDL2

TRMNL = paths.TRMNL_FW
DEPS = paths.DEPS

# Sources that need hardware the browser does not have, or belong to the
# TRMNL X; this adapter's stubs.cpp stands in where something still calls them.
EXCLUDE = {
    "src/qa.cpp",                       # factory test: stubbed as passed
    "src/x_io_expander.cpp",            # TRMNL X only
    "src/touchbar_actions.cpp",         # TRMNL X only
    "src/touchbar_indicator.cpp",       # TRMNL X only
    "src/services/device_setup_x.cpp",  # TRMNL X only
    "src/misc/clock/clock_x.cpp",       # TRMNL X only
    "src/power/tca9535_power.cpp",      # TRMNL X only
    "src/battery/bq27427_battery.cpp",  # TRMNL X only
    "src/misc/sensor/environment_sensor.cpp",  # optional SCD41 sensor
    "src/resumable_wifi_client_secure.cpp",    # TLS socket client; fetch replaces it
}

MODELS = {
    "og": {
        "defines": ["BOARD_TRMNL", "PNG_MAX_BUFFERED_PIXELS=6432", 'DEVICE_MODEL="og"', "CONFIG_IDF_TARGET_ESP32C3=1"],
    },
}

DEFINES = [
    "SIMULATOR", "EINK_WEB", "CORE_DEBUG_LEVEL=0", "ARDUINO=111",
    "ARDUINOJSON_ENABLE_ARDUINO_STRING=1", "memcpy_P=memcpy",
]
COMMON = ["-O2", "-Wno-deprecated-declarations", "-Wno-narrowing", "-Wno-c++11-narrowing",
          "-Wno-unused-command-line-argument", "-Wno-format", "-Wno-writable-strings", "-Wno-register"]


def patched(path):
    """Use build/trmnl/firmware/<rel> when patch_trmnl.py made a fixed copy."""
    rel = os.path.relpath(path, TRMNL)
    alt = os.path.join(paths.BUILD, "trmnl", "firmware", rel)
    return alt if os.path.exists(alt) else path


def sources():
    out = []
    for pattern in ("src/*.cpp", "src/*/*.cpp", "src/*/*/*.cpp", "lib/trmnl/src/**/*.cpp"):
        for f in sorted(glob.glob(os.path.join(TRMNL, pattern), recursive=True)):
            if os.path.relpath(f, TRMNL) not in EXCLUDE and patched(f) not in out:
                out.append(patched(f))
    bbep = os.path.join(paths.BUILD, "trmnl", "bb_epaper")  # patched copy (patch_trmnl.py)
    out += [os.path.join(bbep, "bb_epaper.cpp"), os.path.join(bbep, "Group5.cpp")]
    out += sorted(glob.glob(os.path.join(DEPS, "PNGdec/src/*.c*")))
    out += sorted(glob.glob(os.path.join(DEPS, "JPEGDEC/src/*.c*")))
    out += [os.path.join(DEPS, "Arduino-Log", "ArduinoLog.cpp")]
    out += sorted(glob.glob(os.path.join(HERE, "*.cpp")))
    out += [os.path.join(paths.CORE, "sim_coop.cpp")]
    return out


def includes():
    return [
        os.path.join(HERE, "shim"),
        paths.CORE,
        os.path.join(TRMNL, "include"),
        os.path.join(TRMNL, "src"),
        os.path.join(TRMNL, "lib/trmnl/include"),
        os.path.join(TRMNL, "lib/wificaptive/src"),  # its types; shim/WifiCaptive.h shadows the portal
        TRMNL,
        os.path.join(paths.BUILD, "trmnl", "bb_epaper"),
        os.path.join(DEPS, "PNGdec/src"),
        os.path.join(DEPS, "JPEGDEC/src"),
        os.path.join(DEPS, "Arduino-Log"),
        os.path.join(DEPS, "ArduinoJson/src"),
    ]


def object_path(src, flags):
    key = hashlib.sha1((src + "\0" + " ".join(flags)).encode()).hexdigest()[:16]
    return os.path.join(paths.OBJ, "trmnl", f"{os.path.basename(src)}.{key}.o")


def compile_one(job):
    src, flags = job
    obj = object_path(src, flags)
    if os.path.exists(obj) and os.path.getmtime(obj) >= os.path.getmtime(src):
        return obj, None
    is_c = src.endswith(".c")
    lang = ["-std=gnu11"] if is_c else ["-std=gnu++2a", "-include", os.path.join(paths.CORE, "web_compat.h")]
    cmd = [paths.EMCC if is_c else paths.EMXX] + lang + flags + ["-c", src, "-o", obj]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        return None, f"FAILED {src}\n{r.stderr[-3000:]}"
    return obj, None


LINK = [
    "-O2", "-sASYNCIFY", "-sASYNCIFY_STACK_SIZE=262144", "-sSTACK_SIZE=4MB",
    "-sALLOW_MEMORY_GROWTH", "-sINITIAL_MEMORY=64MB", "-sEXIT_RUNTIME=0", "-sENVIRONMENT=web",
    "-sFORCE_FILESYSTEM", "-lidbfs.js",
    "-sEXPORTED_RUNTIME_METHODS=FS,ENV,IDBFS,UTF8ToString,stringToUTF8,lengthBytesUTF8,HEAPU8,addRunDependency,removeRunDependency",
    "-sEXPORTED_FUNCTIONS=_main,_malloc,_free",
]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--model", default="og", choices=sorted(MODELS))
    ap.add_argument("--only")
    ap.add_argument("--debug", action="store_true", help="symbols and runtime assertions")
    args = ap.parse_args()

    # Log lines print __FILE__: show paths relative to each source tree.
    prefix_maps = [f"-fmacro-prefix-map={d}/=" for d in (os.path.join(paths.BUILD, "trmnl", "firmware"), paths.TRMNL_FW, paths.DEPS)]
    common = prefix_maps + COMMON + (["-g", "-O1"] if args.debug else []) + (os.environ["TRMNL_SYMBOLS"].split() if os.environ.get("TRMNL_SYMBOLS") else [])
    flags = common + [f"-D{d}" for d in DEFINES + MODELS[args.model]["defines"]] + [f"-I{d}" for d in includes()]
    jobs = [(s, flags) for s in sources()]
    if args.only:
        jobs = [j for j in jobs if args.only in j[0]]
    os.makedirs(os.path.join(paths.OBJ, "trmnl"), exist_ok=True)
    objs, errors = [], []
    with ThreadPoolExecutor(max_workers=os.cpu_count()) as pool:
        for obj, err in pool.map(compile_one, jobs):
            (errors.append(err) if err else objs.append(obj))
    print(f"[trmnl-{args.model}] compiled {len(objs)}/{len(jobs)} objects, {len(errors)} failures")
    if errors:
        with open(os.path.join(paths.BUILD, "trmnl-errors.log"), "w") as fh:
            fh.write("\n\n".join(errors))
        sys.exit(1)
    if args.only:
        return
    os.makedirs(paths.DIST, exist_ok=True)
    out_js = os.path.join(paths.DIST, f"trmnl-{args.model}.js")
    r = subprocess.run([paths.EMXX] + LINK + (["-g", "-O1", "-sASSERTIONS=1"] if args.debug else []) + (os.environ["TRMNL_SYMBOLS"].split() if os.environ.get("TRMNL_SYMBOLS") else []) + objs + ["-o", out_js], capture_output=True, text=True)
    if r.returncode != 0:
        err = "\n".join(l for l in r.stderr.splitlines() if "wasm-ld\" failed" not in l and " -o " not in l[:200])
        print(err[:8000])
        sys.exit(1)
    print("linked", out_js)


if __name__ == "__main__":
    main()
