#!/bin/sh
# Serve the built emulator at http://localhost:8790/ (default port; $1 overrides).
set -e
cd "$(dirname "$0")/.."
[ -f build/emu/index.html ] || { echo "dev/serve.sh: no build/emu; run dev/build-emu.sh first"; exit 1; }
exec "${PYTHON:-python}" -m http.server -d build/emu "${1:-8790}"
