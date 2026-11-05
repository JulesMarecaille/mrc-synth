# MRC Synth

[![License: GPL-3.0-only](https://img.shields.io/badge/license-GPL--3.0--only-blue.svg)](LICENSE)

## Overview

MRC Synth is custom synthesizer firmware for the M-VAVE FM-1, version 1.0.0. It is a modified version of
[Felucca](https://github.com/hugelton/Felucca) by Leo Kuroshita ([Hügelton Instruments](https://hugelton.com)):
the sound engines and the sequencer are Felucca's, with MRC's own look (brand colours, boot screen, a drawing per
engine and effect), a simpler project model (one autosaved project, 24 saved patterns) and a reworked page layout.
The full list of changes is in [CHANGES.md](CHANGES.md).

This is not an official Felucca release: please report MRC Synth problems here, not to the Felucca project.

Installing custom firmware on the FM-1 is at your own risk. M-VAVE's own updater takes the device back to its
factory firmware.

## Try it in your browser

The whole firmware runs in a browser emulator, no FM-1 needed. With Python 3 (+ Pillow, fontTools) and
[Emscripten](https://emscripten.org) installed:

```sh
dev/run.sh            # Git Bash, from the repository
```

```
dev\run.cmd           # PowerShell or cmd
```

then open <http://localhost:8790/>. USB, TRS MIDI in and the user sample slots are not emulated.

## Building for the FM-1

A `.fwsc` for the device needs the JieLi toolchain and AC79 SDK; see [BUILDING.md](BUILDING.md).
Clone with `--recurse-submodules`: the engines (`firmware/engines`) and the hardware layer (`firmware/bsp`) are
git submodules.

## Credits and licence

Free software: [GPL-3.0-only](LICENSE), as Felucca. The engines in `firmware/engines` are MIT
([Felucca-Engines](https://github.com/hugelton/Felucca-Engines)); the hardware layer in `firmware/bsp` is MPL-2.0
([felucca-bsp](https://github.com/hugelton/felucca-bsp)); the Hügelton Sample Pack is CC0 1.0; the bundled fonts and
ported DSP keep their own licences ([LICENSES/](LICENSES/), [LICENSING.md](LICENSING.md)). Thanks to Leo Kuroshita
and everyone whose work is in Felucca: [CREDITS.md](CREDITS.md).

Anyone who distributes a build of MRC Synth must also offer its complete source, submodules included.

Developed with the assistance of AI coding agents.

M-VAVE and FM-1 are trademarks of their respective owners. MRC Synth is not affiliated with or endorsed by M-VAVE
or Hügelton Instruments.

MRC Synth modifications Copyright (C) 2026 Jules Marécaille
Felucca Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments

Source: <https://github.com/JulesMarecaille/mrc-synth>
