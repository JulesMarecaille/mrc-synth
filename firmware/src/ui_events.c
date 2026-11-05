/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Felucca step fields (1.2, Discussion #153; #162; 1.5, #173; included by ui.c): a step's CHANCE, RATCH, NUDGE and VEL as
 * the sequencer and SEQ > DETAIL (sd_turn, graph_detail) share them: their values, defaults and text. (MRC: the
 * SEQ > AUTOMATION list page that edited them as rows is gone; the motion engine, motion.c, stays.) */
enum { EVK_CHANCE = 2, EVK_RATCH, EVK_NUDGE = 5, EVK_VEL = 7 };   /* (a step field's kind) */

/* a step's CHANCE (kind EVK_CHANCE), RATCH (EVK_RATCH), NUDGE (EVK_NUDGE) or VEL (EVK_VEL): its value, its default
 * (100 %, x1, 0, 96), at it, set (VEL 96 is stored as 0, as a step that never had one) */
static int32_t ev_sval(const step_t *s, uint32_t k)
{
    return k == EVK_CHANCE ? (int32_t)step_chance_v(s) : k == EVK_RATCH ? (int32_t)step_ratchet(s) :
           k == EVK_VEL ? (s->vel ? s->vel : 96) : step_nudge(s);
}
static int32_t ev_sdefv(uint32_t k) { return k == EVK_CHANCE ? 100 : k == EVK_RATCH ? 1 : k == EVK_VEL ? 96 : 0; }
static int ev_sdef(const step_t *s, uint32_t k) { return ev_sval(s, k) == ev_sdefv(k); }
static void ev_sput(step_t *s, uint32_t k, int32_t v)
{
    if (k == EVK_CHANCE)
        step_set_chance_v(s, (uint32_t)v);
    else if (k == EVK_RATCH)
        step_set_ratchet(s, (uint32_t)v);
    else if (k == EVK_VEL)
        s->vel = (uint8_t)(v == 96 ? 0 : v);
    else
        step_set_nudge(s, v);
}
/* 1.5.5 (#209): CHANCE's value turned on (s detents, d of them accelerated): 0..100 % accelerated, then past 100 the
 * conditions one a detent (core.h step_chance_v: 101..); a fast turn stops at 100 on the way in and out */
static const char COND_NAME[NCOND][6] = {"1:2", "2:2", "1:3", "2:3", "3:3", "1:4", "2:4", "3:4", "4:4", "FILL", "!FILL"};
static int32_t ev_chance_turn(int32_t v, int32_t s, int32_t d)
{
    int32_t to = v > 100 || (v == 100 && s > 0) ? v + s : v + d;
    if ((v < 100 && to > 100) || (v > 100 && to < 100))
        to = 100;
    return clamp(to, 0, (int32_t)CHANCE_V_MAX);
}
/* CHANCE's value as text: "50" (unit "%"), a condition's name ("1:4", "FILL"; unit ""); b holds 8 */
static const char *ev_chance_fmt(char *b, int32_t v)
{
    if (v > 100) {
        str_cpy(b, COND_NAME[(uint32_t)(v - 101) % NCOND], 8);
        return "";
    }
    fmt_int(b, v);
    return "%";
}
/* a VEL row (shown): a note step; it plays: notes, or a hit not accented, and not an ACC step (127) */
static int ev_vel_row(const step_t *s) { return s->time == ST_NOTE && (s->n || s->hit); }
static int ev_vel_plays(const step_t *s)
{
    return ev_vel_row(s) && (s->n || (s->hit & ~s->acc)) && !(s->flags & SF_ACCENT);
}
/* a NUDGE that plays: the track's QUANTIZE OFF, a note step, not ratcheted (seq.c seq_tick) */
static int ev_nudge_plays(const track_t *t, const step_t *s)
{
    return !t->p[P_SQNT] && s->time == ST_NOTE && (s->n || s->hit) && step_ratchet(s) == 1u;
}
/* "+3" "-8" "0" (b holds 4) */
static void ev_nudge_fmt(char *b, int32_t v)
{
    fmt_int(b + (v > 0), v);
    if (v > 0)
        b[0] = '+';
}
/* SEQ > DETAIL (1.5.1, #199; ui_input.c sd_turn, ui_draw.c, ui_graph.c graph_detail): KNOB 1..4 are these fields of
 * the cursor step, (one storage with the sequencer's). A step takes them where they play: a NOTE step (notes or
 * hits); CHANCE a TIE too (a failed roll cuts the held note). A REST or an empty step none (nothing to hear) */
static const uint8_t SD_KIND[4] = {EVK_CHANCE, EVK_RATCH, EVK_NUDGE, EVK_VEL};
static int sd_takes(const step_t *s, uint32_t k) { return step_on(s) || (k == EVK_CHANCE && s->time == ST_TIE); }
