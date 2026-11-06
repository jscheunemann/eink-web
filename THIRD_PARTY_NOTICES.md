# Third-party notices

This repository is licensed GPL-3.0 (see LICENSE). It contains build scripts
and browser adaptations, and does not
vendor upstream source: `tools/bootstrap.sh` fetches each project at the
commit pinned in `upstream.lock`. The built `dist/` output contains compiled
code from these projects, under their own licenses.

| Project | License | Used for |
| --- | --- | --- |
| [CrossPoint Reader](https://github.com/crosspoint-reader/crosspoint-reader) | MIT, © 2025 CrossPoint Reader organization | The firmware itself |
| [FreeInk SDK](https://github.com/Free-Ink/freeink-sdk) | MIT, © 2026 FreeInk (see its NOTICE for bundled components) | Board profiles, UI, fonts |
| [crosspoint-simulator](https://github.com/uxjulia/crosspoint-simulator) | MIT, © 2026 Julia Nguyen | Host shims for display, input, storage |
| [ArduinoJson](https://github.com/bblanchon/ArduinoJson) | MIT, © Benoît Blanchon | JSON parsing |
| [AnimatedGIF](https://github.com/bitbank2/AnimatedGIF) | Apache-2.0, © 2020 BitBank Software, Inc. | GIF decoding |
| [Emscripten](https://github.com/emscripten-core/emscripten) | MIT / University of Illinois NCSA | Compiler toolchain and runtime |
| [SDL 2](https://github.com/libsdl-org/SDL) | zlib | Window, input and drawing in the browser |

## Derived files

`core/freertos/*.h` keep the API surface of the simulator's
`src/freertos/` shims (task handles, notifications, recursive mutexes, binary
semaphores) with new cooperative-fiber implementations. The simulator's
license applies to that interface:

```
MIT License

Copyright (c) 2026 Julia Nguyen

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```
