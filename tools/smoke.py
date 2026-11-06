#!/usr/bin/env python3
"""Boot the browser Picco in headless Chrome and screenshot it.

Files are served from dist/ through Playwright request interception, so no
local web server is started. Usage: smoke.py [page] [actions...]
Actions: wait:MS  key:CODE  tap:X,Y  shot:NAME
"""
import mimetypes
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import paths  # noqa: E402

from playwright.sync_api import sync_playwright

DIST = paths.DIST
SHOTS = paths.SHOTS
ORIGIN = "http://picco.test"


def serve(route):
    path = route.request.url[len(ORIGIN):].split("?")[0].split("#")[0].lstrip("/") or "index.html"
    full = os.path.join(DIST, path)
    if not os.path.isfile(full):
        route.fulfill(status=404, body="not found")
        return
    ctype = "application/wasm" if full.endswith(".wasm") else (mimetypes.guess_type(full)[0] or "application/octet-stream")
    route.fulfill(status=200, body=open(full, "rb").read(), headers={"Content-Type": ctype})


def main():
    page_name = sys.argv[1] if len(sys.argv) > 1 else "test.html"
    actions = sys.argv[2:] or ["wait:8000", "shot:boot"]
    os.makedirs(SHOTS, exist_ok=True)
    logs = []
    with sync_playwright() as p:
        browser = p.chromium.launch(executable_path="/usr/bin/google-chrome", args=["--no-sandbox"])
        page = browser.new_page(viewport={"width": int(os.environ.get("VW", 520)), "height": int(os.environ.get("VH", 900))})
        page.route(ORIGIN + "/**", serve)
        page.on("console", lambda m: logs.append(f"{m.type}: {m.text}"))
        page.on("pageerror", lambda e: logs.append(f"PAGEERROR: {e}"))
        page.goto(f"{ORIGIN}/{page_name}")
        for a in actions:
            kind, _, arg = a.partition(":")
            if kind == "wait":
                page.wait_for_timeout(int(arg))
            elif kind == "key":
                page.evaluate(f"Module.picco.press({int(arg)}, true)")
                page.wait_for_timeout(120)
                page.evaluate(f"Module.picco.press({int(arg)}, false)")
                page.wait_for_timeout(600)
            elif kind == "hold":
                code, ms = (int(v) for v in arg.split(","))
                page.evaluate(f"Module.picco.press({code}, true)")
                page.wait_for_timeout(ms)
                page.evaluate(f"Module.picco.press({code}, false)")
                page.wait_for_timeout(600)
            elif kind == "tap":
                x, y = (int(v) for v in arg.split(","))
                box = page.locator("#canvas").bounding_box(); sx = box["width"] / 480; sy = box["height"] / 800
                page.mouse.click(box["x"] + x * sx, box["y"] + y * sy)
                page.wait_for_timeout(600)
            elif kind == "shot":
                out = os.path.join(SHOTS, arg + ".png")
                page.screenshot(path=out)
                logs.append(f"[shot] {out}")
        size = page.evaluate("(() => { const c = document.getElementById('canvas'); return c.width + 'x' + c.height; })()")
        logs.append(f"[canvas] {size}")
        browser.close()
    for line in logs[-60:]:
        print(line[:300])


if __name__ == "__main__":
    main()
