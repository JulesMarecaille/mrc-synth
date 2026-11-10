#!/usr/bin/env python3
"""The installer, served by the emulator: build/emu/installer/index.html.

web/make_site.py makes the published one. This fills the same two marks (/*LIB*/ with the libraries,
/*META*/ with the package) for http://localhost:8790/installer/. The package is build/felucca.fwsc,
which dev/device-build.sh copies back from WSL after a device build; it is checked as make_site.py
checks it and served at build/emu/firmware/. With no package built yet, META names none and the page
says so (index_pkg.html "nopkg") rather than failing to fetch one.
"""
import json
import re
import shutil
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "web"))
from make_site import product_of, strip_module  # noqa: E402

web, emu = ROOT / "web", ROOT / "build" / "emu"
html = (web / "index_pkg.html").read_text(encoding="utf-8")
lib = "\n".join(strip_module((web / f).read_text(encoding="utf-8")) for f in ("fm1pkg.js", "fm1ota.js", "fm1backup.js"))
src = (ROOT / "firmware" / "src" / "felucca.c").read_text(encoding="utf-8")
version = re.search(r'^#define FELUCCA_VERSION "([^"]*)"', src, re.M).group(1)

pkg, meta, note = ROOT / "build" / "felucca.fwsc", {"version": version, "product": "", "pkg": None}, "no package"
fw = emu / "firmware"
shutil.rmtree(fw, ignore_errors=True)
if pkg.exists():
    raw = pkg.read_bytes()
    product = product_of(raw)
    if not re.fullmatch(r"FM-1_9\d\d", product) or b"FELUCCA-LOADER-1" not in raw:
        sys.exit(f"{pkg}: not a package made by tools/fm1pkg_make.py (identity {product!r}); not served")
    name = f"felucca-{re.sub(r'[^A-Za-z0-9.-]', '-', version)}.fwsc"
    fw.mkdir(parents=True)
    shutil.copy(pkg, fw / name)
    meta.update(product=product, pkg="../firmware/" + name)
    note = f"{name}, {len(raw)} B, {product}"
    # the package holds JieLi SDK files (Apache-2.0): their licence travels next to it, with ours (as make_site.py)
    names = sorted(f.name for f in (ROOT / "LICENSES").glob("*.txt"))
    (fw / "LICENSES").mkdir()
    for n in names:
        shutil.copy(ROOT / "LICENSES" / n, fw / "LICENSES" / n)
    (fw / "LICENSES" / "index.html").write_text(    # the installer links this folder: Pages lists no folders
        '<!doctype html><meta charset="utf-8"><title>MRC Synth licences</title><h1>Licence texts</h1><ul>'
        + "".join(f'<li><a href="{n}">{n}</a></li>' for n in names)
        + '</ul><p><a href="../LICENSING.md">LICENSING.md</a> · <a href="../LICENSE">LICENSE (GPL-3.0)</a></p>\n',
        encoding="utf-8")
    for doc in ("LICENSE", "LICENSING.md"):
        shutil.copy(ROOT / doc, fw / doc)
out = emu / "installer"
out.mkdir(parents=True, exist_ok=True)
# index_pkg.html links the licences for make_site.py's webapp/installer/; here the installer is one level down
html = html.replace('href="../../firmware/', 'href="../firmware/')
(out / "index.html").write_text(html.replace("/*LIB*/", lib).replace("/*META*/", json.dumps(meta)), encoding="utf-8")
print(f"emu: build/emu/installer ({note})")
