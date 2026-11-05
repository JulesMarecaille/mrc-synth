# FM-1 hardware limits and palette

Everything here is read out of this tree (`firmware/app.ld`, `firmware/src/core.h`,
`tools/gen_ui_palettes.py`, `tools/fm1pkg_make.py`, `tests/*budget*`). When a request does not
fit one of these, say which line it busts and by how much.

## The chip

| | |
| --- | --- |
| SoC | JieLi AC791N (WL82), 32-bit pi32v2 |
| CPU clock | **360 MHz** under M-VAVE's bootloader (320 MHz under m-bootloader 1.2 — 11 % slower) |
| Second core | present, **off by default** (MENU > SYSTEM > 2ND CORE); it may render some parts |
| SRAM | ~432 KiB usable, split by the linker (below) |
| Flash | 1 MB SPI NOR, code executes in place (XIP) |
| Audio | 44.1 kHz, 24-bit stereo out via I2S, mono-summed buses |
| Display | **240 x 240 ST7789-class SPI panel, RGB565** |
| Controls | 27 silicone keys, 14 lit buttons, 7 encoders + MASTER knob |
| MIDI | USB MIDI, BLE MIDI, 3.5 mm TRS MIDI IN (all three at once) |

## Memory budget — the hard walls

From `firmware/app.ld`; `tools/build.py check()` fails the build when one is crossed.

| Region | Size | Holds | Headroom rule |
| --- | --- | --- | --- |
| `XIP` (flash) | **0x8DFBC = 581 564 B (568 KiB)** | all code + constants + generated tables | app image must fit the slot exactly |
| `RAMTEXT` | 0x6000 = 24 KiB | code that runs while the flash is busy | — |
| `RAM` (.data+.bss) | **72 KiB** | what the audio and key-scan ISRs touch | at least **4 KiB** must stay free |
| `POOL` (.pool) | **0x5A000 = 360 KiB** | FX lines, canvases, per-part engine memory, work buffers | at least **8 KiB** must stay free |
| `NOINIT` | 0x3D50 = ~15.7 KiB | survives resets | — |
| Per-part engine memory | **12 888 B x 4 parts** (`ENG_MEM_PART`) | one engine's state per part, reused | a bigger engine grows it x4 |

Practical readings:

- A new wavetable, sample set or icon sheet comes out of the **568 KiB of flash**, which already
  holds ~450 KiB of generated headers (samples alone are 453 KiB of `felucca_samples.h`).
- A new delay line, reverb tank or canvas comes out of the **360 KiB pool**, of which the FX
  already take ~210 KiB (`dly_buf` 128 KiB, reverb combs ~10 KiB, chorus 4 KiB).
- Anything the audio ISR touches per sample belongs in the **72 KiB RAM**, and that is the
  tightest region in the build.
- No dynamic allocation. There is no heap worth the name — everything is static.

## CPU budget

- `tests/cpu_baseline.txt` — instructions per 44.1 kHz sample, per preset. ANALOG presets run
  ~1000–1460; PHASE ~900–1560. The test allows **+25 %** over the baseline.
- `tests/target_budget.txt` — per-function instruction counts in the device disassembly, **+10 %**
  allowed. `analog_render` 585, `grain_render` 1731, `slicer_track` 632, `fm6_render` 372.
- A reference point for "too expensive": the ADPCM engine is built out by default because one of
  its voices costs about **three ANALOG voices** (8 voices on one part = 54 % CPU against 17 %).
- The audio ISR works in half buffers of **128 frames = 2.9 ms**. Anything that cannot finish
  inside a half buffer drops audio. Long work belongs in the main loop.
- **The browser emulator cannot tell you any of this.** It runs on a desktop CPU. Cost is
  checked only by `tests/run_tests.sh`.

## Sound limits

| | |
| --- | --- |
| Voices | **8 total**, shared across all parts — not 8 per part |
| Parts / tracks | **4**, each with its own engine, preset, parameters and pattern |
| Steps | **64** per pattern |
| Engines | **14** built (ANALOG, PHASE, LOFI, SAMPLE, VOICE/formant, TRIO, WHEEL, GRAIN, PHYS, DRUM, NOISE, FM6, SLICE; DIGITAL retired, ADPCM off by default) |
| Per-track FX | DIST + one INSERT, then sends |
| Shared FX buses | **3, mono**: chorus, tempo delay, reverb (ROOM / SPRING / HALL) — one instance each for the whole instrument |
| Delay line | 65 536 samples = **1.49 s** max |
| Mod matrix | **4 slots** (source, destination, amount) |
| User presets | **32 slots** (a sound only: MRC's keep no pattern) |
| Saved patterns | **24** (P01..P24): one track's 64 steps, LEN DIV SWING GATE QUANTIZE, no automation; 4 flash banks of 6 in upstream's 4 PROJECT objects (~600 B a pattern) |
| Projects | **1**, the autosave (no PROJECT slots); FUN10 only |
| Song | **16** sections; at most **16 different patterns** a song (RAM `chain.src`, ~713 B each) |
| Headroom | one voice peaks near -6 dBFS; master output ceiling -6 dBFS |

Voice stealing, when the 8 are used: oldest released voice, then oldest extra UNISON voice, then
oldest held voice of a POLY part that is not its lowest note. A stolen voice fades over one block.

**Things that do not fit**, as a feel for the ceiling: a 9th simultaneous voice; a second reverb
or delay instance; a per-track (rather than shared) bus effect; a stereo bus effect; a delay over
1.49 s; floating-point DSP in the audio path; a sample set that pushes the image past 568 KiB.

## What cannot be checked on this machine

- **The device image size.** `tools/build.py check()` is what fails when the app overruns the
  568 KiB slot, 72 KiB of RAM or the 360 KiB pool, and it only runs in the full `./build.sh`
  (Docker + JieLi toolchain + AC79 SDK), which is not set up here. The emulator's `felucca.wasm`
  size is a rough proxy for whether a change grew or shrank the code, nothing more.
- **CPU cost**, as above: the emulator runs on a desktop CPU.
- **`tests/run_tests.sh`**, including the UI test that renders every screen in all ten palettes
  and fails on clipped or overlapping text — there is no host C compiler here. Layout changes are
  checked by eye in the browser instead, so a change to the header, footer or a dialog is worth
  looking at in a light palette (PAPER) as well as a dark one.

## Colours

The screen is **RGB565** — 65 536 colours, 5 bits red, 6 green, 5 blue. There is **no full
framebuffer**: drawing goes into a small off-screen strip and is blitted by DMA, so anything that
wants to read back what is already on screen does not work.

You do not pick colours freely. A palette is **five colours**, and the firmware derives the rest:

| Token | Derivation | Used for |
| --- | --- | --- |
| `BG` | chosen | background |
| `SURF` | chosen | dialogs, menu rows, mixer strips |
| `TEXT` | chosen | primary text |
| `THEME` | chosen | values, curves, gauges, selection |
| `ACCENT` | chosen | the one active thing: hot knob, cursor, playhead, sounding step |
| `MID` | mix(BG, TEXT, 70 %) | labels, units, secondary text |
| `DIM` | mix(BG, TEXT, 42 %) | inactive, empty steps, disabled |
| `LINE` | mix(BG, TEXT, 18 %) | 1 px dividers, faint gauge track |
| `SEL` | mix(BG, THEME, 80 %) | selection fill, gauge fill |
| `TINT` | mix(BG, THEME, 12 %) | faint area (selected drum lane) |
| `RAISE` | mix(SURF, TEXT, 12 %) | raised area on a surface: step stubs, fader slots, chips |
| `KEY` | mix(BG, TEXT, 78 %) | keycap fill |
| `INK` | = BG | text on a selection or THEME fill |

Outside the palette: `REC` is a fixed red, the QR code fixed black/white, the crash screen fixed red.

### The ten palettes (8-bit RGB, stored as RGB565)

| # | Name | BG | SURF | TEXT | THEME | ACCENT |
| --- | --- | --- | --- | --- | --- | --- |
| 0 | MRC | `#0E0E0E` | `#0E0E0E` | `#CCCCCC` | `#E8E8E8` | `#FFFFFF` |
| 1 | GREEN | `#06120A` | `#10281A` | `#E2F4E6` | `#54D678` | `#FFD65C` |
| 2 | AMBER | `#120E08` | `#2A2012` | `#F6ECD8` | `#FFA828` | `#60D6E6` |
| 3 | ICE | `#081018` | `#142434` | `#E4F0F8` | `#50B8EC` | `#FF8C64` |
| 4 | VIOLET | `#100C1C` | `#241C3C` | `#F0E8FC` | `#B288F6` | `#6EE4A8` |
| 5 | ROSE | `#180A12` | `#341628` | `#FCE8F2` | `#F472B6` | `#FFD66E` |
| 6 | PAPER | `#F4EFE4` | `#E2DACA` | `#221E18` | `#0A5446` | `#AC3808` |
| 7 | HI-CON | `#000000` | `#282828` | `#FFFFFF` | `#FFE800` | `#00E6FF` |
| 8 | NIGHT | `#000000` | `#00180A` | `#96E6AA` | `#38DC64` | `#FFFFFF` |
| 9 | MONO | `#000000` | `#000000` | `#FFFFFF` | `#FFFFFF` | `#FFFFFF` |

Two are special cases: **MRC** is pure greyscale (R = G = B) in every colour and every derived
tint. **MONO** is black and white — its tokens are *set*, not blended, with one mid grey
`#525252` for DIM / LINE / RAISE, and selections invert (white fill, black ink).

Saved settings name palettes by a tagged id (`PAL_TAG 64` + index), so **palettes are
append-only**: add at the end, never reorder or remove.

### Rules a new colour or screen has to pass

- `tools/gen_ui_palettes.py` checks the **WCAG contrast of every pairing in every palette** and
  **fails the build** on a miss. The floors are `text/bg` 12.0, `text/surf` 9.0, `mid/bg` 5.0,
  `dim/bg` 2.2, `line/bg` 1.25, `theme/bg` 4.5, `sel/bg` 1.8, `text/raise` 7.0. The tightest
  palette today is PAPER (`accent/surf` 4.6 against a floor of 4.5) — a change there has almost
  no slack.
- The UI test renders **every screen in every palette** and fails on clipped or overlapping text,
  or on a box touching a divider. Ten palettes means a layout has to work on both a near-black
  and a paper-white background.
- Fonts are three fixed sizes, baked at build time: **S 12 px / 400, M 15 px / 500, L 28 px /
  600** (Inter Tight), plus a 12 / 16 / 24 px icon sheet. No runtime font scaling, no arbitrary
  size, no new glyph without regenerating `build/gen/ui_fonts.h`.
