# Picco Web

The [CrossPoint](https://github.com/crosspoint-reader/crosspoint-reader) e-reader
firmware for the BOOX Picco, compiled to WebAssembly so a working Picco runs in a
browser: touch, page buttons, power and sleep, the slide switch, and an SD card
that persists between visits.

A second target, **TRMNL Reader** (`web/trmnl.html`), runs the same firmware on
a TRMNL-sized 800×480 landscape panel driven by a single button, to try reading
on a TRMNL before porting the firmware to it. CrossPoint has no TRMNL board, so
it uses the closest one it has, the Xteink X4 Classic (800×480, buttons only).

It builds the real firmware source, unchanged, against the host shims from
[crosspoint-simulator](https://github.com/uxjulia/crosspoint-simulator), with a
small set of browser adaptations in `overrides/` and `tools/patch_sim.py`.

## Build

```sh
tools/bootstrap.sh   # fetch pinned upstream + build the toolchain into .work/ (once)
tools/build.sh       # compile and link both devices into dist/
```

Requirements: git, cmake, a C++ compiler, python3 with PyYAML, node, and
**LLVM 18** (clang + wasm-ld), which Emscripten 3.1.51 requires. If your system
LLVM is a different version, build in the container instead:

```sh
podman build -t picco-web-build .
podman run --rm -v "$PWD":/src:Z picco-web-build
```

(The container recipe has not been run end to end yet; the native path above
has.)

The first bootstrap takes 10–20 minutes (mostly building Binaryen). A full
compile takes about 4 minutes; later builds only recompile what changed.

`dist/` then holds the pages (`index.html` for the Picco, `trmnl.html` for the
TRMNL), one `.js`/`.wasm` pair per device, and `picco-boot.js`. Build one
device with `PICCO_DEVICES=trmnl tools/build.sh`.
Serve that folder over HTTP to try it; `file://` cannot load WebAssembly.

## Test

```sh
python3 tools/smoke.py index.html wait:6000 shot:home tap:240,346 wait:1500 shot:files
```

Drives the page in headless Chrome through Playwright, serving `dist/` by
request interception (no local server), and saves screenshots to
`build/shots/`. Actions: `wait:MS`, `key:KEYCODE`, `hold:KEYCODE,MS`, `btn:MS` (presses the
TRMNL page's button), `tap:X,Y` (in 480×800 screen pixels) and `shot:NAME`. Set `VW`/`VH` for the
viewport size.

## How it works

| Piece | What it does |
| --- | --- |
| `tools/resolve.py` | Follows `#include`s from the firmware and simulator sources to pick the libraries to build, the way PlatformIO's library finder does |
| `tools/patch_sim.py` | Copies the simulator's shims and applies the browser changes; each patch must match exactly or the build stops |
| `overrides/sim_coop.*` | Cooperative scheduler: the firmware's main loop and render task run as Emscripten fibers, driven from the browser's frame callback, so no threads or cross-origin isolation are needed |
| `overrides/freertos/` | FreeRTOS tasks, notifications and semaphores on top of that scheduler |
| `overrides/web_compat.h` | `std::atomic_ref` and float `std::from_chars`, missing from Emscripten 3.1.51's libc++ |
| `overrides/openssl/sha.h`, `MD5Builder_web.h` | Self-contained hashes in place of OpenSSL |
| `web/picco-boot.js` | Mounts the SD card on IndexedDB, seeds the guide book, turns deep-sleep wake into a page reload |
| `web/index.html` | The Picco page: device frame and controls |
| `web/trmnl.html` | The TRMNL page: landscape panel, one-button scheme (tap next, double-tap previous, hold select, long hold back) |
| `tools/make_seed.py` | Per-device seed SD card: a guide book, plus landscape reading for the TRMNL |

## Not working yet

- Networking: Wi-Fi features, OPDS catalogs, plugin downloads and the File
  Transfer web server. The firmware runs these screens, but nothing connects.
- E-ink behaviour: refresh flashes, ghosting and panel timing are not modelled.
- TRMNL: CrossPoint draws menus in portrait only, so they appear sideways on the
  landscape panel (as they would on real hardware). The TRMNL X (1872×1404, 16
  grays) has no CrossPoint display driver and is not covered.

## Updating upstream

Pins live in `upstream.lock`. Bump one project at a time, run
`tools/bootstrap.sh && tools/build.sh`, and fix whatever `patch_sim.py` or the
compiler reports. The firmware moves faster than the simulator, so expect small
shim gaps (missing methods) after a firmware bump.

## License

MIT for this repository. Upstream projects keep their own licenses; see
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
