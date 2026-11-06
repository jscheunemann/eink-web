#!/usr/bin/env bash
# Fetch the pinned upstream sources and build the toolchain into .work/
# (or $PICCO_WORK). Safe to re-run: finished steps are skipped.
#
# Needs: git, cmake, a C++ compiler, python3 (with PyYAML), node,
# and LLVM 18 (clang + wasm-ld). Set LLVM_ROOT to its bin directory if
# clang-18 is not on PATH. See Containerfile for a known-good environment.
set -euo pipefail

REPO=$(cd "$(dirname "$0")/.." && pwd)
WORK=${PICCO_WORK:-$REPO/.work}
UP=$WORK/upstream
TC=$WORK/toolchain
# shellcheck source=../upstream.lock
source "$REPO/upstream.lock"

say() { printf '\n== %s\n' "$*"; }
need() { command -v "$1" >/dev/null || { echo "bootstrap: '$1' is required but not installed" >&2; exit 1; }; }
for tool in git cmake python3 node; do need "$tool"; done
python3 -c 'import yaml' 2>/dev/null || { echo "bootstrap: python3 needs PyYAML (pip install pyyaml, or the python3-yaml package)" >&2; exit 1; }

if [ -z "${LLVM_ROOT:-}" ]; then
  if command -v clang-18 >/dev/null; then LLVM_ROOT=$(dirname "$(readlink -f "$(command -v clang-18)")");
  else LLVM_ROOT=$(dirname "$(command -v clang)"); fi
fi
CLANG_MAJOR=$("$LLVM_ROOT/clang" --version | sed -n 's/.*clang version \([0-9]*\).*/\1/p' | head -1)
if [ "$CLANG_MAJOR" != "18" ]; then
  echo "bootstrap: Emscripten 3.1.51 needs LLVM 18; $LLVM_ROOT/clang is LLVM $CLANG_MAJOR." >&2
  echo "Install clang-18 and lld-18, set LLVM_ROOT, or build inside the Containerfile image." >&2
  exit 1
fi
[ -x "$LLVM_ROOT/wasm-ld" ] || { echo "bootstrap: $LLVM_ROOT/wasm-ld not found (install lld-18)" >&2; exit 1; }

# fetch URL REF DIR: shallow checkout of one exact commit.
fetch() {
  local url=$1 ref=$2 dir=$3
  if [ -d "$dir/.git" ] && [ "$(git -C "$dir" rev-parse HEAD 2>/dev/null)" = "$ref" ]; then
    echo "  $(basename "$dir") @ ${ref:0:7} (already present)"
    return
  fi
  echo "  $(basename "$dir") @ ${ref:0:7}"
  rm -rf "$dir"
  mkdir -p "$dir"
  git -C "$dir" init -q
  git -C "$dir" remote add origin "$url"
  git -C "$dir" fetch -q --depth 1 origin "$ref"
  git -C "$dir" -c advice.detachedHead=false checkout -q FETCH_HEAD
}

say "Upstream sources"
fetch "$CROSSPOINT_URL" "$CROSSPOINT_REF" "$UP/crosspoint-reader"
FREEINK_URL=$(git -C "$UP/crosspoint-reader" config -f .gitmodules submodule.freeink-sdk.url)
fetch "$FREEINK_URL" "$FREEINK_SDK_REF" "$UP/crosspoint-reader/freeink-sdk"
fetch "$SIMULATOR_URL" "$SIMULATOR_REF" "$UP/crosspoint-simulator"
fetch "$ARDUINOJSON_URL" "$ARDUINOJSON_REF" "$UP/deps/ArduinoJson"
fetch "$ANIMATEDGIF_URL" "$ANIMATEDGIF_REF" "$UP/deps/AnimatedGIF"
fetch "$TRMNL_FW_URL" "$TRMNL_FW_REF" "$UP/trmnl-firmware"
fetch "$BB_EPAPER_URL" "$BB_EPAPER_REF" "$UP/deps/bb_epaper"
fetch "$ARDUINO_LOG_URL" "$ARDUINO_LOG_REF" "$UP/deps/Arduino-Log"
fetch "$PNGDEC_URL" "$PNGDEC_REF" "$UP/deps/PNGdec"
fetch "$JPEGDEC_URL" "$JPEGDEC_REF" "$UP/deps/JPEGDEC"

say "Toolchain sources"
fetch "$EMSCRIPTEN_URL" "$EMSCRIPTEN_REF" "$TC/emscripten"
fetch "$BINARYEN_URL" "$BINARYEN_REF" "$TC/binaryen"
fetch "$SDL2_URL" "$SDL2_REF" "$TC/SDL2"
fetch "$ACORN_URL" "$ACORN_REF" "$TC/acorn"

say "Binaryen (first run takes 10-20 minutes)"
if [ -x "$TC/binaryen-install/bin/wasm-opt" ]; then
  echo "  already built"
else
  git -C "$TC/binaryen" submodule update --init --depth 1 -q
  cmake -S "$TC/binaryen" -B "$TC/binaryen/build" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF \
    -DBUILD_STATIC_LIB=ON -DCMAKE_INSTALL_PREFIX="$TC/binaryen-install" >"$TC/binaryen-cmake.log"
  cmake --build "$TC/binaryen/build" -j"$(nproc 2>/dev/null || echo 4)" >"$TC/binaryen-build.log"
  cmake --install "$TC/binaryen/build" >/dev/null
fi

say "Emscripten configuration"
cat >"$TC/emscripten/.emscripten" <<EOF
LLVM_ROOT = '$LLVM_ROOT'
BINARYEN_ROOT = '$TC/binaryen-install'
NODE_JS = '$(command -v node)'
CACHE = '$TC/em-cache'
EOF
# Emscripten's JS optimizer needs acorn. npm is the normal route; where npm
# cannot reach its registry, use acorn's own ES-module sources instead.
if ! (cd "$TC/emscripten" && npm ci --omit=dev --no-audit --no-fund >/dev/null 2>&1); then
  echo "  npm unavailable; vendoring acorn from git"
  mkdir -p "$TC/emscripten/node_modules/acorn"
  rm -rf "$TC/emscripten/node_modules/acorn/src"
  cp -r "$TC/acorn/acorn/src" "$TC/emscripten/node_modules/acorn/src"
  printf '{"name":"acorn","version":"8.11.2","type":"module","main":"src/index.js","exports":{".":"./src/index.js"}}\n' \
    >"$TC/emscripten/node_modules/acorn/package.json"
fi
# Mark Emscripten's own setup steps done (we did their work above).
(cd "$TC/emscripten" && python3 - <<'PY'
import os
import bootstrap
os.makedirs(bootstrap.STAMP_DIR, exist_ok=True)
for name, _, _ in bootstrap.actions:
    open(bootstrap.get_stamp_file(name), "w").write("stamp")
PY
)

say "Done. Next: tools/build.sh"
