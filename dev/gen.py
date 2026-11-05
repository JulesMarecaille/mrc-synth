#!/usr/bin/env python3
"""Run only Felucca's header-generation phase (build/gen).

The full ./build.sh needs the JieLi toolchain, the AC79 SDK and Docker to make
device firmware. The browser emulator needs none of that: only the generated
headers in build/gen plus emcc. This runs that one phase, so the emulator can
be rebuilt on any machine with Python + Pillow.

dev/pyshim is put on PYTHONPATH for the generator subprocesses; see the note in
dev/pyshim/sitecustomize.py.
"""
import os
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SHIM = ROOT / "dev" / "pyshim"


def main():
    os.environ["PYTHONPATH"] = os.pathsep.join(
        [str(SHIM), *([os.environ["PYTHONPATH"]] if os.environ.get("PYTHONPATH") else [])])
    sys.path.insert(0, str(SHIM))
    sys.path.insert(0, str(ROOT / "tools"))
    import sitecustomize  # noqa: F401  (applies the shim in this process too)
    import build as felucca_build
    felucca_build.generate()
    print("gen: build/gen ready")


if __name__ == "__main__":
    main()
