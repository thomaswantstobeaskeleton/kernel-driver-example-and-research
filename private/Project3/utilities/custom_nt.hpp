#pragma once
/* Custom NT-only replacements for public Windows APIs. No kernel32/psapi in hot path.
 * All symbols resolved at runtime via api_resolve (no IAT). Use for process enum, alloc, close, delay. */

#include <Windows.h>
#include "api_resolve.hpp"
#include <cstdint>
#include <string>

#ifndef NT_SUCCESS
#define NT_SUCCESS(s) ((((NTSTATUS)(s)) >= 0))
#endif

#define PROCESS_QUERY_LIMITED_INFORMATION 0x1000
#define ProcessBasicInformation 0
#define ProcessImageFileName 27

namespace custom_nt {

typedef long NTSTATUS;
typedef struct _PROCESS_BASIC_INFORMATION {
    PVOID Reserved1;
    PVOID PebBaseAddress;
    PVOID Reserved2[2];
    ULONG_PTR UniqueProcessId;
    PVOID Reserved3;
} PROCESS_BASIC_INFORMATION;

/* NtClose - no CloseHandle (kernel32) in call stack */
typedef NTSTATUS(NTAPI* NtClose_t)(HANDLE);
inline NtClose_t get_NtClose() {
    static NtClose_t p = nullptr;
    if (!p) {
        HMODULE ntdll = api_resolve::get_module_w(APIRES_OBF_W(L"ntdll.dll"));
        if (ntdll) p = (NtClose_t)api_resolve::get_proc_a(ntdll, APIRES_OBF_A("NtClose"));
    }
    return p;
}
inline void close_handle(HANDLE h) {
    if (h && h != INVALID_HANDLE_VALUE) {
        NtClose_t fn = get_NtClose();
        if (fn) fn(h);
    }
}

/* NtAllocateVirtualMemory / NtFreeVirtualMemory - no HeapAlloc/VirtualAlloc (kernel32) in IAT for our buffers */
typedef NTSTATUS(NTAPI* NtAllocateVirtualMemory_t)(HANDLE, PVOID*, ULONG_PTR, PSIZE_T, ULONG, ULONG);
typedef NTSTATUS(NTAPI* NtFreeVirtualMemory_t)(HANDLE, PVOID*, PSIZE_T, ULONG);
#define MEM_COMMIT_NT 0x1000
#define MEM_RESERVE_NT 0x2000
#define PAGE_READWRITE_NT 0x04

inline NtAllocateVirtualMemory_t get_NtAllocVM() {
    static NtAllocateVirtualMemory_t p = nullptr;
    if (!p) {
        HMODULE ntdll = api_resolve::get_module_w(APIRES_OBF_W(L"ntdll.dll"));
        if (ntdll) p = (NtAllocateVirtualMemory_t)api_resolve::get_proc_a(ntdll, APIRES_OBF_A("NtAllocateVirtualMemory"));
    }
    return p;
}
inline NtFreeVirtualMemory_t get_NtFreeVM() {
    static NtFreeVirtualMemory_t p = nullptr;
    if (!p) {
        HMODULE ntdll = api_resolve::get_module_w(APIRES_OBF_W(L"ntdll.dll"));
        if (ntdll) p = (NtFreeVirtualMemory_t)api_resolve::get_proc_a(ntdll, APIRES_OBF_A("NtFreeVirtualMemory"));
    }
    return p;
}
inline void* alloc(size_t size) {
    NtAllocateVirtualMemory_t fn = get_NtAllocVM();
    if (!fn) return nullptr;
    PVOID base = nullptr;
    SIZE_T sz = size;
    if (!NT_SUCCESS(fn((HANDLE)-1, &base, 0, &sz, MEM_COMMIT_NT | MEM_RESERVE_NT, PAGE_READWRITE_NT)))
        return nullptr;
    return base;
}
#define MEM_RELEASE_NT 0x8000
inline void free_mem(void* ptr) {
    if (!ptr) return;
    NtFreeVirtualMemory_t fn = get_NtFreeVM();
    if (fn) {
        PVOID base = ptr;
        SIZE_T sz = 0;
        fn((HANDLE)-1, &base, &sz, MEM_RELEASE_NT);
    }
}

/* NtProtectVirtualMemory - no VirtualProtect (kernel32) for exec memory */
typedef NTSTATUS(NTAPI* NtProtectVirtualMemory_t)(HANDLE, PVOID*, PSIZE_T, ULONG, PULONG);
#define PAGE_READWRITE_NT_PROT 0x04
#define PAGE_EXECUTE_READ_NT 0x20
inline NtProtectVirtualMemory_t get_NtProtectVM() {
    static NtProtectVirtualMemory_t p = nullptr;
    if (!p) {
        HMODULE ntdll = api_resolve::get_module_w(APIRES_OBF_W(L"ntdll.dll"));
        if (ntdll) p = (NtProtectVirtualMemory_t)api_resolve::get_proc_a(ntdll, APIRES_OBF_A("NtProtectVirtualMemory"));
    }
    return p;
}
inline BOOL protect_exec(void* ptr, size_t size) {
    NtProtectVirtualMemory_t fn = get_NtProtectVM();
    if (!fn || !ptr) return FALSE;
    PVOID base = ptr;
    SIZE_T sz = size;
    ULONG old;
    return NT_SUCCESS(fn((HANDLE)-1, &base, &sz, PAGE_EXECUTE_READ_NT, &old));
}

/* NtDelayExecution - no Sleep (kernel32) in call stack */
typedef NTSTATUS(NTAPI* NtDelayExecution_t)(BOOLEAN, PLARGE_INTEGER);
inline NtDelayExecution_t get_NtDelayExecution() {
    static NtDelayExecution_t p = nullptr;
    if (!p) {
        HMODULE ntdll = api_resolve::get_module_w(APIRES_OBF_W(L"ntdll.dll"));
        if (ntdll) p = (NtDelayExecution_t)api_resolve::get_proc_a(ntdll, APIRES_OBF_A("NtDelayExecution"));
    }
    return p;
}
inline void delay_ms(DWORD ms) {
    NtDelayExecution_t fn = get_NtDelayExecution();
    if (!fn) return;
    LARGE_INTEGER interval;
    interval.QuadPart = -(LONGLONG)ms * 10000; /* 100ns units */
    fn(FALSE, &interval);
}

/* NtFlushBuffersFile - no FlushFileBuffers (kernel32) in call stack */
typedef NTSTATUS(NTAPI* NtFlushBuffersFile_t)(HANDLE, PIO_STATUS_BLOCK);
typedef struct _IO_STATUS_BLOCK_NT {
    union { NTSTATUS Status; PVOID Pointer; };
    ULONG_PTR Information;
} IO_STATUS_BLOCK_NT;
inline NtFlushBuffersFile_t get_NtFlushBuffersFile() {
    static NtFlushBuffersFile_t p = nullptr;
    if (!p) {
        HMODULE ntdll = api_resolve::get_module_w(APIRES_OBF_W(L"ntdll.dll"));
        if (ntdll) p = (NtFlushBuffersFile_t)api_resolve::get_proc_a(ntdll, APIRES_OBF_A("NtFlushBuffersFile"));
    }
    return p;
}
inline BOOL flush_handle(HANDLE h) {
    NtFlushBuffersFile_t fn = get_NtFlushBuffersFile();
    if (!fn) return FALSE;
    IO_STATUS_BLOCK_NT iosb = { 0 };
    return NT_SUCCESS(fn(h, (PIO_STATUS_BLOCK)&iosb));
}

/* PID from handle via NtQueryInformationProcess(ProcessBasicInformation) - no GetProcessId (kernel32) */
typedef NTSTATUS(NTAPI* NtQueryInformationProcess_t)(HANDLE, ULONG, PVOID, ULONG, PULONG);
inline NtQueryInformationProcess_t get_NtQIP() {
    static NtQueryInformationProcess_t p = nullptr;
    if (!p) {
        HMODULE ntdll = api_resolve::get_module_w(APIRES_OBF_W(L"ntdll.dll"));
        if (ntdll) p = (NtQueryInformationProcess_t)api_resolve::get_proc_a(ntdll, APIRES_OBF_A("NtQueryInformationProcess"));
    }
    return p;
}
inline DWORD get_process_id(HANDLE h) {
    if (!h || h == INVALID_HANDLE_VALUE) return 0;
    NtQueryInformationProcess_t fn = get_NtQIP();
    if (!fn) return 0;
    PROCESS_BASIC_INFORMATION pbi = { 0 };
    ULONG ret = 0;
    if (!NT_SUCCESS(fn(h, ProcessBasicInformation, &pbi, sizeof(pbi), &ret)))
        return 0;
    return (DWORD)(ULONG_PTR)pbi.UniqueProcessId;
}

/* Process image path via NtOpenProcess + NtQueryInformationProcess(ProcessImageFileName) - no QueryFullProcessImageNameW / OpenProcess */
struct _OA_NT { ULONG Length; ULONG Pad; HANDLE Root; PVOID ObjectName; ULONG Attributes; ULONG Pad2; PVOID SecurityDescriptor; PVOID SecurityQualityOfService; };
struct _CID_NT { HANDLE UniqueProcess; HANDLE UniqueThread; };
typedef NTSTATUS(NTAPI* NtOpenProcess_t)(PHANDLE, ULONG, _OA_NT*, _CID_NT*);
struct _UNICODE_STRING_NT { USHORT Length; USHORT MaximumLength; PWSTR Buffer; };
inline std::wstring get_process_path(DWORD pid) {
    std::wstring out;
    NtOpenProcess_t pNtOpen = nullptr;
    NtQueryInformationProcess_t pNtQIP = get_NtQIP();
    if (!pNtQIP) return out;
    HMODULE ntdll = api_resolve::get_module_w(APIRES_OBF_W(L"ntdll.dll"));
    if (ntdll) pNtOpen = (NtOpenProcess_t)api_resolve::get_proc_a(ntdll, APIRES_OBF_A("NtOpenProcess"));
    if (!pNtOpen) return out;
    HANDLE h = nullptr;
    _OA_NT oa = { sizeof(_OA_NT), 0, 0, 0, 0, 0, 0, 0 };
    _CID_NT cid = { (HANDLE)(ULONG_PTR)pid, 0 };
    if (!NT_SUCCESS(pNtOpen(&h, PROCESS_QUERY_LIMITED_INFORMATION, &oa, &cid)) || !h)
        return out;
    /* ProcessImageFileName (27) returns UNICODE_STRING + buffer; Buffer points into same buffer */
    char buf[sizeof(_UNICODE_STRING_NT) + 520 * sizeof(wchar_t)];
    ULONG retLen = 0;
    if (!NT_SUCCESS(pNtQIP(h, ProcessImageFileName, buf, (ULONG)sizeof(buf), &retLen))) {
        close_handle(h);
        return out;
    }
    _UNICODE_STRING_NT* us = (_UNICODE_STRING_NT*)buf;
    if (us->Buffer && us->Length > 0 && us->Length < 520 * sizeof(wchar_t)) {
        size_t n = us->Length / sizeof(wchar_t);
        out.assign(us->Buffer, n);
    }
    close_handle(h);
    return out;
}

/* Path from existing handle - no extra NtOpenProcess (e.g. when iterating with NtGetNextProcess) */
inline std::wstring get_process_path_from_handle(HANDLE h) {
    std::wstring out;
    if (!h || h == INVALID_HANDLE_VALUE) return out;
    NtQueryInformationProcess_t fn = get_NtQIP();
    if (!fn) return out;
    char buf[sizeof(_UNICODE_STRING_NT) + 520 * sizeof(wchar_t)];
    ULONG retLen = 0;
    if (!NT_SUCCESS(fn(h, ProcessImageFileName, buf, (ULONG)sizeof(buf), &retLen)))
        return out;
    _UNICODE_STRING_NT* us = (_UNICODE_STRING_NT*)buf;
    if (us->Buffer && us->Length > 0 && us->Length < 520 * sizeof(wchar_t)) {
        size_t n = us->Length / sizeof(wchar_t);
        out.assign(us->Buffer, n);
    }
    return out;
}

} /* namespace custom_nt */
