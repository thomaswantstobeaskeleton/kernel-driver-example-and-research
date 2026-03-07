#pragma once
/* Optional ETW patching - reduces telemetry that AC/EDR may use.
 * Patch ntdll!EtwEventWrite to return success without logging.
 * Include after flush_comm_config.h (or driver.hpp); call EtwPatch::Init() early in main().
 * DISABLED by default - set FLUSHCOMM_PATCH_ETW 1 in flush_comm_config.h to enable. */

#ifdef _WIN32
#include <Windows.h>
#include "../flush_comm_config.h"
#include "api_resolve.hpp"

#if FLUSHCOMM_PATCH_ETW

namespace EtwPatch {

inline bool Init() {
    HMODULE ntdll = api_resolve::get_module_w(APIRES_OBF_W(L"ntdll.dll"));
    if (!ntdll) return false;
    void* pEtw = (void*)api_resolve::get_proc_a(ntdll, APIRES_OBF_A("EtwEventWrite"));
    if (!pEtw) return false;
    DWORD old;
    if (!VirtualProtect(pEtw, 16, PAGE_EXECUTE_READWRITE, &old))
        return false;
    /* x64: push 0; pop rax; ret - less common than mov eax,0/xor eax,eax (public patterns) */
    unsigned char patch[] = {
        0x6A, 0x00,   /* push 0 */
        0x58,         /* pop rax */
        0xC3,         /* ret */
        0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90
    };
    memcpy(pEtw, patch, sizeof(patch));
    VirtualProtect(pEtw, 16, old, &old);
    return true;
}

} // namespace EtwPatch

#endif /* FLUSHCOMM_PATCH_ETW */

#endif /* _WIN32 */
