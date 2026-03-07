#pragma once
/* Direct syscall - bypasses ntdll usermode hooks (EAC/EDR).
 * SSN from ntdll at runtime; if target is hooked, SSN taken from adjacent export (no public technique names in code).
 * VirtualAlloc: PAGE_READWRITE then VirtualProtect to RX before use (avoids RWX heuristic).
 * API names obfuscated via api_resolve. Stub bytes decoded at runtime (no literal 4C 8B D1 B8 0F 05 C3 in .rdata). */

#include <Windows.h>
#include <cstdint>
#include <string>
#include "api_resolve.hpp"
#include "custom_nt.hpp"
/* api_resolve includes flush_comm_config.h -> FLUSHCOMM_OBF_BASE; same fallback as api_resolve when config missing (no 0x9D literal). */
#ifndef FLUSHCOMM_OBF_BASE
#define FLUSHCOMM_OBF_BASE ((unsigned char)(0x80 + (((__TIME__[7]-'0')*10+(__TIME__[6]-'0')) % 64)))
#endif
#define STUB_OBF_KEY ((unsigned char)(FLUSHCOMM_OBF_BASE))

typedef long NTSTATUS;
#define NT_SUCCESS(s) (((NTSTATUS)(s)) >= 0)

#define PROCESS_QUERY_LIMITED_INFORMATION 0x1000

/* Extract syscall number from ntdll function at runtime. Prologue bytes compared via decode (no literal 4C 8B D1 B8). */
inline DWORD get_ssn(void* func) {
    if (!func) return 0xFFFFFFFF;
    unsigned char* p = (unsigned char*)func;
    static const unsigned char enc[] = {
        (unsigned char)(0x4C ^ (STUB_OBF_KEY + 0)), (unsigned char)(0x8B ^ (STUB_OBF_KEY + 1)),
        (unsigned char)(0xD1 ^ (STUB_OBF_KEY + 2)), (unsigned char)(0xB8 ^ (STUB_OBF_KEY + 3))
    };
    if (p[0] == (unsigned char)(enc[0] ^ (STUB_OBF_KEY + 0)) && p[1] == (unsigned char)(enc[1] ^ (STUB_OBF_KEY + 1)) &&
        p[2] == (unsigned char)(enc[2] ^ (STUB_OBF_KEY + 2)) && p[3] == (unsigned char)(enc[3] ^ (STUB_OBF_KEY + 3)))
        return *(DWORD*)(p + 4);
    return 0xFFFFFFFF;
}

/* SSN from adjacent export when target hooked - use Nth neighbor, not classic public pair */
inline DWORD get_ssn_fallback(void* target, void* prev_func) {
    DWORD s = get_ssn(target);
    if (s != 0xFFFFFFFF) return s;
    DWORD p = get_ssn(prev_func);
    return (p != 0xFFFFFFFF && p < 0xFFF0) ? (p + 1) : 0xFFFFFFFF;
}

/* Alloc exec memory: NtAllocateVirtualMemory + NtProtectVirtualMemory (no VirtualAlloc/VirtualProtect in call stack) */
inline void* alloc_executable(size_t size) {
    void* p = custom_nt::alloc(size);
    if (!p) return nullptr;
    if (!custom_nt::protect_exec(p, size)) {
        custom_nt::free_mem(p);
        return nullptr;
    }
    return p;
}

typedef NTSTATUS(NTAPI* SyscallNtGetNextProcess_t)(HANDLE, DWORD, ULONG, ULONG, PHANDLE);

inline SyscallNtGetNextProcess_t get_direct_syscall_stub() {
    static SyscallNtGetNextProcess_t g_stub = (SyscallNtGetNextProcess_t)-1;
    if (g_stub != (SyscallNtGetNextProcess_t)-1) return g_stub;
    g_stub = nullptr;
    HMODULE ntdll = api_resolve::get_module_w(APIRES_OBF_W(L"ntdll.dll"));
    auto NtGetNextProcess = (void*)api_resolve::get_proc_a(ntdll, APIRES_OBF_A("NtGetNextProcess"));
    auto NtMapViewOfSection = (void*)api_resolve::get_proc_a(ntdll, APIRES_OBF_A("NtMapViewOfSection"));
    DWORD ssn = get_ssn_fallback(NtGetNextProcess, NtMapViewOfSection);
    if (ssn != 0xFFFFFFFF) {
        static const unsigned char enc[] = {
            (unsigned char)(0x4C ^ (STUB_OBF_KEY + 0)), (unsigned char)(0x8B ^ (STUB_OBF_KEY + 1)),
            (unsigned char)(0xD1 ^ (STUB_OBF_KEY + 2)), (unsigned char)(0xB8 ^ (STUB_OBF_KEY + 3)),
            0u ^ (STUB_OBF_KEY + 4), (unsigned char)(0 ^ (STUB_OBF_KEY + 5)), (unsigned char)(0 ^ (STUB_OBF_KEY + 6)), (unsigned char)(0 ^ (STUB_OBF_KEY + 7)),
            (unsigned char)(0x0F ^ (STUB_OBF_KEY + 8)), (unsigned char)(0x05 ^ (STUB_OBF_KEY + 9)), (unsigned char)(0xC3 ^ (STUB_OBF_KEY + 10))
        };
        unsigned char stub[11];
        for (int i = 0; i < 11; i++) stub[i] = (unsigned char)(enc[i] ^ (STUB_OBF_KEY + i));
        *(DWORD*)(stub + 4) = ssn;
        void* exec = alloc_executable(sizeof(stub));
        if (exec) { memcpy(exec, stub, sizeof(stub)); g_stub = (SyscallNtGetNextProcess_t)exec; }
    }
    return g_stub;
}

inline void init_direct_syscall() { (void)get_direct_syscall_stub(); }

inline NTSTATUS sys_NtGetNextProcess(HANDLE h, DWORD access, ULONG attr, ULONG flags, PHANDLE out) {
    auto stub = get_direct_syscall_stub();
    if (stub) return stub(h, access, attr, flags, out);
    typedef NTSTATUS(NTAPI* F)(HANDLE, DWORD, ULONG, ULONG, PHANDLE);
    auto f = (F)api_resolve::get_proc_a(api_resolve::get_module_w(APIRES_OBF_W(L"ntdll.dll")), APIRES_OBF_A("NtGetNextProcess"));
    return f ? f(h, access, attr, flags, out) : (NTSTATUS)0xC000001F;
}

/* NtDeviceIoControlFile direct syscall - bypasses ntdll hooks on DeviceIoControl path */
typedef NTSTATUS(NTAPI* NtDeviceIoControlFile_t)(HANDLE, HANDLE, void*, void*, PVOID, ULONG, PVOID, ULONG, PVOID, ULONG);
inline NtDeviceIoControlFile_t get_NtDeviceIoControlFile_stub() {
    static NtDeviceIoControlFile_t g_stub = (NtDeviceIoControlFile_t)-1;
    if (g_stub != (NtDeviceIoControlFile_t)-1) return g_stub;
    g_stub = nullptr;
    HMODULE ntdll = api_resolve::get_module_w(APIRES_OBF_W(L"ntdll.dll"));
    auto NtDeviceIoControlFile = (void*)api_resolve::get_proc_a(ntdll, APIRES_OBF_A("NtDeviceIoControlFile"));
    auto NtDelayExecution = (void*)api_resolve::get_proc_a(ntdll, APIRES_OBF_A("NtDelayExecution"));
    DWORD ssn = get_ssn_fallback(NtDeviceIoControlFile, NtDelayExecution);
    if (ssn != 0xFFFFFFFF) {
        static const unsigned char enc[] = {
            (unsigned char)(0x4C ^ (STUB_OBF_KEY + 0)), (unsigned char)(0x8B ^ (STUB_OBF_KEY + 1)),
            (unsigned char)(0xD1 ^ (STUB_OBF_KEY + 2)), (unsigned char)(0xB8 ^ (STUB_OBF_KEY + 3)),
            0u ^ (STUB_OBF_KEY + 4), (unsigned char)(0 ^ (STUB_OBF_KEY + 5)), (unsigned char)(0 ^ (STUB_OBF_KEY + 6)), (unsigned char)(0 ^ (STUB_OBF_KEY + 7)),
            (unsigned char)(0x0F ^ (STUB_OBF_KEY + 8)), (unsigned char)(0x05 ^ (STUB_OBF_KEY + 9)), (unsigned char)(0xC3 ^ (STUB_OBF_KEY + 10))
        };
        unsigned char stub[11];
        for (int i = 0; i < 11; i++) stub[i] = (unsigned char)(enc[i] ^ (STUB_OBF_KEY + i));
        *(DWORD*)(stub + 4) = ssn;
        void* exec = alloc_executable(sizeof(stub));
        if (exec) { memcpy(exec, stub, sizeof(stub)); g_stub = (NtDeviceIoControlFile_t)exec; }
    }
    return g_stub;
}
inline NTSTATUS sys_NtDeviceIoControlFile(HANDLE h, HANDLE evt, void* apc, void* ctx, PVOID iosb, ULONG code, PVOID inbuf, ULONG inlen, PVOID outbuf, ULONG outlen) {
    auto stub = get_NtDeviceIoControlFile_stub();
    if (stub) return stub(h, evt, apc, ctx, iosb, code, inbuf, inlen, outbuf, outlen);
    typedef NTSTATUS(NTAPI* F)(HANDLE, HANDLE, void*, void*, PVOID, ULONG, PVOID, ULONG, PVOID, ULONG);
    auto f = (F)api_resolve::get_proc_a(api_resolve::get_module_w(APIRES_OBF_W(L"ntdll.dll")), APIRES_OBF_A("NtDeviceIoControlFile"));
    return f ? f(h, evt, apc, ctx, iosb, code, inbuf, inlen, outbuf, outlen) : (NTSTATUS)0xC000001F;
}

/* NtQueryVirtualMemory - bypasses VirtualQuery (kernel32->ntdll path, often monitored) */
typedef NTSTATUS(NTAPI* NtQueryVirtualMemory_t)(HANDLE, PVOID, ULONG, PVOID, SIZE_T, PSIZE_T);
#define MemoryBasicInformation 0
inline NtQueryVirtualMemory_t get_NtQueryVirtualMemory_stub() {
    static NtQueryVirtualMemory_t g_stub = (NtQueryVirtualMemory_t)-1;
    if (g_stub != (NtQueryVirtualMemory_t)-1) return g_stub;
    g_stub = nullptr;
    HMODULE ntdll = api_resolve::get_module_w(APIRES_OBF_W(L"ntdll.dll"));
    auto NtQVM = (void*)api_resolve::get_proc_a(ntdll, APIRES_OBF_A("NtQueryVirtualMemory"));
    auto NtMapViewOfSection = (void*)api_resolve::get_proc_a(ntdll, APIRES_OBF_A("NtMapViewOfSection"));
    DWORD ssn = get_ssn_fallback(NtQVM, NtMapViewOfSection);
    if (ssn != 0xFFFFFFFF) {
        static const unsigned char enc[] = {
            (unsigned char)(0x4C ^ (STUB_OBF_KEY + 0)), (unsigned char)(0x8B ^ (STUB_OBF_KEY + 1)),
            (unsigned char)(0xD1 ^ (STUB_OBF_KEY + 2)), (unsigned char)(0xB8 ^ (STUB_OBF_KEY + 3)),
            0u ^ (STUB_OBF_KEY + 4), (unsigned char)(0 ^ (STUB_OBF_KEY + 5)), (unsigned char)(0 ^ (STUB_OBF_KEY + 6)), (unsigned char)(0 ^ (STUB_OBF_KEY + 7)),
            (unsigned char)(0x0F ^ (STUB_OBF_KEY + 8)), (unsigned char)(0x05 ^ (STUB_OBF_KEY + 9)), (unsigned char)(0xC3 ^ (STUB_OBF_KEY + 10))
        };
        unsigned char stub[11];
        for (int i = 0; i < 11; i++) stub[i] = (unsigned char)(enc[i] ^ (STUB_OBF_KEY + i));
        *(DWORD*)(stub + 4) = ssn;
        void* exec = alloc_executable(sizeof(stub));
        if (exec) { memcpy(exec, stub, sizeof(stub)); g_stub = (NtQueryVirtualMemory_t)exec; }
    }
    return g_stub;
}
inline NTSTATUS sys_NtQueryVirtualMemory(HANDLE h, PVOID base, ULONG cls, PVOID info, SIZE_T len, PSIZE_T ret) {
    auto stub = get_NtQueryVirtualMemory_stub();
    if (stub) return stub(h, base, cls, info, len, ret);
    typedef NTSTATUS(NTAPI* F)(HANDLE, PVOID, ULONG, PVOID, SIZE_T, PSIZE_T);
    auto f = (F)api_resolve::get_proc_a(api_resolve::get_module_w(APIRES_OBF_W(L"ntdll.dll")), APIRES_OBF_A("NtQueryVirtualMemory"));
    return f ? f(h, base, cls, info, len, ret) : (NTSTATUS)0xC000001F;
}

/* NtQuerySystemInformation direct syscall - no ntdll hook on process list query */
typedef NTSTATUS(NTAPI* NtQuerySystemInformation_t)(ULONG, PVOID, ULONG, PULONG);
inline NtQuerySystemInformation_t get_NtQuerySystemInformation_stub() {
    static NtQuerySystemInformation_t g_stub = (NtQuerySystemInformation_t)-1;
    if (g_stub != (NtQuerySystemInformation_t)-1) return g_stub;
    g_stub = nullptr;
    HMODULE ntdll = api_resolve::get_module_w(APIRES_OBF_W(L"ntdll.dll"));
    auto NtQSI = (void*)api_resolve::get_proc_a(ntdll, APIRES_OBF_A("NtQuerySystemInformation"));
    auto NtDelay = (void*)api_resolve::get_proc_a(ntdll, APIRES_OBF_A("NtDelayExecution"));
    DWORD ssn = get_ssn_fallback(NtQSI, NtDelay);
    if (ssn != 0xFFFFFFFF) {
        static const unsigned char enc[] = {
            (unsigned char)(0x4C ^ (STUB_OBF_KEY + 0)), (unsigned char)(0x8B ^ (STUB_OBF_KEY + 1)),
            (unsigned char)(0xD1 ^ (STUB_OBF_KEY + 2)), (unsigned char)(0xB8 ^ (STUB_OBF_KEY + 3)),
            0u ^ (STUB_OBF_KEY + 4), (unsigned char)(0 ^ (STUB_OBF_KEY + 5)), (unsigned char)(0 ^ (STUB_OBF_KEY + 6)), (unsigned char)(0 ^ (STUB_OBF_KEY + 7)),
            (unsigned char)(0x0F ^ (STUB_OBF_KEY + 8)), (unsigned char)(0x05 ^ (STUB_OBF_KEY + 9)), (unsigned char)(0xC3 ^ (STUB_OBF_KEY + 10))
        };
        unsigned char stub[11];
        for (int i = 0; i < 11; i++) stub[i] = (unsigned char)(enc[i] ^ (STUB_OBF_KEY + i));
        *(DWORD*)(stub + 4) = ssn;
        void* exec = alloc_executable(sizeof(stub));
        if (exec) { memcpy(exec, stub, sizeof(stub)); g_stub = (NtQuerySystemInformation_t)exec; }
    }
    return g_stub;
}
inline NTSTATUS sys_NtQuerySystemInformation(ULONG cls, PVOID buf, ULONG len, PULONG retLen) {
    auto stub = get_NtQuerySystemInformation_stub();
    if (stub) return stub(cls, buf, len, retLen);
    typedef NTSTATUS(NTAPI* F)(ULONG, PVOID, ULONG, PULONG);
    auto f = (F)api_resolve::get_proc_a(api_resolve::get_module_w(APIRES_OBF_W(L"ntdll.dll")), APIRES_OBF_A("NtQuerySystemInformation"));
    return f ? f(cls, buf, len, retLen) : (NTSTATUS)0xC000001F;
}

/* Iterate processes via NtGetNextProcess; NT-only path (custom_nt::get_process_id, get_process_path_from_handle, close_handle). */
inline void enumerate_processes_nt(void (*cb)(DWORD pid, const char* path, void* ctx), void* ctx) {
    HANDLE h = nullptr;
    init_direct_syscall();
    for (;;) {
        HANDLE next = nullptr;
        NTSTATUS st = sys_NtGetNextProcess(h, PROCESS_QUERY_LIMITED_INFORMATION, 0, 0, &next);
        if (h) custom_nt::close_handle(h);
        h = next;
        if (!NT_SUCCESS(st) || !h) break;
        DWORD pid = custom_nt::get_process_id(h);
        if (pid == 0) continue;
        std::wstring pathW = custom_nt::get_process_path_from_handle(h);
        if (pathW.empty()) continue;
        char pathA[MAX_PATH] = { 0 };
        WideCharToMultiByte(CP_ACP, 0, pathW.c_str(), (int)pathW.size() + 1, pathA, MAX_PATH, NULL, NULL);
        cb(pid, pathA, ctx);
    }
    if (h) custom_nt::close_handle(h);
}
