#!/usr/bin/env python3
"""Resolve the source set for the CrossPoint simulator without PlatformIO.

Mimics PlatformIO's library dependency finder in deep+ mode: start from the
project sources, follow #include lines, and pull in each library whose headers
are reached. Writes a JSON build plan: sources, include dirs, per-lib flags.
"""
import fnmatch
import json
import re
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "tools"))
import paths  # noqa: E402

FW = paths.FW
SIM = paths.SIM_PATCHED  # patched copy of simulator src
CORE = paths.CORE
ADAPTER = paths.CROSSPOINT_ADAPTER
DEPS = paths.DEPS

SRC_EXT = (".c", ".cpp", ".cc")
HDR_EXT = (".h", ".hpp", ".hh", ".inc")
SKIP_DIRS = {"examples", "example", "test", "tests", "extras", "docs", "scripts", ".git", "MacOS", "linux"}

# Firmware build_src_filter exclusions from the simulator sample ini.
FW_EXCLUDE = {
    "network/FirmwareFlasher.cpp",
    "network/OtaBootSwitch.cpp",
    "network/OtaUpdater.cpp",
    "platform/skip_efuse_blk_check.c",
}
# The simulator's skip_device_content_protection.py drops this file.
LIB_FILE_EXCLUDE = {os.path.join(FW, "lib/Epub/ContentProtection.cpp")}
# lib_ignore from the simulator sample ini.
LIB_IGNORE = {"hal", "PNGdec", "JPEGDEC", "WebSockets", "SecureNet"}

INCLUDE_RE = re.compile(r'^\s*#\s*include\s*[<"]([^>"]+)[>"]', re.M)


def walk(root, exts, skip=True):
    out = []
    for d, dirs, files in os.walk(root):
        if skip:
            dirs[:] = [x for x in dirs if x not in SKIP_DIRS]
        for f in files:
            if f.endswith(exts):
                out.append(os.path.join(d, f))
    return sorted(out)


class Lib:
    def __init__(self, name, root):
        self.name = name
        self.root = root
        self.flags = []
        cfg = {}
        lj = os.path.join(root, "library.json")
        if os.path.exists(lj):
            cfg = json.load(open(lj)).get("build", {})
        src_dir = cfg.get("srcDir")
        if src_dir is None:
            src_dir = "src" if os.path.isdir(os.path.join(root, "src")) else "."
        self.src_dir = os.path.normpath(os.path.join(root, src_dir))
        inc_dir = cfg.get("includeDir")
        self.inc_dirs = []
        if inc_dir:
            self.inc_dirs.append(os.path.normpath(os.path.join(root, inc_dir)))
        else:
            if os.path.isdir(os.path.join(root, "include")):
                self.inc_dirs.append(os.path.join(root, "include"))
            self.inc_dirs.append(self.src_dir)
        for fl in cfg.get("flags", []):
            if fl.startswith("-I"):
                p = fl[2:].strip()
                self.inc_dirs.append(os.path.normpath(os.path.join(root, p)))
            else:
                self.flags.append(fl)
        self.src_filter = cfg.get("srcFilter")
        self.sources = self._sources()

    def _sources(self):
        files = walk(self.src_dir, SRC_EXT, skip=True)
        if self.src_filter:
            keep = []
            pats = [p[2:-1] for p in self.src_filter if p.startswith("+<")]
            for f in files:
                rel = os.path.relpath(f, self.src_dir)
                if any(fnmatch.fnmatch(rel, p) for p in pats):
                    keep.append(f)
            files = keep
        return [f for f in files if f not in LIB_FILE_EXCLUDE]

    def headers(self):
        out = {}
        for inc in self.inc_dirs:
            if not os.path.isdir(inc):
                continue
            for h in walk(inc, HDR_EXT, skip=False):
                out.setdefault(os.path.relpath(h, inc), h)
        return out


def candidate_libs():
    libs = []
    for name in sorted(os.listdir(os.path.join(FW, "lib"))):
        p = os.path.join(FW, "lib", name)
        if os.path.isdir(p) and name not in LIB_IGNORE:
            libs.append(Lib(name, p))
    for name in ["network/JsonSax", "network/Opds", "network/CatalogList", "ui/FreeInkUI",
                 "assets/Icons", "font/FreeInkFont"]:
        libs.append(Lib(name.split("/")[1], os.path.join(FW, "freeink-sdk/libs", name)))
    # Headers only: the simulator supplies its own ContentProtection and
    # WolfsslCrypto shims, so the SDK's device crypto sources are not built.
    cp = Lib("ContentProtection", os.path.join(FW, "freeink-sdk/libs/book/ContentProtection"))
    cp.sources = []
    libs.append(cp)
    libs.append(Lib("ArduinoJson", os.path.join(DEPS, "ArduinoJson")))
    libs.append(Lib("AnimatedGIF", os.path.join(DEPS, "AnimatedGIF")))
    return libs


def main():
    project_dirs = [CORE, ADAPTER, SIM, os.path.join(FW, "src")]
    project_sources = []
    for f in walk(os.path.join(FW, "src"), SRC_EXT, skip=False):
        if os.path.relpath(f, os.path.join(FW, "src")) not in FW_EXCLUDE:
            project_sources.append(f)
    project_sources += walk(SIM, SRC_EXT, skip=False)
    project_sources += walk(CORE, SRC_EXT, skip=False)
    project_sources += walk(ADAPTER, SRC_EXT, skip=False)

    libs = candidate_libs()
    header_owner = {}
    for lib in libs:
        for rel, path in lib.headers().items():
            header_owner.setdefault(rel, lib)
    # Project-level headers win over library headers.
    project_headers = set()
    for d in project_dirs:
        for h in walk(d, HDR_EXT, skip=False):
            project_headers.add(os.path.relpath(h, d))

    used = []
    seen_files = set()
    queue = list(project_sources)
    for d in project_dirs:
        queue += walk(d, HDR_EXT, skip=False)
    while queue:
        f = queue.pop()
        if f in seen_files:
            continue
        seen_files.add(f)
        try:
            text = open(f, errors="replace").read()
        except OSError:
            continue
        for inc in INCLUDE_RE.findall(text):
            local = os.path.normpath(os.path.join(os.path.dirname(f), inc))
            if os.path.exists(local):
                queue.append(local)
                continue
            if inc in project_headers:
                continue
            lib = header_owner.get(inc)
            if lib and lib not in used:
                used.append(lib)
                queue += lib.sources
                for inc_dir in lib.inc_dirs:
                    if os.path.isdir(inc_dir):
                        queue += walk(inc_dir, HDR_EXT, skip=False)
            if lib:
                queue.append(lib.headers()[inc])

    plan = {
        "project_includes": project_dirs,
        "project_sources": project_sources,
        "libs": [{"name": l.name, "sources": l.sources, "includes": l.inc_dirs, "flags": l.flags} for l in used],
    }
    os.makedirs(paths.BUILD, exist_ok=True)
    json.dump(plan, open(paths.PLAN, "w"), indent=1)
    print("libs:", ", ".join(l.name for l in used))
    print("project sources:", len(project_sources), " lib sources:", sum(len(l.sources) for l in used))


if __name__ == "__main__":
    main()
