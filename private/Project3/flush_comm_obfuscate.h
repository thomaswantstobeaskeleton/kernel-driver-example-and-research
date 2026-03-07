#pragma once
/* Compile-time obfuscation: key from FLUSHCOMM_OBF_BASE (MAGIC-derived) - no public 0x9D literal.
 * Strings L"SharedBuffer", L"SharedPid", L"HookedDevice" encoded at compile time. */

#ifdef __has_include
#  if __has_include("flush_comm_config.h")
#    include "flush_comm_config.h"
#  endif
#endif
#ifndef FLUSHCOMM_OBF_BASE
/* Fallback when config missing: build-time derived so no single 0x9D literal as project-wide signature. */
#  define FLUSHCOMM_OBF_BASE ((unsigned char)(0x80 + (((__TIME__[7]-'0')*10+(__TIME__[6]-'0')) % 64)))
#endif
#define OBF_KEY  FLUSHCOMM_OBF_BASE

#define _OBF_B(b, i) ((unsigned char)((b) ^ (OBF_KEY + ((i) & 0xF))))
/* L"SharedBuffer" = 12 wide + null (26 bytes LE) */
static const unsigned char OBF_SharedBuffer[] = {
    _OBF_B('S',0),_OBF_B(0,1),_OBF_B('h',2),_OBF_B(0,3),_OBF_B('a',4),_OBF_B(0,5),_OBF_B('r',6),_OBF_B(0,7),_OBF_B('e',8),_OBF_B(0,9),_OBF_B('d',10),_OBF_B(0,11),_OBF_B('B',12),_OBF_B(0,13),_OBF_B('u',14),_OBF_B(0,15),_OBF_B('f',16),_OBF_B(0,17),_OBF_B('f',18),_OBF_B(0,19),_OBF_B('e',20),_OBF_B(0,21),_OBF_B('r',22),_OBF_B(0,23),_OBF_B(0,24),_OBF_B(0,25)
};
#define OBF_SharedBuffer_LEN sizeof(OBF_SharedBuffer)
/* L"SharedPid" = 9 wide + null (20 bytes) */
static const unsigned char OBF_SharedPid[] = {
    _OBF_B('S',0),_OBF_B(0,1),_OBF_B('h',2),_OBF_B(0,3),_OBF_B('a',4),_OBF_B(0,5),_OBF_B('r',6),_OBF_B(0,7),_OBF_B('e',8),_OBF_B(0,9),_OBF_B('d',10),_OBF_B(0,11),_OBF_B('P',12),_OBF_B(0,13),_OBF_B('i',14),_OBF_B(0,15),_OBF_B('d',16),_OBF_B(0,17),_OBF_B(0,18),_OBF_B(0,19)
};
#define OBF_SharedPid_LEN sizeof(OBF_SharedPid)
/* L"HookedDevice" = 12 wide + null (26 bytes) */
static const unsigned char OBF_HookedDevice[] = {
    _OBF_B('H',0),_OBF_B(0,1),_OBF_B('o',2),_OBF_B(0,3),_OBF_B('o',4),_OBF_B(0,5),_OBF_B('k',6),_OBF_B(0,7),_OBF_B('e',8),_OBF_B(0,9),_OBF_B('d',10),_OBF_B(0,11),_OBF_B('D',12),_OBF_B(0,13),_OBF_B('e',14),_OBF_B(0,15),_OBF_B('v',16),_OBF_B(0,17),_OBF_B('i',18),_OBF_B(0,19),_OBF_B('c',20),_OBF_B(0,21),_OBF_B('e',22),_OBF_B(0,23),_OBF_B(0,24),_OBF_B(0,25)
};
#define OBF_HookedDevice_LEN sizeof(OBF_HookedDevice)
#undef _OBF_B

#ifdef __cplusplus
extern "C" {
#endif
static inline void obf_decode_str(const unsigned char* enc, size_t enc_len, void* out, size_t out_chars) {
    unsigned char* p = (unsigned char*)out;
    for (size_t i = 0; i < enc_len && i < (out_chars * sizeof(wchar_t)); i++)
        p[i] = (unsigned char)(enc[i] ^ (OBF_KEY + (unsigned char)(i & 0xF)));
}
#ifdef __cplusplus
}
#endif
