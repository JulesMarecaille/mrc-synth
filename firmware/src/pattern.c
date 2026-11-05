/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* MRC: saved patterns (included by project.c, the main loop only). A pattern is one track's sequence: its 64 steps as
 * they are (notes and chords, VEL, CHANCE, RATCH, NUDGE, the DRUM lanes' hits and accents) and LEN DIV SWING GATE
 * QUANTIZE; no sound and no automation. NPAT of them (P01..P24): SAVE > PATTERN writes one from the selected track,
 * GLO > PATTERNS loads one into it or erases it, SONG chains them (chain_prepare).
 *
 * Flash: four banks of PAT_PER_BANK in the storage objects that held upstream's four PROJECT slots (storage.c OBJ_PAT0,
 * A/B sectors at 0x97000..0x9EFFF): a write is a whole bank, the other copy erased and committed last, so a power cut
 * leaves the bank as it was or as written. A bank: magic "PTB1", PAT_PER_BANK, 0, the record size (u16 LE), then the
 * records. A record (PAT_REC bytes): PAT_USED (0 an empty slot), 0, the name (12 bytes, ASCII 32..126, 0-padded), LEN
 * DIV SWING GATE QUANTIZE (+ 64 each), 0, the steps in project.c's 9 bytes each (proj_step_pack), 0-padded. A bank of
 * another format (a project of upstream's slots) reads as six empty ones and is overwritten by the first save there.
 * A bank is staged in proj_wire (project.c), as the autosave and the editor's backups are.
 *
 * RAM: pat_mirror, what lists and SONG draw (name, length, picture) without reading the flash, filled at power-on
 * (pat_boot) and on every write. A build without a flash (FELUCCA_FLASH 0: the host tests) keeps the banks in RAM
 * (pat_ram); a device whose flash did not answer has no patterns (a save: SAVE ERROR). */
#define PAT_PER_BANK 6u
#define PAT_BANKS (NPAT / PAT_PER_BANK)
#define PAT_MAGIC 0x31425450u                    /* "PTB1" */
#define PAT_USED 0xA5u
#define PAT_HDR 8u
#define PAT_REC 600u
#define PAT_NAME_OFF 2u
#define PAT_TIM_OFF 14u                          /* LEN DIV SWING GATE (P_SLEN .. P_SGATE), then QUANTIZE */
#define PAT_STEP_OFF 20u
#define PAT_BANK_SIZE (PAT_HDR + PAT_PER_BANK * PAT_REC)
_Static_assert(PAT_STEP_OFF + NSTEP * PROJ_STEP_B <= PAT_REC && PAT_BANK_SIZE <= sizeof(project_store_t) &&
               NPAT % PAT_PER_BANK == 0u && PAT_BANKS <= 4u && P_SGATE == P_SLEN + 3, "pattern bank layout");
static pat_info_t pat_mirror[NPAT] __attribute__((section(".pool")));   /* (pat_gen: ui.c) */
#if !FELUCCA_FLASH
static uint8_t pat_ram[PAT_BANKS][PAT_BANK_SIZE];   /* (no flash: the banks, as written; 0 = none) */
#endif

static int pat_used(uint32_t k) { return k < NPAT && pat_mirror[k].used; }
static const pat_info_t *pat_info(uint32_t k) { return &pat_mirror[k % NPAT]; }
static void pat_tag(char *b, uint32_t k)         /* "P01" (4 bytes) */
{
    b[0] = 'P';
    b[1] = (char)('0' + (k + 1u) / 10u);
    b[2] = (char)('0' + (k + 1u) % 10u);
    b[3] = 0;
}
static void pat_default_name(char *b, uint32_t k)   /* "PATTERN 01" (13 bytes): what an empty name saves as */
{
    str_cpy(b, "PATTERN ", 13);
    pat_tag(b + 7, k);
    b[7] = ' ';
}

/* a pattern's length as SONG plays it (song_chain.c chain_tick: LEN steps of its DIV) in 1/48 quarter notes (a 4/4
 * bar: 192) */
static uint32_t pat_len48(int32_t len, int32_t div)
{
    uint32_t n = (uint32_t)clamp(len, TP[P_SLEN].min, TP[P_SLEN].max), d = (uint32_t)clamp(div, TP[P_SDIV].min, TP[P_SDIV].max);
    return n * (d < 6u ? 48u / DIV_DEN[d] : d < 10u ? 48u << (d - 5u) : 48u / DIV_DEN[d % 6u]);   /* (fx.c div_samples) */
}
/* SONG's picture of a pattern (ui_graph.c graph_song): its LEN steps in n = min(LEN, 16) cells (a cell: steps
 * c * LEN / n .. (c + 1) * LEN / n - 1), 2 bits a cell, cell c at bits 2c: a step weighs 0 (rest, empty, CHANCE 0),
 * 1 (a tie), 2 (a note or hit), 3 (accented); a cell's level is its steps' mean, rounded up (0..3). b: the packed steps */
static uint32_t pat_density(const uint8_t *b, uint32_t len, uint8_t *n)
{
    uint32_t nc = len < 16u ? len : 16u, c, i, d = 0;
    for (c = 0; c < nc; c++) {
        uint32_t i0 = c * len / nc, i1 = (c + 1u) * len / nc, w = 0;
        for (i = i0; i < i1; i++) {
            step_t s;
            if (!proj_step_unpack(&s, b + i * PROJ_STEP_B))
                continue;
            if (s.time == ST_TIE)
                w += 1u;
            else if (s.time == ST_NOTE && (s.n || s.hit) && step_chance(&s))
                w += (s.flags & SF_ACCENT) || s.acc ? 3u : 2u;
        }
        w = (w + (i1 - i0) - 1u) / (i1 - i0);
        d |= (w < 3u ? w : 3u) << (2u * c);
    }
    *n = (uint8_t)nc;
    return d;
}
static int16_t pat_value(const uint8_t *r, uint32_t i)   /* timing value i (0..3 LEN DIV SWING GATE, 4 QUANTIZE) */
{
    uint32_t id = i < 4u ? P_SLEN + i : P_SQNT;
    return (int16_t)clamp((int32_t)r[PAT_TIM_OFF + i] - 64, TP[id].min, TP[id].max);
}

static uint8_t *pat_rec(uint32_t k) { return proj_wire_u.raw + PAT_HDR + (k % PAT_PER_BANK) * PAT_REC; }
/* record k of the bank in proj_wire -> its mirror entry */
static void pat_mirror_of(uint32_t k)
{
    const uint8_t *r = pat_rec(k);
    pat_info_t *m = &pat_mirror[k];
    memset(m, 0, sizeof *m);
    if (r[0] != PAT_USED)
        return;
    m->used = 1;
    proj_name_get(m->name, r + PAT_NAME_OFF);
    m->len48 = (uint16_t)pat_len48(pat_value(r, 0), pat_value(r, 1));
    m->den = pat_density(r + PAT_STEP_OFF, (uint32_t)pat_value(r, 0), &m->cells);
}
/* bank b -> proj_wire; none stored, a damaged or another format: six empty records. 1 = read */
static int pat_bank_read(uint32_t b)
{
    uint8_t *w = proj_wire_u.raw;
    uint32_t m = 0;
    int n = -1;
    proj_wire_gen++;
#if FELUCCA_FLASH
    if (flash_ok)
        n = st_load(OBJ_PAT0 + b, w, sizeof proj_wire_u.raw);
#else
    memcpy(w, pat_ram[b], PAT_BANK_SIZE);
    n = (int)PAT_BANK_SIZE;
#endif
    if (n == (int)PAT_BANK_SIZE)
        memcpy(&m, w, 4);
    if (m == PAT_MAGIC && w[4] == PAT_PER_BANK && (w[6] | w[7] << 8) == PAT_REC)
        return 1;
    memset(w, 0, PAT_BANK_SIZE);
    m = PAT_MAGIC;
    memcpy(w, &m, 4);
    w[4] = PAT_PER_BANK;
    w[6] = (uint8_t)PAT_REC;
    w[7] = (uint8_t)(PAT_REC >> 8);
    return 0;
}
/* proj_wire -> bank b, then its mirror. 0 written, 2 failed (no flash, a write error: the bank as it was) */
static int pat_bank_write(uint32_t b)
{
    uint32_t i;
#if FELUCCA_FLASH
    if (flash_ok && !st_save(OBJ_PAT0 + b, proj_wire_u.raw, PAT_BANK_SIZE)) {
        for (i = 0; i < PAT_PER_BANK; i++)
            pat_mirror_of(b * PAT_PER_BANK + i);
        pat_gen++;
        return 0;
    }
    pat_bank_read(b);                            /* (the mirror stays the flash's) */
    return 2;
#else
    memcpy(pat_ram[b], proj_wire_u.raw, PAT_BANK_SIZE);
    for (i = 0; i < PAT_PER_BANK; i++)
        pat_mirror_of(b * PAT_PER_BANK + i);
    pat_gen++;
    return 0;
#endif
}
static void pat_bank_reload(uint32_t b)          /* bank b's mirror from the flash (a backup restored it) */
{
    uint32_t i;
    pat_bank_read(b);
    for (i = 0; i < PAT_PER_BANK; i++)
        pat_mirror_of(b * PAT_PER_BANK + i);
    pat_gen++;
}
static void pat_boot(void)                       /* power-on (project.c persist_boot): the mirror */
{
    uint32_t b;
    for (b = 0; b < PAT_BANKS; b++)
        pat_bank_reload(b);
}
/* a bank as a backup holds it (editor_backup.c, ids 2..5): the shape pat_bank_read takes, else refused */
static int pat_bank_ok(const uint8_t *w, uint32_t len)
{
    uint32_t m;
    if (len != PAT_BANK_SIZE)
        return 0;
    memcpy(&m, w, 4);
    return m == PAT_MAGIC && w[4] == PAT_PER_BANK && (w[6] | w[7] << 8) == PAT_REC;
}

static void pat_put_name(uint8_t *r, uint32_t k, const char *name)   /* empty: "PATTERN 01" */
{
    char d[13];
    uint32_t n;
    if (!name || !name[0])
        pat_default_name(d, k);
    else
        str_cpy(d, name, sizeof d);
    n = str_len(d);
    memset(r + PAT_NAME_OFF, 0, PROJ_NAME_LEN);
    memcpy(r + PAT_NAME_OFF, d, n < PROJ_NAME_LEN ? n : PROJ_NAME_LEN);
}
/* track t's sequence -> pattern k, named name (0 or "": the default). 0 saved, 1 refused (playing), 2 failed */
static int pat_store(uint32_t k, const track_t *t, const char *name)
{
    uint8_t *r;
    uint32_t i;
    if (k >= NPAT || transport_busy())           /* (a flash erase silences the audio: only while stopped) */
        return 1;
    pat_bank_read(k / PAT_PER_BANK);
    r = pat_rec(k);
    memset(r, 0, PAT_REC);
    r[0] = PAT_USED;
    pat_put_name(r, k, name);
    for (i = 0; i < 4u; i++)
        r[PAT_TIM_OFF + i] = (uint8_t)(motion_base_value(t, P_SLEN + i) + 64);
    r[PAT_TIM_OFF + 4u] = (uint8_t)(motion_base_value(t, P_SQNT) + 64);
    for (i = 0; i < NSTEP; i++) {
        step_t s = t->step[i];                   /* (inside the fields proj_step_pack takes, as proj_bound keeps them) */
        uint32_t j;
        if (s.n > 4u) s.n = 4;
        if (s.time > ST_REST) s.time = ST_REST;
        for (j = 0; j < 4u; j++) s.note[j] &= 127u;
        s.acc &= s.hit;
        s.flags &= 3u | SF_RATCH | SF_NUDGE;
        s.vel &= 127u;
        if (s.probability >= COND_FIRST + NCOND) s.probability = 0;
        proj_step_pack(r + PAT_STEP_OFF + i * PROJ_STEP_B, &s);
    }
    return pat_bank_write(k / PAT_PER_BANK);
}
/* pattern k renamed (nothing else of it changes) / erased (the slot empty). 0 done, 1 refused (playing, empty), 2 failed */
static int pat_rename(uint32_t k, const char *name)
{
    if (!pat_used(k) || transport_busy())
        return 1;
    pat_bank_read(k / PAT_PER_BANK);
    if (pat_rec(k)[0] != PAT_USED)
        return 1;
    pat_put_name(pat_rec(k), k, name);
    return pat_bank_write(k / PAT_PER_BANK);
}
static int pat_erase(uint32_t k)
{
    if (!pat_used(k) || transport_busy())
        return 1;
    pat_bank_read(k / PAT_PER_BANK);
    memset(pat_rec(k), 0, PAT_REC);
    return pat_bank_write(k / PAT_PER_BANK);
}

/* record r (in proj_wire) -> steps, LEN DIV SWING GATE, QUANTIZE; a DRUM track's (drum): the lanes' notes as its hits,
 * as a step written there would be. A step that does not decode: empty */
static void pat_decode(const uint8_t *r, step_t *step, int16_t *timing, int16_t *qnt, int drum)
{
    uint32_t i;
    for (i = 0; i < NSTEP; i++) {
        if (!proj_step_unpack(&step[i], r + PAT_STEP_OFF + i * PROJ_STEP_B))
            step_clear(&step[i]);
        else if (drum)
            step_to_grid(&step[i]);
    }
    for (i = 0; i < 4u; i++)
        timing[i] = pat_value(r, i);
    *qnt = pat_value(r, 4);
}
/* pattern k -> track t's steps and its LEN DIV SWING GATE QUANTIZE (the sound stays). 1 loaded */
static int pat_get(uint32_t k, track_t *t)
{
    if (!pat_used(k))
        return 0;
    pat_bank_read(k / PAT_PER_BANK);
    if (pat_rec(k)[0] != PAT_USED)
        return 0;
    fm1_irq_off();                               /* the audio ISR must not see half a sequence */
    pat_decode(pat_rec(k), t->step, &t->p[P_SLEN], &t->p[P_SQNT], drum_track(t));
    fm1_irq_on();
    return 1;
}

/* SONG's PLAY (main loop only: no flash access or copies when the ISR changes rows): every pattern the song names, once,
 * into its source. 0 armed; 1 no section; 2 busy; 3 more than CHAIN_SRC patterns; 4 + k pattern k not saved */
static uint32_t chain_prepare(void)
{
    uint32_t i, k, s, n = 0, b = 0xFFu;
    int16_t q;
    if (transport_busy())
        return 2;
    if (!chain_valid(&chain_config) || !chain_config.count)
        return 1;
    memset(chain.map, CHAIN_SILENT, sizeof chain.map);   /* (not running: transport_busy) */
    for (i = 0; i < chain_config.count; i++)
        for (k = 0; k < NTRK; k++) {
            s = chain_config.row[i].slot[k];
            if (s >= NPAT || chain.map[s] != CHAIN_SILENT)
                continue;
            if (!pat_used(s))
                return 4u + s;
            if (n >= CHAIN_SRC)
                return 3;
            chain.map[s] = (uint8_t)n++;
        }
    for (s = 0; s < NPAT; s++) {                 /* (in slot order: each bank read once) */
        chain_pattern_t *p;
        if (chain.map[s] >= CHAIN_SRC)
            continue;
        if (b != s / PAT_PER_BANK)
            pat_bank_read(b = s / PAT_PER_BANK);
        if (pat_rec(s)[0] != PAT_USED)
            return 4u + s;
        p = &chain.src[chain.map[s]];
        pat_decode(pat_rec(s), p->step, p->timing, &q, 0);   /* (the steps as saved: the track decides how a step plays) */
        p->qnt = q != 0;
    }
    chain.config = chain_config;
    RING_PUBLISH();
    chain.armed = 1;
    transport_req = 1;
    return 0;
}
