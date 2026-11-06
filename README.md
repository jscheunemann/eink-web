# Picco Web

The [CrossPoint](https://github.com/crosspoint-reader/crosspoint-reader) e-reader
firmware for the BOOX Picco, compiled to WebAssembly so a working Picco runs in a
browser: touch, page buttons, power and sleep, the slide switch, and an SD card
that persists between visits.

It builds the real firmware source, unchanged, against the host shims from
[crosspoint-simulator](https://github.com/uxjulia/crosspoint-simulator), with a
small set of browser adaptations in `overrides/` and `tools/patch_sim.py`.

## Build

```sh
tools/bootstrap.sh   # fetch pinned upstream + build the toolchain into .work/ (once)
tools/build.sh       # compile and link into dist/
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

`dist/` then holds `index.html`, `picco.js`, `picco.wasm` and `picco-boot.js`.
Serve that folder over HTTP to try it; `file://` cannot load WebAssembly.

## Test

```sh
python3 tools/smoke.py index.html wait:6000 shot:home tap:240,346 wait:1500 shot:files
```

Drives the page in headless Chrome through Playwright, serving `dist/` by
request interception (no local server), and saves screenshots to
`build/shots/`. Actions: `wait:MS`, `key:KEYCODE`, `hold:KEYCODE,MS`,
`tap:X,Y` (in 480×800 screen pixels) and `shot:NAME`. Set `VW`/`VH` for the
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
| `web/index.html` | The device frame, controls and page |

## Not working yet

- Networking: Wi-Fi features, OPDS catalogs, plugin downloads and the File
  Transfer web server. The firmware runs these screens, but nothing connects.
- E-ink behaviour: refresh flashes, ghosting and panel timing are not modelled.

## Updating upstream

Pins live in `upstream.lock`. Bump one project at a time, run
`tools/bootstrap.sh && tools/build.sh`, and fix whatever `patch_sim.py` or the
compiler reports. The firmware moves faster than the simulator, so expect small
shim gaps (missing methods) after a firmware bump.

## License

MIT for this repository. Upstream projects keep their own licenses; see
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
