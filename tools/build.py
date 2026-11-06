#!/usr/bin/env python3
"""Compile the CrossPoint simulator to WebAssembly with emcc (no PlatformIO).

Usage: build.py [--device picco|trmnl-crosspoint] [--only SUBSTRING]
  --device  board profile to build (default: picco). Output: dist/<device>.js/.wasm
  --only    compile just the sources whose path contains SUBSTRING (no link)
"""
import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import paths  # noqa: E402

FW = paths.FW
EMCC = paths.EMCC
EMXX = paths.EMXX
OBJ = paths.OBJ
OUT = paths.DIST
os.environ["EM_CONFIG"] = paths.EM_CONFIG
os.environ["EMCC_LOCAL_PORTS"] = "sdl2=" + paths.SDL2

DEFINES = [
    "SIMULATOR", "CROSSPOINT_SIMULATOR_PROJECT_WEBSERVER", 'CROSSPOINT_VERSION="dev-web"', "ENABLE_SERIAL_LOG",
    "LOG_LEVEL=2", "EINK_DISPLAY_SINGLE_BUFFER_MODE=1", "MINIZ_NO_ZLIB_COMPATIBLE_NAMES=1", "XML_GE=0",
    "XML_CONTEXT_BYTES=1024", "USE_UTF8_LONG_NAMES=1", "memcpy_P=memcpy", "PNG_MAX_BUFFERED_PIXELS=16416",
    "DISABLE_FS_H_WARNING=1", "DESTRUCTOR_CLOSES_FILE=1",
]

# Board profiles. Each selects one simulator device and its FreeInk flag, as
# the simulator's sample platformio.ini environments do.
DEVICES = {
    # BOOX Picco: 480x800 portrait UI, touch, page buttons, slide switch.
    "picco": ["SIMULATOR_DEVICE_PICCO", "FREEINK_DEVICE_PICCO=1", "BOARD_HAS_PSRAM"],
    # Experiment (kept for the record): CrossPoint on a TRMNL. It has no TRMNL board, so this uses the closest
    # existing one, the Xteink X4 Classic: 800x480 panel, buttons only, no
    # touch, ESP32-C3 class. The page drives it with a one-button scheme.
    "trmnl-crosspoint": ["SIMULATOR_DEVICE_X4_CLASSIC", "FREEINK_DEVICE_X4CLASSIC=1"],
}

COMMON = ["-O2", "-sUSE_SDL=2", "-Wno-deprecated-declarations", "-Wno-narrowing", "-Wno-c++11-narrowing",
          "-Wno-unused-command-line-argument"]

LINK = [
    "-O2", "-sUSE_SDL=2", "-sASYNCIFY", "-sASYNCIFY_STACK_SIZE=262144", "-sSTACK_SIZE=4MB",
    "-sALLOW_MEMORY_GROWTH", "-sINITIAL_MEMORY=128MB", "-sEXIT_RUNTIME=0", "-sENVIRONMENT=web",
    "-sFORCE_FILESYSTEM", "-lidbfs.js",
    "-sEXPORTED_RUNTIME_METHODS=FS,ENV,IDBFS,UTF8ToString,addRunDependency,removeRunDependency",
    "-sEXPORTED_FUNCTIONS=_main",
]

# Per-file extra flags.
FILE_FLAGS = {
    # Parses JSON straight from Arduino Strings (the simulator's WString).
    "src/network/CrossPointWebServer.cpp": ["-DARDUINOJSON_ENABLE_ARDUINO_STRING=1"],
}


def file_flags(src):
    return [f for key, fl in FILE_FLAGS.items() if src.endswith(key) for f in fl]


def object_path(src, flags):
    key = hashlib.sha1((src + "\0" + " ".join(flags)).encode()).hexdigest()[:16]
    return os.path.join(OBJ, f"{os.path.basename(src)}.{key}.o")


def compile_one(job):
    src, flags = job
    obj = object_path(src, flags)
    if os.path.exists(obj) and os.path.getmtime(obj) >= os.path.getmtime(src):
        return obj, None
    is_c = src.endswith(".c")
    lang = ["-std=gnu11"] if is_c else ["-std=gnu++2a", "-include", os.path.join(paths.CORE, "web_compat.h")]
    cmd = [EMCC if is_c else EMXX] + lang + flags + ["-c", src, "-o", obj]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        return None, f"FAILED {src}\n{r.stderr[-4000:]}"
    return obj, None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--device", default="picco", choices=sorted(DEVICES))
    ap.add_argument("--only")
    args = ap.parse_args()

    plan = json.load(open(paths.PLAN))
    includes = list(plan["project_includes"]) + [os.path.join(FW, "freeink-sdk/libs/network/BleKeyboardHost/include")]
    for lib in plan["libs"]:
        includes += [d for d in lib["includes"] if d not in includes and os.path.isdir(d)]
    flags = COMMON + [f"-D{d}" for d in DEFINES + DEVICES[args.device]] + [f"-I{d}" for d in includes]

    jobs = [(s, flags + file_flags(s)) for s in plan["project_sources"]]
    for lib in plan["libs"]:
        jobs += [(s, flags + lib["flags"]) for s in lib["sources"]]
    if args.only:
        jobs = [j for j in jobs if args.only in j[0]]

    os.makedirs(OBJ, exist_ok=True)
    os.makedirs(OUT, exist_ok=True)
    objs, errors = [], []
    with ThreadPoolExecutor(max_workers=os.cpu_count()) as pool:
        for obj, err in pool.map(compile_one, jobs):
            if err:
                errors.append(err)
            else:
                objs.append(obj)
    print(f"[{args.device}] compiled {len(objs)}/{len(jobs)} objects, {len(errors)} failures")
    if errors:
        with open(os.path.join(paths.BUILD, "errors.log"), "w") as fh:
            fh.write("\n\n".join(errors))
        for e in errors[:6]:
            print(e[:1500])
            print("-" * 60)
        sys.exit(1)
    if args.only:
        return

    seed = os.path.join(paths.SEED, args.device)
    out_js = os.path.join(OUT, args.device + ".js")
    cmd = [EMXX] + LINK + ["--embed-file", seed + "@/seed"] + objs + ["-o", out_js]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        print(r.stderr[-6000:])
        sys.exit(1)
    print("linked", out_js)
    stage_pages()


# The Artifact publisher wraps each page in a document skeleton; give the
# local copies the same skeleton so dist/ previews the published pages.
SKELETON_HEAD = (
    '<!doctype html><html><head><meta charset="utf-8">'
    '<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">'
    "<style>:root{color-scheme:light;padding-top:env(safe-area-inset-top);"
    "padding-bottom:env(safe-area-inset-bottom)}body{margin:0;font:14px system-ui}"
    "img{max-width:100%}[hidden]{display:none!important}</style></head><body>\n"
)


def trmnl_version():
    """The TRMNL firmware's version, from its include/config.h."""
    import re
    try:
        text = open(os.path.join(paths.TRMNL_FW, "include", "config.h")).read()
    except OSError:
        return "unknown"
    parts = [re.search(r"#define FW_%s_VERSION (\d+)" % k, text) for k in ("MAJOR", "MINOR", "PATCH")]
    return ".".join(m.group(1) for m in parts) if all(parts) else "unknown"


def stage_pages():
    for name in ("picco-boot.js", "trmnl-boot.js", "test.html"):
        shutil.copy(os.path.join(paths.WEB, name), os.path.join(OUT, name))
    for page in ("picco.html", "trmnl-crosspoint.html", "trmnl-og.html"):
        src = os.path.join(paths.WEB, page)
        if os.path.exists(src):
            text = open(src).read().replace("{{FW_VERSION}}", trmnl_version())
            with open(os.path.join(OUT, page), "w") as fh:
                fh.write(SKELETON_HEAD + text + "\n</body></html>\n")
    print("staged pages in", OUT)


if __name__ == "__main__":
    main()
