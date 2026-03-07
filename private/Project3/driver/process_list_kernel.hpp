#pragma once
/* Custom kernel process list walk: iterate EPROCESS via ActiveProcessLinks,
 * match by the 15-byte ImageFileName field. No PsGetNextProcess, no PsGetProcessImageFileName.
 * Returns PID (UniqueProcessId) or 0. Win11 23H2 / Win10 compatible via globals::offsets. */

#include "includes.hpp"

#define EPROC_NAMELEN 15

/* Case-insensitive compare of two narrow strings, max n chars. No CRT _stricmp. */
static __forceinline int eproc_name_cmp(const UCHAR* a, const UCHAR* b, int n) {
    for (int i = 0; i < n; i++) {
        UCHAR ca = a[i], cb = b[i];
        if (ca == 0 && cb == 0) return 0;
        /* tolower: A-Z -> a-z */
        if (ca >= 'A' && ca <= 'Z') ca = (UCHAR)(ca + 32);
        if (cb >= 'A' && cb <= 'Z') cb = (UCHAR)(cb + 32);
        if (ca != cb) return (int)ca - (int)cb;
    }
    return 0;
}

/* Walk EPROCESS list from PsInitialSystemProcess; compare ImageFileName (UCHAR[15]) to name.
 * Returns (ULONG)UniqueProcessId if match, else 0. */
static __forceinline ULONG find_pid_by_image_name(const char* name) {
    if (!name || !PsInitialSystemProcess) return 0;
    const int off_links = globals::offsets::i_active_process_links;
    const int off_name = globals::offsets::i_image_file_name;
    const int off_pid = globals::offsets::i_unique_process_id;

    uintptr_t list_head = *(uintptr_t*)((uintptr_t)PsInitialSystemProcess + off_links);
    uintptr_t cur = list_head;

    do {
        uintptr_t eproc = cur - off_links;
        if (!MmIsAddressValid((PVOID)eproc)) break;
        UCHAR* img_name = (UCHAR*)(eproc + off_name);
        if (!MmIsAddressValid(img_name)) break;
        if (eproc_name_cmp((const UCHAR*)name, img_name, EPROC_NAMELEN) == 0) {
            ULONG_PTR* pid_ptr = (ULONG_PTR*)(eproc + off_pid);
            if (MmIsAddressValid(pid_ptr))
                return (ULONG)*pid_ptr;
            return 0;
        }
        cur = *(uintptr_t*)cur;
    } while (cur != list_head);

    return 0;
}
