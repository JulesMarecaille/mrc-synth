/* SPDX-License-Identifier: GPL-3.0-only */
/* MRC Synth Engine: the brand colours, fixed and outside the palette system.
 *
 * The UI's own colours are palette tokens (gfx.c T_BG, T_TEXT, ...) that follow MENU > PALETTE.
 * These do not: they are the identity, like the crash screen's red and the QR code's black and
 * white, and they look the same whichever palette is saved. Use them only where that is the point
 * (the boot screen); anything that lives inside the running UI takes a token instead, or it stops
 * working in the ten palettes the UI test renders.
 *
 * Values are RGB565 as the panel wants them (gfx.c swaps the bytes on the way out).
 */
#pragma once

#define MRC_TEXT_COLOR     0xE6F9u    /* #E4DCC8, the MRC palette's TEXT (tools/gen_ui_palettes.py) */
#define MRC_BG_COLOR       0x1082u    /* #141414, the MRC palette's BG */
#define MRC_ACCENT_1_COLOR 0xFF54u    /* #FBE8A6 */
#define MRC_ACCENT_2_COLOR 0xF5CBu    /* #F3BA5A */
#define MRC_ACCENT_3_COLOR 0xF4CFu    /* #F79B7A */
#define MRC_ACCENT_4_COLOR 0xEAACu    /* #EB5560 */

/* the four accents in order, for the things there are four of: the knob cards (ui_draw.c draw_column) and
 * the FX sends (ui_graph.c graph_fx) */
#define MRC_ACCENTS 4u
static const uint16_t MRC_ACCENT[MRC_ACCENTS] = {MRC_ACCENT_1_COLOR, MRC_ACCENT_2_COLOR,
                                                 MRC_ACCENT_3_COLOR, MRC_ACCENT_4_COLOR};
