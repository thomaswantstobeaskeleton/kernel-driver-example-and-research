#pragma once
/* Driver trace cleaner - clears MmUnloadedDrivers for kdmapper vuln drivers.
 * PiDDBCacheTable cleanup: requires pattern scan for PiDDBCacheTable/PiDDBLock.
 * g_KernelHashBucketList, Wdfilter: placeholders for additional cleanup. */

#include "includes.hpp"
#include <ntimage.h>
#include "../flush_comm_config.h"
#ifndef FLUSHCOMM_TRACE_CLEANER_WDFILTER
#define FLUSHCOMM_TRACE_CLEANER_WDFILTER 0
#endif

/* _UNLOADED_DRIVER x64 - Win10+ */
#pragma pack(push, 8)
typedef struct _UNLOADED_DRIVER_X64 {
    USHORT Length;
    USHORT MaximumLength;
    PWSTR Buffer;
    PVOID StartAddress;
    PVOID EndAddress;
    LARGE_INTEGER CurrentTime;
} UNLOADED_DRIVER_X64, *PUNLOADED_DRIVER_X64;
#pragma pack(pop)

/* Vuln drivers used by mappers - encoded at compile time (no public literals in .rdata). Key from config so no single literal signature. */
#define TRACE_OBF_KEY  (FLUSHCOMM_OBF_BASE)
#define _TE(c) (UCHAR)((c)^TRACE_OBF_KEY)
static const UCHAR TRACE_DRV_0[]  = { _TE('i'),_TE('q'),_TE('v'),_TE('w'),_TE('6'),_TE('4'),_TE('e'),_TE('.'),_TE('s'),_TE('y'),_TE('s'),0 };
static const UCHAR TRACE_DRV_1[]  = { _TE('i'),_TE('q'),_TE('v'),_TE('w'),_TE('6'),_TE('4'),_TE('e'),0 };
static const UCHAR TRACE_DRV_2[]  = { _TE('c'),_TE('a'),_TE('p'),_TE('c'),_TE('o'),_TE('m'),_TE('.'),_TE('s'),_TE('y'),_TE('s'),0 };
static const UCHAR TRACE_DRV_3[]  = { _TE('d'),_TE('s'),_TE('e'),_TE('f'),_TE('i'),_TE('x'),_TE('.'),_TE('s'),_TE('y'),_TE('s'),0 };
static const UCHAR TRACE_DRV_4[]  = { _TE('D'),_TE('B'),_TE('K'),_TE('6'),_TE('4'),_TE('.'),_TE('s'),_TE('y'),_TE('s'),0 };
static const UCHAR TRACE_DRV_5[]  = { _TE('a'),_TE('s'),_TE('M'),_TE('M'),_TE('a'),_TE('p'),_TE('6'),_TE('4'),_TE('.'),_TE('s'),_TE('y'),_TE('s'),0 };
static const UCHAR TRACE_DRV_6[]  = { _TE('a'),_TE('s'),_TE('I'),_TE('O'),_TE('6'),_TE('4'),_TE('.'),_TE('s'),_TE('y'),_TE('s'),0 };
static const UCHAR TRACE_DRV_7[]  = { _TE('R'),_TE('T'),_TE('C'),_TE('o'),_TE('r'),_TE('e'),_TE('6'),_TE('4'),_TE('.'),_TE('s'),_TE('y'),_TE('s'),0 };
static const UCHAR TRACE_DRV_8[]  = { _TE('g'),_TE('d'),_TE('r'),_TE('v'),_TE('.'),_TE('s'),_TE('y'),_TE('s'),0 };
static const UCHAR TRACE_DRV_9[]  = { _TE('E'),_TE('n'),_TE('e'),_TE('I'),_TE('o'),_TE('6'),_TE('4'),_TE('.'),_TE('s'),_TE('y'),_TE('s'),0 };
static const UCHAR TRACE_DRV_10[] = { _TE('e'),_TE('n'),_TE('e'),_TE('i'),_TE('o'),_TE('6'),_TE('4'),_TE('.'),_TE('s'),_TE('y'),_TE('s'),0 };
static const UCHAR TRACE_DRV_11[] = { _TE('e'),_TE('n'),_TE('e'),_TE('i'),_TE('o'),_TE('6'),_TE('4'),0 };
static const UCHAR* const TRACE_DRIVERS_ENC[] = { TRACE_DRV_0, TRACE_DRV_1, TRACE_DRV_2, TRACE_DRV_3, TRACE_DRV_4, TRACE_DRV_5, TRACE_DRV_6, TRACE_DRV_7, TRACE_DRV_8, TRACE_DRV_9, TRACE_DRV_10, TRACE_DRV_11, nullptr };
#define TRACE_DRIVERS_ENC_COUNT 12

static bool drivers_match(PCUNICODE_STRING u, PCWSTR w) {
    if (!u || !u->Buffer || !w) return false;
    for (USHORT i = 0; i < u->Length / sizeof(WCHAR) && w[i]; i++)
        if ((u->Buffer[i] | 0x20) != (w[i] | 0x20)) return false;
    return w[u->Length / sizeof(WCHAR)] == 0;
}

/* Zero physical page contents for a VA range. Best-effort: skips unmapped pages.
 * Uses MmGetPhysicalAddress + MmMapIoSpace; only works if pages are still mapped. */
static void pfn_zero_pages(PVOID start_va, PVOID end_va) {
    if (!start_va || !end_va || (ULONG_PTR)end_va <= (ULONG_PTR)start_va) return;
    __try {
        for (PUCHAR page = (PUCHAR)start_va; page < (PUCHAR)end_va; page += PAGE_SIZE) {
            if (!MmIsAddressValid(page)) continue;
            PHYSICAL_ADDRESS pa = MmGetPhysicalAddress(page);
            if (pa.QuadPart == 0) continue;
            PHYSICAL_ADDRESS pa_page;
            pa_page.QuadPart = pa.QuadPart & ~(PAGE_SIZE - 1);
            PVOID mapped = MmMapIoSpace(pa_page, PAGE_SIZE, MmNonCached);
            if (mapped) {
                RtlZeroMemory(mapped, PAGE_SIZE);
                MmUnmapIoSpace(mapped, PAGE_SIZE);
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) { }
}

/* MmLastUnloadedDriver checksum: some ACs verify integrity. When zeroing entries, we clear
 * the entry; if a checksum exists in the structure it may need updating. Win10+ UNLOADED_DRIVER
 * layout varies - add checksum fix if targeting specific AC that validates it. */
static void trace_clean_mm_unloaded(PUNLOADED_DRIVER_X64 arr, ULONG count) {
    WCHAR dec_buf[32];
    for (ULONG i = 0; i < count && i < 50; i++) {
        PUNLOADED_DRIVER_X64 e = &arr[i];
        __try {
            if (!MmIsAddressValid(e)) break;
            for (int d = 0; d < TRACE_DRIVERS_ENC_COUNT && TRACE_DRIVERS_ENC[d]; d++) {
                const UCHAR* enc = TRACE_DRIVERS_ENC[d];
                int j = 0;
                while (enc[j] && j < 31) { dec_buf[j] = (WCHAR)(enc[j] ^ TRACE_OBF_KEY); j++; }
                dec_buf[j] = L'\0';
                UNICODE_STRING n;
                n.Length = e->Length;
                n.MaximumLength = e->MaximumLength;
                n.Buffer = e->Buffer;
                if (drivers_match(&n, dec_buf)) {
#if FLUSHCOMM_PFN_ZEROING
                    pfn_zero_pages(e->StartAddress, e->EndAddress);
#endif
                    if (e->Buffer) ExFreePool(e->Buffer);
                    RtlZeroMemory(e, sizeof(UNLOADED_DRIVER_X64));
                    break;
                }
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) { }
    }
}

/* PiDDBCacheTable: optional - patterns vary by build. When found, removes vuln driver entries.
 * Full impl requires: PiDDBLock (ERESOURCE), PiDDBCacheTable (RTL_AVL_TABLE),
 * RtlEnumerateGenericTableAvl, RtlDeleteElementGenericTableAvl, compare by TimeDateStamp/DriverName. */
static void pi_ddb_clean_run(void) {
    /* Placeholder - add pattern scan for PiDDBCacheTable + PiDDBLock per Windows build.
     * See kdmapper TraceCleaner, x64DriverCleaner, or PiDDBCacheTable repos for reference. */
    (void)0;
}

/* g_KernelHashBucketList: ntoskrnl global - hash table of loaded drivers. ACs (BattlEye, EAC)
 * may enumerate this. Clear vuln driver entries. Pattern: varies by build; PDB/symbols most reliable. */
static void trace_clean_hash_bucket(PUCHAR nt_base, DWORD nt_size) {
    /* Placeholder: pattern for g_KernelHashBucketList ref. Example: lea rcx, [rip+off] near
     * CmpKernelHashBucketEntry or similar. 64KernelDriverCleaner uses clearHashBucket().
     * Add pattern per build when symbols unavailable. */
    (void)nt_base;
    (void)nt_size;
}

/* Wdfilter trace cleanup: Wdfilter.sys (Windows Defender) may cache driver info in
 * RuntimeDriverList/RuntimeDriverCount/RuntimeDriverArray. Clearing reduces Defender visibility.
 * Requires loading Wdfilter, pattern scan for FltGlobals or internal structures. */
static void trace_clean_wdfilter(void) {
    /* Placeholder: load wdfilter.sys, find FltGlobals/RuntimeDriver* via pattern,
     * remove vuln driver entries. High risk - Wdfilter is monitored. */
    (void)0;
}

/* Custom: probe for MmUnloadedDrivers by scanning .data/.rdata for pointer to valid UNLOADED_DRIVER_X64 array.
 * No public LEA opcode pattern - structure-based only. Win11 23H2 compatible. */
static BOOLEAN trace_clean_via_structure_probe(PUCHAR base, DWORD sz) {
    const ULONG_PTR kernel_low = 0xFFFF800000000000ULL;
    const ULONG_PTR kernel_high = 0xFFFFFFFFFFFFFFFFULL;
    PIMAGE_NT_HEADERS pe = (PIMAGE_NT_HEADERS)(base + ((PIMAGE_DOS_HEADER)base)->e_lfanew);
    PIMAGE_SECTION_HEADER sec = IMAGE_FIRST_SECTION(pe);
    for (WORD s = 0; s < pe->FileHeader.NumberOfSections; s++, sec++) {
        /* Only scan .data and .rdata - no .text (avoids false positives and speed) */
        if (sec->VirtualAddress + sec->Misc.VirtualSize > sz) continue;
        UCHAR* sec_base = base + sec->VirtualAddress;
        DWORD sec_sz = sec->Misc.VirtualSize;
        if (sec_sz < 8) continue;
        /* Section name: .data or .rdata (first char is '.') */
        if (sec->Name[0] != '.') continue;
        if ((sec->Name[1] != 'd' && sec->Name[1] != 'r') || sec->Name[2] != 'a') continue;
        const DWORD stride = 8;
        for (DWORD i = 0; i + 8 <= sec_sz; i += stride) {
        ULONG_PTR val = *(ULONG_PTR*)(sec_base + i);
        if (val < kernel_low || val > kernel_high) continue;
        PUNLOADED_DRIVER_X64 arr = (PUNLOADED_DRIVER_X64)val;
        __try {
            if (!MmIsAddressValid(arr) || !MmIsAddressValid(&arr[0])) continue;
            PUNLOADED_DRIVER_X64 e = &arr[0];
            if (e->Length > 512 || e->MaximumLength > 512) continue;
            if (!MmIsAddressValid(e->Buffer) && e->Buffer != nullptr) continue;
            if ((ULONG_PTR)e->StartAddress < kernel_low || (ULONG_PTR)e->EndAddress < kernel_low) continue;
            /* Count valid entries (Length/MaxLength/Buffer/Start/End look plausible) */
            ULONG count = 0;
            for (; count < 50; count++) {
                PUNLOADED_DRIVER_X64 ent = &arr[count];
                if (!MmIsAddressValid(ent)) break;
                if (ent->Length > 512 || ent->MaximumLength > 512) break;
                if (ent->Buffer != nullptr && !MmIsAddressValid(ent->Buffer)) break;
            }
            if (count > 0 && count <= 50) {
                trace_clean_mm_unloaded(arr, count);
                pi_ddb_clean_run();
                trace_clean_hash_bucket(base, sz);
#if FLUSHCOMM_TRACE_CLEANER_WDFILTER
                trace_clean_wdfilter();
#endif
                return TRUE;
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) { }
        }
    }
    return FALSE;
}

/* "ntoskrnl.exe" decoded at runtime - same key as TRACE_DRIVERS (no literal in .rdata) */
#undef _TE
static const UCHAR nt_name_enc[] = {
    (UCHAR)('n'^TRACE_OBF_KEY), (UCHAR)('t'^TRACE_OBF_KEY), (UCHAR)('o'^TRACE_OBF_KEY),
    (UCHAR)('s'^TRACE_OBF_KEY), (UCHAR)('k'^TRACE_OBF_KEY), (UCHAR)('r'^TRACE_OBF_KEY),
    (UCHAR)('n'^TRACE_OBF_KEY), (UCHAR)('l'^TRACE_OBF_KEY), (UCHAR)('.'^TRACE_OBF_KEY),
    (UCHAR)('e'^TRACE_OBF_KEY), (UCHAR)('x'^TRACE_OBF_KEY), (UCHAR)('e'^TRACE_OBF_KEY),
    0
};
static void trace_cleaner_run(void) {
    PDRIVER_OBJECT nt = nullptr;
    WCHAR nt_name_buf[16];
    for (int i = 0; i < 12; i++) nt_name_buf[i] = (WCHAR)(nt_name_enc[i] ^ TRACE_OBF_KEY);
    nt_name_buf[12] = L'\0';
    UNICODE_STRING name;
    RtlInitUnicodeString(&name, nt_name_buf);
    if (!NT_SUCCESS(ObReferenceObjectByName(&name, OBJ_CASE_INSENSITIVE, nullptr, 0,
            *globals::IoDriverObjectType, KernelMode, nullptr, (PVOID*)&nt))) return;
    PUCHAR base = (PUCHAR)nt->DriverStart;
    ObDereferenceObject(nt);
    PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)base;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return;
    PIMAGE_NT_HEADERS pe = (PIMAGE_NT_HEADERS)(base + dos->e_lfanew);
    DWORD sz = pe->OptionalHeader.SizeOfImage;
    /* Custom: try structure-based probe first (no public LEA pattern). Win11 23H2. */
    if (trace_clean_via_structure_probe(base, sz)) return;
    /* Pattern bytes decoded at runtime so no literal 48 8D 0D / 4C 8D 05 in .rdata. */
    static const UCHAR enc1[] = { (UCHAR)(0x48 ^ TRACE_OBF_KEY), (UCHAR)(0x8D ^ TRACE_OBF_KEY), (UCHAR)(0x0D ^ TRACE_OBF_KEY) };
    static const UCHAR enc2[] = { (UCHAR)(0x4C ^ TRACE_OBF_KEY), (UCHAR)(0x8D ^ TRACE_OBF_KEY), (UCHAR)(0x05 ^ TRACE_OBF_KEY) };
    UCHAR p1[3], p2[3];
    for (int j = 0; j < 3; j++) { p1[j] = enc1[j] ^ TRACE_OBF_KEY; p2[j] = enc2[j] ^ TRACE_OBF_KEY; }
    for (DWORD i = 0; i < sz - 20; i++) {
        if (base[i] == p1[0] && base[i + 1] == p1[1] && base[i + 2] == p1[2]) {
            LONG off = *(LONG*)(base + i + 3);
            PUNLOADED_DRIVER_X64* parr = (PUNLOADED_DRIVER_X64*)(base + i + 7 + off);
            __try {
                PUNLOADED_DRIVER_X64 arr = *parr;
                if (arr && MmIsAddressValid(arr) && MmIsAddressValid(&arr[0])) {
                    PULONG cnt = (PULONG)(parr + 1);
                    if (MmIsAddressValid(cnt) && *cnt <= 50) {
                        trace_clean_mm_unloaded(arr, *cnt);
                        pi_ddb_clean_run();
                        trace_clean_hash_bucket(base, sz);
#if FLUSHCOMM_TRACE_CLEANER_WDFILTER
                        trace_clean_wdfilter();
#endif
                        return;
                    }
                }
            } __except (EXCEPTION_EXECUTE_HANDLER) { }
        }
    }
    for (DWORD i = 0; i < sz - 20; i++) {
        if (base[i] == p2[0] && base[i + 1] == p2[1] && base[i + 2] == p2[2]) {
            LONG off = *(LONG*)(base + i + 3);
            PUNLOADED_DRIVER_X64* parr = (PUNLOADED_DRIVER_X64*)(base + i + 7 + off);
            __try {
                PUNLOADED_DRIVER_X64 arr = *parr;
                if (arr && MmIsAddressValid(arr) && MmIsAddressValid(&arr[0])) {
                    PULONG cnt = (PULONG)(parr + 1);
                    if (MmIsAddressValid(cnt) && *cnt <= 50) {
                        trace_clean_mm_unloaded(arr, *cnt);
                        pi_ddb_clean_run();
                        trace_clean_hash_bucket(base, sz);
#if FLUSHCOMM_TRACE_CLEANER_WDFILTER
                        trace_clean_wdfilter();
#endif
                        return;
                    }
                }
            } __except (EXCEPTION_EXECUTE_HANDLER) { }
        }
    }
}
