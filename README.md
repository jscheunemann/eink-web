# eink-web

Real e-paper device firmware compiled to WebAssembly, so the devices run in a
browser: the same code, screens and behaviour as the hardware, for designing and
testing what they show without flashing anything.

| Device | Class | Firmware | Page | Status |
| --- | --- | --- | --- | --- |
| BOOX Picco | Reader | [CrossPoint](https://github.com/crosspoint-reader/crosspoint-reader) | `web/picco.html` | Working |
| TRMNL OG | Glance | [TRMNL firmware](https://github.com/usetrmnl/firmware) | `web/trmnl-og.html` | Working |
| TRMNL X | Glance | TRMNL firmware | — | Planned |

**Reader** devices are carried and read in sessions; **glance** devices sit on a
desk and show a server-rendered screen that refreshes on a schedule.

`web/trmnl-crosspoint.html` is a finished experiment, kept for the record: it
ran CrossPoint on a one-button landscape TRMNL to judge whether reading on a
TRMNL was worth a firmware fork. It wasn't; TRMNLs are glance devices.

## Build

```sh
tools/bootstrap.sh   # fetch pinned upstream + build the toolchain into .work/ (once)
tools/build.sh       # compile and link every device into dist/
```

Requirements: git, cmake, a C++ compiler, python3 with PyYAML, node, and
**LLVM 18** (clang + wasm-ld), which Emscripten 3.1.51 requires. If your system
LLVM is a different version, build in the container instead:

```sh
podman build -t eink-web-build .
podman run --rm -v "$PWD":/src:Z eink-web-build
```

(The container recipe has not been run end to end yet; the native path above
has.)

The first bootstrap takes 10–20 minutes (mostly building Binaryen). A full
compile takes about 4 minutes per device; later builds only recompile what
changed. Build a single device with `PICCO_DEVICES=picco tools/build.sh`.

`dist/` then holds one page, `.js` and `.wasm` per device, plus the boot
scripts (`picco-boot.js`, `trmnl-boot.js`). Skip the TRMNL build with
`PICCO_TRMNL=0`; build only it with `python3 firmware/trmnl/patch_trmnl.py &&
python3 firmware/trmnl/build.py --model og` (`--debug` adds symbols and runtime
checks).
Serve that folder over HTTP to try it; `file://` cannot load WebAssembly.

## Test

```sh
python3 tools/smoke.py picco.html wait:6000 shot:home tap:240,346 wait:1500 shot:files
```

Drives a page in headless Chrome through Playwright, serving `dist/` by request
interception (no local server), and saves screenshots to `build/shots/`.
Actions: `wait:MS`, `key:KEYCODE`, `hold:KEYCODE,MS`, `btn:MS` (presses a
page's on-screen button), `tap:X,Y` (in 480×800 screen pixels) and
`shot:NAME`. Set `VW`/`VH` for the viewport size.

## Layout

| Path | What it holds |
| --- | --- |
| `core/` | Shared browser platform layer: the cooperative fiber scheduler (`sim_coop.*`) that replaces threads, FreeRTOS on top of it (`freertos/`), C++ library gaps in Emscripten's libc++ (`web_compat.h`), self-contained hashes in place of OpenSSL, and stubs for calls a page cannot make |
| `firmware/crosspoint/` | CrossPoint adapter: the browser entry point, `patch_sim.py` (adapts the crosspoint-simulator host shims; every patch must match exactly or the build stops) and `resolve.py` (picks the libraries to build the way PlatformIO's library finder does) |
| `firmware/trmnl/` | TRMNL adapter: Arduino and ESP-IDF shims (`shim/`), an emulated UC81xx/SSD16xx panel controller that bb_epaper drives (`web_io.inl`), HTTP over the page's `fetch`, deep sleep with RTC memory (`web_platform.cpp`), and `patch_trmnl.py` for the few source fixes |
| `tools/` | Bootstrap, build, per-device seed SD cards (`make_seed.py`), smoke test, and `paths.py` |
| `web/` | Device pages; `picco-boot.js` mounts the SD card on IndexedDB, seeds it, and turns deep-sleep wake into a page reload; `trmnl-boot.js` does the same for the TRMNL and runs the mock TRMNL server (`/api/setup`, `/api/display`, screen images) |
| `upstream.lock` | Exact upstream commits for every firmware, library and toolchain component |

Adding a device means a board profile in `tools/build.py`, a seed in
`tools/make_seed.py`, and a page in `web/`. A new firmware gets its own folder
under `firmware/`.

## Not working yet

- Networking on CrossPoint: Wi-Fi features, OPDS catalogs, plugin downloads and
  the File Transfer web server run their screens, but nothing connects.
- TRMNL against your own server: set it under Device > Server address (the
  firmware's own `api_url` setting). The server must allow cross-origin
  requests, including preflights for the firmware's headers. The copy
  published on claude.ai can't reach other servers at all, so serve `dist/`
  from the server itself (same origin, no CORS needed) to use this.
- TRMNL X (16 grays, touch bar): not built yet.
- E-ink behaviour: refresh flashes, ghosting and panel timing are not modelled.

## Updating upstream

Pins live in `upstream.lock`. Bump one project at a time, run
`tools/bootstrap.sh && tools/build.sh`, and fix whatever the patch step or the
compiler reports. Firmware moves faster than the simulator shims, so expect
small gaps (missing methods) after a firmware bump.

## License

GPL-3.0, because the TRMNL firmware this repository builds is GPL-3.0. Upstream
projects keep their own licenses; see
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
