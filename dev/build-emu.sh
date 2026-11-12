#!/bin/sh
# Build the browser emulator only: generated headers (Python) + WebAssembly (emcc).
# No JieLi toolchain, no AC79 SDK, no Docker -- those are only needed for a .fwsc
# you flash to a real FM-1 (see BUILDING.md).
#   dev/build-emu.sh            incremental: regenerates headers, recompiles the wasm
#   EMU_OPT=-O0 dev/build-emu.sh    faster compile, slower audio
set -e
cd "$(dirname "$0")/.."
PY="${PYTHON:-python}"

"$PY" dev/gen.py

if ! command -v emcc >/dev/null 2>&1; then
    for d in "$EMSDK" ../emsdk "$HOME/emsdk"; do
        [ -n "$d" ] && [ -f "$d/emsdk_env.sh" ] || continue
        # shellcheck disable=SC1091
        . "$d/emsdk_env.sh" >/dev/null 2>&1 || true
        break
    done
fi
command -v emcc >/dev/null 2>&1 || { echo "dev/build-emu.sh: no emcc; set EMSDK to an emsdk checkout"; exit 1; }

sh web/emu/build.sh

# MRC: the page is a landing page that links the editor, so the editor is served next to it
# (build/emu/editor/), and the page's own faces: Inter Tight (the firmware's), Fukiai (its icons)
mkdir -p build/emu/editor build/emu/fonts
cp web/editor.html build/emu/editor/index.html
cp web/fukiai.ttf web/FUKIAI-LICENSE.txt web/fm1backup.js build/emu/editor/
cp web/fukiai.ttf web/FUKIAI-LICENSE.txt build/emu/fonts/
cp "assets/fonts/InterTight[wght].ttf" build/emu/fonts/InterTight.ttf
cp LICENSES/OFL-InterTight.txt build/emu/fonts/OFL-InterTight.txt
# MRC: the phone remote (build/emu/remote/): same origin as the emulator, which it can drive from another tab
mkdir -p build/emu/remote
cp web/remote.html build/emu/remote/index.html
echo "emu: build/emu/editor (the editor), build/emu/remote (the phone remote), build/emu/fonts (Inter Tight, Fukiai)"
"$PY" dev/installer_preview.py
