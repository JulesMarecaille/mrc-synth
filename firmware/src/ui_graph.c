/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Felucca UI: the panel (Y_GRAPH .. Y_GRAPH + H_GRAPH) between the cards and the footer: a SURF area
 * (rounded, on BG) holding the page graphs (ADSR, LFO, steps, piano roll, drum grid, scale, FX, SLICER,
 * MOD, lists: presets, user presets, patterns, project slots, song), the HOME oscilloscope, the
 * instrument diagrams; the MIXER page draws four SURF columns instead. Each graph is redrawn only when
 * graph_signature() changes. The look:
 * curves THEME (2 px), guides and empty marks RAISE, captions MID / DIM, the active thing ACCENT,
 * bars rounded; a list's selected row is a THEME bar with INK text. */
#include "mrc_theme.h"                               /* MRC: the FX sends' brand accents (graph_fx) */
#define PANEL_X0 10                                  /* the graphs' inner area: x 10..230 */
#define PANEL_W 220
#define GOY ((H_GRAPH - 100) / 2)                    /* graphs are drawn on a 100 px scale, centred in the panel */
/* the height ADSR and LFO are drawn on: 100 px, or (MENU > LARGE's strip: draw_graph) the strip's ~ 56 */
static int32_t graph_ht = 100;
#include "ui_fm6op.c"                                /* EDIT > OPERATOR, OP ENV, OPERATOR 2: FM6's operators (their charts below) */

/* Decay and release match voice.c: exponential (env += (target - env) * k each
 * tick, ~99 % after the set time). MRC: the attack is drawn curved, for the look:
 * voice.c's is linear. Time axis is the parameter value (the times themselves
 * are exponential). A dot joins the stages, in the colour of the one it starts. */
static void graph_adsr(const track_t *t, uint16_t c)
{
    const page_t *pg = cur_page();
    /* MRC: the attack's floor is 14 px, not 4, so the shortest attack still reads as a slope on the curve
     * rather than a vertical edge at the left; the span above it is unchanged, so the knob moves it as before */
    int32_t a = 14 + t->p[pg->id[0]] * 50 / 127, d = 6 + t->p[pg->id[1]] * 50 / 127, r = 6 + t->p[pg->id[3]] * 60 / 127;
    int32_t top = 6 * graph_ht / 100, bot = 88 * graph_ht / 100, sus = t->p[pg->id[2]] * 1000 / 127;   /* 0..1000 */
    int32_t x0 = 12, x1 = x0 + a, x3 = 226 - r, i, px, py;
    int32_t e = 32768;                                                  /* exp(-4.6 u), Q15 */
    /* MRC: each stage is drawn in the accent of the card that sets it -- the four cards of an ADSR page are
     * ATK, DEC, SUS, REL in that order (pg->id[0..3]), on the ENV pages and on FM6's operator envelopes
     * alike, so the curve says which knob moves which part of it. One colour in every other palette. */
    uint16_t sc[4];
    int32_t dx[5], dy[5];                                               /* the joints, in 1/16 px */
    uint32_t k;
    for (k = 0; k < 4u; k++)
        sc[k] = MRC_ON ? MRC_ACCENT[k % MRC_ACCENTS] : c;
#define EGY(lvl) (LN(bot) - (lvl) * LN(bot - top) / 1000)
    cv_rect(PANEL_X0, bot + 2, PANEL_W, 1, T_RAISE);
    px = x0;
    py = LN(bot);
    dx[0] = LN(x0);
    dy[0] = py;
    for (i = 1; i <= a; i++) {                                          /* attack: 1 - (1 - u)^2, up to the top */
        int32_t y = EGY(1000 - 1000 * (a - i) * (a - i) / (a * a));
        cv_stroke(LN(px), py, LN(x0 + i), y, sc[0]);
        px = x0 + i;
        py = y;
    }
    dx[1] = LN(px);
    dy[1] = py;
    for (i = 1; i <= d; i++) {                                          /* decay: exponential to SUS */
        int32_t lvl;
        e = (e * (32768 - 150733 / d)) >> 15;           /* k^d = exp(-4.6) */
        lvl = sus + ((1000 - sus) * e >> 15);
        cv_stroke(LN(px), py, LN(x1 + i), EGY(lvl), sc[1]);
        px = x1 + i;
        py = EGY(lvl);
    }
    dx[2] = LN(px);
    dy[2] = py;
    cv_stroke(LN(px), py, LN(x3), EGY(sus), sc[2]);                      /* sustain */
    px = x3;
    py = EGY(sus);
    dx[3] = LN(px);
    dy[3] = py;
    e = 32768;
    for (i = 1; i <= r; i++) {                                          /* release: exponential to 0 */
        e = (e * (32768 - 150733 / r)) >> 15;
        cv_stroke(LN(px), py, LN(x3 + i), EGY(sus * e >> 15), sc[3]);
        px = x3 + i;
        py = EGY(sus * e >> 15);
    }
    dx[4] = LN(px);
    dy[4] = py;
    for (k = 0; k < 5u; k++)                                            /* last: on top of the strokes */
        cv_dot(dx[k], dy[k], 40, sc[k < 3u ? k : 3u]);
#undef EGY
}

static void graph_lfo(const track_t *t, uint16_t c)
{
    int32_t x, cy = graph_ht / 2, a = 38 * graph_ht / 100, py = cy, uni = t->p[P_LPOL] != 0;
    uint32_t ph = (uint32_t)t->p[P_LPHASE] << 25;
    int32_t z = uni ? cy + a : cy;                   /* the zero line: POL UNI (LFO 2) draws 0..+1 over it */
    cv_rect(PANEL_X0, z, PANEL_W, 1, T_RAISE);
    for (x = 0; x < PANEL_W; x++) {                  /* two cycles (lfo_wave only reads the track) */
        int32_t w = lfo_wave((track_t *)t, ph + (uint32_t)x * (0xFFFFFFFFu / (PANEL_W / 2u))), y;
        if (t->p[P_LWAVE] == 4)
            w = (int32_t)((x / 20 * 2654435761u) >> 16) - 32768;
        y = uni ? LN(z) - (w + 32768) * LN(a) / 32768 : LN(cy) - w * LN(a) / 32768;
        if (x)
            cv_stroke(LN(PANEL_X0 + x - 1), py, LN(PANEL_X0 + x), y, c);
        py = y;
    }
}

/* STEP page (a melodic track): a piano roll of the cursor's 16-step page. PR_ROWS semitone rows of PR_RH px,
 * the highest on top; on the left the C rows named (S, DIM) and a keyboard strip (white keys RAISE, black keys a
 * shorter DIM; a key held on the track: its row ACCENT). Rows in the track's scale (SCL ROOT / SCALE; CHR: the white
 * keys) are tinted LANE, a C row closes with a RAISE line; step lines GRID, every 4th RAISE. A note is a 9 x 3 bar in
 * its column (an accent: TEXT, the row's full height; the cursor step's notes: ACCENT), chords stacked, lane hits as
 * their notes (a RATCH step's bars in its 2..4 parts, 1 px apart); a TIE carries the bars of the note before through
 * its column; a REST is empty; a slide is a TEXT diagonal from the end of its bar into the next note (not from a
 * RATCH step: it never slides out). Notes outside
 * the view: a 1 px mark on the edge. The cursor: a TEXT column frame; the playhead: a 1 px ACCENT line. The view
 * (proll.lo, the bottom row) fits the notes of the page, else centres on the cursor's note, and follows in steps (a
 * third of the way per frame); graph_signature() holds it, so a page that does not change is not drawn again. */
#define PR_ROWS 25                                  /* semitones shown (MRC: 25, the 140 px panel full; was 21) */
#define PR_RH 5                                     /* px per row */
#define PR_Y0 9                                     /* the top row (canvas y) */
#define PR_X0 35                                    /* the first step column */
#define PR_CW 12                                    /* px per step */
#define PR_KX 24                                    /* the keyboard strip, 9 px */
static struct {
    uint8_t lo, trk, init, moving;                    /* lo: the note of the bottom row; moving: on its way */
    uint32_t frame;
} proll;
static const uint8_t KEY_BLACK[12] = {0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0};

static int32_t pr_note(const step_t *st, uint32_t j)   /* the step's note j (0..3), then its lane hits (-1: none) */
{
    if (j < 4u)
        return j < st->n ? st->note[j] : -1;
    return (st->hit >> (j - 4u)) & 1u ? DRUM_LANE_NOTE[j - 4u] : -1;
}
static uint32_t pr_src(const track_t *t, uint32_t si, uint32_t len)   /* the step whose notes sound at si (NSTEP: none) */
{
    uint32_t k;
    for (k = 0; k < len && seq_steps(t)[si].time == ST_TIE; k++)
        si = (si + len - 1u) % len;
    return step_on(&seq_steps(t)[si]) ? si : NSTEP;
}
/* the rows lit by keys held on the selected track (bit r: row r from the top) */
static uint32_t pr_held(void)
{
    uint32_t k, i, m = 0;
    for (k = 0; k < 27u; k++)
        if (kb_chn[k] && kb_trk[k] == song.sel)
            for (i = 0; i < kb_chn[k]; i++) {
                int32_t r = (int32_t)proll.lo + PR_ROWS - 1 - kb_chord[k][i];
                if (r >= 0 && r < PR_ROWS)
                    m |= 1u << r;
            }
    return m;
}
/* once a frame: the view toward the page's notes (centred; once there it stays while they fit); at once on a redraw
 * forced, a gap in the frames, another track or MENU > ANIM OFF */
static void pr_follow(const track_t *t)
{
    uint32_t i, j, len = (uint32_t)t->p[P_SLEN], base = ui.bank * 16u;
    int32_t lo = 127, hi = -1, tgt, d, cur = proll.lo;
    int snap = ui.force || !proll.init || proll.trk != song.sel || ui.frame != proll.frame + 1u || (ui_prefs & PREF_ANIM_OFF);
    if (proll.init && proll.frame == ui.frame)
        return;
    for (i = 0; i < 16u && base + i < len; i++) {
        uint32_t s = pr_src(t, base + i, len);
        for (j = 0; s < NSTEP && j < 4u + NLANE; j++) {
            int32_t n = pr_note(&seq_steps(t)[s], j);
            if (n < 0)
                continue;
            lo = n < lo ? n : lo;
            hi = n > hi ? n : hi;
        }
    }
    if (hi < 0)
        lo = hi = last_note;
    if (hi - lo <= PR_ROWS - 1)                     /* all fit: centred, then kept while they fit */
        tgt = !snap && !proll.moving && lo >= cur && hi <= cur + PR_ROWS - 1 ? cur : (lo + hi + 1) / 2 - PR_ROWS / 2;
    else {                                            /* too wide: the cursor's (lowest) note in the middle */
        const step_t *st = &seq_steps(t)[ui.cursor];
        tgt = (st->n ? st->note[0] : (lo + hi) / 2) - PR_ROWS / 2;
    }
    tgt = clamp(tgt, 0, 128 - PR_ROWS);
    d = tgt - cur;
    proll.lo = (uint8_t)(snap ? tgt : cur + (d / 3 ? d / 3 : d > 0 ? 1 : d < 0 ? -1 : 0));
    proll.moving = proll.lo != tgt;
    proll.trk = (uint8_t)song.sel;
    proll.frame = ui.frame;
    proll.init = 1;
}
static int32_t pr_row_y(int32_t n) { return PR_Y0 + ((int32_t)proll.lo + PR_ROWS - 1 - n) * PR_RH; }
/* a bar w px wide in `hits` equal parts, 1 px apart (a RATCH step; 1: the bar) */
static void pr_split(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c, uint32_t hits)
{
    int32_t k, pw = (w - (int32_t)hits + 1) / (int32_t)hits;
    if (hits < 2u) {
        cv_rect(x, y, w, h, c);
        return;
    }
    for (k = 0; k < (int32_t)hits; k++)
        cv_rect(x + k * (pw + 1), y, pw, h, c);
}
/* the bars of step s in column x (in its RATCH parts, hits; a TIE's carried bars: 1): a 1 px edge mark for a note
 * out of view; first: row y of the step's first note */
static void pr_bars(const step_t *st, int32_t x, int32_t w, uint16_t c, int acc_all, int32_t *first, uint32_t hits)
{
    uint32_t j;
    int32_t ybot = PR_Y0 + PR_ROWS * PR_RH;
    for (j = 0; j < 4u + NLANE; j++) {
        int32_t n = pr_note(st, j), y;
        int acc;
        if (n < 0)
            continue;
        acc = acc_all || (j >= 4u && ((st->acc >> (j - 4u)) & 1u));
        y = pr_row_y(n);
        if (*first < -1000)
            *first = y;
        if (y < PR_Y0)
            pr_split(x, PR_Y0, w, 1, c, hits);
        else if (y >= ybot)
            pr_split(x, ybot - 1, w, 1, c, hits);
        else if (acc)
            pr_split(x, y, w, PR_RH, c == T_ACCENT ? c : T_TEXT, hits);
        else
            pr_split(x, y + 1, w, PR_RH - 2, c, hits);
    }
}
static void graph_roll(const track_t *t, uint16_t c)
{
    uint32_t i, len = (uint32_t)t->p[P_SLEN], base = ui.bank * 16u, ncol, mask = scale_mask(t), held = pr_held();
    uint64_t locks = motion_lock_steps(trk_index(t));
    int32_t r, ybot = PR_Y0 + PR_ROWS * PR_RH, gw;
    char nb[8];
    ncol = base < len ? (len - base < 16u ? len - base : 16u) : 0u;
    gw = (int32_t)ncol * PR_CW + 1;
    for (r = 0; r < PR_ROWS; r++) {                 /* rows: the lane, the key, the C names */
        int32_t n = (int32_t)proll.lo + PR_ROWS - 1 - r, y = PR_Y0 + r * PR_RH;
        uint32_t pc = (uint32_t)n % 12u;
        int in = mask == 0xFFFu ? !KEY_BLACK[pc] : (int)((mask >> ((uint32_t)(n - t->p[P_ROOT] + 120) % 12u)) & 1u);
        if (in)
            cv_rect(PR_X0, y, gw, PR_RH, T_LANE);
        if ((held >> r) & 1u)
            cv_rect(PR_KX, y, 9, PR_RH - 1, T_ACCENT);
        else
            cv_rect(PR_KX, y, KEY_BLACK[pc] ? 5 : 9, PR_RH - 1, KEY_BLACK[pc] ? T_DIM : T_RAISE);
        if (pc == 0u) {
            cv_rect(PR_X0, y + PR_RH - 1, gw, 1, T_RAISE);
            note_name(nb, (uint32_t)n);
            GFX_HOOK_ALIGN(0, y, 23, y + PR_RH, AL_V, "piano roll C label on its row");
            GFX_HOOK_ALIGN(0, y, 23, y + PR_RH, AL_R | AL_PASS, "piano roll C labels' right edge");
            {   /* its ink ends at x 23, a column before the keys (by the advance, C1's 1 ended 2 px short) */
                int32_t b[4];
                text_ink(&AF_S, nb, b);
                cv_text_on(23 - b[2], y - 5, &AF_S, nb, T_DIM, T_SURF);   /* (the widest, "C-1", inside the panel) */
            }
        }
    }
    for (i = 0; i <= ncol; i++)                       /* step lines, the beats brighter */
        cv_rect(PR_X0 + (int32_t)i * PR_CW, PR_Y0, 1, ybot - PR_Y0, i % 4u ? T_GRID : T_RAISE);
    for (i = 0; i < ncol; i++) {
        uint32_t si = base + i;
        int32_t x = PR_X0 + (int32_t)i * PR_CW;
        if (si == ui.cursor)
            cv_frame(x, PR_Y0 - 2, PR_CW + 1, ybot - PR_Y0 + 4, T_TEXT);
        if (song.playing && si == t->seq_idx)
            cv_rect(x + 6, PR_Y0, 1, ybot - PR_Y0, T_ACCENT);
        if ((locks >> si) & 1u)                       /* a parameter lock: a mark under the step */
            cv_rect(x + 3, ybot + 3, 7, 2, T_ACCENT);
        if (step_detailed(&seq_steps(t)[si]))         /* 1.5.1: CHANCE / RATCH / NUDGE set: a mark over it (DETAIL) */
            cv_rect(x + 4, 2, 5, 3, c);
    }
    for (i = 0; i < ncol; i++) {                      /* the notes */
        uint32_t si = base + i, s = pr_src(t, si, len), nx = (si + 1u) % len;
        const step_t *st, *ns = &seq_steps(t)[nx];
        int32_t x = PR_X0 + (int32_t)i * PR_CW, y = -10000;
        uint16_t col;
        if (s == NSTEP)
            continue;                                 /* a REST, empty, or a TIE after one */
        st = &seq_steps(t)[s];
        col = s == ui.cursor || si == ui.cursor ? T_ACCENT : c;
        if (s == si)
            pr_bars(st, x + 2, 9, col, (st->flags & SF_ACCENT) != 0u, &y, step_ratchet(st));
        else                                          /* a TIE: the bars on through the gap before */
            pr_bars(st, x - 1, 12, col, (st->flags & SF_ACCENT) != 0u, &y, 1u);
        if ((st->flags & SF_SLIDE) && step_ratchet(st) == 1u &&   /* (a ratcheted step never slides out) */
            ns->time == ST_NOTE && ns->n && y >= PR_Y0 && y < ybot) {
            int32_t ny = clamp(pr_row_y(ns->note[0]), PR_Y0, ybot - PR_RH);
            cv_line(x + 10, y + 2, x + 14, ny + 2, T_TEXT);   /* the slide into the next note */
        }
    }
}
/* SEQ > STEP on a DRUM track: the grid, 8 lanes x the 16 steps of the page shown. Lanes by their two-letter
 * names (BD SD CP CH OH TM RS CB; CG CL CY on the other kits). A hit is a rounded square (accented: the
 * accent; a RATCH step's: its 2..4 parts as bars), an empty step a dot (brighter on the beats and on the selected lane); the selected lane is
 * underlaid RAISE, the cursor framed in TEXT; a TEXT bar over the step playing. 1.4.1: a muted lane (the EDIT layer's
 * black keys, eng_drum.c drum_mute): its name DIM, the MUTE icon after it, its hits DIM */
#define GD_RH 16                                    /* px a lane (MRC: 8 of them fill the 140 px panel; was 14) */
#define GD_HY ((GD_RH - 10) / 2)                    /* the 10 px hit centred in the lane */
static void graph_grid(const track_t *t, uint16_t c)
{
    uint32_t l, i, len = (uint32_t)t->p[P_SLEN], base = ui.bank * 16u, mute = drum_mute[trk_index(t) % NPART];
    uint64_t locks = motion_lock_steps(trk_index(t));
    int32_t y0 = 6;
    for (i = 0; i < 16u && base + i < len; i++) {   /* a parameter lock: a mark over the step; CHANCE / RATCH / */
        if ((locks >> (base + i)) & 1u)               /* NUDGE set (1.5.1, DETAIL): a dot under it */
            cv_rect(41 + (int32_t)i * 12, 1, 8, 2, T_ACCENT);
        if (step_detailed(&seq_steps(t)[base + i]))
            cv_rect(43 + (int32_t)i * 12, 3, 4, 2, c);
    }
    for (l = 0; l < NLANE; l++) {
        int32_t y = y0 + 4 + (int32_t)l * GD_RH;
        int sel = l == ui.lane, mu = (mute >> l) & 1u;
        uint16_t row = sel ? T_RAISE : T_SURF, hc = mu ? T_DIM : c, ac = mu ? T_DIM : T_ACCENT;
        if (sel) {
            GFX_HOOK_ALIGN(0, 0, 240, 0, AL_H | AL_CELLS, "drum lane fill centred");
            cv_rrect(6, y, 228, GD_RH, 3, T_RAISE, T_SURF);   /* (6 .. 234, as a list row; the hits 40 .. 230) */
        }
        GFX_HOOK_ALIGN(0, y + GD_HY, 0, y + GD_HY + 10, AL_V, "drum lane label on its hits' line");
        cv_text_on(10, y + GD_HY - 3, &AF_S, drum_lane_abbr(t, l), mu ? T_DIM : sel ? T_ACCENT : T_MID, row);   /* ink rows GD_HY .. +9, as the hits */
        if (mu) {                                    /* (27 .. 39: between the name and the first hit) */
            GFX_HOOK_ALIGN(0, y, 0, y + GD_RH, AL_V, "drum lane mute icon centred up/down");
            cv_icon_in(27, y, 0, GD_RH, 12, ICON_MUTE, T_DIM, row);
        }
        for (i = 0; i < 16u && base + i < len; i++) {
            const step_t *st = &seq_steps(t)[base + i];
            uint32_t b = 1u << l;
            int32_t x = 40 + (int32_t)i * 12;
            uint16_t bc = hc, dc = i % 4u == 0u || sel ? T_MID : T_DIM;
            if (MRC_ON) {                             /* MRC: each block of 4 steps in one brand accent, 1 .. 4 */
                uint16_t k = MRC_ACCENT[(i / 4u) % MRC_ACCENTS];
                bc = mu ? T_DIM : k;
                dc = ux_mix(T_SURF, k, mu ? 15 : i % 4u == 0u || sel ? 60 : 30);
            }
            if ((step_lanes(st) & b) && step_ratchet(st) > 1u)
                pr_split(x, y + GD_HY, 10, 10, (step_accents(st) & b) ? ac : bc, step_ratchet(st));
            else if (step_lanes(st) & b)
                cv_rrect(x, y + GD_HY, 10, 10, 2, (step_accents(st) & b) ? ac : bc, row);
            else
                cv_rect(x + 4, y + GD_HY + 4, 2, 2, dc);
            if (sel && base + i == ui.cursor)          /* the cursor: a 1 px frame */
                cv_frame(x - 1, y + GD_HY - 1, 12, 12, T_TEXT);
        }
    }
    if (song.playing && t->seq_idx < len && t->seq_idx / 16u == ui.bank)
        cv_rect(40 + (int32_t)(t->seq_idx % 16u) * 12, y0 - 1, 10, 3, T_TEXT);
}
/* SEQ > DETAIL (1.5.1, Discussion #199): the cursor's 16 steps as four lanes, the knobs' order top to bottom, each
 * named on the left (the lane of the knob just turned in the accent): CHANCE a bar as high as the chance, RATCH the
 * step's bar in its 2..4 parts (as the roll draws a ratchet), NUDGE a tick left / right of the step's middle, VEL a bar
 * as high as the velocity (an ACC step's full, TEXT). Off its default a value is bright (THEME, the cursor's ACCENT),
 * at it faint (RAISE: 100 %, x1, 0, a VEL never set); DIM where it does not play (a NUDGE with QUANTIZE ON or on a
 * ratchet, a VEL under ACC). A step that plays nothing: a dot (a value left on it DIM); past LEN nothing. The cursor
 * framed through the lanes, the step playing a line */
#define SD_X0 36                                    /* the first step column (12 px each) */
static void graph_detail(const track_t *t, uint16_t c)
{
    static const char *const NAME[4] = {"CHN", "RAT", "NDG", "VEL"};
    static const uint8_t LY[4] = {4, 44, 72, 100}, LH[4] = {34, 22, 22, 34};   /* MRC: the panel's 140 px full */
    uint32_t i, l, len = (uint32_t)t->p[P_SLEN], base = ui.bank * 16u, ncol;
    ncol = base < len ? (len - base < 16u ? len - base : 16u) : 0u;
    for (l = 0; l < 4u; l++) {
        int32_t yb = LY[l] + LH[l];
        cv_rect(SD_X0, yb, (int32_t)ncol * 12 + 1, 1, T_GRID);   /* the lane's floor */
        cv_text_on(8, LY[l] + LH[l] / 2 - 8, &AF_S, NAME[l], ui.hot_t && ui.hot_col == l ? T_ACCENT : T_MID, T_SURF);
    }
    for (i = 0; i < ncol; i++) {
        uint32_t si = base + i;
        const step_t *st = &seq_steps(t)[si];
        int32_t x = SD_X0 + (int32_t)i * 12;
        uint16_t hi = si == ui.cursor ? T_ACCENT : c, lo = T_RAISE, nd = T_DIM;
        if (MRC_ON) {                                 /* MRC: each block of 4 steps in one brand accent, 1 .. 4 (as the drum grid) */
            hi = MRC_ACCENT[(i / 4u) % MRC_ACCENTS];
            lo = ux_mix(T_SURF, hi, 30);
            nd = ux_mix(T_SURF, hi, 20);
        }
        if (si == ui.cursor)
            cv_frame(x, 1, 13, 135, T_TEXT);
        if (song.playing && si == t->seq_idx)
            cv_rect(x + 6, 2, 1, 133, T_ACCENT);
        for (l = 0; l < 4u; l++) {
            uint32_t k = SD_KIND[l];
            int32_t v = ev_sval(st, k), y = LY[l], h = LH[l], set = v != ev_sdefv(k), bh;
            uint16_t col;
            if (!sd_takes(st, k) && !set) {           /* nothing plays there */
                cv_rect(x + 5, y + h - 3, 2, 2, nd);
                continue;
            }
            col = !sd_takes(st, k) ? T_DIM : set ? hi : lo;
            if (k == EVK_CHANCE && v > 100) {         /* 1.5.5: a condition: a:b as b cells up the lane, the a-th lit; */
                int32_t c = v - 100, j, nb = c < 10 ? COND_B[c - 1] : 0;   /* FILL a frame, !FILL a frame crossed */
                for (j = 0; j < nb; j++)
                    cv_rect(x + 2, y + h - 6 - 7 * j, 9, 5, j == COND_A[c - 1] - 1 ? col : T_GRID);
                if (!nb)
                    cv_frame(x + 2, y + 2, 9, h - 2, col);
                if (c == (int32_t)COND_NFILL)
                    cv_rect(x + 2, y + h / 2, 9, 1, col);
            } else if (k == EVK_CHANCE) {
                bh = v * h / 100;
                cv_rect(x + 2, y + h - (bh < 2 ? 2 : bh), 9, bh < 2 ? 2 : bh, col);
            } else if (k == EVK_RATCH) {
                pr_split(x + 2, y + 5, 9, 8, col, (uint32_t)v);
            } else if (k == EVK_NUDGE) {
                if (set && (t->p[P_SQNT] || !ev_nudge_plays(t, st)))
                    col = T_DIM;
                cv_rect(x + 6, y + 2, 1, h - 4, T_GRID);
                if (!set)
                    cv_rect(x + 5, y + h / 2 - 1, 3, 3, col);
                else
                    cv_rect(x + 5 + v * 5 / 8, y + 3, 3, h - 6, col);
            } else if (st->flags & SF_ACCENT) {       /* VEL: an ACC step plays 127 */
                cv_rect(x + 2, y, 9, h, sd_takes(st, k) ? T_TEXT : T_DIM);
            } else {
                bh = v * h / 127;
                cv_rect(x + 2, y + h - (bh < 2 ? 2 : bh), 9, bh < 2 ? 2 : bh, col);
            }
        }
    }
}
/* MRC: SCL and CHORD draw their keys as the FM-1's own keybed, not as one row of equal bars. The device has
 * two rows of identical pills (web/emu/index.html): the white keys evenly spaced along the bottom row, the
 * black keys in the row above, each centred on the seam between the two whites it sits between, and a groove
 * down the middle of every key. Here the pill carries the state colour and the groove is cut in SURF.
 * KB_WHITE[pc] is the white key a pitch class is, or the one a black key follows, inside its octave. */
static const uint8_t KB_WHITE[12] = {0, 0, 1, 1, 2, 3, 3, 4, 4, 5, 5, 6};
#define KB_KW(pitch) ((pitch) * 78 / 100)            /* the device's pill: 0.78 of the key pitch across, */
#define KB_KH(h) ((h) * 425 / 1000)                  /* 0.425 of the keybed down, the two rows at its quarters */
/* note i semitones above the C at x0: the centre of its key, whites `pitch` apart */
static int32_t kb_x(int32_t x0, int32_t pitch, uint32_t i)
{
    int32_t w = (int32_t)(i / 12u) * 7 + (int32_t)KB_WHITE[i % 12u];
    return KEY_BLACK[i % 12u] ? x0 + (w + 1) * pitch : x0 + w * pitch + pitch / 2;
}
/* one key: a kw x kh pill on cx, cy with the groove that lights on the device */
static void kb_key(int32_t cx, int32_t cy, int32_t kw, int32_t kh, uint16_t col, uint16_t bg)
{
    int32_t x = cx - kw / 2, y = cy - kh / 2, sw = kw * 14 / 100 < 3 ? 3 : kw * 14 / 100, in = kw * 2 / 5;
    cv_rrect(x, y, kw, kh, kw / 2, col, bg);
    if (kh - 2 * in >= 4)                            /* (a three-octave chord: the keys are too short for it) */
        cv_rrect(cx - sw / 2, y + in, sw, kh - 2 * in, sw / 2, bg, col);
}
/* SCL: one octave of the keybed, the 12 keys from C: in the scale THEME, the root the accent, out of the
 * scale RAISE */
#define SK_Y0 2
#define SK_H 136
static void graph_scale(const track_t *t, uint16_t c)
{
    uint32_t i, mask = scale_mask(t);
    int32_t pitch = 214 / 7, x0 = 13 + (214 - 214 / 7 * 7) / 2;   /* 7 white keys over x 13 .. 227 */
    int32_t kw = KB_KW(pitch), kh = KB_KH(SK_H);
    for (i = 0; i < 12u; i++) {
        uint32_t deg = (i + 12u - (uint32_t)t->p[P_ROOT]) % 12u;
        uint16_t col = (mask >> deg) & 1u ? (deg == 0u ? T_ACCENT : c) : T_RAISE;
        kb_key(kb_x(x0, pitch, i), SK_Y0 + (KEY_BLACK[i] ? SK_H / 4 : SK_H * 3 / 4), kw, kh, col, T_SURF);
    }
}
/* CHORD (SCL 2): the chord's name ("Cm7": the last one the track played; before any, the one on its ROOT from
 * C4) over the keys from the C below its lowest note (two octaves, three for a wide one): its notes THEME, its
 * root's ACCENT, the others RAISE. OFF or a kit: what to do instead */
static void panel_note(const char *a, const char *b, const char *c);
static void panel_alert(uint32_t id, const char *a);
#define CK_Y0 32                                     /* the keybed starts under the chord's name */
#define CK_H 104
#define CK_KH (CK_H * 36 / 100)                      /* its keys a little shorter than the keybed's quarters */
#define CK_BLK (CK_H * 29 / 100)                     /* the two rows a little closer than the quarters: centres */
#define CK_WHT (CK_H * 71 / 100)
static void graph_chord(const track_t *t, uint16_t c)
{
    uint8_t nn[CHORD_MAX];
    uint32_t n, i, j, k = trk_index(t), lo, cnt;
    int32_t r;
    uint16_t mask;
    char b[12];
    const char *cap = "LAST";
    if (!t->p[P_CHRD]) {
        panel_note("CHORD KEYS OFF", "[K1] DIA3: IN-KEY CHORDS", 0);   /* (the title says what they are) */
        return;
    }
    if (chord_kit(t)) {
        panel_note("NO CHORDS ON KITS", 0, 0);
        return;
    }
    if (chord_last[k].n) {
        r = chord_last[k].root;
        mask = chord_last[k].mask;
        n = chord_last[k].n;
        for (i = 0; i < n; i++)
            nn[i] = chord_last[k].note[i];
    } else {
        n = chord_make(t, (uint32_t)(60 + t->p[P_ROOT]), nn, &r, &mask);
        cap = "ON ROOT";
    }
    if (trk_vmode(t) != V_POLY)
        cap = "ROOT ONLY";                              /* MONO / LEGATO / UNISON */
    chord_name(b, (uint32_t)r, mask);
    cv_text_on(14, 8, &AF_M, b, T_TEXT, T_SURF);
    GFX_HOOK_ALIGN(0, 0, 0, 8 + AF_M.asc, AL_B, "chord caption on the name's baseline");
    cv_text_r(226, 8 + AF_M.asc - AF_S.asc, &AF_S, cap, T_MID, T_SURF);   /* on the name's baseline */
    lo = nn[0] - nn[0] % 12u;
    cnt = nn[n - 1u] - lo < 24u ? 24u : 36u;
    {   /* the keybed under the name: two or three octaves, the whites over x 13 .. 227 */
        int32_t nw = (int32_t)(cnt / 12u) * 7, pitch = 214 / nw, x0 = 13 + (214 - 214 / nw * nw) / 2;
        int32_t kw = KB_KW(pitch), kh = CK_KH;
        for (i = 0; i < cnt; i++) {
            uint32_t note = lo + i, on = 0;
            for (j = 0; j < n; j++)
                on |= nn[j] == note;
            kb_key(kb_x(x0, pitch, i), CK_Y0 + (KEY_BLACK[note % 12u] ? CK_BLK : CK_WHT), kw, kh,
                   !on ? T_RAISE : note % 12u == (uint32_t)r % 12u ? T_ACCENT : c, T_SURF);
        }
    }
}
/* MRC: the four sends as isometric wireframe boxes, one brand accent each (mrc_theme.h) in the first palette
 * (MRC_ON); in every other palette they take that palette's colour, all four alike, as upstream's faders did.
 * A box keeps its footprint (FXB_HW wide, FXB_HD deep) and grows in height with the value, from a flat
 * diamond at 0 to FXB_HMAX at 127. The box is drawn as edges only (cv_stroke), the hidden back edges of the base dimmed.
 * Drawn only on a redraw (the panel is redrawn when graph_signature() changes). */
#define FXB_HW 14                                    /* half the footprint's width (the diamond's side points) */
#define FXB_HD 7                                     /* half its depth (the diamond's front and back points) */
#define FXB_Y 8                                      /* the bars run the panel's height, drawn at cv_oy 0 */
#define FXB_H (H_GRAPH - 2 * FXB_Y)
#define FXB_BY (FXB_Y + FXB_H - 9)                   /* the footprint's centre row */
#define FXB_HMAX (FXB_H - 18)                        /* the tallest box: its top back point sits 2 px inside */
static void fx_edge(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t c)   /* whole px in, cv_stroke's 1/16 px out */
{
    cv_stroke(LN(x0), LN(y0), LN(x1), LN(y1), c);
}
static void fx_box(int32_t cx, int32_t h, uint16_t col, uint16_t hid)
{
    int32_t by = FXB_BY, ty = by - h;
    if (h > 0) {
        fx_edge(cx - FXB_HW, by, cx, by - FXB_HD, hid);         /* the hidden back edges of the base, dimmed */
        fx_edge(cx, by - FXB_HD, cx + FXB_HW, by, hid);
    }
    fx_edge(cx - FXB_HW, by, cx, by + FXB_HD, col);           /* the front edges */
    fx_edge(cx, by + FXB_HD, cx + FXB_HW, by, col);
    fx_edge(cx - FXB_HW, by, cx - FXB_HW, ty, col);           /* the verticals */
    fx_edge(cx, by + FXB_HD, cx, ty + FXB_HD, col);
    fx_edge(cx + FXB_HW, by, cx + FXB_HW, ty, col);
    fx_edge(cx - FXB_HW, ty, cx, ty - FXB_HD, col);           /* the top face */
    fx_edge(cx, ty - FXB_HD, cx + FXB_HW, ty, col);
    fx_edge(cx + FXB_HW, ty, cx, ty + FXB_HD, col);
    fx_edge(cx, ty + FXB_HD, cx - FXB_HW, ty, col);
}
static void graph_fx(const track_t *t, uint16_t c)
{
    uint32_t i;
                                                     /* (MRC_ON: the sends carry their own colours; else the
                                                      * palette's, as upstream drew them) */
    for (i = 0; i < 4u; i++) {
        int32_t cx = CARD_X((int32_t)i) + CARD_W / 2, h = clamp(t->p[P_DIST + i], 0, 127) * FXB_HMAX / 127;
        uint16_t col = MRC_ON ? MRC_ACCENT[i % MRC_ACCENTS] : c;
        fx_box(cx, h, col, ux_mix(T_SURF, col, 30));
    }
}
/* MRC: a circle, centre and radius in 1/16 px */
#define RING_SEG 64
static void ln_ring(int32_t cx, int32_t cy, int32_t r, uint16_t c)
{
    uint32_t i;
    int32_t px = 0, py = 0;
    for (i = 0; i <= RING_SEG; i++) {
        uint32_t a = i << 26;                        /* (RING_SEG 64: the turn in 64ths, the last meeting the first) */
        int32_t x = cx + ((r * osc_sine(a + 0x40000000u)) >> 15), y = cy - ((r * osc_sine(a)) >> 15);
        if (i)
            cv_stroke(px, py, x, y, c);
        px = x;
        py = y;
    }
}
/* the moving drawings' own sweep, 0.08 .. 1.5 Hz over RATE, slower than the LFO's so it can be read at the panel's
 * ~15 redraws a second; on the samples' clock, as HALL's torus, so no note moves it. Summed, not multiplied: turning
 * RATE changes the speed, it does not jump the drawing. div slows it for a drawing whose edges travel far in a turn */
#define SW_INC0 7791u                                /* per sample */
#define SW_INC1 1089u                                /* .. per step of RATE */
static uint32_t sw_phase(int32_t rate, uint32_t div)
{
    static uint32_t w, ph;
    uint32_t n = fx.dly_w - w;
    w = fx.dly_w;
    return ph += n * ((SW_INC0 + (uint32_t)rate * SW_INC1) / div);
}
/* MRC: the CHORUS page and INSERT's CHOR: n ellipses turning round one centre, evenly spread over half a turn, a whole
 * turn to a turn of ph (not half: the bumps are not the same after half a turn, and ph's wrap would jump them);
 * dep (0 .. 127) flattens them from a circle, rate (0 .. 127) wobbles their edges, OR_LOBES bumps crawling round each
 * at the sweep, OR_WOB deep at 127. Their sweep runs at an OR_SLOW th of the others' (a quarter turn a second at most):
 * the edge, 62 px out, would otherwise jump ~9 px a redraw at ~33 a second, ~19 px on the device's ~15 */
#define OR_R 62                                      /* the long radius, px */
#define OR_SEG 48
#define OR_LOBES 5u
#define OR_SLOW 6u
#define OR_WOB 1311                                  /* 4 % of the radius, Q15 */
static void orbits(uint32_t ph, int32_t dep, int32_t rate, uint32_t n, uint16_t c)
{
    int32_t rx = LN(OR_R), ry = rx - rx * dep * 4 / (5 * 127);
    uint32_t i, k;
    for (i = 0; i < n; i++) {
        uint32_t r = ph + i * (0x80000000u / n);
        int32_t cr = osc_sine(r + 0x40000000u), sr = osc_sine(r), px = 0, py = 0;
        uint16_t col = ux_mix(T_SURF, c, 100 - (int32_t)i * 15);
        for (k = 0; k <= OR_SEG; k++) {
            uint32_t a = k * (0xFFFFFFFFu / OR_SEG);
            int32_t s = 32768 + ((OR_WOB * rate / 127 * osc_sine(a * OR_LOBES + ph + i * 0x55555555u)) >> 15);
            int32_t ex = (((rx * osc_sine(a + 0x40000000u)) >> 15) * s) >> 15, ey = (((ry * osc_sine(a)) >> 15) * s) >> 15;
            int32_t x = LN(120) + ((ex * cr - ey * sr) >> 15), y = LN(70) + ((ex * sr + ey * cr) >> 15);
            if (k)
                cv_stroke(px, py, x, y, col);
            px = x;
            py = y;
        }
    }
}
/* MRC: the CHORUS page (the global RATE and DPTH, not the scope): three orbits, RATE turns and wobbles them, DPTH flattens them */
static void graph_chorus(uint16_t c)
{
    int32_t rate = clamp(song.g[G_CRATE], 0, 127);
    orbits(sw_phase(rate, OR_SLOW), clamp(song.g[G_CDEPTH], 0, 127), rate, 3u, c);
}
/* MRC: the DLY page: BOUNCE, in place of the scope. A note a ball kicked off the floor at the left, landing once a
 * delay time on the delay line's own clock (fx.c dly_drop): each landing an echo. The arcs ahead of the ball dotted,
 * behind it solid, so the whole tail shows at once. TIME: the gap between landings (the delay's length in samples on
 * a log scale, so the tempo moves it too), BN_W0 .. BN_W1. FDBK: each arc's height against the last, as the loop takes
 * the echo down: its gain and COLR's low-pass, a pass a landing, measured on a C4 saw (bn_levels; under BN_END: no more
 * arcs). COLR also shapes the ball: round at 0, a five-toothed sawblade at 127, spinning as it flies. MIX dims all of
 * it, BN_RIM % at 0. Arcs and ball in FDBK's accent, the landings and their splashes in MIX's, the note's own kick in
 * the ink. Once its last echo has landed a note fades out over two delay times (BN_FADE at least), and an older note
 * dims to 55 % as a newer one starts. A track with its DLY send at 0: a single hop. Prototype: dev/delay_visuals.html */
#define BN_GMAX 32                                  /* the landings drawn at most: the kick and 31 echoes */
#define BN_PER 169                                  /* a C4 period, samples: the saw the levels are measured on */
#define BN_END 983u                                 /* the quietest echo drawn, Q15 (3 %) */
#define BN_FY 122                                   /* the floor, px */
#define BN_X0 14                                    /* the kick, px */
#define BN_HMAX 104                                 /* the first echo's arc, px */
#define BN_DRY 22                                   /* a dry note's hop, px */
#define BN_W0 14                                    /* px from a landing to the next, at 2^10 samples (23 ms) and below */
#define BN_W1 64                                    /* .. at 2^16 (DLY_LEN, 1.49 s) */
#define BN_SEG 16                                   /* chords to an arc */
#define BN_RIM 10                                   /* everything at MIX 0, % (the ripples' outlines) */
#define BN_FADE 26460u                              /* the fade after the last landing, at least: 0.6 s */
#define BN_NEXT 13230u                              /* an older note dimming as a newer one starts: 0.3 s */
static uint16_t bn_lev[BN_GMAX];                    /* each landing's level, Q15 of the kick's */
static uint32_t bn_n, bn_key = ~0u;                 /* the landings an echo reaches (2 .. BN_GMAX), for FDBK and COLR */
/* the levels, when FDBK or COLR moved: landing 1 the line read straight (fx_buses' x: the kick's level), each one
 * after it one more pass round the loop, its low-pass (COLR, the saw's peak after it) and its gain (FDBK) */
static void bn_levels(void)
{
    int16_t w[BN_PER];
    uint32_t key = (uint32_t)song.g[G_DFDBK] | (uint32_t)song.g[G_DCOLOR] << 8, i, k;
    int32_t fb = clamp(song.g[G_DFDBK], 0, 120) * 230, col = 2000 + clamp(song.g[G_DCOLOR], 0, 127) * 240, g = 32767;
    if (key == bn_key)
        return;
    bn_key = key;
    for (i = 0; i < BN_PER; i++)
        w[i] = (int16_t)((int32_t)i * 32767 / (BN_PER - 1) - 16384);
    bn_lev[0] = bn_lev[1] = 32767;
    for (k = 2; k < BN_GMAX; k++) {
        int32_t y = 0, pk = 1, r;
        for (r = 0; r < 2; r++)                      /* (the second period: settled) */
            for (i = 0; i < BN_PER; i++) {
                y += (int32_t)(((int64_t)(((int32_t)w[i] << 8) - y) * col) >> 15);
                if (r) {
                    int32_t o = y >> 8;
                    w[i] = (int16_t)o;
                    pk = o > pk ? o : -o > pk ? -o : pk;
                }
            }
        g = g * fb >> 15;
        bn_lev[k] = (uint16_t)(g * pk / 16384 > 32767 ? 32767 : g * pk / 16384);
        if (bn_lev[k] <= BN_END)
            break;
    }
    bn_n = k < BN_GMAX ? k : BN_GMAX;
}
/* px from a landing to the next, LN, from the delay's length: log2 in Q4 (the top bit and the four under it) */
static int32_t bn_gap(void)
{
    uint32_t dl = delay_samples(), b = 31u - (uint32_t)__builtin_clz(dl);   /* (dl >= 16: b >= 4) */
    int32_t l = (int32_t)((b << 4) | ((dl >> (b - 4u)) & 15u)) - (10 << 4);
    return LN(BN_W0) + clamp(l, 0, 6 << 4) * LN(BN_W1 - BN_W0) / (6 << 4);
}
/* smoothstep, Q8 */
static uint32_t bn_ease(uint32_t x)
{
    x = x > 256u ? 256u : x;
    return x * x * (768u - 2u * x) >> 16;
}
/* a note's brightness, Q8 (0: gone): full while it bounces, then fading over two delay times (BN_FADE at least), and
 * to 55 % as the next note starts, over BN_NEXT. age: samples since its kick; next: since the next one's (~0: none) */
static uint32_t bn_life(uint32_t age, uint32_t next, uint32_t wet, uint32_t dl)
{
    uint32_t end = (wet ? bn_n - 1u : 1u) * dl, fade = 2u * dl > BN_FADE ? 2u * dl : BN_FADE, a = 256u;
    if (age > end) {
        if (age - end >= fade)
            return 0;
        a = 256u - bn_ease((age - end) * 256u / fade);
    }
    if (next != ~0u)
        a = a * (256u - 115u * bn_ease((next > BN_NEXT ? BN_NEXT : next) * 256u / BN_NEXT) / 256u) >> 8;
    return a;
}
/* how lit a landing is s samples after it: a fast rise, then gone by the next one (as the ripples' fx clock), Q8 */
static uint32_t bn_flash(uint32_t s, uint32_t dl)
{
    uint32_t d = dl < 2646u ? 2646u : dl > 22050u ? 22050u : dl, p, v;
    if (s >= d)
        return 0;
    p = s * 256u / d;
    if (p < 20u)
        return p * 256u / 20u;
    v = (256u - p) * 256u / 236u;
    return v * v >> 8;
}
/* the drops in order, oldest first: n of them; their slot i (0 the oldest) */
static uint32_t bn_drops(void)
{
    uint32_t w = dly_drop_w;
    return w < DROP_N ? w : DROP_N;
}
static uint32_t bn_slot(uint32_t i, uint32_t n)
{
    return (dly_drop_w - n + i) & (DROP_N - 1u);
}
/* any note still drawn (the page's signature: it moves) */
static uint32_t bn_live(void)
{
    uint32_t n = bn_drops(), i, now = fx.dly_w, dl = delay_samples();
    bn_levels();
    for (i = 0; i < n; i++) {
        uint32_t s = bn_slot(i, n), nx = i + 1u < n ? now - dly_drop[bn_slot(i + 1u, n)] : ~0u;
        if (bn_life(now - dly_drop[s], nx, dly_drop_wet[s], dl))
            return 1;
    }
    return 0;
}
static uint16_t bn_col(uint16_t c, int32_t pct)
{
    return ux_mix(cv_bg, c, clamp(pct, 0, 100));
}
/* the point u (Q8) along the arc into landing j, LN; h its height (LN), w the gap (LN) */
static void bn_pt(uint32_t j, int32_t u, int32_t h, int32_t w, int32_t *x, int32_t *y)
{
    *x = LN(BN_X0) + ((int32_t)j - 1) * w + w * u / 256;
    *y = LN(BN_FY) - 4 * h * u * (256 - u) / 65536;
}
static void graph_delay(uint16_t c)
{
    uint32_t n = bn_drops(), i, now = fx.dly_w, dl = delay_samples();
    int32_t w = bn_gap(), x;
    int32_t m = BN_RIM + (100 - BN_RIM) * clamp(song.g[G_DMIX], 0, 127) / 127;   /* MIX: all of it, % */
    int32_t sh = clamp(song.g[G_DCOLOR], 0, 127);
    uint16_t a1 = MRC_ON ? MRC_ACCENT[1 % MRC_ACCENTS] : c, a3 = MRC_ON ? MRC_ACCENT[3 % MRC_ACCENTS] : c;
    bn_levels();
    for (x = 4; x < 236; x += 3)                     /* the floor */
        cv_pset(x, BN_FY + 1, bn_col(T_TEXT, 30 * m / 100));
    for (i = 0; i < n; i++) {
        uint32_t s = bn_slot(i, n), age = now - dly_drop[s], wet = dly_drop_wet[s];
        uint32_t nx = i + 1u < n ? now - dly_drop[bn_slot(i + 1u, n)] : ~0u;
        uint32_t f = bn_life(age, nx, wet, dl) * (uint32_t)m / 100u, ng = wet ? bn_n : 2u, k, j;
        int32_t fq, px, py;
        if (!f)
            continue;
        k = age / dl;
        fq = (int32_t)((age % dl) * 256u / dl);
        /* the arcs: behind the ball solid, ahead dotted; each as bright as its echo is loud */
        for (j = 1; j < ng; j++) {
            int32_t h = wet ? LN(BN_HMAX) * bn_lev[j] / 32767 : LN(BN_DRY), br = wet ? bn_lev[j] * 100 >> 15 : 100;
            int32_t u, end = j <= k ? 256 : j == k + 1u ? fq : 0, x0, y0, x1, y1;
            uint16_t solid = bn_col(a1, (25 + 55 * br / 100) * (int32_t)f >> 8);
            uint16_t dots = bn_col(a1, (20 + 40 * br / 100) * (int32_t)f >> 8);
            if (LN(BN_X0) + ((int32_t)j - 1) * w > LN(240))
                break;
            h = h < LN(4) && wet ? LN(4) : h;
            bn_pt(j, 0, h, w, &x0, &y0);
            for (u = 256 / BN_SEG; u <= 256 && u - 256 / BN_SEG < end; u += 256 / BN_SEG) {
                bn_pt(j, u < end ? u : end, h, w, &x1, &y1);
                cv_stroke(x0, y0, x1, y1, solid);
                x0 = x1;
                y0 = y1;
            }
            for (u = 0; u <= 256; u += 32)
                if (u > end) {
                    bn_pt(j, u, h, w, &x1, &y1);
                    cv_dot(x1, y1, 10, dots);
                }
        }
        /* the landings: a dash each once reached, a splash as it plays (the kick and a dry hop in the ink) */
        for (j = 0; j < ng; j++) {
            int32_t lx = LN(BN_X0) + (int32_t)j * w;
            uint16_t lc = j && wet ? a3 : T_TEXT;
            uint32_t fl;
            if (lx > LN(240))
                break;
            if (j > k)
                continue;
            cv_stroke(lx - LN(2), LN(BN_FY + 3), lx + LN(2), LN(BN_FY + 3), bn_col(lc, (j && wet ? 80 : 60) * (int32_t)f >> 8));
            fl = bn_flash(age - j * dl, dl);
            if (fl > 5u) {
                int32_t r = LN(3) + (int32_t)(256u - fl) * LN(10) / 256, q, ex = 0, ey = 0;
                uint16_t sc = bn_col(lc, (int32_t)(fl * 100u >> 8) * (int32_t)f >> 8);
                for (q = 0; q <= 12; q++) {          /* the upper half of an ellipse, 0.35 as tall as wide */
                    uint32_t a = (uint32_t)q * (0x80000000u / 12u);
                    int32_t qx = lx + ((r * osc_sine(a + 0x40000000u)) >> 15);
                    int32_t qy = LN(BN_FY) - ((r * 35 / 100 * osc_sine(a)) >> 15);
                    if (q)
                        cv_stroke(ex, ey, qx, qy, sc);
                    ex = qx;
                    ey = qy;
                }
            }
        }
        /* the ball, in flight: COLR its teeth, turning with its way along (3 rad a landing) */
        if (k + 1u < ng) {
            uint32_t rot = (k * 256u + (uint32_t)fq) * 8010446u, b;
            int32_t h = wet ? LN(BN_HMAX) * bn_lev[k + 1u] / 32767 : LN(BN_DRY), br = wet ? bn_lev[k + 1u] * 100 >> 15 : 100;
            int32_t cx, cy, ex = 0, ey = 0;
            uint16_t bc;
            h = h < LN(4) && wet ? LN(4) : h;
            bn_pt(k + 1u, fq, h, w, &cx, &cy);
            cy -= LN(3) + 8;                         /* (sitting on its arc) */
            bc = bn_col(wet ? a1 : T_TEXT, (50 + 50 * br / 100) * (int32_t)f >> 8);
            cv_dot(cx, cy, LN(2), bc);
            for (b = 0; b <= 40u; b++) {             /* 8 points a tooth: a saw, 1 .. -0.75 px over a 2.5 px ball */
                uint32_t a = b * 107374182u + rot;
                int32_t saw = 256 - 64 * (int32_t)(b % 8u), rr = 40 + sh * (8 * 256 + saw * 16) / (127 * 256);
                int32_t qx = cx + ((rr * osc_sine(a + 0x40000000u)) >> 15), qy = cy + ((rr * osc_sine(a)) >> 15);
                if (b)
                    cv_stroke(ex, ey, qx, qy, bc);
                ex = qx;
                ey = qy;
            }
        }
    }
}
/* MRC: the REVERB page, in place of the scope: one drawing to a model, from SIZE and DAMP alone (no note, no level).
 * ROOM: a cube and up to RM_N hexagons inside its outline, turning about its near corner once in 16 s. A room's combs do not
 * change length, so the cube does not grow: SIZE adds the hexagons from the outermost in, the newest shrinking in from the
 * one outside it as it fades in (none at 0, all at 127), DAMP dims them from the inside out,
 * the innermost first, each fading over RM_FADE hexagons' worth of the knob, all at RM_MIN at 127 (the cube stays
 * bright) */
#define RV_CX 120
#define RV_CY 70
#define RM_W 59                                     /* half the cube's width: RM_U * sqrt 3, so the hexagons are regular */
#define RM_U 34                                     /* its outline's rows: 2 up and down, 1 at the sides (radius 68, inside the panel's 70) */
#define RM_N 5
#define RM_STEP 16                                  /* each hexagon this % of the cube smaller than the last */
#define RM_MIN 10                                   /* the dimmest, % */
#define RM_FADE 2
#define RM_SPIN 6087u                               /* a turn (2^32) in 16 s of samples */
/* the outline at k % of the cube (Q8), turned about the centre; the near corner's edges too */
static int32_t rm_c, rm_s;                          /* the turn's cosine and sine, Q15 */
static void rm_pt(int32_t x, int32_t y, int32_t *ox, int32_t *oy)   /* x, y: 1/16 px from the centre */
{
    *ox = LN(RV_CX) + ((x * rm_c - y * rm_s) >> 15);
    *oy = LN(RV_CY) + ((x * rm_s + y * rm_c) >> 15);
}
static void rm_hex(int32_t k, uint16_t c)
{
    static const int8_t HX[6] = {-1, 0, 1, 1, 0, -1}, HY[6] = {-1, -2, -1, 1, 2, 1};
    int32_t px = 0, py = 0, i;
    for (i = 0; i <= 6; i++) {
        int32_t x, y;
        rm_pt(HX[i % 6] * LN(RM_W) * k / 25600, HY[i % 6] * LN(RM_U) * k / 25600, &x, &y);
        if (i)
            cv_stroke(px, py, x, y, c);
        px = x;
        py = y;
    }
}
static void rm_edge(int32_t x, int32_t y, uint16_t c)   /* from the centre to (x, y) px */
{
    int32_t cx, cy, ex, ey;
    rm_pt(0, 0, &cx, &cy);
    rm_pt(LN(x), LN(y), &ex, &ey);
    cv_stroke(cx, cy, ex, ey, c);
}
static void graph_room(int32_t s, int32_t d, uint16_t c)
{
    uint32_t ph = fx.dly_w * RM_SPIN;
    int32_t sq = s * RM_N * 256 / 127, n = (sq + 255) >> 8, x = d * (n - 1 + RM_FADE) * 100 / 127, i;   /* x: the fade's edge, % of a hexagon */
    int32_t fq = sq & 255 ? sq & 255 : 256;         /* the newest: how far in (Q8) */
    rm_s = osc_sine(ph);
    rm_c = osc_sine(ph + 0x40000000u);
    rm_hex(100 * 256, c);
    rm_edge(-RM_W, -RM_U, c);                       /* the near corner's three edges */
    rm_edge(RM_W, -RM_U, c);
    rm_edge(0, 2 * RM_U, c);
    for (i = 1; i <= n; i++) {
        int32_t f = clamp(x - (n - i) * 100, 0, RM_FADE * 100);   /* (n - i: how many lie inside this one) */
        int32_t k = (100 - i * RM_STEP) * 256, m = 100 - f * (100 - RM_MIN) / (RM_FADE * 100);
        if (i == n && fq < 256) {                   /* the newest comes in from the outer one's size, fading in */
            k += (RM_STEP * 256) * (256 - fq) >> 8;
            m = m * fq >> 8;
        }
        rm_hex(k, ux_mix(T_SURF, c, m));
    }
}
/* SPRING: a coil, a helix seen from the side, rolling left to right (a turn in 2 s on the audio clock). SIZE takes its height and loops up from a flat line (a wave on the
 * way, as they close); DAMP makes it decay left to right, height and loops alike, into the line it settles on: 2^-6.5
 * by the right end at 127 (fx.c exp2_neg). A flat lead in, from the loop's foot; a flat stub out */
#define CL_TURNS 11
#define CL_SEG 24                                   /* strokes a turn */
#define CL_PITCH 16                                 /* px from a turn to the next */
#define CL_X0 29                                    /* the first turn's x */
#define CL_XA 14                                    /* the lead in's end */
#define CL_XB 226                                   /* the stub's */
#define CL_AMP 45                                   /* half the height at SIZE 127, px */
#define CL_LOOP 12                                  /* how far a loop swings back and forth, px */
#define CL_SPIN 48694u                              /* a turn (2^32) in 2 s of samples: the coil rolls 8 px a second */
#define CL_DECAY 3349                               /* DAMP to the decay's exponent at the right end: Q16 / 127 */
static void graph_spring(int32_t s, int32_t d, uint16_t c)
{
    int32_t a = LN(CL_AMP) * s / 127, r = LN(CL_LOOP) * s / 127, n = CL_TURNS * CL_SEG, i;
    int32_t px = LN(CL_XA), py = LN(RV_CY) + a;
    uint32_t rot = fx.dly_w * CL_SPIN;
    for (i = 0; i <= n + CL_SEG / 4; i++) {              /* (a quarter turn more: down to the line) */
        uint32_t ph = (uint32_t)i * (0xFFFFFFFFu / CL_SEG) - rot;   /* (the phase running back: the coil rolls right) */
        int32_t v = (int32_t)exp2_neg((uint32_t)(d * CL_DECAY * (i < n ? i : n) / n));
        int32_t x = LN(CL_X0) + LN(CL_PITCH) * i / CL_SEG + (((r * v) >> 15) * osc_sine(ph) >> 15);
        int32_t y = LN(RV_CY) + (((a * v) >> 15) * osc_sine(ph + 0x40000000u) >> 15);
        if (!i)
            py = y;                                      /* (the lead in meets the coil where it stands) */
        cv_stroke(px, py, x, y, c);
        px = x;
        py = y;
    }
    cv_stroke(px, py, LN(CL_XB), LN(RV_CY), c);
}
/* HALL: a torus, turning once in 8 s on the audio clock (fx.dly_w; no sound moves it), tilted 30 degrees: rings of
 * tube round it, the far side dimmer. SIZE is the hole's radius, DAMP thins the tube. Three passes, one a brightness
 * (cv_stroke keeps a colour's ramp between strokes). At both knobs' most, its top and bottom are 3 px inside the panel */
#define TOR_M 12                                    /* rings */
#define TOR_SEG 16                                  /* strokes a ring */
#define TOR_R0 10                                   /* the hole's radius at SIZE 0 .. */
#define TOR_R1 56                                   /* .. and 127, px */
#define TOR_W0 42                                   /* the tube's width at DAMP 0 .. */
#define TOR_W1 8                                    /* .. and 127, px */
#define TOR_COS 28378                               /* cos 30 degrees, Q15; sin 30 is a shift */
#define TOR_SPIN 12173u                             /* a turn (2^32) in 8 s of samples */
static void graph_hall(int32_t s, int32_t d, uint16_t c)
{
    int32_t a = (LN(TOR_W0) - LN(TOR_W0 - TOR_W1) * d / 127) / 2, rr = LN(TOR_R0) + LN(TOR_R1 - TOR_R0) * s / 127 + a;
    int32_t lim = (rr + a) * 2 / 3, ct[TOR_SEG + 1], st[TOR_SEG + 1];
    uint16_t col[3];
    uint32_t rot = fx.dly_w * TOR_SPIN, b, m, k;
    col[0] = ux_mix(T_SURF, c, 30);
    col[1] = ux_mix(T_SURF, c, 60);
    col[2] = c;
    for (k = 0; k <= TOR_SEG; k++) {
        st[k] = osc_sine(k << 28);                   /* (TOR_SEG 16: the turn in 16ths) */
        ct[k] = osc_sine((k << 28) + 0x40000000u);
    }
    for (b = 0; b < 3u; b++)
        for (m = 0; m < TOR_M; m++) {
            uint32_t u = m * (0xFFFFFFFFu / TOR_M) + rot;
            int32_t cu = osc_sine(u + 0x40000000u), su = osc_sine(u), px = 0, py = 0, pz = 0;
            for (k = 0; k <= TOR_SEG; k++) {
                int32_t q = rr + ((a * ct[k]) >> 15), xr = (q * cu) >> 15, zr = (q * su) >> 15, yr = (a * st[k]) >> 15;
                int32_t x = LN(RV_CX) + xr, y = LN(RV_CY) - ((yr * TOR_COS) >> 15) + (zr >> 1);
                int32_t z = (yr >> 1) + ((zr * TOR_COS) >> 15), t = pz + z;
                if (k && (t < -lim ? 0u : t < lim ? 1u : 2u) == b)
                    cv_stroke(px, py, x, y, col[b]);
                px = x;
                py = y;
                pz = z;
            }
        }
}
static void graph_reverb(uint16_t c)
{
    int32_t s = clamp(song.g[G_RSIZE], 0, 127), d = clamp(song.g[G_RDAMP], 0, 127), ty = song.g[G_RTYPE];
    if (ty == 2)
        graph_hall(s, d, c);
    else if (ty)
        graph_spring(s, d, c);
    else
        graph_room(s, d, c);
}
/* MRC: the CLOCK page, a metronome in place of the scope: ICON_TEMPO's glyph (Fukiai symbol_tempo, 2048 units, y up)
 * drawn by its strokes' centre lines: the case, the bar across its foot, the stick pivoting on the bar's middle. The
 * stick stands 45 degrees left on the bar's first beat (the glyph mirrored) and swings across, one side to a beat, on
 * the click's own clock (seq.c click_tick, the count-in's too);
 * stopped, it rests there too. The bar's first beat lights all of it in the accent, fading over the beat */
#define MT_S 303                                     /* LN per glyph unit, Q8: the case 1485 units, 110 px */
#define MT_CX 930                                    /* the glyph's middle .. */
#define MT_CY 1023
#define MT_PY 661                                    /* the bar, the stick's pivot */
#define MT_LEN 990                                   /* the stick, its round end included */
#define MT_SWING 0x20000000                          /* 45 degrees (a turn: 2^32) */
#define MT_TIP LN(3)                                 /* the dot on the stick's end: its radius, 3 px (MRC, not the glyph's) */
static int32_t mt_x(int32_t u) { return LN(RV_CX) + (u - MT_CX) * MT_S / 256; }
static int32_t mt_y(int32_t v) { return LN(RV_CY) - (v - MT_CY) * MT_S / 256; }
/* where the click is: the beat (0..3, -1 stopped) and how far into it, Q16 */
static int32_t mt_beat(uint32_t *f)
{
    int32_t b = click_beat_now();
    uint32_t n = click_beat_len(), p = div_samples(2);
    *f = 0;
    if (cin_left)
        *f = n ? (uint32_t)(((uint64_t)cin_pos << 16) / n) : 0u;
    else if (b >= 0 && clk_pos != CLK_START && p)
        *f = (uint32_t)((((uint64_t)(clk_step & 3u) * p + clk_pos) << 16) / (4u * p));
    *f = *f > 65535u ? 65535u : *f;
    return b;
}
/* the stick's angle (a turn: 2^32, + right) and its accent, %: what the drawing and graph_signature read */
static int32_t mt_pose(uint32_t *lit)
{
    uint32_t f;
    int32_t b = mt_beat(&f);
    *lit = b == 0 ? (65535u - f) * 100u >> 16 : 0u;
    if (b < 0)
        return -MT_SWING;                            /* (where the first beat starts it) */
    return (int32_t)(-((int64_t)MT_SWING * osc_sine(((uint32_t)(b & 1) << 31) + (f << 15) + 0x40000000u)) >> 15);
}
static void graph_clock(uint16_t c)
{
    static const int16_t CASE[5][2] = {{630, 1765}, {1228, 1765}, {1436, 280}, {422, 280}, {630, 1765}};
    uint32_t lit, i;
    int32_t a = mt_pose(&lit), px = mt_x(MT_CX), py = mt_y(MT_PY), tx, ty;
    uint16_t sc = lit ? ux_mix(c, T_ACCENT, (int32_t)lit) : c;   /* (the whole metronome lights) */
    for (i = 1; i < 5u; i++)
        cv_stroke(mt_x(CASE[i - 1u][0]), mt_y(CASE[i - 1u][1]), mt_x(CASE[i][0]), mt_y(CASE[i][1]), sc);
    cv_stroke(mt_x(475), py, mt_x(1383), py, sc);    /* (the case's walls at the bar's height) */
    tx = px + (MT_LEN * MT_S / 256) * osc_sine((uint32_t)a) / 32768;
    ty = py - (MT_LEN * MT_S / 256) * osc_sine((uint32_t)a + 0x40000000u) / 32768;
    cv_stroke(px, py, tx, ty, sc);
    cv_dot(tx, ty, MT_TIP, sc);                      /* (on top of the stroke it ends) */
}
/* MRC: the ENV DEST page, in place of the scope: the envelope (ENV's icon in a box, left) routed to its three
 * destinations (FLT, PTCH, SHP's icons in boxes, right), one line splitting into three at right angles; each
 * destination and its branch in its knob's accent, dimmed by its amount (10 % at 0, full at -100 / +100 %) */
#define EG_B 30                                      /* a box's side, px */
#define EG_L 22                                      /* the envelope's box, left edge */
#define EG_R 186                                     /* the destinations' boxes, left edge */
#define EG_TX 120                                    /* the trunk splitting the line */
#define EG_DY 40                                     /* the destinations apart */
static void eg_box(int32_t x, int32_t cy, uint32_t id, uint16_t col)
{
    int32_t y = cy - EG_B / 2;
    cv_rrect(x, y, EG_B, EG_B, 6, col, T_SURF);
    cv_rrect(x + 2, y + 2, EG_B - 4, EG_B - 4, 4, T_SURF, col);
    GFX_HOOK_ALIGN(x, y, x + EG_B, y + EG_B, AL_HV, "ENV DEST box icon");
    cv_icon_in(x, y, EG_B, EG_B, 16, id, col, T_SURF);
}
static void graph_edest(const track_t *t, uint16_t c)
{
    static const uint8_t DEST[3] = {ICON_CUTOFF, ICON_PITCH, ICON_SHAPE};   /* the page's knobs 1..3 */
    int32_t cy = graph_h() / 2, k;
    cv_rect(EG_L + EG_B, cy - 1, EG_TX + 1 - EG_L - EG_B, 2, c);   /* the line out of the envelope .. */
    cv_rect(EG_TX - 1, cy - EG_DY - 1, 2, 2 * EG_DY + 2, c);       /* .. the trunk splitting it */
    for (k = 0; k < 3; k++) {
        int32_t y = cy + (k - 1) * EG_DY;
        int32_t v = t->p[P_ED_FLT + k], a = v < 0 ? -v * 90 / 64 : v * 90 / 63;   /* (-64 .. 63: -100 .. +100 %) */
        uint16_t col = ux_mix(T_SURF, MRC_ON ? MRC_ACCENT[k % MRC_ACCENTS] : c, 10 + a);
        cv_rect(EG_TX + 1, y - 1, EG_R - EG_TX - 1, 2, col);
        eg_box(EG_R, y, DEST[k], col);
    }
    eg_box(EG_L, cy, ICON_ENV, c);
}
/* MRC: the INSERT pages, one drawing to a TYPE, from A B C and MIX alone (no sound moves them; the swept ones move at
 * RATE, sw_phase). MIX dims the whole drawing, OFF reads OFF.
 * ins_wave: a sine across the panel, gained (Q8) into its ceiling h px; the HARD and FLANG lines */
#define WV_SEG 40                                    /* 5 px each */
#define WV_X0 20
#define WV_D15 161061274u                            /* a stroke's phase: 1.5 turns across */
#define WV_D2 214748365u                             /* .. 2 turns */
static void ins_wave(uint32_t ph, uint32_t dph, int32_t g, int32_t h, uint16_t c)
{
    int32_t j, py = 0;
    for (j = 0; j <= WV_SEG; j++) {
        int32_t v = clamp((osc_sine(ph + (uint32_t)j * dph) * g) >> 8, -32767, 32767), y = LN(RV_CY) - v * LN(h) / 32768;
        if (j)
            cv_stroke(LN(WV_X0 + (j - 1) * 5), py, LN(WV_X0 + j * 5), y, c);
        py = y;
    }
}
/* SOFT: a wire sheet lifted by a ball underneath, seen from a corner. DRIVE lifts it, LEVEL widens it, TONE
 * subdivides it: its lines sit SH_U / m apart from the middle out, m 2 .. SH_MMAX over TONE without steps, so they
 * close in on the middle and a new one comes in at each edge as the last one leaves it; the edges always drawn */
#define SH_U 64                                      /* the sheet's half side, sheet units */
#define SH_MMAX 5
#define SH_R2 6000                                   /* the bump's reach squared, sheet units */
static void sh_point(int32_t u, int32_t v, int32_t h, int32_t w, int32_t *x, int32_t *y)
{
    int32_t r = u * u + v * v, q = r < SH_R2 ? 256 - r * 256 / SH_R2 : 0;
    *x = LN(120) + (u - v) * LN(w) / 128;
    *y = LN(84) + (u + v) * LN(w) * 117 / 32768 - ((h * q * q) >> 16);
}
static void sh_line(int32_t u, int32_t side, int32_t h, int32_t w, uint16_t c)   /* u's line, across the sheet */
{
    int32_t j, px = 0, py = 0, qx, qy;
    for (j = -8; j <= 8; j++) {
        sh_point(side ? j * 8 : u, side ? u : j * 8, h, w, &qx, &qy);
        if (j > -8)
            cv_stroke(px, py, qx, qy, c);
        px = qx;
        py = qy;
    }
}
static void graph_soft(int32_t a, int32_t b, int32_t l, uint16_t c)
{
    int32_t m = 512 + b * (SH_MMAX - 2) * 256 / 127, h = LN(4) + LN(62) * a / 127, w = 52 + l * 60 / 127, k, side;   /* m Q8 */
    uint16_t col = ux_mix(T_SURF, c, 70);
    for (side = 0; side < 2; side++) {
        sh_line(-SH_U, side, h, w, col);
        sh_line(SH_U, side, h, w, col);
        for (k = -SH_MMAX; k <= SH_MMAX; k++) {
            int32_t u = k * SH_U * 256 / m;
            if (u > -SH_U && u < SH_U)
                sh_line(u, side, h, w, col);
        }
    }
}
/* HARD: nested sines all stopped by one ceiling. DRIVE raises them into it, LEVEL sets it, TONE packs them: the lines'
 * gains 1, 1 - 1/n, 1 - 2/n .. down to the middle, n 2 .. 7 over TONE without steps, so they rise towards the ceiling
 * and the next one comes out of the flat middle line */
static void graph_hard(int32_t a, int32_t b, int32_t l, uint16_t c)
{
    int32_t n = 512 + b * 1280 / 127, g = 102 + a * 564 / 127, h = 20 + l * 38 / 127, k, i;   /* n Q8 */
    for (i = 0; (k = 256 - i * 65536 / n) > 0; i++)
        ins_wave(0u, WV_D15, (g * k) >> 8, h, ux_mix(T_SURF, c, 30 + 70 * k / 256));
}
/* FOLD: an accordion-folded strip, PS_HALF px each side of the middle. DRIVE folds it from the outside in: the folds sit
 * PS_HALF / m apart from the middle out, m 1 .. 7 without steps, so they close in on the middle and a new one comes in
 * at each end as the last one leaves it (the ends follow the next fold's slope). TONE sets its height, LEVEL dims all of
 * it (to PS_MIN), the valleys at PS_VALLEY of the rest */
#define PS_MIN 10
#define PS_VALLEY 40
#define PS_HALF 90
#define PS_D 11                                      /* a fold's rise, px */
static int32_t ps_y(int32_t k) { return LN(RV_CY) + (k & 1 ? LN(PS_D) : -LN(PS_D)); }
static void graph_fold(int32_t a, int32_t b, int32_t l, uint16_t c)
{
    int32_t p = LN(PS_HALF) * 256 / (256 + a * 1536 / 127), h = LN(14) + LN(34) * b / 127, n = LN(PS_HALF) / p, k;
    int32_t ye = ps_y(n) + (ps_y(n + 1) - ps_y(n)) * (LN(PS_HALF) - n * p) / p, px = LN(120 - PS_HALF), py = ye;
    int32_t lv = PS_MIN + l * (100 - PS_MIN) / 127;
    uint16_t dim = ux_mix(T_SURF, c, lv * PS_VALLEY / 100);
    c = ux_mix(T_SURF, c, lv);
    cv_stroke(px, py - h, px, py + h, c);
    for (k = -n; k <= n + 1; k++) {                  /* (n + 1: the right end) */
        int32_t x = k > n ? LN(120 + PS_HALF) : LN(120) + k * p, y = k > n ? ye : ps_y(k);
        cv_stroke(x, y - h, x, y + h, k <= n && (k & 1) ? dim : c);
        cv_stroke(px, py - h, x, y - h, c);
        cv_stroke(px, py + h, x, y + h, c);
        px = x;
        py = y;
    }
}
/* FUZZ: a ring folded on itself (fx.c ins_shape's folder over a sine of six lobes). DRIVE multiplies the petals, TONE
 * lights a copy inside it, LEVEL sets its size */
#define RO_SEG 240
#define RO_DA 17895697u                              /* 2^32 / RO_SEG */
static void graph_fuzz(int32_t a, int32_t b, int32_t l, uint16_t c)
{
    int32_t g = 256 + a * 1280 / 127, rad = LN(26) + LN(38) * l / 127, tone = b * 100 / 127;
    int32_t px = 0, py = 0, qx = 0, qy = 0, i;
    uint16_t inner = ux_mix(T_SURF, c, tone);
    for (i = 0; i <= RO_SEG; i++) {
        uint32_t u = (uint32_t)i * RO_DA;
        int32_t w = ins_shape(IT_FOLD, (osc_sine(u * 6u) * g) >> 9, 0);   /* +-24576 */
        int32_t r = (rad * (635 + w * 389 / 24576)) >> 10, co = osc_sine(u + 0x40000000u), si = osc_sine(u);
        int32_t x = LN(120) + ((r * co) >> 15), y = LN(70) + ((r * si) >> 15);
        int32_t x2 = LN(120) + ((((r * 141) >> 8) * co) >> 15), y2 = LN(70) + ((((r * 141) >> 8) * si) >> 15);
        if (i) {
            cv_stroke(px, py, x, y, c);
            if (tone > 3)
                cv_stroke(qx, qy, x2, y2, inner);
        }
        px = x;
        py = y;
        qx = x2;
        qy = y2;
    }
}
/* CRUSH: a sine as a row of isometric blocks, one to a BIT (1 .. 16). RATE tightens the wave, half a turn across the
 * row at 0 to three at 127; LPF dims them from the right: at its lowest (30 Hz) the left one alone at full, the others
 * at BW_MIN, the edge block fading as it opens. Each block paints over what lies behind it */
#define BW_X0 24
#define BW_W 176
#define BW_Y 118                                     /* the blocks' feet */
#define BW_HMAX 76
#define BW_DX 10                                     /* the top face's slant */
#define BW_DY 6
#define BW_MIN 10                                    /* the dimmest, % */
static void graph_crush(int32_t a, int32_t b, int32_t l, uint16_t c)
{
    int32_t n = 1 + (a >> 3), e = 100 + l * (n - 1) * 100 / 127, i, k;
    uint32_t dph = (uint32_t)(128 + b * 640 / 127) * (0x01000000u / (uint32_t)n);   /* turns across, Q8, over the row */
    for (i = 0; i < n; i++) {
        int32_t x = BW_X0 + i * BW_W / n, w = BW_X0 + (i + 1) * BW_W / n - x, pct = clamp(e - i * 100, 0, 100);
        int32_t ty = BW_Y - 4 - (osc_sine((uint32_t)i * dph) + 32768) * BW_HMAX / 65536;
        uint16_t col = ux_mix(T_SURF, c, BW_MIN + pct * (100 - BW_MIN) / 100);
        cv_rect(x, ty, w, BW_Y - ty + 1, T_SURF);
        for (k = 1; k <= BW_DY; k++)
            cv_rect(x + k * BW_DX / BW_DY, ty - k, w, 1, T_SURF);
        fx_edge(x, ty, x + BW_DX, ty - BW_DY, col);  /* the top face */
        fx_edge(x + BW_DX, ty - BW_DY, x + w + BW_DX, ty - BW_DY, col);
        fx_edge(x + w + BW_DX, ty - BW_DY, x + w, ty, col);
        fx_edge(x + w, ty, x, ty, col);
        fx_edge(x, ty, x, BW_Y, col);                /* the front */
        fx_edge(x, BW_Y, x + w, BW_Y, col);
        fx_edge(x + w, BW_Y, x + w, ty, col);
        fx_edge(x + w, BW_Y, x + w + BW_DX, BW_Y - BW_DY, col);   /* the right side (the next block paints over it) */
        fx_edge(x + w + BW_DX, BW_Y - BW_DY, x + w + BW_DX, ty - BW_DY, col);
    }
}
/* PHASR: one ring to an all-pass stage, each rolling inside the one before. A turn to a sweep, one in two the other way, each PR_LAG off the one
 * outside it; DEPTH takes them off centre (one in the other at 0), FDBK lights the inner three from the outside in: at 0
 * all three at PR_MIN, at 127 all full, each over PR_FADE rings' worth of the knob; the outermost always full. m: MIX's brightness, % */
#define PR_LAG 0x14600000u                           /* ~29 degrees */
#define PR_FADE 2
#define PR_MIN 10
static void graph_phaser(int32_t a, int32_t b, int32_t l, uint16_t c, int32_t m)
{
    static const int8_t R[4] = {62, 48, 34, 20};
    uint32_t ph = sw_phase(a, 1u);
    int32_t x = LN(120), y = LN(70), fd = (127 - l) * (3 - 1 + PR_FADE) * 100 / 127, i;
    for (i = 0; i < 4; i++) {
        uint32_t u = 0xC0000000u + (i & 1 ? 0u - ph : ph) + (uint32_t)i * PR_LAG;
        int32_t o = i ? LN(R[i - 1] - R[i]) * b / 127 : 0, f = i ? clamp(fd - (3 - i) * 100, 0, PR_FADE * 100) : 0;
        x += (o * osc_sine(u + 0x40000000u)) >> 15;
        y += (o * osc_sine(u)) >> 15;
        ln_ring(x, y, LN(R[i]), ux_mix(T_SURF, c, m *(100 - f * (100 - PR_MIN) / (PR_FADE * 100)) / 100));
    }
}
/* FLANG: the wave and its delayed self. The sweep (fx.c's squared triangle) and DEPTH slide the copy, FDBK adds fainter
 * ones behind it */
static void graph_flanger(int32_t a, int32_t b, int32_t l, uint16_t c)
{
    uint32_t tr = pf_tri(sw_phase(a, 1u));
    int32_t dx = 64 + (int32_t)((tr * tr) >> 16) * b / 127 * 544 / 65536, i;   /* 1/16 px */
    for (i = (l * 3 + 63) / 127; i >= 0; i--)
        ins_wave(0u - (uint32_t)(i * dx) * 2684355u, WV_D2, 256, 44, ux_mix(T_SURF, c, 100 - i * 24));
}
static void graph_insert(const track_t *t, uint16_t c)
{
    int32_t ty = clamp(t->p[P_ITYPE], 0, IT_N - 1), a = clamp(t->p[P_IA], 0, 127), b = clamp(t->p[P_IB], 0, 127);
    int32_t l = clamp(t->p[P_IC], 0, 127), m = 22 + clamp(t->p[P_IMIX], 0, 127) * 78 / 127;
    uint16_t ink = ux_mix(T_SURF, c, m);
    switch (ty) {
    case IT_SOFT:
        graph_soft(a, b, l, ink);
        break;
    case IT_HARD:
        graph_hard(a, b, l, ink);
        break;
    case IT_FOLD:
        graph_fold(a, b, l, ink);
        break;
    case IT_FUZZ:
        graph_fuzz(a, b, l, ink);
        break;
    case IT_CRUSH:
        graph_crush(a, b, l, ink);
        break;
    case IT_PHASER:
        graph_phaser(a, b, l, c, m);
        break;
    case IT_FLANGER:
        graph_flanger(a, b, l, ink);
        break;
    case IT_CHORUS:
        orbits(sw_phase(a, OR_SLOW), b, a, 2u + (uint32_t)(l * 3 + 63) / 127u, ink);
        break;
    default: {                                       /* OFF: the word */
        const aafont_t *f = large_face_has("OFF") ? &AF_L : &AF_M;
        GFX_HOOK_ALIGN(0, 0, 240, H_GRAPH, AL_HV, "insert off centred");
        cv_text_in(0, f == &AF_L ? CAP_IN(L, H_GRAPH) : CAP_IN(M, H_GRAPH), 240, f, "OFF", T_MID, T_SURF);
    }
    }
}

/* MRC: the VOICE pages, the voices round a listener's head seen from above, from the knobs alone (prototype:
 * dev/voice_visuals.html, ARC). A dot a voice on a half circle from L to R, at its pan: PAN turns them (64: 80 degrees
 * over), SPRD spreads them as fx.c mix_spread does, -1 .. 1 across the voices; DTUNE pushes each in or out by its own
 * share, the same in POLY and UNI, never past the half circle; GLD one arc through each dot, round the head, its
 * middle on the dot, out past it from GLD 1, the same on every dot. MONO: one dot at PAN; LEG: tied.
 * PAN a triangle on the half circle pointing at the head. GLMOD, ALLOC and PRIO draw nothing */
#define VA_X 120                                     /* the head, px */
#define VA_Y 128
#define VA_R 104                                     /* the half circle */
#define VA_HEAD 10                                   /* the head's radius */
#define VA_DOT (VA_R - 32)                           /* the dots, at DTUNE 0 */
#define VA_DT 20                                     /* .. in or out at most, at DTUNE 127 */
#define VA_GLD0 5                                    /* GLD's arc, px either side at 1: out past the dot */
#define VA_GLD 14                                    /* .. and more at 127 */
#define VA_PAN 233016                                /* 80 degrees in osc_sine's turn, over 4096 (Q12 pan) */
#define VA_RAD 683565                                /* a radian in osc_sine's turn, over 1000 */
static void gn_arc(int32_t cx, int32_t cy, int32_t rx, int32_t ry, uint32_t a0, uint32_t da, int32_t n, int dot, uint16_t c);
static void graph_voice(const track_t *t, uint16_t c)
{
    const engine_t *e = ENGINES[t->eng_req % NENGINES];
    int32_t mode = e->oneshot ? V_POLY : clamp(t->p[P_VOICE], V_POLY, V_UNISON), pan = clamp(t->p[P_PAN], -64, 63);
    int32_t n = mode == V_MONO || mode == V_LEGATO ? 1 : e->poly && e->poly < NVOICE ? e->poly : NVOICE;
    int32_t sprd = clamp(t->p[P_SPRD], 0, 127), dt = clamp(t->p[P_DETUNE], 0, 127), i, k;
    int32_t gld = e->oneshot ? 0 : clamp(t->p[P_GLIDE], 0, 127);   /* (hits: no glide) */
    uint16_t ink = MRC_ON ? T_TEXT : c;
    uint16_t a0 = MRC_ON ? MRC_ACCENT[0] : c, a1 = MRC_ON ? MRC_ACCENT[1 % MRC_ACCENTS] : c;
    uint16_t a2 = MRC_ON ? MRC_ACCENT[2 % MRC_ACCENTS] : c;
    int32_t hx = LN(VA_X), hy = LN(VA_Y), px, py, tx, ty;
    uint32_t a;
    gn_arc(hx, hy, LN(VA_R), LN(VA_R), 0x80000000u, 0x80000000u / 48u, 48, 0, ux_mix(T_SURF, ink, 45));
    cv_text_in(0, VA_Y - 6 + CAP_IN(S, 12), 14, &AF_S, "L", T_MID, T_SURF);
    cv_text_in(226, VA_Y - 6 + CAP_IN(S, 12), 14, &AF_S, "R", T_MID, T_SURF);
    ln_ring(hx, hy, LN(VA_HEAD), ux_mix(T_SURF, ink, 90));                      /* the head, its nose, its ears */
    cv_stroke(hx, hy - LN(VA_HEAD), hx, hy - LN(VA_HEAD + 4), ux_mix(T_SURF, ink, 90));
    ln_ring(hx - LN(VA_HEAD + 1), hy, LN(3), ux_mix(T_SURF, ink, 75));
    ln_ring(hx + LN(VA_HEAD + 1), hy, LN(3), ux_mix(T_SURF, ink, 75));
    a = (uint32_t)(pan * 4096 / 64 * VA_PAN);                             /* PAN: a triangle pointing at the head */
    tx = osc_sine(a);
    ty = -osc_sine(a + 0x40000000u);
    for (k = -6; k <= 6; k++) {                                           /* (filled: strokes from the tip across) */
        int32_t bx = hx + ((LN(VA_R + 8) * tx) >> 15) - ((ty * LN(3) * k / 6) >> 15);
        int32_t by = hy + ((LN(VA_R + 8) * ty) >> 15) + ((tx * LN(3) * k / 6) >> 15);
        cv_stroke(hx + ((LN(VA_R + 2) * tx) >> 15), hy + ((LN(VA_R + 2) * ty) >> 15), bx, by, a2);
    }
    for (k = 0; k < 2; k++)                                               /* the beams and arcs, then the dots on top */
        for (i = 0; i < n; i++) {
            int32_t q = pan * 4096 / 64 + (n > 1 ? (2 * i - (n - 1)) * 4096 / (n - 1) * sprd / 127 : 0);
            int32_t d = n > 1 ? ((i * 37) % 7 + 3) * (i & 1 ? 1 : -1) : 0;   /* the voice's share of DTUNE, of 9 */
            int32_t r = LN(VA_DOT) + d * dt * LN(VA_DT) / (9 * 127);
            a = (uint32_t)(clamp(q, -4096, 4096) * VA_PAN);
            px = hx + ((r * osc_sine(a)) >> 15);
            py = hy - ((r * osc_sine(a + 0x40000000u)) >> 15);
            if (k) {
                cv_dot(px, py, LN(3), a0);
                if (mode == V_LEGATO)                                     /* tied: an arc over it */
                    gn_arc(px, py, LN(8), LN(8), 0u - 1825361101u, 1503238553u / 10u, 10, 0, a2);
                continue;
            }
            cv_stroke(hx + ((LN(VA_HEAD + 4) * osc_sine(a)) >> 15), hy - ((LN(VA_HEAD + 4) * osc_sine(a + 0x40000000u)) >> 15),
                      px, py, ux_mix(T_SURF, ink, 35));
            if (gld) {                                                    /* GLD: round the head, through the dot */
                int32_t s = (LN(VA_GLD0) + gld * LN(VA_GLD) / 127) * VA_RAD / r * 1000;   /* (from past the dot) */
                gn_arc(hx, hy, r, r, a - 0x40000000u - (uint32_t)s, (uint32_t)s / 4u, 8, 0, a1);
            }
        }
}
/* MRC: the SLICER's ring. Step i, a sixteenth of it, clockwise from 12 o'clock, radii r0..r1 px; a 2 px gap between
 * every two steps. Each pixel's coverage is its distance (1/16 px) to the nearest of the four edges,
 * mixed over the canvas as cv_dot does; hatch keeps the diagonals only. A step never crosses an axis: its corners
 * bound it */
#define SLR_CX 120
#define SLR_CY 70
#define SLR_R0 34                                    /* the ring */
#define SLR_R1 60
#define SLR_GAP 16                                   /* half a gap, 1/16 px */
#define SLR_ARC0 63                              /* the step playing: an arc round the ring */
#define SLR_ARC1 66
static void sl_wedge(int32_t r0, int32_t r1, uint32_t i, int hatch, uint16_t c)
{
    uint32_t a = i << 28, b = (i + 1u) << 28, k;
    int32_t sa = osc_sine(a), ca = osc_sine(a + 0x40000000u), sb = osc_sine(b), cb = osc_sine(b + 0x40000000u);
    int32_t x0 = SLR_CX, x1 = SLR_CX, y0 = SLR_CY, y1 = SLR_CY, x, y;
    for (k = 0; k < 4u; k++) {
        int32_t r = k & 1u ? r1 : r0, cx = SLR_CX + (r * (k & 2u ? sb : sa) >> 15), cy = SLR_CY - (r * (k & 2u ? cb : ca) >> 15);
        x0 = k && x0 < cx ? x0 : cx;
        x1 = k && x1 > cx ? x1 : cx;
        y0 = k && y0 < cy ? y0 : cy;
        y1 = k && y1 > cy ? y1 : cy;
    }
    for (y = y0 - 1; y <= y1 + 1; y++)
        for (x = x0 - 1; x <= x1 + 1; x++) {
            int32_t dx = x - SLR_CX, dy = y - SLR_CY, rr = dx * dx + dy * dy, yy = y + cv_oy, d, e;
            uint32_t p;
            if (rr <= (r0 - 1) * (r0 - 1) || rr >= (r1 + 1) * (r1 + 1) || (uint32_t)x >= cv_w || (uint32_t)yy >= cv_h ||
                (hatch && ((x + y) & 3)))
                continue;
            d = ((dx * ca + dy * sa) >> 11) - SLR_GAP;
            e = (-(dx * cb + dy * sb) >> 11) - SLR_GAP;
            d = d < e ? d : e;
            e = (r1 * r1 - rr) * 8 / r1;
            d = d < e ? d : e;
            e = (rr - r0 * r0) * 8 / r0;
            d = (d < e ? d : e) + 8;
            if (d <= 0)
                continue;
            p = (uint32_t)yy * cv_w + (uint32_t)x;
            cv_px[p] = swap16(d >= 16 ? c : ux_mix(swap16(cv_px[p]), c, d * 100 / 16));
        }
}
/* SLICER page: the pattern's 16 steps round a ring, a 'x' step whole; a '.' step: GATE as deep as it stays open from
 * the ring's inside (DEPTH, absent at 100 %), STUT hatched (it repeats the last 'x'); an arc outside the ring on the
 * step playing. Grey when OFF. */
static void graph_slicer(const track_t *t, uint16_t c)
{
    uint32_t i, pat = sl_pattern(t), mode = (uint32_t)t->p[P_SLCR], cur = sl[t - trk].idx;
    int32_t open = (SLR_R1 - SLR_R0) - t->p[P_SLDEPTH] * (SLR_R1 - SLR_R0) / 127;   /* px a closed GATE step keeps */
    uint16_t col = mode == SL_OFF ? T_DIM : c;
    for (i = 0; i < 16u; i++) {
        if ((pat >> i) & 1u)
            sl_wedge(SLR_R0, SLR_R1, i, 0, mode == SL_OFF ? T_RAISE : col);
        else if (mode == SL_STUT)
            sl_wedge(SLR_R0, SLR_R1, i, 1, col);
        else if (open > 0)
            sl_wedge(SLR_R0, SLR_R0 + open, i, 0, T_RAISE);
    }
    if (mode != SL_OFF)
        sl_wedge(SLR_ARC0, SLR_ARC1, cur, 0, T_ACCENT);
}
/* MOD page: the four slots as rows "1 LFO > CUT +50%" (M, a list's rows: the source to x 78, the arrow 84..98, the
 * destination of up to 5 characters 106..160, the amount from 172), the one KNOB 2..4 edit selected, slots that do
 * nothing (SRC, DST or AMT at 0) dim */
static void graph_mod(const track_t *t, uint16_t c)
{
    uint32_t k;
    for (k = 0; k < NMSLOT; k++) {
        const int16_t *p = &t->p[P_M1SRC + 3u * k];
        int32_t y = 14 + (int32_t)k * 24, on = p[0] && p[1] && p[2], sel = k == mod_ui_slot, x;
        uint16_t bg = sel ? T_THEME : T_SURF, col = sel ? T_INK : on ? c : T_DIM;
        char b[8];
        const char *unit;
        if (sel)
            cv_rrect(6, y, 228, 17, 4, T_THEME, T_SURF);
        b[0] = (char)('1' + k);
        b[1] = 0;
        GFX_HOOK_ALIGN(0, y, 0, y + 17, AL_V, "mod row text centred up/down");
        cv_text_on(14, y + CAP_IN(M, 17), &AF_M, b, sel ? T_INK : T_MID, bg);
        GFX_HOOK_ALIGN(0, y, 0, y + 17, AL_V, "mod row text centred up/down");
        cv_text_on(30, y + CAP_IN(M, 17), &AF_M, N_MSRC[clamp(p[0], 0, MS_N - 1)], col, bg);
        cv_line(84, y + 8, 98, y + 8, col);              /* an arrow */
        cv_line(94, y + 4, 98, y + 8, col);
        cv_line(94, y + 12, 98, y + 8, col);
        cv_text_on(106, y + CAP_IN(M, 17), &AF_M, mod_dst_name(t, p[1]), col, bg);
        param_format(&TP[P_M1AMT], p[2], b, &unit);
        GFX_HOOK_ALIGN(0, y, 0, y + 17, AL_V, "mod row text centred up/down");
        x = cv_text_on(172, y + CAP_IN(M, 17), &AF_M, b, col, bg) + 2;
        GFX_HOOK_ALIGN(0, 0, 0, y + CAP_IN(M, 17) + AF_M.asc, AL_B, "mod row unit on its amount's baseline");
        cv_text_on(x, y + CAP_IN(M, 17) + AF_M.asc - AF_S.asc, &AF_S, unit, sel ? T_INK : T_DIM, bg);   /* (a unit: S) */
    }
}

static uint32_t steps_hash(const track_t *t)
{
    uint32_t h = 2166136261u, i;
    for (i = 0; i < NSTEP; i++) {
        const step_t *st = &seq_steps(t)[i];
        h = (h ^ (st->note[0] + st->n * 128u + st->time * 1024u + st->flags * 4096u + st->note[1] * 65536u)) *
            16777619u;
        h = (h ^ (st->hit | (uint32_t)st->acc << 8 | (uint32_t)st->note[2] << 16 | (uint32_t)st->note[3] << 24)) * 16777619u;
        h = (h ^ (st->vel | (uint32_t)st->probability << 8)) * 16777619u;   /* (1.5.1: DETAIL, the marks) */
    }
    return h;
}

static uint32_t str_hash(uint32_t h, const char *s)
{
    while (*s)
        h = (h ^ (uint8_t)*s++) * 16777619u;
    return h;
}

/* One shared, reduced sample display. Decode only in the UI/main loop with a
 * private IMA state, 512 samples per frame; no audio voice state is touched.
 * Cache follows the sample zone, selected note and user-slot generation. */
#define SAMPLE_WAVE_COLS 96u
static struct {
    int16_t lo[SAMPLE_WAVE_COLS], hi[SAMPLE_WAVE_COLS];
    const smp_zone_t *zone;
    uint32_t key, gen, pos;
    int32_t pred, index, peak;
    uint8_t ready;
} sample_wave;

static uint32_t sample_wave_zone(const track_t *t)
{
    uint32_t si = (uint32_t)t->p[P_E0] % SMP_NALL, i, zi = 0xFFFFu;
    if (si < SMP_NSETS) {
        const smp_set_t *set = &SMP_SETS[si];
        for (i = 0; i < set->nz; i++)
            if (last_note >= SMP_ZONES[set->z0 + i].lo && last_note <= SMP_ZONES[set->z0 + i].hi) zi = set->z0 + i;
        if (zi == 0xFFFFu && set->nz) zi = set->z0;
    } else {
        uint32_t k = si - SMP_NSETS;
        for (i = 0; i < usr_nz[k]; i++)
            if (last_note >= usr_zone[k][i].lo && last_note <= usr_zone[k][i].hi) zi = 0x8000u | k << 5 | i;
        if (zi == 0xFFFFu && usr_nz[k]) zi = 0x8000u | k << 5;
    }
    return zi;
}

static void sample_wave_tick(const track_t *t)
{
    uint32_t zi = sample_wave_zone(t), key = zi + ((uint32_t)last_note << 16), i;
    if (key != sample_wave.key || sample_wave.gen != smp_user_gen || !sample_wave.zone) {
        sample_wave.key = key; sample_wave.gen = smp_user_gen;
        sample_wave.zone = zi == 0xFFFFu ? 0 : smp_zone(zi);
        sample_wave.pos = 0; sample_wave.pred = sample_wave.index = 0;
        sample_wave.peak = 1; sample_wave.ready = 0;
        for (i = 0; i < SAMPLE_WAVE_COLS; i++) sample_wave.lo[i] = sample_wave.hi[i] = 0;
    }
    const smp_zone_t *z = sample_wave.zone;
    if (!z || !z->n || sample_wave.ready) return;
    for (i = 0; i < 512u && sample_wave.pos < z->n; i++) {
        uint32_t pos = sample_wave.pos, b = SMP_DATA[z->off + (pos >> 1)];
        uint32_t code = (pos & 1u) ? b >> 4 : b & 15u;
        int32_t step = IMA_STEP[sample_wave.index], d = step >> 3;
        if (code & 4u) d += step;
        if (code & 2u) d += step >> 1;
        if (code & 1u) d += step >> 2;
        sample_wave.pred = clamp(sample_wave.pred + ((code & 8u) ? -d : d), -32768, 32767);
        sample_wave.index = clamp(sample_wave.index + IMA_IDX[code & 7u], 0, 88);
        uint32_t col = pos * SAMPLE_WAVE_COLS / z->n;
        int32_t v = sample_wave.pred, a = v < 0 ? -v : v;
        if (v < sample_wave.lo[col]) sample_wave.lo[col] = (int16_t)v;
        if (v > sample_wave.hi[col]) sample_wave.hi[col] = (int16_t)v;
        if (a > sample_wave.peak) sample_wave.peak = a;
        sample_wave.pos++;
    }
    sample_wave.ready = sample_wave.pos == z->n;
}

static void graph_sample(uint16_t c)
{
    uint32_t i;
    const smp_zone_t *z = sample_wave.zone;
    cv_rect(PANEL_X0, 48, PANEL_W, 1, T_RAISE);
    for (i = 0; i < SAMPLE_WAVE_COLS; i++) {
        int32_t x = 12 + (int32_t)i * 216 / (SAMPLE_WAVE_COLS - 1u);
        cv_line(x, 48 - sample_wave.hi[i] * 38 / sample_wave.peak,
                x, 48 - sample_wave.lo[i] * 38 / sample_wave.peak, c);
    }
    if (z->looped && TSEL->p[P_E3]) {
        int32_t left = 12 + (int32_t)(z->ls * 216u / z->n), right = 12 + (int32_t)(z->le * 216u / z->n);
        cv_line(left, 4, left, 90, T_MID); cv_line(right, 4, right, 90, T_MID);
    }
}

#if FELUCCA_SLICE
/* SLICES (ui_slice.c): the waveform from the marker before the selected one to the one after it (the selected
 * slice tinted, its marker the accent, outside the slices DIM), under it the whole material with every marker and
 * the view; then the selected slice's length and the source */
static void graph_slices(void)
{
    uint32_t a, b, len, n = slice_count(), j = slice_sel(), i, src, div, ma, mb;
    char t[20], u[12];
    if (!slice_src(&src, &div) || !n) {             /* (it plays a sine: eng_slice.c) */
        panel_alert(ICON_X_NOFILE, "SAMPLE NOT FOUND");
        return;
    }
    slice_view(&a, &b, &len);
    slice_env(a, b);
    ma = slice_mark(0);
    mb = slice_mark(n);
#define SPX(p) (12 + (int32_t)((uint32_t)((p) - a) * SP_COLS / (b - a)))   /* p in [a, b] */
    if (j < n) {                                     /* the selected slice */
        uint32_t s0 = slice_mark(j), s1 = slice_mark(j + 1u);
        int32_t x0 = SPX(s0 > a ? s0 : a), x1 = SPX(s1 < b ? s1 : b);
        if (x1 > x0)
            cv_rect(x0, 6, x1 - x0, 80, T_TINT);
    }
    for (i = 0; i < SP_COLS; i++) {
        uint32_t p = a + (uint32_t)((uint32_t)i * (b - a) / SP_COLS);
        cv_line(12 + (int32_t)i, 46 - sp.hi[i] * 38 / 64, 12 + (int32_t)i, 46 - sp.lo[i] * 38 / 64,
                p >= ma && p < mb ? T_THEME : T_DIM);
    }
    for (i = 0; i <= n; i++) {                       /* the markers in the view (the selected one last: on top) */
        uint32_t p = slice_mark(i);
        if (i != j && p >= a && p <= b)
            cv_rect(SPX(p), 4, 1, 84, T_MID);
    }
    {
        uint32_t p = slice_mark(j);
        if (p >= a && p <= b)
            cv_rect(SPX(p) > 226 ? 226 : SPX(p), 2, 2, 88, T_ACCENT);
    }
#undef SPX
    cv_rrect(12, 96, (int32_t)SP_COLS, 3, 1, T_RAISE, T_SURF);   /* the whole: every marker, the view */
    for (i = 0; i <= n; i++)
        cv_rect(12 + (int32_t)((uint32_t)slice_mark(i) * (SP_COLS - 1u) / len), 94, 1, 7, i == j ? T_ACCENT : T_MID);
    {
        int32_t x0 = 12 + (int32_t)((uint32_t)a * SP_COLS / len), x1 = 12 + (int32_t)((uint32_t)b * SP_COLS / len);
        cv_rect(x0, 103, x1 - x0 < 2 ? 2 : x1 - x0, 2, T_THEME);
    }
    str_cpy(t, "LEN ", sizeof t);
    slice_time(u, j < n ? slice_mark(j + 1u) - slice_mark(j) : len - mb);
    str_cpy(t + 4, u, sizeof t - 4);
    str_cpy(t + str_len(t), " S", sizeof t - str_len(t));
    cv_text(12, 104, &AF_M, t, T_MID);                  /* the slice's detail (M): its capitals rows 108 .. 118 */
    cv_text_r(228, 104, &AF_M, N_SLC_SRC[src], src && src != SLC_SRC_PIANO ? T_THEME : T_DIM, T_SURF);
}
#endif

/* MRC: the EDIT pages' drawings (ANALOG, LOFI, WHEEL; TRIO's moves), from the knobs alone, drawn at cv_oy 0. In the first
 * palette (MRC_ON) a part takes the brand accent of the knob that moves it, the same on both pages (knobs 1 and 5
 * the first, 2 and 6 the second, ...); in every other palette all of it takes c */
static uint16_t ed_acc(uint32_t k, uint16_t c) { return MRC_ON ? MRC_ACCENT[k % MRC_ACCENTS] : c; }
static uint32_t ed_hash(uint32_t n)                  /* 0..255, scattered */
{
    n *= 2654435761u;
    n ^= n >> 13;
    n *= 0x5BD1E995u;
    return n >> 24;
}
/* exp(-(d / 5 px)^2) in Q8, d in 1/16 px (the bumps: ANALOG's resonance, TRIO's crest) */
static int32_t ed_bump(int32_t d)
{
    static const uint8_t G[13] = {255, 246, 218, 179, 135, 94, 61, 36, 20, 10, 5, 2, 1};
    int32_t i;
    d = d < 0 ? -d : d;
    i = d >> 4;
    return i >= 12 ? 0 : G[i] + (G[i + 1] - G[i]) * (d & 15) / 16;
}
/* a rectangle (1/16 px) with corners rounded r, each corner three strokes */
static void ed_rrect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint16_t c)
{
    static const int16_t COS[12] = {256, 222, 128, 0, -128, -222, -256, -222, -128, 0, 128, 222};
    int32_t q, j, px = 0, py = 0, fx = 0, fy = 0;
    for (q = 0; q < 4; q++)                          /* clockwise from the right edge's foot */
        for (j = 0; j <= 3; j++) {
            int32_t a = q * 3 + j, cx = q == 0 || q == 3 ? x + w - r : x + r, cy = q < 2 ? y + h - r : y + r;
            int32_t px1 = cx + r * COS[a % 12] / 256, py1 = cy + r * COS[(a + 9) % 12] / 256;
            if (q || j)
                cv_stroke(px, py, px1, py1, c);
            else {
                fx = px1;
                fy = py1;
            }
            px = px1;
            py = py1;
        }
    cv_stroke(px, py, fx, fy, c);
}

/* ANALOG: a ridge of eight cycles of WAVE in perspective, back to front, each dimmer the further back. MIX turns the
 * back lines to osc 2 (the second accent), DTN slides each one further out of phase, NOIS roughens them. The filter
 * cuts across the ridge: past CUT the lines fall away over AN_FALL, RES raising a crest at the edge, DRV flattening
 * the tops, KTR slanting the edge from back to front; the edge dotted, in the fourth accent */
#define AN_N 60                                      /* segments a line */
#define AN_FALL 22                                   /* px past the cut to flat */
static int32_t an_wave(int32_t w, uint32_t ph)       /* Q15, ph's low 16 bits the cycle */
{
    int32_t f = (int32_t)(ph & 0xFFFFu);
    switch (w) {
    case 0: return 32767 - f;                                          /* SAW */
    case 1: return f < 32768 ? 32767 : -32767;                         /* SQR */
    case 2: return f < 32768 ? f * 2 - 32767 : 98303 - f * 2;          /* TRI */
    case 3: return osc_sine((uint32_t)f << 16);                        /* SIN */
    case 4: return f < 16384 ? 32767 : -32767;                         /* PWM */
    case 5: return 32767 - ((f * 166 / 64) & 0xFFFF);                  /* SYNC */
    default: return ((32767 - f) >> 1) + (f < 32768 ? 16383 : -16383); /* SUB */
    }
}
static void graph_analog(const track_t *t, uint16_t c)
{
    const int16_t *p = t->p;
    int32_t w = clamp(p[P_E0], 0, 6), dtn = clamp(p[P_E1], 0, 127), nb = (clamp(p[P_E2], 0, 127) * 8 + 63) / 127;
    int32_t nois = clamp(p[P_E3], 0, 127), cut = clamp(p[P_E4], 0, 127), res = clamp(p[P_E5], 0, 127);
    int32_t drv = clamp(p[P_E6], 0, 127), ktr = clamp(p[P_E7], 0, 127), ex[8], ey[8], i, n, k;
    uint16_t ec = ux_mix(T_SURF, ed_acc(3u, c), 80);
    for (i = 0; i < 8; i++) {
        int32_t y0 = LN(26 + 12 * i), x0 = LN(20 + (7 - i) * 7), x1 = LN(220 - (7 - i) * 7), amp = LN(4) + i * LN(4) / 7;
        int32_t cx = x0 + cut * (x1 - x0) / 127 + (ktr - 64) * (2 * i - 7) * LN(5) / 128, px = 0, py = 0;
        uint16_t col = ux_mix(T_SURF, ed_acc(i < nb ? 1u : 0u, c), 35 + 65 * i / 7);
        ex[i] = cx;
        ey[i] = y0;
        for (n = 0; n <= AN_N; n++) {
            int32_t x = x0 + (x1 - x0) * n / AN_N, d = x - cx, g, v, y;
            v = clamp(an_wave(w, (uint32_t)(n * 3 * 65536 / AN_N + i * dtn * 77)) * (25 + drv) / 25, -32767, 32767);
            v += ((int32_t)ed_hash((uint32_t)(i * 131 + n)) - 128) * nois;
            g = d <= 0 ? 256 : d >= LN(AN_FALL) ? 0 : 256 - d * 256 / LN(AN_FALL);
            g = g * (256 + ((res * 461 / 127 * ed_bump(d)) >> 8)) >> 8;   /* (RES 127: 1.8 x at the edge) */
            y = y0 - (((v * amp) >> 15) * g >> 8);
            if (n)
                cv_stroke(px, py, x, y, col);
            px = x;
            py = y;
        }
    }
    for (i = 0; i < 7; i++)                          /* the edge, dotted: every other sixth */
        for (k = 0; k < 6; k += 2)
            cv_stroke(ex[i] + (ex[i + 1] - ex[i]) * k / 6, ey[i] + (ey[i + 1] - ey[i]) * k / 6,
                      ex[i] + (ex[i + 1] - ex[i]) * (k + 1) / 6, ey[i] + (ey[i + 1] - ey[i]) * (k + 1) / 6, ec);
}

/* LOFI: a dot-matrix screen, the wave drawn in its cells. CHIP sets the grid (1BIT two rows, 4BIT coarse, 8BIT fine,
 * STEP held in threes), WAVE the shape (PLS a square), DUTY rounding every cell from a square to a disc, CRSH knocking
 * cells out. SWP squeezes the cycles towards the right, VIB wiggles them, ARP stacks shifted copies (the third
 * accent), TONE dims the upper rows. The grid's dots in the first accent, faint */
#define LFM_X 20                                     /* the screen: 200 px from LFM_X, LFM_H up from LFM_Y */
#define LFM_Y 118
#define LFM_H 96
static void graph_lofi(const track_t *t, uint16_t c)
{
    static const uint8_t ROWS[5] = {16, 8, 24, 2, 12};
    static const uint8_t ARP[4][2] = {{0, 0}, {30, 0}, {12, 22}, {10, 22}};   /* the copies, % of the rows up */
    const int16_t *p = t->p;
    int32_t chip = clamp(p[P_E0], 0, 4), w = clamp(p[P_E1], 0, 4), duty = clamp(p[P_E2], 0, 127);
    int32_t crsh = clamp(p[P_E3], 0, 127), swp = clamp(p[P_E4], 0, 127), vib = clamp(p[P_E5], 0, 127);
    int32_t arp = clamp(p[P_E6], 0, 3), tone = clamp(p[P_E7], 0, 127);
    int32_t rows = ROWS[chip], ph = LFM_H / rows, cw = ph < 6 ? ph : 6, nc = 200 / cw, r, k, j;
    int32_t rr = duty * (LN(cw < ph ? cw : ph) / 2 - 6) / 127;   /* the cells' corners */
    uint16_t dots = ux_mix(T_SURF, ed_acc(0u, c), 30), cp = ux_mix(T_SURF, ed_acc(2u, c), 60);
    for (r = 0; r <= rows; r++)
        for (k = 0; k <= nc; k++)
            if (rows <= 12 || !(k & 1))
                cv_rect(LFM_X + k * cw, LFM_Y - r * ph, 1, 1, dots);
    for (k = 0; k < nc; k++) {
        int32_t tq = (chip == 4 ? k / 3 * 3 : k) * 65536 / nc, q = (tq * swp * 13) >> 10, row, v, f;
        int32_t pq = 3 * tq + (((3 * tq) >> 4) * (q >> 4) >> 8) + osc_sine((uint32_t)tq * 417221u) * vib / 907;
        f = pq & 0xFFFF;
        v = w == 0 ? (f < 32768 ? 32767 : -32767)                                  /* PLS */
          : w == 1 ? (f < 32768 ? f * 2 - 32767 : 98303 - f * 2)                   /* TRI */
          : w == 2 ? 32767 - f                                                     /* SAW */
          : w == 3 ? ((int32_t)ed_hash((uint32_t)pq >> 12) - 128) * 256           /* NOIS */
          : osc_sine((uint32_t)f << 16);                                           /* WRAM */
        if ((int32_t)ed_hash((uint32_t)(k * 7 + 1)) < crsh * 153 / 127)
            continue;                                /* crushed out */
        row = ((v + 32768) * (rows - 1) + 32767) / 65535;
        ed_rrect(LN(LFM_X + k * cw) + 6, LN(LFM_Y - (row + 1) * ph) + 6, LN(cw) - 12, LN(ph) - 12, rr,
                 ux_mix(T_SURF, ed_acc(1u, c), 100 - (127 - tone) * 85 * row / (127 * (rows > 1 ? rows - 1 : 1))));
        for (j = 0; j < 2 && ARP[arp][j]; j++) {
            int32_t r2 = row + (ARP[arp][j] * rows + 50) / 100;
            if (r2 < rows)
                ed_rrect(LN(LFM_X + k * cw) + 6, LN(LFM_Y - (r2 + 1) * ph) + 6, LN(cw) - 12, LN(ph) - 12, rr, cp);
        }
    }
}

static const uint16_t ED_OCT[25] = {0, 7, 15, 23, 31, 40, 48, 58, 67, 76, 86, 96, 106, 117, 128,
                                    139, 150, 163, 175, 188, 200, 214, 227, 241, 256};   /* 2^(s / 24) - 1, Q8 (TRIO, GRAIN) */
/* TRIO: MOONS, from the knobs, moving on the audio clock (fx.dly_w; no sound moves it). A moon an oscillator (osc 1 in
 * the middle, osc 2 left, osc 3 right) in its knob's accent (WAVE's, INT2's, INT3's), set at its pitch (C3 at 48 px,
 * 0.9 px a semitone up) and the smaller the higher (2^(-st / 30)); its rim is its wave wrapped round it, six of osc 1's
 * cycles a turn and the others' as many as their interval gives, drawn with the engine's own waves (trio_naive,
 * TRIO_SETS, PW as the engine takes it) through the engine's own filter (tsvf_coef_k, MODE, CUT, RES) at the moon's
 * pitch, so LP rounds it, HP leaves the spikes. An oscillator off: a dotted new moon. SYNC: osc 2's rim restarts with
 * osc 1's cycles; RING: a ring round osc 1's moon, dashed by osc 3's top phase bit (third accent), sliding with DTN.
 * DTN: a faint double of each moon, a tenth larger, spinning the faster the wider the detune (osc 3's the other way;
 * none at 0). Under them the sea: the mix in six rows, nearer ones taller and quicker, rolling left, in the fourth
 * accent, with the filter laid across it: left to right CUT's range (30 Hz .. 16 kHz), each row's swell the filter's
 * gain there (the analogue SVF's), RES's crest in the third accent; the cut a dotted line in the second (BP, NOT: the
 * band's two edges, 14 px either side). Moonlight glitters under each moon that shines. Prototype: dev/trio_visuals.html */
#define MN_HZ 94                                     /* the horizon, px */
#define MN_N 120                                     /* points a rim */
#define MN_SPC 32                                    /* the rims' samples an oscillator cycle */
#define MN_CUT 12190                                 /* 32 samples a cycle of C3 against FS, in CUT's units << 8:
                                                      * log2(44100 / (32 x 130.81)) x 127 / log2(16000 / 30) */
#define MN_SEMI 299                                  /* a semitone in CUT's units << 8 */
#define MN_BAND 14                                   /* BP, NOT: the band's half width, px */
static const uint32_t MN_START[3] = {0u, 0x1C000000u, 0x38000000u};   /* trio_note_on's start phases */
static const uint8_t MN_ROW[6][2] = {{99, 19}, {104, 29}, {110, 42}, {117, 54}, {125, 70}, {134, 86}};   /* y px, swing 1/16 px */
static int32_t mn_pow2(int32_t q)                    /* 2^(q / 24), Q8 */
{
    int32_t o = q >= 0 ? q / 24 : -((23 - q) / 24), v = (int32_t)ED_OCT[q - o * 24] + 256;
    o = clamp(o, -7, 7);
    return o >= 0 ? v << o : v >> -o;
}
/* oscillator j at phase ph (osc 3's p3 for RING; nz: the noise's 16th of a cycle) at full scale, Q15 */
static int32_t mn_osc(const trio_set_t *s, uint32_t j, uint32_t ph, uint32_t p3, uint32_t nz, const trio_pw_t *q)
{
    int32_t v;
    if (j == 0u && (s->flags & TRIO_RING))
        return (p3 & 0x80000000u) ? -osc_tri(ph) : osc_tri(ph);
    if (s->w[j] == TW_NOI)
        return ((int32_t)ed_hash(nz + j * 977u) - 128) * 256;
    v = trio_naive(s->w[j], ph, q);
    return clamp(v, -32767, 32767);
}
/* the filter's gain at w (Q8, its frequency over the cut), MODE, damping k (Q12): the analogue SVF's, Q8, at most 2.4 */
static int32_t mn_gain(int32_t w, int32_t mode, int32_t k)
{
    int64_t w2 = (int64_t)w * w, re = 65536 - w2, im = ((int64_t)k * w) >> 4, num;
    uint32_t den = px_isqrt64((uint64_t)(re * re + im * im));
    num = mode == 0 ? 65536 : mode == 1 ? (int64_t)w * 351 : mode == 2 ? w2 : (re < 0 ? -re : re);   /* (BP x 1.37) */
    return den ? (int32_t)(num * 256 / den < 614 ? num * 256 / den : 614) : 614;
}
static void graph_trio(const track_t *t, uint16_t c)
{
    const int16_t *p = t->p;
    const trio_set_t *s = &TRIO_SETS[(uint32_t)clamp(p[P_E0], 0, 15)];
    int32_t duty = clamp((clamp(p[P_E7], 0, 127) << 8) - (64 << 8), 768, 32000);
    int32_t mode = clamp(p[P_E4], 0, 3), cut = clamp(p[P_E5], 0, 127), dtn = clamp(p[P_E3], 0, 50);
    int32_t k = 8192 - clamp(p[P_E6], 0, 127) * 7168 / 127;   /* eng_trio's damping, Q12 */
    int32_t st[3], r[3], ks[3], mx[3], my[3], mr[3], gn[81], i, j, q, ri, cx;
    uint32_t now = fx.dly_w, off3 = 0u - now * (uint32_t)(34087 * dtn / 50);   /* the ring slides with DTN */
    uint16_t ink = MRC_ON ? T_TEXT : c;
    trio_pw_t pw;
    pw.pw = (uint32_t)duty << 17;
    pw.pdc = 2 * duty - 32768;
    pw.psm = 32768 - ((duty * duty) >> 15);
    for (j = 0; j < 3; j++) {
        st[j] = j ? clamp(p[P_E1 + j - 1], -24, 24) : 0;
        r[j] = mn_pow2(st[j] * 2);                   /* the interval, Q8 */
        ks[j] = j == 1 && (s->flags & TRIO_SYNC) ? 6 : clamp((6 * r[j] + 128) >> 8, 2, 24);   /* cycles a turn */
        mx[j] = LN(j == 0 ? 120 : j == 1 ? 58 : 182);
        my[j] = LN(48) - st[j] * LN(9) / 10;
        q = st[j] * 4 / 5;                           /* 2^(st / 30): st x 0.8 / 24 */
        mr[j] = clamp(st[j] >= 0 ? LN(15) * 256 / mn_pow2(q) : LN(15) * mn_pow2(-q) / 256, LN(7), LN(21));
    }
    ks[0] = 6;
    for (i = 0; i < 34; i++) {                       /* the stars, none on a moon */
        int32_t x = 4 + (int32_t)ed_hash((uint32_t)(i * 3 + 9)) * 232 / 255, y = 3 + (int32_t)ed_hash((uint32_t)(i * 5 + 4)) * (MN_HZ - 8) / 255;
        for (j = 0; j < 3; j++) {
            int32_t dx = LN(x) - mx[j], dy = LN(y) - my[j], rr = mr[j] + LN(4);
            if (dx * dx + dy * dy < rr * rr) break;
        }
        if (j == 3)
            cv_rect(x, y, 1, 1, ux_mix(T_SURF, T_MID, 25 + (int32_t)(ed_hash((uint32_t)i) % 45u)));
    }
    for (j = 0; j < 3; j++) {
        int32_t rim[MN_N], nt = MN_SPC * ks[j], nx = 0, v1, v2, ic1 = 0, ic2 = 0, half;
        uint32_t st3 = (uint32_t)(((uint64_t)ks[2] << 32) / (uint32_t)nt), rate;
        tsvf_t f;
        if (j == 0 && (s->flags & TRIO_RING)) {      /* the ring's back half, behind the moon */
            for (i = 0; i < 72; i++) {
                uint32_t a = (uint32_t)i * 59652323u, b = a + 59652323u;
                int32_t ca = osc_sine(a + 0x40000000u), sa = osc_sine(a), cb = osc_sine(b + 0x40000000u), sb = osc_sine(b);
                int32_t ax = mx[0] + (mr[0] * 21 / 10 * ca >> 15), ay = my[0] + (mr[0] * 42 / 100 * sa >> 15) - (mr[0] * 35 / 100 * ca >> 15);
                int32_t bx = mx[0] + (mr[0] * 21 / 10 * cb >> 15), by = my[0] + (mr[0] * 42 / 100 * sb >> 15) - (mr[0] * 35 / 100 * cb >> 15);
                int32_t dx = (ax + bx) / 2 - mx[0], dy = (ay + by) / 2 - my[0], rr = mr[0] + LN(3);
                if (sa >= 0 || dx * dx + dy * dy < rr * rr || (((uint32_t)i * (uint32_t)(((uint64_t)ks[2] << 31) / 72u) + off3) & 0x80000000u))
                    continue;
                cv_stroke(ax, ay, bx, by, ux_mix(T_SURF, ed_acc(2u, c), 45));
            }
        }
        if (s->w[j] == TW_OFF) {                     /* off: a new moon, dotted */
            gn_arc(mx[j], my[j], mr[j], mr[j], 0u, 0xFFFFFFFFu / 40u, 40, 1, ux_mix(T_SURF, ed_acc((uint32_t)j, c), 30));
            continue;
        }
        tsvf_coef_k(&f, (cut << 8) + MN_CUT - st[j] * MN_SEMI, k);   /* the filter at the moon's pitch */
        for (i = 0; i < 4 * nt; i++) {               /* three turns to settle, the fourth kept */
            uint32_t ph = (uint32_t)i << 27, p3 = j == 0 ? MN_START[2] + (uint32_t)i * st3 : 0u, ph1 = ph;
            int32_t x, y, v3;
            if (j == 1 && (s->flags & TRIO_SYNC))
                ph = (ph1 >> 8) * (uint32_t)r[1];    /* (ks[1] is osc 1's six: i counts osc 1's samples) */
            else
                ph += MN_START[j];
            x = mn_osc(s, (uint32_t)j, ph, p3, (uint32_t)(i % nt) >> 1, &pw) >> 1;
            v3 = x - ic2;
            v1 = (f.a1 * ic1 + f.a2 * v3) >> 13;
            v2 = ic2 + ((f.a2 * ic1 + f.a3 * v3) >> 13);
            ic1 = clamp(2 * v1 - ic1, -150000, 150000);
            ic2 = clamp(2 * v2 - ic2, -150000, 150000);
            y = mode == 0 ? v2 : mode == 1 ? v1 * 1400 >> 10 : mode == 2 ? x - (k * v1 >> 12) - v2 : x - (k * v1 >> 12);
            y = clamp(y * 2, -58982, 58982);         /* (1.8) */
            while (i >= 3 * nt && nx < MN_N && i - 3 * nt == nx * nt / MN_N)
                rim[nx++] = y;
        }
        rate = (uint32_t)(58434 * dtn * (100 + 35 * j) / 5000);   /* the double: 0.6 turn a second at DTN 50 */
        for (half = dtn ? 0 : 1; half < 2; half++) { /* the double, faint, then the moon */
            uint32_t rot = half ? 0u : j == 2 ? 0u - now * rate : now * rate;
            int32_t px = 0, py = 0, rad = half ? mr[j] : mr[j] * 11 / 10;
            uint16_t col = half ? ed_acc((uint32_t)j, c) : ux_mix(T_SURF, ed_acc((uint32_t)j, c), 30);
            for (q = 0; q <= MN_N; q++) {
                uint32_t a = (uint32_t)q * 35791394u + rot + 0xC0000000u;   /* from the top */
                int32_t rr = rad + (rim[q % MN_N] * 42 >> 15), x = mx[j] + (rr * osc_sine(a + 0x40000000u) >> 15);
                int32_t y = my[j] + (rr * osc_sine(a) >> 15);
                if (q)
                    cv_stroke(px, py, x, y, col);
                px = x;
                py = y;
            }
        }
    }
    if (s->flags & TRIO_RING)                        /* the ring's front half, over the moon */
        for (i = 0; i < 72; i++) {
            uint32_t a = (uint32_t)i * 59652323u, b = a + 59652323u;
            int32_t ca = osc_sine(a + 0x40000000u), sa = osc_sine(a), cb = osc_sine(b + 0x40000000u), sb = osc_sine(b);
            if (sa < 0 || (((uint32_t)i * (uint32_t)(((uint64_t)ks[2] << 31) / 72u) + off3) & 0x80000000u))
                continue;
            cv_stroke(mx[0] + (mr[0] * 21 / 10 * ca >> 15), my[0] + (mr[0] * 42 / 100 * sa >> 15) - (mr[0] * 35 / 100 * ca >> 15),
                      mx[0] + (mr[0] * 21 / 10 * cb >> 15), my[0] + (mr[0] * 42 / 100 * sb >> 15) - (mr[0] * 35 / 100 * cb >> 15),
                      ux_mix(T_SURF, ed_acc(2u, c), 90));
        }
    /* the sea: the filter across it, CUT's range left to right; its gain every 3 px */
    for (q = 0; q <= 80; q++)
        gn[q] = mn_gain(mn_pow2(((3 * q - 8) * 127 / 224 - cut) * 1712 / 1000), mode, k);   /* (24 / 14.02: CUT's octave) */
    cx = 8 + cut * 224 / 127;
    for (i = 97; i < 140; i += 4) {                  /* the cut dotted; BP, NOT: the band's two edges */
        if (mode == 1 || mode == 3) {
            cv_stroke(LN(cx - MN_BAND), LN(i), LN(cx - MN_BAND), LN(i + 1), ux_mix(T_SURF, ed_acc(1u, c), 70));
            cv_stroke(LN(cx + MN_BAND), LN(i), LN(cx + MN_BAND), LN(i + 1), ux_mix(T_SURF, ed_acc(1u, c), 70));
        } else
            cv_stroke(LN(cx), LN(i), LN(cx), LN(i + 1), ux_mix(T_SURF, ed_acc(1u, c), 70));
    }
    for (ri = 0; ri < 6; ri++) {
        uint64_t t0 = (uint64_t)now * (uint32_t)(24347 + 11687 * ri) + 31653908971ull * (uint32_t)ri;   /* osc 1's cycles, Q32 */
        int32_t y0 = LN(MN_ROW[ri][0]), amp = MN_ROW[ri][1], base = 30 + ri * 12, px = 0, py = 0;
        for (q = 0; q <= 80; q++) {                  /* 3 px a point, three of osc 1's cycles across */
            uint64_t tt = t0 + (uint64_t)q * 161061274u, t2 = tt * (uint32_t)r[1], t3 = tt * (uint32_t)r[2];
            uint32_t ph1 = (uint32_t)tt, p3 = (uint32_t)(t3 >> 8) + MN_START[2];
            uint32_t ph2 = (s->flags & TRIO_SYNC) ? (ph1 >> 8) * (uint32_t)r[1] : (uint32_t)(t2 >> 8) + MN_START[1];
            int32_t m = 0, y, gq;
            m += mn_osc(s, 0u, ph1, p3, (uint32_t)(tt >> 28), &pw) * s->g[0] / 24000;
            m += mn_osc(s, 1u, ph2, p3, (uint32_t)(t2 >> 36), &pw) * s->g[1] / 24000;
            m += mn_osc(s, 2u, p3, p3, (uint32_t)(t3 >> 36), &pw) * s->g[2] / 24000;
            m = clamp(m, -52428, 52428);             /* (1.6) */
            y = y0 - (((m * amp) >> 8) * gn[q] >> 15);
            if (q) {
                gq = (gn[q - 1] + gn[q]) / 2;
                cv_stroke(px, py, LN(3 * q), y, gq > 269 ? ux_mix(T_SURF, ed_acc(2u, c), base + 40 * (gq - 256) / 256 > 100 ? 100 : base + 40 * (gq - 256) / 256)
                                                     : ux_mix(T_SURF, ed_acc(3u, c), base * (51 + gq * 205 / 256) / 256));
            }
            px = LN(3 * q);
            py = y;
        }
        for (j = 0; j < 3; j++) {                    /* moonlight glittering under each moon */
            if (s->w[j] == TW_OFF)
                continue;
            for (q = 0; q < 2; q++) {
                int32_t h = (int32_t)ed_hash((uint32_t)(ri * 31 + j * 7 + q * 101) + (now + (uint32_t)q * 5439u) / 14700u * 13u), gx, gy, w;
                if (h > 150)
                    continue;
                gx = mx[j] + (h - 75) * (mr[j] * 6 / 10 + LN(2 * ri)) / 75;
                gy = LN(MN_ROW[ri][0] - 1 - (h & 1));
                w = LN(1 + (ri >> 1));
                cv_stroke(gx - w, gy, gx + w, gy, ux_mix(T_SURF, ink, 55 + h % 40));
            }
        }
    }
}
/* WHEEL: the nine drawbars hanging from their rail, pulled out to their level (drw_level: REG with SUB, BODY, TOP;
 * PERC on: the 1' bar in), each in the accent of the knob that moves it (SUB the 16' and 5 1/3', BODY the 8' and
 * 4', TOP the rest), its level under its handle. DRV turns the rods to zigzags, sharper the more; CLICK sparks at the
 * rail over every bar out; PERC fills the 4' (2ND) or 2 2/3' (3RD) handle in the first accent, SOFT hatching it
 * instead. REG, ROTR and PERC's decay draw nothing of their own */
#define WH_X 20                                      /* the first bar, then every WH_DX px */
#define WH_DX 25
#define WH_RAIL 8
#define WH_STEP 12                                   /* px a level */
static void graph_wheel(const track_t *t, uint16_t c)
{
    const int16_t *p = t->p;
    int32_t perc = (int32_t)((uint32_t)p[P_E4] % 7u), click = clamp(p[P_E5], 0, 127);
    int32_t zz = clamp(p[P_E6], 0, 127) * LN(3) / 127, k, j;
    cv_stroke(LN(6), LN(WH_RAIL), LN(234), LN(WH_RAIL), T_MID);
    for (k = 0; k < 9; k++) {
        int32_t x = LN(WH_X + k * WH_DX), lv = perc && k == 8 ? 0 : drw_level(p, (uint32_t)k), y1 = LN(WH_RAIL + 6) + lv * LN(WH_STEP);
        uint16_t col = ed_acc(k < 2 ? 1u : k < 4 ? 2u : 3u, c);
        char d[2] = {(char)('0' + lv), 0};
        if (zz) {                                    /* DRV: the rod zigzags, a tooth every 3 px */
            int32_t y = LN(WH_RAIL), s = 1;
            for (; y < y1; y += LN(3), s = -s)
                cv_stroke(x - s * zz, y, x + s * zz, y + LN(3) < y1 ? y + LN(3) : y1, col);
        } else
            cv_stroke(x, LN(WH_RAIL), x, y1, col);
        if (perc && k == (perc & 1 ? 3 : 4)) {       /* PERC: its harmonic's handle filled, hatched when SOFT */
            int32_t hx = (x >> 4) - 7, hy = y1 >> 4;
            if (perc == 3 || perc == 4)
                for (j = -4; j < 12; j += 3) {       /* diagonals up from the foot at j, a px inside the handle */
                    int32_t u0 = j < 0 ? -j : 0, u1 = 12 - j < 5 ? 12 - j : 5;
                    if (u0 < u1)
                        cv_stroke(LN(hx + 1 + j + u0), LN(hy + 6 - u0), LN(hx + 1 + j + u1), LN(hy + 6 - u1), ed_acc(0u, c));
                }
            else
                cv_rrect(hx, hy, 14, 7, 2, ed_acc(0u, c), T_SURF);
        }
        ed_rrect(x - LN(7), y1, LN(14), LN(7), LN(2), col);
        GFX_HOOK_ALIGN((x >> 4) - 7, 0, (x >> 4) + 7, 0, AL_H, "wheel level under its drawbar");
        cv_text_in((x >> 4) - 7, (y1 >> 4) + 9, 14, &AF_S, d, col, T_SURF);
        if (lv && click > 8) {                       /* CLICK: a spark where the bar leaves the rail */
            int32_t s = click * LN(7) / 254;
            cv_stroke(x - s, LN(WH_RAIL) - s, x + s, LN(WH_RAIL) + s, ed_acc(1u, c));
            cv_stroke(x - s, LN(WH_RAIL) + s, x + s, LN(WH_RAIL) - s, ed_acc(1u, c));
        }

    }
}
/* GRAIN: a saucer beaming up a cow, from the knobs, moving on the audio clock (fx.dly_w; no sound moves it): the cow
 * bobs 1.5 px either way (a cycle in 1.6 s), the saucer's lights run left to right round its rim (a turn in 4 s).
 * POS lifts the cow from the grass to under the saucer, SIZE widens the beam, DENS lights the stars (3 .. 48, every
 * fourth a cross), PTCH spaces the rings down the beam (14 px at 0, halved an octave up, doubled an octave down),
 * SPRD molecularises the cow (its outline goes to dots drifting up the beam towards the saucer, the parts nearest it
 * first, then at random), RAND tumbles the cow and wavers the beam's edges, TONE sets how far down the beam stays
 * bright. SRC: the source's name in the corner. The saucer and the grass in MID and DIM, its lights in TEXT */
#define GN_CX 120                                    /* the beam's axis */
#define GN_TOP 31                                    /* the beam: from under the saucer .. */
#define GN_GND 127                                   /* .. to the grass */
#define GN_LIFT 42                                   /* POS 127: the cow's feet this far under the beam's top, px */
#define GN_PY (-18)                                  /* the cow turns about its body's middle, px over its feet */
#define GN_BOB 60870u                                /* a cycle (2^32) in 1.6 s of samples */
#define GN_SPIN 24347u                               /* the saucer: a turn in 4 s */
#define GN_TILT 2960449                              /* RAND 127 tips the cow 0.55 rad (a turn: 2^32): / 127 */
#define GN_SPOT 13                                   /* GN_COW's polylines from here on: the coat */
/* the cow, side view facing right, feet on y 0, in 1/2 px: polylines, each its point count then its points. The
 * body, head, snout, ear, horns, legs, tail and udder, then the coat: three spots, a patch over the back, the eye */
static const int8_t GN_COW[] = {
    17, 30, -34, 29, -29, 25, -25, 20, -24, -22, -24, -27, -25, -31, -29, -32, -34, -32, -44, -31, -49, -27, -53,
        -22, -54, 20, -54, 25, -53, 29, -49, 30, -44, 30, -34,
    17, 46, -48, 45, -45, 43, -43, 40, -42, 32, -42, 29, -43, 27, -45, 26, -48, 26, -56, 27, -59, 29, -61, 32, -62,
        40, -62, 43, -61, 45, -59, 46, -56, 46, -48,
    17, 52, -42, 51, -40, 50, -39, 48, -38, 42, -38, 40, -39, 39, -40, 38, -42, 38, -46, 39, -48, 40, -49, 42, -50,
        48, -50, 50, -49, 51, -48, 52, -46, 52, -42,
    3, 28, -58, 20, -64, 26, -60,
    2, 32, -62, 30, -70,
    2, 40, -62, 42, -70,
    4, -26, -24, -26, 0, -22, 0, -22, -24,
    4, -16, -24, -16, 0, -12, 0, -12, -24,
    4, 12, -24, 12, 0, 16, 0, 16, -24,
    4, 20, -24, 20, 0, 24, 0, 24, -24,
    3, -32, -48, -38, -40, -40, -30,
    3, -42, -30, -40, -24, -38, -30,
    4, 2, -24, 4, -20, 10, -20, 12, -24,
    7, 6, -44, 5, -41, -1, -41, -2, -44, 0, -47, 4, -47, 6, -44,
    7, -11, -36, -15, -31, -21, -33, -27, -36, -22, -42, -15, -41, -11, -36,
    7, -11, -37, -16, -32, -22, -32, -28, -37, -23, -41, -14, -43, -11, -37,
    6, 6, -54, 7, -49, 12, -46, 18, -47, 21, -50, 22, -54,
    2, 40, -56, 41, -56,
};
static int32_t gn_fade(int32_t y, int32_t tone)      /* TONE: the beam's brightness at y (1/16 px), % */
{
    return 100 - (127 - tone) * 90 * (y - LN(GN_TOP)) / (127 * LN(GN_GND - GN_TOP));
}
/* n strokes of an ellipse's arc from a0, da each (a turn: 2^32; y down: a quarter turn is the bottom), centre and
 * radii in 1/16 px; dotted: every other stroke */
static void gn_arc(int32_t cx, int32_t cy, int32_t rx, int32_t ry, uint32_t a0, uint32_t da, int32_t n, int dot, uint16_t c)
{
    int32_t px = 0, py = 0, i;
    for (i = 0; i <= n; i++) {
        uint32_t a = a0 + da * (uint32_t)i;
        int32_t x = cx + (rx * osc_sine(a + 0x40000000u) >> 15), y = cy + (ry * osc_sine(a) >> 15);
        if (i && (!dot || (i & 1)))
            cv_stroke(px, py, x, y, c);
        px = x;
        py = y;
    }
}
static int32_t gn_hw(int32_t tw, int32_t bw, int32_t y)   /* the beam's half width at y, 1/16 px */
{
    return tw + (bw - tw) * (y - LN(GN_TOP)) / LN(GN_GND - GN_TOP);
}
static void graph_grain(const track_t *t, uint16_t c)
{
    const int16_t *p = t->p;
    int32_t src = clamp(p[P_E0], 0, SMP_NALL - 1), pos = clamp(p[P_E1], 0, 127), size = clamp(p[P_E2], 0, 127);
    int32_t dens = clamp(p[P_E3], 0, 127), ptch = clamp(p[P_E4], -24, 24), sprd = clamp(p[P_E5], 0, 127);
    int32_t rnd = clamp(p[P_E6], 0, 127), tone = clamp(p[P_E7], 0, 127);
    int32_t tw = LN(6) + size * LN(8) / 127, bw = LN(12) + size * LN(62) / 127, cx = LN(GN_CX), top = LN(GN_TOP);
    int32_t gnd = LN(GN_GND), ns = 3 + (dens * 45 + 63) / 127, sp, i, y, s, n, f;
    uint32_t now = fx.dly_w;
    for (i = 0; i < ns; i++) {                       /* DENS: the stars, none behind the saucer, in the beam or the name */
        int32_t x = 4 + (int32_t)ed_hash((uint32_t)(i * 3 + 9)) * 232 / 255, sy = 3 + (int32_t)ed_hash((uint32_t)(i * 5 + 4)) * 100 / 255;
        uint16_t sc = ux_mix(T_SURF, ed_acc(3u, c), 40 + (int32_t)(ed_hash((uint32_t)i) % 60u));
        if ((x > GN_CX - 37 && x < GN_CX + 37 && sy > 7 && sy < 33) || (x > 190 && sy < 18) ||
            (sy >= GN_TOP && (x < GN_CX ? GN_CX - x : x - GN_CX) * 16 <= gn_hw(tw, bw, LN(sy)) + LN(5)))   /* (5 px: RAND's waver, the cross) */
            continue;
        cv_rect(x, sy, 1, 1, sc);
        if (!(i & 3)) {                              /* every fourth a cross */
            cv_rect(x - 1, sy, 3, 1, sc);
            cv_rect(x, sy - 1, 1, 3, sc);
        }
    }
    for (s = 0; s < 2; s++) {                        /* the beam's edges, SIZE apart, RAND wavering them */
        int32_t px = 0, py = 0;
        for (y = GN_TOP; y <= GN_GND; y += 4) {
            int32_t wob = ((int32_t)ed_hash((uint32_t)((y + 2) / 4 * 2 + s * 97 + 5)) - 128) * rnd * LN(3) / (128 * 127);
            int32_t x = s ? cx + gn_hw(tw, bw, LN(y)) + wob : cx - gn_hw(tw, bw, LN(y)) - wob;
            f = gn_fade(LN(y), tone) * 85 / 100;
            if (y > GN_TOP)
                cv_stroke(px, py, x, LN(y), ux_mix(T_SURF, ed_acc(2u, c), f < 20 ? 20 : f));
            px = x;
            py = LN(y);
        }
    }
    sp = ptch >= 0 ? LN(14) * 256 / ((int32_t)ED_OCT[ptch] + 256) : LN(14) * ((int32_t)ED_OCT[-ptch] + 256) / 256;
    for (y = gnd - sp; y > top + LN(4); y -= sp) {   /* PTCH: the rings, the near half lit, the far half dotted */
        int32_t hw = gn_hw(tw, bw, y), ry = hw * 16 / 100;
        uint16_t rc = ed_acc(0u, c);
        if (ry < LN(3) / 2) ry = LN(3) / 2;
        n = hw * 12 / LN(10) < 12 ? 12 : hw * 12 / LN(10);
        f = gn_fade(y, tone);
        gn_arc(cx, y, hw, ry, 0u, 0x80000000u / (uint32_t)n, n, 0, ux_mix(T_SURF, rc, f * 55 / 100));
        gn_arc(cx, y, hw, ry, 0x80000000u, 0x80000000u / (uint32_t)n, n, 1, ux_mix(T_SURF, rc, f * 30 / 100));
    }
    for (i = 6; i < 234; i += 3)                     /* the grass dotted, round the beam's footprint */
        if ((i < GN_CX ? GN_CX - i : i - GN_CX) * 16 > bw + LN(2))
            cv_rect(i, GN_GND + 1, 1, 1, T_DIM);
    f = gn_fade(gnd, tone);
    n = bw * 12 / LN(10) < 12 ? 12 : bw * 12 / LN(10);
    gn_arc(cx, gnd + LN(1), bw, LN(3), 0u, 0xFFFFFFFFu / (uint32_t)n, n, 0, ux_mix(T_SURF, ed_acc(2u, c), f < 25 ? 25 : f));
    for (i = 0; i < 4; i++) {                        /* tufts */
        int32_t gx = i == 0 ? 24 : i == 1 ? 41 : i == 2 ? 196 : 214;
        cv_stroke(LN(gx), gnd, LN(gx - 2), gnd - LN(3), T_DIM);
        cv_stroke(LN(gx + 1), gnd, LN(gx + 2), gnd - LN(4), T_DIM);
    }
    {                                                /* the cow: POS lifts it, RAND tumbles it, SPRD takes it apart */
        int32_t feet = LN(GN_GND - 2) - pos * LN(GN_GND - 2 - GN_TOP - GN_LIFT) / 127 + (LN(3) / 2 * osc_sine(now * GN_BOB) >> 15);
        uint32_t ang = 0u - (uint32_t)(rnd * GN_TILT);
        int32_t ca = osc_sine(ang + 0x40000000u), sa = osc_sine(ang), j = 0, piece = 0, line = 0, q, k;
        for (; j < (int32_t)sizeof GN_COW; j += 1 + 2 * GN_COW[j], line++) {
            uint16_t col = ed_acc(line >= GN_SPOT ? 0u : 1u, c);
            for (q = 1; q < GN_COW[j]; q++) {
                int32_t ax = GN_COW[j + 2 * q - 1] * 8, ay = GN_COW[j + 2 * q] * 8, bx = GN_COW[j + 2 * q + 1] * 8;
                int32_t by = GN_COW[j + 2 * q + 2] * 8, dx = bx > ax ? bx - ax : ax - bx, dy = by > ay ? by - ay : ay - by;
                int32_t m = ((dx > dy ? dx + dy * 3 / 8 : dy + dx * 3 / 8) + LN(1)) / LN(2);   /* pieces ~2 px long */
                if (m < 1) m = 1;
                for (k = 0; k < m; k++, piece++) {
                    int32_t x0 = ax + (bx - ax) * k / m, y0 = ay + (by - ay) * k / m;
                    int32_t x1 = ax + (bx - ax) * (k + 1) / m, y1 = ay + (by - ay) * (k + 1) / m;
                    int32_t up = clamp(-(y0 + y1) * 255 / (2 * LN(35)), 0, 255);   /* 0 the feet .. 255 the top */
                    int32_t order = ((int32_t)ed_hash((uint32_t)(piece * 7 + 1)) + 255 - up) / 2;   /* the top first */
                    int32_t tg = sprd * 261 / 127 - order, p0x, p0y, p1x, p1y, mx, my, lift, pull, x;
                    p0x = cx - LN(2) + ((x0 * ca - (y0 - LN(GN_PY)) * sa) >> 15);
                    p0y = feet + LN(GN_PY) + ((x0 * sa + (y0 - LN(GN_PY)) * ca) >> 15);
                    p1x = cx - LN(2) + ((x1 * ca - (y1 - LN(GN_PY)) * sa) >> 15);
                    p1y = feet + LN(GN_PY) + ((x1 * sa + (y1 - LN(GN_PY)) * ca) >> 15);
                    if (tg <= 0) {
                        cv_stroke(p0x, p0y, p1x, p1y, col);
                        continue;
                    }
                    /* gone to grains: a dot drifting up towards the saucer, drawn into the beam */
                    mx = (p0x + p1x) / 2;
                    my = (p0y + p1y) / 2;
                    lift = my - tg * (LN(6) + (int32_t)ed_hash((uint32_t)(piece * 5 + 3)) * LN(34) / 255) / 255;
                    if (lift < top + LN(2)) lift = top + LN(2);
                    x = mx + ((int32_t)ed_hash((uint32_t)(piece * 3 + 2)) - 128) * tg * LN(12) / (128 * 255);
                    pull = (lift - top) * 256 / (gnd - top);
                    x = cx + (x - cx) * (90 + 166 * pull / 256) / 256;
                    cv_rect((x + 8) >> 4, (lift + 8) >> 4, 1, 1, ux_mix(T_SURF, col, 100 - tg * 35 / 255));
                }
            }
        }
    }
    gn_arc(cx, LN(21), LN(14), LN(11), 0x80000000u, 0x80000000u / 17u, 17, 0, T_MID);    /* the dome */
    gn_arc(cx, LN(25), LN(34), LN(6), 0u, 0xFFFFFFFFu / 41u, 41, 0, T_MID);               /* the saucer */
    cv_stroke(LN(GN_CX - 34), LN(25), LN(GN_CX + 34), LN(25), ux_mix(T_SURF, T_MID, 60));
    gn_arc(cx, LN(30), LN(11), LN(5) / 2, 0u, 0x80000000u / 13u, 13, 0, T_MID);          /* the emitter */
    for (i = 0; i < 10; i++) {                       /* the lights round the rim, the near ones running left to right */
        uint32_t a = (uint32_t)i * 429496730u + now * GN_SPIN;
        int32_t sn = osc_sine(a);
        if (sn > 1638)                               /* (round the back: none) */
            cv_rect(GN_CX - (30 * osc_sine(a + 0x40000000u) >> 15) - 1, 27, 2, 1, ux_mix(T_SURF, T_TEXT, 30 + 70 * sn / 32768));
    }
    cv_text_r(234, 6 - AF_S_CAP_Y, &AF_S, SMP_ALL_NAMES[src], T_MID, T_SURF);
}
/* PHASE: a duel, two mages casting WAVE (left, KNOB 1's accent) and WAVE2 (right, KNOB 4's: ui.c PG_PHASE1) at each
 * other, from the knobs and the part's amp envelope (the loudest voice's), moving on the audio clock (fx.dly_w). Each
 * beam is its wave as the engine bends it (du_wave), scrolling towards the clash, its height the envelope; they take
 * turns brighter (WAVE2 alternates with WAVE). DCW, raised by ENV with the envelope, sharpens them and pushes the
 * clash right (the mage it nears leans back); WAVE2 "-": the right mage casts WAVE, dimmer. LINE MIX: DTN > 0 a
 * second beam a mage, DTN parting it (6 px at 127) and sliding it along; RING: one beaded beam each (the wave times
 * its detuned copy). DTN also sways the clash. SUB ripples the ground, FOLD folds the beams towards the clash and
 * spikes the orb there. A note's attack throws the boot screen's stars (ui_draw.c spl_burst), fewer and shorter,
 * and they keep coming while it sounds. Silent: the mages stand, their staffs glowing */
#define DU_GND 122                                   /* the ground, px */
#define DU_XL 22                                     /* the mages' feet */
#define DU_XR 218
#define DU_SEC 44100u                                /* (samples a second: fx.dly_w's clock) */
#define DU_STARS 24                                  /* a note's burst */
#define DU_TRK 4200u                                 /* the stars after: a slot each 1/10.5 s, lit 2.5 .. 10.5 a second */
static uint32_t du_bt;                               /* the latest burst, on fx.dly_w */
static uint8_t du_att;                               /* a voice was attacking at the last drawing */
static int32_t du_ncos(uint32_t p16) { return -osc_sine(((p16 & 0xFFFFu) << 16) + 0x40000000u); }   /* -cos, Q15 */
static uint32_t du_bend(uint32_t f, int32_t d)       /* the SAW's bent phase, Q16; d DCW Q15 */
{
    uint32_t x = 32768u - (uint32_t)(d * 30802 >> 15);   /* (the bend point: half a cycle .. 3 %) */
    return f < x ? f * 32768u / x : 32768u + (f - x) * 32768u / (65536u - x);
}
/* the eight waves, f Q16, d Q15, out Q15: eng_phase.c's shapes, drawn (not its tables or its band limits) */
static int32_t du_wave(uint32_t w, uint32_t f, int32_t d)
{
    uint32_t x = 32768u - (uint32_t)(d * 30802 >> 15), q, k;
    int32_t win;
    f &= 0xFFFFu;
    switch (w) {
    case 0: return du_ncos(du_bend(f, d));                                              /* SAW */
    case 1: q = f & 32767u;                                                             /* SQR: each half rises, holds */
        return du_ncos((f & 32768u) + (q >= x ? 32768u : q * 32768u / x));
    case 2: x = 65536u - (uint32_t)(d * 57672 >> 15);                                   /* PLS */
        return du_ncos(f >= x ? 0u : f * 65536u / x);
    case 3: return du_ncos(du_bend((f * 2u) & 0xFFFFu, d));                             /* DSIN */
    case 4: return f < 32768u ? du_ncos(du_bend(f * 2u, d)) : -32767;                   /* SPLS */
    default:                                                                            /* RSAW RTRI RTRP */
        k = 256u + (uint32_t)(d * 2816 >> 15);
        win = w == 5u ? 32767 - (int32_t)(f >> 1) : w == 6u ? 32767 - (int32_t)(f > 32768u ? f - 32768u : 32768u - f)
            : clamp(clamp(3 * (65536 - (int32_t)f) >> 1, 0, 32767), 0, (int32_t)f * 4);
        return ((32767 + du_ncos(f * k >> 8)) * win >> 15) - 32767;
    }
}
static int32_t du_fold(int32_t v, int32_t g)         /* FOLD's sine folder, gain g Q8 (256: none drawn) */
{
    return g <= 256 ? v : osc_sine((uint32_t)(v * g) << 7);
}
static int32_t du_gy(int32_t x, int32_t sub, uint32_t now)   /* the ground at x px, 1/16 px: SUB's ripple */
{
    return LN(DU_GND) + (sub * osc_sine((uint32_t)x * 30760000u - now * 77500u) >> 15);
}
static void du_disc(int32_t cx, int32_t cy, int32_t r, uint16_t c)   /* a filled disc, centre and radius 1/16 px */
{
    int32_t y;
    for (y = -r / 16; y <= r / 16; y++) {
        int32_t w = r * r - y * y * 256, h = 0;
        while ((h + 1) * (h + 1) * 256 <= w) h++;
        cv_rect((cx >> 4) - h, (cy >> 4) + y, 2 * h + 1, 1, c);
    }
}
/* a mage at x px facing dir (1 right), lean 1/16 px back; its staff's tip out */
static void du_mage(int32_t x, int32_t dir, uint16_t col, int32_t lean, int32_t lvl, int32_t sub, uint32_t now,
                    int32_t *tx, int32_t *ty)
{
    int32_t gy = du_gy(x, sub, now), bx = LN(x) - dir * lean, top = gy - LN(30), hx, hy, r, f;
    int32_t flop = (LN(3) / 2 * osc_sine(now * 46454u) >> 15) - dir * lean * 6 / 10;
    cv_stroke(bx - LN(9), gy, bx + LN(9), gy, col);                                     /* the robe */
    cv_stroke(bx + LN(9), gy, bx + dir * LN(1), top, col);
    cv_stroke(bx + dir * LN(1), top, bx - LN(9), gy, col);
    cv_stroke(bx - dir * LN(8), gy - LN(6), bx + dir * LN(6), gy - LN(6), ux_mix(T_SURF, col, 45));
    gn_arc(bx + dir * LN(3) / 2, top - LN(4), LN(4), LN(4), 0u, 0xFFFFFFFFu / 14u, 14, 0, T_TEXT);   /* the head */
    cv_stroke(bx - LN(8), top - LN(7), bx + LN(9), top - LN(7), T_TEXT);                /* the hat */
    cv_stroke(bx - LN(5) + dir * LN(1), top - LN(7), bx - dir * LN(2) + flop, top - LN(20), col);
    cv_stroke(bx - dir * LN(2) + flop, top - LN(20), bx - dir * LN(7) + flop, top - LN(22), col);
    cv_stroke(bx + LN(5) - dir * LN(1), top - LN(7), bx - dir * LN(2) + flop, top - LN(20), col);
    hx = bx + dir * LN(9);                                                              /* the arm, the staff */
    hy = top + LN(9);
    *tx = bx + dir * LN(15);
    *ty = top - LN(12);
    cv_stroke(bx + dir * LN(2), top + LN(4), hx, hy, T_TEXT);
    cv_stroke(hx - dir * LN(2), hy + LN(18), *tx, *ty, T_MID);
    r = lvl ? LN(2) + LN(5) / 2 * lvl / 32768 : LN(5) / 2 + (LN(1) / 2 * osc_sine(now * 61980u) >> 15);
    f = lvl ? 25 + 75 * lvl / 32768 : 32;
    du_disc(*tx, *ty, r, ux_mix(T_SURF, col, f));
}
/* a beam from (x0, y0) to (x1, y1), 1/16 px, off 1/16 px down; ph2 the phase added (Q16), ring: the wave times its
 * detuned copy (r2 Q8) */
static void du_beam(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t w, int32_t d, uint16_t c, int32_t off,
                    uint32_t ph2, int ring, int32_t r2, int32_t lvl, int32_t fold, uint32_t now)
{
    int32_t n = (x1 > x0 ? x1 - x0 : x0 - x1) / LN(2), i, px = 0, py = 0;
    uint32_t scroll = (now >> 4) * 29u;              /* 1.2 cycles a second */
    if (n < 2) n = 2;
    for (i = 0; i <= n; i++) {
        int32_t u = i * 1024 / n, x = x0 + (x1 - x0) * i / n, y = y0 + (y1 - y0) * i / n + off;
        int32_t dist = (x > x0 ? x - x0 : x0 - x) >> 4, v, taper, g;
        uint32_t f = (uint32_t)dist * 1928u - scroll + ph2;          /* a cycle in 34 px */
        v = du_wave(w, f, d);
        if (ring)
            v = clamp((v * du_wave(w, (((uint32_t)dist * 1928u - scroll) * (uint32_t)r2 >> 8) + ph2, d) >> 15) * 13 / 10,
                      -32767, 32767);
        g = 256 + fold * 7 * 256 / 127 * (u * u / 1024) / 1024;   /* (growing towards the clash) */
        v = du_fold(v, g);
        taper = (u * 8 > 1024 ? 1024 : u * 8) * (614 + 410 * u / 1024) / 1024;
        y -= (int32_t)((int64_t)v * LN(10) * lvl / 32768 * taper / 32768 / 1024);
        if (i)
            cv_stroke(px, py, x, y, c);
        px = x;
        py = y;
    }
}
/* the clash: an outline round it, FOLD's spikes, a core */
static void du_orb(int32_t cx, int32_t cy, int32_t d, int32_t lvl, int32_t fold, int32_t flash, uint32_t now, uint16_t c)
{
    int32_t base = LN(3) + LN(7) * lvl / 32768 * (16384 + d / 2) / 32768 + LN(4) * flash / 1024;
    int32_t sp = (fold * 9 + 63) / 127, i, px = 0, py = 0;
    for (i = 0; i <= 48; i++) {
        uint32_t a = (uint32_t)i * (0xFFFFFFFFu / 48u);
        int32_t s = osc_sine(a * (uint32_t)sp + now * 93000u), jag = sp ? fold * 55 / 127 * (s < 0 ? -s : s) / 32768 : 0;
        int32_t r = base * (100 + jag) / 100 + (LN(3) / 5 * lvl / 32768 * osc_sine(a * 3u - now * 170500u) >> 15);
        int32_t x = cx + (r * osc_sine(a + 0x40000000u) >> 15), y = cy + (r * 9 / 10 * osc_sine(a) >> 15);
        if (i)
            cv_stroke(px, py, x, y, ux_mix(T_SURF, c, 35 + 65 * lvl / 32768));
        px = x;
        py = y;
    }
    du_disc(cx, cy, base * 45 / 100, ux_mix(T_SURF, c, 15 + 40 * lvl / 32768));
}
/* one of the boot screen's stars: a plus with s px arms, from (x, y) px out along (dx, dy) Q8 by reach px, born t0
 * samples back of now, living life samples (cubic ease out, fading as 1 - u^2, twinkling every 70 ms) */
static void du_star(int32_t x, int32_t y, int32_t dx, int32_t dy, int32_t reach, uint32_t age, uint32_t life,
                    int32_t s, uint32_t k, uint16_t c, uint32_t now)
{
    int32_t u = (int32_t)(age * 1000u / life), v = 1000 - u, e = 1000 - v * v / 1000 * v / 1000, pct, j;
    x += dx * reach / 256 * e / 1000;
    y += dy * reach / 256 * e / 1000;
    pct = (1000 - u * u / 1000) / 10;
    if (((now / 3087u + k) & 3u) == 0u)              /* (70 ms) */
        pct /= 3;
    if (pct <= 0)
        return;
    cv_pset(x, y, ux_mix(T_SURF, c, pct));
    for (j = 1; j <= s; j++) {
        uint16_t a = ux_mix(T_SURF, c, pct * (s + 1 - j) / (s + 1));
        cv_pset(x - j, y, a);
        cv_pset(x + j, y, a);
        cv_pset(x, y - j, a);
        cv_pset(x, y + j, a);
    }
}
/* star n of a burst seeded sd, from (cx, cy) px, spread sp px; born at t0 (fx.dly_w) */
static void du_burst_star(uint32_t sd, uint32_t n, int32_t cx, int32_t cy, int32_t sp, uint32_t t0, uint32_t now, uint16_t c0)
{
    uint32_t h = sd * 131u + n * 17u, delay = ed_hash(h) * (DU_SEC * 9u / 100u) / 255u;
    uint32_t life = DU_SEC * 65u / 100u + ed_hash(h + 1u) * (DU_SEC * 45u / 100u) / 255u, age = now - t0, a;
    int32_t m = 64 + (int32_t)ed_hash(h + 2u) * 192 / 255;
    uint32_t ci = ed_hash(h + 3u) % 5u;
    uint16_t c = ci == 4u ? T_TEXT : ed_acc(ci, c0);   /* (the four accents and the cream, as the boot's) */
    if (age < delay || age >= delay + life)
        return;
    a = ed_hash(h + 4u) << 24 | ed_hash(h + 5u) << 16;
    du_star(cx + ((int32_t)ed_hash(h + 6u) - 128) * sp / 128, cy + ((int32_t)ed_hash(h + 7u) - 128) * sp * 6 / 1280,
            (m * osc_sine(a + 0x40000000u)) >> 15, (m * osc_sine(a)) >> 15, 12 + (int32_t)ed_hash(h + 8u) * 26 / 255,
            age - delay, life, 1 + (int32_t)ed_hash(h + 9u) % 3, n, c, now);
}
static void graph_phase(const track_t *t, uint16_t c)
{
    const int16_t *p = t->p;
    uint32_t w1 = (uint32_t)clamp(p[P_E0], 0, 7), w2 = (uint32_t)clamp(p[P_E1], 0, 8), now = fx.dly_w, k;
    int32_t dcw = clamp(p[P_E2], 0, 127), env = clamp(p[P_E3], 0, 127), dtn = clamp(p[P_E4], 0, 127);
    int32_t ring = p[P_E5] == 1, sub = clamp(p[P_E6], 0, 127), fold = clamp(p[P_E7], 0, 127);
    int32_t lvl = 0, d, cx, cy, lx, ly, rx, ry, closel, closer, flash, i, att = 0, mirror = w2 == 0u;
    uint16_t c1 = ed_acc(0u, c), c2 = ed_acc(3u, c), b2 = mirror ? ux_mix(T_SURF, c2, 65) : c2;
    for (i = 0; i < NVOICE; i++)                     /* the loudest voice's envelope, Q15 */
        if (t->v[i].active) {
            if (t->v[i].env >> 9 > lvl) lvl = t->v[i].env >> 9;
            att |= t->v[i].stage == 1u;
        }
    lvl = clamp(lvl, 0, 32767);
    if (lvl < 66) lvl = 0;
    if (att && !du_att)                              /* an attack: the burst */
        du_bt = now;
    du_att = (uint8_t)att;
    w2 = mirror ? w1 : w2 - 1u;
    d = dcw * 32767 / 127;                           /* DCW, ENV raising it with the envelope */
    d += (env * 32767 / 127 * lvl >> 15) * (32767 - d) >> 15;
    cx = LN(120) + (d - 16384) * LN(100) / 32768 + LN(12) * dtn / 127 * osc_sine(now * (9300u + 775u * (uint32_t)dtn)) / 32768;
    closel = clamp(LN(1) - (cx - LN(40)) / 80, 0, LN(1));   /* (1/16: how near the clash is) */
    closer = clamp(LN(1) - (LN(200) - cx) / 80, 0, LN(1));
    sub = LN(7) / 2 * sub / 127 * lvl / 32768;
    for (i = 0; i <= 240; i += 2) {                  /* the ground, dashes under it */
        if (i)
            cv_stroke(LN(i - 2), du_gy(i - 2, sub, now) + LN(1) / 2, LN(i), du_gy(i, sub, now) + LN(1) / 2, T_DIM);
        if (i % 16 == 8)
            cv_stroke(LN(i), du_gy(i, sub, now) + LN(4), LN(i + 5), du_gy(i, sub, now) + LN(4), ux_mix(T_SURF, T_DIM, 50));
    }
    du_mage(DU_XL, 1, c1, closel * 5 * lvl / 32768, lvl, sub, now, &lx, &ly);
    du_mage(DU_XR, -1, c2, closer * 5 * lvl / 32768, lvl, sub, now, &rx, &ry);
    flash = now - du_bt < DU_SEC / 3u ? 1024 - (int32_t)((now - du_bt) * 3072u / DU_SEC) : 0;
    if (lvl) {
        int32_t alt = (int32_t)(now / (DU_SEC / 6u)) & 1, f1 = alt ? 75 : 100, f2 = alt ? 100 : 75;
        uint16_t k1 = ux_mix(T_SURF, c1, f1), k2 = ux_mix(T_SURF, b2, f2);
        cy = (ly + ry) / 2 + (LN(2) * osc_sine(now * 35650u) >> 15);
        if (!ring) {                                 /* MIX: one beam each; DTN a second, parted and sliding */
            int32_t o = LN(3) * dtn / 127;
            uint32_t drift = (now >> 6) * (uint32_t)dtn * 73u >> 6;
            du_beam(lx, ly, cx - LN(2), cy, w1, d, k1, -o, 0u, 0, 256, lvl, fold, now);
            du_beam(rx, ry, cx + LN(2), cy, w2, d, k2, -o, 0u, 0, 256, lvl, fold, now);
            if (dtn) {
                du_beam(lx, ly, cx - LN(2), cy, w1, d, ux_mix(T_SURF, c1, f1 * 55 / 100), o, drift, 0, 256, lvl, fold, now);
                du_beam(rx, ry, cx + LN(2), cy, w2, d, ux_mix(T_SURF, b2, f2 * 55 / 100), o, 0u - drift, 0, 256, lvl, fold, now);
            }
        } else {                                     /* RING: one beaded beam each */
            uint32_t drift = (now >> 6) * (uint32_t)(25 + dtn) * 73u >> 6;   /* (0.3 + DTN * 0.012 a second) */
            int32_t r2 = 302 + dtn * 128 / 127;
            du_beam(lx, ly, cx - LN(2), cy, w1, d, k1, 0, drift, 1, r2, lvl, fold, now);
            du_beam(rx, ry, cx + LN(2), cy, w2, d, k2, 0, 0u - drift, 1, r2, lvl, fold, now);
        }
        du_orb(cx, cy, d, lvl, fold, flash, now, ed_acc(1u, c));
        for (k = 0; k < 13u; k++) {                  /* the stars after: the last 1.3 s of slots, as many lit as lvl says */
            uint32_t slot = now / DU_TRK - k, t0 = slot * DU_TRK;
            if ((uint32_t)ed_hash(slot * 7u + 3u) * 105u < (uint32_t)(25 + 80 * lvl / 32768) * 256u && t0 >= du_bt)
                du_burst_star(slot, 0u, cx >> 4, cy >> 4, 3, t0, now, c);
        }
    } else
        cy = (ly + ry) / 2;
    if (du_bt && now - du_bt < DU_SEC * 12u / 10u)   /* the note's burst */
        for (k = 0; k < DU_STARS; k++)
            du_burst_star(du_bt >> 8, k, cx >> 4, cy >> 4, 6, du_bt, now, c);
}
/* VOICE: a singer (left) and a speech bubble (right), from the knobs and the loudest voice, on the audio clock
 * (fx.dly_w). The mouth is the vowel the engine sings: VOWL's moving to VOWL2's over TALK (the voice's own progress,
 * eased out as eng_formant.c eases it), leaning to the note's random vowel by RAND; A tall with a tongue, E wide, I a
 * spread smile with teeth, O round, U a pucker (VX_MOUTH), opened by the envelope, the jaw dropping with it. SHIFT
 * sizes the head (down bigger) and tilts the brows (up lifted, down stern); the note's pitch lifts them. BUZZ (+
 * velocity, as the engine takes it) opens the eyes and, past the middle, spikes the bubble (the comic shout); BRTH
 * dashes its outline (the whisper) and fades the wave with the voicing. In the bubble the voice's wave, two periods,
 * the voice rendered again by the engine (vx_wave). RAND sends each note's glance somewhere new. Glasses, a tiny top
 * hat hopping on each note-on, a blink every 3.7 s. Silent: the mouth shut, three dots. Prototype: dev/voice_face.html */
#define VX_FX 62                                     /* the face's centre, px */
#define VX_FY 76
#define VX_BX 175                                    /* the bubble's */
#define VX_BY 58
#define VX_WP 88                                     /* the wave's points */
#define VX_HOP 12348u                                /* the hat's jump, samples (0.28 s, 7 px) */
#define VX_POP 7938u                                 /* the bubble's pop (0.18 s) */
#define VX_BLINK 163170u                             /* a blink every 3.7 s, 0.13 s long */
#define VX_SHUT 5733u
#define VX_TA 0x6AAAAAABu                            /* the tail's direction from the bubble's centre (150 deg) */
#define VX_HALF 109371000u                           /* half its root (0.16 rad) */
static uint32_t vx_bt;                               /* the latest note-on, on fx.dly_w */
static uint32_t vx_age;                              /* the newest voice's voice.c age at the last drawing */
static uint8_t vx_note = 60, vx_rv;                  /* the latest note and its random vowel (kept after it ends) */
/* the mouths of A E I O U: width, height (1/16 px), boxiness Q8 (an ellipse .. (1 - u^4)^(1/4)), smile (the corners
 * up, 1/16 px), teeth, tongue (Q8) */
static const int16_t VX_MOUTH[5][6] = {
    {LN(20), LN(16), 51, 0, 0, 256}, {LN(22), LN(10), 128, LN(2), 256, 0}, {LN(24), LN(4), 256, LN(7) / 2, 256, 0},
    {LN(13), LN(13), 0, 0, 0, 0}, {LN(8), LN(7), 0, -LN(1) / 2, 0, 0},
};
/* sqrt(1 - u^2) and (1 - u^4)^(1/4), Q15, |u| 0 .. 1 in tenths: the lips */
static const uint16_t VX_F2[11] = {32767, 32603, 32105, 31258, 30032, 28377, 26214, 23400, 19660, 14283, 0};
static const uint16_t VX_F4[11] = {32767, 32766, 32754, 32700, 32555, 32243, 31650, 30593, 28722, 25093, 0};
/* cos^0.6, Q15, a quarter turn in eighths: the lenses' rounded squares */
static const uint16_t VX_SQ[9] = {32767, 32388, 31247, 29334, 26615, 23037, 18406, 12295, 0};
static int32_t vx_sqrt(uint32_t x)
{
    uint32_t r = 0, b = 1u << 30;
    while (b > x) b >>= 2;
    for (; b; b >>= 2)
        if (x >= r + b) {
            x -= r + b;
            r = (r >> 1) + b;
        } else
            r >>= 1;
    return (int32_t)r;
}
static void vx_mouth(int32_t pos, int32_t *m)        /* the mouth at vowel position pos (0..127 << 8): VX_MOUTH's */
{
    int32_t x = clamp(pos, 0, 127 << 8) * 4 / 127, i = x >> 8, fr = x & 255, j;
    if (i >= 4) {
        i = 3;
        fr = 256;
    }
    for (j = 0; j < 6; j++)
        m[j] = VX_MOUTH[i][j] + ((VX_MOUTH[i + 1][j] - VX_MOUTH[i][j]) * fr >> 8);
}
/* a stroke of the hat: (x, y) 1/16 px about its brim's middle, tipped right, hs Q8 its size */
static void vx_hat(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t ox, int32_t oy, int32_t hs, uint16_t c)
{
    cv_stroke(ox + ((x0 * 32237 - y0 * 5866) >> 15) * hs / 256, oy + ((x0 * 5866 + y0 * 32237) >> 15) * hs / 256,
              ox + ((x1 * 32237 - y1 * 5866) >> 15) * hs / 256, oy + ((x1 * 5866 + y1 * 32237) >> 15) * hs / 256, c);
}
/* a piece of a dashed line, *d the length so far (1/16 px); on of each per drawn, cut where the dashes end; per 0:
 * whole */
static void vx_dash(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t *d, int32_t on, int32_t per, uint16_t c)
{
    int32_t dx = x1 > x0 ? x1 - x0 : x0 - x1, dy = y1 > y0 ? y1 - y0 : y0 - y1, l = dx > dy ? dx + dy * 3 / 8 : dy + dx * 3 / 8;
    int32_t s = 0, e, ph;
    if (!per)
        cv_stroke(x0, y0, x1, y1, c);
    else
        for (; s < l; s = e) {
            ph = (*d + s) % per;
            e = ph < on ? s + on - ph : s + per - ph;
            if (e > l) e = l;
            if (ph < on)
                cv_stroke(x0 + (x1 - x0) * s / l, y0 + (y1 - y0) * s / l, x0 + (x1 - x0) * e / l, y0 + (y1 - y0) * e / l, c);
        }
    *d += l;
}
/* the bubble's wave: voice src rendered again by the engine (a copy: its phase, its filters, TALK where it is; at full
 * level, no LFO), from the glottis's next opening, two periods (one under 63 Hz); w points into y, back the peak. The
 * breath noise's seed is put back, so the sound never hears it */
static int32_t vx_wave(const track_t *t, const voice_t *src, int32_t *y, int32_t w)
{
    voice_t v = *src;
    vmod_t m;
    int32_t buf[32], pk = 1, nz = FORMANT_NZ(t);
    uint32_t per, skip, n, s, j, got = 0;
    m.pitch16 = clamp(v.pitch_cur, 0, 2047);
    m.inc = pitch_inc((uint32_t)m.pitch16);
    m.amp0 = m.amp1 = 32767;
    m.cutoff = 0;
    m.shape = 64 << 8;
    m.envq15 = 32767;
    m.fine = 0;
    per = (uint32_t)((1ull << 32) / m.inc);
    skip = (0u - v.ph[0]) / m.inc;
    n = per <= 700u ? 2u * per : per;
    for (s = 0; s < skip + n && got < (uint32_t)w; s += 32u) {
        uint32_t k = skip + n - s < 32u ? skip + n - s : 32u;
        for (j = 0; j < k; j++)
            buf[j] = 0;
        formant_render((track_t *)t, &v, buf, k, &m);
        for (j = 0; j < k; j++) {
            uint32_t c = s + j - skip;
            if (s + j < skip) continue;
            if (buf[j] > pk) pk = buf[j];
            else if (-buf[j] > pk) pk = -buf[j];
            while (got < (uint32_t)w && c >= got * (n - 1u) / (uint32_t)(w - 1))
                y[got++] = buf[j];
        }
    }
    while (got < (uint32_t)w)
        y[got++] = 0;
    FORMANT_NZ(t) = nz;
    return pk;
}
static void graph_formant(const track_t *t, uint16_t c)
{
    const int16_t *p = t->p;
    const voice_t *lv = 0;
    uint32_t now = fx.dly_w, i, u;
    int32_t shift = clamp(p[P_E3], -12, 12), sc = 256 - shift * 3, lvl = 0, k = 0, buzz, v0, v1, j, side;
    uint32_t age = 0;
    int32_t m0[6], m1[6], m[6], open, mw, mh, mx, my, hx, hyt, hyb, op, gx, gy, lift, ey, ew, px = 0, py = 0, x, y;
    int32_t fx16 = LN(VX_FX), fy16 = LN(VX_FY);
    uint16_t ink = ed_acc(0u, c), eye = ed_acc(1u, c), lens = ed_acc(2u, c);
    for (i = 0; i < NVOICE; i++)                     /* the loudest voice */
        if (t->v[i].active) {
            if (t->v[i].env >> 9 > lvl) {
                lvl = t->v[i].env >> 9;
                lv = &t->v[i];
            }
            if (t->v[i].age > age) age = t->v[i].age;
        }
    lvl = clamp(lvl, 0, 32767);
    if (lvl < 66) lvl = 0;
    /* a note-on (a voice started since: voice.c ages each one; MONO / LEGATO: the note changed): the hat jumps,
     * the bubble pops */
    if (age > vx_age || (lv && lv->note != vx_note && (p[P_VOICE] == V_MONO || p[P_VOICE] == V_LEGATO)))
        vx_bt = now;
    if (age > vx_age || age + 0x10000u < vx_age)     /* (vage wrapped, or another track) */
        vx_age = age;
    buzz = clamp(p[P_E4] + (lv ? ((int32_t)lv->vel - 96) * 40 / 256 : 0), 0, 127);
    v0 = p[P_E0] << 8;                               /* the vowel, as formant_render takes it */
    v1 = p[P_E1] << 8;
    if (lv) {
        uint32_t s7 = (uint32_t)lv->s[7];
        vx_note = lv->note;
        vx_rv = (uint8_t)(s7 >> 25);
        if (p[P_E2]) {
            k = (int32_t)((s7 & FMT_TALK_MASK) >> 9);
            k = (k * (65536 - k)) >> 15;
        }
    }
    if (lv || p[P_E7]) {
        v0 += (((int32_t)vx_rv << 8) - v0) * p[P_E7] * 258 >> 15;
        v1 += (((int32_t)vx_rv << 8) - v1) * p[P_E7] * 258 >> 15;
    }
    vx_mouth(v0, m0);
    vx_mouth(v1, m1);
    for (j = 0; j < 6; j++)
        m[j] = m0[j] + ((m1[j] - m0[j]) * k >> 15);
    open = vx_sqrt((uint32_t)lvl << 15);
    mw = m[0] * sc / 256 * (85 + 15 * open / 32768) / 100;
    mh = m[1] * sc / 256 * open / 32768;
    if (mh < 10) mh = 10;
    mx = fx16 + LN(8);
    my = fy16 + LN(20) * sc / 256;
    /* the head, the jaw dropping with the mouth; an ear */
    hx = LN(38) * sc / 256;
    hyt = LN(42) * sc / 256;
    hyb = LN(44) * sc / 256 + mh * 35 / 100;
    gn_arc(fx16, fy16, hx, hyt, 0x80000000u, 0x80000000u / 40u, 40, 0, T_TEXT);
    gn_arc(fx16, fy16, hx, hyb, 0u, 0x80000000u / 40u, 40, 0, T_TEXT);
    gn_arc(fx16 - hx + LN(1), fy16 + LN(2) * sc / 256, LN(5) * sc / 256, LN(8) * sc / 256, 0x40000000u, 0x80000000u / 16u,
           16, 0, T_TEXT);
    /* the eyes: BUZZ opens them, a blink now and then; RAND a glance of the note's own */
    u = now % VX_BLINK;
    op = 9175 + 23592 * buzz / 127;
    if (u < VX_SHUT) {
        int32_t b = (int32_t)(u * 32768u / (VX_SHUT / 2u)) - 32768;
        op = op * (b < 0 ? -b : b) >> 15;
    }
    gx = LN(13) / 10;
    gy = -LN(4) / 10;
    if (p[P_E7]) {
        uint32_t a = ed_hash((uint32_t)vx_rv * 7u + vx_note) << 24;
        gx = (gx * (127 - p[P_E7]) + (LN(18) / 10 * osc_sine(a + 0x40000000u) >> 15) * p[P_E7]) / 127;
        gy = (gy * (127 - p[P_E7]) + (LN(14) / 10 * osc_sine(a) >> 15) * p[P_E7]) / 127;
    }
    lift = clamp(shift * LN(22) / 100 + ((int32_t)vx_note - 60) * LN(1) / 10, -LN(5) / 2, LN(4));
    ey = fy16 - LN(7) * sc / 256;
    ew = LN(6) * sc / 256;
    for (side = -1; side <= 1; side += 2) {
        int32_t ex = fx16 + LN(7) + side * LN(13) * sc / 256, up = LN(9) / 2 * sc / 256 * op >> 15, dn = LN(22) / 10 * sc / 256;
        int32_t by = ey - LN(11) * sc / 256 - lift, tl = shift * LN(18) / 100;
        if (op < 4588) {                             /* (shut) */
            cv_stroke(ex - ew, ey, ex, ey + LN(1), T_TEXT);
            cv_stroke(ex, ey + LN(1), ex + ew, ey, T_TEXT);
        } else {
            int32_t pcx = ex + gx * sc / 256, pcy = ey + gy * sc / 256 - up * 15 / 100, pr = LN(23) / 10 * sc / 256;
            int32_t qx = 0, qy = 0, rx = 0, ry = 0;
            for (y = (pcy - pr) >> 4; y <= (pcy + pr) >> 4; y++)   /* the pupil, inside the lids */
                for (x = (pcx - pr) >> 4; x <= (pcx + pr) >> 4; x++) {
                    int32_t dx = LN(x) + 8 - pcx, dy = LN(y) + 8 - pcy, q = (LN(x) + 8 - ex) * 32768 / ew, f;
                    if (dx * dx + dy * dy > pr * pr || q <= -32768 || q >= 32768) continue;
                    f = 32768 - (q * q >> 15);
                    if (LN(y) + 8 >= ey - (up * f >> 15) && LN(y) + 8 <= ey + (dn * f >> 15))
                        cv_rect(x, y, 1, 1, eye);
                }
            for (j = 0; j <= 12; j++) {              /* the lids */
                int32_t q = -32768 + j * 65536 / 12, f = 32768 - (q * q >> 15), lx = ex + (q * ew >> 15);
                if (j) {
                    cv_stroke(qx, qy, lx, ey - (up * f >> 15), T_TEXT);
                    cv_stroke(rx, ry, lx, ey + (dn * f >> 15), ux_mix(T_SURF, T_TEXT, 55));
                }
                qx = rx = lx;
                qy = ey - (up * f >> 15);
                ry = ey + (dn * f >> 15);
            }
        }
        /* the brows: SHIFT up lifts their inner ends, down lowers them; the pitch lifts them */
        cv_stroke(ex - side * ew, by - tl, ex, by - LN(16) / 10 - tl * 3 / 10, eye);
        cv_stroke(ex, by - LN(16) / 10 - tl * 3 / 10, ex + side * ew, by + tl * 3 / 10, eye);
    }
    {   /* the glasses: two rounded lenses, the bridge, the arms */
        int32_t gy0 = ey + LN(1) / 2, lw = LN(9) * sc / 256, lh = LN(13) / 2 * sc / 256, q, gcx;
        for (side = -1; side <= 1; side += 2) {
            gcx = fx16 + LN(7) + side * LN(13) * sc / 256;
            for (q = 0; q <= 32; q++) {
                int32_t a = q & 7, cq = (q >> 3) & 3, cs, sn;
                cs = cq == 0 ? VX_SQ[a] : cq == 1 ? -VX_SQ[8 - a] : cq == 2 ? -VX_SQ[a] : VX_SQ[8 - a];
                sn = cq == 0 ? VX_SQ[8 - a] : cq == 1 ? VX_SQ[a] : cq == 2 ? -VX_SQ[8 - a] : -VX_SQ[a];
                x = gcx + (lw * cs >> 15);
                y = gy0 + (lh * sn >> 15);
                if (q) cv_stroke(px, py, x, y, lens);
                px = x;
                py = y;
            }
        }
        gcx = LN(13) * sc / 256;
        cv_stroke(fx16 + LN(7) - gcx + lw, gy0 - LN(1), fx16 + LN(7), gy0 - LN(5) / 2 * sc / 256, lens);
        cv_stroke(fx16 + LN(7), gy0 - LN(5) / 2 * sc / 256, fx16 + LN(7) + gcx - lw, gy0 - LN(1), lens);
        cv_stroke(fx16 + LN(7) - gcx - lw, gy0 - LN(1), fx16 - hx + LN(3), gy0 - LN(2), lens);
        cv_stroke(fx16 + LN(7) + gcx + lw, gy0 - LN(1), fx16 + hx - LN(3) / 2, gy0 - LN(2), lens);
    }
    cv_stroke(fx16 + LN(9), fy16 - LN(1), fx16 + LN(12), fy16 + LN(7) * sc / 256, T_MID);   /* the nose */
    cv_stroke(fx16 + LN(12), fy16 + LN(7) * sc / 256, fx16 + LN(8), fy16 + LN(8) * sc / 256, T_MID);
    {   /* the mouth */
        int32_t lx[21], uy[21], ly[21];
        for (j = 0; j <= 20; j++) {
            int32_t q = j - 10, a = q < 0 ? -q : q, f = VX_F2[a] + ((VX_F4[a] - VX_F2[a]) * m[2] >> 8);
            int32_t l = (m[3] * sc / 256 * q * q / 100) * (16384 + open / 2) >> 15;
            lx[j] = mx + q * mw / 20;
            uy[j] = my - (mh * 35 / 100 * f >> 15) - l;
            ly[j] = my + (mh * 65 / 100 * f >> 15) - l;
        }
        if (mh > 19) {                               /* open */
            if (m[4] > 77 && mh > 40)
                cv_stroke(mx - mw * 35 / 100, my - mh * 35 / 100 + LN(14) / 10, mx + mw * 35 / 100, my - mh * 35 / 100 + LN(14) / 10,
                          ux_mix(T_SURF, T_TEXT, 70 * m[4] / 256));
            if (m[5] > 77 && mh > LN(6))
                gn_arc(mx + LN(1), my + mh * 65 / 100, mw * 28 / 100, mh * 30 / 100, 0x80000000u, 0x80000000u / 12u, 12, 0,
                       ux_mix(T_SURF, ink, 55 * m[5] / 256));
            for (j = 1; j <= 20; j++)
                cv_stroke(lx[j - 1], ly[j - 1], lx[j], ly[j], ink);
        }
        for (j = 1; j <= 20; j++)
            cv_stroke(lx[j - 1], uy[j - 1], lx[j], uy[j], ink);
    }
    {   /* the bubble: a note-on pops it, the envelope breathes it, BUZZ spikes it, BRTH dashes it */
        int32_t pop = 32768, rx, ry, sp, tx = mx + mw / 2 + LN(9), ty = my - LN(3), d = 0, on = 0, per = 0;
        int32_t br = clamp(p[P_E5], 0, 127);         /* BRTH as drawn: 25 % draws 70 %'s, 50 % 100 %'s, on to 190 */
        br = br <= 32 ? br * 89 / 32 : br <= 64 ? 89 + (br - 32) * 38 / 32 : 127 + (br - 64);
        int32_t fx0 = 0, fy0 = 0, bx = LN(VX_BX), by = LN(VX_BY);
        uint16_t oc = ux_mix(T_SURF, ink, lvl ? 60 + 40 * lvl / 32768 : 45);
        u = now - vx_bt;
        if (vx_bt && u < VX_POP) {
            uint32_t a = u * (0x80000000u / VX_POP);
            pop = 24576 + (8192 * (32768 - osc_sine(a + 0x40000000u)) >> 16) + osc_sine(a) * 12 / 100;
        }
        pop = pop * (30802 + 1966 * lvl / 32768) >> 15;
        rx = LN(56) * pop >> 15;
        ry = LN(40) * pop >> 15;
        sp = clamp((buzz - 50) * 32768 / 77, 0, 32768) * 24 / 100;
        if (!lvl) sp /= 2;
        if (br > 30) {                               /* (a little breath keeps it whole: dashes 5 px .. 2 px, gaps .. 6.6 px) */
            on = LN(5) - (br - 30) * LN(2) / 97;
            if (on < LN(2)) on = LN(2);
            per = on + (br - 30) * LN(4) / 97;
        }
        for (i = 0; i <= 150u; i++) {
            uint32_t th = VX_TA + VX_HALF + i * ((0xFFFFFFFFu - 2u * VX_HALF) / 150u), w = th - VX_TA;
            uint32_t seg = (uint32_t)((uint64_t)w * 15u >> 32), fr = (uint32_t)((uint64_t)w * 15u >> 16) & 0xFFFFu;
            int32_t tri = (65536 - 2 * (int32_t)(fr > 32768u ? fr - 32768u : 32768u - fr)) >> 1;
            int32_t hv = 166 + (int32_t)ed_hash(seg * 3u + 11u) * 179 / 255;
            int32_t r = 32768 + ((sp * hv >> 8) * ((tri * tri >> 15) - 11469) >> 15);
            x = bx + ((rx * r >> 15) * osc_sine(th + 0x40000000u) >> 15);
            y = by + ((ry * r >> 15) * osc_sine(th) >> 15);
            if (i) vx_dash(px, py, x, y, &d, on, per, oc);
            else {
                fx0 = x;
                fy0 = y;
            }
            px = x;
            py = y;
        }
        vx_dash(px, py, tx, ty, &d, on, per, oc);   /* the tail, to the mouth */
        vx_dash(tx, ty, fx0, fy0, &d, on, per, oc);
        if (!lvl)                                    /* silent: three dots */
            for (j = -1; j <= 1; j++)
                cv_rect(VX_BX + j * 8 - 1, VX_BY - 1, 2, 2, T_MID);
        else {                                       /* the wave, its height the envelope, BRTH fading it */
            int32_t wy[VX_WP], pk = vx_wave(t, lv, wy, VX_WP), ww = rx * 155 / 100;
            int32_t hh = (ry * 62 / 100) * (lvl < 3932 ? 3932 : lvl) >> 15;
            int32_t bv = br < 127 ? br : 127;        /* (the voicing's fade, then on down to 12 %) */
            uint16_t wc = ux_mix(T_SURF, ed_acc(2u, c), 25 + 75 * (32767 - bv * bv * 2) / 32767 - (br - bv) * 14 / 63);
            for (j = 0; j < VX_WP; j++) {
                x = bx - ww / 2 + ww * j / (VX_WP - 1);
                y = by - (int32_t)((int64_t)wy[j] * hh / pk);
                if (j) cv_stroke(px, py, x, y, wc);
                px = x;
                py = y;
            }
        }
    }
    {   /* the tiny top hat, over it all, jumping on each note-on */
        static const int16_t CROWN[8] = {-LN(7), 0, -LN(6), -LN(13), LN(6), -LN(13), LN(7), 0};
        int32_t hop = 0, hs = sc * 12 / 10, ox = LN(VX_FX + 5), oy, cx[4], cy[4], top = 0x7FFFFFFF, bot = -0x7FFFFFFF;
        uint16_t hc = ed_acc(3u, c);
        u = now - vx_bt;
        if (vx_bt && u < VX_HOP)
            hop = LN(7) * osc_sine(u * (0x80000000u / VX_HOP)) >> 15;
        oy = fy16 - hyt + LN(3) / 2 - hop;
        for (j = 0; j < 4; j++) {                    /* the crown, tipped */
            cx[j] = ox + ((CROWN[2 * j] * 32237 - CROWN[2 * j + 1] * 5866) >> 15) * hs / 256;
            cy[j] = oy + ((CROWN[2 * j] * 5866 + CROWN[2 * j + 1] * 32237) >> 15) * hs / 256;
            if (cy[j] < top) top = cy[j];
            if (cy[j] > bot) bot = cy[j];
        }
        for (y = (top + 8) >> 4; y <= (bot + 8) >> 4; y++) {   /* filled: the head's line goes behind it */
            int32_t lo = 0x7FFFFFFF, hi = -0x7FFFFFFF, yc = LN(y) + 8;
            for (j = 0; j < 4; j++) {
                int32_t a = j, b = (j + 1) & 3;
                if ((cy[a] <= yc && cy[b] > yc) || (cy[b] <= yc && cy[a] > yc)) {
                    x = cx[a] + (cx[b] - cx[a]) * (yc - cy[a]) / (cy[b] - cy[a]);
                    if (x < lo) lo = x;
                    if (x > hi) hi = x;
                }
            }
            if (hi > lo) cv_rect((lo + 8) >> 4, y, ((hi + 8) >> 4) - ((lo + 8) >> 4) + 1, 1, cv_bg);
        }
        for (j = 0; j < 3; j++)
            cv_stroke(cx[j], cy[j], cx[j + 1], cy[j + 1], hc);
        vx_hat(-LN(12), 0, LN(12), 0, ox, oy, hs, hc);              /* the brim */
        vx_hat(-LN(13) / 2, -LN(4), LN(13) / 2, -LN(4), ox, oy, hs, ux_mix(T_SURF, hc, 55));   /* the band */
    }
}
/* The ARP page (GR_ARP): a ladder. Three dotted rungs an OCT octave (MODE's accent), evenly apart top to bottom. The
 * steps the arp fired scroll left to the right edge, one bar wide (4 steps at the least), each a bar on a rung as long
 * as GATE keeps it (the one sounding in the theme's T_ACCENT, the others their rung's accent at 80%: the four in
 * turn up the rungs; CHORD's notes each on theirs,
 * a PROB rest none) and a tick under its start (RATE's). A step's rung is its note's place among the notes the arp
 * plays (low to high, spread over the rungs: one each when it holds three), taken the first time it is drawn and kept
 * (ar_rung) whatever is held after. Nothing played: the rungs alone; MODE OFF: the word. The steps from seq.c's
 * arp_fired, their starts back from arp_pos step by step (SWG's long and short ones as arp_step times them) */
#define AR_X0 20                                        /* the window; the newest step ends at its right end */
#define AR_W 200
#define AR_TOP 12                                       /* the top rung; the bottom one at AR_BOT */
#define AR_BOT 122
#define AR_TICK 128
#define AR_NH 5                                         /* a note's height */
static uint16_t ar_rung[NPART][ARP_SEEN];               /* a step's rungs, bit r: rung r up from the bottom */
static uint32_t ar_next[NPART];                         /* the first step without its rungs yet */
static uint8_t ar_run[NPART];                           /* seq.c arp_gen when they were taken */
static int32_t ar_x(uint32_t ago, uint32_t win) { return AR_X0 + AR_W - (int32_t)((ago >> 6) * AR_W / (win >> 6)); }
/* the rung of note x among the len notes of list (the same note once), n rungs */
static uint32_t ar_place(const uint32_t *list, uint32_t len, uint32_t x, uint32_t n)
{
    uint32_t i, j, r = 0, l = 0;
    for (i = 0; i < len; i++) {
        for (j = 0; j < i && list[j] != list[i]; j++)
            ;
        if (j < i) continue;
        l++;
        r += list[i] < x;
    }
    if (l < 2u) return (n - 1u) / 2u;
    if (r >= l) r = l - 1u;
    return (r * (n - 1u) + (l - 1u) / 2u) / (l - 1u);
}
static void graph_arp(const track_t *t, uint16_t c)
{
    uint32_t list[65], cnt = 0, len = 0, nch = 1, i, j, k, x, m, back, glen, dur, p = trk_index(t);
    uint32_t idx = t->arp_idx, pos = t->arp_pos >= 0xFFFFFFFu ? 0u : t->arp_pos, nst;
    uint32_t period = div_samples((uint32_t)t->p[P_ARATE]), win = 4u * beat_samples();
    uint32_t sw = (uint32_t)t->p[P_ASWING] * period / 250u, n = 3u * (uint32_t)(t->p[P_AOCT] > 0 ? t->p[P_AOCT] : 1);
    const uint8_t *seen = arp_fired[p];
    int32_t y, gap = (AR_BOT - AR_TOP) / (int32_t)(n - 1u), nh = gap - 1 < AR_NH ? gap - 1 : AR_NH, x0, x1;
    uint16_t dot = ux_mix(T_SURF, ed_acc(0u, c), 30), tick = ed_acc(1u, c);
    if (!t->p[P_AMODE]) {                               /* OFF: the word, as INSERT's */
        const aafont_t *f = large_face_has("OFF") ? &AF_L : &AF_M;
        GFX_HOOK_ALIGN(0, 0, 240, H_GRAPH, AL_HV, "arp off centred");
        cv_text_in(0, f == &AF_L ? CAP_IN(L, H_GRAPH) : CAP_IN(M, H_GRAPH), 240, f, "OFF", T_MID, T_SURF);
        return;
    }
    for (i = 0; i < n; i++)                             /* the rungs */
        for (x0 = AR_X0, y = AR_BOT - (int32_t)i * gap; x0 < AR_X0 + AR_W; x0 += 4)
            cv_rect(x0, y, 2, 1, dot);
    if (win < 4u * period) win = 4u * period;
    nst = idx == 0xFFFFFFFFu ? 0u : idx + 1u < ARP_SEEN ? idx + 1u : ARP_SEEN;
    if (!nst)
        return;                                         /* (nothing played: the rungs alone) */
    if (ar_run[p] != arp_gen[p] || ar_next[p] > idx + 1u || idx + 1u - ar_next[p] > nst) {
        ar_run[p] = arp_gen[p];                         /* (a new chord held, or steps gone by unseen) */
        ar_next[p] = idx + 1u - nst;
    }
    if (ar_next[p] != idx + 1u) {                       /* the new steps' rungs, from the notes it plays now */
        if (t->nheld)
            len = arp_list(t, list, &cnt);
        if (len && t->p[P_AMODE] == AM_UP8 && (x = list[0] + 12u * (uint32_t)t->p[P_AOCT]) < 128u)
            list[len++] = x;                            /* (UP+8's top note) */
        if (t->p[P_AMODE] == AM_CHORD) nch = cnt < 4u ? (cnt ? cnt : 1u) : 4u;   /* (a step's notes from its first) */
        for (k = ar_next[p]; k != idx + 1u; k++)
            for (i = 0, ar_rung[p][k % ARP_SEEN] = 0; seen[k % ARP_SEEN] && i < nch; i++) {
                x = (uint32_t)((int32_t)seen[k % ARP_SEEN] + (i < cnt ? (int32_t)list[i] - (int32_t)list[0] : 0));
                if (x < 128u) ar_rung[p][k % ARP_SEEN] |= (uint16_t)(1u << ar_place(list, len, x, n));
            }
        ar_next[p] = idx + 1u;
    }
    for (j = 0, back = pos; j < nst; j++) {             /* the steps, newest first */
        k = idx - j;
        dur = period + ((k & 1u) ? sw : 0u - sw);
        glen = period * (uint32_t)t->p[P_AGATE] / 128u;
        if (glen > dur) glen = dur;                     /* (the next step cuts it) */
        if (back - (back > glen ? glen : back) >= win) break;
        x0 = ar_x(back < win ? back : win, win);
        x1 = ar_x(back > glen ? back - glen : 0u, win);
        if (back < win) cv_rect(x0, AR_TICK, 1, 5, j ? ux_mix(T_SURF, tick, 55) : tick);
        if (x1 <= x0) x1 = x0 + 1;
        for (i = 0, m = ar_rung[p][k % ARP_SEEN] & ((1u << n) - 1u); m; i++, m >>= 1) {
            uint16_t col = j || !t->arp_note ? ux_mix(T_SURF, ed_acc(i, c), 80) : T_ACCENT;   /* (a colour a rung) */
            if (!(m & 1u)) continue;
            y = AR_BOT - (int32_t)i * gap;
            if (x1 - x0 >= 4 && nh >= 4) cv_rrect(x0, y - nh / 2, x1 - x0, nh, 2, col, T_SURF);
            else cv_rect(x0, y - nh / 2, x1 - x0, nh, col);
        }
        back += period + (((k - 1u) & 1u) ? sw : 0u - sw);   /* (the step before's length) */
    }
}
/* MRC: NOISE's EDIT 1 / EDIT 2: a generative drawing a MODE in place of the scope, from the knobs, the loudest voice
 * and the scope, on the audio clock (silent, it holds still). ANLG: TOPO, contour lines of moving value noise (no
 * impulses drawn); DUST: FLOW, streaks through a noise flow field, no impulses drawn either; LFSR:
 * CONSTELLATION, the register's output cut into words, each pair a point, fading; META: CHLADNI, sand on a plate shaken
 * by the note, gathering on its nodal lines. COLR tilts the noise's octaves as it tilts the sound (0, -3, -6 dB an
 * octave); CRSH (ANLG, DUST) snaps to a grid. The state (the field, the particles, the points, the grains) in one
 * buffer for whichever is shown. Prototype: dev/noise_visuals.html */
#define NZ_SEC 44100u                                /* (samples a second: fx.dly_w's clock) */
#define NZ_TC 5                                      /* TOPO: the grid's cell, px (CRSH: coarser) */
#define NZ_TN ((240 / NZ_TC + 1) * (140 / NZ_TC + 1))
#define NZ_FG 8                                      /* FLOW: the field's grid, px */
#define NZ_FX (240 / NZ_FG + 2)
#define NZ_FY (140 / NZ_FG + 2)
#define NZ_FP 300                                    /* FLOW: the particles */
#define NZ_SN 600                                    /* CONSTELLATION: the points */
#define NZ_CN 1000                                   /* CHLADNI: the grains */
#define NZ_PX0 8                                     /* CHLADNI: the plate */
#define NZ_PY0 4
#define NZ_PW 224
#define NZ_PH 132
static union {
    int16_t topo[NZ_TN];                             /* the field at each corner, Q12 */
    struct { int16_t vx[NZ_FX * NZ_FY], vy[NZ_FX * NZ_FY], x[NZ_FP], y[NZ_FP]; uint16_t age[NZ_FP], life[NZ_FP]; } fl;
    struct { uint32_t t0[NZ_SN]; uint8_t x[NZ_SN], y[NZ_SN], k[NZ_SN]; uint16_t head, n; } cs;
    struct { uint16_t x[NZ_CN], y[NZ_CN]; int8_t vx[NZ_CN], vy[NZ_CN]; } ch;
} nz __attribute__((section(".pool")));
static struct {
    uint32_t last, tp, rng, reg, age, clk;   /* tp: the time moved (samples at full motion) */
    int32_t aud;
    uint8_t kind, sel, note;
} nzs = {0u, 0u, 1u, 1u, 0u, 0u, 0, 0xFFu, 0xFFu, 0u};
typedef struct {
    const int16_t *p;
    uint32_t now, dt, steps, frac;                   /* dt in samples; the register's clock: steps, frac Q16 */
    int32_t lvl, mv, aud, kq, cut, res, col, drn, dn, dv, d16, q, crn, note, wb;   /* Q15, but d16 1/16 st, q px */
    int on;                                          /* a note started */
} nzf_t;
static uint32_t nz_rand(void)
{
    uint32_t x = nzs.rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return nzs.rng = x ? x : 1u;
}
static int32_t nz_gauss(void)                        /* about a unit normal, Q7 */
{
    uint32_t r = nz_rand();
    return (int32_t)((r & 255u) + ((r >> 8) & 255u) + ((r >> 16) & 255u)) - 382;
}
static int32_t nz_ex(int32_t x)                      /* e^-x, x Q15, Q15 out */
{
    int32_t y, ip, f;
    if (x <= 0) return 32768;
    y = (int32_t)((int64_t)x * 47274 >> 15);         /* x / ln 2 */
    ip = y >> 15;
    f = y & 32767;
    if (ip > 14) return 0;
    return (32768 - (f * 21512 >> 15) + ((f * f >> 15) * 5128 >> 15)) >> ip;   /* 2^-f: 1 - 0.6565 f + 0.1565 f^2 */
}
static int32_t nz_exp2(int32_t x)                    /* 2^(x / 256), Q16 (x -2560 .. 2816) */
{
    int32_t ip = x >> 8, f = (x & 255) << 7, r = 65536 + (f * 43024 >> 15) + ((f * f >> 15) * 22512 >> 15);
    return ip >= 0 ? r << ip : r >> -ip;
}
static int32_t nz_h3(int32_t x, int32_t y, int32_t z)   /* a lattice corner's value, -128..127 */
{
    return (int32_t)ed_hash((uint32_t)x * 73856093u ^ (uint32_t)y * 19349663u ^ (uint32_t)z * 83492791u) - 128;
}
#define NZ_L(a, b, t) ((a) + (((b) - (a)) * (t) >> 8))
static int32_t nz_vn(int32_t x, int32_t y, int32_t z)   /* value noise, coordinates Q8, about -1..1 Q15 */
{
    int32_t xi = x >> 8, yi = y >> 8, zi = z >> 8, u = x & 255, v = y & 255, w = z & 255, a, b;
    u = u * u * (768 - 2 * u) >> 16;                 /* (smoothstep) */
    v = v * v * (768 - 2 * v) >> 16;
    w = w * w * (768 - 2 * w) >> 16;
    a = NZ_L(NZ_L(nz_h3(xi, yi, zi), nz_h3(xi + 1, yi, zi), u), NZ_L(nz_h3(xi, yi + 1, zi), nz_h3(xi + 1, yi + 1, zi), u), v);
    b = NZ_L(NZ_L(nz_h3(xi, yi, zi + 1), nz_h3(xi + 1, yi, zi + 1), u),
             NZ_L(nz_h3(xi, yi + 1, zi + 1), nz_h3(xi + 1, yi + 1, zi + 1), u), v);
    return NZ_L(a, b, w) << 8;
}
static int32_t nz_fbm(int32_t x, int32_t y, int32_t z, int32_t k, int32_t oct)   /* k: an octave's gain, Q15 */
{
    int32_t s = 0, n = 0, a = 32768, o;
    for (o = 0; o < oct; o++) {
        s += nz_vn((x << o) + o * 8115, (y << o) + o * 4429, z << o) * a >> 15;
        n += a;
        a = a * k >> 15;
    }
    return (int32_t)((int64_t)s * 32768 / n);
}
static int32_t nz_smp(uint32_t i)                    /* a recent output sample, x 3, Q15 */
{
    return clamp(scope_buf[(scope_w - 1u - ((i * 31u) & 255u)) & (SCOPE_N - 1u)] * 3, -32767, 32767);
}
static int32_t nz_sn(const nzf_t *f, int32_t v)      /* CRSH: v (1/16 px) to the middle of its q px cell */
{
    int32_t q = f->q;
    if (q <= 1) return v;
    v >>= 4;
    return LN((v >= 0 ? v / q : (v - q + 1) / q) * q) + LN(q) / 2;
}
static int32_t nz_pct(const nzf_t *f, int32_t pct)   /* CRSH: fewer shades */
{
    int32_t l;
    if (f->q > 1) {
        l = 9 - 7 * f->crn / 32768;
        if (l < 2) l = 2;
        pct = (pct * l + 50) / 100 * 100 / l;
    }
    return clamp(pct, 0, 100);
}
static void nz_frame(nzf_t *f, const track_t *t)
{
    const int16_t *p = t->p;
    const voice_t *lv = 0;
    uint32_t i, age = 0, mode = (uint32_t)p[P_E0] & 3u;
    int32_t lvl = 0, drft = clamp(p[P_E6], 0, 127), cut, s = 0, a, dn = 0;
    f->p = p;
    f->now = fx.dly_w;
    f->dt = f->now - nzs.last;
    if (f->dt > NZ_SEC / 20u) f->dt = NZ_SEC / 20u;   /* (back on the page: one frame's worth) */
    nzs.last = f->now;
    for (i = 0; i < NVOICE; i++)                     /* the loudest voice */
        if (t->v[i].active) {
            if (t->v[i].env >> 9 > lvl) {
                lvl = t->v[i].env >> 9;
                lv = &t->v[i];
            }
            if (t->v[i].age > age) age = t->v[i].age;
        }
    lvl = clamp(lvl, 0, 32767);
    if (lvl < 66) lvl = 0;
    f->on = age > nzs.age;                           /* (voice.c ages each voice: a newer one, a new note) */
    if (age > nzs.age || age + 0x10000u < nzs.age)
        nzs.age = age;
    if (lv) {
        nzs.note = lv->note;
        if (drft) dn = clamp((int32_t)lv->ph[2] >> 4, -32767, 32767);   /* the drift, Q19 in ph[2] */
    }
    f->note = nzs.note ? nzs.note : 60;
    for (i = 0; i < 128u; i++) {                     /* the level heard: the scope's last 128 samples */
        int32_t v = scope_buf[(scope_w - 1u - i) & (SCOPE_N - 1u)] >> 4;
        s += v * v >> 4;
    }
    a = clamp((int32_t)isqrt32((uint32_t)s / 128u) * 320, 0, 32767);
    nzs.aud += (a - nzs.aud) * (a > nzs.aud ? 19661 : 4915) >> 15;
    f->lvl = lvl;
    f->aud = nzs.aud;
    f->mv = clamp(lvl * (22938 + (19661 * nzs.aud >> 15)) >> 15, 0, 32767);   /* how much moves: 0.7 .. 1.3 x lvl */
    nzs.tp += f->dt * (uint32_t)f->mv >> 15;
    a = clamp(p[P_E1], 0, 127);
    f->col = a * 258;
    f->kq = a <= 64 ? 32768 - a * 150 : 23168 - (a - 64) * 108;   /* 2^(-COLR / 127): -3 dB at 64, -6 at 127 */
    f->res = clamp(p[P_E3], 0, 127) * 258;
    f->drn = drft * 258;
    f->dn = dn;
    f->dv = dn * drft / 127;
    f->d16 = ((dn >> 7) * drft * drft) >> 14;        /* (as noise_render: up to about 16 semitones) */
    cut = (p[P_E2] << 8) + (((f->note * 16 - 960) * p[P_E4] * 151) >> 10) + ((f->d16 * 4785) >> 8);
    f->cut = clamp(cut, 0, 127 << 8) * 32767 / (127 << 8);
    f->q = mode < NZ_LFSR && p[P_E7] > 0 ? 1 + (p[P_E7] >> 3) : 1;
    f->crn = mode < NZ_LFSR ? clamp(p[P_E7], 0, 127) * 258 : 0;
    f->steps = f->frac = 0;
    if (mode >= NZ_LFSR) {                           /* the register's clock, slowed: 5 a second at C4, 60 at most */
        int32_t x = ((f->note + clamp(p[P_E7], 0, 127) - 60) * 16 + f->d16) * 8 / 9;
        uint32_t r = (uint32_t)(5 * nz_exp2(clamp(x, -2560, 2048)));
        if (r > 60u << 16) r = 60u << 16;
        nzs.clk += (uint32_t)((uint64_t)r * f->dt * (uint32_t)(lvl > 8191 ? 32767 : lvl * 4) / NZ_SEC >> 15);
        f->steps = nzs.clk >> 16;
        nzs.clk &= 0xFFFFu;
        f->frac = nzs.clk;
    }
}
/* TOPO: the edges a level crosses in a cell, two segments at most (0 top, 1 right, 2 bottom, 3 left; -1 none) */
static const int8_t NZ_MS[16][4] = {
    {-1, -1, -1, -1}, {3, 2, -1, -1}, {2, 1, -1, -1}, {3, 1, -1, -1}, {0, 1, -1, -1}, {0, 1, 3, 2}, {0, 2, -1, -1},
    {3, 0, -1, -1}, {3, 0, -1, -1}, {0, 2, -1, -1}, {3, 0, 2, 1}, {0, 1, -1, -1}, {3, 1, -1, -1}, {2, 1, -1, -1},
    {3, 2, -1, -1}, {-1, -1, -1, -1}};
static void nz_edge(int32_t e, int32_t x0, int32_t y0, int32_t l, const int32_t *v, int32_t th, int32_t *x, int32_t *y)
{
    switch (e) {                                     /* (v: the corners tl tr br bl; a crossed edge's ends differ) */
    case 0: *x = x0 + l * (th - v[0]) / (v[1] - v[0]); *y = y0; break;
    case 1: *x = x0 + l; *y = y0 + l * (th - v[1]) / (v[2] - v[1]); break;
    case 2: *x = x0 + l * (th - v[3]) / (v[2] - v[3]); *y = y0 + l; break;
    default: *x = x0; *y = y0 + l * (th - v[0]) / (v[3] - v[0]); break;
    }
}
static void nz_topo(const nzf_t *f, uint16_t c)
{
    int32_t cs = f->q > NZ_TC ? f->q : NZ_TC, nx = 240 / cs + 1, ny = 140 / cs + 1, i, j, k;
    int32_t amp = 16384 + 26214 * f->mv / 32768, ox = f->dv * 200 >> 15, z = (int32_t)(nzs.tp / 862u);
    int32_t nl = 3 + 9 * f->cut / 32767, lf, wid;
    for (j = 0; j < ny; j++)                         /* the field: 0.013 a px, 0.2 a second of motion */
        for (i = 0; i < nx; i++) {
            int32_t h = nz_fbm(i * cs * 213 / 64 + ox, j * cs * 213 / 64, z, f->kq, 3);
            nz.topo[j * nx + i] = (int16_t)clamp((int32_t)((int64_t)h * 23 / 10 * amp >> 15) >> 3, -32767, 32767);
        }
    lf = (osc_sine(nzs.tp * 17045u) * 3 / 4) >> 3;   /* the level RES picks out, sweeping (Q12) */
    wid = 410 + (int32_t)((int64_t)(32767 - f->res) * 2458 >> 15);   /* how near to it stays lit: 0.1 .. 0.7 */
    for (k = 0; k < nl; k++) {
        int32_t th = -4096 + (2 * k + 1) * 4096 / nl, dd = (th - lf) * 4096 / wid, foc, wgt, pct, el;
        uint16_t col;
        foc = nz_ex((int32_t)((int64_t)dd * dd >> 9));
        wgt = 32768 + ((foc - 32768) * f->res >> 15);
        pct = nz_pct(f, ((20 + 65 * f->mv / 32768) * wgt >> 15) + 6);
        el = nl > 1 ? k * 4 / (nl - 1) : 2;          /* the colour by height: the lowest the fourth accent, peaks the first */
        col = ux_mix(T_SURF, ed_acc(3u - (uint32_t)(el > 3 ? 3 : el), c), pct);
        if (pct <= 0) continue;
        for (j = 0; j < ny - 1; j++)
            for (i = 0; i < nx - 1; i++) {
                int32_t v[4], code, jit, x0, y0, xa, ya, xb, yb;
                const int8_t *ms;
                v[0] = nz.topo[j * nx + i];
                v[1] = nz.topo[j * nx + i + 1];
                v[2] = nz.topo[(j + 1) * nx + i + 1];
                v[3] = nz.topo[(j + 1) * nx + i];
                code = (v[0] > th) << 3 | (v[1] > th) << 2 | (v[2] > th) << 1 | (v[3] > th);
                if (!code || code == 15) continue;
                ms = NZ_MS[code];
                x0 = LN(i * cs);
                y0 = LN(j * cs);
                jit = (nz_smp((uint32_t)(i * 7 + j * 13 + k * 3)) * 13 >> 15) * f->mv >> 15;   /* the hiss: 0.8 px */
                nz_edge(ms[0], x0, y0, LN(cs), v, th, &xa, &ya);
                nz_edge(ms[1], x0, y0, LN(cs), v, th, &xb, &yb);
                cv_stroke(xa + jit, ya - jit, xb + jit, yb - jit, col);
                if (ms[2] >= 0) {
                    nz_edge(ms[2], x0, y0, LN(cs), v, th, &xa, &ya);
                    nz_edge(ms[3], x0, y0, LN(cs), v, th, &xb, &yb);
                    cv_stroke(xa + jit, ya - jit, xb + jit, yb - jit, col);
                }
            }
    }
}
/* FLOW: the field on an 8 px grid (a unit vector each), the particles following it */
static void nz_oct(int32_t *vx, int32_t *vy)         /* CRSH: the nearest of eight directions */
{
    int32_t ax = *vx < 0 ? -*vx : *vx, ay = *vy < 0 ? -*vy : *vy, sx = *vx < 0 ? -1 : 1, sy = *vy < 0 ? -1 : 1;
    if (ax * 5 > ay * 12) { *vx = sx * 32767; *vy = 0; }
    else if (ay * 5 > ax * 12) { *vx = 0; *vy = sy * 32767; }
    else { *vx = sx * 23170; *vy = sy * 23170; }
}
static void nz_flow_field(const nzf_t *f)
{
    int32_t i, j, rr = 14 + 36 * f->cut / 32768, k = f->res * 23 / 25, z = (int32_t)(nzs.tp / 492u), ox = f->dv * 512 >> 15;
    for (j = 0; j < NZ_FY; j++)
        for (i = 0; i < NZ_FX; i++) {
            int32_t x = i * NZ_FG, y = j * NZ_FG, n = nz_fbm(x * 262 / 64 + ox, y * 262 / 64, z, f->kq, 3), vx, vy;
            uint32_t a = (uint32_t)n * 146028u + (uint32_t)f->dv * 25033u;   /* (fbm x 7 radians, DRFT leaning it 1.2) */
            vx = osc_sine(a + 0x40000000u);
            vy = osc_sine(a);
            if (k) {                                 /* RES: steered into an orbit, a ring rr px round the middle */
                int32_t dx = (x - 120) * 10 / 16, dy = y - 70, r = (int32_t)isqrt32((uint32_t)(dx * dx + dy * dy)) + 1;
                int32_t ux = dx * 32767 / r, uy = dy * 32767 / r, del = clamp((r - rr) * 5 * 16384 / rr, -42598, 42598);
                int32_t cd = osc_sine((uint32_t)del * 20861u + 0x40000000u), sd = osc_sine((uint32_t)del * 20861u), m;
                int32_t rx = ((-uy * cd) >> 15) - ((ux * sd) >> 15), ry = ((ux * cd) >> 15) - ((uy * sd) >> 15);
                rx = rx * 16 / 10;                   /* (the ring is 1.6 times as wide as tall) */
                m = (int32_t)isqrt32((uint32_t)((rx >> 2) * (rx >> 2) + (ry >> 2) * (ry >> 2))) + 1;
                rx = rx * 8192 / m;
                ry = ry * 8192 / m;
                vx = (vx * (32768 - k) + rx * k) >> 15;
                vy = (vy * (32768 - k) + ry * k) >> 15;
            }
            if (f->q > 1) nz_oct(&vx, &vy);
            nz.fl.vx[j * NZ_FX + i] = (int16_t)clamp(vx, -32767, 32767);
            nz.fl.vy[j * NZ_FX + i] = (int16_t)clamp(vy, -32767, 32767);
        }
}
static void nz_dir(int32_t x, int32_t y, int32_t *dx, int32_t *dy)   /* the field at (x, y) 1/16 px, about unit Q15 */
{
    int32_t gx = clamp(x, 0, LN(NZ_FG * (NZ_FX - 1)) - 1), gy = clamp(y, 0, LN(NZ_FG * (NZ_FY - 1)) - 1);
    int32_t i = gx / LN(NZ_FG), j = gy / LN(NZ_FG), u = gx % LN(NZ_FG) * 2, v = gy % LN(NZ_FG) * 2, k = j * NZ_FX + i;
    int32_t vx = NZ_L(NZ_L(nz.fl.vx[k], nz.fl.vx[k + 1], u), NZ_L(nz.fl.vx[k + NZ_FX], nz.fl.vx[k + NZ_FX + 1], u), v);
    int32_t vy = NZ_L(NZ_L(nz.fl.vy[k], nz.fl.vy[k + 1], u), NZ_L(nz.fl.vy[k + NZ_FX], nz.fl.vy[k + NZ_FX + 1], u), v);
    int32_t ax = vx < 0 ? -vx : vx, ay = vy < 0 ? -vy : vy, m = (ax > ay ? ax + ay * 3 / 8 : ay + ax * 3 / 8);
    if (m < 64) m = 64;
    *dx = vx * 32767 / m;
    *dy = vy * 32767 / m;
}
static void nz_flow_spawn(const nzf_t *f, uint32_t i)
{
    nz.fl.age[i] = 0;
    nz.fl.life[i] = (uint16_t)(800u + nz_rand() % 2500u);   /* ms */
    if ((int32_t)(nz_rand() & 32767u) < f->res * 9 / 10) {   /* RES: born on the ring */
        uint32_t a = nz_rand();
        int32_t r = LN(14 + 36 * f->cut / 32768) + nz_gauss() * LN(5) / 128;
        nz.fl.x[i] = (int16_t)clamp(LN(120) + (r * 16 / 10 * osc_sine(a + 0x40000000u) >> 15), 0, LN(240));
        nz.fl.y[i] = (int16_t)clamp(LN(70) + (r * osc_sine(a) >> 15), 0, LN(140));
    } else {
        nz.fl.x[i] = (int16_t)(nz_rand() % (uint32_t)LN(240));
        nz.fl.y[i] = (int16_t)(nz_rand() % (uint32_t)LN(140));
    }
}
static void nz_flow(const nzf_t *f, uint16_t c)
{
    int32_t n = 110 + 190 * f->mv / 32768, sp = (8 + 64 * f->cut / 32768) * f->mv / 32768 * LN(1) * 256;
    int32_t sp8 = (int32_t)((int64_t)sp * (int32_t)f->dt / (int32_t)NZ_SEC);   /* this frame's step, 1/16 px Q8 */
    int32_t len = 2 + (3 + 9 * f->cut / 32768) * (9830 + 22938 * f->mv / 32768) / 32768, stp = f->q * 12 > LN(2) ? f->q * 12 : LN(2);
    uint32_t dage = (f->dt * 1000u / NZ_SEC) * (uint32_t)f->mv * 6u / 5u >> 15, i;
    for (i = 0; i < (uint32_t)n; i++) {
        int32_t dx, dy, x, y, jx, jy, k, px, py, m, fade, pct;
        uint16_t col;
        nz_dir(nz.fl.x[i], nz.fl.y[i], &dx, &dy);
        x = nz.fl.x[i] + (int32_t)((int64_t)dx * sp8 >> 23);
        y = nz.fl.y[i] + (int32_t)((int64_t)dy * sp8 >> 23);
        nz.fl.age[i] = (uint16_t)(nz.fl.age[i] + dage);
        if (nz.fl.age[i] > nz.fl.life[i] || x < -LN(4) || x > LN(244) || y < -LN(4) || y > LN(144)) {
            nz_flow_spawn(f, i);
            continue;
        }
        nz.fl.x[i] = (int16_t)x;
        nz.fl.y[i] = (int16_t)y;
        m = nz.fl.age[i] < nz.fl.life[i] - nz.fl.age[i] ? nz.fl.age[i] : nz.fl.life[i] - nz.fl.age[i];
        fade = clamp(m * 3 * 32768 / 1000, 8192, 32768);
        pct = nz_pct(f, (20 + 60 * f->mv / 32768) * fade >> 15);
        if (pct <= 0) continue;
        col = ux_mix(T_SURF, i % 6u ? ed_acc(2u, c) : T_TEXT, pct);   /* (FREQ's accent, one in six the cream) */
        jx = (nz_smp(i * 7u) * 24 >> 15) * f->mv >> 15;   /* the hiss: 1.5 px */
        jy = (nz_smp(i * 7u + 3u) * 24 >> 15) * f->mv >> 15;
        px = nz_sn(f, x + jx);
        py = nz_sn(f, y + jy);
        for (k = 1; k < len; k++) {                  /* the streak, back along the field */
            int32_t qx, qy;
            nz_dir(x, y, &dx, &dy);
            x -= dx * stp >> 15;
            y -= dy * stp >> 15;
            qx = nz_sn(f, x + jx);
            qy = nz_sn(f, y + jy);
            cv_stroke(px, py, qx, qy, col);
            px = qx;
            py = qy;
        }
    }
}
/* CONSTELLATION: the output stream cut into words of up to 8 bits, each pair of words a point (x, y), four a clock.
 * A short register's few points fall on a lattice; a long one fills the field like noise */
static uint32_t nz_word(uint32_t taps, uint32_t wb)
{
    uint32_t w = 0, k;
    for (k = 0; k < wb; k++) {
        w = w << 1 | (nzs.reg & 1u);
        nzs.reg = (nzs.reg >> 1) ^ (-(nzs.reg & 1u) & taps);
    }
    return w;
}
static void nz_sxy(const nzf_t *f, uint32_t k, int32_t ca, int32_t sa, int32_t *x, int32_t *y)   /* point k, 1/16 px */
{
    int32_t m = (1 << f->wb) - 1, u = nz.cs.x[k] * 32768 / m - 16384, v = nz.cs.y[k] * 32768 / m - 16384;
    *x = LN(120) + (((u * ca >> 15) - (v * sa >> 15)) * LN(200) >> 15);
    *y = LN(70) + (((u * sa >> 15) + (v * ca >> 15)) * LN(124) >> 15);
}
static void nz_stars(nzf_t *f, uint16_t c)
{
    const int16_t *p = f->p;
    uint32_t sel = ((uint32_t)p[P_E5] >> 4) & 7u, taps = NOISE_TAPS[sel], mask = taps, i, s, j;
    uint32_t keep = NZ_SEC * 8u / 10u + (uint32_t)((uint64_t)NZ_SEC * 6u * (uint32_t)(f->col * f->col >> 15) >> 15);   /* COLR */
    uint32_t ph = (uint32_t)(nzs.tp * 13530u);       /* (DRFT's sway: 2 rad a second of motion) */
    int32_t ang = (f->dv / 2 + (f->dv * 3 / 20 * osc_sine(ph) >> 15)), ca, sa, sz = 1 + (2 * (32767 - f->cut) + 16384) / 32768;
    int32_t lv = f->lvl > 9830 ? f->lvl : 9830;
    uint16_t sc[4];
    mask |= mask >> 1; mask |= mask >> 2; mask |= mask >> 4; mask |= mask >> 8; mask |= mask >> 16;
    for (f->wb = 0; f->wb < 8 && mask >> f->wb; f->wb++) {}   /* (the register's width, 8 at most) */
    if (nzs.sel != sel || f->on || !(nzs.reg & mask)) {   /* the register from the note's seed (noise_note_on); no points */
        nzs.sel = (uint8_t)sel;
        nzs.reg = ((f->note + 1u) * 0x9E3779B9u & mask) | 1u;
        nz.cs.n = nz.cs.head = 0;
    }
    for (s = 0; s < f->steps && s < 16u; s++)        /* four points a clock */
        for (j = 0; j < 4u; j++) {
            uint32_t k = nz.cs.head;
            nz.cs.x[k] = (uint8_t)nz_word(taps, (uint32_t)f->wb);
            nz.cs.y[k] = (uint8_t)nz_word(taps, (uint32_t)f->wb);
            nz.cs.t0[k] = f->now;
            nz.cs.k[k] = (uint8_t)(nz_rand() & 3u);
            nz.cs.head = (uint16_t)((k + 1u) % NZ_SN);
            if (nz.cs.n < NZ_SN) nz.cs.n++;
        }
    ca = osc_sine((uint32_t)ang * 20861u + 0x40000000u);   /* (ang Q15 radians) */
    sa = osc_sine((uint32_t)ang * 20861u);
    sc[0] = ed_acc(0u, c); sc[1] = ed_acc(1u, c); sc[2] = ed_acc(2u, c); sc[3] = T_TEXT;
    if (f->res > 655 && nz.cs.n > 1u) {              /* RES: the path through the last 40 */
        uint32_t m = nz.cs.n < 40u ? nz.cs.n : 40u;
        for (i = 1; i < m; i++) {
            uint32_t k1 = (nz.cs.head + NZ_SN - i) % NZ_SN, k0 = (k1 + NZ_SN - 1u) % NZ_SN;
            int32_t x0, y0, x1, y1;
            nz_sxy(f, k0, ca, sa, &x0, &y0);
            nz_sxy(f, k1, ca, sa, &x1, &y1);
            cv_stroke(x0, y0, x1, y1, ux_mix(T_SURF, ed_acc(3u, c), 60 * f->res / 32768 * (int32_t)(40u - i) / 40));
        }
    }
    for (i = nz.cs.n; i-- > 0u;) {                   /* oldest first, the newest four on top with a cross */
        uint32_t k = (nz.cs.head + NZ_SN - 1u - i) % NZ_SN, age = f->now - nz.cs.t0[k];
        int32_t x, y, pct;
        if (age > keep * 3u) continue;
        nz_sxy(f, k, ca, sa, &x, &y);
        x >>= 4;
        y >>= 4;
        if (i < 4u) {
            cv_rect(x - sz / 2, y - sz / 2, sz, sz, sc[0]);
            cv_rect(x - 3, y, 7, 1, sc[0]);
            cv_rect(x, y - 3, 1, 7, sc[0]);
            continue;
        }
        pct = nz_ex((int32_t)((uint64_t)age * 32768u / keep)) * (13107 + (19661 * lv >> 15)) / 327 >> 15;
        if (pct > 0) cv_rect(x - sz / 2, y - sz / 2, sz, sz, ux_mix(T_SURF, sc[nz.cs.k[k]], pct));
    }
}
/* CHLADNI: cos(n pi x) cos(m pi y) +- cos(m pi x) cos(n pi y) on the plate; a grain jumps by how much its spot moves */
static const uint8_t NZ_DM[8] = {4, 4, 3, 3, 2, 2, 1, 1};   /* m - n by LEN: the long sequences, the busier figures */
static int32_t nz_cos(int32_t n, uint32_t x) { return osc_sine(((uint32_t)n * x << 15) + 0x40000000u); }   /* x Q16 */
static int32_t nz_plate(int32_t n, int32_t m, int32_t sg, uint32_t x, uint32_t y)
{
    return (nz_cos(n, x) * nz_cos(m, y) >> 15) + sg * (nz_cos(m, x) * nz_cos(n, y) >> 15);
}
static int32_t nz_pow(int32_t a, int32_t pq)         /* about a^(pq / 256), a Q15, pq 128 .. 768 */
{
    int32_t a2 = a * a >> 15, s;
    if (pq < 256) {
        s = (int32_t)isqrt32((uint32_t)a << 15);
        return s + ((a - s) * (pq - 128) >> 7);
    }
    if (pq < 512) return a + ((a2 - a) * (pq - 256) >> 8);
    return a2 + (((a2 * a >> 15) - a2) * (pq - 512) >> 8);
}
static void nz_chladni(const nzf_t *f, uint16_t c)
{
    const int16_t *p = f->p;
    uint32_t sel = ((uint32_t)p[P_E5] >> 4) & 7u, i;
    int32_t n = 1 + (4 * f->cut + 16384) / 32768 + clamp(p[P_E7], 0, 127) / 32, m = n + NZ_DM[sel], sg = sel & 1u ? -1 : 1;
    int32_t wm = f->drn * (16384 + f->dn / 2) >> 15;   /* DRFT: towards the next mode up */
    int32_t en = clamp(f->lvl * (26214 + 22938 * f->aud / 32768) >> 15, 0, 39321);   /* how hard it shakes */
    int32_t pq = 154 + 461 * f->res / 32768, rho = 113 * f->col / 32768, inn = (int32_t)isqrt32((uint32_t)(16384 - rho * rho));
    int32_t step = 1966 * en >> 15;                  /* (3 % of the plate a jump, at the most) */
    uint16_t lut[3][8];
    uint32_t k;
    for (k = 0; k < 8u; k++) {                       /* the cream, and a dust of the second and the fourth accents */
        int32_t pct = 30 + 70 * (int32_t)k / 7;
        lut[0][k] = ux_mix(T_SURF, T_TEXT, pct);
        lut[1][k] = ux_mix(T_SURF, ed_acc(1u, c), pct);
        lut[2][k] = ux_mix(T_SURF, ed_acc(3u, c), pct);
    }
    for (i = 0; i < NZ_CN; i++) {
        uint32_t x = nz.ch.x[i], y = nz.ch.y[i];
        int32_t a = nz_plate(n, m, sg, x, y), u;
        if (wm > 0) a += (nz_plate(n + 1, m + 1, sg, x, y) - a) * wm >> 15;
        a = a < 0 ? -a : a;
        a = a >= 65534 ? 32767 : a >> 1;
        if (step) {                                  /* COLR: white jumps afresh, brown keeps its way (momentum) */
            int32_t s = step * nz_pow(a, pq) >> 15, vx, vy, nx, ny;
            vx = clamp((nz.ch.vx[i] * rho + nz_gauss() * inn) >> 7, -127, 127);
            vy = clamp((nz.ch.vy[i] * rho + nz_gauss() * inn) >> 7, -127, 127);
            nz.ch.vx[i] = (int8_t)vx;
            nz.ch.vy[i] = (int8_t)vy;
            nx = (int32_t)x + (vx * s >> 7);
            ny = (int32_t)y + (vy * s * NZ_PW / NZ_PH >> 7);
            if (nx < 0) nx = -nx;
            if (nx > 65535) nx = 131070 - nx;
            if (ny < 0) ny = -ny;
            if (ny > 65535) ny = 131070 - ny;
            nz.ch.x[i] = (uint16_t)clamp(nx, 0, 65535);
            nz.ch.y[i] = (uint16_t)clamp(ny, 0, 65535);
        }
        u = (32768 - a) * (32768 - a) >> 15;         /* the still ones brightest */
        cv_pset(NZ_PX0 + (int32_t)(nz.ch.x[i] * NZ_PW >> 16), NZ_PY0 + (int32_t)(nz.ch.y[i] * NZ_PH >> 16),
                lut[i % 5u == 1u ? 1 : i % 5u == 3u ? 2 : 0][u * 7 >> 15]);
    }
}
static void graph_noise(const track_t *t, uint16_t c)
{
    nzf_t f;
    uint32_t mode = (uint32_t)t->p[P_E0] & 3u, i;
    nz_frame(&f, t);
    if (nzs.kind != mode) {                          /* the buffer: another drawing's now */
        nzs.kind = (uint8_t)mode;
        nzs.sel = 0xFFu;
        for (i = 0; mode == NZ_DUST && i < NZ_FP; i++) {
            nz_flow_spawn(&f, i);
            nz.fl.age[i] = (uint16_t)(nz_rand() % nz.fl.life[i]);
        }
        for (i = 0; mode == NZ_META && i < NZ_CN; i++) {
            nz.ch.x[i] = (uint16_t)nz_rand();
            nz.ch.y[i] = (uint16_t)nz_rand();
            nz.ch.vx[i] = nz.ch.vy[i] = 0;
        }
    }
    if (mode == NZ_ANLG) nz_topo(&f, c);
    else if (mode == NZ_DUST) {
        nz_flow_field(&f);
        nz_flow(&f, c);
    } else if (mode == NZ_LFSR) nz_stars(&f, c);
    else nz_chladni(&f, c);
}
/* MRC: DRUM's EDIT 1 / EDIT 2: the kit from the front (dev/drum_visuals.html STD 1), in place of the scope, the same
 * for every KIT but for the pieces it swaps in (drum_swaps: a conga for the second rack tom and the floor tom, claves
 * by the snare, a ride for the crash; 66 and PERC's two-sine bell a hand bell, the others' a cowbell). Each piece is
 * struck by its lane's hits (eng_drum.c drum_hit_n, timed here on the audio clock, fx.dly_w), ringing out at its
 * velocity over its own time, DECY's 1/4 .. x2; a closed hat chokes the open one. TUNE sizes the pieces, DECY the
 * rings running over a struck head (1 .. 4), TONE a shine on the heads and the cymbals' grooves, SNAP the snare's
 * wires and the metal's sparks, ACC how far a hit throws its drum, KICK the beater (PUNCH a block, ROUND a felt ball),
 * DRV shakes the pieces while they ring. The MRC accents by knob as the drawings above: shells KNOB 2's, metal and the
 * beater KNOB 3's, rings KNOB 4's, wires and sparks KNOB 1's; heads TEXT, lit by the hit; the stands MID. Each piece
 * dimmed to its lane's level (P_LN0 ..), here and on EDIT > LANES / LANES 2, which draw the same kit */
#define DK_FULL 0xFFFFFFFFu
#define DK_HALF 0x80000000u
#define DK_HEAD 697300u                              /* a struck head's wobble: 7.2 Hz, a turn 2^32 per sample */
#define DK_KHEAD 464900u                             /* the kick's: 4.8 Hz */
#define DK_BUZZ 1392000u                             /* the snare's wires: 14 Hz */
#define DK_SWAY 248000u                              /* a cymbal's sway: 2.5 Hz */
#define DK_FLAP 356400u                              /* .. its flapping: 3.7 Hz */
#define DK_HATW 930000u                              /* the hats' chatter: 9.5 Hz */
#define DK_SWING 278500u                             /* a bell's swing: 2.9 Hz */
static const uint16_t DK_TAU[DV_NLANE] = {320, 180, 220, 60, 420, 340, 80, 500};   /* each lane's ring at DECY 64, ms */
static struct {
    uint32_t w[DV_NLANE];                            /* each lane's last hit, fx.dly_w */
    uint8_t n[DV_NLANE], v[DV_NLANE], part;          /* .. its count (drum_hit_n) and velocity; the part followed */
    uint32_t now;                                    /* (the frame's, below) */
    int32_t e[DV_NLANE], f[DV_NLANE], all, jit;      /* rings and flashes, Q15; the loudest; DRV's shake, 1/16 px */
    int32_t tone, decy, snap, acc, kick;
    int32_t lv[DV_NLANE], dim;                       /* each lane's piece at its level (EDIT > LANES), %; the piece drawn's */
    uint16_t c;
} dk = {.part = 0xFF, .dim = 100};
static int32_t dk_sqrt(int32_t v)
{
    int32_t r = 0, b = 1 << 30;
    if (v <= 0) return 0;
    while (b > v) b >>= 2;
    for (; b; b >>= 2)
        if (v >= r + b) {
            v -= r + b;
            r = (r >> 1) + b;
        } else
            r >>= 1;
    return r;
}
/* a lane's ring now, Q15: the hit's velocity dying away over tau ms (DECY's 1/4 .. x2 when decy >= 0) */
static int32_t dk_lvl(uint32_t l, uint32_t tau, int32_t decy)
{
    uint32_t age = dk.now - dk.w[l], x;
    int32_t a, b;
    if (decy >= 0) {                                 /* 2^((DECY - 64) / 32), linear between the octaves, Q8 */
        int32_t e = decy - 64, k = e >= 0 ? 256 << (e >> 5) : 256 >> -(e >> 5);
        tau = tau * (uint32_t)(k + k * (e & 31) / 32) / 256u;
    }
    tau = tau * 441u / 10u;                          /* (in samples) */
    if (!tau || age >= tau * 12u)
        return 0;
    x = age * 369u / tau;                            /* age / tau / ln 2, Q8: 2^-x */
    a = 32768 >> (x >> 8);
    b = a >> 1;
    return (a - (a - b) * (int32_t)(x & 255u) / 256) * dk.v[l] / 127;
}
static int32_t dk_hot(int32_t l) { return l < 0 ? 0 : dk.e[l] > dk.f[l] ? dk.e[l] : dk.f[l]; }
static uint16_t dk_mix(uint16_t c, int32_t pct) { return ux_mix(T_SURF, c, clamp(pct, 0, 100) * dk.dim / 100); }
static uint16_t dk_ink(uint16_t c) { return dk_mix(c, 100); }
static void dk_dim(int32_t l) { dk.dim = l < 0 ? 100 : dk.lv[l]; }   /* the pieces of lane l next (-1: none) */
static uint16_t dk_headc(int32_t l) { return dk_mix(T_TEXT, 62 + 38 * dk_hot(l) / 32768); }
static uint16_t dk_metal(int32_t l) { return dk_mix(ed_acc(2u, dk.c), 72 + 14 * dk.tone / 127 + 14 * dk_hot(l) / 32768); }
/* DRV: a piece's place shaken while the kit rings */
static void dk_shake(int32_t *x, int32_t *y, uint32_t id)
{
    uint32_t fr = dk.now / 1500u;
    if (!dk.jit) return;
    *x += ((int32_t)ed_hash(id * 7u + fr) - 128) * dk.jit / 128;
    *y += ((int32_t)ed_hash(id * 13u + fr + 5u) - 128) * dk.jit / 128;
}
/* an ellipse's arc from a0 over span (a turn: 2^32; y down), 1/16 px in, its strokes ~2 px */
static void dk_arc(int32_t cx, int32_t cy, int32_t rx, int32_t ry, uint32_t a0, uint32_t span, int dot, uint16_t c)
{
    int32_t n = (rx + ry) / LN(2) * (int32_t)(span >> 24) / 256 + 6;
    if (rx < LN(1) / 2) return;
    gn_arc(cx, cy, rx, ry, a0, span / (uint32_t)n, n, dot, c);
}
/* an ellipse turned rot (a turn: 2^32) */
static void dk_rell(int32_t cx, int32_t cy, int32_t rx, int32_t ry, uint32_t rot, int dot, uint16_t c)
{
    int32_t n = (rx + ry) / LN(2) + 8, cr = osc_sine(rot + 0x40000000u), sr = osc_sine(rot), px = 0, py = 0, i;
    for (i = 0; i <= n; i++) {
        uint32_t a = DK_FULL / (uint32_t)n * (uint32_t)i;
        int32_t x0 = rx * osc_sine(a + 0x40000000u) >> 15, y0 = ry * osc_sine(a) >> 15;
        int32_t x = cx + ((x0 * cr - y0 * sr) >> 15), y = cy + ((x0 * sr + y0 * cr) >> 15);
        if (i && (!dot || (i & 1)))
            cv_stroke(px, py, x, y, c);
        px = x;
        py = y;
    }
}
/* an ellipse filled with the panel: what lies behind a piece hidden */
static void dk_fill(int32_t cx, int32_t cy, int32_t rx, int32_t ry)
{
    int32_t y;
    if (ry < 1) return;
    for (y = (cy - ry) >> 4; y <= (cy + ry) >> 4; y++) {
        int32_t dy = LN(y) + 8 - cy, hw, x0, x1;
        if (dy < -ry || dy > ry) continue;
        hw = rx * dk_sqrt(ry * ry - dy * dy) / ry;
        x0 = (cx - hw + 8) >> 4;
        x1 = (cx + hw + 8) >> 4;
        cv_rect(x0, y, x1 - x0, 1, T_SURF);
    }
}
/* DECY: rings running out over a struck head, 0.9 of its size */
static void dk_ripples(int32_t cx, int32_t cy, int32_t rx, int32_t ry, uint32_t l)
{
    int32_t e = dk.e[l], n = 1 + (dk.decy + 21) / 42, k;
    uint32_t age = dk.now - dk.w[l];
    if (e < 983) return;                             /* (3 %) */
    for (k = 0; k < n; k++) {
        int32_t r = (int32_t)((age / 72u + (uint32_t)(k * 256 / n)) & 255u);   /* 2.4 a second, Q8 */
        dk_arc(cx, cy, rx * r * 230 / 65536, ry * r * 230 / 65536, 0u, DK_FULL, 0,
               dk_mix(ed_acc(3u, dk.c), 110 * e / 32768 * (256 - r) / 256 * (64 + dk.acc) / 127));
    }
}
/* TONE: a shine on a head */
static void dk_shine(int32_t cx, int32_t cy, int32_t rx, int32_t ry)
{
    if (dk.tone >= 12)
        dk_arc(cx, cy, rx * 62 / 100, ry * 62 / 100, 0x8A3D70A4u, 0x3C28F5C2u, 0, dk_mix(ed_acc(2u, dk.c), 30 + dk.tone * 60 / 127));
}
/* SNAP: sparks off a ringing piece */
static void dk_sparks(int32_t cx, int32_t cy, int32_t rx, int32_t ry, uint32_t l)
{
    int32_t e = dk.e[l], n = dk.snap * 16 * (e >> 7) / (127 * 256), i;
    uint32_t fr = dk.now / 1838u;                    /* (24 a second) */
    for (i = 0; i < n; i++) {
        uint32_t a = ed_hash((uint32_t)i * 31u + fr * 7u + l) << 24;
        int32_t d = 282 + (int32_t)ed_hash((uint32_t)i * 17u + fr + l * 5u) / 2;   /* 1.1 .. 1.6 out, Q8 */
        int32_t x = cx + ((rx * d >> 8) * osc_sine(a + 0x40000000u) >> 15), y = cy + ((ry * d >> 8) * osc_sine(a) >> 15);
        cv_rect((x + 8) >> 4, (y + 8) >> 4, 1, 1, dk_mix(ed_acc(0u, dk.c), 60 + 40 * e / 32768));
    }
}
/* a drum in three-quarter view, its head at (cx, cy), its shell h deep; lane l (-1: none, still), lugs on its front,
 * the snare's wires, its hoop lit by lane rim (-1: none) */
static void dk_drum(int32_t cx, int32_t cy, int32_t rx, int32_t ry, int32_t h, int32_t l, int32_t lugs, int wires, int32_t rim)
{
    int32_t e = l >= 0 ? dk.e[l] : 0, i, wob;
    uint16_t sh = ed_acc(1u, dk.c);
    dk_dim(l);
    if (l >= 0)                                      /* ACC: the hit throws it up, whole px */
        cy -= ((dk.f[l] >> 7) * (64 + dk.acc) / 127 * dk.v[l] / 127 * LN(2) / 256 + 8) & ~15;
    dk_shake(&cx, &cy, (uint32_t)cx);
    dk_fill(cx, cy + h, rx, ry);
    cv_rect((cx - rx + 8) >> 4, (cy + 8) >> 4, (2 * rx) >> 4, (h + 8) >> 4, T_SURF);
    dk_fill(cx, cy, rx, ry);
    cv_stroke(cx - rx, cy, cx - rx, cy + h, dk_ink(sh));
    cv_stroke(cx + rx, cy, cx + rx, cy + h, dk_ink(sh));
    dk_arc(cx, cy + h, rx, ry, 0u, DK_HALF, 0, dk_ink(sh));
    for (i = 0; i < lugs; i++) {                     /* round the front, left to right */
        uint32_t a = DK_HALF / (uint32_t)lugs * (uint32_t)i + DK_HALF / (uint32_t)(2 * lugs);
        int32_t x = cx - (rx * osc_sine(a + 0x40000000u) >> 15), y = cy + (ry * osc_sine(a) >> 15);
        cv_stroke(x, y + h / 4, x, y + h * 7 / 10, dk_mix(sh, 70));
    }
    if (wires)                                       /* SNAP: the wires under it, buzzing while it rings */
        for (i = 0; i < 2 + dk.snap * 5 / 127; i++) {
            int32_t y = cy + h + ry + LN(2) + i * LN(3) / 2;
            int32_t b = (e >> 7) * (osc_sine(dk.now * DK_BUZZ + (uint32_t)i * 0x55555555u) >> 7) / 256 * LN(2) / 256 * dk.snap / 127;
            cv_stroke(cx - rx * 6 / 10, y + b, cx + rx * 6 / 10, y - b, dk_mix(ed_acc(0u, dk.c), 55 + 45 * e / 32768));
        }
    if (rim >= 0) {                                  /* (the hoop at its own lane's level) */
        dk_dim(rim);
        dk_arc(cx, cy, rx + LN(1), ry + LN(1), 0u, DK_FULL, 0, dk_mix(sh, 72 + 28 * dk_hot(rim) / 32768));
        dk_dim(l);
    }
    wob = (e >> 7) * (osc_sine(dk.now * DK_HEAD + (uint32_t)l * 0x28F5C28Fu) >> 7) / 256;   /* Q8 */
    dk_arc(cx, cy, rx, ry + ry * 35 / 100 * wob / 256, 0u, DK_FULL, 0, dk_headc(l));
    dk_shine(cx, cy, rx, ry);
    if (l >= 0) dk_ripples(cx, cy, rx, ry, (uint32_t)l);
}
/* the kick's front head: the hoop, its lugs, the head, the beater behind it (KICK: PUNCH a block, ROUND a felt ball) */
static void dk_kick(int32_t cx, int32_t cy, int32_t r)
{
    int32_t e = dk.e[DV_KICK], f = dk.f[DV_KICK], rv, by, i;
    uint16_t sh = ed_acc(1u, dk.c), bc;
    dk_dim(DV_KICK);
    bc = dk_mix(ed_acc(2u, dk.c), 75 + 25 * dk_hot(DV_KICK) / 32768);
    dk_shake(&cx, &cy, 3u);
    dk_fill(cx, cy, r + LN(1), r + LN(1));
    cv_stroke(cx - r * 8 / 10, cy + r / 2, cx - r - LN(4), cy + r + LN(2), dk_mix(sh, 70));   /* the spurs */
    cv_stroke(cx + r * 8 / 10, cy + r / 2, cx + r + LN(4), cy + r + LN(2), dk_mix(sh, 70));
    dk_arc(cx, cy, r, r, 0u, DK_FULL, 0, dk_ink(sh));
    rv = r - LN(3) + (e >> 7) * (osc_sine(dk.now * DK_KHEAD) >> 7) / 256 * LN(3) / 2 / 256;
    dk_arc(cx, cy, rv, rv, 0u, DK_FULL, 0, dk_headc(DV_KICK));
    if (dk.tone >= 12)
        dk_arc(cx, cy, rv * 7 / 10, rv * 7 / 10, 0x86666666u, 0x39999999u, 0, dk_mix(ed_acc(2u, dk.c), 30 + dk.tone * 50 / 127));
    dk_ripples(cx, cy, rv, rv, DV_KICK);
    by = cy + r * 28 / 100 - f * LN(3) / 32768;
    if (!dk.kick) cv_rect(((cx + 8) >> 4) - 2, ((by + 8) >> 4) - 2, 4, 4, bc);
    else dk_arc(cx, by, LN(7) / 2 + f * LN(1) / 32768, LN(7) / 2 + f * LN(1) / 32768, 0u, DK_FULL, 0, bc);
    for (i = 0; i < 8; i++) {
        uint32_t a = (uint32_t)i << 29 | 0x10000000u;
        int32_t x = cx + ((r + LN(3) / 2) * osc_sine(a + 0x40000000u) >> 15), y = cy + ((r + LN(3) / 2) * osc_sine(a) >> 15);
        cv_rect((x + 8) >> 4, (y + 8) >> 4, 1, 1, dk_mix(sh, 90));
    }
}
/* a stand from y down to the floor fy, on three legs */
static void dk_stand(int32_t x, int32_t y, int32_t fy)
{
    cv_stroke(x, y, x, fy, dk_ink(T_MID));
    cv_stroke(x, fy - LN(8), x - LN(7), fy, dk_ink(T_MID));
    cv_stroke(x, fy - LN(8), x + LN(7), fy, dk_ink(T_MID));
}
/* a cymbal tilted tilt (a turn: 2^32), swaying and flapping while lane l rings (-1: still); TONE its grooves */
static void dk_cymbal(int32_t cx, int32_t cy, int32_t r, int32_t l, int32_t fy, int32_t tilt)
{
    int32_t e = l >= 0 ? dk.e[l] : 0, ry, ng, k;
    uint32_t rot;
    uint16_t mc;
    dk_dim(l);
    mc = dk_metal(l);
    if (fy) dk_stand(cx, cy, fy);
    dk_shake(&cx, &cy, (uint32_t)cx + 1u);
    rot = (uint32_t)(tilt + (e >> 8) * (osc_sine(dk.now * DK_SWAY + (uint32_t)l * 0x30000000u) >> 8) / 128 * 1184000);   /* 0.22 rad */
    ry = r / 5 + r / 5 * 6 / 10 * ((e >> 7) * (osc_sine(dk.now * DK_FLAP) >> 7) / 256) / 256;
    dk_fill(cx, cy, r, ry + LN(1));
    dk_rell(cx, cy, r, ry, rot, 0, mc);
    ng = dk.tone * 3 / 127;
    for (k = 1; k <= ng; k++)
        dk_rell(cx, cy, r * k / (ng + 1), ry * k / (ng + 1), rot, 1, dk_mix(ed_acc(2u, dk.c), 50 + 30 * e / 32768));
    dk_rell(cx, cy - LN(1), r * 17 / 100, ry * 45 / 100, rot, 0, mc);   /* the bell */
    if (l >= 0) dk_sparks(cx, cy, r, ry * 5 / 2, (uint32_t)l);
}
/* the hi-hat: HAT OP parts the two, HAT CL chatters them shut */
static void dk_hihat(int32_t cx, int32_t cy, int32_t r, int32_t fy)
{
    int32_t eo = dk.e[DV_HATO], ec = dk.e[DV_HATC], gap = LN(3) / 2 + LN(7) * eo / 32768, ry = r * 17 / 100, w;
    dk_dim(dk.lv[DV_HATC] > dk.lv[DV_HATO] ? DV_HATC : DV_HATO);   /* the stand: the louder's */
    dk_stand(cx, cy - gap - LN(4), fy);
    dk_shake(&cx, &cy, 4u);
    w = (ec >> 7) * (osc_sine(dk.now * DK_HATW) >> 7) / 256 * LN(1) / 4 / 256;
    dk_dim(DV_HATC);                                 /* the bottom one HAT CL's, the top one HAT OP's */
    dk_fill(cx, cy, r, ry + LN(1));
    dk_arc(cx, cy + w, r, ry, 0u, DK_FULL, 0, dk_metal(DV_HATC));
    dk_sparks(cx, cy - gap / 2, r, ry * 3, DV_HATC);
    dk_dim(DV_HATO);
    dk_fill(cx, cy - gap, r, ry + LN(1));
    dk_arc(cx, cy - gap + w, r, ry, 0u, DK_FULL, 0, dk_metal(eo > ec ? DV_HATO : DV_HATC));
    dk_arc(cx, cy - gap - LN(1), r * 17 / 100, ry * 4 / 10, 0u, DK_FULL, 0, dk_metal(DV_HATO));
    dk_sparks(cx, cy - gap, r * 11 / 10, ry * 4, DV_HATO);
}
/* the bell's swing while it rings: 0.4 rad (0.5 for the hand bell) */
static uint32_t dk_swing(int32_t big)
{
    return (uint32_t)((dk.e[DV_BELL] >> 8) * (osc_sine(dk.now * DK_SWING) >> 8) / 128 * (big ? 2691000 : 2152800));
}
/* n points (1/16 px about the origin) turned rot and put at (cx, cy), as strokes; closed: back to the first */
static void dk_poly(const int32_t *pt, int32_t n, int closed, int32_t cx, int32_t cy, uint32_t rot, uint16_t c)
{
    int32_t cr = osc_sine(rot + 0x40000000u), sr = osc_sine(rot), i, px = 0, py = 0, fx = 0, fy = 0;
    for (i = 0; i < n; i++) {
        int32_t x = cx + ((pt[2 * i] * cr - pt[2 * i + 1] * sr) >> 15), y = cy + ((pt[2 * i] * sr + pt[2 * i + 1] * cr) >> 15);
        if (i) cv_stroke(px, py, x, y, c);
        else {
            fx = x;
            fy = y;
        }
        px = x;
        py = y;
    }
    if (closed) cv_stroke(px, py, fx, fy, c);
}
static void dk_cowbell(int32_t cx, int32_t cy, int32_t w)
{
    int32_t h = w * 13 / 10, e = dk.e[DV_BELL];
    int32_t pt[8] = {-w * 35 / 100, 0, w * 35 / 100, 0, w / 2, h, -w / 2, h}, lip[4] = {w / 2, h + LN(1), -w / 2, h + LN(1)};
    uint32_t rot = dk_swing(0);
    dk_dim(DV_BELL);
    dk_shake(&cx, &cy, 7u);
    dk_poly(pt, 4, 1, cx, cy, rot, dk_metal(DV_BELL));
    dk_poly(lip, 2, 0, cx, cy, rot, dk_mix(ed_acc(2u, dk.c), 65 + 35 * e / 32768));
    dk_sparks(cx, cy + h / 2, w * 7 / 10, h * 6 / 10, DV_BELL);
}
/* 66's and PERC's two-sine bell: a hand bell, its clapper swinging */
static void dk_handbell(int32_t cx, int32_t cy, int32_t r)
{
    int32_t pt[2 * 22], i, k = 0, e = dk.e[DV_BELL];
    int32_t cl[2] = {(e >> 7) * (osc_sine(dk.now * DK_SWING * 2u) >> 7) / 256 * LN(4) / 256, r * 17 / 10};
    int32_t hd[4] = {0, -LN(3), 0, 0};
    uint32_t rot = dk_swing(1);
    dk_dim(DV_BELL);
    dk_shake(&cx, &cy, 7u);
    for (i = 0; i <= 10; i++, k += 2) {              /* its left side down, then its right side up */
        pt[k] = -r * (35 + 65 * i * i / 100) / 100;
        pt[k + 1] = r * 16 * i / 100;
    }
    for (i = 10; i >= 0; i--, k += 2) {
        pt[k] = r * (35 + 65 * i * i / 100) / 100;
        pt[k + 1] = r * 16 * i / 100;
    }
    dk_poly(pt, 22, 1, cx, cy, rot, dk_metal(DV_BELL));
    dk_poly(hd, 2, 0, cx, cy, rot, dk_metal(DV_BELL));
    {
        int32_t cr = osc_sine(rot + 0x40000000u), sr = osc_sine(rot);
        int32_t x = cx + ((cl[0] * cr - cl[1] * sr) >> 15), y = cy + ((cl[0] * sr + cl[1] * cr) >> 15);
        dk_arc(x, y, LN(3) / 2, LN(3) / 2, 0u, DK_FULL, 0, dk_mix(ed_acc(2u, dk.c), 60 + 40 * e / 32768));
    }
    dk_sparks(cx, cy + r, r * 12 / 10, r * 11 / 10, DV_BELL);
}
/* a conga (TOM on the kits that swap it): a tall barrel */
static void dk_conga(int32_t cx, int32_t cy, int32_t rx, int32_t ry, int32_t h)
{
    int32_t e = dk.e[DV_TOM], i, w[11], wob;
    uint16_t sh = ed_acc(1u, dk.c);
    dk_dim(DV_TOM);
    cy -= ((dk.f[DV_TOM] >> 7) * (64 + dk.acc) / 127 * dk.v[DV_TOM] / 127 * LN(2) / 256 + 8) & ~15;
    dk_shake(&cx, &cy, 5u);
    for (i = 0; i <= 10; i++)                        /* the half width down it: bellied, narrowing to the foot */
        w[i] = rx + rx * 18 / 100 * osc_sine((uint32_t)i * 0x0B851EB8u) / 32768 - rx * 25 * i / 1000;
    for (i = 0; i < 10; i++)
        cv_rect((cx - w[i] + 8) >> 4, (cy + h * i / 10 + 8) >> 4, (2 * w[i]) >> 4, (h / 10 + 15) >> 4, T_SURF);
    dk_fill(cx, cy, rx, ry);
    for (i = 1; i <= 10; i++) {
        cv_stroke(cx - w[i - 1], cy + h * (i - 1) / 10, cx - w[i], cy + h * i / 10, dk_ink(sh));
        cv_stroke(cx + w[i - 1], cy + h * (i - 1) / 10, cx + w[i], cy + h * i / 10, dk_ink(sh));
    }
    dk_arc(cx, cy + h, w[10], ry * 3 / 4, 0u, DK_HALF, 0, dk_ink(sh));
    dk_arc(cx, cy + h * 12 / 100, w[1], ry * 97 / 100, 0u, DK_HALF, 0, dk_mix(sh, 65));   /* the bands */
    dk_arc(cx, cy + h * 88 / 100, w[9], ry * 78 / 100, 0u, DK_HALF, 0, dk_mix(sh, 65));
    wob = (e >> 7) * (osc_sine(dk.now * DK_HEAD) >> 7) / 256;
    dk_arc(cx, cy, rx, ry + ry * 40 / 100 * wob / 256, 0u, DK_FULL, 0, dk_headc(DV_TOM));
    dk_shine(cx, cy, rx, ry);
    dk_ripples(cx, cy, rx, ry, DV_TOM);
}
/* the claves (RIM on the kits that swap it): two sticks crossing, meeting on the hit */
static void dk_claves(int32_t cx, int32_t cy, int32_t len)
{
    int32_t f = dk.f[DV_RIM], d = LN(5) * (32768 - f) / 32768 + LN(1), s;
    uint16_t wood;
    dk_dim(DV_RIM);
    wood = dk_mix(ed_acc(1u, dk.c), 80 + 20 * dk.e[DV_RIM] / 32768);
    dk_shake(&cx, &cy, 6u);
    for (s = -1; s <= 1; s += 2) {
        uint32_t a = (uint32_t)(s * (307600000 + 5215 * (32768 - f)));   /* 0.45 .. 0.7 rad */
        int32_t dx = (len / 2) * osc_sine(a + 0x40000000u) >> 15, dy = (len / 2) * osc_sine(a) >> 15;
        int32_t nx = -(LN(3) / 2) * osc_sine(a) >> 15, ny = (LN(3) / 2) * osc_sine(a + 0x40000000u) >> 15;
        int32_t pt[8] = {-dx + nx, -dy + ny, dx + nx, dy + ny, dx - nx, dy - ny, -dx - nx, -dy - ny};
        dk_poly(pt, 4, 1, cx + s * d, cy, 0u, wood);
    }
    dk_sparks(cx, cy, len * 3 / 10, len * 3 / 10, DV_RIM);
}
/* CLAP: two palms meeting on the hit; SNAP throws lines off them */
static void dk_hands(int32_t cx, int32_t cy)
{
    int32_t f = dk.f[DV_CLAP], e = dk.e[DV_CLAP], hit = f > e * 8 / 10 ? f : e * 8 / 10, s, i, n;
    int32_t gap = LN(2) + LN(6) * (32768 - hit) / 32768;
    uint16_t hc;
    dk_dim(DV_CLAP);
    hc = dk_mix(T_TEXT, 65 + 35 * dk_hot(DV_CLAP) / 32768);
    dk_shake(&cx, &cy, 2u);
    for (s = -1; s <= 1; s += 2) {
        int32_t pt[10] = {0, -LN(6), s * LN(5), -LN(8), s * LN(7), LN(4), s * LN(2), LN(7), 0, LN(4)};
        dk_poly(pt, 5, 1, cx + s * gap, cy, 0u, hc);
    }
    if (e < 1638) return;                            /* (5 %) */
    n = 3 + dk.snap * 5 / 127;
    for (i = 0; i < n; i++) {
        uint32_t a = 0xC0000000u + (uint32_t)((2 * i - (n - 1)) * 136700800);   /* up, 0.4 rad apart */
        int32_t r0 = LN(10), r1 = r0 + LN(6) * e / 32768, ca = osc_sine(a + 0x40000000u), sa = osc_sine(a);
        cv_stroke(cx + (r0 * ca >> 15), cy + (r0 * sa >> 15), cx + (r1 * ca >> 15), cy + (r1 * sa >> 15),
                  dk_mix(ed_acc(0u, dk.c), 100 * e / 32768));
    }
}
static void graph_drum(const track_t *t, uint16_t c)
{
    const int16_t *p = t->p;
    uint32_t part = (uint32_t)(t - trk) % NPART, sw = drum_swaps(t), kit = drum_kit_plays(p[P_E0]), l;
    int32_t s = 287 - 61 * clamp(p[P_E1], 0, 127) / 127;   /* TUNE: the pieces' size, Q8: 1.12 (low) .. 0.88 */
    int32_t fy = LN(130);
#define DK_S(v) ((v) * s >> 4)                       /* px at the kit's size, in 1/16 px */
    dk.now = fx.dly_w;
    dk.c = c;
    dk.tone = clamp(p[P_E2], 0, 127);
    dk.decy = clamp(p[P_E3], 0, 127);
    dk.snap = clamp(p[P_E4], 0, 127);
    dk.acc = clamp(p[P_E5], 0, 127);
    dk.kick = p[P_E6] == 1;
    for (l = 0; l < DV_NLANE; l++)                   /* each piece at its lane's level: 15 % at 0, full at 100 % */
        dk.lv[l] = 15 + 85 * clamp(p[P_LN0 + l], 0, 127) / 127;
    for (l = 0; l < DV_NLANE; l++)                   /* the hits since the last frame; another part: none */
        if (part != dk.part) {
            dk.n[l] = drum_hit_n[part][l];
            dk.w[l] = dk.now - 0x40000000u;
        } else if (drum_hit_n[part][l] != dk.n[l]) {
            dk.n[l] = drum_hit_n[part][l];
            dk.v[l] = drum_hit_vel[part][l];
            dk.w[l] = dk.now;
        }
    dk.part = (uint8_t)part;
    dk.all = 0;
    for (l = 0; l < DV_NLANE; l++) {
        dk.e[l] = dk_lvl(l, DK_TAU[l], dk.decy);
        dk.f[l] = dk_lvl(l, 50u, -1);
        if (dk.all < dk.e[l]) dk.all = dk.e[l];
    }
    if (dk.now - dk.w[DV_HATC] < dk.now - dk.w[DV_HATO]) {   /* the closed hat struck since: the open one choked */
        dk.e[DV_HATO] /= 20;
        dk.f[DV_HATO] /= 20;
    }
    dk.jit = clamp(p[P_E7], 0, 127) * (dk.all >> 7) / 127 * LN(5) / 2 / 256;
    dk_dim(-1);
    cv_stroke(LN(4), fy, LN(235), fy, dk_mix(T_TEXT, 30));   /* the floor */
    if (!(sw & 4u)) dk_cymbal(LN(210), LN(40), DK_S(24), -1, fy, 68355000);   /* the crash, still (no lane) */
    dk_hihat(LN(30), LN(66), DK_S(17), fy);
    dk_drum(LN(103), LN(54), DK_S(12), DK_S(9) / 2, DK_S(9), sw & 1u ? -1 : DV_TOM, 3, 0, -1);
    if (sw & 1u) dk_conga(LN(174), LN(58), DK_S(12), DK_S(9) / 2, DK_S(66));
    else dk_drum(LN(140), LN(54), DK_S(13), DK_S(5), DK_S(10), DV_TOM, 3, 0, -1);
    dk_kick(LN(121), LN(96), DK_S(29));
    if (!(sw & 1u)) {                                /* the floor tom on its legs */
        int32_t x = LN(174), rx = DK_S(17), y = LN(90) + DK_S(14);
        dk_dim(DV_TOM);
        cv_stroke(x - rx + LN(2), y, x - rx - LN(4), fy, dk_ink(T_MID));
        cv_stroke(x + rx - LN(2), y, x + rx + LN(4), fy, dk_ink(T_MID));
        dk_drum(x, LN(90), rx, DK_S(6), DK_S(28), DV_TOM, rx / LN(5), 0, -1);
    }
    if (sw & 4u) dk_cymbal(LN(210), LN(40), DK_S(22), DV_BELL, fy, -102532000);
    else if (kit == DK_66 || kit == DK_PERC) dk_handbell(LN(121), LN(52), DK_S(5));
    else dk_cowbell(LN(121), LN(52), DK_S(7));
    if (sw & 2u) dk_claves(LN(47), LN(112), LN(22));
    dk_dim(DV_SNARE);
    dk_stand(LN(64), LN(84) + DK_S(8), fy);          /* the snare on its stand */
    dk_drum(LN(64), LN(84), DK_S(16), DK_S(11) / 2, DK_S(8), DV_SNARE, DK_S(16) / LN(5), 1, sw & 2u ? DV_SNARE : DV_RIM);
    dk_hands(LN(34), LN(22));
    dk_dim(-1);
#undef DK_S
}

/* MRC: PHYS's EDIT 1 / EDIT 2: one drawing a MODEL in place of the scope, from the knobs and the loudest of the part's
 * first three voices (the ones with a model state: eng_phys.c phys_slot_of), on the audio clock (fx.dly_w): its
 * envelope, a new note (voice.c age), and the model's state read as it runs (each mode's resonator, the string's delay
 * line, the sympathetic strings' lines, MEMB's bend), slowed so the eye can follow. The MRC accents by knob as the
 * drawings above. MODAL: GLASSES, a glass harp, a glass a mode (the first six) as full as its pitch is low (STRC), the
 * water shivering with its mode (DAMP's accent) and throwing drops while it rings loud, each glass tinged at POS's
 * height as hard as the strike reaches its mode (ACC), BRIT a glint, BOW a wet finger round the loudest
 * rim. STRNG: PLUCK, the string nut to bridge drawn from one period of its delay line, STRC its winding (dispersion) or
 * the curved bridge it buzzes on, BRIT glints where the wave is sharp, DAMP a felt lifting off the nut, POS the pick
 * (ACC its pull, EXC its flash), BOW the bow's hair. MEMB: TRAMPOLINE, the head a trampoline with someone (accent 3)
 * jumping on it at POS, thrown by a note (ACC: how high; from 70 % a somersault) and bouncing while the head rings,
 * the mat dipping under them (deeper with BEND) and moving with the head's modes, ripples a mode round their feet
 * (BRIT), DAMP's springs round the frame. SYMP: ORBITS, the played string against each sympathetic one as a Lissajous
 * figure at CHRD's ratio, turning with its distance from a small fraction and the strings' detune, sized by the
 * strings' swing, BUZZ creasing the sides, DAMP a trail, SYMP a dot running each. Prototype: dev/phys_visuals.html */
#define PV_SEC 44100u                                /* (samples a second: fx.dly_w's clock) */
#define PV_DROPS 16
#define PV_TRAIL 4
#define PV_MAGIC 0x50485953u
static const int16_t PV_J[PX_NMEMB][17] = {          /* J_m(j_mn r), r 0 .. 1 in 16ths, Q15: the head's mode shapes */
    {32767, 32582, 32031, 31123, 29872, 28301, 26436, 24308, 21952, 19407, 16716, 13923, 11072, 8209, 5381, 2631, 0},
    {0, 3895, 7624, 11027, 13962, 16306, 17966, 18883, 19029, 18413, 17081, 15109, 12603, 9691, 6517, 3235, 0},
    {0, 418, 1631, 3513, 5871, 8460, 11008, 13241, 14911, 15818, 15830, 14898, 13058, 10430, 7206, 3636, 0},
    {32767, 31799, 28981, 24562, 18928, 12566, 6019, -173, -5518, -9620, -12214, -13188, -12590, -10615, -7581, -3894, 0},
    {0, 43, 333, 1068, 2357, 4193, 6447, 8883, 11189, 13025, 14078, 14113, 13013, 10809, 7682, 3943, 0},
    {0, 7012, 13030, 17214, 19015, 18252, 15142, 10249, 4394, -1487, -6495, -9914, -11315, -10621, -8101, -4314, 0},
    {0, 4, 66, 316, 921, 2023, 3677, 5807, 8186, 10466, 12227, 13067, 12687, 10970, 8025, 4194, 0},
    {0, 1108, 4130, 8243, 12305, 15153, 15893, 14136, 10094, 4543, -1373, -6414, -9547, -10191, -8356, -4624, 0},
    {32767, 30414, 23860, 14500, 4286, -4729, -10865, -13183, -11674, -7203, -1253, 4470, 8473, 9833, 8387, 4729, 0},
    {0, 0, 13, 92, 353, 958, 2061, 3731, 5890, 8274, 10454, 11918, 12193, 10981, 8275, 4405, 0},
};
static const int8_t PV_SIDE[6] = {1, 0, -1, 0, 1, 0};   /* cos(m pi / 2): a mode of m diameters along the one struck */
static struct {
    uint32_t magic, last, dt, age, hit, rng, tw;     /* the frame's clock (fx.dly_w), its step; the last note-on; trail timer */
    uint32_t ph[PX_NMODE], oph[PX_NSYMP];            /* each mode's slowed phase, each ORBITS figure's turn (2^32) */
    int32_t pk, lpk, mpk, spk;                       /* running peaks: the modes, the string, its energy, the sympathetic */
    int32_t ma[PX_NMODE], r[PX_NMODE], a0[PX_NMODE];   /* each mode now (Q15 of the peak), its ratio (Q16), the strike's reach (Q15) */
    int32_t nm, lvl, note, md, benv, mainE, syE[PX_NSYMP], sr[PX_NSYMP];   /* sr: the sympathetic strings' ratios, Q16 */
    int32_t jy, jv;                                  /* TRAMPOLINE: height (1/16 px), speed (1/16 px a second) */
    uint32_t spin;                                   /* the somersault, turned so far (2^32) */
    uint32_t tph[PV_TRAIL][PX_NSYMP];                /* ORBITS: the trail, each figure's turn and swing */
    int16_t tex[PV_TRAIL], tey[PV_TRAIL][PX_NSYMP];
    int16_t line[64];                                /* PLUCK: one period of the string, Q15 of the peak */
    int16_t dx[PV_DROPS], dy[PV_DROPS], dvx[PV_DROPS], dvy[PV_DROPS], dfl[PV_DROPS];   /* GLASSES' drops (dfl 0: none) */
    uint8_t part, on, air, flip, ntr;
} pv __attribute__((section(".pool")));
static uint32_t pv_rnd(void)
{
    uint32_t x = pv.rng ? pv.rng : 1u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return pv.rng = x;
}
static int32_t pv_log2(int32_t r)                    /* log2 r (Q16, >= 1), Q8 */
{
    uint32_t x = (uint32_t)r;
    int32_t e = 0;
    if (x < 65536u) return 0;
    while (x >= 131072u) {
        x >>= 1;
        e++;
    }
    return e * 256 + (int32_t)((x - 65536u) >> 8);
}
static int32_t pv_peak(int32_t pk, int32_t v, uint32_t tau)   /* a peak decaying over tau seconds, never below v */
{
    pk -= (int32_t)((int64_t)pk * pv.dt / (PV_SEC * tau));
    return pk > v ? pk : v > 0 ? v : 1;
}
static int32_t pv_rel(int64_t v, int32_t pk)         /* sqrt(v / pk), Q15 */
{
    uint32_t q = (uint32_t)px_isqrt64((uint64_t)(v << 30) / (uint64_t)(pk > 0 ? pk : 1));
    return q > 32767u ? 32767 : (int32_t)q;
}
/* the modes MODAL or MEMB rings at the knobs and the note f0 (cycles Q32): each one's ratio to the lowest (Q16) and how
 * much of the strike reaches it (Q15), as px_modal_block and px_memb_block place them; the count below 0.499 */
static int32_t pv_modes(const int16_t *p, uint32_t md, uint32_t f0, int32_t *r, int32_t *a)
{
    int32_t i, n = 0;
    if (md == PM_MEMB) {
        int32_t harm = clamp(p[P_E1], 0, 127) * 516, pos = clamp(p[P_E4], 0, 127) * 516, w[6], pm = pos + (pos >> 1);
        w[0] = 65536 - px_m(pos, 39322, 16);
        w[1] = pm > 65536 ? 65536 : pm;
        for (i = 2; i < 6; i++)
            w[i] = px_m(w[i - 1], w[1], 16);
        for (i = 0; i < PX_NMEMB; i++) {
            uint32_t rr = PX_MEMB_IDEAL[i] + (uint32_t)px_m((int32_t)(PX_MEMB_LOADED[i] - PX_MEMB_IDEAL[i]), harm, 16);
            if (((uint64_t)f0 * rr >> 16) >= 2143260078u) break;
            r[n] = (int32_t)rr;
            a[n++] = w[PX_MEMB_M[i]] >> 1;
        }
    } else {
        int32_t stiff = px_stiff(clamp(p[P_E1], 0, 127) * 516), st2 = stiff, stretch = 65536;
        uint32_t comp = (uint32_t)(65536 + stiff + px_m(stiff, stiff < 0 ? 60948 : 64225, 16)), phs = 0;
        for (i = 0; i < PX_NMODE; i++) {
            uint64_t rr = (uint64_t)(i + 1) * (uint32_t)stretch;   /* over the compensated fundamental, Q16 */
            if ((uint64_t)f0 * rr / comp >= 2143260078u) break;
            r[n] = (int32_t)rr;
            a[n] = osc_sine(phs + 0x40000000u);
            if (a[n] < 0) a[n] = -a[n];
            n++;
            stretch += st2;
            st2 = px_m(st2, st2 < 0 ? 60948 : 64225, 16);
            phs += (uint32_t)(clamp(p[P_E4], 0, 127) * 129) << 16;
        }
    }
    return n ? n : 1;
}
/* the frame: the clock, the loudest voice, a new note, and what the model holds now */
static void pv_frame(const track_t *t)
{
    const int16_t *p = t->p;
    uint32_t i, k, age = 0, md = (uint32_t)p[P_E0] % PM_COUNT, part = (uint32_t)(t - trk) % NPART, f0, n16;
    int32_t lvl = 0, fresh = 0, lv1;
    voice_t *lv = 0;
    phys_slot_t *S = 0;
    if (pv.magic != PV_MAGIC || pv.part != part || pv.md != (int32_t)md) {   /* first, another part or model: from rest */
        uint8_t *b = (uint8_t *)&pv;
        for (i = 0; i < sizeof pv; i++)
            b[i] = 0;
        pv.magic = PV_MAGIC;
        pv.part = (uint8_t)part;
        pv.md = (int32_t)md;
        pv.last = fx.dly_w;
        pv.hit = fx.dly_w - 0x40000000u;
        pv.note = 60;
        pv.pk = pv.lpk = pv.mpk = pv.spk = 1;
        fresh = 1;
    }
    pv.dt = fx.dly_w - pv.last;
    if (pv.dt > PV_SEC / 20u) pv.dt = PV_SEC / 20u;   /* (back on the page: one frame's worth) */
    pv.last = fx.dly_w;
    for (i = 0; i < NVOICE; i++) {
        voice_t *v = (voice_t *)&t->v[i];
        if (!v->active) continue;
        if (i < PHYS_POLY && v->env >> 9 > lvl) {
            lvl = v->env >> 9;
            lv = v;
        }
        if (v->age > age) age = v->age;
    }
    pv.on = !fresh && age > pv.age;                  /* (voice.c ages each voice: a newer one, a new note) */
    if (age > pv.age || age + 0x10000u < pv.age) pv.age = age;
    if (pv.on) {
        pv.hit = fx.dly_w;
        pv.pk = pv.lpk = pv.mpk = pv.spk = 1;
    }
    pv.lvl = lvl = clamp(lvl, 0, 32767);
    lv1 = lvl * 3 / 2 > 32767 ? 32767 : lvl * 3 / 2;
    if (lv) {
        pv.note = lv->note;
        S = phys_slot_of((track_t *)t, lv);
        if (S && S->model != md) S = 0;
    }
    n16 = lv ? (uint32_t)clamp(lv->pitch_cur, 0, 2047) : (uint32_t)pv.note * 16u;
    f0 = pitch_inc(n16);
    pv.benv = S && md == PM_MEMB ? S->u.m.benv : 0;
    /* the modes: the knobs place them; the resonators' states, each weighted by its gain, how much each rings */
    pv.nm = 0;
    if (md == PM_MODAL || md == PM_MEMB) {
        int32_t mag[PX_NMODE], mx = 0;
        pv.nm = pv_modes(p, md, f0, pv.r, pv.a0);
        for (i = 0; i < (uint32_t)pv.nm; i++) {
            int32_t s1 = S ? S->u.m.s[i][0] : 0, s2 = S ? S->u.m.s[i][1] : 0, w = pv.a0[i];
            int32_t m = (int32_t)px_isqrt64((uint64_t)((int64_t)s1 * s1) + (uint64_t)((int64_t)s2 * s2));   /* Q22 */
            if (md == PM_MEMB) w = (int32_t)(((int64_t)w << 32) / ((int64_t)pv.r[i] * pv.r[i]));   /* (the gain's 1 / ratio^2) */
            mag[i] = (int32_t)((int64_t)m * w >> 15);
            if (mag[i] > mx) mx = mag[i];
        }
        pv.pk = pv_peak(pv.pk, mx, 5u);
        for (i = 0; i < PX_NMODE; i++)
            pv.ma[i] = i < (uint32_t)pv.nm && S ? (int32_t)((int64_t)mag[i] * 32767 / pv.pk) * lv1 >> 15 : 0;
    }
    /* the string: one period behind the write head (the line runs resampled below its lowest pitch), its energy */
    if (S && (md == PM_STRING || md == PM_SYMP)) {
        const px_string_t *st = md == PM_SYMP ? &S->u.y.s : &S->u.s;
        uint32_t d = px_period(f0 < 65536u ? 65536u : f0), wp = st->wp;
        int32_t raw[64], mx = 0;
        int64_t e = 0;
        if (d > (uint32_t)(PX_LINE - 4) << 16) d = (uint32_t)(PX_LINE - 4) << 16;
        for (k = 0; k < 64u; k++) {
            int32_t v = st->line[(wp + 1u + (uint32_t)((uint64_t)d * k / 64u >> 16)) & PX_LMASK], a = v < 0 ? -v : v;
            raw[k] = v;
            if (a > mx) mx = a;
            e += (int64_t)(v >> 8) * (v >> 8);
        }
        pv.lpk = pv_peak(pv.lpk, mx, 2u);
        for (k = 0; k < 64u; k++)
            pv.line[k] = (int16_t)((int32_t)((int64_t)raw[k] * 32767 / pv.lpk) * lv1 >> 15);
        e /= 64;
        pv.mpk = pv_peak(pv.mpk, (int32_t)(e > 0x7FFFFFFF ? 0x7FFFFFFF : e), 3u);
        pv.mainE = pv_rel(e, pv.mpk) * lvl >> 15;
        if (md == PM_SYMP) {
            int64_t es[PX_NSYMP];
            int32_t smx = 0, l2 = lvl * 2 > 32767 ? 32767 : lvl * 2;
            for (k = 0; k < PX_NSYMP; k++) {
                int32_t j;
                es[k] = 0;
                for (j = 0; j < PX_YLINE; j++)
                    es[k] += (int32_t)S->u.y.line[k][j] * S->u.y.line[k][j];
                es[k] /= PX_YLINE;
                if (es[k] > smx) smx = (int32_t)(es[k] > 0x7FFFFFFF ? 0x7FFFFFFF : es[k]);
            }
            pv.spk = pv_peak(pv.spk, smx, 4u);
            for (k = 0; k < PX_NSYMP; k++)
                pv.syE[k] = pv_rel(es[k], pv.spk) * l2 >> 15;
        }
    } else {                                         /* silent: it settles */
        int32_t f = 32767 - (int32_t)(pv.dt * 6u * 32767u / PV_SEC);
        for (k = 0; k < 64u; k++)
            pv.line[k] = (int16_t)(pv.line[k] * f >> 15);
        pv.mainE = pv.mainE * f >> 15;
        for (k = 0; k < PX_NSYMP; k++)
            pv.syE[k] = pv.syE[k] * f >> 15;
    }
    /* SYMP: the sympathetic strings' ratios to the note, as phys_symp_pitches tunes them */
    if (md == PM_SYMP) {
        uint32_t ch = (uint32_t)clamp(p[P_E1], 0, 127) >> 4;
        for (k = 0; k < PX_NSYMP; k++) {
            int32_t p16 = PHYS_DETUNE[k] + (ch < 7u ? (int32_t)n16 + ((PHYS_CHORDS[ch][k] * 41) >> 8)
                                                    : (48 + p[P_ROOT] + (k == 1u ? 7 : k == 2u ? 12 : 0)) * 16);
            pv.sr[k] = (int32_t)(((uint64_t)pitch_inc((uint32_t)clamp(p16, 0, 2047)) << 16) / f0);
        }
    }
    /* the slowed clock of the shapes: 1.2 Hz a partial's ratio at C4, faster up the keys; MEMB's bend speeds it */
    {
        int32_t k8 = clamp(256 + (pv.note - 60) * 256 / 52, 128, 640);
        uint64_t base = 116868u * (uint32_t)k8 >> 8;
        if (md == PM_MEMB) base = base * (uint32_t)(65536 + px_m(pv.benv, clamp(p[P_E6], 0, 127) * 516, 16)) >> 16;
        for (i = 0; i < (uint32_t)pv.nm; i++)
            pv.ph[i] += (uint32_t)(base * (uint32_t)pv.r[i] >> 16) * pv.dt;
    }
}

/* MODAL: GLASSES */
static void pv_glasses(const int16_t *p, uint16_t c)
{
    int32_t n = pv.nm < 6 ? pv.nm : 6, i, k, sp = 36, base = 130, bh = 46, bw = 13, top = 130 - 20 - 46, x0, rmax, loud = 0;
    int32_t posp = clamp(p[P_E4], 0, 127), br = clamp(p[P_E2], 0, 127), acc = clamp(p[P_E5], 0, 127);
    uint32_t since = fx.dly_w - pv.hit;
    int32_t ting = since < PV_SEC * 3u / 10u ? 32767 - (int32_t)(since * 32767u / (PV_SEC * 3u / 10u)) : 0;
    int32_t ty = LN(top) + LN(bh) * posp * 8 / 1270, tv = (ty - LN(top)) * 32767 / LN(bh);
    int32_t tw = LN(bw) * dk_sqrt(1073676289 - tv * tv) / 32767;   /* where POS tings them: the rim .. half way down */
    x0 = 120 - sp * (n - 1) / 2;
    rmax = pv_log2(pv.r[n - 1]);
    if (rmax < 256) rmax = 256;
    cv_stroke(LN(6), LN(base) + 8, LN(234), LN(base) + 8, T_MID);   /* the table */
    for (i = 1; i < n; i++)
        if (pv.ma[i] > pv.ma[loud]) loud = i;
    for (i = 0; i < n; i++) {
        int32_t cx = x0 + i * sp, a = pv.ma[i], px = 0, py = 0, st;
        int32_t fill = clamp(220 - pv_log2(pv.r[i]) * 184 / rmax, 31, 220);   /* STRC: the lowest the fullest, Q8 */
        int32_t wy = LN(top) + LN(bh) * (256 - fill) / 256;
        uint16_t water = dk_mix(ed_acc(1u, c), 55 + 25 * a / 32767), surf = ed_acc(3u, c);
#define PV_RIP(x) (wy + (int32_t)((int64_t)osc_sine((uint32_t)(x) * 34125000u - 3u * pv.ph[i]) * a >> 15) * (LN(5) / 2) / 32767)
        for (k = -bw + 1; k <= bw - 1; k++) {        /* the water, a column a pixel down to the bowl */
            int32_t u = k * 32767 / bw, yb = LN(top) + LN(bh) * dk_sqrt(1073676289 - u * u) / 32767, y = PV_RIP(LN(k));
            if ((yb >> 4) > (y >> 4) + 1) cv_rect(cx + k, (y >> 4) + 1, 1, (yb >> 4) - (y >> 4) - 1, water);
        }
        {                                            /* its surface, from one side of the bowl to the other at that level */
            int32_t v = (wy - LN(top)) * 32767 / LN(bh), hw = LN(bw) * dk_sqrt(1073676289 - v * v) / 32767 - 12;
            for (k = 0; k <= 12; k++) {
                int32_t x = -hw + 2 * hw * k / 12, y = PV_RIP(x);
                if (k) cv_stroke(px, py, LN(cx) + x, y, surf);
                px = LN(cx) + x;
                py = y;
            }
        }
#undef PV_RIP
        gn_arc(LN(cx), LN(top), LN(bw), LN(bh), 0u, DK_HALF / 24u, 24, 0, T_TEXT);   /* the bowl */
        dk_arc(LN(cx), LN(top), LN(bw) + (int32_t)((int64_t)osc_sine(4u * pv.ph[i]) * a >> 15) * LN(1) / 32767,
               LN(5) / 2, 0u, DK_FULL, 0, T_TEXT);   /* the rim, trembling */
        cv_stroke(LN(cx), LN(top + bh), LN(cx), LN(base - 2), T_TEXT);
        dk_arc(LN(cx), LN(base) - 24, LN(9), LN(2), 0u, DK_FULL, 0, T_TEXT);
        cv_stroke(LN(cx - bw + 3), LN(top + 7), LN(cx - bw) + 72, LN(top + 18),
                  dk_mix(ed_acc(2u, c), 50 + (20 + 30 * a / 32767) * br / 127));   /* BRIT: the glint */
        cv_stroke(LN(cx) + tw - 16, ty, LN(cx) + tw + 32, ty, ed_acc(0u, c));   /* POS */
        st = (int32_t)((int64_t)pv.a0[i] * ting >> 15) * (40 + 60 * acc / 127) / 100;
        if (st > 655) {                              /* the ting, as hard as the strike reaches this mode */
            int32_t ox = LN(cx) + tw + LN(3);
            for (k = 0; k < 4; k++) {
                uint32_t an = (uint32_t)(-410000000 + k * 273000000);   /* -0.6 .. 0.6 rad */
                int32_t cs = osc_sine(an + 0x40000000u), sn = osc_sine(an), l = LN(3) + LN(6) * st / 32767;
                cv_stroke(ox + (cs * LN(2) >> 15), ty + (sn * LN(2) >> 15), ox + (cs * l >> 15), ty + (sn * l >> 15),
                          dk_mix(ed_acc(0u, c), 40 + 60 * st / 32767));
            }
        }
        if (a > 16384 && (pv_rnd() & 0xFFFFu) < (uint32_t)((int64_t)a * pv.dt * 8 * 2 / PV_SEC))   /* a drop off a loud one */
            for (k = 0; k < PV_DROPS; k++)
                if (!pv.dfl[k]) {
                    pv.dx[k] = (int16_t)(LN(cx) + (int32_t)(pv_rnd() % (uint32_t)LN(bw)) - LN(bw) / 2);
                    pv.dy[k] = pv.dfl[k] = (int16_t)wy;
                    pv.dvx[k] = (int16_t)((int32_t)(pv_rnd() % 321u) - 160);
                    pv.dvy[k] = (int16_t)(-LN(30) - LN(30) * a / 32767);
                    break;
                }
        if (p[P_E6] && pv.lvl > 327 && i == loud) {   /* BOW: a wet finger round the rim */
            uint32_t an = fx.dly_w * 62032u;
            int32_t fx0 = LN(cx) + (LN(bw) * osc_sine(an + 0x40000000u) >> 15), fy = LN(top) + ((LN(5) / 2) * osc_sine(an) >> 15);
            cv_stroke(fx0, fy - 32, fx0 + LN(8), fy - LN(22), T_MID);
            cv_stroke(fx0 + 16, fy - 32, fx0 + LN(9), fy - LN(22), T_MID);
            cv_dot(fx0, fy - 16, LN(5) / 2, dk_mix(ed_acc(2u, c), pv.lvl * 200 / 32767));
        }
    }
    for (k = 0; k < PV_DROPS; k++)                   /* the drops, falling back */
        if (pv.dfl[k]) {
            pv.dvy[k] = (int16_t)(pv.dvy[k] + (int32_t)(LN(160) * pv.dt / PV_SEC));
            pv.dx[k] = (int16_t)(pv.dx[k] + pv.dvx[k] * (int32_t)pv.dt / (int32_t)PV_SEC);
            pv.dy[k] = (int16_t)(pv.dy[k] + pv.dvy[k] * (int32_t)pv.dt / (int32_t)PV_SEC);
            if (pv.dvy[k] > 0 && pv.dy[k] > pv.dfl[k]) pv.dfl[k] = 0;
            else cv_dot(pv.dx[k], pv.dy[k], 14, ed_acc(3u, c));
        }
}

/* STRNG: PLUCK */
static void pv_pluck(const int16_t *p, uint16_t c)
{
    int32_t x0 = LN(22), x1 = LN(222), y0 = LN(70), L = x1 - x0, k, px = 0, py = 0, x;
    int32_t s = clamp(p[P_E1], 0, 127) * 516, nl, pos = clamp(p[P_E4], 0, 127), br = clamp(p[P_E2], 0, 127);
    int32_t dm = clamp(p[P_E3], 0, 127), acc = clamp(p[P_E5], 0, 127), exc = clamp(p[P_E7], 0, 127);
    uint32_t since = fx.dly_w - pv.hit;
    int32_t pk = since < PV_SEC / 5u ? 32767 - (int32_t)(since * 32767u / (PV_SEC / 5u)) : 0, o, pkx, pky, lift;
#define PV_W(k) ((k) < 32 ? ((k) * 5 >= 32 ? 32767 : (k) * 5 * 1024) : ((64 - (k)) * 5 >= 32 ? 32767 : (64 - (k)) * 5 * 1024))   /* the ends held */
#define PV_Y(k) (y0 + (int32_t)((int64_t)pv.line[(k) & 63] * PV_W(k) >> 15) * LN(34) / 32767)
    nl = s < 15729 ? (int32_t)(((int64_t)(s - 15729) * 273024) >> 16) : s > 17039 ? (int32_t)(((int64_t)(s - 17039) * 88562) >> 16) : 0;
    nl = clamp(nl, -65536, 65536);                   /* (px_string_block: < 0 the curved bridge, > 0 dispersion) */
    cv_stroke(x0, y0 - LN(14), x0, y0 + LN(14), T_MID);   /* the nut */
    cv_stroke(x0 + 8, y0 - LN(14), x0 + 8, y0 + LN(14), T_MID);
    if (nl < 0) {                                    /* STRC: the curved bridge the string lies on */
        int32_t ax = x1 - LN(10) - (int32_t)((int64_t)-nl * LN(14) >> 16), ay = y0 + LN(2);
        int32_t bx = x1 - LN(2), by = y0 + LN(2) - (int32_t)((int64_t)-nl * 8 >> 16), cx = x1 + LN(2), cy = y0 + LN(14);
        for (k = 0; k <= 10; k++) {
            int32_t u = k * 32767 / 10, v = 32767 - u;
            int32_t qx = (int32_t)((((int64_t)v * v >> 15) * ax + ((int64_t)2 * u * v >> 15) * bx + ((int64_t)u * u >> 15) * cx) >> 15);
            int32_t qy = (int32_t)((((int64_t)v * v >> 15) * ay + ((int64_t)2 * u * v >> 15) * by + ((int64_t)u * u >> 15) * cy) >> 15);
            if (k) cv_stroke(px, py, qx, qy, ed_acc(1u, c));
            px = qx;
            py = qy;
        }
    } else {
        cv_stroke(x1, y0 - LN(10), x1, y0 + LN(14), T_MID);
        cv_stroke(x1 + 8, y0 - LN(10), x1 + 8, y0 + LN(14), T_MID);
    }
    for (k = 0; k <= 64; k++) {                      /* the string: one period of its line */
        int32_t sx = x0 + L * k / 64, sy = PV_Y(k);
        if (k) cv_stroke(px, py, sx, sy, T_TEXT);
        px = sx;
        py = sy;
    }
    if (nl > 0) {                                    /* STRC: the winding, closer as it stiffens */
        int32_t stp = LN(9) - (int32_t)((int64_t)nl * LN(6) >> 16);
        if (stp < LN(3)) stp = LN(3);
        for (x = x0 + LN(4); x < x1 - LN(2); x += stp) {
            int32_t kk = (x - x0) * 64 / L, y = PV_Y(kk);
            cv_stroke(x - 16, y + 32, x + 16, y - 32, ed_acc(1u, c));
        }
    }
    for (k = 2; k < 62; k++) {                       /* BRIT: glints where the wave is sharp */
        int32_t cu = pv.line[k - 1] - 2 * pv.line[k] + pv.line[k + 1], cb;
        cb = (cu < 0 ? -cu : cu) * br / 127;
        if (cb * 8 > 16384) cv_dot(x0 + L * k / 64, PV_Y(k), 19, dk_mix(ed_acc(2u, c), clamp(cb * 600 / 32767, 40, 100)));
    }
    if (nl < 0 && (pv.line[60] > 4915 || pv.line[60] < -4915))   /* the curved bridge buzzes: sparks */
        for (k = 0; k < 3; k++) {
            int32_t sx = x1 - LN(6) - (int32_t)(pv_rnd() % (uint32_t)LN(10)), sy = y0 - LN(2) + (int32_t)(pv_rnd() % (uint32_t)LN(6));
            uint16_t sc = dk_mix(ed_acc(1u, c), 40 + 60 * -nl / 65536);
            cv_stroke(sx - 24, sy, sx + 24, sy, sc);
            cv_stroke(sx, sy - 24, sx, sy + 24, sc);
        }
    lift = dm * 18 / 127;                            /* DAMP: the felt at the nut, lifted off as it rings on */
    cv_rect(26, 70 - 9 - lift, 7, 8, dk_mix(ed_acc(3u, c), 90));
    cv_rect(26, 72 + lift, 7, 8, dk_mix(ed_acc(3u, c), 90));
    o = pk > 19660 ? (32767 - pk) * 5 / 2 : pk;      /* POS: the pick, pulled by ACC and through at the note */
    pkx = 22 + 200 * clamp(pos * 100 / 254, 3, 50) / 100;
    pky = 62 - o * 14 / 32767 * (40 + 60 * acc / 127) / 100;
    for (k = 0; k < 7; k++) {
        int32_t w = 8 * (7 - k) / 7;
        if (w) cv_rect(pkx - w / 2, pky - 7 + k, w, 1, dk_mix(ed_acc(0u, c), pos ? 100 : 60));
    }
    if (exc && pk > 13107) cv_dot(LN(pkx), y0, LN(3) + LN(7) * exc / 127, dk_mix(ed_acc(3u, c), (pk - 13107) * 100 / 32767 * exc / 127));
    if (p[P_E6] && pv.lvl > 327) {                   /* BOW: the hair across the string */
        int32_t bx = x0 + L * 82 / 100 + (osc_sine(fx.dly_w * 31004u) * LN(6) >> 15);
        uint16_t bc = dk_mix(ed_acc(2u, c), pv.lvl * 200 / 32767);
        cv_stroke(bx - LN(6), y0 - LN(24), bx + LN(6), y0 + LN(24), bc);
        cv_stroke(bx - LN(6) + 12, y0 - LN(24), bx + LN(6) + 12, y0 + LN(24), bc);
    }
#undef PV_Y
#undef PV_W
}

/* MEMB: TRAMPOLINE. The mat's dip at x (1/16 px in and out): the head's modes along the diameter through them, and
 * depth under their feet */
static int32_t pv_z(int32_t x, int32_t jx, int32_t depth)
{
    int32_t u = (x - LN(120)) * 32767 / LN(86), r = u < 0 ? -u : u, z = 0, i, ri, f;
    if (r > 32767) r = 32767;
    ri = r >> 11;
    f = r & 2047;
    for (i = 0; i < pv.nm && i < PX_NMEMB; i++) {
        int32_t sd = PV_SIDE[PX_MEMB_M[i]], j;
        if (!sd || !pv.ma[i]) continue;
        j = PV_J[i][ri] + ((PV_J[i][ri < 16 ? ri + 1 : 16] - PV_J[i][ri]) * f >> 11);
        z += (int32_t)(((int64_t)pv.ma[i] * j >> 15) * sd * osc_sine(pv.ph[i] + 0x40000000u) >> 15);
    }
    z = (int32_t)((int64_t)z * LN(4) >> 15) * (32767 - (r * r >> 15)) >> 15;
    return z + depth * ed_bump((x - jx) * 5 / 24) / 255;
}
static void pv_tramp(const int16_t *p, uint16_t c)
{
    int32_t cx = LN(120), my = LN(104), rx = LN(86), ry = LN(13), pos = clamp(p[P_E4], 0, 127), dm = clamp(p[P_E3], 0, 127);
    int32_t br = clamp(p[P_E2], 0, 127), acc = clamp(p[P_E5], 0, 127), exc = clamp(p[P_E7], 0, 127), i, k, e = 0, px = 0, py = 0;
    int32_t jx = cx + pos * (rx - LN(18)) / 127, sag = px_m(pv.benv, clamp(p[P_E6], 0, 127) * 516, 16);   /* Q16 */
    uint32_t since = fx.dly_w - pv.hit, rot = 0;
    int32_t kick = since < PV_SEC * 12u / 100u ? (32767 - (int32_t)(since * 32767u / (PV_SEC * 12u / 100u))) * (40 + 60 * acc / 127) / 100 : 0;
    int32_t depth, fy, rise, bend, hip, neck, tuck, arm, col_x[11], col_y[11];
    uint16_t ink = ed_acc(1u, c);                    /* the jumper: accent 2 */
    for (i = 0; i < pv.nm; i++)
        if (pv.ma[i] > e) e = pv.ma[i];
    e = e * pv.lvl >> 15;
    /* the jump: a note throws them (ACC; from 70 % a somersault), they bounce back while the head rings (DAMP). At
     * most PV_TOP up (the head out of the panel); silent, they come down three times as fast */
#define PV_G LN(540)                                 /* gravity, 1/16 px a second a second */
#define PV_TOP LN(120)
    if (pv.on) {
        pv.jv = LN(130) + LN(125) * acc / 127;   /* (full ACC: about 60 px up) */
        pv.air = 1;
        if (pv.jy < 1) pv.jy = 1;
        if (acc >= 89 && !pv.flip) {                 /* (one under way carries on) */
            pv.flip = 1;
            pv.spin = 0;
        }
    }
    if (pv.air) {
        int32_t g = PV_G * (pv.lvl < 327 ? 3 : 1);
        pv.jv -= (int32_t)(g * pv.dt / PV_SEC);
        pv.jy += pv.jv * (int32_t)pv.dt / (int32_t)PV_SEC;
        if (pv.jy >= PV_TOP && pv.jv > 0) {          /* the top: stopped there */
            pv.jy = PV_TOP;
            pv.jv = 0;
        }
        if (pv.flip) {                               /* the rest of the turn spread over the time left till they land */
            uint32_t left = DK_FULL - pv.spin;
            int64_t t = (int64_t)(pv.jv + dk_sqrt(pv.jv * pv.jv + 2 * g * (pv.jy > 0 ? pv.jy : 0))) * PV_SEC / g;
            pv.spin += t > (int64_t)pv.dt ? (uint32_t)((uint64_t)left * pv.dt / (uint64_t)t) : left;
        }
        if (pv.jy <= 0) {
            pv.flip = 0;                             /* (the turn done: they land on their feet) */
            int32_t reb = (30 + 50 * dm / 127) * (e * 2 + 6553 > 32767 ? 32767 : e * 2 + 6553) / 32767, nv = -pv.jv * reb / 100;
            pv.jy = 0;
            if (nv > LN(28) && e > 983) {
                pv.jv = nv;
            } else {
                pv.air = 0;
                pv.jv = 0;
            }
        }
    }
    depth = (pv.air ? (pv.jy < LN(8) ? (LN(8) - pv.jy) * LN(4) / LN(8) : 0) : LN(3)) + (int32_t)((int64_t)sag * LN(10) >> 16) + kick * LN(6) / 32767;
    /* the legs and the frame */
    for (k = -1; k <= 1; k += 2) {
        cv_stroke(cx + k * rx * 82 / 100, my + LN(6), cx + k * rx * 86 / 100, LN(138), T_MID);
        cv_stroke(cx + k * rx * 45 / 100, my + LN(12), cx + k * rx * 48 / 100, LN(138), T_MID);
    }
    dk_arc(cx, my, rx, ry, 0u, DK_FULL, 0, T_TEXT);
    dk_arc(cx, my, rx + 8, ry + 8, 0u, DK_FULL, 0, T_TEXT);
    for (k = 0; k < 18; k++) {                       /* DAMP: the springs, tauter as it rings longer */
        uint32_t an = DK_FULL / 18u * (uint32_t)k;
        int32_t cs = osc_sine(an + 0x40000000u), sn = osc_sine(an), zz = LN(3) / 2 - LN(1) * dm / 127, j;
        px = cx + (cs * rx >> 15);
        py = my + (sn * ry >> 15);
        for (j = 1; j <= 4; j++) {
            int32_t sgn = j & 1 ? 1 : -1, qx, qy;
            qx = cx + (cs * (rx - LN(8) * j / 4) >> 15) + sgn * (zz * sn >> 15);
            qy = my + (sn * (ry - LN(3) * j / 4) >> 15) + sgn * zz / 2;
            cv_stroke(px, py, qx, qy, ed_acc(3u, c));
            px = qx;
            py = qy;
        }
    }
    for (k = 0; k <= 40; k++) {                      /* the mat's front edge, dipping */
        uint32_t an = DK_HALF / 40u * (uint32_t)k;
        int32_t x = cx + (osc_sine(an + 0x40000000u) * (rx - LN(7)) >> 15);
        int32_t y = my + (osc_sine(an) * (ry - LN(3)) >> 15) + pv_z(x, jx, depth) * 6 / 10;
        if (k) cv_stroke(px, py, x, y, T_TEXT);
        px = x;
        py = y;
    }
    for (i = 0; i < pv.nm && i < 6; i++) {           /* BRIT: ripples round their feet, a mode each */
        int32_t m = pv.ma[i] * pv.lvl >> 15;
        if (m < 1310) continue;
        dk_arc(jx, my + LN(1), LN(8 + 6 * i) + (osc_sine(pv.ph[i]) * 24 >> 15), LN(3) / 2 + i * LN(9) / 10, 0u, DK_FULL, 0,
               dk_mix(ed_acc(2u, c), 35 + 65 * m / 32767 * (127 + br) / 254));
    }
    /* the jumper, turned about the hip in a somersault */
    fy = my + LN(1) + pv_z(jx, jx, depth) - pv.jy;
    rise = pv.air ? clamp(pv.jv * 32767 / LN(150), -32767, 32767) : -32767;
    bend = pv.air ? 0 : 1;
    if (pv.air && pv.flip) rot = pv.spin;
    hip = LN(-11 + 3 * bend);
    neck = LN(-23 + 3 * bend);
    tuck = pv.air && rise > 0 ? LN(4) * rise / 32767 : 0;
    arm = pv.air ? 32767 - (rise + 32767) / 2 * 3 / 10 : 6553;
    {                                                /* feet, knees, hip, neck, shoulder, hands, head */
        static const int8_t SEG[7][2] = {{0, 2}, {2, 4}, {1, 3}, {3, 4}, {4, 5}, {6, 7}, {6, 8}};
        int32_t ptx[10] = {LN(-2), LN(2), LN(-3) - LN(2) * bend, LN(3) + LN(2) * bend, 0, 0, 0, LN(-7), LN(7), 0};
        int32_t pty[10] = {-tuck * 3 / 2, -tuck * 3 / 2, LN(-5) + LN(1) * bend - tuck, LN(-5) + LN(1) * bend - tuck, hip, neck,
                           neck + LN(2), LN(-20) - LN(10) * arm / 32767, LN(-20) - LN(10) * arm / 32767, neck - LN(4)};
        int32_t cs = osc_sine(rot + 0x40000000u), sn = osc_sine(rot);
        for (k = 0; k < 10; k++) {
            int32_t dy = pty[k] - hip;
            col_x[k] = jx + ((ptx[k] * cs - dy * sn) >> 15);
            col_y[k] = fy + hip + ((ptx[k] * sn + dy * cs) >> 15);
        }
        for (k = 0; k < 7; k++)
            cv_stroke(col_x[SEG[k][0]], col_y[SEG[k][0]], col_x[SEG[k][1]], col_y[SEG[k][1]], ink);
        dk_arc(col_x[9], col_y[9], LN(7) / 2, LN(7) / 2, 0u, DK_FULL, 0, ink);
    }
    if (kick) {                                      /* the launch: ACC's push, EXC's flash under the feet */
        for (k = 0; k < 5; k++) {
            uint32_t an = (uint32_t)(0x8CCCCCCDu + (uint32_t)k * 0x1999999Au);   /* (1.1 + 0.2 k) pi */
            int32_t cs = osc_sine(an + 0x40000000u), sn = osc_sine(an);
            cv_stroke(jx + (cs * LN(5) >> 15), fy + LN(2) + (sn * LN(3) >> 15),
                      jx + (cs * (LN(7) + LN(7) * kick / 32767) >> 15), fy + LN(2) + (sn * (LN(4) + LN(5) * kick / 32767) >> 15),
                      dk_mix(ed_acc(1u, c), 40 + 60 * kick / 32767));
        }
        if (exc) {
            uint16_t fc = dk_mix(ed_acc(3u, c), kick * 60 / 32767 * exc / 127 + 20);
            dk_arc(jx, fy + LN(2), LN(6) + LN(10) * exc / 127, LN(2) + LN(3) * exc / 127, 0u, DK_FULL, 0, fc);
            dk_arc(jx, fy + LN(2), LN(4) + LN(6) * exc / 127, LN(1) + LN(2) * exc / 127, 0u, DK_FULL, 0, fc);
        }
    }
    cv_stroke(jx, my + ry + LN(3), jx, my + ry + LN(7), ed_acc(0u, c));   /* POS: where they land */
#undef PV_TOP
#undef PV_G
}

/* SYMP: ORBITS. Point j (0..160) of figure k: x the played string (two turns), y the sympathetic one at its ratio */
static void pv_pt(int32_t cx, int32_t r, uint32_t ph, int32_t ex, int32_t ey, int32_t buzz, int32_t j, int32_t *x, int32_t *y)
{
    uint64_t u = (uint64_t)j * (((uint64_t)1 << 33) / 160u);
    int32_t sx = osc_sine((uint32_t)u + (uint32_t)((uint64_t)ph * 3u / 10u)), a = sx < 0 ? -sx : sx;
    if (a > 19660) {                                 /* BUZZ: the bridge clips the swing */
        int32_t cut = (int32_t)((int64_t)buzz * (a - 19660) * 7 / (8 * 127));
        sx += sx < 0 ? cut : -cut;
    }
    *x = LN(cx) + (int32_t)((int64_t)sx * ex >> 15) * LN(34) / 32767;
    *y = LN(62) + (int32_t)((int64_t)osc_sine((uint32_t)(u * (uint32_t)r >> 16) + ph) * ey >> 15) * LN(34) / 32767;
}
static void pv_fig(int32_t cx, int32_t r, uint32_t ph, int32_t ex, int32_t ey, int32_t buzz, int32_t step, uint16_t col)
{
    int32_t j, px = 0, py = 0, x, y;
    for (j = 0; j <= 160; j += step) {
        pv_pt(cx, r, ph, ex, ey, buzz, j, &x, &y);
        if (j) cv_stroke(px, py, x, y, col);
        px = x;
        py = y;
    }
}
static void pv_orbits(const int16_t *p, uint16_t c)
{
    static const int32_t CX[PX_NSYMP] = {42, 120, 198};
    int32_t buzz = clamp(p[P_E6], 0, 127), ch = clamp(p[P_E1], 0, 127) >> 4, sy = clamp(p[P_E4], 0, 127);
    int32_t keep = 1 + clamp(p[P_E3], 0, 127) * (PV_TRAIL - 1) / 127, live = pv.lvl > 327, k, t;
    int32_t ex = live ? 8192 + 24575 * pv.mainE / 32767 : 26214;
    char s[6];
    pv.tw += pv.dt;
    if (pv.tw > PV_SEC * 12u / 100u) {               /* DAMP: the trail, a figure every 0.12 s while it sounds */
        pv.tw = 0;
        if (live) {
            for (t = PV_TRAIL - 1; t > 0; t--) {
                pv.tex[t] = pv.tex[t - 1];
                for (k = 0; k < PX_NSYMP; k++) {
                    pv.tph[t][k] = pv.tph[t - 1][k];
                    pv.tey[t][k] = pv.tey[t - 1][k];
                }
            }
            pv.tex[0] = (int16_t)ex;
            if (pv.ntr < PV_TRAIL) pv.ntr++;
        }
    }
    for (k = 0; k < PX_NSYMP; k++) {
        int32_t r = pv.sr[k], best = 65536, q, ey = live ? 6553 + 26214 * pv.syE[k] / 32767 : 26214, cents, w, j, x, y;
        for (q = 1; q <= 4; q++) {                   /* how far the ratio is from a small fraction: how fast it turns */
            int32_t fr = (int32_t)(((int64_t)r * q) & 0xFFFF), d = (fr < 32768 ? fr : 65536 - fr) / q;
            if (d < best) best = d;
        }
        pv.oph[k] += (uint32_t)(1860 + 46500 * best / 65536) * pv.dt * (k == 1 ? (uint32_t)-1 : 1u);
        if (live) {
            pv.tph[0][k] = pv.oph[k];
            pv.tey[0][k] = (int16_t)ey;
        }
        for (t = (keep < pv.ntr ? keep : pv.ntr) - 1; t >= 1; t--)
            pv_fig(CX[k], r, pv.tph[t][k], pv.tex[t], pv.tey[t][k], buzz, 2,
                   dk_mix(ed_acc(3u, c), (keep - t) * 60 / keep * pv.lvl / 32767 + 15));
        pv_fig(CX[k], r, pv.oph[k], ex, ey, buzz, 1, dk_mix(T_TEXT, live ? 100 : 80));
        j = (int32_t)((uint64_t)fx.dly_w * 30u * (uint32_t)(10 + 2 * k) / (10u * PV_SEC) % 160u);   /* SYMP: the dot */
        pv_pt(CX[k], r, pv.oph[k], ex, ey, buzz, j, &x, &y);
        cv_dot(x, y, LN(3) / 2 + LN(2) * sy / 127, dk_mix(ed_acc(0u, c), 50 + 50 * sy / 127));
        cv_stroke(LN(CX[k] - 34), LN(112), LN(CX[k] + 34), LN(112), T_MID);   /* the string's swing */
        x = LN(CX[k] - 34) + LN(68) * pv.syE[k] / 32767;
        cv_stroke(x, LN(110), x, LN(114), ed_acc(2u, c));
        cents = ch < 7 ? PHYS_CHORDS[ch][k] : (k == 1 ? 700 : k == 2 ? 1200 : 0) + (48 + p[P_ROOT] - pv.note) * 100;
        j = (cents < 0 ? -cents + 50 : cents + 50) / 100;   /* CHRD: the interval, semitones */
        s[0] = cents < 0 ? '-' : '+';
        s[1] = (char)(j >= 10 ? '0' + j / 10 % 10 : '0' + j);
        s[2] = (char)(j >= 10 ? '0' + j % 10 : 0);
        s[3] = 0;
        w = text_w(&AF_S, s);
        cv_text(CX[k] - w / 2, 118, &AF_S, s, ed_acc(1u, c));
    }
}
static void graph_phys(const track_t *t, uint16_t c)
{
    pv_frame(t);
    switch (pv.md) {
    case PM_MODAL: pv_glasses(t->p, c); break;
    case PM_STRING: pv_pluck(t->p, c); break;
    case PM_MEMB: pv_tramp(t->p, c); break;
    default: pv_orbits(t->p, c); break;
    }
}
/* an engine with a still drawing on its EDIT pages (graph_signature: no frame term) */
static int ed_still(const track_t *t)
{
    const engine_t *e = ENGINES[t->eng_req % NENGINES];
    return e == &ENG_ANALOG || e == &ENG_LOFI || e == &ENG_WHEEL;
}
/* The FM charts' parts: an operator box 21 x 17 (rows 23 px apart, columns 24), junction dots, Manhattan routes */
#define FM_BH 17
static void fm_dot(int32_t x, int32_t y, uint16_t c) { cv_rect(x - 1, y - 1, 3, 3, c); }
/* a Manhattan route, 1 px: down from (x0, y0) to the jog row jy, across to x1, down to y1 */
static void fm_route(int32_t x0, int32_t y0, int32_t x1, int32_t jy, int32_t y1, uint16_t c)
{
    cv_rect(x0, y0, 1, jy - y0, c);
    cv_rect(x0 < x1 ? x0 : x1, jy, (x0 < x1 ? x1 - x0 : x0 - x1) + 1, 1, c);
    cv_rect(x1, jy, 1, y1 - jy, c);
}
/* a feedback loop on the operator box at (x, y): out of its right side, up, back in on top with an arrow */
static void fm_loop(int32_t x, int32_t y, uint16_t c)
{
    cv_rect(x + 11, y + 5, 5, 1, c);
    cv_rect(x + 15, y - 4, 1, 9, c);
    cv_rect(x, y - 4, 15, 1, c);
    cv_rect(x, y - 4, 1, 4, c);
    cv_line(x - 2, y - 3, x - 1, y - 2, c); cv_line(x + 2, y - 3, x + 1, y - 2, c);
}
/* the output bus under the carriers (the leftmost at cl, the rightmost at cr) at y bus, an arrow out on the right */
static void fm_bus(int32_t cl, int32_t cr, int32_t bus)
{
    cv_rect(cl, bus, cr + 24 - cl, 1, T_MID);
    cv_line(cr + 20, bus - 3, cr + 23, bus, T_MID); cv_line(cr + 20, bus + 3, cr + 23, bus, T_MID);
}

#ifndef FM6_CHART_HOOK
#define FM6_CHART_HOOK(kind, a, b) ((void)0)   /* ui_render.c (host tests): the chart's parts one by one (its lint) */
#endif
enum { FMH_BOX, FMH_ROUTE, FMH_CAR, FMH_BUS, FMH_FB, FMH_LABEL, FMH_END };
/* FM6's 32 algorithms as charts. FM6_CELL: per operator 1..6 its cell, the column in 24 px steps (bits 0..2) and
 * the row above the output bus (bits 4..5; row 0 = the carriers). The routes, the carriers and the feedback
 * operator are fm6_core.c's FM6_ALG itself (fm6_routes, fm6_car_ops, fm6_fb_op). ui_test.c checks the cells
 * against them (a modulator one row above what it modulates, the carriers on the bottom row and only they, one
 * operator per cell) and the routes against the 32 algorithms written out; ui_render.c lints every chart as drawn
 * (no route through a box or touching another route, no two boxes overlapping, all inside the panel). The cells:
 * a search for the fewest and shortest bends, the operators numbered left to right. */
static const uint8_t FM6_CELL[32][6] = {
    {0x00, 0x10, 0x01, 0x11, 0x21, 0x31}, {0x00, 0x10, 0x02, 0x12, 0x22, 0x32}, {0x00, 0x10, 0x20, 0x01, 0x11, 0x21},
    {0x00, 0x10, 0x20, 0x01, 0x11, 0x21}, {0x00, 0x10, 0x01, 0x11, 0x02, 0x12}, {0x00, 0x10, 0x01, 0x11, 0x02, 0x12},
    {0x00, 0x10, 0x01, 0x11, 0x12, 0x22}, {0x00, 0x10, 0x01, 0x12, 0x11, 0x21}, {0x00, 0x10, 0x02, 0x12, 0x13, 0x23},
    {0x00, 0x10, 0x20, 0x01, 0x11, 0x12}, {0x00, 0x10, 0x20, 0x01, 0x11, 0x12}, {0x00, 0x10, 0x03, 0x12, 0x13, 0x14},
    {0x00, 0x10, 0x02, 0x11, 0x12, 0x13}, {0x00, 0x10, 0x01, 0x11, 0x20, 0x21}, {0x00, 0x10, 0x02, 0x12, 0x21, 0x22},
    {0x01, 0x10, 0x11, 0x21, 0x12, 0x22}, {0x01, 0x12, 0x10, 0x20, 0x11, 0x21}, {0x01, 0x10, 0x12, 0x11, 0x21, 0x31},
    {0x00, 0x10, 0x20, 0x01, 0x02, 0x11}, {0x00, 0x01, 0x10, 0x02, 0x12, 0x13}, {0x00, 0x01, 0x10, 0x02, 0x03, 0x12},
    {0x00, 0x10, 0x01, 0x02, 0x03, 0x12}, {0x00, 0x01, 0x11, 0x02, 0x03, 0x12}, {0x00, 0x01, 0x02, 0x03, 0x04, 0x13},
    {0x00, 0x01, 0x02, 0x03, 0x04, 0x13}, {0x00, 0x01, 0x11, 0x02, 0x12, 0x13}, {0x00, 0x01, 0x11, 0x03, 0x13, 0x14},
    {0x00, 0x10, 0x01, 0x11, 0x21, 0x02}, {0x00, 0x01, 0x02, 0x12, 0x03, 0x13}, {0x00, 0x01, 0x02, 0x12, 0x22, 0x03},
    {0x00, 0x01, 0x02, 0x03, 0x04, 0x14}, {0x00, 0x01, 0x02, 0x03, 0x04, 0x05}};
/* algorithm a's routes: m[i] bit j: operator j + 1 modulates operator i + 1. FM6_ALG runs the operators sixth
 * first, each reading a bus (or none) and writing one (or adding to it): who wrote a bus modulates its reader */
static void fm6_routes(uint32_t a, uint8_t *m)
{
    const uint8_t *f = FM6_ALG[a & 31u];
    uint8_t bus[4] = {0, 0, 0, 0};
    uint32_t k;
    for (k = 0; k < 6u; k++) {
        uint32_t op = 5u - k, in = (f[k] >> 4) & 3u, o = f[k] & 3u;
        m[op] = in ? bus[in] : 0u;
        bus[o] = (uint8_t)(((f[k] & FM6_OADD) ? bus[o] : 0u) | 1u << op);
    }
}
/* the operator with feedback in algorithm a (0..5: operator 1..6) */
static uint32_t fm6_fb_op(uint32_t a)
{
    uint32_t k;
    for (k = 0; k < 5u && (FM6_ALG[a & 31u][5u - k] & 0xC0u) != 0xC0u; k++)
        ;
    return k;
}
/* the carriers of algorithm a: bit k operator k + 1 (fm6_carriers counts the sixth first) */
static uint32_t fm6_car_ops(uint32_t a)
{
    uint32_t c = fm6_carriers(a), r = 0, k;
    for (k = 0; k < 6u; k++)
        r |= (c >> (5u - k) & 1u) << k;
    return r;
}
/* the algorithm FM6 plays on track t (0..31): ALG, or the patch's at PAT */
static uint32_t fm6_alg_of(const track_t *t)
{
    return t->p[P_E0] >= 1 && t->p[P_E0] <= 32 ? (uint32_t)t->p[P_E0] - 1u : fm6_patch[(t - trk) % NTRK][FP_ALG] & 31u;
}
/* FM6's EDIT pages: the algorithm, drawn as graph_fm draws DIGITAL's, "ALG 05" top left; SLOT on a bank voice (B1..B32,
 * 1.4.1): its name top right. Carriers THEME with INK
 * numerals, modulators RAISE with THEME ones; an operator at output level 0: a DIM numeral (a carrier on RAISE).
 * Routes THEME, the feedback loop MID (DIM at feedback 0), the output bus MID. The knob just turned in ACCENT:
 * ALG and PTCH the label, FB the loop, MLVL the routes, MRAT MEG VMOD the modulators, DTUN the carriers.
 * sel < 6 (the OPERATOR pages, ui_fm6op.c): that operator's box ACCENT with an INK numeral, its frequency top right */
static void graph_fm6(const track_t *t, uint16_t c, uint32_t sel)
{
    uint32_t alg = fm6_alg_of(t), k, j, car = fm6_car_ops(alg), top = 0, hi = 0, fbop = fm6_fb_op(alg), hotset = 0;
    uint32_t hot = ui.hot_t ? (uint32_t)cur_page()->id[ui.hot_col & 3u] : 0u;
    const uint8_t *pt = fm6_patch[(t - trk) % NTRK];
    int32_t x[6], y[6], bus, cl = 240, cr = 0, fb = clamp(pt[FP_FB] + t->p[P_E1], 0, 7);
    uint8_t m[6], fan[6] = {0, 0, 0, 0, 0, 0};
    uint16_t rc = hot == P_E2 ? T_ACCENT : c;
    char b[7] = {'A', 'L', 'G', ' ', (char)('0' + (alg + 1u) / 10u), (char)('0' + (alg + 1u) % 10u), 0};
    if (hot == P_E3 || hot == P_E4 || hot == P_E5)
        hotset = 63u & ~car;
    else if (hot == P_E6)
        hotset = car;
    fm6_routes(alg, m);
    for (k = 0; k < 6u; k++) {
        uint32_t col = FM6_CELL[alg][k] & 7u, row = FM6_CELL[alg][k] >> 4;
        hi = col > hi ? col : hi;
        top = row > top ? row : top;
        for (j = 0; j < 6u; j++)
            fan[j] += (uint8_t)(m[k] >> j & 1u);
    }
    bus = 50 + (27 + 23 * (int32_t)top) / 2;         /* the chart centred on the 100 px scale */
    for (k = 0; k < 6u; k++) {
        x[k] = 120 + (int32_t)(FM6_CELL[alg][k] & 7u) * 24 - (int32_t)hi * 12;
        y[k] = bus - 6 - FM_BH - 23 * (int32_t)(FM6_CELL[alg][k] >> 4);
        if (car >> k & 1u) { cl = x[k] < cl ? x[k] : cl; cr = x[k] > cr ? x[k] : cr; }
    }
    for (k = 0; k < 6u; k++) {                       /* into k: its sources down to its jog row, across, down */
        for (j = 0; j < 6u; j++)
            if (m[k] >> j & 1u) {
                FM6_CHART_HOOK(FMH_ROUTE, j, k);
                fm_route(x[j], y[j] + FM_BH, x[k], y[k] - 3, y[k], rc);
                if (fan[j] > 1u) fm_dot(x[j], y[k] - 3, rc);          /* one operator modulating several */
                if (m[k] & (m[k] - 1u)) fm_dot(x[k], y[k] - 3, rc);   /* several modulating one */
            }
        if (car >> k & 1u) {
            FM6_CHART_HOOK(FMH_CAR, k, 0);
            cv_rect(x[k], y[k] + FM_BH, 1, bus - y[k] - FM_BH, T_MID);
            if (x[k] != cl) fm_dot(x[k], bus, T_MID);
        }
    }
    FM6_CHART_HOOK(FMH_BUS, 0, 0);
    fm_bus(cl, cr, bus);
    FM6_CHART_HOOK(FMH_FB, fbop, 0);
    fm_loop(x[fbop], y[fbop], hot == P_E1 ? T_ACCENT : fb ? T_MID : T_DIM);
    for (k = 0; k < 6u; k++) {
        uint32_t on = pt[(5u - k) * FP_OP + FP_OL] != 0, fill_c = (car >> k & 1u) && on;
        uint16_t f = hotset >> k & 1u ? T_ACCENT : c, fill = k == sel ? T_ACCENT : fill_c ? f : T_RAISE;
        char n[2] = {(char)('1' + k), 0};
        FM6_CHART_HOOK(FMH_BOX, k, 0);
        cv_rrect(x[k] - 10, y[k], 21, FM_BH, 3, fill, T_SURF);
        GFX_HOOK_ALIGN(x[k] - 10, y[k], x[k] + 11, y[k] + FM_BH, AL_HV, "FM6 operator number in its box");
        cv_text_in(x[k] - 10, y[k] + CAP_IN(S, FM_BH), 21, &AF_S, n, fill_c || k == sel ? T_INK : on ? f : T_DIM, fill);
    }
    FM6_CHART_HOOK(FMH_LABEL, 0, 0);
    cv_text(10, 4 - GOY, &AF_S, b, hot == P_E0 || hot == P_E7 ? T_ACCENT : T_MID);
    if (sel < 6u) {                                  /* the operator picked: "OP3 x1.50" */
        char fq[16] = {'O', 'P', (char)('1' + sel), ' ', 0};
        fop_freq(pt + (5u - sel) * FP_OP, fq + 4);
        cv_text_r(228, 4 - GOY, &AF_S, fq, T_TEXT, T_SURF);
    } else if (fm6_bank_slot(fm6_slot[(t - trk) % NTRK]) >= 0) {   /* SLOT B1..B32: the bank voice's name */
        char nm[11];
        for (k = 0; k < 10u; k++)
            nm[k] = (char)pt[FP_NAME + k];
        for (nm[10] = 0; k && nm[k - 1] == ' '; k--)
            nm[k - 1] = 0;
        cv_text_r(228, 4 - GOY, &AF_S, nm, T_TEXT, T_SURF);
    }
    FM6_CHART_HOOK(FMH_END, alg, 0);
}

/* OP ENV (ui_fm6op.c): the picked operator's envelope as the patch has it: from L4 to L1 at R1, L2 at R2, L3 at R3, held
 * there while the key is down (a RAISE mark where it is let go), back to L4 at R4. A stage's width grows as its rate
 * falls (4..48 px: 99 is a step, 0 the longest), the levels 0..99 linearly (dB). The curve THEME, the stage KNOB 3 / 4
 * edit (STAGE) ACCENT with a dot at its target; "OP3" top left, the stage's rate and level top right */
static void graph_fm6env(const track_t *t, uint16_t c)
{
    static const char *const STG[4] = {"ATK", "DEC", "SUS", "REL"};
    const uint8_t *op = fop_bytes((uint32_t)(t - trk));
    int32_t top = 24, bot = 88, x[6], y[6], k;
    uint32_t sg = fop_stg & 3u;
    char b[24] = {'O', 'P', (char)('1' + fop_op % 6u), 0};
#define FEGY(l) (bot - (int32_t)(l) * (bot - top) / 99)
#define FEGW(r) (4 + (99 - (int32_t)(r)) * (99 - (int32_t)(r)) * 44 / 9801)
    x[0] = 12;
    y[0] = FEGY(op[FP_L1 + 3]);
    for (k = 0; k < 3; k++) {
        x[k + 1] = x[k] + FEGW(op[FP_R1 + k]);
        y[k + 1] = FEGY(op[FP_L1 + k]);
    }
    x[5] = 228;
    x[4] = x[5] - FEGW(op[FP_R1 + 3]);
    y[4] = y[3];
    y[5] = y[0];
    cv_rect(PANEL_X0, bot + 2, PANEL_W, 1, T_RAISE);
    cv_rect(x[4], top, 1, bot - top + 1, T_RAISE);   /* the key let go */
    for (k = 0; k < 5; k++)                          /* (3: the hold, no stage of its own) */
        cv_stroke(LN(x[k]), LN(y[k]), LN(x[k + 1]), LN(y[k + 1]), k != 3 && (uint32_t)(k < 3 ? k : 3) == sg ? T_ACCENT : c);
    k = sg < 3u ? (int32_t)sg + 1 : 5;
    fm_dot(x[k], y[k], T_ACCENT);
#undef FEGY
#undef FEGW
    cv_text(10, 4 - GOY, &AF_S, b, T_MID);
    str_cpy(b, STG[sg], sizeof b);
    str_cpy(b + str_len(b), "  R ", 5);
    fmt_int(b + str_len(b), op[FP_R1 + sg]);
    str_cpy(b + str_len(b), "  L ", 5);
    fmt_int(b + str_len(b), op[FP_L1 + sg]);
    cv_text_r(228, 4 - GOY, &AF_S, b, T_TEXT, T_SURF);
}

#if FELUCCA_FM4
/* DIGITAL's eight algorithms as FM charts, read from src/eng_digital.c's switch (alg) (ui_test.c checks
 * these tables against it). FM_CELL: per operator 1..4 its grid cell, the column in 24 px steps (bits 0..2)
 * and the row above the output bus (bits 4..5; row 0 = the carriers). FM_MOD: per destination operator d a
 * nibble at bit 4 d of the operators modulating it (bit k = operator k + 1). Operator 4 feeds itself (FB). */
static const uint8_t FM_CELL[8][4] = {
    {0x00, 0x10, 0x20, 0x30}, {0x01, 0x11, 0x20, 0x22}, {0x01, 0x10, 0x20, 0x12}, {0x01, 0x12, 0x10, 0x20},
    {0x00, 0x10, 0x02, 0x12}, {0x00, 0x02, 0x04, 0x12}, {0x00, 0x02, 0x04, 0x14}, {0x00, 0x02, 0x04, 0x06}};
static const uint16_t FM_MOD[8] = {0x0842, 0x00C2, 0x004A, 0x0806, 0x0802, 0x0888, 0x0800, 0x0000};
/* Modulators above what they modulate, the carriers on the bottom row over the output bus (an arrow out).
 * Carriers THEME with INK numerals, modulators RAISE with THEME ones; an operator at LEVEL 0: DIM numeral.
 * Routes by IDX: DIM at 0, MID, THEME from 64; op 4's feedback loop MID (DIM at FB 0). The knob just turned
 * (an operator's ratio or level, IDX, FB): ACCENT. Junction dots where routes split or merge. */
static void graph_fm(const track_t *t, uint16_t c)
{
    uint32_t alg = (uint32_t)t->p[P_E0] & 7u, k, j, mods = FM_MOD[alg], car = 0, hop = 9;
    uint32_t hot = ui.hot_t ? (uint32_t)cur_page()->id[ui.hot_col & 3u] : 0u;
    int32_t x[4], y[4], lo = 7, hi = 0, top = 0, bus, cl = 240, cr = 0, idx = t->p[P_E4];
    uint16_t ec = hot == P_E4 ? T_ACCENT : !idx ? T_DIM : idx < 64 ? T_MID : c;
    if (hot >= P_E1 && hot <= P_E3) hop = hot - P_E1 + 1u;
    else if (hot >= P_FM1_ATK && hot < P_E0) hop = (hot - P_FM1_ATK) / 5u;
    for (k = 0; k < 4u; k++) {
        int32_t col = FM_CELL[alg][k] & 7, row = FM_CELL[alg][k] >> 4;
        lo = col < lo ? col : lo; hi = col > hi ? col : hi; top = row > top ? row : top;
    }
    bus = 50 + (27 + 23 * top) / 2;                  /* the chart centred on the 100 px scale */
    for (k = 0; k < 4u; k++) {
        x[k] = 120 + (FM_CELL[alg][k] & 7) * 24 - (lo + hi) * 12;
        y[k] = bus - 6 - FM_BH - 23 * (FM_CELL[alg][k] >> 4);
        if (FM_CELL[alg][k] < 0x10) { car |= 1u << k; cl = x[k] < cl ? x[k] : cl; cr = x[k] > cr ? x[k] : cr; }
    }
    for (k = 0; k < 4u; k++) {                       /* into k: sources down to its jog row, across, down */
        uint32_t m = (mods >> (4u * k)) & 15u;
        for (j = 0; j < 4u; j++) if (m >> j & 1u) {
            uint32_t fan = mods & (0x1111u << j);
            fm_route(x[j], y[j] + FM_BH, x[k], y[k] - 3, y[k], ec);
            if (fan & (fan - 1u)) fm_dot(x[j], y[k] - 3, ec);   /* one operator modulating several */
        }
        if (m & (m - 1u)) fm_dot(x[k], y[k] - 3, ec);           /* several modulating one */
        if (car >> k & 1u) {
            cv_rect(x[k], y[k] + FM_BH, 1, bus - y[k] - FM_BH, T_MID);
            if (x[k] != cl) fm_dot(x[k], bus, T_MID);
        }
    }
    fm_bus(cl, cr, bus);                             /* the output bus and its arrow */
    fm_loop(x[3], y[3], hot == P_E6 ? T_ACCENT : t->p[P_E6] ? T_MID : T_DIM);   /* op 4's feedback */
    for (k = 0; k < 4u; k++) {
        char b[2] = {(char)('1' + k), 0};
        uint32_t on = t->p[P_FM1_LEVEL + k * 5u] != 0, fill_c = (car >> k & 1u) && on;
        uint16_t f = k == hop ? T_ACCENT : c, fill = fill_c ? f : T_RAISE;
        cv_rrect(x[k] - 10, y[k], 21, FM_BH, 3, fill, T_SURF);
        GFX_HOOK_ALIGN(x[k] - 10, y[k], x[k] + 11, y[k] + FM_BH, AL_HV, "DIGITAL operator number in its box");
        cv_text_in(x[k] - 10, y[k] + CAP_IN(S, FM_BH), 21, &AF_S, b, fill_c ? T_INK : on ? f : T_DIM, fill);
    }
}
#endif
/* SONG's timeline (graph_song): the sections left to right, each as wide as it plays: REPS x its clock track's pattern
 * (song_chain.c chain_clock: the first track not silent there; pattern.c pat_mirror: its pattern's LEN x DIV, what
 * chain_tick counts; a pattern not saved, or every track silent: one bar). A short song is stretched over the strip
 * (fitted); a section under SG_MINW gets SG_MINW and the rest is shared out again; when the sections would get under
 * SG_FITBAR px a bar the song scrolls at SG_BAR px a bar, the view following the cursor's section and, playing, the
 * playing one. + ADD's slot (count < CHAIN_ROWS) at the end. Content px: 0 .. the width. Four lanes, T1 at the top */
#define SG_X0 24                                       /* the strip: x 24 .. 234 (the lanes' cushions left of it) */
#define SG_W 210
#define SG_Y 3                                         /* the lanes: y 3 .. 88 */
#define SG_LH 20                                       /* a lane's height (its letter, the pattern's picture), SG_LG between two */
#define SG_LG 2
#define SG_H (NTRK * (SG_LH + SG_LG) - SG_LG)
#define SG_LY(k) (SG_Y + (int32_t)(k) * (SG_LH + SG_LG))   /* lane k's top */
#define SG_GAP 2                                       /* between two blocks (in a block's width) */
#define SG_MINW 16                                     /* a block's least width, its gap included: a letter in it */
#define SG_ADDW 20                                     /* + ADD's slot */
#define SG_BAR 6                                       /* scrolled: px a bar */
#define SG_FITBAR 3                                    /* fitted: at least this many px a bar */
#define SG_BARU 192u                                   /* a 4/4 bar in 1/48 quarters */
#define SG_RY (SG_Y + SG_H + 3)                        /* the ruler's line (92); its ticks and numbers under it */
typedef struct {
    uint32_t n, add, fit;                              /* the rows, + ADD shown, fitted (no scrolling) */
    uint32_t u0[CHAIN_ROWS + 1u];                      /* each row's start in 1/48 quarters; u0[n]: the song's length */
    int32_t x[CHAIN_ROWS + 2u];                        /* each row's left edge (content px); x[n]: + ADD's; x[n + 1]: the end */
} song_lay_t;
static struct { int32_t off; uint32_t frame; uint8_t init, cur, play, last; } sgv;   /* the view (scrolled) */
/* a * b / c without a 64-bit divide: a and c halved alike until a * b fits */
static uint32_t sg_muldiv(uint32_t a, uint32_t b, uint32_t c)
{
    while (b && a > 0xFFFFFFFFu / b) { a >>= 1; c >>= 1; }
    return c ? a * b / c : 0u;
}
static uint32_t song_unit(const chain_row_t *r)        /* one pass of a section: its clock track's (1/48 quarters) */
{
    uint32_t s = r->slot[chain_clock(r)];
    return pat_used(s) && pat_info(s)->len48 ? pat_info(s)->len48 : SG_BARU;
}
static void song_layout(song_lay_t *L)
{
    uint32_t i, n = chain_config.count <= CHAIN_ROWS ? chain_config.count : CHAIN_ROWS, forced = 0, nf = 0, uf = 0, chg = 1;
    uint32_t wb;
    L->n = n;
    L->add = n < CHAIN_ROWS;
    L->u0[0] = 0;
    for (i = 0; i < n; i++)
        L->u0[i + 1u] = L->u0[i] + chain_config.row[i].repeat * song_unit(&chain_config.row[i]);
    wb = SG_W - (L->add ? SG_ADDW : 0u);
    while (chg) {                                      /* fitted: shared by length, SG_MINW the least */
        chg = 0; nf = 0; uf = 0;
        for (i = 0; i < n; i++)
            if ((forced >> i) & 1u) nf++;
            else uf += L->u0[i + 1u] - L->u0[i];
        if (nf * SG_MINW > wb)
            break;
        for (i = 0; i < n; i++)
            if (!((forced >> i) & 1u) && sg_muldiv(L->u0[i + 1u] - L->u0[i], wb - nf * SG_MINW, uf) < SG_MINW) {
                forced |= 1u << i;
                chg = 1;
            }
    }
    L->fit = nf * SG_MINW <= wb && (!uf || sg_muldiv(SG_BARU, wb - nf * SG_MINW, uf) >= SG_FITBAR);
    if (L->fit) {
        uint32_t nb = 0, ub = 0;
        for (i = 0; i <= n; i++) {
            L->x[i] = (int32_t)(nb * SG_MINW + (uf ? sg_muldiv(ub, wb - nf * SG_MINW, uf) : 0u));
            if (i < n && ((forced >> i) & 1u)) nb++;
            else if (i < n) ub += L->u0[i + 1u] - L->u0[i];
        }
    } else {
        L->x[0] = 0;
        for (i = 0; i < n; i++) {
            uint32_t w = ((L->u0[i + 1u] - L->u0[i]) * SG_BAR + SG_BARU / 2u) / SG_BARU;
            L->x[i + 1u] = L->x[i] + (int32_t)(w > SG_MINW ? w : SG_MINW);
        }
    }
    L->x[n + 1u] = L->x[n] + (L->add ? SG_ADDW : 0);
}
static int32_t song_ux(const song_lay_t *L, uint32_t u)   /* 1/48 quarters -> content px (the blocks' scale) */
{
    uint32_t i = 0;
    while (i + 1u < L->n && u >= L->u0[i + 1u])
        i++;
    if (!L->n)
        return 0;
    return L->x[i] + (int32_t)sg_muldiv(u - L->u0[i], (uint32_t)(L->x[i + 1u] - L->x[i]), L->u0[i + 1u] - L->u0[i]);
}
static uint32_t song_cur(void) { return ui.song_row < chain_config.count ? ui.song_row : chain_config.count; }
/* the playhead, content px: the playing section's repeats played and its clock track's step in its pattern (-1: none) */
static int32_t song_playx(const song_lay_t *L)
{
    uint32_t r = chain.row, len, idx, reps;
    const track_t *t = &trk[chain.clk % NTRK];
    if (!chain.running || r >= L->n)
        return -1;
    len = t->p[P_SLEN] > 0 ? (uint32_t)t->p[P_SLEN] : 1u;
    idx = t->seq_pos >= 0x7FFFFFFFu ? 0u : t->seq_idx % len;
    reps = chain.config.row[r].repeat ? chain.config.row[r].repeat : 1u;
    return L->x[r] + (int32_t)sg_muldiv((reps - (chain.remaining <= reps ? chain.remaining : reps)) * len + idx,
                                        (uint32_t)(L->x[r + 1u] - L->x[r] - SG_GAP), reps * len);
}
/* once a frame: the view toward the cursor's block and the playing one (both when they fit, else the one that moved
 * last), a block's margin round them; halving the way with MENU > ANIM ON, at once on a redraw forced, a gap in
 * the frames or ANIM OFF. Returns the view's left edge */
static int32_t song_follow(const song_lay_t *L)
{
    uint32_t c = song_cur(), p = chain.running && chain.row < L->n ? chain.row : 0xFFu;
    int32_t a = L->x[c], b = c < L->n ? L->x[c + 1u] - SG_GAP : L->x[c] + SG_ADDW, tgt = sgv.off, d, m, end;
    int fresh = !sgv.init || ui.frame != sgv.frame + 1u, snap = fresh || ui.force || (ui_prefs & PREF_ANIM_OFF);
    if (sgv.init && sgv.frame == ui.frame)
        return sgv.off;
    if (fresh) {                                     /* the page (re)entered: the playing row first */
        sgv.off = 0;
        sgv.last = p != 0xFFu;
    } else if (p != sgv.play) {
        sgv.last = 1;
    } else if (c != sgv.cur) {
        sgv.last = 0;
    }
    if (p != 0xFFu) {
        int32_t pa = L->x[p], pb = L->x[p + 1u] - SG_GAP;
        if ((pb > b ? pb : b) - (pa < a ? pa : a) <= SG_W) {
            a = pa < a ? pa : a;
            b = pb > b ? pb : b;
        } else if (sgv.last) {
            a = pa;
            b = pb;
        }
    }
    m = (SG_W - (b - a)) / 2;
    m = m > SG_MINW ? SG_MINW : m < 0 ? 0 : m;
    if (a - m < tgt) tgt = a - m;
    if (b + m > tgt + SG_W) tgt = b + m - SG_W;
    end = L->x[L->n + 1u] - SG_W;
    tgt = L->fit || end < 0 ? 0 : tgt < 0 ? 0 : tgt > end ? end : tgt;
    d = tgt - sgv.off;
    sgv.off = snap ? tgt : sgv.off + (d / 3 ? d / 3 : d > 0 ? 1 : d < 0 ? -1 : 0);
    sgv.cur = (uint8_t)c;
    sgv.play = (uint8_t)p;
    sgv.frame = ui.frame;
    sgv.init = 1;
    return sgv.off;
}

/* MENU > LARGE's strip under the tall cards (ui.c LK_TALL): HOME's scope, ENV's ADSR, LFO's wave,
 * MIXER's tracks drawn small; on the other pages the page's title and number (their charts need the whole panel) */
enum { SK_NONE, SK_SCOPE, SK_ADSR, SK_LFO, SK_TRK, SK_TITLE };
static uint32_t strip_kind(void)
{
    uint32_t g;
    if (large_kind() != LK_TALL)
        return SK_NONE;
    if (ui.home)
        return SK_SCOPE;
    g = cur_page()->graph;
    return g == GR_ADSR ? SK_ADSR : g == GR_LFO ? SK_LFO : g == GR_TRK ? SK_TRK : SK_TITLE;
}
/* the page's title and its number in its family ("ENV DEST 2/2", EDIT: the engine's "OSC 1/2"); the piano roll: its
 * scale ("C MIN"); ti holds 20 (the footer, the strip) */
static void page_title(char *ti)
{
    const page_t *pg = cur_page();
    const track_t *t = TSEL;
    const engine_t *e = ENGINES[t->eng_req % NENGINES];
    uint32_t i, n = 0, k = 0;
    const char *pt = pg->scope == SC_ENGINE ? e->page_title[pg->id[0] != P_E0] : 0;   /* EDIT: the engine's */
    for (i = 0; i < NPAGES; i++)
        if (PAGES[i].fam == pg->fam && page_visible(i)) {
            n++;
            if (i == ui.page)
                k = n;
        }
    if (pg->fam == FAM_TRK) {                       /* MRC: HOME ("SOUND") is the round's second, after the MIXER */
        n++;
        k += k > 1u;
    }
    str_cpy(ti, pt ? pt : grid_on() && !detail_on() ? "GRID" : pg->title, 12);
    if (n > 1) {
        str_cpy(ti + str_len(ti), " ", 4);
        fmt_int(ti + str_len(ti), (int32_t)k);
        str_cpy(ti + str_len(ti), "/", 4);
        fmt_int(ti + str_len(ti), (int32_t)n);
    }
    if (pg->graph == GR_ROLL && !drum_track(t) && !detail_on()) {   /* the piano roll: its tinted rows' scale ("C MIN") */
        str_cpy(ti, N_NOTE[(uint32_t)t->p[P_ROOT] % 12u], 4);
        str_cpy(ti + str_len(ti), " ", 4);
        str_cpy(ti + str_len(ti), N_SCALE[clamp(t->p[P_SCALE], 0, (int32_t)(sizeof N_SCALE / sizeof N_SCALE[0]) - 1)], 8);
    }
}
/* HOME's title: "SOUND 2/3", its place in the MIXER, HOME, CLOCK round (the MIXER's and CLOCK's: page_title) */
static void home_title(char *ti)
{
    uint32_t i, n = 1;
    for (i = 0; i < NPAGES; i++)
        n += PAGES[i].fam == FAM_TRK && page_visible(i);
    str_cpy(ti, "SOUND 2/", 12);
    fmt_int(ti + str_len(ti), (int32_t)n);
}
/* the strip's title: L centred (M when L lacks a glyph or is too wide) */
static void graph_title(void)
{
    char ti[20];
    const aafont_t *f = &AF_L;
    int32_t y;
    page_title(ti);
    if (!large_face_has(ti) || text_w(f, ti) > 220)
        f = &AF_M;
    y = f == &AF_L ? CAP_IN(L, LG_H_GRAPH) : CAP_IN(M, LG_H_GRAPH);
    GFX_HOOK_ALIGN(0, 0, 240, LG_H_GRAPH, AL_HV, "large strip title centred");
    cv_text_in(0, y, 240, f, ti, T_THEME, T_SURF);
}

static uint32_t graph_signature(void)
{
    const page_t *pg = cur_page();
    const track_t *t = TSEL;
    uint32_t h = 2166136261u, i;
    if (ui.home)
        return h ^ (ui.frame / 2u);                  /* scope: redraw every other frame */
    h ^= (uint32_t)pg->graph * 131u + TSEL->eng_req + song.sel * 7777u + ui.page * 1291u;
    if (strip_kind() == SK_TITLE)                    /* MENU > LARGE: the page's title (its engine, its number) */
        return h ^ 0x5A17u;
    if ((pg->graph == GR_NONE && !(pg->scope == SC_ENGINE && ed_still(t))) || pg->graph == GR_ARP || pg->graph == GR_CHO ||
        pg->graph == GR_LANES ||
        (pg->graph == GR_INS && t->p[P_ITYPE] >= IT_PHASER)) h ^= ui.frame / 2u;   /* (the swept INSERTs move) */
    if (pg->graph == GR_CHO)                        /* (the globals, not the track's) */
        h = (h ^ ((uint32_t)song.g[G_CRATE] | (uint32_t)song.g[G_CDEPTH] << 8)) * 16777619u;
    if (pg->graph == GR_REV) {                       /* (the globals; all three move) */
        h = (h ^ ((uint32_t)song.g[G_RTYPE] | (uint32_t)song.g[G_RSIZE] << 8 | (uint32_t)song.g[G_RDAMP] << 16)) * 16777619u;
        h ^= ui.frame / 2u;   /* (all three move: the cube turns, the coil rolls, the torus turns) */
    }
    if (pg->graph == GR_DLY) {                       /* BOUNCE, while a note is still drawn; the globals and the tempo shape them */
        h = (h ^ ((uint32_t)song.g[G_DTIME] | (uint32_t)song.g[G_DFDBK] << 8 | (uint32_t)song.g[G_DCOLOR] << 16 |
                  (uint32_t)song.g[G_DMIX] << 24)) * 16777619u;
        h = (h ^ delay_samples()) * 16777619u;
        if (bn_live()) h ^= ui.frame / 2u;
    }
    for (i = 0; i < P_COUNT; i++)
        h = (h ^ (uint32_t)t->p[i]) * 16777619u;
    h ^= (uint32_t)TSEL->preset * 7u + ui.pslot * 13u + TSEL->user * 257u + up_gen * 7919u + ui.uslot * 104729u +
         ui.ppick * 1299709u + pat_gen * 15485863u;
    if (pg->graph == GR_CHORD) {                     /* the last chord played */
        h = (h ^ (chord_last[song.sel].root + 131u * chord_last[song.sel].mask)) * 16777619u;
        for (i = 0; i < CHORD_MAX; i++)
            h = (h ^ chord_last[song.sel].note[i]) * 16777619u;
        h ^= (uint32_t)t->engine * 389u;             /* (MONO and kits follow the sounding engine) */
    }
    if (pg->graph == GR_SONG) {
        h ^= ui.song_row * 40503u + chain_config.count * 7919u + ui.song_trk * 3571u;
        h = fnv(h, chain_config.row, sizeof chain_config.row);
        h ^= chain.running ? (chain.row + 1u) * 104729u + chain.remaining * 1299709u : 0u;
        {                                            /* the timeline: its view (following, ui_graph.c song_follow), the playhead */
            song_lay_t L;
            song_layout(&L);
            h = (h ^ (uint32_t)song_follow(&L)) * 16777619u;
            h = (h ^ (uint32_t)(song_playx(&L) + 1)) * 16777619u;
            h ^= (ui_prefs & PREF_LARGE) ? 0x2545F491u : 0u;
        }
    }
    if (pg->graph == GR_MOD)
        h ^= (mod_ui_slot + 1u) * 40503u;
    if (pg->graph == GR_CLOCK) {                     /* the metronome's stick, while the click's clock runs */
        uint32_t lit;
        h = (h ^ (uint32_t)mt_pose(&lit) ^ lit << 24) * 16777619u;
    }
    if (pg->graph == GR_SLCR && t->p[P_SLCR])        /* the SLICER's step playing */
        h ^= (sl[song.sel].idx + 1u) * 2654435761u;
#if FELUCCA_SLICE
    if (pg->graph == GR_SLICES && slice_page_ok()) h ^= slice_sig();
#endif
    if (pg->scope == SC_ENGINE && (ENGINES[t->eng_req % NENGINES] == &ENG_WHEEL || t->eng_req % NENGINES == ENGI_FM6 ||
                                   (FELUCCA_FM4 && t->eng_req % NENGINES == ENGI_DIGITAL)))
        h ^= (ui.hot_t ? ui.hot_col + 1u : 0u) * 65537u;
    if ((pg->scope == SC_ENGINE || fop_page(pg->graph)) && t->eng_req % NENGINES == ENGI_FM6)   /* the patch (PAT's algorithm,
                                                     * levels, FB; the operator pages: the operator, the stage) */
        h ^= (fm6_pgen[(t - trk) % NTRK] + 1u) * 2246822519u + (fop_op + 1u) * 40503u + (fop_stg + 1u) * 7919u;
    if (pg->scope == SC_ENGINE && ENGINES[t->eng_req % NENGINES] == &ENG_SAMPLE) h ^= sample_wave.pos * 13u + sample_wave.key;
    if (pg->graph == GR_ROLL) {
        uint32_t ph = song.playing ? t->seq_idx : 0xFFFFu;
        if (ph / 16u != ui.bank)
            ph = 0xFFFFu;                            /* the roll shows the cursor's bank only */
        h ^= steps_hash(t) + ph * 31u + ui.cursor * 7919u + ui.lane * 104723u + ui.bank * 613u +
             drum_mute[trk_index(t) % NPART] * 2654435761u;
        {                                            /* the parameter locks' marks (the roll, the grid) */
            uint64_t lk = motion_lock_steps(trk_index(t));
            h = (h ^ (uint32_t)lk ^ (uint32_t)(lk >> 32) * 2654435761u) * 16777619u;
        }
        if (detail_on())                             /* DETAIL: QUANTIZE (NUDGE's DIM), the lane named in the accent */
            h = (h ^ (0xD1u + (uint32_t)t->p[P_SQNT] * 31u + (ui.hot_t ? ui.hot_col + 1u : 0u) * 977u)) * 2654435761u;
        else if (!drum_track(t)) {                   /* the roll: its view and the keys held */
            pr_follow(t);
            h = (h ^ (proll.lo + 1u)) * 16777619u;
            h = (h ^ pr_held()) * 16777619u;
        }
    }
    return h;
}
/* 4-letter engine tags of the preset list */
static const char *eng_abbr(const char *name)
{
    static const char *const A[][2] = {{"ANALOG", "ANLG"},
#if FELUCCA_FM4
                                       {"DIGITAL", "DGTL"},
#endif
                                       {"PHASE", "PHAS"}, {"LOFI", "LOFI"},
                                       {"SAMPLE", "SMPL"}, {"VOICE", "VOCL"}, {"TRIO", "TRIO"}, {"WHEEL", "WHEL"},
                                       {"GRAIN", "GRAN"}, {"PHYS", "PHYS"}, {"DRUM", "DRUM"},
                                       {"NOISE", "NOIS"}, {"FM6", "FM6"}, {"SLICE", "SLCE"},
                                       {"ADPCM", "ADPC"}};
    uint32_t i;
    for (i = 0; i < sizeof A / sizeof A[0]; i++)
        if (str_eq(name, A[i][0]))
            return A[i][1];
    return name;
}

/* a list row (17 px), in M as every list's (PRESETS, USER, PATTERNS, PROJECT, MOD, the MENU): the selected
 * one a THEME bar with INK text; tag at x 14 (the widest, DRUM, to x 57), free text from x LIST_NX to x1 */
#define LIST_Y(k) (3 + 17 * (int32_t)(k))
#define LIST_TY CAP_IN(M, 16)                           /* a row's text: its capitals rows 2 .. 12 of the row */
#define LIST_NX 64
static void list_row(int32_t y, int sel, const char *tag, uint16_t tc, const char *name, uint16_t nc, int32_t x1)
{
    uint16_t bg = sel ? T_THEME : T_SURF;
    if (sel)
        cv_rrect(6, y, 228, 16, 4, T_THEME, T_SURF);
    GFX_HOOK_ALIGN(0, y, 0, y + 16, AL_V, "list row text centred up/down");
    cv_text_on(14, y + LIST_TY, &AF_M, tag, sel ? T_INK : tc, bg);
    if (name[0])
        GFX_HOOK_ALIGN(0, y, 0, y + 16, AL_V, "list row text centred up/down");
    cv_free_text(LIST_NX, y + LIST_TY, &AF_M, name, sel ? T_INK : nc, bg, x1 - LIST_NX);
}
/* an empty list: a title and a hint, centred */
static void note_line(int32_t y, const char *s, uint16_t fg)   /* centred S; "[K2] ADD PATTERN": a key hint */
{
    uint32_t n;
    int32_t id = kc_tag(s, &n);
    if (id < 0) {
        GFX_HOOK_ALIGN(0, 0, 240, 0, AL_H, "panel note centred");
        cv_text_in(0, y, 240, &AF_S, s, fg, T_SURF);
        return;
    }
    s += n;
    while (*s == ' ')
        s++;
    GFX_HOOK_ALIGN(0, 0, 240, 0, AL_H | AL_N(2), "panel note hint centred");
    cv_text_on(cv_keycap(HALF_UP(240 - kh_ink((uint32_t)id, s)), y + 1, (uint32_t)id, T_KEY, T_INK, T_SURF) + KH_GAP, y,
               &AF_S, s, fg, T_SURF);
}
static void panel_note(const char *a, const char *b, const char *c)
{
    GFX_HOOK_ALIGN(0, 0, 240, 0, AL_H, "panel note centred");
    cv_text_in(0, 30, 240, &AF_M, a, T_TEXT, T_SURF);
    if (b) note_line(60, b, T_MID);
    if (c) note_line(82, c, T_DIM);
}
/* the same title led by an icon (16 px, the accent's: the header message's, ui_draw.c draw_head) in the middle of
 * the panel: the title's capitals centred up and down in it, the icon's ink on them; across, the pair centred by
 * its ink */
#define PA_GAP 6                                        /* icon ink -> title ink (setup_title's) */
static void panel_alert(uint32_t id, const char *a)
{
    int32_t bi[4], bt[4], x, y = CAP_IN(M, (int32_t)graph_h());
    icon_ink(16, id, bi);
    text_ink(&AF_M, a, bt);
    x = HALF_UP(240 - (bi[2] - bi[0] + PA_GAP + bt[2] - bt[0])) - bi[0];
    GFX_HOOK_ALIGN(0, 0, 240, 0, AL_H | AL_N(2), "panel alert centred");
    GFX_HOOK_ALIGN(0, y + AF_M_CAP_Y, 0, y + AF_M_CAP_Y + AF_M_CAP_H, AL_V | AL_PASS, "panel alert icon on its title's line");
    cv_icon_in(x, y + AF_M_CAP_Y, 0, AF_M_CAP_H, 16, id, T_ACCENT, T_SURF);
    GFX_HOOK_ALIGN(0, 0, 0, (int32_t)graph_h(), AL_V | AL_PASS, "panel alert title in the panel's middle");
    cv_text_on(x + bi[2] + PA_GAP - bt[0], y, &AF_M, a, T_TEXT, T_SURF);
}

/* MRC: a sound list's row, the PRESETS page's and the full-screen list's (ui_draw.c draw_presets_full) alike: each
 * sound one line in S, the loaded one taller in a dim RAISE box, its name in M */
#define PREF_H 15                                      /* a neighbour's row, S */
#define PREF_PAD 7                                     /* the loaded sound's box: its name's capitals to its edges */
#define PREF_HC (2 * PREF_PAD + 2 + AF_M_CAP_H)        /* .. its row (1 px clear above and below the box) */
/* one row at y, h high: the tag (engine / U07 / P07) in the accent, the name (empty: DIM), a favourite's star; the
 * selected one (sel) in a RAISE box, its name in M (MRC: no suggested pattern at the right). Every piece's capitals
 * centred on the row */
static void pref_line(int32_t y, int32_t h, const char *tag, const char *nm, int empty, int star, int sel)
{
    int32_t ty = y + CAP_IN(S, h), x1 = 226;
    uint16_t bg = sel ? T_RAISE : T_SURF;
    if (sel)
        cv_rrect(6, y + 1, 228, h - 2, 6, T_RAISE, T_SURF);
    cv_text_on(14, ty, &AF_S, tag, T_ACCENT, bg);
    if (star) {
        cv_icon_in(214, y, 0, h, 12, ICON_X_STAR, T_ACCENT, bg);
        x1 = 210;
    }
    if (!sel)
        cv_free_text(LIST_NX, ty, &AF_S, nm, empty ? T_DIM : T_MID, bg, x1 - LIST_NX);
    else
        cv_free_text(LIST_NX, y + CAP_IN(M, h), &AF_M, nm, empty ? T_DIM : T_TEXT, bg, x1 - LIST_NX);
}
static void pref_row(int32_t y, int32_t h, uint32_t index, int sel)
{
    char tag[6], nm[13];
    uint32_t e, k;
    e = preset_at(index, &k);
    if (e == NENGINES) {
        up_slot_label(tag, k);
        up_name(k, nm);
    } else {
        str_cpy(tag, eng_abbr(ENGINES[e]->name), sizeof tag);
        str_cpy(nm, ENGINES[e]->presets[k].name, sizeof nm);
    }
    pref_line(y, h, tag, nm, 0, favorite_has(e, k), sel);
}
/* MRC: a slot list (SAVE > USER, SAVE > PATTERN, GLO > PATTERNS) drawn as the sound list: seven rows centred in the
 * panel, the selected one taller; the window bounded as a category's (ui.c preset_visible) */
static void slot_list(uint32_t cur, uint32_t total, void (*row)(int32_t y, int32_t h, uint32_t k, int sel))
{
    uint32_t i, first = cur > 3u ? cur - 3u : 0u, last = total > 7u ? total - 7u : 0u;
    int32_t y = HALF_UP((int32_t)graph_h() - (6 * PREF_H + PREF_HC));
    if (first > last) first = last;
    for (i = 0; i < 7u && first + i < total; i++) {
        int32_t rh = first + i == cur ? PREF_HC : PREF_H;
        row(y, rh, first + i, first + i == cur);
        y += rh;
    }
}
/* preset browser: the global list (every engine), the current one selected; MRC: drawn as the full-screen list
 * (pref_row), its seven rows centred in the panel */
static void graph_browse(void)
{
    uint32_t total, cur = preset_pos(&total), row;
    int32_t y = HALF_UP((int32_t)graph_h() - (6 * PREF_H + PREF_HC));
    if (!total) {
        char b[24];
        if (list_mode() == 1u) {
            panel_note("NO FAVORITES", "LIST ALL TO ADD SOUNDS", 0);
            return;
        }
        str_cpy(b, "NO ", sizeof b);                    /* a category with no sound (OTHER: tag your own) */
        str_cpy(b + 3, list_name(list_mode()), sizeof b - 3);
        str_cpy(b + str_len(b), " SOUNDS", sizeof b - str_len(b));
        panel_note(b, "[K4] ANOTHER LIST", "TAG YOURS WHEN SAVING");
        return;
    }
    for (row = 0; row < 7u; row++) {
        uint32_t index = preset_visible(cur, total, row);
        int32_t rh = index == cur ? PREF_HC : PREF_H;
        if (index < total)
            pref_row(y, rh, index, index == cur);
        y += rh;
    }
}
/* the EDIT layer (ui_layer.c): the sound loaded, as the browser's selected row: "03" (its place in KNOB 2's list,
 * the engine's sounds) or "U07", the name, the star of a favourite */
static void engine_sound_row(int32_t y)
{
    const engine_t *e = ENGINES[TSEL->eng_req % NENGINES];
    uint32_t total, cur = eng_list_pos(&total), u = user_of(TSEL);
    char tag[6], nm[13];
    int fav = preset_favorite();
    if (u < UP_SLOTS) {
        up_slot_label(tag, u);
        up_name(u, nm);
    } else {
        tag[0] = (char)('0' + (cur + 1u) / 10u % 10u);
        tag[1] = (char)('0' + (cur + 1u) % 10u);
        tag[2] = 0;
        str_cpy(nm, e->npresets ? e->presets[TSEL->preset % e->npresets].name : "", sizeof nm);
    }
    list_row(y, 1, tag, T_DIM, nm, T_TEXT, fav ? 212 : 232);
    if (fav) {
        GFX_HOOK_ALIGN(0, y, 0, y + 16, AL_V, "list row star centred up/down");
        cv_icon_in(214, y, 0, 16, 12, ICON_X_STAR, T_INK, T_THEME);
    }
    (void)total;
}
/* user preset slots around the selected one: "U07  NAME" / "--" (MRC: as the sound list, slot_list) */
static void user_row(int32_t y, int32_t h, uint32_t k, int sel)
{
    char tag[4], nm[13];
    int used = up_used(k);
    up_slot_label(tag, k);
    if (used)
        up_name(k, nm);
    else
        str_cpy(nm, "--", sizeof nm);
    pref_line(y, h, tag, nm, !used, 0, sel);
}
static void graph_user(void)
{
    slot_list(ui.uslot, UP_SLOTS, user_row);
}
/* GLO > PATTERNS: the saved patterns around the one picked ("P07  MY BASS"); none saved: a note */
static void pats_row(int32_t y, int32_t h, uint32_t k, int sel)
{
    char tag[4], nm[13];
    pat_label(k, tag, nm);
    pref_line(y, h, tag, nm, 0, 0, sel);
}
static void graph_pats(void)
{
    uint32_t n = pat_count();
    if (!n) {
        panel_note("NO PATTERNS", "[SAVE] PATTERN TO ADD", 0);
        return;
    }
    slot_list(pat_pick(), n, pats_row);
}
/* MRC: SAVE > PATTERN: the slots around the selected one, P01..P24, a name or "--" (as SAVE > USER's) */
static void psave_row(int32_t y, int32_t h, uint32_t k, int sel)
{
    char tag[4];
    int used = pat_used(k);
    pat_tag(tag, k);
    pref_line(y, h, tag, used ? pat_info(k)->name : "--", !used, 0, sel);
}
static void graph_psave(void)
{
    slot_list(ui.pslot, NPAT, psave_row);
}
/* MIXER page: four SURF columns, one under each card: the track number on its cushion (in the accent:
 * the selected track) with a REC / ARM / MUTE badge (P_MUTE, KNOB 1), the sound's short name (a MUTE badge
 * when armed and muted), the LEVEL knob (dB inside) with the output meter beside it, then the PAN and REV
 * knobs with their values. Knobs: a 270 degree ring, the track RAISE, the value arc THEME (the knob just
 * turned: ACCENT; muted: DIM); PAN from 12 o'clock. Each column is its own canvas with its own signature:
 * while the transport runs only the meters move. */
#define TS_MY 39                                     /* meter slot: column rows 39 .. 70 */
#define TS_MH 32
#define KB_X 4                                       /* LEVEL knob: box 4..37 x 37..70, centre (21, 54) */
#define KB_Y 37
#define KS_Y 86                                      /* PAN / REV knobs: boxes 86..105, centres x 15 and 42 */
static struct {
    uint32_t col[NTRK];
    uint8_t meter[NTRK];
} ts;

static int32_t meter_px(int32_t a)                   /* |sample| (Q15) -> px: 60 dB over the slot */
{
    int32_t lg = 0, v;
    if (a < 64)
        return 0;
    while ((a >> lg) > 1)
        lg++;
    v = lg * 8 + (((a << 3) >> lg) & 7);             /* 8 log2(a): 48 (-54 dB) .. 128 (+6 dB) */
    return clamp((v - 48) * (TS_MH - 2) / 80, 0, TS_MH - 2);
}

/* a knob's ring: 270 degrees (7:30 .. 4:30 o'clock), 2 px, anti-aliased, from the quadrant mask of its size
 * (tools/gen_aa_keycaps.py: coverage and angle; the other three quadrants mirror it, no trigonometry here).
 * Box top-left (x, y), outer radius r. Angles: 1/1024 turn from 12 o'clock, clockwise, -384 .. 384; the value
 * arc is lo .. hi (in vc), the rest of the ring the track (tr); bg lies under the ring */
#define KA_END 384
static void knob_arc(int32_t x, int32_t y, int32_t r, const uint8_t *cov, const uint8_t *ang, int32_t lo, int32_t hi,
                     uint16_t tr, uint16_t vc, uint16_t bg)
{
    const uint16_t *rt = ramp(tr, bg), *rv = ramp(vc, bg);
    int32_t i, j, m;
    for (j = 0; j < r; j++)
        for (i = 0; i < r; i++) {
            uint32_t k = (uint32_t)(j * r + i), a = (cov[k >> 1] >> ((k & 1u) ? 0 : 4)) & 15u;
            int32_t q = ang[k];
            if (!a)
                continue;
            for (m = 0; m < 4; m++) {                /* top right, bottom right, bottom left, top left */
                int32_t s = m == 0 ? q : m == 1 ? 512 - q : m == 2 ? q - 512 : -q;
                int32_t px = m < 2 ? x + r + i : x + r - 1 - i, py = (m == 0 || m == 3 ? y + r - 1 - j : y + r + j) + cv_oy;
                if (s < -KA_END || s > KA_END || (uint32_t)px >= cv_w || (uint32_t)py >= cv_h)
                    continue;
                cv_px[(uint32_t)py * cv_w + (uint32_t)px] = (s >= lo && s <= hi ? rv : rt)[a];
            }
        }
}
/* value v of lo..hi as an arc: from the start, or (bipolar, lo < 0) from 12 o'clock with a 1 px nub at 0 */
static void knob(int32_t x, int32_t y, int32_t r, const uint8_t *cov, const uint8_t *ang, int32_t v, int32_t lo,
                 int32_t hi, uint16_t vc)
{
    int32_t a0, a1;
    if (lo < 0) {
        int32_t s = v * KA_END / (v < 0 ? -lo : hi);
        a0 = (s < 0 ? s : 0) - 8;
        a1 = (s > 0 ? s : 0) + 8;
    } else {
        a0 = -KA_END;
        a1 = v > lo ? -KA_END + (v - lo) * 2 * KA_END / (hi - lo) : -KA_END - 1;
    }
    knob_arc(x, y, r, cov, ang, a0, a1, T_RAISE, vc, T_SURF);
}

static uint32_t trk_level(uint32_t c) { return (uint32_t)trk[c].p[P_LEVEL] & 127u; }   /* LEVEL 0..127 */

static void trk_short_name(uint32_t c, char *b)      /* the track's sound, b holds 13 */
{
    const track_t *t = &trk[c];
    const engine_t *e = ENGINES[t->eng_req % NENGINES];
    if (user_of(t) < UP_SLOTS)
        up_name(user_of(t), b);
    else if (e->npresets)
        str_cpy(b, e->presets[t->preset % e->npresets].name, 13);
    else
        str_cpy(b, e->name, 13);
}

/* MENU > LARGE: the four tracks in the strip under the tall cards (57 x LG_H_GRAPH each): the cushion (the selected
 * one the accent), REC / ARM / MUTE as on the full mixer, the sound's name, LEVEL as a gauge (THEME: the cards'),
 * the output meter under it (MID); a muted track DIM */
#define TSS_GY 40                                    /* the LEVEL gauge, 3 px */
#define TSS_MY 48                                    /* the meter, 3 px */
static void track_strip(uint32_t c, uint32_t sel, uint32_t st, uint32_t mute, uint32_t arm, uint32_t hot, uint32_t lvl,
                        const char *b, uint32_t lit)
{
    int32_t gw = CARD_W - 10, m = ts.meter[c] * gw / (TS_MH - 2), fx = (int32_t)lvl * gw / 127;
    uint16_t vc = mute ? T_DIM : !sel ? T_TEXT : MRC_ON ? T_ACCENT : T_THEME;   /* MRC: the palette's accent on the
                                                   * selected track alone, the rest in ink */
    cv_begin(CARD_W, LG_H_GRAPH, T_BG);
    cv_rrect(0, 0, CARD_W, LG_H_GRAPH, 5, T_SURF, T_BG);
    if (st != 3u)                                    /* muted: the MUTE badge takes the track number's place */
        cv_icon_on(4, 4, 16, trk_icon(c, sel), lit ? (sel ? T_TEXT : T_ACCENT) : sel ? T_ACCENT : T_MID, T_SURF);
    if (st)
        GFX_HOOK_ALIGN(0, 4, 0, 20, AL_V, "mixer badge on the cushion's line");
    if (st == 1u || st == 2u)
        cv_keycap(53 - kc_w(st == 1u ? KC_REC : KC_ARM), 5, st == 1u ? KC_REC : KC_ARM, st == 1u ? T_REC : T_ACCENT,
                  T_INK, T_SURF);
    else if (st)
        cv_keycap(5, 5, KC_MUTE, sel || hot == 4u ? T_ACCENT : T_KEY, T_INK, T_SURF);
    if (mute && arm)                                 /* armed and muted: MUTE in place of the name */
        GFX_HOOK_ALIGN(0, 22 + AF_S_CAP_Y, 0, 22 + AF_S_CAP_Y + AF_S_CAP_H, AL_V, "mixer MUTE badge on the name's line");
    if (mute && arm)
        cv_keycap(5, 22 + AF_S_CAP_Y + HALF_UP(AF_S_CAP_H - KC_H), KC_MUTE, hot == 4u ? T_ACCENT : T_KEY, T_INK, T_SURF);
    else
        cv_free_text(5, 22, &AF_S, b, mute ? T_DIM : sel ? T_TEXT : T_MID, T_SURF, CARD_W - 10);
    cv_rrect(5, TSS_GY, gw, 3, 1, ux.style ? T_LINE : T_BG, T_SURF);
    cv_rrect(5, TSS_GY, fx < 3 ? 3 : fx, 3, 1, hot == 1u ? T_ACCENT : vc, T_BG);
    cv_rrect(5, TSS_MY, gw, 3, 1, T_RAISE, T_SURF);
    if (m)
        cv_rrect(5, TSS_MY, m < 3 ? 3 : m, 3, 1, T_MID, T_RAISE);
    cv_blit((uint32_t)CARD_X(c), LG_Y_GRAPH);
}
static void draw_tracks(void)
{
    uint32_t c, sk = strip_kind() == SK_TRK;
    if (ui.force) {                                  /* the BG beside the columns only, they cover the rest (#181: */
        draw_gaps(graph_y(), graph_h());             /* the whole panel cleared first flashed on every sound load and
                                                      * track change); LINE: divided as the cards above */
        for (c = 0; c < NTRK; c++)
            ts.meter[c] = 0;
    }
    for (c = 0; c < NTRK; c++) {
        track_t *t = &trk[c];
        uint32_t sel = c == song.sel, lvl = trk_level(c), mute = t->p[P_MUTE] != 0, lit = (uint32_t)midi_lit(c);
        uint32_t arm = (song.rec >> c) & 1u, st = arm ? (song.playing ? 1u : 2u) : mute ? 3u : 0u, sig;
        uint32_t hot = sel && ui.hot_t ? ui.hot_col + 1u : 0u;   /* the knob just turned (hot_col + 1): 1 LEVEL, 2 PAN,
                                                                * 3 REV, 4 MUTE (its badge) */
        int32_t pk = t->peak, m, pan = clamp(t->p[P_PAN], -64, 63), rv = clamp(t->p[P_REV], 0, 127);
        uint16_t vc = mute ? T_DIM : !sel ? T_TEXT : MRC_ON ? T_ACCENT : T_THEME;   /* MRC: the palette's accent on the
                                                   * selected track alone, the rest in ink */
        char b[16];
        t->peak = 0;
        trk_short_name(c, b);
        m = mute ? 0 : meter_px(pk);
        if (m < ts.meter[c] - 1)
            m = ts.meter[c] - 1;                     /* falls ~2 dB a frame */
        ts.meter[c] = (uint8_t)(m < 0 ? 0 : m);
        sig = str_hash(1u + sel + st * 2u + (mute && arm) * 16u + hot * 32u, b) + lvl * 7919u +
              ts.meter[c] * 131u + (uint32_t)(pan + 128) * 104729u + (uint32_t)rv * 1299709u + mute * 3u + sk * 0x9E37u +
              lit * 15485863u;                         /* (1.5.5: MIDI IN lights the cushion) */
        if (!ui.force && sig == ts.col[c])
            continue;
        ts.col[c] = sig;
        if (sk) {
            track_strip(c, sel, st, mute, arm, hot, lvl, b, lit);
            continue;
        }
        cv_begin(CARD_W, H_GRAPH, T_BG);
        cv_rrect(0, 0, CARD_W, H_GRAPH, 5, T_SURF, T_BG);
        if (st != 3u)                                /* muted: the MUTE badge takes the track number's place */
            cv_icon_on(4, 5, 16, trk_icon(c, sel), lit ? (sel ? T_TEXT : T_ACCENT) : sel ? T_ACCENT : T_MID, T_SURF);
        if (st)
            GFX_HOOK_ALIGN(0, 5, 0, 21, AL_V, "mixer badge on the cushion's line");
        if (st == 1u || st == 2u)                    /* REC (recording) / ARM (armed, stopped) */
            cv_keycap(53 - kc_w(st == 1u ? KC_REC : KC_ARM), 6, st == 1u ? KC_REC : KC_ARM, st == 1u ? T_REC : T_ACCENT,
                      T_INK, T_SURF);
        else if (st)                                 /* muted (K4 just turned: lit) */
            cv_keycap(5, 6, KC_MUTE, sel || hot == 4u ? T_ACCENT : T_KEY, T_INK, T_SURF);
        if (mute && arm)                             /* armed and muted: MUTE in place of the name */
            GFX_HOOK_ALIGN(0, 21 + AF_S_CAP_Y, 0, 21 + AF_S_CAP_Y + AF_S_CAP_H, AL_V,
                           "mixer MUTE badge on the name's line");
        if (mute && arm)                             /* (on the name's capitals) */
            cv_keycap(5, 21 + AF_S_CAP_Y + HALF_UP(AF_S_CAP_H - KC_H), KC_MUTE, hot == 4u ? T_ACCENT : T_KEY, T_INK, T_SURF);
        else
            cv_free_text(5, 21, &AF_S, b, mute ? T_DIM : sel ? T_TEXT : T_MID, T_SURF, CARD_W - 10);
        {   /* LEVEL: the knob, its dB inside (OFF at 0), and the meter of the output */
            char v[8];
            knob(KB_X, KB_Y, KNOB_BIG_R, KNOB_BIG_COV, KNOB_BIG_ANG, (int32_t)lvl, 0, 127, hot == 1u ? T_ACCENT : vc);
            if (lvl) {
                int32_t d = LEVEL_DB_X10[lvl];
                fmt_int(v, (d + (d < 0 ? -5 : 5)) / 10);
                GFX_HOOK_ALIGN(KB_X, 0, KB_X + 2 * KNOB_BIG_R, 0, AL_H, "mixer level text centred across");
                cv_text_in(KB_X, KB_Y + KNOB_BIG_R + 5, 2 * KNOB_BIG_R, &AF_S, "dB", T_DIM, T_SURF);
            } else {
                str_cpy(v, "OFF", sizeof v);
            }
            GFX_HOOK_ALIGN(KB_X, KB_Y, KB_X + 2 * KNOB_BIG_R, KB_Y + 2 * KNOB_BIG_R, AL_HV, "mixer level value in its knob");
            cv_text_in(KB_X, KB_Y + CAP_IN(S, 2 * KNOB_BIG_R), 2 * KNOB_BIG_R, &AF_S, v, hot == 1u ? T_ACCENT : lvl ? vc : T_DIM, T_SURF);
            cv_rrect(45, TS_MY, 4, TS_MH, 2, T_RAISE, T_SURF);
            if (ts.meter[c])
                cv_rrect(45, TS_MY + TS_MH - 1 - ts.meter[c], 4, ts.meter[c] + 1, ts.meter[c] >= 4 ? 2 : 0, T_MID, T_RAISE);
        }
        {   /* PAN (from the centre) and the REV send: captions, knobs, values */
            char v[8];
            uint16_t pc = hot == 2u ? T_ACCENT : vc, rc = hot == 3u ? T_ACCENT : vc;
            GFX_HOOK_ALIGN(15 - KNOB_SMALL_R, 0, 15 + KNOB_SMALL_R, 0, AL_H, "mixer caption over its knob");
            cv_text_in(15 - KNOB_SMALL_R, 72, 2 * KNOB_SMALL_R, &AF_S, "PAN", T_DIM, T_SURF);   /* (by their ink) */
            GFX_HOOK_ALIGN(42 - KNOB_SMALL_R, 0, 42 + KNOB_SMALL_R, 0, AL_H, "mixer caption over its knob");
            cv_text_in(42 - KNOB_SMALL_R, 72, 2 * KNOB_SMALL_R, &AF_S, "REV", T_DIM, T_SURF);
            knob(15 - KNOB_SMALL_R, KS_Y, KNOB_SMALL_R, KNOB_SMALL_COV, KNOB_SMALL_ANG, pan, -64, 63, pc);
            knob(42 - KNOB_SMALL_R, KS_Y, KNOB_SMALL_R, KNOB_SMALL_COV, KNOB_SMALL_ANG, rv, 0, 127, rc);
            v[0] = '+';                              /* the cards' numbers, without the % */
            fmt_int(pan > 0 ? v + 1 : v, pan * 100 / (pan > 0 ? 63 : 64));   /* (+100 at the top: param_format) */
            GFX_HOOK_ALIGN(15 - KNOB_SMALL_R, 0, 15 + KNOB_SMALL_R, 0, AL_H, "mixer value under its knob");
            cv_text_in(15 - KNOB_SMALL_R, KS_Y + 17, 2 * KNOB_SMALL_R, &AF_S, v, pc, T_SURF);
            fmt_int(v, (rv * 100 + 63) / 127);
            GFX_HOOK_ALIGN(42 - KNOB_SMALL_R, 0, 42 + KNOB_SMALL_R, 0, AL_H, "mixer value under its knob");
            cv_text_in(42 - KNOB_SMALL_R, KS_Y + 17, 2 * KNOB_SMALL_R, &AF_S, v, rc, T_SURF);
        }
        cv_blit((uint32_t)CARD_X(c), Y_GRAPH);
    }
}
/* HOME > TRACKS (1.3, Discussions #112 / #134: MENU > DISPLAY > HOME TRACKS): the four tracks as rows in the panel, one
 * under the other (29 px, 2 apart), in place of the scope; the cards above stay HOME's four (the selected track's). A row:
 * the track's cushion at the left; on its first line the engine's icon and the sound's name (MENU > LARGE: the name in M,
 * no icon), a MUTE badge at its right when muted; under it the 16 steps of the page of the track's pattern that plays
 * (stopped: the selected track's cursor page, the others' first): a note a bar (accented: taller, the accent), a TIE after
 * a note the bar carried on from the one before, DRUM hits a pad (two hits or more: two pads), an empty step or a REST a
 * stub (a TIE with nothing to carry: brighter); the step playing on a raised cell. LEN > 16: a pip per page right of the
 * steps (the page shown THEME). At the right the track's output meter. The selected track's row is the selection (THEME,
 * its ink INK, as a list's selected row); a muted track's steps DIM. Each row is its own canvas with its own signature
 * (ts.col, the MIXER's: the two never show together); a meter that moved alone redraws its slot only. LINE: the rows on
 * BG, 1 px rules between them */
#define TR_H 29                                      /* a row; rows TR_PITCH apart from Y_GRAPH, 3 .. 237 across */
#define TR_PITCH 31
#define TR_W 234
#define TR_NY(lg) ((lg) ? 2 : 4)                     /* the name line's capitals from row 4 (LARGE, M: 2) */
#define TR_SB 26                                     /* the steps: bars standing on row 25 */
#define TR_BW 9                                      /* a step's bar, 11 px apart, 4 more between groups of 4 */
#define TR_PX 216                                    /* the page pips */
#define TR_MX 226                                    /* the meter: 4 x 22 from row 4 */
#define TR_MY 4
#define TR_MH 22
static int32_t tr_bx(uint32_t i) { return 24 + (int32_t)i * 11 + (int32_t)(i / 4u) * 4; }
static void tr_meter(int32_t x, int32_t y, uint32_t c, uint16_t slot, uint16_t fill, uint16_t under)
{
    int32_t m = ts.meter[c] * TR_MH / (TS_MH - 2);
    cv_rrect(x, y, 4, TR_MH, 2, slot, under);
    if (m)
        cv_rrect(x, y + TR_MH - m, 4, m, m >= 4 ? 2 : 0, fill, slot);
}
static void home_track_row(uint32_t c)
{
    track_t *t = &trk[c];
    const step_t *s = seq_steps(t);
    uint32_t sel = c == song.sel, mute = t->p[P_MUTE] != 0, large = (ui_prefs & PREF_LARGE) != 0u, lit = (uint32_t)midi_lit(c);
    uint32_t len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP), np = (len + 15u) / 16u, pg, p0, ph, i, sig, carry = 0;
    int32_t y = Y_GRAPH + (int32_t)c * TR_PITCH, m, x, xr = 222, ch = large ? AF_M_CAP_H : AF_S_CAP_H;
    uint16_t bg = sel ? T_THEME : T_SURF, half = ux_mix(T_THEME, T_INK, 35);
    uint16_t cn = sel ? (mute ? ux_mix(T_THEME, T_INK, 55) : T_INK) : mute ? T_DIM : T_THEME;   /* a note */
    uint16_t ca = sel || mute ? cn : T_ACCENT, cs = sel ? half : T_RAISE;   /* accented; a stub */
    uint16_t cp = sel ? ux_mix(T_THEME, T_INK, 20) : T_RAISE, cpl = sel ? T_INK : mute ? T_MID : T_TEXT;   /* playing */
    char b[16];
    pg = song.playing ? t->seq_idx / 16u : sel ? ui.cursor / 16u : 0u;
    if (pg >= np)
        pg = np - 1u;
    p0 = pg * 16u;
    ph = song.playing && t->seq_idx / 16u == pg ? t->seq_idx % 16u : 0xFFu;
    trk_short_name(c, b);
    m = mute ? 0 : meter_px(t->peak);
    t->peak = 0;
    if (m < ts.meter[c] - 1)
        m = ts.meter[c] - 1;                         /* (falls as the MIXER's) */
    sig = str_hash(1u + sel + mute * 2u + large * 4u + t->eng_req * 8u, b) + steps_hash(t) * 31u + pg * 7919u +
          ph * 104729u + len * 1299709u + lit * 15485863u;   /* (1.5.5: MIDI IN lights the cushion) */
    if (!ui.force && sig == ts.col[c]) {
        if (m != ts.meter[c]) {                      /* the meter alone: its slot */
            ts.meter[c] = (uint8_t)m;
            cv_begin(4, TR_MH, bg);
            tr_meter(0, 0, c, cs, sel ? T_INK : T_MID, bg);
            cv_blit(3 + TR_MX, (uint32_t)(y + TR_MY));
        }
        return;
    }
    ts.col[c] = sig;
    ts.meter[c] = (uint8_t)m;
    cv_begin(TR_W, TR_H, T_BG);
    cv_rrect(0, 0, TR_W, TR_H, 5, bg, T_BG);
    cv_icon_mid(4, TR_H / 2, 16, trk_icon(c, 1), lit ? (sel ? half : T_ACCENT) : sel ? T_INK : mute ? T_DIM : T_MID, bg);
    if (mute) {                                      /* MUTE: its badge at the name line's end */
        xr -= kc_w(KC_MUTE);
        GFX_HOOK_ALIGN(0, TR_NY(large), 0, TR_NY(large) + ch, AL_V, "tracks row badge on the name's line");
        cv_keycap(xr, TR_NY(large) + (ch - KC_H) / 2, KC_MUTE, sel ? T_INK : T_KEY, sel ? T_THEME : T_INK, bg);
        xr -= 6;
    }
    x = 24;
    if (FELUCCA_ICONS && !large) {                   /* the engine's icon on the name's capitals */
        uint32_t ei = engine_icon(ENGINES[t->eng_req % NENGINES]->name);
        if (ei != ICON_NONE)
            GFX_HOOK_ALIGN(0, TR_NY(large), 0, TR_NY(large) + ch, AL_V, "tracks row icon on the name's line");
        x += cv_icon_in(x, TR_NY(large), 0, ch, 12, ei, sel ? T_INK : T_MID, bg) + 5;
    }
    cv_free_text(x, TR_NY(large) - (large ? AF_M_CAP_Y : AF_S_CAP_Y), large ? &AF_M : &AF_S, b, sel ? T_INK : mute ? T_DIM : T_TEXT,
                 bg, xr - x);
    for (i = p0; i-- > 0u;)                          /* a TIE at the page's start: carried from a note before it? */
        if (s[i].time != ST_TIE) {
            carry = s[i].time == ST_NOTE && s[i].n;
            break;
        }
    for (i = 0; i < 16u && p0 + i < len; i++) {
        const step_t *st = &s[p0 + i];
        int32_t bx = tr_bx(i);
        uint16_t under = i == ph ? cp : bg;
        if (i == ph)                                 /* the step playing: a raised cell */
            cv_rrect(bx - 1, 15, TR_BW + 2, 13, 2, cp, bg);
        if (st->time == ST_TIE && carry) {           /* carried on: joined to the bar before */
            int32_t x0 = i ? tr_bx(i - 1u) + TR_BW : bx - 2;
            cv_rect(x0, TR_SB - 7, bx + 2 - x0, 7, i == ph ? cpl : cn);
            cv_rrect(bx, TR_SB - 7, TR_BW, 7, 2, i == ph ? cpl : cn, under);
        } else if (st->time == ST_NOTE && st->n) {   /* notes: a bar, accented taller */
            int acc = (st->flags & SF_ACCENT) != 0u;
            cv_rrect(bx, TR_SB - (acc ? 10 : 7), TR_BW, acc ? 10 : 7, 2, i == ph ? cpl : acc ? ca : cn, under);
            carry = 1;
            continue;
        } else if (st->time == ST_NOTE && st->hit) { /* DRUM hits: a pad, two for two hits or more */
            uint16_t pc = i == ph ? cpl : (st->acc & st->hit) ? ca : cn;
            cv_rrect(bx, TR_SB - 4, TR_BW, 4, 1, pc, under);
            if (st->hit & (st->hit - 1u))
                cv_rrect(bx, TR_SB - 9, TR_BW, 4, 1, pc, under);
        } else {                                     /* empty, a REST, a TIE carrying nothing */
            cv_rect(bx, TR_SB - 2, TR_BW, 2, i == ph ? (sel ? T_INK : T_MID) : st->time == ST_TIE ? (sel ? T_INK : T_MID) : cs);
        }
        carry = st->time == ST_TIE && carry;
    }
    if (np > 1u)                                     /* LEN > 16: the pages, top to bottom */
        for (i = 0; i < np; i++)
            cv_rect(TR_PX, TR_SB - 3 * (int32_t)(np - i) + 1, 3, 2, i == pg ? (sel ? T_INK : T_THEME) : cs);
    tr_meter(TR_MX, TR_MY, c, cs, sel ? T_INK : T_MID, bg);
    cv_blit(3, (uint32_t)y);
}
static void draw_home_tracks(void)
{
    uint32_t c;
    if (ui.force) {                                  /* the BG round the rows only, they cover the rest (#181) */
        lcd_fill(0, Y_GRAPH, 3, H_GRAPH, T_BG);
        lcd_fill(3 + TR_W, Y_GRAPH, 240 - 3 - TR_W, H_GRAPH, T_BG);
        for (c = 0; c < NTRK; c++)
            ts.meter[c] = 0;
        for (c = 1; c < NTRK; c++) {                 /* the 2 rows between two rows; LINE: a rule on the first */
            uint32_t y = (uint32_t)(Y_GRAPH + (int32_t)c * TR_PITCH - 2);
            if (ux.style == ST_LINE) {
                lcd_fill(3, y, TR_W, 1, T_RULE);
                GFX_HOOK_RULE(3, y, TR_W, 1);
            }
            lcd_fill(3, ux.style == ST_LINE ? y + 1u : y, TR_W, ux.style == ST_LINE ? 1u : 2u, T_BG);
        }
        {   /* MRC: the panel is 140 px (upstream 122): the rows end at 198, BG under them to its end */
            uint32_t y = (uint32_t)(Y_GRAPH + (NTRK - 1) * TR_PITCH + TR_H);
            lcd_fill(3, y, TR_W, (uint32_t)(Y_GRAPH + H_GRAPH) - y, T_BG);
        }
    }
    for (c = 0; c < NTRK; c++)
        home_track_row(c);
}
/* oscilloscope of the output, triggered on a rising zero crossing: a RAISE centre line, the trace cv_stroke's (in silence
 * a flat line on it; MENU > SCREEN OFF keeps it from staying on the panel for hours) */
static void graph_scope(uint16_t c)
{
    static int16_t snap[SCOPE_N] __attribute__((section(".pool")));
    uint32_t w = scope_w, i, trig = 0;
    int32_t cy = (int32_t)cv_h / 2, py = cy, x, peak = 1500, a = cv_h == H_GRAPH ? 46 : cy - 6;   /* (LARGE: the strip) */
    for (i = 0; i < SCOPE_N; i++) {
        snap[i] = scope_buf[(w + i) & (SCOPE_N - 1u)];
        if (snap[i] > peak)
            peak = snap[i];
        else if (-snap[i] > peak)
            peak = -snap[i];
    }
    for (i = 1; i < SCOPE_N - 240u; i++)
        if (snap[i - 1] < 0 && snap[i] >= 0) {
            trig = i;
            break;
        }
    cv_rect(PANEL_X0, cy, PANEL_W, 1, T_RAISE);
    for (x = 0; x < PANEL_W; x++) {
        int32_t y = LN(cy) - snap[trig + (uint32_t)x] * LN(a) / peak;    /* auto-scaled */
        if (x)
            cv_stroke(LN(PANEL_X0 - 1 + x), py, LN(PANEL_X0 + x), y, c);
        py = y;
    }
}

/* SONG is a playing order of saved patterns (MRC: pattern.c, numbered 1..24 as P01..P24; upstream the four PROJECT
 * slots, lettered A..D). It plays their steps with the current sounds. */
/* SONG as a timeline (song_layout), four tall lanes (T1 at the top, each led by its track's cushion, the selected
 * lane's the accent's) over a thin bar ruler: in a lane, the sections that play the same pattern one after the other
 * make one block (RAISE, the pattern's number centred in it; a pattern not saved only outlined, DIM). A
 * silent section ("-") is an empty lane with a faint line. The sections' boundaries are faint lines across the lanes
 * (seen between the blocks). The cell selected (KNOB 1 the section, KNOB 2 the lane) is THEME, its bars and letter
 * INK (a pattern not saved: outlined THEME 2 px; "-": a THEME outline in the gap); + ADD's slot dashed after the last
 * section. Playing: the playhead across the lanes down to the ruler (ACCENT; INK on THEME), the playing section's
 * blocks filled up to it (RAISE -> ACCENT). The ruler: a tick a bar, longer every 4 with the bar's number. Under it
 * one line (M: the selected cell's detail): the selected cell's pattern name (NOT SAVED, SILENT; on + ADD the hint, S as
 * every key hint) and, right, the song's length in bars where it fits. MENU > LARGE: the letters in M too (the ruler's
 * numbers stay S) */
#define SG_PAD 3                                       /* a block's inner margin: the letter's, the picture's */
#define SG_PH (SG_LH - 2 * SG_PAD)                     /* the picture's height (14 px: 3 levels) */
#define SG_TX 5                                        /* the lanes' cushions (16 px) */
#define SG_IY 119                                      /* the line under the ruler: its capitals end on row 118 */
static void sg_dash(int32_t bx, int32_t bw, int32_t lo, int32_t hi, uint16_t c)   /* + ADD: dashed, clipped lo .. hi */
{
    int32_t k;
    for (k = 0; k < bw; k++)
        if (!((k >> 1) & 1) && bx + k >= lo && bx + k < hi) {
            cv_rect(bx + k, SG_Y, 1, 1, c);
            cv_rect(bx + k, SG_Y + SG_H - 1, 1, 1, c);
        }
    for (k = 0; k < SG_H; k++)
        if (!((k >> 1) & 1)) {
            if (bx >= lo) cv_rect(bx, SG_Y + k, 1, 1, c);
            if (bx + bw <= hi) cv_rect(bx + bw - 1, SG_Y + k, 1, 1, c);
        }
}
/* an outline of w x h at x, y, n px thick, clipped to lo .. hi (its ends only where not clipped) */
static void sg_frame(int32_t bx, int32_t bw, int32_t y, int32_t h, uint32_t n, int32_t lo, int32_t hi, uint16_t c)
{
    int32_t x0 = bx > lo ? bx : lo, x1 = bx + bw < hi ? bx + bw : hi;
    uint32_t k;
    if (x1 <= x0)
        return;
    for (k = 0; k < n; k++) {
        cv_rect(x0, y + (int32_t)k, x1 - x0, 1, c);
        cv_rect(x0, y + h - 1 - (int32_t)k, x1 - x0, 1, c);
        if (x0 == bx) cv_rect(bx + (int32_t)k, y, 1, h, c);
        if (x1 == bx + bw) cv_rect(bx + bw - 1 - (int32_t)k, y, 1, h, c);
    }
}
/* one lane's drawing state: the screen spans of the cell selected and of the playing section's part played */
typedef struct { int32_t c0, c1, p0, p1; uint16_t pf; } sg_fill_t;
static uint16_t sg_fill_at(const sg_fill_t *F, int32_t x)   /* the block's fill under column x */
{
    return x >= F->c0 && x < F->c1 ? T_THEME : x >= F->p0 && x < F->p1 ? F->pf : T_RAISE;
}
/* the pattern's number centred in xa .. xb and in the lane (MENU > LARGE: M); 0: no room */
static int32_t sg_letter(int32_t xa, int32_t xb, int32_t y, uint32_t s, uint16_t ink, uint16_t bg)
{
    const aafont_t *f = (ui_prefs & PREF_LARGE) ? &AF_M : &AF_S;
    char a[4];
    int32_t w, ch = f == &AF_M ? AF_M_CAP_H : AF_S_CAP_H, cy = f == &AF_M ? AF_M_CAP_Y : AF_S_CAP_Y;
    fmt_int(a, (int32_t)s + 1);
    w = text_w(f, a);
    if (w + 2 > xb - xa)
        return 0;
    cv_text_on(xa + (xb - xa - w) / 2, y + (SG_LH - ch) / 2 - cy, f, a, ink, bg);
    return w;
}
/* lane k's block of sections i .. j - 1 on pattern s; c: the cursor's section when it is in this lane, else past it;
 * sp: the playhead on screen (-100: none) */
static void sg_block(const song_lay_t *L, int32_t off, uint32_t k, uint32_t i, uint32_t j, uint32_t s, uint32_t c,
                     int32_t sp)
{
    int32_t hi = SG_X0 + SG_W, y = SG_LY(k), bx = SG_X0 + L->x[i] - off, bw = L->x[j] - L->x[i] - SG_GAP;
    int32_t x0 = bx > SG_X0 ? bx : SG_X0, x1 = bx + bw < hi ? bx + bw : hi;
    int saved = pat_used(s), cur = c >= i && c < j;
    sg_fill_t F = {0, 0, 0, 0, 0};
    uint32_t p = chain.running && chain.row >= i && chain.row < j ? chain.row : 0xFFu;
    if (x1 <= x0)
        return;
    if (!saved) {                                      /* not saved: an outline, the number DIM */
        if (cur) {
            int32_t cx = SG_X0 + L->x[c] - off, cw = L->x[c + 1u] - L->x[c] - SG_GAP;
            sg_frame(bx, bw, y, SG_LH, 1, SG_X0, hi, T_DIM);
            sg_frame(cx, cw, y, SG_LH, 2, SG_X0, hi, T_THEME);
            sg_letter(cx > SG_X0 ? cx : SG_X0, cx + cw < hi ? cx + cw : hi, y, s, T_THEME, T_SURF);
            return;
        }
        sg_frame(bx, bw, y, SG_LH, 1, SG_X0, hi, T_DIM);
        sg_letter(x0, x1, y, s, T_DIM, T_SURF);
        return;
    }
    cv_rrect(x0, y, x1 - x0, SG_LH, x0 != bx || x1 != bx + bw ? 0 : 2, T_RAISE, T_SURF);
    if (p != 0xFFu && sp >= SG_X0) {                   /* playing here: the part played */
        int32_t a = SG_X0 + L->x[p] - off;
        F.pf = ux_mix(T_RAISE, T_ACCENT, 35);
        F.p0 = a > x0 ? a : x0;
        F.p1 = sp + 2 < x1 ? sp + 2 : x1;              /* (its right corners under the playhead) */
        if (F.p1 > F.p0) {                             /* (no cell of its own: the letter may cross its end) */
            int32_t e = F.p0 == bx && ux.style != ST_LINE && F.p1 - F.p0 > 2 ? 2 : 0;   /* the block's rounded end */
            cv_rect(F.p0 + e, y, F.p1 - F.p0 - e, SG_LH, F.pf);
            if (e) {
                cv_rect(F.p0, y + 2, 1, SG_LH - 4, F.pf);
                cv_rect(F.p0 + 1, y + 1, 1, SG_LH - 2, F.pf);
            }
        }
    }
    if (cur) {                                         /* the cell selected */
        int32_t cx = SG_X0 + L->x[c] - off, cw = L->x[c + 1u] - L->x[c] - SG_GAP;
        F.c0 = cx > SG_X0 ? cx : SG_X0;
        F.c1 = cx + cw < hi ? cx + cw : hi;
        if (F.c1 > F.c0)
            cv_rrect(F.c0, y, F.c1 - F.c0, SG_LH, j - i == 1u && F.c0 == cx && F.c1 == cx + cw ? 2 : 0, T_THEME, T_SURF);
    }
    {                                                  /* the number centred on the block's visible part; INK where the cell selected is */
        int32_t m = (x0 + x1) / 2;
        if (F.c1 > F.c0 && m >= F.c0 && m < F.c1)
            sg_letter(F.c0, F.c1, y, s, T_INK, T_THEME);
        else
            sg_letter(x0, x1, y, s, T_TEXT, sg_fill_at(&F, m));
    }
}
static void graph_song(void)
{
    song_lay_t L;
    const aafont_t *fl = &AF_M;                       /* the line: the selected cell's detail and the length, M */
    int32_t off, px, sp, hi = SG_X0 + SG_W, bx, x0, ly = SG_IY - (AF_M_CAP_Y + AF_M_CAP_H);
    uint32_t i, j, k, c = song_cur(), lane = ui.song_trk % NTRK, total, bars;
    char b[24], a[12];
    song_layout(&L);
    off = song_follow(&L);
    px = song_playx(&L);
    sp = px >= 0 ? SG_X0 + px - off : -100;
    total = L.u0[L.n];
    bars = (total + SG_BARU - 1u) / SG_BARU;
    for (k = 0; k < NTRK; k++)                        /* the lanes' cushions */
        cv_trk(SG_TX, SG_LY(k) + (SG_LH - 16) / 2, 16, k, k == lane ? T_ACCENT : T_MID, T_SURF);
    for (i = 1; i < L.n; i++) {                       /* the sections' boundaries, faint, behind the blocks */
        int32_t x = SG_X0 + L.x[i] - off - 1;
        if (x >= SG_X0 && x < hi)
            cv_rect(x, SG_Y, 1, SG_H, T_LINE);
    }
    for (k = 0; k < NTRK; k++)                        /* the lanes: a block per run of one pattern, "-" a faint line */
        for (i = 0; i < L.n; i = j) {
            uint32_t s = chain_config.row[i].slot[k];
            for (j = i + 1u; j < L.n && chain_config.row[j].slot[k] == s; j++)
                ;
            if (s < NPAT) {
                sg_block(&L, off, k, i, j, s, k == lane ? c : CHAIN_ROWS, sp);
            } else {
                int32_t sx = SG_X0 + L.x[i] - off, ex = SG_X0 + L.x[j] - off - SG_GAP;
                sx = sx > SG_X0 ? sx : SG_X0;
                ex = ex < hi ? ex : hi;
                if (ex > sx)
                    cv_rect(sx, SG_LY(k) + SG_LH / 2, ex - sx, 1, T_LINE);
                if (k == lane && c >= i && c < j)      /* the cursor on a silent cell: its outline */
                    sg_frame(SG_X0 + L.x[c] - off, L.x[c + 1u] - L.x[c] - SG_GAP, SG_LY(k), SG_LH, 1, SG_X0, hi, T_THEME);
            }
        }
    if (L.add) {                                       /* + ADD */
        bx = SG_X0 + L.x[L.n] - off;
        x0 = bx > SG_X0 ? bx : SG_X0;
        if ((bx + SG_ADDW < hi ? bx + SG_ADDW : hi) > x0) {
            int32_t x1 = bx + SG_ADDW < hi ? bx + SG_ADDW : hi;
            sg_dash(bx, SG_ADDW, SG_X0, hi, c == L.n ? T_THEME : T_DIM);
            if (text_w(fl, "+") <= x1 - x0 - 4) {
                GFX_HOOK_ALIGN(x0, SG_Y, x1, SG_Y + SG_H, AL_HV, "song + ADD centred");
                if (fl == &AF_M) cv_text_in(x0, SG_Y + CAP_IN(M, SG_H), x1 - x0, &AF_M, "+", c == L.n ? T_THEME : T_DIM, T_SURF);
                else cv_text_in(x0, SG_Y + CAP_IN(S, SG_H), x1 - x0, &AF_S, "+", c == L.n ? T_THEME : T_DIM, T_SURF);
            }
        }
    }
    if (L.n) {                                         /* the ruler: a tick a bar, every 4th longer with its number */
        int32_t r0 = SG_X0 + L.x[0] - off, r1 = SG_X0 + L.x[L.n] - off - SG_GAP, last = -100;
        int32_t lx[24];                                /* the numbers, drawn after the ticks */
        uint32_t bar, lb[24], nl = 0;
        r0 = r0 > SG_X0 ? r0 : SG_X0;
        r1 = r1 < hi ? r1 : hi;
        if (r1 > r0) cv_rect(r0, SG_RY, r1 - r0, 1, T_LINE);
        if (sp >= SG_X0 && sp < hi - 1) {              /* the playhead, across the lanes (INK on the cell selected) */
            for (k = 0; k < NTRK; k++) {
                uint32_t s = c < L.n ? chain_config.row[c].slot[k] : CHAIN_SILENT;
                int on = k == lane && pat_used(s) && sp >= SG_X0 + L.x[c] - off &&
                         sp + 2 <= SG_X0 + L.x[c + 1u] - off - SG_GAP;
                cv_rect(sp, SG_LY(k), 2, SG_LH + (k + 1u < NTRK ? SG_LG : 0), on ? T_INK : T_ACCENT);
            }
            cv_rect(sp, SG_Y + SG_H, 2, SG_RY - SG_Y - SG_H + 6, T_ACCENT);
        }
        for (i = 0; i < L.n; i++) {
            uint32_t u1 = L.u0[i + 1u], w = (uint32_t)(L.x[i + 1u] - L.x[i]);
            bx = SG_X0 + L.x[i] - off;
            if (bx >= hi || bx + (int32_t)w < SG_X0)
                continue;
            bar = (L.u0[i] + SG_BARU - 1u) / SG_BARU;  /* (from the view's left edge) */
            if (bx < SG_X0) {
                uint32_t uv = L.u0[i] + sg_muldiv((uint32_t)(SG_X0 - bx), u1 - L.u0[i], w);
                bar = uv / SG_BARU > bar ? uv / SG_BARU : bar;
            }
            for (; bar * SG_BARU < u1 || (i + 1u == L.n && bar * SG_BARU == u1); bar++) {
                int32_t x = SG_X0 + song_ux(&L, bar * SG_BARU) - off, lg = !(bar % 4u);
                if (i + 1u == L.n && bar * SG_BARU == u1) x = r1 - 1;   /* (the song's end: its last section's edge) */
                if (x > hi - 1)
                    break;
                if (x < SG_X0)
                    continue;
                cv_rect(x, SG_RY + 1, 1, lg ? 5 : 2, lg ? T_MID : T_DIM);
                if (lg && bar * SG_BARU < total && nl < 24u) {
                    lx[nl] = x;
                    lb[nl++] = bar + 1u;
                }
            }
        }
        for (k = 0; k < nl; k++) {                     /* each number right of its tick, where it has room */
            int32_t w;
            fmt_int(a, (int32_t)lb[k]);
            w = text_w(&AF_S, a);
            if (lx[k] + 3 > last + 4 && lx[k] + 3 + w <= hi && (sp + 2 <= lx[k] + 2 || sp > lx[k] + 4 + w))
                last = cv_text_on(lx[k] + 3, SG_RY + 4 - AF_S_CAP_Y, &AF_S, a, T_DIM, T_SURF);
        }
    }
    /* one line: the song's length right (where it fits), the selected cell's pattern left */
    x0 = hi;
    if (L.n) {
        fmt_int(b, (int32_t)bars);
        str_cpy(b + str_len(b), bars == 1u ? " BAR" : " BARS", 6);
        x0 = hi - text_w(fl, b);
        cv_text_on(x0, ly, fl, b, T_MID, T_SURF);
    }
    if (c < L.n) {
        uint32_t s = chain_config.row[c].slot[lane];
        if (s >= NPAT)
            cv_free_text(SG_X0, ly, fl, "SILENT", T_DIM, T_SURF, x0 - 10 - SG_X0);
        else if (!pat_used(s))
            cv_free_text(SG_X0, ly, fl, "NOT SAVED", T_DIM, T_SURF, x0 - 10 - SG_X0);
        else
            cv_free_text(SG_X0, ly, fl, pat_info(s)->name, T_MID, T_SURF, x0 - 10 - SG_X0);
    } else {                                           /* + ADD: how */
        for (i = 0; i < NPAT && !pat_used(i); i++)
            ;
        cv_free_hint(SG_X0, ly + AF_M_CAP_Y + AF_M_CAP_H - AF_S_CAP_Y - AF_S_CAP_H,   /* (a key hint: S) */
                     i < NPAT ? "[K3] ADD SECTION" : "[SAVE] PATTERN FIRST", T_MID, T_SURF, x0 - 10 - SG_X0);
    }
}
static void draw_graph(void)
{
    const page_t *pg = cur_page();
    const track_t *t = TSEL;
    uint16_t c = ACC;
    uint32_t sig;
    if (!ui.home && pg->graph == GR_TRK) {
        draw_tracks();
        return;
    }
    if (ui.home && home_tracks()) {                  /* MENU > HOME TRACKS (1.3) */
        draw_home_tracks();
        return;
    }
    if (!ui.home && pg->scope == SC_ENGINE && ENGINES[t->eng_req % NENGINES] == &ENG_SAMPLE) sample_wave_tick(t);
    sig = graph_signature();
    if (!ui.force && sig == ui.graph_sig)
        return;
    ui.graph_sig = sig;
    cv_begin(240, graph_h(), T_BG);
    cv_rrect(3, 0, 234, graph_h(), 5, T_SURF, T_BG);   /* the panel (MENU > LARGE: the strip) */
    cv_bg = T_SURF;                                  /* (text drawn with cv_text lands on it) */
    cv_oy = GOY;                                     /* graphs on a 100 px scale */
    if (strip_kind() != SK_NONE) {                   /* MENU > LARGE: the strip */
        uint32_t k = strip_kind();
        cv_oy = k == SK_ADSR || k == SK_LFO ? 3 : 0;
        graph_ht = LG_H_GRAPH - 6;
        if (k == SK_SCOPE) graph_scope(c);
        else if (k == SK_ADSR) graph_adsr(t, c);
        else if (k == SK_LFO) graph_lfo(t, c);
        else graph_title();
        graph_ht = 100;
    } else if (ui.home) {
        cv_oy = 0;
        graph_scope(c);
    } else {
        switch (pg->graph) {
        case GR_ADSR:
            graph_adsr(t, c);
            break;
        case GR_LFO:
            graph_lfo(t, c);
            break;
        case GR_ROLL:
            cv_oy = 0;
            if (detail_on())                          /* 1.5.1 (#199) */
                graph_detail(t, c);
            else if (drum_track(t))
                graph_grid(t, c);
            else
                graph_roll(t, c);
            break;
        case GR_SCALE:
            cv_oy = 0;                                /* (the keybed takes the panel's height, not the 100 scale) */
            graph_scale(t, c);
            break;
        case GR_CHORD:
            cv_oy = 0;
            graph_chord(t, c);
            break;
        case GR_FX:
            cv_oy = 0;                                /* (the bars take the panel's height, not the 100 scale) */
            graph_fx(t, c);
            break;
        case GR_LANES:                               /* (the kit as EDIT's, its pieces at their levels) */
            cv_oy = 0;
            graph_drum(t, c);
            break;
        case GR_CHO:
            cv_oy = 0;                                /* (the circles take the panel's height, not the 100 scale) */
            graph_chorus(c);
            break;
        case GR_DLY:
            cv_oy = 0;                                /* (the circles take the panel's height, not the 100 scale) */
            graph_delay(c);
            break;
        case GR_REV:
            cv_oy = 0;                                /* (the drawings take the panel's height, not the 100 scale) */
            graph_reverb(c);
            break;
        case GR_EDEST:
            cv_oy = 0;                                /* (the boxes take the panel's height, not the 100 scale) */
            graph_edest(t, c);
            break;
        case GR_CLOCK:
            cv_oy = 0;                                /* (the metronome takes the panel's height, not the 100 scale) */
            graph_clock(c);
            break;
        case GR_SLCR:
            cv_oy = 0;                                /* (the ring takes the panel's height, not the 100 scale) */
            graph_slicer(t, c);
            break;
        case GR_INS:
            cv_oy = 0;                                /* (the drawings take the panel's height, not the 100 scale) */
            graph_insert(t, c);
            break;
        case GR_VOICE:
            cv_oy = 0;                                /* (the arc takes the panel's height, not the 100 scale) */
            graph_voice(t, c);
            break;
        case GR_MOD:
            cv_oy = 0;
            graph_mod(t, c);
            break;
        case GR_BROWSE:
            cv_oy = 0;
            graph_browse();
            break;
        case GR_PSAVE:
            cv_oy = 0;
            graph_psave();
            break;
        case GR_USER:
            cv_oy = 0;
            graph_user();
            break;
        case GR_SONG:
            cv_oy = 0;
            graph_song();
            break;
        case GR_PATS:
            cv_oy = 0;
            graph_pats();
            break;
        case GR_TOOLS:
            cv_oy = 0;
            panel_note("TURN TO PICK", "[OCT+] CONFIRM", 0);
            break;
#if FELUCCA_SLICE
        case GR_SLICES:
            cv_oy = 0;
            graph_slices();
            break;
#endif
        case GR_FMOP:
        case GR_FMOP2:
            if (fop_ok()) graph_fm6(t, c, fop_op % 6u);   /* the chart, the operator picked */
            break;
        case GR_FMENV:
            if (fop_ok()) graph_fm6env(t, c);
            break;
        case GR_ARP:
            cv_oy = 0;                                /* (the ladder takes the panel's height, not the 100 scale) */
            graph_arp(t, c);
            break;
        default:
            if (pg->scope == SC_ENGINE && ed_still(t)) {   /* (the drawings take the panel's height, not the 100 scale) */
                const engine_t *e = ENGINES[t->eng_req % NENGINES];
                cv_oy = 0;
                if (e == &ENG_ANALOG) graph_analog(t, c);
                else if (e == &ENG_LOFI) graph_lofi(t, c);
                else graph_wheel(t, c);
            }
            else if (pg->scope == SC_ENGINE && ENGINES[t->eng_req % NENGINES] == &ENG_TRIO) {   /* (it moves: no ed_still) */
                cv_oy = 0;
                graph_trio(t, c);
            }
            else if (pg->scope == SC_ENGINE && ENGINES[t->eng_req % NENGINES] == &ENG_GRAIN) {   /* (it moves: no ed_still) */
                cv_oy = 0;
                graph_grain(t, c);
            }
            else if (pg->scope == SC_ENGINE && ENGINES[t->eng_req % NENGINES] == &ENG_PHASE) {   /* (it moves too) */
                cv_oy = 0;
                graph_phase(t, c);
            }
            else if (pg->scope == SC_ENGINE && ENGINES[t->eng_req % NENGINES] == &ENG_FORMANT) {   /* (it moves too) */
                cv_oy = 0;
                graph_formant(t, c);
            }
            else if (pg->scope == SC_ENGINE && ENGINES[t->eng_req % NENGINES] == &ENG_NOISE) {   /* (it moves too) */
                cv_oy = 0;
                graph_noise(t, c);
            }
            else if (pg->scope == SC_ENGINE && ENGINES[t->eng_req % NENGINES] == &ENG_DRUM) {   /* (it moves too) */
                cv_oy = 0;
                graph_drum(t, c);
            }
            else if (pg->scope == SC_ENGINE && ENGINES[t->eng_req % NENGINES] == &ENG_PHYS) {   /* (it moves too) */
                cv_oy = 0;
                graph_phys(t, c);
            }
            else if (pg->scope == SC_ENGINE && ENGINES[t->eng_req % NENGINES] == &ENG_SAMPLE && sample_wave.ready) graph_sample(c);
            else if (pg->scope == SC_ENGINE && t->eng_req % NENGINES == ENGI_FM6) graph_fm6(t, c, 6u);   /* EDIT 1 and 2 */
#if FELUCCA_FM4
            else if ((pg->scope == SC_ENGINE || pg->id[0] == P_FM1_LEVEL) && t->eng_req % NENGINES == ENGI_DIGITAL)
                graph_fm(t, c);                      /* (OP LEVEL too: the levels on the chart) */
#endif
            else { cv_oy = 0; graph_scope(c); }
            break;
        }
    }
    cv_oy = 0;
    cv_blit(0, graph_y());
}
