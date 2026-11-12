/* SPDX-License-Identifier: GPL-3.0-only
 * MRC: the phone remote (docs/editor/remote.html). */
/* REMOTE: the FX layer's effects, its macros, the solos, the track mutes and the DRUM lanes' mutes from a phone,
 * through the editor's SysEx (editor.c ED_PERFORM, cmd 82; the browser: web/emu/felucca_web.c web_remote).
 * Request: F0 7D 46 4C 52 op args.. F7. Every request answers with the state (rmt_state).
 *   op 0              query
 *   op 1 e down       hold (1) / let go (0) effect e (PF_*, below PF_M1): as a white key held with FX (perf_rmt)
 *   op 2 k v14        macro k (0 FILTER -100..100, 1 CRUSH, 2 THROW, 3 DEPTH 0..100): perf_k, as KNOB 1..4 with FX
 *   op 3 t on         solo of track t (perf_rsolo, OR'd with GLO's while held)
 *   op 4 t on         mute of track t (P_MUTE, saved with the project as GLO's black keys)
 *   op 5 t l on       mute of lane l of track t (eng_drum.c drum_mute, as EDIT's quick actions)
 *   op 6              let everything of the remote's go (its effects, its solos; the macros back to 0)
 *   op 7 t            the lanes' names as the track's KIT plays them (8 x 0-terminated, after the state)
 *   op 8 t            select track t (as KNOB 1 on TRACKS: keys, pages and the editor follow)
 *   op 9 k v14        EDIT value k (P_E0 + k) of the selected track: clamped, an F_ENUM past its aliases
 *   op 10             as op 0, and the sound block carries the 8 values' labels and ranges
 * After the state (and op 7's names), the sound block: the selected track, its engine, then per value v14 and its
 * text as the device shows it (0-terminated); op 10 then per value its label (0-terminated), min v14, max v14.
 * (At the end, so a page of before it reads the rest as it did.)
 * Nothing here is recorded or saved but the mutes. A remote that stops asking (RMT_ALIVE ms: the page asks
 * every 250 ms) lets its effects and solos go, so a pulled cable never leaves an effect held. */
#define RMT_ALIVE 1500u
static uint32_t rmt_ms;

static uint32_t rmt_put(uint8_t *o, uint32_t n, uint32_t max, uint32_t v)
{
    if (n < max)
        o[n] = (uint8_t)(v & 0x7Fu);
    return n + 1u;
}
static uint32_t rmt_put14(uint8_t *o, uint32_t n, uint32_t max, uint32_t v)   /* 14 bits, LSB first */
{
    n = rmt_put(o, n, max, v);
    return rmt_put(o, n, max, v >> 7);
}

static uint32_t rmt_str(uint8_t *o, uint32_t n, uint32_t max, const char *s)   /* 0-terminated */
{
    while (s && *s)
        n = rmt_put(o, n, max, (uint8_t)*s++);
    return rmt_put(o, n, max, 0);
}

static void rmt_release(void)
{
    perf_rmt = 0;
    perf_rsolo = 0;
}

/* main loop: a remote gone quiet lets go */
static void rmt_service(void)
{
    if ((perf_rmt | perf_rsolo) && fm1_ms - rmt_ms > RMT_ALIVE) {
        rmt_release();
        ui.force = 1;
    }
}

/* a: the bytes after the cmd; the reply's bytes after the cmd into o (at most max); returns their count, 0 = none */
static uint32_t rmt_handle(const uint8_t *a, uint32_t na, uint8_t *o, uint32_t max)
{
    uint32_t op, rc = 0, n = 0, t, l, fx = (1u << PF_NFX) - 1u;
    if (!na)
        return 0;
    op = a[0];
    rmt_ms = fm1_ms;
    if (op == 1u && na == 3u && a[1] < PF_NFX) {
        if (a[2]) {
            perf_ord[a[1]] = ++perf_seq;               /* (the last pressed buffer effect plays) */
            perf_rmt |= PF_BIT(a[1]);
        } else {
            perf_rmt &= ~PF_BIT(a[1]);
        }
    } else if (op == 2u && na == 4u && a[1] < 4u) {
        int32_t v = (int32_t)(a[2] | a[3] << 7) - 8192;
        perf_k[a[1]] = (int8_t)clamp(v, a[1] ? 0 : -100, 100);
    } else if (op == 3u && na == 3u && a[1] < NTRK) {
        perf_rsolo = (uint8_t)(a[2] ? perf_rsolo | 1u << a[1] : perf_rsolo & ~(1u << a[1]));
    } else if (op == 4u && na == 3u && a[1] < NTRK) {
        trk[a[1]].p[P_MUTE] = (int16_t)(a[2] != 0u);
    } else if (op == 5u && na == 4u && a[1] < NTRK && a[2] < NLANE) {
        t = a[1] % NPART;
        drum_mute[t] = (uint8_t)(a[3] ? drum_mute[t] | 1u << a[2] : drum_mute[t] & ~(1u << a[2]));
    } else if (op == 6u && na == 1u) {
        rmt_release();
        perf_k[0] = perf_k[1] = perf_k[2] = perf_k[3] = 0;
    } else if (op == 8u && na == 2u && a[1] < NTRK) {
        track_select(a[1]);
    } else if (op == 9u && na == 4u && a[1] < 8u) {
        uint32_t id = P_E0 + a[1];
        const param_desc_t *d = param_desc_of(eng_idx(TSEL->eng_req), id);
        int32_t v = clamp((int32_t)(a[2] | a[3] << 7) - 8192, d->min, d->max);
        if (d->max > d->min && !(chain_busy() && chain_owns(id))) {   /* (a fixed value stays, as the editor's SET) */
            if (d->fmt == F_ENUM)
                v = enum_orig(d, enum_step(d, TSEL->p[id], v));
            TSEL->p[id] = (int16_t)v;
            (void)motion_capture(TSEL, id, TSEL->p[id]);
        }
    } else if (!((op == 0u && na == 1u) || (op == 7u && na == 2u && a[1] < NTRK) || (op == 10u && na == 1u))) {
        rc = 1;
    }
    if (op && op != 7u && op != 10u && !rc)
        ui.force = 1;
    /* the state */
    n = rmt_put(o, n, max, op);
    n = rmt_put(o, n, max, rc);
    n = rmt_put(o, n, max, NTRK);
    n = rmt_put(o, n, max, PF_NFX);
    n = rmt_put14(o, n, max, perf_rmt & fx);                                      /* the remote's effects */
    n = rmt_put14(o, n, max, perf_kill ? 0u : (perf_held | perf_latched) & fx);  /* .. the device's keys' */
    n = rmt_put14(o, n, max, perf_act & fx);                                      /* .. sounding */
    n = rmt_put14(o, n, max, perf_avail() & fx);                                  /* .. possible at the tempo */
    for (l = 0; l < 4u; l++)
        n = rmt_put14(o, n, max, (uint32_t)(perf_k[l] + 8192));
    n = rmt_put(o, n, max, perf_rsolo);
    n = rmt_put(o, n, max, perf_solo);
    n = rmt_put(o, n, max, song.playing != 0u);
    n = rmt_put(o, n, max, perf_kill != 0u);
    for (t = 0; t < NTRK; t++) {
        n = rmt_put(o, n, max, trk[t].eng_req % NENGINES);
        n = rmt_put(o, n, max, (uint32_t)(trk[t].p[P_MUTE] != 0) | (uint32_t)drum_track(&trk[t]) << 1);
        n = rmt_put(o, n, max, drum_mute[t % NPART]);                            /* (8 lanes: bit 7 in the next) */
        n = rmt_put(o, n, max, drum_mute[t % NPART] >> 7);
    }
    if (op == 7u && !rc)
        for (l = 0; l < NLANE; l++) {
            const char *s = drum_lane_name(&trk[a[1]], l);
            while (*s)
                n = rmt_put(o, n, max, (uint8_t)*s++);
            n = rmt_put(o, n, max, 0);
        }
    /* the sound block: the selected track's EDIT values */
    n = rmt_put(o, n, max, song.sel);
    n = rmt_put(o, n, max, TSEL->eng_req % NENGINES);
    for (l = 0; l < 8u; l++) {
        const param_desc_t *d = track_desc(TSEL, P_E0 + l);
        const char *unit;
        char val[16];
        param_format(d, TSEL->p[P_E0 + l], val, &unit);
        n = rmt_put14(o, n, max, (uint32_t)(TSEL->p[P_E0 + l] + 8192));
        for (t = 0; val[t]; t++)
            n = rmt_put(o, n, max, (uint8_t)val[t]);
        if (*unit && *unit != '%')
            n = rmt_put(o, n, max, ' ');
        n = rmt_str(o, n, max, unit);
    }
    if (op == 10u && !rc)
        for (l = 0; l < 8u; l++) {
            const param_desc_t *d = track_desc(TSEL, P_E0 + l);
            n = rmt_str(o, n, max, d->max > d->min ? d->label : "");   /* (a fixed value: no label, not shown) */
            n = rmt_put14(o, n, max, (uint32_t)(d->min + 8192));
            n = rmt_put14(o, n, max, (uint32_t)(d->max + 8192));
        }
    return n <= max ? n : 0u;
}
