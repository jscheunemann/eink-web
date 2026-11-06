#!/usr/bin/env bash
# Build dist/ (picco.wasm, picco.js and the page) from the sources fetched by
# tools/bootstrap.sh. Incremental: unchanged objects are reused.
set -euo pipefail

REPO=$(cd "$(dirname "$0")/.." && pwd)
WORK=${PICCO_WORK:-$REPO/.work}
FW=$WORK/upstream/crosspoint-reader
[ -d "$FW/src" ] || { echo "build: run tools/bootstrap.sh first" >&2; exit 1; }

echo "== Firmware code generation"
# The firmware's web UI minifiers are optional for this build; fall back to
# pass-through stand-ins when they are not installed.
PYPATH=""
python3 -c 'import rcssmin, rjsmin' 2>/dev/null || PYPATH=$REPO/tools/pyshim
(cd "$FW" && PYTHONPATH=$PYPATH python3 scripts/build_html.py >/dev/null && python3 scripts/gen_i18n.py >/dev/null)

echo "== Seed SD card"
python3 "$REPO/tools/make_seed.py"
echo "== Simulator sources"
python3 "$REPO/tools/patch_sim.py"
python3 "$REPO/tools/resolve.py"
echo "== Compile and link"
python3 "$REPO/tools/build.py"
