#pragma once
/* Usermode spoofer hooks - RtlGetVersion, NtQuerySystemInformation.
 * Hides our process/driver from EAC queries. Inline hook with trampoline.
 * Uses RW->RX alloc (avoids RWX heuristic). Validates target before hooking.
 * Set SPOOFER_ENABLE_HOOKS to 1 to enable - may crash on some Windows builds.
 * API resolve via obfuscated names - no literal "ntdll.dll"/"RtlGetVersion" in binary. */
#ifndef SPOOFER_ENABLE_HOOKS
#define SPOOFER_ENABLE_HOOKS 0
#endif

#include <Windows.h>
#include <cstdint>
#include "../utilities/api_resolve.hpp"

typedef long NTSTATUS;
#define NT_SUCCESS(s) (((NTSTATUS)(s)) >= 0)

#include <winternl.h>

enum { SystemModuleInformation = 11, SystemKernelDebuggerInformation = 35 };

namespace spoofer {
    static BYTE g_RtlGetVersion_orig[16];
    static BYTE g_NtQuerySystemInfo_orig[16];
    static void* g_RtlGetVersion_addr = nullptr;
    static void* g_NtQuerySystemInfo_addr = nullptr;
    static bool g_hooked = false;
    static constexpr size_t HOOK_SZ = 12;  /* mov rax,addr; jmp rax - avoids 0xFF 0x25 (signatured) */

    /* Alloc exec: RW first, then RX - avoids RWX scan */
    static void* alloc_exec(size_t sz) {
        void* p = VirtualAlloc(nullptr, sz, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!p) return nullptr;
        DWORD old;
        if (!VirtualProtect(p, sz, PAGE_EXECUTE_READ, &old)) {
            VirtualFree(p, 0, MEM_RELEASE);
            return nullptr;
        }
        return p;
    }

    /* Validate: target prologue (reject rel-call/jmp). */
    static bool is_safe_to_hook(void* target, size_t sz) {
        unsigned char* p = (unsigned char*)target;
        if (p[0] == 0xE8 || p[0] == 0xE9) return false;  /* call/jmp rel */
        if (p[0] != 0x4C || p[1] != 0x8B || p[2] != 0xD1) return false;
        if (p[3] != 0xB8 && p[3] != 0xB9) return false;
        if (sz >= 12 && (p[10] != 0x0F || p[11] != 0x05)) return false;
        return true;
    }

    /* Hook: mov rax,addr; jmp rax (48 B8 xx FF E0) - avoids 0xFF 0x25 pattern */
    static bool hook_function(void* target, void* detour, BYTE* backup, size_t hook_sz) {
        if (!target || !detour || hook_sz < HOOK_SZ) return false;
        if (!is_safe_to_hook(target, hook_sz)) return false;
        DWORD old;
        if (!VirtualProtect(target, hook_sz, PAGE_EXECUTE_READWRITE, &old)) return false;
        memcpy(backup, target, hook_sz);
        unsigned char* p = (unsigned char*)target;
        p[0] = 0x48; p[1] = 0xB8;  /* mov rax, imm64 */
        *(uintptr_t*)(p + 2) = (uintptr_t)detour;
        p[10] = 0xFF; p[11] = 0xE0;  /* jmp rax */
        VirtualProtect(target, hook_sz, old, &old);
        return true;
    }

    static NTSTATUS NTAPI hooked_RtlGetVersion(PRTL_OSVERSIONINFOW ver) {
        typedef NTSTATUS(NTAPI* F)(PRTL_OSVERSIONINFOW);
        F orig = (F)g_RtlGetVersion_addr;
        if (!orig) return 0xC0000005;
        void* trampoline = alloc_exec(32);
        if (trampoline) {
            memcpy(trampoline, g_RtlGetVersion_orig, HOOK_SZ);
            unsigned char* t = (unsigned char*)trampoline;
            t[HOOK_SZ] = 0x48; t[HOOK_SZ + 1] = 0xB8;
            *(uintptr_t*)(t + HOOK_SZ + 2) = (uintptr_t)orig + HOOK_SZ;
            t[HOOK_SZ + 10] = 0xFF; t[HOOK_SZ + 11] = 0xE0;
            NTSTATUS r = ((F)trampoline)(ver);
            VirtualFree(trampoline, 0, MEM_RELEASE);
            return r;
        }
        return 0xC0000017;
    }

    static NTSTATUS NTAPI hooked_NtQuerySystemInformation(ULONG cls, PVOID buf, ULONG len, PULONG ret) {
        typedef NTSTATUS(NTAPI* F)(ULONG, PVOID, ULONG, PULONG);
        F orig = (F)g_NtQuerySystemInfo_addr;
        if (!orig) return 0xC0000005;
        void* trampoline = alloc_exec(32);
        if (!trampoline) return 0xC0000017;
        memcpy(trampoline, g_NtQuerySystemInfo_orig, HOOK_SZ);
        unsigned char* t = (unsigned char*)trampoline;
        t[HOOK_SZ] = 0x48; t[HOOK_SZ + 1] = 0xB8;
        *(uintptr_t*)(t + HOOK_SZ + 2) = (uintptr_t)orig + HOOK_SZ;
        t[HOOK_SZ + 10] = 0xFF; t[HOOK_SZ + 11] = 0xE0;
        NTSTATUS r = ((F)trampoline)(cls, buf, len, ret);
        VirtualFree(trampoline, 0, MEM_RELEASE);
        return r;
    }

    static bool install() {
#if !SPOOFER_ENABLE_HOOKS
        return false;  /* Disabled by default - was causing crashes on some systems */
#else
        if (g_hooked) return true;
        HMODULE ntdll = api_resolve::get_module_w(APIRES_OBF_W(L"ntdll.dll"));
        if (!ntdll) return false;
        g_RtlGetVersion_addr = (void*)api_resolve::get_proc_a(ntdll, APIRES_OBF_A("RtlGetVersion"));
        g_NtQuerySystemInfo_addr = (void*)api_resolve::get_proc_a(ntdll, APIRES_OBF_A("NtQuerySystemInformation"));
        if (!g_RtlGetVersion_addr || !g_NtQuerySystemInfo_addr) return false;
        if (hook_function(g_RtlGetVersion_addr, (void*)hooked_RtlGetVersion, g_RtlGetVersion_orig, HOOK_SZ) &&
            hook_function(g_NtQuerySystemInfo_addr, (void*)hooked_NtQuerySystemInformation, g_NtQuerySystemInfo_orig, HOOK_SZ)) {
            g_hooked = true;
            return true;
        }
        return false;
#endif
    }
}
