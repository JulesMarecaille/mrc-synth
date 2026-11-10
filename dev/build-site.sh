#!/bin/sh
# The GitHub Pages site, built into docs/ on main (Pages serves main:/docs): the landing page with the emulator at
# the root, editor/, installer/, firmware/ (the package + its licence texts), fonts/ -- build/emu as served locally.
#   dev/build-site.sh              device build (the package), emulator build, then docs/
#   dev/build-site.sh --no-device  skip the device build: the installer gets the last build/felucca.fwsc
# The device build: WSL's dev/device-build.sh on Windows (no tests: `build` only), ./build.sh elsewhere.
# docs/ also holds upstream's own docs (controls.jpg, the .md notes): only the site's paths below are replaced.
# dev/githooks/pre-commit runs this and stages docs/.
set -e
cd "$(dirname "$0")/.."

DEVICE=1
for a in "$@"; do
    case "$a" in
        --no-device) DEVICE=0 ;;
        *) echo "dev/build-site.sh: unknown argument '$a'"; exit 2 ;;
    esac
done

if [ "$DEVICE" = 1 ]; then
    if command -v wsl.exe >/dev/null 2>&1; then
        MSYS_NO_PATHCONV=1 wsl.exe -d "${FELUCCA_WSL_DISTRO:-Ubuntu-24.04}" -- sh dev/device-build.sh build
    else
        sh ./build.sh
    fi
fi
[ -f build/felucca.fwsc ] || { echo "dev/build-site.sh: no build/felucca.fwsc (the device build)"; exit 1; }
sh dev/build-emu.sh
ls build/emu/firmware/*.fwsc >/dev/null 2>&1 || { echo "dev/build-site.sh: the installer has no package"; exit 1; }

SITE="index.html felucca.wasm worklet.js editor installer firmware fonts .nojekyll"
for p in $SITE; do rm -rf "docs/$p"; done
mkdir -p docs
cp -R build/emu/. docs/
touch docs/.nojekyll                            # served as they are, no Jekyll pass
echo "site: docs/ ($(ls docs/firmware/*.fwsc | sed 's|.*/||'))"
