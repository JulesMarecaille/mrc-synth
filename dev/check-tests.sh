#!/bin/sh
# Syntax-check every host test (tests/*.c) at once, in WSL, after dev/device-build.sh has made build/gen there:
#   wsl -d Ubuntu-24.04 -- sh dev/check-tests.sh [tests/x.c ..]      (default: every one; N=20 errors a file)
# run_tests.sh stops at the first test that does not compile; this lists them all (the errors only, per file).
# RUN=1 also builds and runs each one that compiles, showing its lines that are not "ok" (N of them) and its exit status.
SRC="$(cd "$(dirname "$0")/.." && pwd)"
DST="${FELUCCA_WSL:-$HOME/felucca-wsl}"
rsync -a --delete --exclude /build/ --exclude __pycache__/ "$SRC/" "$DST/"
grep -rlI --exclude-dir=.git "$(printf '\r')" "$DST" | while IFS= read -r f; do sed -i 's/\r$//' "$f"; done
cd "$DST"
for f in ${*:-tests/*.c}; do
    out=$(cc -fsyntax-only -Werror=implicit-function-declaration -std=gnu99 -DHALF_FRAMES=128 -DT_CDC=1 -DT_UAC=1 -DT_LAYOUT=0 -DT_ON=1 -DT_48K=0 -Ibuild/gen -Ifirmware/hal -Ifirmware/bsp/chip -Ifirmware/bsp/boards/fm1 -Ifirmware/src -Itests "$f" 2>&1 | grep -E " error: " | head -"${N:-20}")
    [ -n "$out" ] && printf '== %s\n%s\n' "$f" "$out"
    if [ -z "$out" ] && [ -n "$RUN" ]; then
        cc -O1 -w -std=gnu99 -DHALF_FRAMES=128 -DT_CDC=1 -DT_UAC=1 -DT_LAYOUT=0 -DT_ON=1 -DT_48K=0 -Ibuild/gen -Ifirmware/hal \
           -Ifirmware/bsp/chip -Ifirmware/bsp/boards/fm1 -Ifirmware/src -Itests -o /tmp/check-one "$f" -lm || continue
        (/tmp/check-one > /tmp/check-one.log 2>&1); rc=$?
        printf '== %s: exit %s\n' "$f" "$rc"
        grep -vE ' ok$|passed$' /tmp/check-one.log | head -"${N:-20}"
    fi
done
echo "check-tests: done"
