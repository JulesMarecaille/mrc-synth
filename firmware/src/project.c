/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Projects: the music as a whole, in one storage object. MRC: the autosave is the one project (no slots: upstream's four
 * PROJECT slots, their .noinit cache and their flash objects are gone; the flash holds the saved patterns there,
 * pattern.c), and it and the editor's backup (id 0) are the only users of this format. Only FUN10 is read: the
 * formats before it (FUN1..FUN9, upstream 0.5 .. 1.1, and their DIGITAL / PERC / PHYS / GM drum part migrations)
 * were dropped with the slots that held them; the autosave has only ever been FUN10.
 *
 * FUN10 ("FUNA", written since upstream 1.2), 3840 bytes (a whole storage object, storage.c ST_PAYLOAD_MAX):
 *   - bytes 0..7: magic, size; 8..61: the G_COUNT globals (int16 LE); 62 the selected track, 63 parts (NPART), 64
 *     phys (PROJ_PHYS), 66 P_COUNT as written;
 *   - from 68: four tracks, each P_COUNT parameters as bytes (+ 64), engine, preset, 64 steps of 9 bytes
 *     (proj_step_pack: a step's NUDGE in bit 7 of its note bytes, RATCH in bit 7 of its velocity and chance bytes);
 *   - the song (core.h chain_config_t, 84 bytes; MRC: a saved pattern per track, kind CHAIN_KIND_PAT: a song of
 *     PROJECT slots loads as none), then the motion block (core.h motion_store_t, 388 bytes: 128 records);
 *   - PROJ_MIDI_OFF: MIDI IN TRACK's channels (1.5.5); PROJ_FM6_OFF: each track's FM6 patch (eng_fm6.c, 128 bytes
 *     packed); PROJ_NAME_OFF: the name (12 bytes); the last 4: an FNV-1a hash of the rest.
 * A FUN10 of fewer parameters (104: upstream 1.2 .. 1.4) maps by count (params_by_count: its engine values at today's
 * P_E0.., the parameters it lacks at their defaults; its motion ids moved with them, proj_motion_ids). Parameter locks
 * are motion records with bit 7 of their id set (MOTION_LOCK).
 *
 * Built on the Mac too (tests/project_test.c, -DPROJ_HOST): the part above the #ifndef
 * PROJ_HOST needs core.h, params.c (TP) and engines.c. */
#define PROJ_MAGIC 0x46554E41u                 /* "FUNA" (FUN10): 104 (1.5: 111) parameters, 128 motion records, nudges */
#define PROJ_PHYS 2u                           /* project_t.phys: always written (upstream: PHYS without DUST and DRUM) */
#define PROJ_NAME_LEN 12u                      /* the name: bytes PROJ_NAME_OFF.. */
typedef struct {                               /* one track */
    int16_t p[P_COUNT];
    uint8_t engine, preset;
    step_t step[NSTEP];
} proj_trk_t;
typedef struct {
    uint32_t magic, size;
    int16_t g[G_COUNT];
    uint8_t sel;                               /* the selected track */
    uint8_t parts;                             /* NPART (written; upstream's older formats: the GM drum part) */
    uint8_t phys;                              /* PROJ_PHYS (written) */
    uint8_t rsv;
    proj_trk_t t[NTRK];
    chain_config_t chain;
    motion_store_t motion;
    uint8_t fm6[NTRK][FM6_PACKED];             /* each track's FM6 patch, packed (eng_fm6.c) */
    uint8_t mch[NTRK];                         /* 1.5.5: MIDI IN TRACK's channels, as song.mch (PROJ_MIDI_OFF) */
    char name[PROJ_NAME_LEN];                  /* the project's name: upper-case ASCII 32..126, 0-padded; "" = none */
    uint32_t sum;
} project_t;
#define PROJ_STORE_SIZE 3840u                  /* FUN10: a whole storage object (storage.c ST_PAYLOAD_MAX) */
#define PROJ_NAME_OFF (PROJ_STORE_SIZE - 4u - PROJ_NAME_LEN)
#define PROJ_FM6_OFF (PROJ_NAME_OFF - NTRK * FM6_PACKED)
/* 1.5.5 (#202): MIDI IN TRACK, the last 3 of FUN10's 16 spare bytes (just before the patches: the parameters a later
 * version adds grow from the other end, 13 bytes, 3 more track parameters). 24 bits LE: bits 5k..5k+4 track k's channel
 * as song.mch holds it (0..15 its distance up from channel k + 1, 16 OMNI, 17 OFF; 18..31 read as 0), bit 23 set: MIDI IN
 * is TRACK. Then the globals' G_ROUTE holds the block TRACK falls back to (proj_route_fallback: the block its four
 * channels make, else CH1-4), which is all a firmware before 1.5.5 reads (these bytes it never did): a TRACK project plays
 * there as that block. Every project before has them 0: channels 1..4, MIDI IN as its G_ROUTE says */
/* MRC: the track FILTER (P_TLC P_THC P_TRES, P_COUNT 114) took 12 of those 13 bytes: 1 is left, no track parameter fits */
#define PROJ_MIDI_OFF (PROJ_FM6_OFF - 3u)
typedef union { uint32_t align; uint8_t raw[PROJ_STORE_SIZE]; } project_store_t;
_Static_assert(G_COUNT == 27u, "FUN10 globals retain original IDs");
_Static_assert(sizeof(project_store_t) == 3840u, "FUN10 size");

static uint32_t proj_hash(const void *p, uint32_t n)   /* FNV-1a over n bytes */
{
    const uint8_t *b = (const uint8_t *)p;
    uint32_t i, s = 0x811C9DC5u;
    for (i = 0; i < n; i++)
        s = (s ^ b[i]) * 16777619u;
    return s;
}
static uint32_t proj_sum(const project_t *p) { return proj_hash(p, sizeof *p - 4u); }
static int proj_ok(const project_t *q) { return q->magic == PROJ_MAGIC && q->size == sizeof *q &&
    q->sum == proj_sum(q) && chain_valid(&q->chain) && motion_valid(&q->motion); }

/* every track's engine one this firmware has: a project of another number is not one */
static int proj_engines_ok(const project_t *q)
{
    uint32_t t;
    for (t = 0; t < NTRK; t++)
        if (q->t[t].engine >= NENGINES)
            return 0;
    return 1;
}

/* 12 stored name bytes -> d (PROJ_NAME_LEN + 1): up to the first 0, upper case; a byte outside 32..126 makes it
 * no name (a damaged tail never refuses the project) */
static uint32_t proj_name_get(char *d, const uint8_t *s)   /* its length */
{
    uint32_t i;
    for (i = 0; i < PROJ_NAME_LEN && s[i]; i++) {
        if (s[i] < 32u || s[i] > 126u) { i = 0; break; }
        d[i] = (char)(s[i] >= 'a' && s[i] <= 'z' ? s[i] - 32u : s[i]);
    }
    d[i] = 0;
    return i;
}

/* MIDI IN TRACK (PROJ_MIDI_OFF): G_ROUTE TRACK is stored as the block its channels make (0 1 2 3 + 4n: CH1-4 .. CH13-16),
 * else CH1-4, and the TRACK bit */
static uint32_t proj_route_fallback(const uint8_t *mch)
{
    uint32_t k;
    for (k = 1; k < NTRK; k++)
        if (mch[k] != mch[0])
            return 0;
    return mch[0] % 4u || mch[0] >= 16u ? 0u : mch[0] ? mch[0] / 4u + 1u : 0u;
}
static void proj_midi_pack(uint8_t *b, const project_t *q)
{
    uint32_t k, v = 0;
    for (k = 0; k < NTRK; k++)
        v |= (uint32_t)(q->mch[k] < MCH_N ? q->mch[k] : 0u) << (5u * k);
    if (q->g[G_ROUTE] == ROUTE_TRACK) {
        int16_t r = (int16_t)proj_route_fallback(q->mch);
        memcpy(b + 8 + 2u * G_ROUTE, &r, 2);
        v |= 1u << 23;
    }
    b[PROJ_MIDI_OFF] = (uint8_t)v; b[PROJ_MIDI_OFF + 1u] = (uint8_t)(v >> 8); b[PROJ_MIDI_OFF + 2u] = (uint8_t)(v >> 16);
}
static void proj_midi_unpack(project_t *q, const uint8_t *b)
{
    uint32_t k, v = b[PROJ_MIDI_OFF] | (uint32_t)b[PROJ_MIDI_OFF + 1u] << 8 | (uint32_t)b[PROJ_MIDI_OFF + 2u] << 16, f;
    for (k = 0; k < NTRK; k++) {
        f = (v >> (5u * k)) & 31u;
        q->mch[k] = (uint8_t)(f < MCH_N ? f : 0u);
    }
    if (v >> 23)
        q->g[G_ROUTE] = ROUTE_TRACK;
}

/* one step in its 9 bytes (FUN10; MRC: a saved pattern's too, pattern.c): four notes (bit 7: the NUDGE's bits), n | time |
 * accent and slide, velocity (bit 7: RATCH bit 0), hits, accents, chance (bit 7: RATCH bit 1). 0: a step outside the
 * fields, nothing written that proj_step_unpack would refuse */
#define PROJ_STEP_B 9u
static int proj_step_pack(uint8_t *b, const step_t *s)
{
    uint32_t r = step_ratchet(s) - 1u, nd = (uint32_t)step_nudge(s) & 15u, j;
    if (s->n > 4u || s->time > ST_REST || (s->flags & ~(3u | SF_RATCH | SF_NUDGE)) || s->probability >= COND_FIRST + NCOND)
        return 0;
    for (j = 0; j < 4u; j++)                            /* notes and velocity 0..127, accents only on hits */
        *b++ = (uint8_t)((s->note[j] > 127u ? 127u : s->note[j]) | (nd >> j & 1u) << 7);
    *b++ = (uint8_t)(s->n | s->time << 3 | (s->flags & 3u) << 5);
    *b++ = (uint8_t)((s->vel > 127u ? 127u : s->vel) | (r & 1u) << 7);
    *b++ = s->hit;
    *b++ = s->acc & s->hit;
    *b = (uint8_t)(s->probability | (r >> 1) << 7);
    return 1;
}
/* .. back. 0: not a step */
static int proj_step_unpack(step_t *s, const uint8_t *b)
{
    uint32_t meta, nd = 0, j;
    for (j = 0; j < 4u; j++) {
        nd |= (uint32_t)(b[j] >> 7) << j;
        s->note[j] = b[j] & 127u;
    }
    meta = b[4];
    s->n = meta & 7u; s->time = (meta >> 3) & 3u; s->flags = (meta >> 5) & 3u;
    s->vel = b[5] & 127u; meta |= (b[5] & 128u) << 1;  /* (RATCH bit 0 at bit 8 of meta) */
    s->hit = b[6]; s->acc = b[7];
    s->probability = b[8] & 127u; meta |= (b[8] & 128u) << 2;   /* (bit 1 at bit 9) */
    if ((meta & 255u) > 127u || s->n > 4u || s->time > ST_REST || s->probability >= COND_FIRST + NCOND) return 0;
    if (s->acc & ~s->hit) return 0;
    step_set_ratchet(s, (meta >> 8) + 1u);
    step_set_nudge(s, (int32_t)(nd ^ 8u) - 8);
    return 1;
}

/* A stable serialized schema: first header retains FUN6's fields; at byte66
 * np, format flags, then four byte-param tracks and nine-byte steps; FUN8: the patches at PROJ_FM6_OFF. */
static int proj_pack(project_store_t *out, const project_t *q)
{
    uint8_t *b = out->raw; uint32_t pos = 68u, t, i; uint32_t magic = PROJ_MAGIC, size = PROJ_STORE_SIZE, sum;
    for (t = 0; t < NTRK; t++)
        for (i = 0; i < FM6_PACKED; i++)
            if (q->fm6[t][i] > 127u) return 0;
    if (!chain_valid(&q->chain) || !motion_valid(&q->motion) || P_COUNT > 127u) return 0;
    memset(out, 0, sizeof *out); memcpy(b, &magic, 4); memcpy(b + 4, &size, 4);
    memcpy(b + 8, q->g, sizeof q->g); b[62] = q->sel; b[63] = q->parts; b[64] = q->phys; b[66] = P_COUNT;
    for (t = 0; t < NTRK; t++) {
        for (i = 0; i < P_COUNT; i++) {
            if (q->t[t].p[i] < -64 || q->t[t].p[i] > 127) return 0;
            b[pos++] = (uint8_t)(q->t[t].p[i] + 64);
        }
        b[pos++] = q->t[t].engine; b[pos++] = q->t[t].preset;
        for (i = 0; i < NSTEP; i++, pos += PROJ_STEP_B)   /* what proj_unpack checks, so a saved project always loads */
            if (!proj_step_pack(b + pos, &q->t[t].step[i]))
                return 0;
    }
    if (pos + sizeof q->chain + sizeof q->motion > PROJ_MIDI_OFF) return 0;
    memcpy(b + pos, &q->chain, sizeof q->chain); pos += sizeof q->chain;
    memcpy(b + pos, &q->motion, sizeof q->motion);
    proj_midi_pack(b, q);
    memcpy(b + PROJ_FM6_OFF, q->fm6, sizeof q->fm6);
    {   /* the name (0-padded; stops at the first 0) */
        char n[PROJ_NAME_LEN + 1u];
        memcpy(b + PROJ_NAME_OFF, n, proj_name_get(n, (const uint8_t *)q->name));
    }
    sum = proj_hash(b, PROJ_STORE_SIZE - 4u); memcpy(b + PROJ_STORE_SIZE - 4u, &sum, 4);
    return 1;
}
/* the motion of a FUN7 / FUN8 written with np parameters (np < P_COUNT: a FUN7 of 89 before the chord keys, every
 * FUN8 of 91 before the DRUM lane levels) -> today's ids: an event names a parameter id, and the ids from that
 * store's P_E0 (np - 8) on moved up with P_E0, as its values did (params_by_count); the ones below kept theirs. A
 * lock's bit 7 (MOTION_LOCK) stays: the id moves under it (a FUN8 / FUN7 of a 1.1 development build may hold
 * locks). 0: an id that store could not have named (np or more) */
static int proj_motion_ids(motion_store_t *m, uint32_t np)
{
    uint32_t i;
    for (i = 0; i < m->count && i < MOTION_MAX; i++) {
        uint32_t id = MOTION_ID(&m->event[i]);
        if (id >= np) return 0;
        if (np < P_COUNT && id >= np - 8u)
            m->event[i].param = (uint8_t)((m->event[i].param & MOTION_LOCK) | (id + P_COUNT - np));
    }
    return 1;
}
/* a stored FUN10 -> q; 0 = not one (another format, damaged) */
static int proj_unpack(project_t *q, const uint8_t *b)
{
    uint32_t pos = 68u, t, i, magic, size, sum, np = b[66], end = PROJ_FM6_OFF;
    memcpy(&magic, b, 4); memcpy(&size, b + 4, 4); memcpy(&sum, b + PROJ_STORE_SIZE - 4u, 4);
    if (magic != PROJ_MAGIC || size != PROJ_STORE_SIZE || sum != proj_hash(b, PROJ_STORE_SIZE - 4u) ||
        np < 8u || np > P_COUNT || 68u + NTRK * (np + 2u + NSTEP * PROJ_STEP_B) + sizeof q->chain + sizeof q->motion > end)
        return 0;
    memset(q, 0, sizeof *q); q->magic = PROJ_MAGIC; q->size = sizeof *q;
    memcpy(q->g, b + 8, sizeof q->g); q->sel = b[62]; q->parts = b[63]; q->phys = b[64];
    for (t = 0; t < NTRK; t++) {
        int16_t values[P_COUNT], def[P_COUNT];
        for (i = 0; i < np; i++) { if (b[pos] > 191u) return 0; values[i] = (int16_t)b[pos++] - 64; }
        q->t[t].engine = b[pos++]; q->t[t].preset = b[pos++];
        if (q->t[t].engine >= NENGINES) return 0;
        for (i = 0; i < P_COUNT; i++) def[i] = param_desc_of(q->t[t].engine, i)->def;
        params_by_count(q->t[t].p, values, np, def);
        for (i = 0; i < NSTEP; i++, pos += PROJ_STEP_B)
            if (!proj_step_unpack(&q->t[t].step[i], b + pos))
                return 0;
    }
    memcpy(&q->chain, b + pos, sizeof q->chain);        /* MRC: a song of saved patterns (chain_config_t.kind); */
    if (q->chain.kind != CHAIN_KIND_PAT)                /* one of PROJECT slots (upstream): none, the slots are gone */
        chain_defaults(&q->chain);
    pos += sizeof q->chain;
    memcpy(&q->motion, b + pos, sizeof q->motion);
    if (!proj_motion_ids(&q->motion, np) || !chain_valid(&q->chain) || !motion_valid(&q->motion)) return 0;
    proj_midi_unpack(q, b);
    for (t = 0; t < NTRK; t++)
        for (i = 0; i < FM6_PACKED; i++)
            q->fm6[t][i] = b[end + t * FM6_PACKED + i] & 0x7Fu;
    {
        char n[PROJ_NAME_LEN + 1u];
        memcpy(q->name, n, proj_name_get(n, b + PROJ_NAME_OFF));
    }
    q->sum = proj_sum(q);
    return 1;
}
/* n bytes of a stored project -> q; 0 = not a FUN10 project */
static int proj_import(project_t *q, const void *b, int n)
{
    return n == (int)PROJ_STORE_SIZE && proj_unpack(q, b) && proj_engines_ok(q);
}

#ifndef PROJ_HOST
static project_t proj_scratch               /* decoded main-loop work, never audio ISR */
    __attribute__((section(".pool")));
static char proj_name[PROJ_NAME_LEN + 1u]    /* the name of the music as it is now (loaded, saved, the editor's */
    __attribute__((section(".pool")));       /* runtime restore); "" = none. A save takes it unless one is given */
static union {                               /* serialized main-loop work; no retained expansion */
    project_store_t s;
    uint8_t raw[3840];                         /* (the staging of a backup object, up to a storage object: editor_backup.c) */
} proj_wire_u __attribute__((section(".pool")));
#define proj_wire (proj_wire_u.s)
static uint8_t proj_wire_gen;                /* +1 whenever proj_wire is rewritten (a backup's runtime copy lives there) */

static void proj_steps(step_t *s)            /* a loaded sequence stays inside its fixed fields */
{
    uint32_t i, j;
    for (i = 0; i < NSTEP; i++) {
        if (s[i].n > 4u) s[i].n = 4;
        if (s[i].time > ST_REST) s[i].time = ST_REST;
        for (j = 0; j < 4u; j++) s[i].note[j] &= 127u;
        s[i].acc &= s[i].hit;
    }
}

/* a restored backup may hold values out of today's fields and ranges: keep them inside, as proj_pack takes them */
static void proj_bound(project_t *q)
{
    uint32_t t, i;
    for (t = 0; t < NTRK; t++) {
        proj_steps(q->t[t].step);
        for (i = 0; i < NSTEP; i++) {
            step_t *s = &q->t[t].step[i];
            s->flags &= 3u | SF_RATCH | SF_NUDGE;
            s->vel &= 127u;
            if (s->probability >= COND_FIRST + NCOND) s->probability = 0;
        }
        for (i = 0; i < P_COUNT; i++) {
            const param_desc_t *d = param_desc_of(q->t[t].engine % NENGINES, i);
            q->t[t].p[i] = (int16_t)param_fit(d, q->t[t].p[i]);   /* (a retired KIT: the kit it plays) */
        }
    }
    q->sum = proj_sum(q);
}

static void project_capture(project_t *p)
{
    uint32_t i;
    uint32_t f = motion_guard();
    memset(p, 0, sizeof *p);
    p->magic = PROJ_MAGIC;
    p->size = sizeof *p;
    for (i = 0; i < G_COUNT; i++)
        p->g[i] = song.g[i];
    p->sel = song.sel;
    p->parts = NPART;
    p->phys = PROJ_PHYS;
    p->chain = chain_config;
    for (i = 0; i < NTRK; i++) {
        for (uint32_t j = 0; j < P_COUNT; j++) p->t[i].p[j] = motion_base_value(&trk[i], j);
        p->t[i].engine = trk[i].eng_req;
        p->t[i].preset = trk[i].preset;
        memcpy(p->t[i].step, trk[i].step, sizeof trk[i].step);
        fm6_pack(fm6_patch[i], p->fm6[i]);
    }
    p->motion = motion;
    memcpy(p->mch, song.mch, sizeof p->mch);
    motion_unguard(f);
    memcpy(p->name, proj_name, str_len(proj_name));
    p->sum = proj_sum(p);
}

static int project_restore_runtime(const project_t *input)
{
    project_t *p = &proj_scratch;
    uint32_t i, k;
    if (!proj_ok(input) || !proj_engines_ok(input)) return 1;
    mom_back(0);                                  /* (ui_input.c MOMENTARY: the values as they were, then the load) */
    memset(drum_mute, 0, sizeof drum_mute);       /* (1.4.1: the DRUM lanes' mutes, performance state) */
    if (p != input) memcpy(p, input, sizeof *p);
    transport_req = 2;
    panic_req = (1u << NTRK) - 1u;
    fm1_irq_off();                                      /* the audio ISR must not see half a project */
    seq_stop();
    transport_req = 0;
    chain_config = p->chain;
    motion = p->motion;
    memset(motion_active, 0, sizeof motion_active);
    motion_base_valid = 0;
    ui.song_row = 0;
    for (i = 0; i < G_COUNT; i++)
        if (i != G_SLOT && i != G_LOAD && i != G_SAVE)
            song.g[i] = (int16_t)clamp(p->g[i], GP[i].min, GP[i].max);
    for (k = 0; k < NTRK; k++)                          /* MIDI IN TRACK's channels (1.5.5; 0, channels 1..4, before) */
        song.mch[k] = (uint8_t)(p->mch[k] < MCH_N ? p->mch[k] : 0u);
    midi_route = MIDI_ROUTE_AGAIN;                      /* (events_block: what the channels no longer hear let go) */
    for (k = 0; k < NTRK; k++) {
        track_t *t = &trk[k];
        const proj_trk_t *s = &p->t[k];
        uint32_t e = s->engine;                         /* (< NENGINES: proj_engines_ok) */
        t->eng_req = (uint8_t)e;
        t->user = 0;                                    /* (no user preset slot is saved) */
        for (i = 0; i < P_COUNT; i++) {                 /* every value back inside its range (param_fit) */
            const param_desc_t *d = param_desc_of(e, i);
            t->p[i] = (int16_t)param_fit(d, s->p[i]);
        }
        t->preset = (uint8_t)(ENGINES[e]->npresets ? s->preset % ENGINES[e]->npresets : 0u);
        memcpy(t->step, s->step, sizeof t->step);
        proj_steps(t->step);
        {   /* the project's own FM6 patch, never reloaded from SLOT: F n if it is that factory patch, else OWN */
            uint8_t v[FP_SIZE + 1u];
            fm6_unpack(p->fm6[k], v);
            fm6_set_patch(k, v);
            fm6_adopt(k);
        }
    }
    song.sel = (uint8_t)(p->sel < NTRK ? p->sel : 0u);
    fm1_irq_on();
    proj_name_get(proj_name, (const uint8_t *)p->name);
    pat_from_set(0);                                    /* (no saved pattern: the header's number goes) */
    undo.trk = 0;                                       /* (ui.c) the undo copy belongs to the old project */
    for (k = 0; k < NTRK; k++)
        pat_sig[k] = ~steps_sig(&trk[k]);               /* a project's steps are the user's */
    sync_reload = 1;
    ui.force = 1;
    ui_message("LOADED");
    memset(snd_said, 0, sizeof snd_said);               /* a missing sample is said again, after LOADED */
    return 0;
}
/* settings + learned panel table: one flash object. The flash copy wins at
 * boot (the .noinit copies are garbage after a power-off). */
#include "settings_persist.c"
#if FELUCCA_FLASH && FELUCCA_SLICE
#include "slice_store.c"                          /* SLICE's MAN slices, kept in the user slots */
#endif
#if FELUCCA_FLASH
static persist_t persist_saved __attribute__((section(".pool")));
static uint8_t persist_pending;                 /* 1 requested, 2 waiting after a flash error */
static uint32_t persist_retry_ms;
#endif

static void persist_boot(void)                    /* before settings_init / panel_init */
{
#if FELUCCA_FLASH
    persist_t p;
    uint32_t f = irq_save();
    uint32_t flash_id = FL_FAR(fl_jedec_ram)();
    flash_ok = flash_id == 0x856014u || flash_id == 0xB36014u; /* known 1 MiB parts, else RAM-only */
    irq_restore(f);
    if (!flash_ok)
        return;
    fl_plain_window_init();                        /* flash above 0x93000 reads as plaintext through XIP
                                                    * (user sample sets are played from there) */
#if FELUCCA_SLICE
    slc_store_boot();                              /* (the scans read each slot's stored slices) */
#endif
    {
        uint32_t k;
        for (k = 0; k < SMP_USER_SLOTS; k++)
            smp_user_scan(k);
    }
    {
        int n = st_load(OBJ_SETTINGS, &p, sizeof p);
        if (settings_import(&p, n))
            persist_saved = p;
    }
    up_boot();                                     /* user presets */
    pat_boot();                                    /* MRC: the saved patterns' names, lengths, pictures */
#endif
}

static void settings_poll(void)
{
#if FELUCCA_FLASH
    persist_t p;
#if FELUCCA_SLICE
    slc_store_poll();                              /* SLICE's slices edited on the SLICES page */
#endif
    if (!persist_pending || !flash_ok || transport_busy() ||
        (persist_pending == 2u && (uint32_t)(fm1_ms - persist_retry_ms) < 1000u))
        return;
    p = persist_saved;
    settings_export(&p);
    if (!memcmp(&p, &persist_saved, sizeof p)) {
        persist_pending = 0;
        return;                                    /* unchanged: no erase cycle */
    }
    if (st_save(OBJ_SETTINGS, &p, sizeof p) == 0) {
        persist_saved = p;
        persist_pending = 0;
    } else {
        persist_pending = 2;
        persist_retry_ms = fm1_ms;
    }
#endif
}

static void settings_save(void)
{
#if FELUCCA_FLASH
    persist_pending = 1;
#endif
    settings_poll();
}

/* The autosave (1.2, Discussion #130): the music as it is (what a project holds: every track's sound, steps, LEN DIV
 * SWING GATE, mix, the song, the automation and locks, the FM6 patches, the name) kept in its own storage object
 * (OBJ_AUTOSAVE, A/B at 0xE5000 / 0xE6000, never one of the four project slots), and put back at power-on
 * (autosave_boot), always (MRC: the autosave is the one project; MENU > SYSTEM > RESTORE LAST is gone). Flash wear: a
 * 4 KiB erase per write, the two sectors taking turns, so a write only when
 *   - something a project holds changed since the last one (autosave_sig: a hash of those values, no copy; not the
 *     selected track, nor the PROJECT page's own controls),
 *   - and it has stayed the same for AS_IDLE_MS (MRC: 3 s; upstream 10 s) with the transport stopped, no key or button down, no voice
 *     sounding and no editor transfer (a backup or a sample: autosave_hold); an erase stops the audio for its
 *     ~40 ms, as every save does, so it never happens while anything plays,
 *   - and at least AS_GAP_MS (60 s) after the one before (AS_RETRY_MS after a failed write).
 * So at most one write a minute while editing, none while playing or idle: about 100,000 erase cycles per sector
 * give over 200,000 writes, a write a minute for 8 hours a day for over a year, far more with real pauses.
 * The shorter idle changes nothing of that: the gap is what bounds the writes. Never restored after a crash, a hang or a
 * failed boot (main.c): the music that was playing then could be what crashed. Checked every AS_POLL_MS; the main loop only (never the audio ISR) */
#define AS_POLL_MS 250u
#define AS_IDLE_MS 3000u
#define AS_GAP_MS 60000u
#define AS_RETRY_MS 30000u
static struct {
    uint32_t saved, seen;                        /* the signature written last (or loaded at power-on), seen last */
    uint32_t t, last, poll;                      /* fm1_ms: seen changed / busy, the last write, the last check */
    uint32_t writes;                             /* (written since power-on: the host tests count them) */
    uint8_t wrote, err;                          /* a write since power-on (last is valid); the last one failed */
} as __attribute__((section(".pool")));

static uint32_t as_mix(uint32_t h, const void *p, uint32_t n)   /* FNV-1a, continued */
{
    const uint8_t *b = p;
    while (n--) {
        h ^= *b++;
        h *= 16777619u;
    }
    return h;
}

/* what an autosave would hold, hashed (no copy, no RAM): project_capture's values */
static uint32_t autosave_sig(void)
{
    uint32_t h = 2166136261u, i, j;
    for (i = 0; i < NTRK; i++) {
        const track_t *t = &trk[i];
        for (j = 0; j < P_COUNT; j++) {
            int16_t v = motion_base_value(t, j);
            h = as_mix(h, &v, sizeof v);
        }
        h = as_mix(h, &t->eng_req, 1);
        h = as_mix(h, &t->preset, 1);
        h = as_mix(h, t->step, sizeof t->step);
        h = as_mix(h, fm6_patch[i], sizeof fm6_patch[i]);
    }
    for (i = 0; i < G_COUNT; i++)
        if (i != G_SLOT && i != G_NAME && i != G_LOAD && i != G_SAVE)
            h = as_mix(h, &song.g[i], sizeof song.g[i]);
    h = as_mix(h, song.mch, sizeof song.mch);           /* (1.5.5: MIDI IN TRACK's channels, the project's) */
    h = as_mix(h, &motion, sizeof motion);
    h = as_mix(h, &chain_config, sizeof chain_config);
    return as_mix(h, proj_name, sizeof proj_name);
}

/* nothing going on that an erase (the audio stopped ~40 ms) would be heard in, or would race */
static int autosave_quiet(void)
{
    uint32_t k, i;
    if (transport_busy() || fm1_in.notes || fm1_in.buttons)
        return 0;
    for (k = 0; k < NTRK; k++)
        for (i = 0; i < NVOICE; i++)
            if (trk[k].v[i].active)
                return 0;
    return 1;
}

static void autosave_hold(void) { as.t = fm1_ms; }   /* the editor's transfers: their staging RAM is proj_wire */

#if FELUCCA_FLASH
/* the music now -> the autosave object (sig: its autosave_sig); 1 written */
static int autosave_write(uint32_t sig)
{
    as.last = fm1_ms;
    as.wrote = 1;
    project_capture(&proj_scratch);
    proj_wire_gen++;
    as.err = !proj_pack(&proj_wire, &proj_scratch) || st_save(OBJ_AUTOSAVE, &proj_wire, sizeof proj_wire) != 0;
    if (!as.err) {
        as.saved = sig;
        as.writes++;
    }
    return !as.err;
}
#endif

/* main loop, every pass (after settings_poll) */
static void autosave_poll(void)
{
#if FELUCCA_FLASH
    uint32_t sig;
    if (!flash_ok || fm1_ms - as.poll < AS_POLL_MS)
        return;
    as.poll = fm1_ms;
    sig = autosave_sig();
    if (sig != as.seen || !autosave_quiet()) {
        as.seen = sig;
        as.t = fm1_ms;
        return;
    }
    if (sig == as.saved || fm1_ms - as.t < AS_IDLE_MS ||
        (as.wrote && fm1_ms - as.last < (as.err ? AS_RETRY_MS : AS_GAP_MS)))
        return;
    autosave_write(sig);
#endif
}

/* MRC: SAVE held + OCT+ (ui_input.c): the autosave written now, whatever the idle wait and the gap (the user asked:
 * before a power-off, which the firmware cannot see coming). Refused while the transport runs (an erase stops the
 * audio ~40 ms); a voice still ringing is cut for that long. Nothing changed: nothing written, SAVED all the same.
 * Then the full-screen SAVED (ui_draw.c draw_saved_full) */
static void save_now_ui(void)
{
#if FELUCCA_FLASH
    uint32_t sig;
    if (transport_busy()) {
        ui_message("STOP TO SAVE");
        return;
    }
    sig = autosave_sig();
    as.seen = sig;
    if (!flash_ok || (sig != as.saved && !autosave_write(sig))) {
        ui_message("SAVE ERROR");
        return;
    }
    ui.saved_t = SAVEDF_FRAMES;
    ui.force = 1;
#else
    ui_message("SAVE ERROR");
#endif
}

/* the music differs from what the autosave holds (as of the last check, at most AS_POLL_MS ago): the footer's dot
 * (ui_draw.c draw_foot). Never without a flash: there is nothing to wait for */
static int autosave_dirty(void)
{
#if FELUCCA_FLASH
    return flash_ok && as.seen != as.saved;
#else
    return 0;
#endif
}

/* power-on, after the defaults (main.c): the autosave becomes the music when `allowed` (no crash, hang or failed boot
 * before); an autosave that does not load (none, torn, damaged, another format) is ignored.
 * The music now (restored or not) is what is saved: nothing is written until it changes. 1 = restored */
static int autosave_boot(int allowed)
{
    int rc = 0;
#if FELUCCA_FLASH
    if (flash_ok && allowed) {
        int n;
        proj_wire_gen++;
        n = st_load(OBJ_AUTOSAVE, &proj_wire, sizeof proj_wire);
        if (n > 0 && proj_import(&proj_scratch, &proj_wire, n)) {
            proj_bound(&proj_scratch);
            if (!project_restore_runtime(&proj_scratch)) {
                ui_message("RESTORED");
                rc = 1;
            }
        }
    }
#else
    (void)allowed;
#endif
    as.saved = as.seen = autosave_sig();
    as.t = fm1_ms;
    return rc;
}

#if FELUCCA_FLASH
_Static_assert(sizeof(project_store_t) <= ST_PAYLOAD_MAX, "project does not fit one flash sector");
_Static_assert(sizeof(persist_t) <= ST_PAYLOAD_MAX, "settings do not fit one flash sector");
#endif
#include "pattern.c"                           /* MRC: the saved patterns, SONG's sources */
#endif /* PROJ_HOST */
