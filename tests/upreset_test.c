/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Host test of the user preset record (firmware/src/upreset.c, -DUP_HOST part):
 * UP_PUT parsing, a bank round trip through storage.c on a simulated NOR,
 * bank / record version checks, map-by-count, PHYS MODEL DRUM records -> the DRUM engine (MRC: UP_PUT carries no
 * pattern, a record's is left empty; records of upstream that hold one or a drum grid stay valid). */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define __attribute__(x)
#define UP_HOST 1
#include "../firmware/src/core.h"

static uint8_t nor[0x100000];
static int st_read(uint32_t off, void *dst, uint32_t n) { memcpy(dst, nor + off, n); return 0; }
static int st_erase(uint32_t off) { memset(nor + off, 0xFF, 4096); return 0; }
static int st_prog(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *s = src;
    uint32_t i;
    for (i = 0; i < n; i++)
        nor[off + i] &= s[i];
    return 0;
}
#include "../firmware/src/storage.c"
#include "../firmware/src/upreset.c"

static int check(const char *what, int ok)
{
    printf("%-46s %s\n", what, ok ? "ok" : "FAIL");
    return ok ? 0 : 1;
}

static uint32_t put_frame(uint8_t *a, uint32_t slot, uint32_t eng, const char *name, int32_t base)
{
    uint32_t n = 0, i;
    a[n++] = (uint8_t)slot;
    a[n++] = (uint8_t)eng;
    for (i = 0; name[i]; i++)
        a[n++] = (uint8_t)name[i];
    a[n++] = 0;
    for (i = 0; i < P_COUNT; i++) {
        uint32_t u = (uint32_t)(base + (int32_t)i + 8192);
        a[n++] = u & 127u;
        a[n++] = (u >> 7) & 127u;
    }
    return n;
}

int main(void)
{
    uint8_t a[640];
    up_rec_t r, got;
    uint32_t n, slot = 99, i;
    int bad = 0, len, ok;
    int16_t v[P_COUNT], def[P_COUNT];
    memset(nor, 0xFF, sizeof nor);

    n = put_frame(a, 5, 2, "Bass One", -40);
    bad += check("UP_PUT frame < 640 bytes", 5u + n + 1u < 640u);
    bad += check("UP_PUT parses", up_parse(a, n, &r, &slot) == 0 && slot == 5u && r.engine == 2u &&
                                      up_valid(&r) && !memcmp(r.name, "Bass One", 8) && !r.name[8]);
    ok = 1;
    for (i = 0; i < P_COUNT; i++)
        ok &= up_value(&r, i) == (int16_t)(-40 + (int32_t)i);
    bad += check("UP_PUT values (negative v14 too)", ok);
    ok = 1;
    for (i = 0; i < 16u; i++)
        ok &= !r.note[i] && !r.flags[i];
    bad += check("UP_PUT: no pattern, the record's empty (MRC)", ok);
    bad += check("UP_PUT short frame -> args", up_parse(a, n - 1u, &r, &slot) == 1);
    {
        up_rec_t keep = r;
        ok = 1;
        for (i = 0; i < n; i++)
            ok &= up_parse(a, i, &r, &slot) == 1 && !memcmp(&r, &keep, sizeof r);
        bad += check("every short prefix rejected without record changes", ok);
        a[n] = a[n + 1u] = 0;
        bad += check("a tail past the category rejected", up_parse(a, n + 2u, &r, &slot) == 1 &&
                                                       !memcmp(&r, &keep, sizeof r));
    }
    n = put_frame(a, 32, 0, "X", 0);
    bad += check("UP_PUT slot 32 -> args", up_parse(a, n, &r, &slot) == 1);
    n = put_frame(a, 0, NENGINES, "X", 0);
    bad += check("UP_PUT bad engine -> args", up_parse(a, n, &r, &slot) == 1);
    n = put_frame(a, 3, ENGI_DIGITAL, "OLD FM", 0);   /* (DIGITAL, retired: a record keeps it, its load converts) */
    bad += check("UP_PUT engine 1 (DIGITAL): kept as it is", up_parse(a, n, &r, &slot) == 0 && slot == 3u &&
                                                        r.engine == ENGI_DIGITAL && up_valid(&r));
    up_migrate(&r);
    bad += check("  a bank read keeps it engine 1 (its values DIGITAL's)", r.engine == ENGI_DIGITAL && up_value(&r, P_E4) ==
                                                                       (int16_t)P_E4);
    n = put_frame(a, 0, 0, "", 0);
    bad += check("UP_PUT empty name -> args", up_parse(a, n, &r, &slot) == 1);
    n = put_frame(a, 0, 0, "THIRTEEN CHRS", 0);
    bad += check("UP_PUT 13-char name -> args", up_parse(a, n, &r, &slot) == 1);
    n = put_frame(a, 0, 0, "TWELVE CHARS", 0);
    bad += check("UP_PUT 12-char name ok", up_parse(a, n, &r, &slot) == 0 && !memcmp(r.name, "TWELVE CHARS", 12));
    {
        char nm[13];
        *up_rec(31) = r;
        memcpy(up_rec(31)->name, "Low case", 9);
        up_name(31, nm);
        bad += check("name shown upper case", !strcmp(nm, "LOW CASE"));
    }

    /* bank round trip through storage.c */
    n = put_frame(a, 17, 3, "Keys", 7);
    up_parse(a, n, &r, &slot);
    up_bank[1].magic = UP_BANK_MAGIC;
    up_bank[1].rsize = sizeof(up_rec_t);
    up_bank[1].nslot = UP_PER_BANK;
    *up_rec(17) = r;
    bad += check("bank fits one object", sizeof(up_bank_t) <= ST_PAYLOAD_MAX);
    bad += check("bank save", st_save(OBJ_UPRESET0 + 1, &up_bank[1], sizeof up_bank[1]) == 0);
    memset(up_bank, 0, sizeof up_bank);
    len = st_load(OBJ_UPRESET0 + 1, &up_bank[1], sizeof up_bank[1]);
    up_bank_check(1, len);
    got = *up_rec(17);
    bad += check("bank load: the record is back", up_used(17) && !memcmp(&got, &r, sizeof r));
    bad += check("other slots empty", !up_used(16) && !up_used(18) && !up_used(0));
    len = st_load(OBJ_UPRESET0, &up_bank[0], sizeof up_bank[0]);
    up_bank_check(0, len);
    bad += check("bank 0 never written -> empty", len < 0 && !up_used(0) && up_bank[0].magic == 0);
    bad += check("banks in 0xDC000..0xDFFFF", st_sector(OBJ_UPRESET0, 0) == 0xDC000u &&
                                                   st_sector(OBJ_UPRESET0 + 1, 1) == 0xDF000u &&
                                                   st_sector(OBJ_PAT0 + 3, 1) + 4096u <= 0xA0000u);
    up_bank[1].rsize = 190;                                 /* another record layout */
    up_bank_check(1, (int)sizeof up_bank[1]);
    bad += check("bank with another record size -> empty", !up_used(17));
    up_bank[1].magic = UP_BANK_MAGIC;
    up_bank[1].rsize = sizeof(up_rec_t);
    up_bank[1].nslot = UP_PER_BANK;
    *up_rec(17) = r;
    up_rec(17)->ver = UP_VER_GRID + 1u;
    bad += check("record with another version -> empty", !up_used(17));
    up_rec(17)->ver = UP_VER_GRID;
    bad += check("a version 3 record (a drum grid) is used", up_used(17));

    /* PHYS MODEL DRUM (before 1.0) -> the DRUM engine: from flash (a record of an older layout too), by UP_PUT */
    {
        static const int16_t OLD[8] = {4, 70, 80, 60, 50, 110, 100, 70}, NEW[8] = {2, 70, 80, 60, 50, 110, 1, 0};
        up_rec_t d = r, m = r, o = r;
        uint32_t k;
        d.engine = ENGI_PHYS;
        m.engine = ENGI_PHYS;                               /* MEMB: stays PHYS */
        o.engine = ENGI_PHYS;
        o.np = P_COUNT - 2u;                                /* an older layout: E0 at np - 8 */
        for (k = 0; k < 8u; k++) {
            up_set_value(&d, P_E0 + k, OLD[k]);
            up_set_value(&m, P_E0 + k, (int16_t)(k ? 9 : 2));
            up_set_value(&o, o.np - 8u + k, OLD[k]);
        }
        *up_rec(16) = d;
        *up_rec(17) = m;
        *up_rec(18) = o;
        st_save(OBJ_UPRESET0 + 1, &up_bank[1], sizeof up_bank[1]);
        memset(up_bank, 0, sizeof up_bank);
        up_bank_check(1, st_load(OBJ_UPRESET0 + 1, &up_bank[1], sizeof up_bank[1]));
        ok = up_used(16) && up_rec(16)->engine == ENGI_DRUM && up_used(17) && up_rec(17)->engine == ENGI_PHYS &&
             !memcmp(up_rec(17)->p, m.p, sizeof m.p) && up_used(18) && up_rec(18)->engine == ENGI_DRUM &&
             up_rec(16)->ver == UP_VER;
        for (k = 0; k < 8u; k++)
            ok &= up_value(up_rec(16), P_E0 + k) == NEW[k] && up_value(up_rec(18), o.np - 8u + k) == NEW[k];
        bad += check("bank load: PHYS DRUM -> DRUM engine, MEMB kept", ok);
        n = put_frame(a, 3, ENGI_PHYS, "Old kit", 0);
        for (k = 0; k < 8u; k++) {                          /* E0..E7 of the frame: OLD */
            uint32_t at = 3u + 7u + 1u + 2u * (P_E0 + k), u = (uint32_t)(OLD[k] + 8192);
            a[at - 1u] = u & 127u;
            a[at] = (u >> 7) & 127u;
        }
        ok = up_parse(a, n, &got, &slot) == 0 && got.engine == ENGI_DRUM;
        for (k = 0; k < 8u; k++)
            ok &= up_value(&got, P_E0 + k) == NEW[k];
        bad += check("UP_PUT of PHYS DRUM -> DRUM engine", ok);
    }

    /* SAMPLE SET 4 (PERC, the GM kit, retired after 1.0.2) -> the DRUM engine with its default kit: from flash (an
     * older layout too), by UP_PUT; the rest of the sound and the pattern as stored; another SET stays SAMPLE */
    {
        static const int16_t KIT[8] = DRUM_KIT_E;
        up_rec_t d = r, m = r, o = r, before;
        uint32_t k;
        d.engine = m.engine = o.engine = ENGI_SAMPLE;
        o.np = P_COUNT - 2u;                                /* an older layout: E0 at np - 8 */
        for (k = 0; k < 8u; k++) {
            up_set_value(&d, P_E0 + k, (int16_t)(k ? 30 + k : SMP_SET_PERC));
            up_set_value(&m, P_E0 + k, (int16_t)(k ? 30 + k : 2));   /* FLUTE: stays SAMPLE */
            up_set_value(&o, o.np - 8u + k, (int16_t)(k ? 30 + k : SMP_SET_PERC));
        }
        *up_rec(19) = d;
        *up_rec(20) = m;
        *up_rec(21) = o;
        st_save(OBJ_UPRESET0 + 1, &up_bank[1], sizeof up_bank[1]);
        memset(up_bank, 0, sizeof up_bank);
        up_bank_check(1, st_load(OBJ_UPRESET0 + 1, &up_bank[1], sizeof up_bank[1]));
        ok = up_used(19) && up_rec(19)->engine == ENGI_DRUM && up_used(20) && up_rec(20)->engine == ENGI_SAMPLE &&
             !memcmp(up_rec(20)->p, m.p, sizeof m.p) && up_used(21) && up_rec(21)->engine == ENGI_DRUM &&
             !memcmp(up_rec(19)->note, d.note, sizeof d.note) && !memcmp(up_rec(19)->flags, d.flags, sizeof d.flags);
        for (k = 0; k < 8u; k++)
            ok &= up_value(up_rec(19), P_E0 + k) == KIT[k] && up_value(up_rec(21), o.np - 8u + k) == KIT[k];
        for (k = 0; k < P_E0; k++)
            ok &= up_value(up_rec(19), k) == up_value(&d, k);
        bad += check("bank load: SAMPLE PERC -> DRUM kit, FLUTE kept", ok);
        before = *up_rec(19);
        up_migrate(up_rec(19));
        bad += check("  migrated again: as it is", !memcmp(up_rec(19), &before, sizeof before));
        n = put_frame(a, 3, ENGI_SAMPLE, "Old perc", 0);
        {
            uint32_t at = 3u + 8u + 1u + 2u * P_E0, u = (uint32_t)(SMP_SET_PERC + 8192);   /* E0 of the frame: SET 4 */
            a[at - 1u] = u & 127u;
            a[at] = (u >> 7) & 127u;
        }
        ok = up_parse(a, n, &got, &slot) == 0 && got.engine == ENGI_DRUM;
        for (k = 0; k < 8u; k++)
            ok &= up_value(&got, P_E0 + k) == KIT[k];
        bad += check("UP_PUT of SAMPLE PERC -> DRUM kit", ok);
    }

    /* map by count: a record from a build with 2 parameters fewer */
    for (i = 0; i < P_COUNT; i++)
        def[i] = (int16_t)(1000 + i);
    r.np = P_COUNT - 2u;
    for (i = 0; i < P_COUNT; i++)
        up_set_value(&r, i, (int16_t)i);
    up_params(&r, v, def);
    ok = 1;
    for (i = 0; i < P_E0; i++)
        ok &= v[i] == (i < P_E0 - 2u ? (int16_t)i : def[i]);
    for (i = 0; i < 8u; i++)
        ok &= v[P_E0 + i] == (int16_t)(P_E0 - 2u + i);
    bad += check("np < P_COUNT: mapped by count", ok);
    /* a record saved before the SLICER (P_COUNT 53, P_E0 45): the four SLICER parameters (just
     * before P_E0) take their defaults, everything else keeps its id */
    r.ver = 2;
    r.np = 53;
    for (i = 0; i < 53u; i++)
        r.p[i] = (int16_t)(2000 + i);
    up_params(&r, v, def);
    ok = P_SLCR == 45 && P_SLDEPTH + 1 == P_M1SRC && P_M4AMT + 1 == P_FM1_ATK && P_FM4_LEVEL + 1 == P_CHRD &&
         P_VOIC + 1 == P_LN0 && P_LN7 + 1 == P_LSYNC && P_SQNT + 1 == P_SPRD && P_SPRD + 1 == P_ITYPE &&
         P_IMIX + 1 == P_FTYPE && P_ESYNC + 1 == P_TLC && P_TRES + 1 == P_E0 && P_E0 == 106 && P_COUNT == 114;
    for (i = 0; i < 45u; i++)
        ok &= v[i] == (int16_t)(2000 + i);
    for (i = P_SLCR; i < P_E0; i++)
        ok &= v[i] == def[i];
    for (i = 0; i < 8u; i++)
        ok &= v[P_E0 + i] == (int16_t)(2000 + 45 + i);
    bad += check("old record (np 53): SLICER and matrix defaults, E0..E7 kept", ok);
    /* a record saved before the modulation matrix (P_COUNT 57, P_E0 49): the twelve matrix parameters (just
     * before P_E0) take their defaults (every slot OFF), the SLICER and everything else keep their ids */
    r.np = 57;
    for (i = 0; i < 57u; i++)
        r.p[i] = (int16_t)(3000 + i);
    up_params(&r, v, def);
    ok = 1;
    for (i = 0; i < 49u; i++)
        ok &= v[i] == (int16_t)(3000 + i);
    for (i = P_M1SRC; i <= P_M4AMT; i++)
        ok &= v[i] == def[i];
    for (i = 0; i < 8u; i++)
        ok &= v[P_E0 + i] == (int16_t)(3000 + 49 + i);
    bad += check("old record (np 57): SLICER kept, matrix defaults, E0..E7 kept", ok);
    /* records of versions 4 (packed bytes) and 5 (a drum grid) saved before the DRUM lane levels (1.0.x: P_COUNT 91,
     * P_E0 83), before the chord keys (89, P_E0 81) and before the FM operator ENVs (69, P_E0 61): their engine
     * values land on today's E0..E7 (91..98), the lane levels (100 %) and the chord keys their defaults (OFF, CLOSE), the
     * operator parameters too for 69 */
    {
        static const uint8_t VERS[2] = {UP_VER, UP_VER_GRID}, NPS[3] = {91, 89, 69};
        uint32_t a, b;
        ok = 1;
        for (a = 0; a < 2u; a++)
            for (b = 0; b < 3u; b++) {
                uint32_t np = NPS[b];
                up_rec_t o;
                memset(&o, 0, sizeof o);
                o.used = UP_USED; o.ver = VERS[a]; o.engine = 1; o.np = (uint8_t)np;
                memcpy(o.name, "OLD", 3);
                for (i = 0; i < np; i++)
                    up_set_value(&o, i, (int16_t)(i % 100u - 30));
                ok &= up_valid(&o);
                up_params(&o, v, def);
                for (i = 0; i < np - 8u; i++)
                    ok &= v[i] == (int16_t)(i % 100u - 30);
                for (i = np - 8u; i < P_E0; i++)
                    ok &= v[i] == def[i];
                for (i = 0; i < 8u; i++)
                    ok &= v[P_E0 + i] == (int16_t)((np - 8u + i) % 100u - 30);
            }
        bad += check("v4 / v5 records of 91, 89 and 69 parameters: E0..E7 at P_E0, lane levels and chord keys defaults", ok);
    }
    r.ver = UP_VER;
    r.np = P_COUNT;
    for (i = 0; i < P_COUNT; i++)
        up_set_value(&r, i, (int16_t)i);
    up_params(&r, v, def);
    ok = 1;
    for (i = 0; i < P_COUNT; i++)
        ok &= v[i] == (int16_t)i;
    bad += check("np == P_COUNT: as stored", ok);

    {   /* 1.2 (Discussion #90): the category, the record's last byte (UP_CAT_AT), outside its values; UP_PUT's optional
         * last byte; through a bank's flash round trip; a record of before reads its engine's; versions 2 / 3 converted */
        n = put_frame(a, 17, 0, "CAT", 0);
        a[n++] = CAT_KEYS;
        ok = !up_parse(a, n, &r, &slot) && up_cat_of(&r) == CAT_KEYS && UP_CAT_AT >= P_COUNT && r.np == P_COUNT;
        a[n - 1u] = CAT_N;
        ok &= up_parse(a, n, &got, &slot) == 1;
        n = put_frame(a, 17, ENGI_DRUM, "KIT", 0);
        ok &= !up_parse(a, n, &got, &slot) && up_cat_of(&got) == CAT_DRUM && !got.packed[UP_CAT_AT];
        bad += check("category: UP_PUT's last byte, none = the engine's", ok);
        memset(&up_bank[1], 0, sizeof up_bank[1]);
        up_bank[1].magic = UP_BANK_MAGIC; up_bank[1].rsize = sizeof(up_rec_t); up_bank[1].nslot = UP_PER_BANK;
        up_bank[1].r[1] = r;
        st_save(OBJ_UPRESET0 + 1, &up_bank[1], sizeof up_bank[1]);
        memset(&up_bank[1], 0, sizeof up_bank[1]);
        up_bank_check(1, st_load(OBJ_UPRESET0 + 1, &up_bank[1], sizeof up_bank[1]));
        bad += check("category: kept through the bank on flash", up_used(17) && up_cat_of(up_rec(17)) == CAT_KEYS);
        memset(&r, 0, sizeof r);
        r.used = UP_USED; r.ver = 3; r.engine = ENGI_DRUM; r.np = 60;
        memcpy(r.name, "GRID", 4);
        for (i = 0; i < 60u; i++) r.p[i] = (int16_t)(i * 3u) - 64;
        r.note[0] = 0x81; r.flags[0] = 0x80;
        got = r;
        up_set_cat(&got, CAT_FX);
        ok = got.ver == UP_VER_GRID && up_cat_of(&got) == CAT_FX && got.note[0] == 0x81 && up_valid(&got);
        for (i = 0; i < 60u; i++) ok &= up_value(&got, i) == r.p[i];
        r.p[7] = 200;                                   /* a value a byte cannot hold: stays version 3, no category */
        got = r;
        up_set_cat(&got, CAT_FX);
        bad += check("category: a version 3 grid -> 5 with it, values kept; one that cannot be packed keeps none",
                     ok && got.ver == 3u && up_cat_of(&got) == CAT_DRUM && !memcmp(&got, &r, sizeof r));
    }
    printf("%s\n", bad ? "USER PRESET TEST FAILED" : "user preset test passed");
    return bad != 0;
}
