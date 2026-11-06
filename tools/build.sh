#!/usr/bin/env bash
# Build dist/ (one .js/.wasm pair per device, plus the pages) from the sources
# fetched by tools/bootstrap.sh. Incremental: unchanged objects are reused.
# PICCO_DEVICES limits the targets, e.g. PICCO_DEVICES=picco tools/build.sh
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
python3 "$REPO/firmware/crosspoint/patch_sim.py"
python3 "$REPO/firmware/crosspoint/resolve.py"
for device in ${PICCO_DEVICES:-picco trmnl-crosspoint}; do
  echo "== Compile and link: $device"
  python3 "$REPO/tools/build.py" --device "$device"
done
