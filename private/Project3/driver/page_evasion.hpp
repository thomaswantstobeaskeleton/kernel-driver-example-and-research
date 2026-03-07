#pragma once
/* Page/evasion helpers for manually mapped driver.
 * - Run trace_cleaner early to clear MmUnloadedDrivers for vuln drivers.
 * - Use less suspicious pool tags for internal allocations.
 * - Runtime pool tag variation to reduce static scan effectiveness.
 * - Document: full page hiding (MiUnlinkPage, PTE spoof) requires undocumented
 *   ntoskrnl symbols and is version-dependent - see DRIVER_EVASION_RESEARCH.md */

#include <ntifs.h>
#include <ntimage.h>
#include "../flush_comm_config.h"

/* Pool tags derived from FLUSHCOMM_MAGIC so no fixed public set (Fls/Cc/Io/Mm).
 * Each tag is 4 bytes, driver-like (0x20-0x7E), unique per build - not in documented cheat/mapper lists. */
/* 4-byte tag: byte0=space-like, byte1=A-Z, byte2/3=space-like (driver convention). All from MAGIC. */
static inline ULONG evasion_tag_reg(void) {
    ULONGLONG m = (ULONGLONG)FLUSHCOMM_MAGIC;
    return (ULONG)(UCHAR)(0x20u + (m & 0x1Fu)) | ((ULONG)(UCHAR)(0x41u + ((m >> 5) % 26u)) << 8) |
        ((ULONG)(UCHAR)(0x20u + ((m >> 10) & 0x1Fu)) << 16) | ((ULONG)(UCHAR)(0x20u + ((m >> 15) & 0x1Fu)) << 24);
}
static inline ULONG evasion_tag_list(void) {
    ULONGLONG m = (ULONGLONG)(FLUSHCOMM_MAGIC >> 20);
    return (ULONG)(UCHAR)(0x20u + (m & 0x1Fu)) | ((ULONG)(UCHAR)(0x41u + ((m >> 5) % 26u)) << 8) |
        ((ULONG)(UCHAR)(0x20u + ((m >> 10) & 0x1Fu)) << 16) | ((ULONG)(UCHAR)(0x20u + ((m >> 15) & 0x1Fu)) << 24);
}
static inline ULONG evasion_tag_copy(void) {
    ULONGLONG m = (ULONGLONG)(FLUSHCOMM_MAGIC >> 40);
    return (ULONG)(UCHAR)(0x20u + (m & 0x1Fu)) | ((ULONG)(UCHAR)(0x41u + ((m >> 5) % 26u)) << 8) |
        ((ULONG)(UCHAR)(0x20u + ((m >> 10) & 0x1Fu)) << 16) | ((ULONG)(UCHAR)(0x20u + ((m >> 15) & 0x1Fu)) << 24);
}
static inline ULONG evasion_tag_work(void) {
    ULONGLONG m = (ULONGLONG)(FLUSHCOMM_MAGIC ^ (FLUSHCOMM_MAGIC >> 32));
    return (ULONG)(UCHAR)(0x20u + (m & 0x1Fu)) | ((ULONG)(UCHAR)(0x41u + ((m >> 5) % 26u)) << 8) |
        ((ULONG)(UCHAR)(0x20u + ((m >> 10) & 0x1Fu)) << 16) | ((ULONG)(UCHAR)(0x20u + ((m >> 15) & 0x1Fu)) << 24);
}

/* Optional: rotate pool tags at runtime to vary per boot/session.
 * Define FLUSHCOMM_POOL_TAG_ROTATE 1 in flush_comm_config.h to enable.
 * Requires: page_evasion_init() before any allocation; use EVASION_POOL_TAG_*_R in code. */
#ifndef FLUSHCOMM_POOL_TAG_ROTATE
#define FLUSHCOMM_POOL_TAG_ROTATE 0
#endif

#if FLUSHCOMM_POOL_TAG_ROTATE
/* Benign tags - one chosen at init based on KeQueryPerformanceCounter; built from bytes */
static ULONG g_evasion_tag_reg, g_evasion_tag_list, g_evasion_tag_copy, g_evasion_tag_work;
#define EVASION_POOL_TAG_REG_R   g_evasion_tag_reg
#define EVASION_POOL_TAG_LIST_R  g_evasion_tag_list
#define EVASION_POOL_TAG_COPY_R  g_evasion_tag_copy
#define EVASION_POOL_TAG_WORK_R  g_evasion_tag_work
#else
#define EVASION_POOL_TAG_REG_R   (evasion_tag_reg())
#define EVASION_POOL_TAG_LIST_R  (evasion_tag_list())
#define EVASION_POOL_TAG_COPY_R  (evasion_tag_copy())
#define EVASION_POOL_TAG_WORK_R  (evasion_tag_work())
#endif

/* Call from DriverEntry after trace_cleaner. Optional extra evasion steps. */
static inline void page_evasion_init(PVOID ImageBase) {
    UNREFERENCED_PARAMETER(ImageBase);
#if FLUSHCOMM_POOL_TAG_ROTATE
    LARGE_INTEGER pc;
    KeQueryPerformanceCounter(&pc);
    ULONG sel = (ULONG)(pc.QuadPart % 4);
    /* All tags MAGIC-derived - no fixed public set */
    static ULONG rot_tags[8];
    if (rot_tags[0] == 0) {
        rot_tags[0] = evasion_tag_reg();  rot_tags[1] = evasion_tag_list();
        rot_tags[2] = evasion_tag_copy(); rot_tags[3] = evasion_tag_work();
        { ULONGLONG m = (ULONGLONG)(FLUSHCOMM_MAGIC >> 16); rot_tags[4] = (ULONG)(UCHAR)(0x20u+(m&0x1Fu))|((ULONG)(UCHAR)(0x41u+((m>>5)%26u))<<8)|((ULONG)(UCHAR)(0x20u+((m>>10)&0x1Fu))<<16)|((ULONG)(UCHAR)(0x20u+((m>>15)&0x1Fu))<<24); }
        { ULONGLONG m = (ULONGLONG)(FLUSHCOMM_MAGIC >> 24); rot_tags[5] = (ULONG)(UCHAR)(0x20u+(m&0x1Fu))|((ULONG)(UCHAR)(0x41u+((m>>5)%26u))<<8)|((ULONG)(UCHAR)(0x20u+((m>>10)&0x1Fu))<<16)|((ULONG)(UCHAR)(0x20u+((m>>15)&0x1Fu))<<24); }
        { ULONGLONG m = (ULONGLONG)(FLUSHCOMM_MAGIC >> 36); rot_tags[6] = (ULONG)(UCHAR)(0x20u+(m&0x1Fu))|((ULONG)(UCHAR)(0x41u+((m>>5)%26u))<<8)|((ULONG)(UCHAR)(0x20u+((m>>10)&0x1Fu))<<16)|((ULONG)(UCHAR)(0x20u+((m>>15)&0x1Fu))<<24); }
        { ULONGLONG m = (ULONGLONG)(FLUSHCOMM_MAGIC ^ (FLUSHCOMM_MAGIC << 16)); rot_tags[7] = (ULONG)(UCHAR)(0x20u+(m&0x1Fu))|((ULONG)(UCHAR)(0x41u+((m>>5)%26u))<<8)|((ULONG)(UCHAR)(0x20u+((m>>10)&0x1Fu))<<16)|((ULONG)(UCHAR)(0x20u+((m>>15)&0x1Fu))<<24); }
    }
    g_evasion_tag_reg  = rot_tags[(sel + 0) % 8];
    g_evasion_tag_list = rot_tags[(sel + 1) % 8];
    g_evasion_tag_copy = rot_tags[(sel + 2) % 8];
    g_evasion_tag_work = rot_tags[(sel + 3) % 8];
#endif
    /* Manually mapped: not in PsLoadedModuleList, no DriverObject.
     * BigPool cleared by kdmapper when using pool mode.
     * Full page hiding (MiUnlinkPage, VAD unlink) requires:
     * - Resolving MiUnlinkPage or similar from ntoskrnl
     * - Our allocation context - version-dependent
     * See: Kernel-VAD-Injector, revers.engineering/hiding-drivers */
}
