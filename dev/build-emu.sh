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
