"""Every location the build uses, in one place.

Upstream sources and the toolchain live under PICCO_WORK (default: .work/ in
the repo), filled by tools/bootstrap.sh. Build outputs go to build/ and dist/.
"""
import os

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
WORK = os.path.abspath(os.environ.get("PICCO_WORK", os.path.join(REPO, ".work")))

UPSTREAM = os.path.join(WORK, "upstream")
FW = os.path.join(UPSTREAM, "crosspoint-reader")
SIM_REPO = os.path.join(UPSTREAM, "crosspoint-simulator")
SIM_SRC = os.path.join(SIM_REPO, "src")
DEPS = os.path.join(UPSTREAM, "deps")

TOOLCHAIN = os.path.join(WORK, "toolchain")
EMSCRIPTEN = os.path.join(TOOLCHAIN, "emscripten")
EMCC = os.path.join(EMSCRIPTEN, "emcc")
EMXX = os.path.join(EMSCRIPTEN, "em++")
EM_CONFIG = os.path.join(EMSCRIPTEN, ".emscripten")
SDL2 = os.path.join(TOOLCHAIN, "SDL2")
PYSHIM = os.path.join(REPO, "tools", "pyshim")

OVERRIDES = os.path.join(REPO, "overrides")
WEB = os.path.join(REPO, "web")
BUILD = os.path.join(REPO, "build")
SIM_PATCHED = os.path.join(BUILD, "simsrc")
SEED = os.path.join(BUILD, "seed")
PLAN = os.path.join(BUILD, "plan.json")
OBJ = os.path.join(BUILD, "obj")
SHOTS = os.path.join(BUILD, "shots")
DIST = os.path.join(REPO, "dist")
