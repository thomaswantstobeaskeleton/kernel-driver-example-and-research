#pragma once
/* Kernel routine name obfuscation - no literal "MmCopyVirtualMemory", "ObCreateObject", etc. in .rdata.
 * Key from FLUSHCOMM_OBF_BASE (MAGIC-derived). Decode at runtime before MmGetSystemRoutineAddress.
 * Win11 23H2 compatible - same API. */

#include "includes.hpp"
#include "../flush_comm_config.h"
#include "../flush_comm_obfuscate.h"

#define ROUTINE_OBF_KEY  (FLUSHCOMM_OBF_BASE)
#define _ROB(c, i)  ((UCHAR)((c) ^ (ROUTINE_OBF_KEY + ((i) & 0xF))))

/* Encoded narrow (ASCII) routine names - decode to WCHAR for UNICODE_STRING */
static const UCHAR OBF_MmMapIoSpaceEx[]    = { _ROB('M',0),_ROB('m',1),_ROB('M',2),_ROB('a',3),_ROB('p',4),_ROB('I',5),_ROB('o',6),_ROB('S',7),_ROB('p',8),_ROB('a',9),_ROB('c',10),_ROB('e',11),_ROB('E',12),_ROB('x',13),0 };
static const UCHAR OBF_MmUnmapIoSpace[]   = { _ROB('M',0),_ROB('m',1),_ROB('U',2),_ROB('n',3),_ROB('m',4),_ROB('a',5),_ROB('p',6),_ROB('I',7),_ROB('o',8),_ROB('S',9),_ROB('p',10),_ROB('a',11),_ROB('c',12),_ROB('e',13),0 };
static const UCHAR OBF_MmCopyVirtualMemory[] = { _ROB('M',0),_ROB('m',1),_ROB('C',2),_ROB('o',3),_ROB('p',4),_ROB('y',5),_ROB('V',6),_ROB('i',7),_ROB('r',8),_ROB('t',9),_ROB('u',10),_ROB('a',11),_ROB('l',12),_ROB('M',13),_ROB('e',14),_ROB('m',15),_ROB('o',16),_ROB('r',17),_ROB('y',18),0 };
static const UCHAR OBF_IoCompleteRequest[] = { _ROB('I',0),_ROB('o',1),_ROB('C',2),_ROB('o',3),_ROB('m',4),_ROB('p',5),_ROB('l',6),_ROB('e',7),_ROB('t',8),_ROB('e',9),_ROB('R',10),_ROB('e',11),_ROB('q',12),_ROB('u',13),_ROB('e',14),_ROB('s',15),_ROB('t',16),0 };
static const UCHAR OBF_ObCreateObject[]    = { _ROB('O',0),_ROB('b',1),_ROB('C',2),_ROB('r',3),_ROB('e',4),_ROB('a',5),_ROB('t',6),_ROB('e',7),_ROB('O',8),_ROB('b',9),_ROB('j',10),_ROB('e',11),_ROB('c',12),_ROB('t',13),0 };
static const UCHAR OBF_ObInsertObject[]    = { _ROB('O',0),_ROB('b',1),_ROB('I',2),_ROB('n',3),_ROB('s',4),_ROB('e',5),_ROB('r',6),_ROB('t',7),_ROB('O',8),_ROB('b',9),_ROB('j',10),_ROB('e',11),_ROB('c',12),_ROB('t',13),0 };
static const UCHAR OBF_ObMakeTemporaryObject[] = { _ROB('O',0),_ROB('b',1),_ROB('M',2),_ROB('a',3),_ROB('k',4),_ROB('e',5),_ROB('T',6),_ROB('e',7),_ROB('m',8),_ROB('p',9),_ROB('o',10),_ROB('r',11),_ROB('a',12),_ROB('r',13),_ROB('y',14),_ROB('O',15),_ROB('b',16),_ROB('j',17),_ROB('e',18),_ROB('c',19),_ROB('t',20),0 };
static const UCHAR OBF_HalPrivateDispatchTable[] = { _ROB('H',0),_ROB('a',1),_ROB('l',2),_ROB('P',3),_ROB('r',4),_ROB('i',5),_ROB('v',6),_ROB('a',7),_ROB('t',8),_ROB('e',9),_ROB('D',10),_ROB('i',11),_ROB('s',12),_ROB('p',13),_ROB('a',14),_ROB('t',15),_ROB('c',16),_ROB('h',17),_ROB('T',18),_ROB('a',19),_ROB('b',20),_ROB('l',21),_ROB('e',22),0 };
static const UCHAR OBF_Driver[]            = { _ROB('D',0),_ROB('r',1),_ROB('i',2),_ROB('v',3),_ROB('e',4),_ROB('r',5),0 };
static const UCHAR OBF_SystemRoot[]        = { _ROB('S',0),_ROB('y',1),_ROB('s',2),_ROB('t',3),_ROB('e',4),_ROB('m',5),_ROB('R',6),_ROB('o',7),_ROB('o',8),_ROB('t',9),0 };
static const UCHAR OBF_LargePageDrivers[] = { _ROB('L',0),_ROB('a',1),_ROB('r',2),_ROB('g',3),_ROB('e',4),_ROB('P',5),_ROB('a',6),_ROB('g',7),_ROB('e',8),_ROB('D',9),_ROB('r',10),_ROB('i',11),_ROB('v',12),_ROB('e',13),_ROB('r',14),_ROB('s',15),0 };
/* Pool tag comparison - no literal "TnoC" in .rdata */
static const UCHAR OBF_TagTnoC[]           = { _ROB('T',0),_ROB('n',1),_ROB('o',2),_ROB('C',3),0 };
/* \Driver\IDE - for LoadDriverIntoSignedMemory */
static const UCHAR OBF_DriverIDE[]         = { _ROB('\\',0),_ROB('D',1),_ROB('r',2),_ROB('i',3),_ROB('v',4),_ROB('e',5),_ROB('r',6),_ROB('\\',7),_ROB('I',8),_ROB('D',9),_ROB('E',10),0 };
/* LPG registry path - no literal in .rdata */
static const UCHAR OBF_LPG_RegPath[]       = { _ROB('\\',0),_ROB('R',1),_ROB('e',2),_ROB('g',3),_ROB('i',4),_ROB('s',5),_ROB('t',6),_ROB('r',7),_ROB('y',8),_ROB('\\',9),_ROB('M',10),_ROB('a',11),_ROB('c',12),_ROB('h',13),_ROB('i',14),_ROB('n',15),_ROB('e',16),_ROB('\\',17),_ROB('S',18),_ROB('Y',19),_ROB('S',20),_ROB('T',21),_ROB('E',22),_ROB('M',23),_ROB('\\',24),_ROB('C',25),_ROB('u',26),_ROB('r',27),_ROB('r',28),_ROB('e',29),_ROB('n',30),_ROB('t',31),_ROB('C',32),_ROB('o',33),_ROB('n',34),_ROB('t',35),_ROB('r',36),_ROB('o',37),_ROB('l',38),_ROB('S',39),_ROB('e',40),_ROB('t',41),_ROB('\\',42),_ROB('C',43),_ROB('o',44),_ROB('n',45),_ROB('t',46),_ROB('r',47),_ROB('o',48),_ROB('l',49),_ROB('\\',50),_ROB('S',51),_ROB('e',52),_ROB('s',53),_ROB('s',54),_ROB('i',55),_ROB('o',56),_ROB('n',57),_ROB(' ',58),_ROB('M',59),_ROB('a',60),_ROB('n',61),_ROB('a',62),_ROB('g',63),_ROB('e',64),_ROB('r',65),_ROB('\\',66),_ROB('M',67),_ROB('e',68),_ROB('m',69),_ROB('o',70),_ROB('r',71),_ROB('y',72),_ROB(' ',73),_ROB('M',74),_ROB('a',75),_ROB('n',76),_ROB('a',77),_ROB('g',78),_ROB('e',79),_ROB('m',80),_ROB('e',81),_ROB('n',82),_ROB('t',83),0 };
/* WSK - no literal WskRegister etc. in .rdata */
static const UCHAR OBF_WskRegister[]       = { _ROB('W',0),_ROB('s',1),_ROB('k',2),_ROB('R',3),_ROB('e',4),_ROB('g',5),_ROB('i',6),_ROB('s',7),_ROB('t',8),_ROB('e',9),_ROB('r',10),0 };
static const UCHAR OBF_WskDeregister[]     = { _ROB('W',0),_ROB('s',1),_ROB('k',2),_ROB('D',3),_ROB('e',4),_ROB('r',5),_ROB('e',6),_ROB('g',7),_ROB('i',8),_ROB('s',9),_ROB('t',10),_ROB('e',11),_ROB('r',12),0 };
static const UCHAR OBF_WskCaptureProviderNPI[] = { _ROB('W',0),_ROB('s',1),_ROB('k',2),_ROB('C',3),_ROB('a',4),_ROB('p',5),_ROB('t',6),_ROB('u',7),_ROB('r',8),_ROB('e',9),_ROB('P',10),_ROB('r',11),_ROB('o',12),_ROB('v',13),_ROB('i',14),_ROB('d',15),_ROB('e',16),_ROB('r',17),_ROB('N',18),_ROB('P',19),_ROB('I',20),0 };
static const UCHAR OBF_WskReleaseProviderNPI[]  = { _ROB('W',0),_ROB('s',1),_ROB('k',2),_ROB('R',3),_ROB('e',4),_ROB('l',5),_ROB('e',6),_ROB('a',7),_ROB('s',8),_ROB('e',9),_ROB('P',10),_ROB('r',11),_ROB('o',12),_ROB('v',13),_ROB('i',14),_ROB('d',15),_ROB('e',16),_ROB('r',17),_ROB('N',18),_ROB('P',19),_ROB('I',20),0 };
static const UCHAR OBF_WskPort[]                = { _ROB('W',0),_ROB('s',1),_ROB('k',2),_ROB('P',3),_ROB('o',4),_ROB('r',5),_ROB('t',6),0 };

/* Resolved at runtime - no IAT import for EAC hook bypass */
static const UCHAR OBF_PsLookupProcessByProcessId[] = { _ROB('P',0),_ROB('s',1),_ROB('L',2),_ROB('o',3),_ROB('o',4),_ROB('k',5),_ROB('u',6),_ROB('p',7),_ROB('P',8),_ROB('r',9),_ROB('o',10),_ROB('c',11),_ROB('e',12),_ROB('s',13),_ROB('s',14),_ROB('B',15),_ROB('y',16),_ROB('P',17),_ROB('r',18),_ROB('o',19),_ROB('c',20),_ROB('e',21),_ROB('s',22),_ROB('s',23),_ROB('I',24),_ROB('d',25),0 };
/* ZwQuerySystemInformation - no IAT for BigPool/other queries */
static const UCHAR OBF_ZwQuerySystemInformation[]  = { _ROB('Z',0),_ROB('w',1),_ROB('Q',2),_ROB('u',3),_ROB('e',4),_ROB('r',5),_ROB('y',6),_ROB('S',7),_ROB('y',8),_ROB('s',9),_ROB('t',10),_ROB('e',11),_ROB('m',12),_ROB('I',13),_ROB('n',14),_ROB('f',15),_ROB('o',16),_ROB('r',17),_ROB('m',18),_ROB('a',19),_ROB('t',20),_ROB('i',21),_ROB('o',22),_ROB('n',23),0 };
/* ObReferenceObjectByName target - no literal "ntoskrnl.exe" in .rdata */
static const UCHAR OBF_NtoskrnlExe[]           = { _ROB('n',0),_ROB('t',1),_ROB('o',2),_ROB('s',3),_ROB('k',4),_ROB('r',5),_ROB('n',6),_ROB('l',7),_ROB('.',8),_ROB('e',9),_ROB('x',10),_ROB('e',11),0 };
/* Registry path for SystemRoot (CurrentVersion) - no literal in .rdata */
static const UCHAR OBF_RegPathCurrentVersion[] = { _ROB('\\',0),_ROB('R',1),_ROB('e',2),_ROB('g',3),_ROB('i',4),_ROB('s',5),_ROB('t',6),_ROB('r',7),_ROB('y',8),_ROB('\\',9),_ROB('M',10),_ROB('a',11),_ROB('c',12),_ROB('h',13),_ROB('i',14),_ROB('n',15),_ROB('e',16),_ROB('\\',17),_ROB('S',18),_ROB('O',19),_ROB('F',20),_ROB('T',21),_ROB('W',22),_ROB('A',23),_ROB('R',24),_ROB('E',25),_ROB('\\',26),_ROB('M',27),_ROB('i',28),_ROB('c',29),_ROB('r',30),_ROB('o',31),_ROB('s',32),_ROB('o',33),_ROB('f',34),_ROB('t',35),_ROB('\\',36),_ROB('W',37),_ROB('i',38),_ROB('n',39),_ROB('d',40),_ROB('o',41),_ROB('w',42),_ROB('s',43),_ROB(' ',44),_ROB('N',45),_ROB('T',46),_ROB('\\',47),_ROB('C',48),_ROB('u',49),_ROB('r',50),_ROB('r',51),_ROB('e',52),_ROB('n',53),_ROB('t',54),_ROB('V',55),_ROB('e',56),_ROB('r',57),_ROB('s',58),_ROB('i',59),_ROB('o',60),_ROB('n',61),0 };
/* Mouse stack - no literal \Driver\MouHID / \Driver\MouClass in .rdata */
static const UCHAR OBF_MouHID[]                = { _ROB('\\',0),_ROB('D',1),_ROB('r',2),_ROB('i',3),_ROB('v',4),_ROB('e',5),_ROB('r',6),_ROB('\\',7),_ROB('M',8),_ROB('o',9),_ROB('u',10),_ROB('H',11),_ROB('I',12),_ROB('D',13),0 };
static const UCHAR OBF_MouClass[]              = { _ROB('\\',0),_ROB('D',1),_ROB('r',2),_ROB('i',3),_ROB('v',4),_ROB('e',5),_ROB('r',6),_ROB('\\',7),_ROB('M',8),_ROB('o',9),_ROB('u',10),_ROB('C',11),_ROB('l',12),_ROB('a',13),_ROB('s',14),_ROB('s',15),0 };
/* Device/link name prefixes - no literal \Device\ or \DosDevices\Global\ in .rdata */
static const UCHAR OBF_DevicePrefix[]         = { _ROB('\\',0),_ROB('D',1),_ROB('e',2),_ROB('v',3),_ROB('i',4),_ROB('c',5),_ROB('e',6),_ROB('\\',7),0 };
static const UCHAR OBF_LinkPrefix[]           = { _ROB('\\',0),_ROB('D',1),_ROB('o',2),_ROB('s',3),_ROB('D',4),_ROB('e',5),_ROB('v',6),_ROB('i',7),_ROB('c',8),_ROB('e',9),_ROB('s',10),_ROB('\\',11),_ROB('G',12),_ROB('l',13),_ROB('o',14),_ROB('b',15),_ROB('a',16),_ROB('l',17),_ROB('\\',18),0 };
/* create_driver: \Driver\ prefix for ObCreateObject name - no literal in .rdata */
static const UCHAR OBF_DriverNamePrefix[]     = { _ROB('\\',0),_ROB('D',1),_ROB('r',2),_ROB('i',3),_ROB('v',4),_ROB('e',5),_ROB('r',6),_ROB('\\',7),0 };

#undef _ROB

/* Forward declare so inline getters below can call before full definition. */
static __forceinline PVOID get_system_routine_obf(const UCHAR* enc, size_t enc_len);

/* Optional: call PsLookupProcessByProcessId via MmGetSystemRoutineAddress - no IAT, bypass usermode/kernel hooks that patch import. */
typedef NTSTATUS(NTAPI* PsLookupProcessByProcessId_t)(HANDLE ProcessId, PEPROCESS* Process);
static inline PsLookupProcessByProcessId_t get_PsLookupProcessByProcessId_fn(void) {
    static PsLookupProcessByProcessId_t p = nullptr;
    if (!p)
        p = (PsLookupProcessByProcessId_t)get_system_routine_obf(OBF_PsLookupProcessByProcessId, sizeof(OBF_PsLookupProcessByProcessId));
    return p;
}
static inline NTSTATUS safe_PsLookupProcessByProcessId(HANDLE pid, PEPROCESS* proc) {
    PsLookupProcessByProcessId_t fn = get_PsLookupProcessByProcessId_fn();
    return fn ? fn(pid, proc) : STATUS_PROCEDURE_NOT_FOUND;
}

/* ZwQuerySystemInformation - resolved at runtime (no IAT). */
typedef NTSTATUS(NTAPI* ZwQuerySystemInformation_t)(ULONG SystemInformationClass, PVOID SystemInformation, ULONG SystemInformationLength, PULONG ReturnLength);
static inline ZwQuerySystemInformation_t get_ZwQuerySystemInformation_fn(void) {
    static ZwQuerySystemInformation_t p = nullptr;
    if (!p) p = (ZwQuerySystemInformation_t)get_system_routine_obf(OBF_ZwQuerySystemInformation, sizeof(OBF_ZwQuerySystemInformation));
    return p;
}

/* Decode narrow encoded string to WCHAR buffer; max 63 chars. Returns length in WCHARs (excluding null). */
static ULONG routine_obf_decode_to_wide(const UCHAR* enc, size_t enc_len, WCHAR* wbuf, ULONG wbuf_chars) {
    if (!enc || !wbuf || wbuf_chars == 0) return 0;
    ULONG n = 0;
    for (size_t i = 0; i < enc_len && n < wbuf_chars - 1; i++) {
        UCHAR b = (UCHAR)(enc[i] ^ (ROUTINE_OBF_KEY + (UCHAR)(i & 0xF)));
        if (b == 0) break;
        wbuf[n++] = (WCHAR)b;
    }
    wbuf[n] = L'\0';
    return n;
}

/* Resolve system routine by encoded name. Returns nullptr if not found. */
static __forceinline PVOID get_system_routine_obf(const UCHAR* enc, size_t enc_len) {
    WCHAR wbuf[64];
    if (routine_obf_decode_to_wide(enc, enc_len, wbuf, 64) == 0) return nullptr;
    UNICODE_STRING u;
    RtlInitUnicodeString(&u, wbuf);
    return MmGetSystemRoutineAddress(&u);
}

/* Decode to narrow buffer for memcmp (e.g. pool tag). */
static void routine_obf_decode_narrow(const UCHAR* enc, size_t enc_len, UCHAR* out, size_t out_size) {
    if (!enc || !out || out_size == 0) return;
    for (size_t i = 0; i < enc_len && i < out_size - 1; i++)
        out[i] = (UCHAR)(enc[i] ^ (ROUTINE_OBF_KEY + (UCHAR)(i & 0xF)));
    out[enc_len < out_size ? enc_len : out_size - 1] = 0;
}
