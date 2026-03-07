#pragma once
/* Signature/code dilution for driver binary. Reduces exact hash and byte-pattern match against
 * known cheat drivers (EAC and similar ACs scan for code signatures). Uses project-derived
 * constants and layout only – no copy of public "junk" implementations (e.g. no 0x11111111,
 * no JunkFunction1 naming, no fixed 1000-iteration loops). Include from driver.cpp only. */

#include "../flush_comm_config.h"
#include <ntifs.h>

/* Derived constants from MAGIC – no single literal (e.g. 0x11111111) as signature. */
#define _SD_V(n)  ((ULONG_PTR)((FLUSHCOMM_MAGIC >> ((n) * 4)) & 0xFFFFFFFFULL))
#define _SD_W(n)  ((ULONG_PTR)(((FLUSHCOMM_MAGIC >> (n)) ^ (FLUSHCOMM_MAGIC << (n))) & 0xFFFFFFFFULL))
#define _SD_CNT(n) (256u + (ULONG)((FLUSHCOMM_MAGIC >> (n)) & 0x1FFu))

namespace signature_dilution {

static ULONG_PTR _v1 = _SD_V(0);
static ULONG_PTR _v2 = _SD_V(1);
static ULONG_PTR _v3 = _SD_V(2);
static ULONG_PTR _v4 = _SD_W(3);
static ULONG_PTR _v5 = _SD_W(5);
static ULONG_PTR _v6 = _SD_W(7);
static ULONG_PTR _v7 = _SD_V(4);
static ULONG_PTR _v8 = _SD_V(6);
static ULONG_PTR _v9 = _SD_W(1);
static ULONG_PTR _v10 = _SD_W(9);

struct _blk1 { ULONG_PTR a, b, c, d, e; };
struct _blk2 { ULONG_PTR f, g, h, i, j, k; };
static _blk1 _b1 = { _SD_V(0), _SD_V(2), _SD_W(4), _SD_V(1), _SD_W(2) };
static _blk2 _b2 = { _SD_W(0), _SD_V(3), _SD_W(6), _SD_V(5), _SD_W(8), _SD_V(7) };

static ULONG_PTR _f1(void) {
    ULONG n = _SD_CNT(0);
    ULONG_PTR t = 0;
    for (ULONG i = 0; i < n; i++) t += _v1 + (ULONG_PTR)i;
    return t;
}
static ULONG_PTR _f2(void) {
    ULONG n = _SD_CNT(4);
    ULONG_PTR t = _v2;
    for (ULONG i = 0; i < n; i++) t ^= (i & 0xFF);
    return t;
}
static ULONG_PTR _f3(void) {
    ULONG n = _SD_CNT(8);
    for (ULONG i = 0; i < n; i++) _v3 += (i % 31);
    return _v3;
}
static ULONG_PTR _f4(void) {
    ULONG n = _SD_CNT(12);
    ULONG_PTR t = _b1.a + _b1.b;
    for (ULONG i = 0; i < n; i++) t = (t << 1) | (t >> 63);
    return t;
}
static ULONG_PTR _f5(void) {
    ULONG n = _SD_CNT(16);
    for (ULONG i = 0; i < n; i++) _b2.f += _v4;
    return _b2.f;
}
static ULONG_PTR _f6(void) {
    ULONG n = _SD_CNT(20);
    ULONG_PTR t = _v5 ^ _v6;
    for (ULONG i = 0; i < n; i++) t += _b1.c;
    return t;
}
static ULONG_PTR _f7(void) {
    ULONG n = _SD_CNT(24);
    for (ULONG i = 0; i < n; i++) _v7 = (_v7 * 31) + i;
    return _v7;
}
static ULONG_PTR _f8(void) {
    ULONG n = _SD_CNT(28);
    ULONG_PTR t = _v8;
    for (ULONG i = 0; i < n; i++) t -= _b2.g;
    return t;
}
static ULONG_PTR _f9(void) {
    ULONG n = _SD_CNT(32);
    for (ULONG i = 0; i < n; i++) _v9 = _v9 ^ (_v10 + i);
    return _v9;
}
static ULONG_PTR _f10(void) {
    ULONG n = _SD_CNT(36);
    ULONG_PTR t = _b1.d + _b1.e + _b2.h;
    for (ULONG i = 0; i < n; i++) t = t + (t >> 3);
    return t;
}
static ULONG_PTR _f11(void) {
    ULONG n = _SD_CNT(40);
    for (ULONG i = 0; i < n; i++) _b2.i ^= (ULONG_PTR)i;
    return _b2.i;
}
static ULONG_PTR _f12(void) {
    ULONG n = _SD_CNT(44);
    ULONG_PTR t = _v1;
    for (ULONG i = 0; i < n; i++) t += (ULONG_PTR)i + _b1.a;
    return t;
}
static ULONG_PTR _f13(void) {
    ULONG n = _SD_CNT(48);
    ULONG_PTR t = _v2 + _v3;
    for (ULONG i = 0; i < n; i++) t ^= (ULONG_PTR)i + _b1.b;
    return t;
}
static ULONG_PTR _f14(void) {
    ULONG n = _SD_CNT(52);
    for (ULONG i = 0; i < n; i++) _v4 = _v4 + _b2.j;
    return _v4;
}
static ULONG_PTR _f15(void) {
    ULONG n = _SD_CNT(56);
    ULONG_PTR t = _b2.k;
    for (ULONG i = 0; i < n; i++) t = (t * 9) + i;
    return t;
}

/* Single entry point: touch dilution data/code so linker keeps it; no observable effect.
 * Call once from DriverEntry to retain all dilution in the binary. */
inline void touch(void) {
    volatile ULONG_PTR s = 0;
    s += _v1 + _v2 + _v3 + _v4 + _v5 + _v6 + _v7 + _v8 + _v9 + _v10;
    s += _b1.a + _b1.b + _b1.c + _b2.f + _b2.g + _b2.h;
    s += _f1() + _f2() + _f3() + _f4() + _f5();
    s += _f6() + _f7() + _f8() + _f9() + _f10();
    s += _f11() + _f12() + _f13() + _f14() + _f15();
    (void)s;
}

} /* namespace signature_dilution */

#undef _SD_V
#undef _SD_W
#undef _SD_CNT
