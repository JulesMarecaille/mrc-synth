#!/bin/sh
# Build the browser emulator and serve it -- the one command for an iteration.
#   dev/run.sh              build, then http://localhost:8790/
#   dev/run.sh 9000         .. on another port
#   dev/run.sh --no-build   serve what is already in build/emu
#   EMU_OPT=-O0 dev/run.sh  faster compile, slower audio (fine for UI work)
set -e
cd "$(dirname "$0")/.."

BUILD=1
PORT=8790
for a in "$@"; do
    case "$a" in
        --no-build) BUILD=0 ;;
        [0-9]*) PORT="$a" ;;
        *) echo "dev/run.sh: unknown argument '$a'"; exit 2 ;;
    esac
done

[ "$BUILD" = 1 ] && sh dev/build-emu.sh

echo
echo "  Felucca emulator:  http://localhost:$PORT/"
echo "  stop with Ctrl-C"
echo
exec "${PYTHON:-python}" -m http.server -d build/emu "$PORT"
