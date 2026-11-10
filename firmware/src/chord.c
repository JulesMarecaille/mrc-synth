/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Chord keys (SCL > CHORD: P_CHRD, P_VOIC): one key, MIDI note or arp input plays a chord. Included by seq.c
 * after kb_map; runs in the audio ISR with the rest of the note input.
 *   CHRD OFF; DIA3 / DIA7: the triad / seventh of the track's scale (ROOT, SCALE) built on the key, every tone
 *        in key (a key outside the scale takes the scale note below it, as QNT SNAP does; CHR builds on the
 *        major scale of ROOT); MAJ MIN DOM7 MAJ7 MIN7 SUS4 POW: that shape on the key, whatever the scale.
 *   VOIC CLOSE (root position); OPEN (the second tone an octave up: 1-5-3); INV1 / INV2 (the lowest tone, then
 *        the next, an octave up); +OCT (the root an octave down added; a seventh drops its fifth); MRC: LEAD (the
 *        inversion and octave nearest the last chord the track played: chord_lead). At most
 *        CHORD_MAX notes, notes outside 0..127 left out.
 * The key is mapped first (kb_map: QNT SNAP / WHITE, TRN, the octave), the chord is built on the note it gives.
 * MONO / LEGATO / UNISON parts play the chord's root only; a kit (the DRUM engine, slices: an
 * engine that maps the keys itself) ignores CHRD. Every source keeps the notes it started (kb_chord for a
 * key, mchord for a MIDI note) and its release ends exactly those, so CHRD / VOIC changed while it is held
 * leave nothing hanging. A note several sources hold sounds once and ends with the last of them. */
enum { CH_OFF, CH_DIA3, CH_DIA7, CH_MAJ, CH_MIN, CH_DOM7, CH_MAJ7, CH_MIN7, CH_SUS4, CH_POW };
enum { VC_CLOSE, VC_OPEN, VC_INV1, VC_INV2, VC_BASS, VC_LEAD };
#define CHORD_MAX 4u
enum { CN_9 = 1u << 12, CN_11 = 1u << 13, CN_S11 = 1u << 14 };   /* MRC: chord_name's mask, a 9th / 11th above the octave */
#define MCHORD_N 24u                     /* MIDI notes held as chords at once (more: their root alone) */

static const int8_t CHORD_SHAPE[CH_POW - CH_MAJ + 1][CHORD_MAX] = {
    {0, 4, 7, -1}, {0, 3, 7, -1}, {0, 4, 7, 10}, {0, 4, 7, 11}, {0, 3, 7, 10}, {0, 5, 7, -1}, {0, 7, 12, -1},
};

static uint8_t kb_chord[27][CHORD_MAX], kb_chn[27];   /* the notes key k started (kb_chn 0: none) */
typedef struct { uint8_t id, ch, src, n, note[CHORD_MAX]; } mchord_t;   /* id: track + 1, 0 = free */
static mchord_t mchord[MCHORD_N];
/* the last chord played per track (the CHORD page shows it): its root, notes, and the shape's tones above the
 * root (bit i = i semitones, 0..11) */
static struct { uint8_t root, n, note[CHORD_MAX]; uint16_t mask; uint8_t gen; } chord_last[NTRK];
/* MRC, VOIC LEAD (after Moonphase): the last chord a track played as LEAD voiced it (vl_n 0: none yet) */
static uint8_t vl_prev[NTRK][CHORD_MAX], vl_n[NTRK];

/* 1 = the track's keys are a kit: no chords */
static int chord_kit(const track_t *t)
{
    const engine_t *e = ENGINES[eng_idx(t->eng_req)];
    return e->oneshot || (e->keys && e->keys(t, 0) >= 0);
}

/* MRC (after Moonphase's CHORD mode): the flavor keys. With CHRD on (and not a kit) the 7 leftmost keys, F3 .. B3, play
 * nothing and are lit (ui_input.c key_leds); held while a chord key plays, or pressed under it (seq.c chord_revoice),
 * they colour its chord. What a flavor adds stays in the chord's scale: DIA3 / DIA7 the track's (CHR: the major scale
 * of ROOT); the fixed shapes their own mode (MAJ MAJ7 SUS4 POW ionian, DOM7 mixolydian, MIN MIN7 dorian). Where the
 * scale's note would clash, the nearest one that does not:
 *   F3  7     the scale's 7th                       F#3  11    the scale's 11th (a #11 where the scale has it)
 *   G3  6     the scale's 6th (a b6: for the 5th)   G#3  5     power: root, 5th, octave (no 3rd)
 *   A3  ADD9  the scale's 9th (a b9: the 11th)      A#3  SUS2  the 2nd for the 3rd (a b2: the 4th)
 *   B3  SUS4  the 4th for the 3rd (an augmented 4th: the 2nd)
 * Held together they combine; a chord keeps CHORD_MAX notes at most: the plain 5th goes first, then the 9th. A shape
 * keeps what it has (DIA7 MAJ7 MIN7 DOM7 their 7th, SUS4 its 4th, POW no 3rd). Flavors colour the track the keys were
 * pressed on (flav_trk): its keys, its MIDI IN notes, step entry, the CHORD page */
enum { MF_7 = 1, MF_11 = 2, MF_6 = 4, MF_POW = 8, MF_ADD9 = 16, MF_SUS2 = 32, MF_SUS4 = 64 };
#define MF_KEYS 7u                       /* keys 0 .. MF_KEYS-1, F3 .. B3: flavor k is bit k */
static uint32_t kb_flav;                 /* the flavor keys held (bit k: key k) */
static uint8_t flav_trk;                 /* the track they were pressed on */

/* the track's keys 0 .. MF_KEYS-1 are flavor keys */
static int chord_flav_on(const track_t *t)
{
    return t->p[P_CHRD] > CH_OFF && t->p[P_CHRD] <= CH_POW && !chord_kit(t);
}

/* the flavors that colour track t's chords now (MF_*) */
static uint32_t chord_flavors(const track_t *t)
{
    return kb_flav && trk_index(t) == flav_trk && chord_flav_on(t) ? kb_flav : 0u;
}

/* the semitones of the deg-th note above the root of a scale (mask: pitch classes above the root, bit 0 set) */
static int32_t flav_up(uint32_t mask, uint32_t deg)
{
    int32_t m = 0, guard = 48;
    while (deg > 0 && guard--)
        if ((mask >> (uint32_t)(++m % 12)) & 1u)
            deg--;
    return m;
}

/* the chord on mode's scale with the flavors f -> iv (semitones above the root, unsorted, iv[0] = 0), its tones */
static uint32_t flav_tones(const track_t *t, uint32_t mode, int32_t root, uint32_t f, int32_t *iv)
{
    int32_t want[8], x2, x3, x4, x5, x;
    uint32_t mask, nw = 0, n = 0, i, j, plain5;
    if (mode >= CH_MAJ) {
        mask = mode == CH_DOM7 ? 0x6B5u : mode == CH_MIN || mode == CH_MIN7 ? 0x6ADu : 0xAB5u;
    } else {
        uint32_t s = scale_mask(t), r = (uint32_t)((root - t->p[P_ROOT] + 120) % 12);
        if (s == 0xFFFu)
            s = SCALE_MASK[1];                          /* (CHR: the major scale of ROOT, as chord_tones) */
        mask = ((s >> r) | (s << (12u - r))) & 0xFFFu;
    }
    if (mode == CH_DIA7 || mode == CH_DOM7 || mode == CH_MAJ7 || mode == CH_MIN7)
        f |= MF_7;
    if (mode == CH_SUS4)
        f |= MF_SUS4;
    if (mode == CH_POW)
        f |= MF_POW;
    x2 = flav_up(mask, 1u);
    x3 = flav_up(mask, 2u);
    x4 = flav_up(mask, 3u);
    x5 = flav_up(mask, 4u);
    plain5 = x5 == 7;
    want[nw++] = 0;                                     /* in the order they are kept */
    if (f & MF_POW)
        ;                                               /* (no 3rd) */
    else if ((f & MF_SUS2) && (f & MF_SUS4)) {
        if (x2 != 1)
            want[nw++] = x2;
        if (x4 != 6)
            want[nw++] = x4;
    } else if (f & MF_SUS4)
        want[nw++] = x4 != 6 ? x4 : x2 != 1 ? x2 : x3;
    else if (f & MF_SUS2)
        want[nw++] = x2 != 1 ? x2 : x4 != 6 ? x4 : x3;
    else
        want[nw++] = x3;
    if (!plain5)
        want[nw++] = x5;                                /* a diminished / augmented 5th makes the chord: kept */
    if (f & MF_7)
        want[nw++] = flav_up(mask, 6u);
    if (f & MF_6) {
        want[nw++] = x = flav_up(mask, 5u);
        if (x == 8)
            plain5 = 0;                                 /* a b6 a semitone over the 5th: instead of it */
    }
    if (f & MF_ADD9) {
        x = flav_up(mask, 8u);
        want[nw++] = x == 13 ? flav_up(mask, 10u) : x;
    }
    if (f & MF_11)
        want[nw++] = flav_up(mask, 10u);                /* (MRC: F#3, the scale's 11th) */
    if (plain5)
        want[nw++] = x5;                                /* (the first to go past CHORD_MAX: a 9th chord drops it) */
    if (f & MF_POW)
        want[nw++] = 12;
    for (i = 0; i < nw && n < CHORD_MAX; i++) {
        for (j = 0; j < n && iv[j] != want[i]; j++)
            ;
        if (j == n)
            iv[n++] = want[i];
    }
    return n;
}

/* the tones of the chord (semitones above *root, ascending, iv[0] = 0) -> their number; DIA may move *root
 * down onto the scale */
static uint32_t chord_tones(const track_t *t, uint32_t mode, int32_t *root, int32_t *iv)
{
    uint32_t n = 0, i;
    if (mode >= CH_MAJ) {
        for (i = 0; i < CHORD_MAX && CHORD_SHAPE[mode - CH_MAJ][i] >= 0; i++)
            iv[n++] = CHORD_SHAPE[mode - CH_MAJ][i];
        return n;
    }
    {
        uint32_t mask = scale_mask(t), deg[12], c = 0, at = 0, guard = 12;
        int32_t r = t->p[P_ROOT];
        if (mask == 0xFFFu)
            mask = SCALE_MASK[1];                       /* CHR: no key of its own, the major scale of ROOT */
        while (guard-- && !((mask >> (uint32_t)((*root - r + 120) % 12)) & 1u))
            (*root)--;                                  /* outside the scale: the scale note below */
        for (i = 0; i < 12u; i++)
            if ((mask >> i) & 1u) {
                if (i == (uint32_t)((*root - r + 120) % 12))
                    at = c;
                deg[c++] = i;
            }
        for (i = 0; i < (mode == CH_DIA7 ? 4u : 3u); i++) {   /* every other note of the scale */
            uint32_t k = at + 2u * i;
            iv[n++] = (int32_t)deg[k % c] + 12 * (int32_t)(k / c) - (int32_t)deg[at];
        }
        return n;
    }
}

/* VOIC LEAD: chord c (ascending, root position, m notes) as the inversion and octave whose notes move least from the
 * last chord track k played (the sum of the moves, note by note from the bottom), inside C1 .. C8; the first chord, or
 * none that fits, as it is */
static void chord_lead(uint32_t k, uint8_t *c, uint32_t m)
{
    uint8_t cand[CHORD_MAX], best[CHORD_MAX];
    uint32_t pn = k < NTRK ? vl_n[k] : 0u, i, j, inv;
    int32_t bcost = 0x7FFFFFFF, oct, cost, v;
    if (!pn)
        return;
    for (inv = 0; inv < m; inv++)
        for (oct = -1; oct <= 1; oct++) {
            for (i = 0, cost = 0; i < m; i++) {
                v = c[i] + 12 * oct + (i < inv ? 12 : 0);
                if (v < 24 || v > 108)
                    break;
                cand[i] = (uint8_t)v;
            }
            if (i < m)
                continue;
            for (i = 1; i < m; i++)                     /* ascending */
                for (j = i; j > 0 && cand[j - 1u] > cand[j]; j--) {
                    uint8_t x = cand[j];
                    cand[j] = cand[j - 1u];
                    cand[j - 1u] = x;
                }
            for (i = 1; i < m && cand[i] != cand[i - 1u]; i++)
                ;
            if (i < m)
                continue;                               /* (POW's doubled root met itself: each note once) */
            for (i = 0; i < m; i++) {
                v = (int32_t)cand[i] - vl_prev[k][i < pn ? i : pn - 1u];
                cost += v < 0 ? -v : v;
            }
            if (cost < bcost) {
                bcost = cost;
                memcpy(best, cand, m);
            }
        }
    if (bcost != 0x7FFFFFFF)
        memcpy(c, best, m);
}

/* the chord on note `root` as track t plays it now -> out (ascending), the number of notes (1..CHORD_MAX);
 * *rp its root, *maskp its tones above the root (bit i: i semitones; 0 = no chord). The root alone when CHRD
 * is OFF, on a kit, or for MONO / LEGATO / UNISON (their chord's root) */
static uint32_t chord_make(const track_t *t, uint32_t root, uint8_t *out, int32_t *rp, uint16_t *maskp)
{
    int32_t iv[CHORD_MAX + 1u], r = (int32_t)root, x;
    uint32_t mode = (uint32_t)t->p[P_CHRD], n, i, j, m = 0, f;
    uint16_t mask = 0;
    *rp = r;
    *maskp = 0;
    if (!mode || mode > CH_POW || chord_kit(t)) {
        out[0] = (uint8_t)root;
        return 1;
    }
    n = chord_tones(t, mode, &r, iv);
    if ((f = chord_flavors(t)) != 0) {         /* (MRC: the flavor keys; on the root DIA snapped) */
        n = flav_tones(t, mode, r, f, iv);
        for (i = 1; i < n; i++)                         /* ascending, as chord_tones gives them (VOIC below) */
            for (j = i; j > 0 && iv[j - 1u] > iv[j]; j--) {
                x = iv[j];
                iv[j] = iv[j - 1u];
                iv[j - 1u] = x;
            }
    }
    for (i = 0; i < n; i++)                             /* (MRC: an ADD9 flavor's 9th / 11th above the octave: */
        mask |= (uint16_t)(iv[i] == 14 ? CN_9 : iv[i] == 17 ? CN_11 : iv[i] == 18 ? CN_S11 : 1u << (iv[i] % 12));   /* bits of their own) */
    *rp = r;
    *maskp = mask;
    if (trk_vmode(t) != V_POLY) {                       /* one voice: the root */
        out[0] = (uint8_t)clamp(r, 0, 127);
        return 1;
    }
    switch (t->p[P_VOIC]) {
    case VC_OPEN:                                       /* 1-5-3(-7): the second tone an octave up */
        if (n >= 3u)
            iv[1] += 12;
        break;
    case VC_INV2:
    case VC_INV1:
        for (j = 0; j < (t->p[P_VOIC] == VC_INV2 ? 2u : 1u); j++) {   /* the lowest tone an octave up */
            x = iv[0] + 12;
            for (i = 1; i < n; i++)
                iv[i - 1u] = iv[i];
            iv[n - 1u] = x;
        }
        break;
    case VC_BASS:                                       /* the root an octave down; a seventh drops its fifth */
        if (n == CHORD_MAX) {
            iv[2] = iv[3];
            n--;
        }
        for (i = n; i > 0; i--)
            iv[i] = iv[i - 1u];
        iv[0] = -12;
        n++;
        break;
    default:
        break;
    }
    for (i = 1; i < n; i++)                             /* ascending */
        for (j = i; j > 0 && iv[j - 1u] > iv[j]; j--) {
            x = iv[j];
            iv[j] = iv[j - 1u];
            iv[j - 1u] = x;
        }
    for (i = 0; i < n && m < CHORD_MAX; i++) {          /* inside 0..127, each note once */
        x = r + iv[i];
        if (x < 0 || x > 127 || (m && out[m - 1u] == (uint8_t)x))
            continue;
        out[m++] = (uint8_t)x;
    }
    if (!m) {
        out[0] = (uint8_t)clamp(r, 0, 127);
        return 1;
    }
    if (t->p[P_VOIC] == VC_LEAD)
        chord_lead(trk_index(t), out, m);
    return m;
}

/* chord_make for a note played now: a chord is kept in chord_last (the CHORD page shows it) */
static uint32_t chord_build(track_t *t, uint32_t root, uint8_t *out)
{
    int32_t r;
    uint16_t mask;
    uint32_t m = chord_make(t, root, out, &r, &mask), i, k = trk_index(t);
    if (m > 1u) {
        chord_last[k].root = (uint8_t)clamp(r, 0, 127);
        chord_last[k].mask = mask;
        chord_last[k].n = (uint8_t)m;
        for (i = 0; i < CHORD_MAX; i++)
            chord_last[k].note[i] = i < m ? out[i] : 0u;
        chord_last[k].gen++;
        if (k < NTRK) {                                 /* (VOIC LEAD: the next chord leads from this one) */
            memcpy(vl_prev[k], out, m);
            vl_n[k] = (uint8_t)m;
        }
    }
    return m;
}

/* a chord's name from its root and tones (bit i: i semitones above the root; CN_9 / CN_11 a 9th / 11th above the
 * octave, the ADD9 flavor's): "C", "Cm7", "F#m7b5", "Gsus4", "A5", MRC: "Cadd9", "Dm9", "F6/9", "G7sus24", "C7no3"
 * -> b (12 bytes) */
static void chord_name(char *b, uint32_t root, uint32_t mask)
{
    uint32_t mi = (mask >> 3) & 1u, ma = (mask >> 4) & 1u, b5 = (mask >> 6) & 1u, p5 = (mask >> 7) & 1u;
    uint32_t s5 = (mask >> 8) & 1u, d7 = (mask >> 9) & 1u, m7 = (mask >> 10) & 1u, M7 = (mask >> 11) & 1u;
    uint32_t has7 = m7 | M7, l, six = 0;
    const char *q, *x = "", *e = "";
    str_cpy(b, N_NOTE[root % 12u], 12);
    if (mi && !ma && b5 && !p5)
        q = d7 ? "dim7" : m7 ? "m7b5" : "dim";
    else if (ma && !mi && s5 && !p5)
        q = m7 ? "+7" : M7 ? "+M7" : "aug";
    else if (ma && !mi) {
        q = m7 ? "7" : M7 ? "maj7" : d7 ? "6" : "";
        six = d7 && has7;
    } else if (mi) {
        q = m7 ? "m7" : M7 ? "mM7" : d7 ? "m6" : "m";
        six = d7 && has7;
        if (s5 && !p5 && !b5)
            e = "b6";                                   /* (the 6 flavor's b6 for the 5th) */
    } else if (((mask >> 5) | (mask >> 2)) & 1u) {      /* no third: suspended */
        q = m7 ? "7" : M7 ? "maj7" : d7 ? "6" : "";
        x = (mask >> 5) & (mask >> 2) & 1u ? "sus24" : (mask >> 5) & 1u ? "sus4" : "sus2";
        six = d7 && has7;
    } else
        q = m7 ? "7no3" : M7 ? "M7no3" : d7 ? "6no3" : "5";
    l = str_len(b);
    str_cpy(b + l, q, 12 - l);
    if (mask & CN_9) {                                  /* a 7th's chord: the 9th names it ("9", "maj9", "m9") */
        l = str_len(b);
        if (has7 && q[0] && q[str_len(q) - 1u] == '7')
            b[l - 1u] = '9';
        else
            e = d7 && !has7 ? "/9" : "add9";
    }
    if (mask & (CN_11 | CN_S11))
        e = mask & CN_S11 ? "add#11" : "add11";
    l = str_len(b);                                     /* (7 characters at most, the CHORD page's: what does not */
    if (l + str_len(x) > 7u)                            /* fit is left out, the extensions first) */
        x = "sus";
    str_cpy(b + l, x, 12 - l);
    if (six) {
        l = str_len(b);
        if (l + 5u <= 7u)
            str_cpy(b + l, "add13", 12 - l);
    }
    l = str_len(b);
    if (l + str_len(e) <= 7u)
        str_cpy(b + l, e, 12 - l);
}

/* the MIDI note (ch, src) of track id - 1 that plays a chord, 0 = none */
static mchord_t *mchord_of(uint32_t ch, uint32_t src, uint32_t id)
{
    uint32_t i;
    for (i = 0; i < MCHORD_N; i++)
        if (mchord[i].id == id && mchord[i].ch == ch && mchord[i].src == src)
            return &mchord[i];
    return 0;
}

/* a MIDI-held chord of track id - 1 sounds note */
static int mchord_held(uint32_t id, uint32_t note)
{
    uint32_t i, j;
    for (i = 0; i < MCHORD_N; i++)
        if (mchord[i].id == id)
            for (j = 0; j < mchord[i].n; j++)
                if (mchord[i].note[j] == note)
                    return 1;
    return 0;
}

static void mchord_forget(uint32_t track)               /* panic: the track's MIDI chords are gone */
{
    uint32_t i;
    for (i = 0; i < MCHORD_N; i++)
        if (mchord[i].id == track + 1u)
            mchord[i].id = 0;
}
