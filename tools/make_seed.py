#!/usr/bin/env python3
"""Build the seed SD card content: a short guide EPUB for the browser Picco."""
import zipfile
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import paths  # noqa: E402

OUT = os.path.join(paths.SEED, "books")

CHAPTERS = [
    ("Welcome", """
<p>This is CrossPoint Reader running in your browser, compiled from the real
firmware source to WebAssembly. The screen you are reading is drawn by the same
code that drives the Picco's e-ink panel.</p>
<p>Everything you do here stays in this browser. Books you add and settings you
change are saved on this device only.</p>"""),
    ("Controls", """
<p><b>Page buttons.</b> The two buttons on the right side of the frame turn
pages and move through menus, like the Picco's own buttons.</p>
<p><b>Touch.</b> Tap or swipe on the screen, as on the real touch panel.</p>
<p><b>Power.</b> Hold it for about half a second to sleep; press any button
to wake. Waking restarts the page, the same way the device boots fresh after
deep sleep.</p>
<p><b>Slide switch.</b> Toggles the Picco's two-position switch. What it does
is set in the firmware's settings.</p>
<p>On a keyboard: Up and Down arrows, P for power (hold to sleep), Return
for confirm, T for the switch.</p>"""),
    ("Adding books", """
<p>Use the <b>Add EPUB</b> button next to the device to copy a book from your
phone or computer onto the simulated SD card. Open <b>Browse Files</b>, then
<b>books</b>, to find it.</p>"""),
    ("What is not here yet", """
<p>Wi-Fi features, the file-transfer web server and catalog downloads need the
network, which this first browser build does not connect yet.</p>
<p>The screen is a pixel-exact image of the framebuffer, not a model of e-ink
physics: there is no refresh flash, ghosting or panel timing.</p>"""),
]


def xhtml(title, body):
    return f"""<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE html>
<html xmlns="http://www.w3.org/1999/xhtml"><head><title>{title}</title></head>
<body><h1>{title}</h1>{body}</body></html>
"""


def main():
    os.makedirs(OUT, exist_ok=True)
    path = os.path.join(OUT, "Picco Web Guide.epub")
    with zipfile.ZipFile(path, "w") as z:
        z.writestr("mimetype", "application/epub+zip", compress_type=zipfile.ZIP_STORED)
        z.writestr("META-INF/container.xml", """<?xml version="1.0"?>
<container version="1.0" xmlns="urn:oasis:names:tc:opendocument:xmlns:container">
<rootfiles><rootfile full-path="OEBPS/content.opf" media-type="application/oebps-package+xml"/></rootfiles>
</container>""")
        manifest, spine, nav = [], [], []
        for i, (title, body) in enumerate(CHAPTERS, 1):
            z.writestr(f"OEBPS/ch{i}.xhtml", xhtml(title, body), compress_type=zipfile.ZIP_DEFLATED)
            manifest.append(f'<item id="ch{i}" href="ch{i}.xhtml" media-type="application/xhtml+xml"/>')
            spine.append(f'<itemref idref="ch{i}"/>')
            nav.append(f'<li><a href="ch{i}.xhtml">{title}</a></li>')
        z.writestr("OEBPS/nav.xhtml", f"""<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE html>
<html xmlns="http://www.w3.org/1999/xhtml" xmlns:epub="http://www.idpf.org/2007/ops">
<head><title>Contents</title></head><body><nav epub:type="toc"><ol>{''.join(nav)}</ol></nav></body></html>""")
        z.writestr("OEBPS/content.opf", f"""<?xml version="1.0" encoding="utf-8"?>
<package xmlns="http://www.idpf.org/2007/opf" version="3.0" unique-identifier="uid">
<metadata xmlns:dc="http://purl.org/dc/elements/1.1/">
<dc:identifier id="uid">picco-web-guide-1</dc:identifier>
<dc:title>Picco Web Guide</dc:title><dc:creator>Picco Web</dc:creator><dc:language>en</dc:language>
<meta property="dcterms:modified">2026-11-05T00:00:00Z</meta>
</metadata>
<manifest><item id="nav" href="nav.xhtml" media-type="application/xhtml+xml" properties="nav"/>{''.join(manifest)}</manifest>
<spine>{''.join(spine)}</spine>
</package>""", compress_type=zipfile.ZIP_DEFLATED)
    print("seed:", path, os.path.getsize(path), "bytes")


if __name__ == "__main__":
    main()
