/* SPDX-License-Identifier: GPL-3.0-only */
/* MRC: 64-bit division for the device build. The firmware links no compiler runtime, and the compiler calls
 * __udivdi3 / __divdi3 for a 64-bit quotient it cannot narrow (the drawings' in ui_graph.c: main loop only).
 * Included by felucca.c alone: the host tests and the emulator have their compiler's own.
 * One bit a turn, shifts by one only (a variable 64-bit shift would be a runtime call of its own); a quotient
 * of two 32-bit values takes the CPU's divide. A zero divisor gives 0. */
uint64_t __udivdi3(uint64_t n, uint64_t d)
{
    uint64_t q = 0, r = 0;
    uint32_t i = 64u;
    if (!d)
        return 0;
    if (!(n >> 32)) {
        if (!(d >> 32))
            return (uint32_t)n / (uint32_t)d;
        return 0;                                    /* (d > n) */
    }
    for (; i; i--) {
        uint32_t c = (uint32_t)(r >> 63);            /* (the bit shifted out: d above 2^63) */
        r = (r << 1) | (n >> 63);
        n <<= 1;
        q <<= 1;
        if (c || r >= d) {
            r -= d;
            q |= 1u;
        }
    }
    return q;
}
int64_t __divdi3(int64_t n, int64_t d)
{
    uint64_t q = __udivdi3(n < 0 ? 0u - (uint64_t)n : (uint64_t)n, d < 0 ? 0u - (uint64_t)d : (uint64_t)d);
    return (n < 0) != (d < 0) ? (int64_t)(0u - q) : (int64_t)q;
}
