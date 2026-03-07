#pragma once
/* Process enumeration order (detection-aware): when driver loaded, EPROCESS walk first (no usermode handle).
 * Otherwise: NtQuerySystemInformation first (single buffer, no handle per process – EAC detects Toolhelp32
 * because it creates a handle on each process during iteration and triggers EAC callback + logging).
 * Then NtGetNextProcess / NTFS (both open handles when resolving name), CreateToolhelp32Snapshot last.
 * See PROCESS_ENUM_EAC_RESEARCH.md, Geoff Chappell SYSTEM_PROCESS_INFORMATION layout. */

#include <Windows.h>
#include "api_resolve.hpp"
#include <winternl.h>
#include "direct_syscall.hpp"
#include "custom_nt.hpp"
#include <cstdint>
#include <vector>
#include <string>

#ifndef NT_SUCCESS
#define NT_SUCCESS(s) ((((NTSTATUS)(s)) >= 0))
#endif

#define PROCESS_QUERY_LIMITED_INFORMATION 0x1000
#define FILE_PROC_BUFFER_SIZE 65536

/* CLIENT_ID / PCLIENT_ID from <winternl.h> (included above). */

/* NTFS FileProcessIds - derived at runtime (no literal 0x2F from LloydLabs docs) */
#define _NTFS_FPF_BASE 0x2E
static ULONG get_ntfs_file_proc_ids_class() { return _NTFS_FPF_BASE + 1; }

#pragma pack(push, 1)
typedef struct _FILE_PROCESS_IDS_USING_FILE_INFORMATION {
    ULONG NumberOfProcessIdsInList;
    ULONG_PTR ProcessIdList[1];
} FILE_PROCESS_IDS_USING_FILE_INFORMATION, *PFILE_PROCESS_IDS_USING_FILE_INFORMATION;
#pragma pack(pop)

typedef NTSTATUS(NTAPI* NtQueryInformationFile_t)(HANDLE, PIO_STATUS_BLOCK, PVOID, ULONG, ULONG);
typedef NTSTATUS(NTAPI* NtCreateFile_t)(PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES, PIO_STATUS_BLOCK,
    PLARGE_INTEGER, ULONG, ULONG, ULONG, PVOID, ULONG);
typedef NTSTATUS(NTAPI* NtOpenFile_t)(PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES, PIO_STATUS_BLOCK, ULONG, ULONG);

/* NTFS device path tokens - encoded so no literal "Ntfs"/"Device"/"GLOBALROOT" in .rdata (avoids LloydLabs-style signature) */
#define _PE_OBF_KEY  APIRES_OBF_KEY
static const unsigned char _enc_ntfs[]   = { (unsigned char)('N'^_PE_OBF_KEY), (unsigned char)('t'^_PE_OBF_KEY), (unsigned char)('f'^_PE_OBF_KEY), (unsigned char)('s'^_PE_OBF_KEY), 0 };
static const unsigned char _enc_device[] = { (unsigned char)('D'^_PE_OBF_KEY), (unsigned char)('e'^_PE_OBF_KEY), (unsigned char)('v'^_PE_OBF_KEY), (unsigned char)('i'^_PE_OBF_KEY), (unsigned char)('c'^_PE_OBF_KEY), (unsigned char)('e'^_PE_OBF_KEY), 0 };
static const unsigned char _enc_global[] = { (unsigned char)('G'^_PE_OBF_KEY), (unsigned char)('L'^_PE_OBF_KEY), (unsigned char)('O'^_PE_OBF_KEY), (unsigned char)('B'^_PE_OBF_KEY), (unsigned char)('A'^_PE_OBF_KEY), (unsigned char)('L'^_PE_OBF_KEY), (unsigned char)('R'^_PE_OBF_KEY), (unsigned char)('O'^_PE_OBF_KEY), (unsigned char)('O'^_PE_OBF_KEY), (unsigned char)('T'^_PE_OBF_KEY), 0 };
static void _decode_ntfs_path_tokens(wchar_t* out_ntfs, wchar_t* out_device, wchar_t* out_global) {
    for (size_t i = 0; i < 4; i++) out_ntfs[i] = (wchar_t)(_enc_ntfs[i] ^ _PE_OBF_KEY); out_ntfs[4] = L'\0';
    for (size_t i = 0; i < 6; i++) out_device[i] = (wchar_t)(_enc_device[i] ^ _PE_OBF_KEY); out_device[6] = L'\0';
    for (size_t i = 0; i < 10; i++) out_global[i] = (wchar_t)(_enc_global[i] ^ _PE_OBF_KEY); out_global[10] = L'\0';
}

namespace process_enum {

/* Get PIDs via NTFS base device. Paths built at runtime from encoded tokens - no literal Ntfs/Device in binary. */
inline bool enum_pids_ntfs(std::vector<DWORD>& pids) {
    pids.clear();
    HMODULE ntdll = api_resolve::get_module_w(APIRES_OBF_W(L"ntdll.dll"));
    if (!ntdll) return false;

    auto NtQueryInformationFile = (NtQueryInformationFile_t)api_resolve::get_proc_a(ntdll, APIRES_OBF_A("NtQueryInformationFile"));
    auto NtOpenFile = (NtOpenFile_t)api_resolve::get_proc_a(ntdll, APIRES_OBF_A("NtOpenFile"));
    if (!NtQueryInformationFile || !NtOpenFile) return false;

    wchar_t tok_ntfs[8], tok_device[8], tok_global[12];
    _decode_ntfs_path_tokens(tok_ntfs, tok_device, tok_global);

    wchar_t path1[64], path2[32], path3[32];
    wcscpy_s(path1, L"\\??\\"); wcscat_s(path1, tok_global); wcscat_s(path1, L"\\"); wcscat_s(path1, tok_device); wcscat_s(path1, L"\\"); wcscat_s(path1, tok_ntfs);
    wcscpy_s(path2, L"\\"); wcscat_s(path2, tok_device); wcscat_s(path2, L"\\"); wcscat_s(path2, tok_ntfs);
    wcscpy_s(path3, L"\\"); wcscat_s(path3, tok_ntfs); wcscat_s(path3, L"\\");

    const wchar_t* paths[] = { path1, path2, path3 };
    HANDLE hNtfs = INVALID_HANDLE_VALUE;
    for (const wchar_t* pathW : paths) {
        UNICODE_STRING path;
        RtlInitUnicodeString(&path, pathW);
        OBJECT_ATTRIBUTES oa;
        InitializeObjectAttributes(&oa, &path, OBJ_CASE_INSENSITIVE, NULL, NULL);
        IO_STATUS_BLOCK iosb = { 0 };

        if (NT_SUCCESS(NtOpenFile(&hNtfs, GENERIC_READ | SYNCHRONIZE, &oa, &iosb, FILE_SHARE_READ, 0)) && hNtfs != INVALID_HANDLE_VALUE)
            break;
        hNtfs = INVALID_HANDLE_VALUE;
    }

    /* Fallback: NtCreateFile on \??\unavailable for \\.\Ntfs - keep CreateFileW only if NtOpenFile failed on all paths */
    if (hNtfs == INVALID_HANDLE_VALUE) {
        wchar_t dosPath[16];
        wcscpy_s(dosPath, L"\\\\.\\"); wcscat_s(dosPath, tok_ntfs);
        hNtfs = CreateFileW(dosPath, GENERIC_READ | SYNCHRONIZE, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    }

    if (hNtfs == INVALID_HANDLE_VALUE) return false;

    bool ok = false;
    void* buf = custom_nt::alloc(FILE_PROC_BUFFER_SIZE);
    if (buf) {
        memset(buf, 0, FILE_PROC_BUFFER_SIZE);
        IO_STATUS_BLOCK iosb = { 0 };
        if (NT_SUCCESS(NtQueryInformationFile(hNtfs, &iosb, buf, FILE_PROC_BUFFER_SIZE, get_ntfs_file_proc_ids_class()))) {
            auto info = (PFILE_PROCESS_IDS_USING_FILE_INFORMATION)buf;
            for (ULONG i = 0; i < info->NumberOfProcessIdsInList && i < 8192; i++) {
                DWORD pid = (DWORD)(info->ProcessIdList[i] & 0xFFFFFFFF);
                if (pid != 0) pids.push_back(pid);
            }
            ok = true;
        }
        custom_nt::free_mem(buf);
    }
    custom_nt::close_handle(hNtfs);
    return ok;
}

/* Get process exe path for PID. NT-only: NtOpenProcess + NtQueryInformationProcess(ProcessImageFileName); no QueryFullProcessImageNameW / OpenProcess. */
inline std::wstring get_process_path(DWORD pid) {
    return custom_nt::get_process_path(pid);
}

/* Find process by name using NtGetNextProcess. NT-only: custom_nt::get_process_id, get_process_path_from_handle, close_handle. */
inline DWORD find_process_nt(const wchar_t* process_name) {
    init_direct_syscall();
    HANDLE h = nullptr;
    DWORD found = 0;
    for (;;) {
        HANDLE next = nullptr;
        NTSTATUS st = sys_NtGetNextProcess(h, PROCESS_QUERY_LIMITED_INFORMATION, 0, 0, &next);
        if (h) custom_nt::close_handle(h);
        h = next;
        if (!NT_SUCCESS(st) || !h) break;
        DWORD pid = custom_nt::get_process_id(h);
        if (pid == 0) continue;
        std::wstring path = custom_nt::get_process_path_from_handle(h);
        if (path.empty()) continue;
        size_t slash = path.find_last_of(L"\\/");
        std::wstring exe = (slash != std::wstring::npos) ? path.substr(slash + 1) : path;
        if (_wcsicmp(exe.c_str(), process_name) == 0) {
            found = pid;
            break;
        }
    }
    if (h) custom_nt::close_handle(h);
    return found;
}

/* Find process by name using NTFS PIDs + get_process_path (NT-only). Returns PID or 0. */
inline DWORD find_process_ntfs(const wchar_t* process_name) {
    std::vector<DWORD> pids;
    if (!enum_pids_ntfs(pids)) return 0;
    for (DWORD pid : pids) {
        std::wstring path = get_process_path(pid);
        if (path.empty()) continue;
        size_t slash = path.find_last_of(L"\\/");
        std::wstring exe = (slash != std::wstring::npos) ? path.substr(slash + 1) : path;
        if (_wcsicmp(exe.c_str(), process_name) == 0)
            return pid;
    }
    return 0;
}

/* NtQuerySystemInformation(SystemProcessInformation): direct syscall when available; buffer via NtAllocateVirtualMemory.
 * No HeapAlloc, no ntdll call path for query. Offsets: x64 SYSTEM_PROCESS_INFORMATION. ImageName @ 0x38, UniqueProcessId @ 0x50. */
#define SystemProcessInformation 5
#define _SPI_ImageName     0x38
#define _SPI_UniqueProcessId 0x50
inline DWORD find_process_sysinfo(const wchar_t* process_name) {
    init_direct_syscall();
    ULONG size = 0;
    sys_NtQuerySystemInformation(SystemProcessInformation, nullptr, 0, &size);
    if (size == 0) size = 1024 * 1024;
    void* buf = custom_nt::alloc(size);
    if (!buf) return 0;
    memset(buf, 0, size);
    NTSTATUS st = sys_NtQuerySystemInformation(SystemProcessInformation, buf, size, &size);
    DWORD found = 0;
    if (NT_SUCCESS(st)) {
        const char* end = (const char*)buf + size;
        for (const char* p = (const char*)buf; p < end; ) {
            ULONG next = *(const ULONG*)(p + 0);
            if (next == 0 || next > (ULONG)(end - p)) break;
            PVOID pidPtr = *(const PVOID*)(p + _SPI_UniqueProcessId);
            DWORD pid = (DWORD)(ULONG_PTR)pidPtr;
            USHORT len = *(const USHORT*)(p + _SPI_ImageName);
            const wchar_t* nameBuf = *(const wchar_t* const*)(p + _SPI_ImageName + 8);
            if (pid != 0 && nameBuf && len >= 2 && len < 0x2000) {
                const char* entryEnd = p + next;
                const char* nameBufEnd = (const char*)nameBuf + len;
                if ((const char*)nameBuf >= p && nameBufEnd <= entryEnd && nameBufEnd > (const char*)nameBuf) {
                    size_t n = len / sizeof(wchar_t);
                    if (n >= 260) n = 259;
                    wchar_t name[260];
                    for (size_t i = 0; i < n && i < 259; i++) name[i] = nameBuf[i];
                    name[n < 259 ? n : 259] = 0;
                    const wchar_t* basename = name;
                    for (size_t i = n; i > 0; i--)
                        if (name[i - 1] == L'\\' || name[i - 1] == L'/') { basename = &name[i]; break; }
                    if (_wcsicmp(basename, process_name) == 0) {
                        found = pid;
                        break;
                    }
                }
            }
            p += next;
        }
    }
    custom_nt::free_mem(buf);
    return found;
}

/* EAC/UC: no process handles in primary path. Order: sysinfo only (direct syscall, single buffer, no handles). When driver loaded, DotMem::find_process tries EPROCESS first. find_process_nt/ntfs not used in stealth path to avoid handle callbacks. */
inline DWORD find_process_stealth(const wchar_t* process_name) {
    return find_process_sysinfo(process_name);
}

/* Enumerate processes with callback(pid, exePath). EAC/UC: no process handles. Uses NtQuerySystemInformation buffer only (direct syscall); path = ImageName from buffer. No __try (C2712: cannot mix SEH with C++ object unwinding). */
template<typename F>
inline bool enumerate_stealth(F callback) {
    init_direct_syscall();
    ULONG size = 0;
    sys_NtQuerySystemInformation(SystemProcessInformation, nullptr, 0, &size);
    if (size == 0) size = 1024 * 1024;
    void* buf = custom_nt::alloc(size);
    if (!buf) return false;
    memset(buf, 0, size);
    NTSTATUS st = sys_NtQuerySystemInformation(SystemProcessInformation, buf, size, &size);
    bool any = false;
    if (NT_SUCCESS(st)) {
        const char* end = (const char*)buf + size;
        for (const char* p = (const char*)buf; p < end; ) {
            ULONG next = *(const ULONG*)(p + 0);
            if (next == 0 || next > (ULONG)(end - p)) break;
            PVOID pidPtr = *(const PVOID*)(p + _SPI_UniqueProcessId);
            DWORD pid = (DWORD)(ULONG_PTR)pidPtr;
            USHORT len = *(const USHORT*)(p + _SPI_ImageName);
            const wchar_t* nameBuf = *(const wchar_t* const*)(p + _SPI_ImageName + 8);
            if (pid != 0 && nameBuf && len >= 2 && len < 0x2000) {
                const char* entryEnd = p + next;
                const char* nameBufEnd = (const char*)nameBuf + len;
                if ((const char*)nameBuf >= p && nameBufEnd <= entryEnd && nameBufEnd > (const char*)nameBuf) {
                    size_t n = len / sizeof(wchar_t);
                    if (n >= 260) n = 259;
                    wchar_t name[260];
                    for (size_t i = 0; i < n && i < 259; i++) name[i] = nameBuf[i];
                    name[n < 259 ? n : 259] = 0;
                    callback(pid, std::wstring(name));
                    any = true;
                }
            }
            p += next;
        }
    }
    custom_nt::free_mem(buf);
    return any;
}

} /* namespace process_enum */
