#!/usr/bin/env python3
"""Compile the Picco simulator to WebAssembly with emcc (no PlatformIO)."""
import hashlib
import json
import shutil
import subprocess
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import paths  # noqa: E402
from concurrent.futures import ThreadPoolExecutor

ROOT = paths.REPO
FW = paths.FW
EMCC = paths.EMCC
EMXX = paths.EMXX
OBJ = paths.OBJ
OUT = paths.DIST
os.environ["EM_CONFIG"] = paths.EM_CONFIG
os.environ["EMCC_LOCAL_PORTS"] = "sdl2=" + paths.SDL2

DEFINES = [
    "SIMULATOR", "SIMULATOR_DEVICE_PICCO", "FREEINK_DEVICE_PICCO=1", "BOARD_HAS_PSRAM",
    "CROSSPOINT_SIMULATOR_PROJECT_WEBSERVER", 'CROSSPOINT_VERSION="dev-web"', "ENABLE_SERIAL_LOG", "LOG_LEVEL=2",
    "EINK_DISPLAY_SINGLE_BUFFER_MODE=1", "MINIZ_NO_ZLIB_COMPATIBLE_NAMES=1", "XML_GE=0", "XML_CONTEXT_BYTES=1024",
    "USE_UTF8_LONG_NAMES=1", "memcpy_P=memcpy", "PNG_MAX_BUFFERED_PIXELS=16416", "DISABLE_FS_H_WARNING=1", "DESTRUCTOR_CLOSES_FILE=1",
]
COMMON = ["-O2", "-sUSE_SDL=2", "-Wno-deprecated-declarations", "-Wno-narrowing", "-Wno-c++11-narrowing",
          "-Wno-unused-command-line-argument"] + [f"-D{d}" for d in DEFINES]

LINK = [
    "-O2", "-sUSE_SDL=2", "-sASYNCIFY", "-sASYNCIFY_STACK_SIZE=262144", "-sSTACK_SIZE=4MB",
    "-sALLOW_MEMORY_GROWTH", "-sINITIAL_MEMORY=128MB", "-sEXIT_RUNTIME=0", "-sENVIRONMENT=web",
    "-sFORCE_FILESYSTEM", "-lidbfs.js", "-sEXPORTED_RUNTIME_METHODS=FS,ENV,IDBFS,UTF8ToString,addRunDependency,removeRunDependency",
    "-sEXPORTED_FUNCTIONS=_main",
    "--embed-file", paths.SEED + "@/seed",
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
    cmd = [EMCC if is_c else EMXX] + (["-std=gnu11"] if is_c else ["-std=gnu++2a", "-include", os.path.join(ROOT, "overrides/web_compat.h")]) + flags + ["-c", src, "-o", obj]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        return None, f"FAILED {src}\n{r.stderr[-4000:]}"
    return obj, None


def main():
    plan = json.load(open(paths.PLAN))
    includes = list(plan["project_includes"]) + [os.path.join(FW, "freeink-sdk/libs/network/BleKeyboardHost/include")]
    for lib in plan["libs"]:
        includes += [d for d in lib["includes"] if d not in includes and os.path.isdir(d)]
    inc_flags = [f"-I{d}" for d in includes]

    jobs = [(s, COMMON + inc_flags + file_flags(s)) for s in plan["project_sources"]]
    for lib in plan["libs"]:
        jobs += [(s, COMMON + inc_flags + lib["flags"]) for s in lib["sources"]]

    os.makedirs(OBJ, exist_ok=True)
    os.makedirs(OUT, exist_ok=True)
    only = sys.argv[1] if len(sys.argv) > 1 and sys.argv[1] != "--link" else None
    if only:
        jobs = [j for j in jobs if only in j[0]]

    objs, errors = [], []
    with ThreadPoolExecutor(max_workers=os.cpu_count()) as pool:
        for obj, err in pool.map(compile_one, jobs):
            if err:
                errors.append(err)
            else:
                objs.append(obj)
    print(f"compiled {len(objs)}/{len(jobs)} objects, {len(errors)} failures")
    if errors:
        with open(os.path.join(paths.BUILD, "errors.log"), "w") as fh:
            fh.write("\n\n".join(errors))
        for e in errors[:6]:
            print(e[:1500])
            print("-" * 60)
        sys.exit(1)
    if only:
        return

    cmd = [EMXX] + LINK + objs + ["-o", os.path.join(OUT, "picco.js")]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        print(r.stderr[-6000:])
        sys.exit(1)
    print("linked", os.path.join(OUT, "picco.js"))
    stage_page()


# The Artifact publisher wraps web/index.html in a document skeleton; give the
# local copy the same skeleton so dist/ previews the published page.
SKELETON_HEAD = (
    '<!doctype html><html><head><meta charset="utf-8">'
    '<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">'
    "<style>:root{color-scheme:light;padding-top:env(safe-area-inset-top);"
    "padding-bottom:env(safe-area-inset-bottom)}body{margin:0;font:14px system-ui}"
    "img{max-width:100%}[hidden]{display:none!important}</style></head><body>\n"
)


def stage_page():
    for name in ("picco-boot.js", "test.html"):
        shutil.copy(os.path.join(paths.WEB, name), os.path.join(OUT, name))
    page = open(os.path.join(paths.WEB, "index.html")).read()
    with open(os.path.join(OUT, "index.html"), "w") as fh:
        fh.write(SKELETON_HEAD + page + "\n</body></html>\n")
    print("staged page in", OUT)


if __name__ == "__main__":
    main()
