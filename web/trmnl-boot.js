// Boots the TRMNL firmware (WebAssembly) as a browser device.
//
// Provides Module.eink, the firmware's link to the page (firmware/trmnl/
// web_platform.cpp): panel frames, HTTP, deep sleep and wake, battery and
// button. Requests to the TRMNL server are answered by a mock server in this
// page that serves sample screens and any images the viewer adds.
//
// Deep sleep saves storage and reloads the page; the reload is the wake. RTC
// memory and the wake cause are saved with the device's storage.
//
// The page provides <canvas id="panel"> and, optionally, window.einkScreens
// (built-in screens: { name, title, draw(ctx, w, h) }) and window.einkUi with
// hooks onBoot(info), onFrame(info), onSleep(ms), onLog(line),
// onRequest(method, url, mocked).
(function () {
  const ROOT = "/persist";
  const PAGE = ROOT + "/.page";          // page-side state; the firmware never reads it
  const RTC = PAGE + "/rtc.bin";
  const WAKE = PAGE + "/wake";
  const UPLOADS = PAGE + "/screens";
  const PANEL = PAGE + "/panel.bin";     // what the panel shows; e-paper keeps it while asleep
  const SHOWN = PAGE + "/shown";
  const W = 800, H = 480;
  const ui = () => window.einkUi || {};
  let syncing = false;
  let rtc = null;
  let wake = null;            // "timer" | "button" | null (cold boot)
  let buttonPending = false;
  let buttonDownUntil = 0;

  function log(t) { console.log("[trmnl] " + t); }

  function persist(done) {
    if (syncing) { setTimeout(() => persist(done), 50); return; }
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

  const mkdirs = (FS, p) => {
    let cur = "";
    for (const part of p.split("/").filter(Boolean)) {
      cur += "/" + part;
      try { FS.mkdir(cur); } catch (e) {}
    }
  };

  // Viewer preferences (refresh interval, serial log): per browser, optional.
  const KEY = "eink-web-trmnl.";
  function pref(k, v) {
    try {
      if (v === undefined) return JSON.parse(localStorage.getItem(KEY + k));
      localStorage.setItem(KEY + k, JSON.stringify(v));
    } catch (e) { return null; }
    return v;
  }

  // ---- Mock TRMNL server -------------------------------------------------
  // Answers the hosts the firmware talks to by default.
  const MOCK_HOSTS = /^https?:\/\/(trmnl\.app|usetrmnl\.com|[a-z0-9-]+\.trmnl\.app)(\/|$)/i;
  const IMG = "https://trmnl.app/eink-web/screens/";
  const enc = (s) => new TextEncoder().encode(s);

  const server = {
    refreshRate: pref("refresh") || 300, // seconds; the firmware sleeps this long
    uploads: [],                         // { name, title, bytes }
    index: pref("index") || 0,
    shown: null,                         // name of the screen last served
    screens() {
      const builtIn = (window.einkScreens || []).map((s) => ({ name: s.name, title: s.title || s.name, builtIn: true }));
      return builtIn.concat(this.uploads.map((u) => ({ name: u.name, title: u.title, builtIn: false })));
    },
    setRefresh(sec) { this.refreshRate = sec; pref("refresh", sec); },
    queue(name) {
      const i = this.screens().findIndex((s) => s.name === name);
      if (i >= 0) { this.index = i; pref("index", i); }
    },
    upNext() {
      const list = this.screens();
      return list.length ? list[this.index % list.length].name : null;
    },
    next() {
      const list = this.screens();
      if (!list.length) return null;
      const s = list[this.index % list.length];
      this.index = (this.index + 1) % list.length;
      pref("index", this.index);
      return s;
    },
    async handle(method, url) {
      const path = url.replace(MOCK_HOSTS, "/").replace(/\?.*$/, "");
      const json = (o) => ({ status: 200, headers: { "content-type": "application/json" }, body: enc(JSON.stringify(o)) });
      if (path === "/api/setup") {
        return json({ status: 200, api_key: "eink-web-demo-key", friendly_id: "EINKWB", image_url: IMG + "setup.png", message: "Registered with the eink-web mock server" });
      }
      if (path === "/api/display" || path === "/api/current_screen") {
        const s = this.next();
        const name = s ? s.name : "setup";
        this.shown = name;
        return json({
          status: 0,
          image_url: IMG + encodeURIComponent(name) + ".png?t=" + Date.now(),
          filename: name.replace(/[^a-z0-9_-]/gi, "-").slice(0, 32) + "-" + (Date.now() % 100000),
          refresh_rate: this.refreshRate,
          update_firmware: false,
          firmware_url: "",
          reset_firmware: false,
          special_function: "identify",
        });
      }
      if (path === "/api/log") return { status: 204, headers: {}, body: new Uint8Array(0) };
      if (path.startsWith("/eink-web/screens/")) {
        const name = decodeURIComponent(path.slice("/eink-web/screens/".length).replace(/\.png$/, ""));
        return { status: 200, headers: { "content-type": "image/png" }, body: await this.image(name) };
      }
      return { status: 404, headers: {}, body: enc("not found") };
    },
    async image(name) {
      const up = this.uploads.find((u) => u.name === name);
      if (up) return up.bytes;
      const s = (window.einkScreens || []).find((x) => x.name === name) || { draw: drawSetup };
      if (document.fonts && document.fonts.ready) await document.fonts.ready;
      const c = document.createElement("canvas");
      c.width = W; c.height = H;
      const g = c.getContext("2d");
      g.fillStyle = "#fff"; g.fillRect(0, 0, W, H);
      g.fillStyle = "#000"; g.strokeStyle = "#000";
      s.draw(g, W, H);
      return toPng(c);
    },
  };

  async function toPng(canvas) {
    const blob = await new Promise((r) => canvas.toBlob(r, "image/png"));
    return new Uint8Array(await blob.arrayBuffer());
  }

  function drawSetup(g, w, h) {
    g.textAlign = "center";
    g.font = "600 44px system-ui, sans-serif";
    g.fillText("eink-web", w / 2, h / 2 - 10);
    g.font = "24px system-ui, sans-serif";
    g.fillText("TRMNL firmware running in your browser", w / 2, h / 2 + 34);
  }

  // An image the viewer adds is fitted to 800x480 on white and re-encoded as
  // PNG, saved with the page's state, and shown on the next wake.
  async function addScreen(file) {
    const bmp = await createImageBitmap(file);
    const c = document.createElement("canvas");
    c.width = W; c.height = H;
    const g = c.getContext("2d");
    g.fillStyle = "#fff"; g.fillRect(0, 0, W, H);
    const k = Math.min(W / bmp.width, H / bmp.height);
    const dw = bmp.width * k, dh = bmp.height * k;
    g.drawImage(bmp, (W - dw) / 2, (H - dh) / 2, dw, dh);
    const bytes = await toPng(c);
    const title = file.name.replace(/\.[^.]+$/, "").slice(0, 60) || "Image";
    const name = "added-" + Date.now().toString(36);
    server.uploads.push({ name, title, bytes });
    try {
      Module.FS.writeFile(UPLOADS + "/" + name + ".png", bytes);
      Module.FS.writeFile(UPLOADS + "/" + name + ".title", title);
    } catch (e) {}
    server.queue(name);
    persist();
    return name;
  }

  function removeScreen(name) {
    server.uploads = server.uploads.filter((u) => u.name !== name);
    for (const ext of [".png", ".title"]) { try { Module.FS.unlink(UPLOADS + "/" + name + ext); } catch (e) {} }
    server.index = 0; pref("index", 0);
    persist();
  }

  // ---- Device settings (NVS) ----------------------------------------------
  // Preferences live in /persist/nvs/<namespace>.kv, one "hexkey<TAB>hexvalue"
  // line per entry; numbers are decimal text (firmware/trmnl/shim/Preferences.h).
  const hex = (bytes) => Array.from(bytes, (b) => b.toString(16).padStart(2, "0")).join("");
  function setPref(ns, key, text) {
    const FS = Module.FS, file = ROOT + "/nvs/" + ns + ".kv";
    const kv = new Map();
    if (FS.analyzePath(file).exists) {
      for (const line of FS.readFile(file, { encoding: "utf8" }).split("\n")) {
        const [k, v] = line.split("\t");
        if (k && v !== undefined) kv.set(k, v);
      }
    }
    kv.set(hex(enc(key)), hex(enc(text)));
    FS.writeFile(file, Array.from(kv, ([k, v]) => k + "\t" + v + "\n").join(""));
  }

  // ---- HTTP --------------------------------------------------------------
  const requests = new Map();
  let nextId = 1;
  function fetchStart(method, url, headers, body) {
    const id = nextId++;
    const r = { state: 0, status: 0, body: new Uint8Array(0), headers: {} };
    requests.set(id, r);
    const mocked = MOCK_HOSTS.test(url);
    if (ui().onRequest) ui().onRequest(method, url, mocked);
    const done = (res) => { r.status = res.status; r.body = res.body; r.headers = res.headers; r.state = 1; };
    const fail = (e) => { log("request failed: " + url + " " + e); r.state = -1; };
    if (mocked) {
      setTimeout(() => server.handle(method, url).then(done, fail), 120); // a network's worth of delay
    } else {
      fetch(url, { method, headers, body: body || undefined })
        .then(async (res) => {
          const h = {};
          res.headers.forEach((v, k) => { h[k.toLowerCase()] = v; });
          done({ status: res.status, headers: h, body: new Uint8Array(await res.arrayBuffer()) });
        }, fail);
    }
    return id;
  }

  // ---- Panel -------------------------------------------------------------
  const panel = document.getElementById("panel");
  // UC81xx RAM: in 1-bit mode a set bit is white; in 4-gray mode the two
  // planes together pick one of four levels, 0 (both set) being black.
  const GRAY4 = [0, 85, 170, 255];
  let lastFrame = null;
  function present(p0, p1, w, h, gray4, restoring) {
    lastFrame = { p0, p1, w, h, gray4 };
    if (panel.width !== w || panel.height !== h) { panel.width = w; panel.height = h; }
    const g = panel.getContext("2d");
    const img = g.createImageData(w, h);
    const d = img.data;
    const pitch = (w + 7) >> 3;
    for (let y = 0; y < h; y++) {
      for (let x = 0; x < w; x++) {
        const i = y * pitch + (x >> 3), m = 0x80 >> (x & 7);
        const a = (p0[i] & m) ? 1 : 0, b = (p1[i] & m) ? 1 : 0;
        const v = gray4 ? GRAY4[3 - ((b << 1) | a)] : (a ? 255 : 0);
        const o = (y * w + x) * 4;
        d[o] = d[o + 1] = d[o + 2] = v;
        d[o + 3] = 255;
      }
    }
    g.putImageData(img, 0, 0);
    if (!restoring && ui().onFrame) ui().onFrame({ w, h, gray4: !!gray4, screen: server.shown });
  }

  // ---- Sleep and wake ----------------------------------------------------
  let asleep = false;
  function deepSleep(rtcBytes, sleepMs) {
    try { Module.FS.writeFile(RTC, rtcBytes); } catch (e) {}
    if (lastFrame) {
      const f = lastFrame, n = f.p0.length;
      const out = new Uint8Array(5 + 2 * n);
      out.set([f.w & 255, f.w >> 8, f.h & 255, f.h >> 8, f.gray4 ? 1 : 0]);
      out.set(f.p0, 5); out.set(f.p1, 5 + n);
      try { Module.FS.writeFile(PANEL, out); } catch (e) {}
    }
    try { Module.FS.writeFile(SHOWN, server.shown || ""); } catch (e) {}
    persist(() => {
      asleep = true;
      log("deep sleep for " + Math.round(sleepMs / 1000) + " s");
      if (ui().onSleep) ui().onSleep(sleepMs);
      else setTimeout(() => wakeUp("timer"), sleepMs);
    });
  }

  // Wakes the sleeping device: "timer" (its refresh interval ran out) or
  // "button" (the button on the back was pressed).
  function wakeUp(cause) {
    try { Module.FS.writeFile(WAKE, cause); } catch (e) {}
    persist(() => location.reload());
  }

  // ESP_SLEEP_WAKEUP_*: 0 cold boot, 4 timer, 7 GPIO (the button).
  const WAKE_CAUSE = { timer: 4, button: 7 };

  window.eink = {
    server,
    addScreen,
    removeScreen,
    wakeUp,
    isAsleep: () => asleep,
    serialLog: (on) => (on === undefined ? !!pref("serial") : pref("serial", !!on)),
    // Wipes the device's storage (Wi-Fi, API key, cached images) and cold-boots.
    // Screens the viewer added are kept.
    factoryReset() {
      const FS = Module.FS;
      const rm = (p) => {
        for (const n of FS.readdir(p)) {
          if (n === "." || n === "..") continue;
          const q = p + "/" + n;
          if (q === PAGE) continue;
          if (FS.isDir(FS.stat(q).mode)) { rm(q); FS.rmdir(q); } else FS.unlink(q);
        }
      };
      try { rm(ROOT); } catch (e) {}
      for (const f of [RTC, PANEL, SHOWN]) { try { FS.unlink(f); } catch (e) {} }
      server.index = 0; pref("index", 0);
      persist(() => location.reload());
    },
  };

  window.Module = {
    print: (t) => { if (ui().onLog) ui().onLog(t); else console.log(t); },
    printErr: (t) => { if (ui().onLog) ui().onLog(t); else console.log(t); },
    eink: {
      present,
      fetchStart,
      fetchState: (id) => (requests.get(id) || { state: -1 }).state,
      fetchStatus: (id) => (requests.get(id) || {}).status || 0,
      fetchBody: (id) => (requests.get(id) || {}).body || new Uint8Array(0),
      fetchHeader: (id, name) => {
        const r = requests.get(id);
        const v = r && r.headers[name.toLowerCase()];
        return v === undefined ? null : v;
      },
      fetchFree: (id) => { requests.delete(id); },
      deepSleep,
      rtcData: () => rtc,
      wakeCause: () => WAKE_CAUSE[wake] || 0,
      batteryMv: () => 4050,
      // After a button wake the pin reads LOW (pressed) for a short press.
      pinRead: () => {
        if (buttonPending) { buttonPending = false; buttonDownUntil = performance.now() + 150; }
        return performance.now() < buttonDownUntil ? 0 : 1;
      },
      restart: () => persist(() => location.reload()),
    },
    preRun: [function () {
      const FS = Module.FS;
      mkdirs(FS, ROOT);
      FS.mount(Module.IDBFS, {}, ROOT);
      Module.addRunDependency("persist");
      FS.syncfs(true, (err) => {
        if (err) log("load failed: " + err);
        mkdirs(FS, ROOT + "/nvs");
        mkdirs(FS, ROOT + "/flash");
        mkdirs(FS, UPLOADS);
        // The wake cause and RTC memory apply to this boot only.
        if (FS.analyzePath(WAKE).exists) {
          wake = FS.readFile(WAKE, { encoding: "utf8" });
          FS.unlink(WAKE);
          if (FS.analyzePath(RTC).exists) rtc = FS.readFile(RTC);
        }
        try { FS.unlink(RTC); } catch (e) {}
        buttonPending = wake === "button";
        // Added screens.
        for (const n of FS.readdir(UPLOADS)) {
          if (!n.endsWith(".png")) continue;
          const name = n.slice(0, -4);
          const tf = UPLOADS + "/" + name + ".title";
          const title = FS.analyzePath(tf).exists ? FS.readFile(tf, { encoding: "utf8" }) : name;
          server.uploads.push({ name, title, bytes: FS.readFile(UPLOADS + "/" + n) });
        }
        server.uploads.sort((a, b) => a.name.localeCompare(b.name));
        // The panel still shows what it showed before the device slept.
        if (FS.analyzePath(SHOWN).exists) server.shown = FS.readFile(SHOWN, { encoding: "utf8" }) || null;
        if (FS.analyzePath(PANEL).exists) {
          const b = FS.readFile(PANEL), w = b[0] | (b[1] << 8), h = b[2] | (b[3] << 8), n = ((w + 7) >> 3) * h;
          if (b.length === 5 + 2 * n) present(b.subarray(5, 5 + n), b.subarray(5 + n), w, h, b[4], true);
        }
        // The firmware's serial log (its verbose_log setting) follows the page's switch.
        setPref("data", "verbose_log", pref("serial") ? "1" : "0");
        if (ui().onBoot) ui().onBoot({ wake });
        Module.removeRunDependency("persist");
      });
    }],
  };
})();
