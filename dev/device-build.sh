#!/bin/sh
# The device build (felucca.fwsc) and the host test suite, in WSL. Run inside WSL (Ubuntu, x86-64), from Windows:
#   wsl -d Ubuntu-24.04 -- sh dev/device-build.sh [build|test|all]      (default: all)
# The Windows checkout has CRLF line endings (core.autocrlf), which break every .sh under Linux and the text
# fixtures the tests read, so this mirrors the working tree -- uncommitted changes included -- into
# ~/felucca-wsl with LF endings and works there. The Windows tree is only read. Results:
#   ~/felucca-wsl/build/felucca.fwsc, felucca.bin, loader/ota.bin; the tests' PNGs and reports in build/ui_new.
# Needs (once): Pillow and fontTools for python3, tools/get_toolchain.sh (~/.jieli/toolchain), the AC79 SDK in
# ~/fw-AC79_AIoT_SDK (BUILDING.md), rsync. On Linux x86-64 the toolchain runs natively: no Docker.
set -e
SRC="$(cd "$(dirname "$0")/.." && pwd)"
DST="${FELUCCA_WSL:-$HOME/felucca-wsl}"
what="${1:-all}"

mkdir -p "$DST"
rsync -a --delete --exclude /build/ --exclude __pycache__/ "$SRC/" "$DST/"
# CRLF -> LF in every text file outside .git (binary files -- samples, fonts -- are skipped by grep -I)
grep -rlI --exclude-dir=.git "$(printf '\r')" "$DST" | while IFS= read -r f; do sed -i 's/\r$//' "$f"; done
cd "$DST"

case "$what" in
build|all) sh ./build.sh ;;
esac
case "$what" in
test|all) sh tests/run_tests.sh ;;
esac
