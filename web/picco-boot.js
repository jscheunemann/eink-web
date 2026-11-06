// Boots the CrossPoint firmware (WebAssembly) as a browser Picco.
// Load before picco.js. The page must contain <canvas id="canvas"> (the id Emscripten SDL2 targets).
(function () {
  const SD = "/fs_"; // the simulator's SD-card root (SimulatorStorageRoot.h)
  const params = new URLSearchParams(location.hash.slice(1));
  let syncing = false;

  function log(t) { console.log("[picco] " + t); }

  function persist(done) {
    if (syncing) { if (done) setTimeout(() => persist(done), 50); return; }
    syncing = true;
    try {
      Module.FS.syncfs(false, (err) => {
        syncing = false;
        if (err) log("save failed: " + err);
        if (done) done();
      });
    } catch (e) {
      syncing = false;
      if (done) done();
    }
  }

  function seed(FS) {
    if (FS.analyzePath(SD + "/.picco-web-seeded").exists) return;
    for (const dir of ["/books"]) {
      try { FS.mkdir(SD + dir); } catch (e) {}
      for (const name of FS.readdir("/seed" + dir)) {
        if (name === "." || name === "..") continue;
        FS.writeFile(SD + dir + "/" + name, FS.readFile("/seed" + dir + "/" + name));
      }
    }
    FS.writeFile(SD + "/.picco-web-seeded", "1");
    log("seeded SD card");
  }

  window.Module = {
    canvas: document.getElementById("canvas"),
    print: (t) => console.log(t),
    printErr: (t) => console.warn(t),
    preRun: [function () {
      const FS = Module.FS;
      if (params.get("wake") === "power") Module.ENV.CROSSPOINT_SIM_WAKE_REASON = "power";
      if (params.get("switch")) Module.ENV.CROSSPOINT_SIM_SLIDE_SWITCH = params.get("switch");
      if (params.has("wake")) history.replaceState(null, "", location.pathname + location.search);
      // The simulator resolves the SD card as ./fs_ from the working directory.
      FS.chdir("/");
      try { FS.mkdir(SD); } catch (e) {}
      try {
        FS.mount(Module.IDBFS, {}, SD);
      } catch (e) {
        log("no persistent storage; this session only");
      }
      Module.addRunDependency("sd-card");
      FS.syncfs(true, (err) => {
        if (err) log("could not load saved SD card: " + err);
        Module.removeRunDependency("sd-card");
        setInterval(() => persist(), 5000);
      });
    }],
    // Embedded seed files exist only once the runtime has initialised, which
    // is still before the firmware's main() runs.
    onRuntimeInitialized() {
      try { seed(Module.FS); } catch (e) { log("seed failed: " + e + " errno=" + (e && e.errno)); }
    },
    picco: {
      // Deep-sleep wake: the desktop simulator relaunches the process; here we
      // save the SD card and reload the page as a power-button wake.
      reboot(switchPos) {
        persist(() => {
          location.hash = "wake=power&switch=" + switchPos;
          location.reload();
        });
      },
      persist,
      // Slide-switch position as last set from the page (the firmware starts
      // "off" unless a wake reload carries the position across).
      switchOn: params.get("switch") === "on",
      // Cold boot: save the SD card, then reload.
      restart() {
        persist(() => location.reload());
      },
      // Erase the simulated SD card (books, settings, caches) and cold boot.
      resetCard() {
        const FS = Module.FS;
        const wipe = (dir) => {
          for (const name of FS.readdir(dir)) {
            if (name === "." || name === "..") continue;
            const path = dir + "/" + name;
            if (FS.isDir(FS.stat(path).mode)) {
              wipe(path);
              FS.rmdir(path);
            } else {
              FS.unlink(path);
            }
          }
        };
        try { wipe(SD); } catch (e) { log("reset failed: " + e); }
        persist(() => location.reload());
      },
      // Copy a user file onto the simulated SD card.
      addFile(name, bytes) {
        const FS = Module.FS;
        try { FS.mkdir(SD + "/books"); } catch (e) {}
        const safe = name.replace(/[\\/:*?"<>|]/g, "_");
        FS.writeFile(SD + "/books/" + safe, bytes);
        persist();
        return "/books/" + safe;
      },
      // Press a device button by injecting the key SDL maps to it.
      press(keyCode, down) {
        const ev = new KeyboardEvent(down ? "keydown" : "keyup", { bubbles: true, cancelable: true });
        Object.defineProperty(ev, "keyCode", { get: () => keyCode });
        Object.defineProperty(ev, "which", { get: () => keyCode });
        window.dispatchEvent(ev);
      },
    },
  };
  window.addEventListener("pagehide", () => persist());
})();
