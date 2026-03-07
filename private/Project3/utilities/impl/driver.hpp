#pragma once

#include <Windows.h>
#include <TlHelp32.h>
#include "../direct_syscall.hpp"
#include "../custom_nt.hpp"
#include "../process_enum.hpp"
/* spoofer_hooks disabled - inline hooks on RtlGetVersion/NtQuerySystemInformation cause instant crash */
#include <cstdint>
#include <mutex>
#include <winternl.h>
#include <winioctl.h>

uintptr_t virtualaddy;
uintptr_t cr3;

#include "../../flush_comm_config.h"
#include "../../flush_comm_obfuscate.h"
#if FLUSHCOMM_USE_LAZY_IMPORT
#include "../lazy_import.hpp"
#define DRV_CreateFileW       LazyImport::CreateFileW
#define DRV_DeviceIoControl    LazyImport::DeviceIoControl
#define DRV_RegCreateKeyExW    LazyImport::RegCreateKeyExW
#define DRV_RegSetValueExW     LazyImport::RegSetValueExW
#define DRV_RegQueryValueExW   LazyImport::RegQueryValueExW
#define DRV_RegOpenKeyExW      LazyImport::RegOpenKeyExW
#define DRV_RegCloseKey        LazyImport::RegCloseKey
#define DRV_OpenFileMappingW   LazyImport::OpenFileMappingW
#define DRV_CreateFileMappingW LazyImport::CreateFileMappingW
#define DRV_GetWindowsDirectoryW LazyImport::GetWindowsDirectoryW
#define DRV_MapViewOfFile     LazyImport::MapViewOfFile
#define DRV_UnmapViewOfFile   LazyImport::UnmapViewOfFile
#define DRV_FlushFileBuffers  LazyImport::FlushFileBuffers
#define DRV_CloseHandle       LazyImport::CloseHandle
#else
#define DRV_CreateFileW       ::CreateFileW
#define DRV_DeviceIoControl   ::DeviceIoControl
#define DRV_RegCreateKeyExW   ::RegCreateKeyExW
#define DRV_RegSetValueExW    ::RegSetValueExW
#define DRV_RegQueryValueExW  ::RegQueryValueExW
#define DRV_RegOpenKeyExW     ::RegOpenKeyExW
#define DRV_RegCloseKey       ::RegCloseKey
#define DRV_OpenFileMappingW  ::OpenFileMappingW
#define DRV_CreateFileMappingW ::CreateFileMappingW
#define DRV_GetWindowsDirectoryW ::GetWindowsDirectoryW
#define DRV_MapViewOfFile     ::MapViewOfFile
#define DRV_UnmapViewOfFile   ::UnmapViewOfFile
#define DRV_FlushFileBuffers  ::FlushFileBuffers
#define DRV_CloseHandle       ::CloseHandle
#endif
typedef NTSTATUS(NTAPI* NtOpenFile_t)(PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES, PIO_STATUS_BLOCK, ULONG, ULONG);
/* Device path obfuscation: key from FLUSHCOMM_OBF_BASE (MAGIC-derived) - no public 0x9D literal */
#ifndef DRV_OBF_KEY
#define DRV_OBF_KEY  (FLUSHCOMM_OBF_BASE)
#endif
static const unsigned char OBF_DEV_BEEP[]  = { (unsigned char)('B'^DRV_OBF_KEY), (unsigned char)('e'^DRV_OBF_KEY), (unsigned char)('e'^DRV_OBF_KEY), (unsigned char)('p'^DRV_OBF_KEY), 0 };
static const unsigned char OBF_DEV_NULL[]  = { (unsigned char)('N'^DRV_OBF_KEY), (unsigned char)('u'^DRV_OBF_KEY), (unsigned char)('l'^DRV_OBF_KEY), (unsigned char)('l'^DRV_OBF_KEY), 0 };
static const unsigned char OBF_DEV_PEAUTH[]= { (unsigned char)('P'^DRV_OBF_KEY), (unsigned char)('E'^DRV_OBF_KEY), (unsigned char)('A'^DRV_OBF_KEY), (unsigned char)('u'^DRV_OBF_KEY), (unsigned char)('t'^DRV_OBF_KEY), (unsigned char)('h'^DRV_OBF_KEY), 0 };
/* Device path prefix built from chars - no literal L"\\\\.\\" or L"\\Device\\" in .rdata */
static void get_device_path_w(int index, wchar_t* out, size_t out_chars) {
    const unsigned char* enc = nullptr; size_t enc_len = 0;
    if (index == 0) { enc = OBF_DEV_BEEP;   enc_len = sizeof(OBF_DEV_BEEP)-1; }
    else if (index == 1) { enc = OBF_DEV_NULL;   enc_len = sizeof(OBF_DEV_NULL)-1; }
    else { enc = OBF_DEV_PEAUTH; enc_len = sizeof(OBF_DEV_PEAUTH)-1; }
    if (out_chars < 5 + enc_len + 1) return;
    out[0] = L'\\'; out[1] = L'\\'; out[2] = L'.'; out[3] = L'\\';
    for (size_t i = 0; i < enc_len; i++)
        out[4 + i] = (wchar_t)(enc[i] ^ DRV_OBF_KEY);
    out[4 + enc_len] = L'\0';
}
static void get_device_native_w(int index, wchar_t* out, size_t out_chars) {
    const unsigned char* enc = nullptr; size_t enc_len = 0;
    if (index == 0) { enc = OBF_DEV_BEEP;   enc_len = sizeof(OBF_DEV_BEEP)-1; }
    else if (index == 1) { enc = OBF_DEV_NULL;   enc_len = sizeof(OBF_DEV_NULL)-1; }
    else { enc = OBF_DEV_PEAUTH; enc_len = sizeof(OBF_DEV_PEAUTH)-1; }
    if (out_chars < 8 + enc_len + 1) return;
    out[0] = L'\\'; out[1] = L'D'; out[2] = L'e'; out[3] = L'v'; out[4] = L'i'; out[5] = L'c'; out[6] = L'e'; out[7] = L'\\';
    for (size_t i = 0; i < enc_len; i++)
        out[8 + i] = (wchar_t)(enc[i] ^ DRV_OBF_KEY);
    out[8 + enc_len] = L'\0';
}
/* Prefer NtOpenFile (no CreateFileW in call stack - avoids monitored API). Fallback to CreateFileW only if needed. */
static HANDLE open_hooked_device(int index) {
    if (index < 0 || index > 2) index = 0;
    static NtOpenFile_t NtOpenFile = nullptr;
    if (!NtOpenFile) {
        NtOpenFile = (NtOpenFile_t)api_resolve::get_proc_a(api_resolve::get_module_w(APIRES_OBF_W(L"ntdll.dll")), APIRES_OBF_A("NtOpenFile"));
    }
    if (NtOpenFile) {
        wchar_t nativeBuf[32];
        get_device_native_w(index, nativeBuf, 32);
        UNICODE_STRING path;
        RtlInitUnicodeString(&path, nativeBuf);
        OBJECT_ATTRIBUTES oa;
        InitializeObjectAttributes(&oa, &path, OBJ_CASE_INSENSITIVE, NULL, NULL);
        IO_STATUS_BLOCK iosb = { 0 };
        HANDLE hn = nullptr;
        if (NT_SUCCESS(NtOpenFile(&hn, GENERIC_READ | GENERIC_WRITE, &oa, &iosb, FILE_SHARE_READ, FILE_NON_DIRECTORY_FILE)) && hn)
            return hn;
    }
    wchar_t pathBuf[32];
    get_device_path_w(index, pathBuf, 32);
    HANDLE h = DRV_CreateFileW(pathBuf, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    return (h != INVALID_HANDLE_VALUE) ? h : INVALID_HANDLE_VALUE;
}
/* Suffix derived from MAGIC at runtime - no literal in .rdata (avoids signature). */
static const wchar_t* get_obf_suffix() {
    static wchar_t buf[8];
    static bool once = false;
    if (!once) {
        unsigned long long v = (FLUSHCOMM_MAGIC >> 12) & 0xFFFFFFu;
        static const wchar_t hex[] = L"0123456789abcdef";
        for (int i = 5; i >= 0; i--) { buf[i] = hex[v & 0xF]; v >>= 4; }
        buf[6] = L'\0';
        once = true;
    }
    return buf;
}
/* Registry path built at runtime from FLUSHCOMM_SECTION_SEED + suffix (no MdmTrace literal). */
static const wchar_t* get_reg_path() {
    static wchar_t buf[128];
    static bool once = false;
    if (!once) { swprintf_s(buf, L"SOFTWARE\\%06X\\%ws", (unsigned)FLUSHCOMM_SECTION_SEED, get_obf_suffix()); once = true; }
    return buf;
}
static int read_hooked_device_index() {
    HKEY hKey = nullptr;
    if (DRV_RegOpenKeyExW(HKEY_LOCAL_MACHINE, get_reg_path(), 0, KEY_READ | KEY_WOW64_64KEY, &hKey) != ERROR_SUCCESS)
        return 0;
    ULONG64 idx = 0;
    DWORD type = 0, size = sizeof(idx);
    wchar_t valName[16];
    obf_decode_str(OBF_HookedDevice, OBF_HookedDevice_LEN, valName, 16);
    if (DRV_RegQueryValueExW(hKey, valName, nullptr, &type, (BYTE*)&idx, &size) == ERROR_SUCCESS && size == sizeof(idx))
        { DRV_RegCloseKey(hKey); return (int)(idx <= 2 ? idx : 0); }
    DRV_RegCloseKey(hKey);
    return 0;
}

/* FlushComm protocol - values from flush_comm_config.h (shared with driver) */
#ifndef FILE_DEVICE_BEEP
#define FILE_DEVICE_BEEP 0x00000001
#endif
#define IOCTL_REXCOMM     CTL_CODE(FILE_DEVICE_BEEP, FLUSHCOMM_IOCTL_FUNC, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_REXCOMM_PING CTL_CODE(FILE_DEVICE_BEEP, FLUSHCOMM_IOCTL_PING, METHOD_BUFFERED, FILE_ANY_ACCESS)

typedef long NTSTATUS;
#ifndef STATUS_SUCCESS
#define STATUS_SUCCESS ((NTSTATUS)0)
#endif

/* Must match driver flush_comm.hpp exactly */
typedef enum _REQUEST_TYPE : ULONG {
    REQ_READ,
    REQ_WRITE,
    // REQ_READ_BATCH removed - not used
    REQ_MAINBASE,
    REQ_GET_DIR_BASE,
    REQ_GET_GUARDED_REGION,
    REQ_MOUSE_MOVE,
    REQ_GET_PID_BY_NAME,
    REQ_INIT = 99,
} REQUEST_TYPE;

typedef struct _REQUEST_DATA {
    ULONG64* MaggicCode;
    ULONG Type;
    PVOID Arguments;
    NTSTATUS* Status;
} REQUEST_DATA, * PREQUEST_DATA;

typedef struct _REQUEST_READ {
    ULONG ProcessId;
    PVOID Dest;
    PVOID Src;
    ULONG Size;
    ULONG bPhysicalMem;
} REQUEST_READ, * PREQUEST_READ;

typedef struct _REQUEST_READ_BATCH {
    ULONG ProcessId;
    ULONG Count;
} REQUEST_READ_BATCH, * PREQUEST_READ_BATCH;

typedef struct _REQUEST_WRITE {
    ULONG ProcessId;
    PVOID Dest;
    PVOID Src;
    ULONG Size;
    ULONG bPhysicalMem;
} REQUEST_WRITE, * PREQUEST_WRITE;

typedef struct _REQUEST_MAINBASE {
    ULONG ProcessId;
    ULONGLONG* OutAddress;
} REQUEST_MAINBASE, * PREQUEST_MAINBASE;

typedef struct _REQUEST_GET_DIR_BASE {
    ULONG ProcessId;
    ULONGLONG* OutCr3;
    ULONGLONG InBase;  /* Optional: usermode base from find_image for validation */
} REQUEST_GET_DIR_BASE, * PREQUEST_GET_DIR_BASE;

typedef struct _REQUEST_GET_GUARDED_REGION {
    ULONGLONG* OutAddress;
} REQUEST_GET_GUARDED_REGION, * PREQUEST_GET_GUARDED_REGION;

typedef struct _REQUEST_MOUSE_MOVE {
    LONG DeltaX;
    LONG DeltaY;
    USHORT ButtonFlags;
} REQUEST_MOUSE_MOVE, * PREQUEST_MOUSE_MOVE;

#define REQUEST_GET_PID_BY_NAME_NAMELEN 16
typedef struct _REQUEST_GET_PID_BY_NAME {
    CHAR Name[REQUEST_GET_PID_BY_NAME_NAMELEN];
    ULONG OutPid;
} REQUEST_GET_PID_BY_NAME, * PREQUEST_GET_PID_BY_NAME;

namespace DotMem {
    HANDLE driver_handle = INVALID_HANDLE_VALUE;
#if FLUSHCOMM_USE_ALPC_FALLBACK
    static HANDLE g_alpc_port = nullptr;
    static bool g_use_alpc = false;
#endif
    INT32 process_id = 0;

    inline static std::mutex g_driver_mutex;

    static PVOID g_shared_buf = nullptr;
    static constexpr SIZE_T SHARED_BUF_SIZE = 512;
#if FLUSHCOMM_USE_SECTION
    static HANDLE g_section_handle = nullptr;
    static constexpr SIZE_T SECTION_DATA_OFFSET = FLUSHCOMM_DATA_OFFSET;
    static constexpr SIZE_T SECTION_DATA_SIZE = FLUSHCOMM_SECTION_SIZE - FLUSHCOMM_DATA_OFFSET;
#endif

#if FLUSHCOMM_USE_ALPC_FALLBACK
#include "alpc_fallback.hpp"
#endif

    static DWORD g_last_send_tick = 0;

    static bool write_registry(PVOID bufAddr, DWORD pid) {
        HKEY hKey = nullptr;
        LSTATUS ls = DRV_RegCreateKeyExW(HKEY_LOCAL_MACHINE, get_reg_path(), 0, NULL,
            REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS | KEY_WOW64_64KEY, NULL, &hKey, NULL);
        if (ls != ERROR_SUCCESS) {
            /* ERROR_ACCESS_DENIED (5) = need Administrator */
            return false;
        }

        ULONG64 addr = (ULONG64)(uintptr_t)bufAddr;
        ULONG64 pid64 = (ULONG64)pid;
        wchar_t valBuf[16], valPid[16];
        obf_decode_str(OBF_SharedBuffer, OBF_SharedBuffer_LEN, valBuf, 16);
        obf_decode_str(OBF_SharedPid, OBF_SharedPid_LEN, valPid, 16);
        ls = DRV_RegSetValueExW(hKey, valBuf, 0, REG_QWORD, (BYTE*)&addr, sizeof(addr));
        if (ls == ERROR_SUCCESS)
            ls = DRV_RegSetValueExW(hKey, valPid, 0, REG_QWORD, (BYTE*)&pid64, sizeof(pid64));
        DRV_RegCloseKey(hKey);
        return (ls == ERROR_SUCCESS);
    }

    static bool is_admin() {
        BOOL elevated = FALSE;
        HANDLE hToken = nullptr;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
            return false;
        TOKEN_ELEVATION tkElev = { 0 };
        DWORD cb = sizeof(tkElev);
        if (GetTokenInformation(hToken, TokenElevation, &tkElev, sizeof(tkElev), &cb))
            elevated = tkElev.TokenIsElevated ? TRUE : FALSE;
        CloseHandle(hToken);
        return elevated != FALSE;
    }

    bool find_driver() {
        const int max_attempts = 5;
        const int delay_ms = 500;

        if (!is_admin())
            return false;

#if FLUSHCOMM_USE_LAZY_IMPORT
        if (!LazyImport::Init())
            return false;
#endif

#if FLUSHCOMM_USE_SECTION
#if FLUSHCOMM_USE_FILEBACKED_SECTION
        /* File-backed section: no named object - avoids \\BaseNamedObjects\\Global scan.
         * Path = %%SystemRoot%%\\Temp\\Fx<12hex>.tmp (MAGIC-derived, unique per build). */
        if (!g_section_handle) {
            WCHAR winDir[MAX_PATH] = { 0 };
            if (DRV_GetWindowsDirectoryW(winDir, MAX_PATH) > 0) {
                WCHAR path[MAX_PATH];
                ULONG64 m = FLUSHCOMM_MAGIC & 0xFFFFFFFFFFFFull;
                static const wchar_t hex[] = L"0123456789abcdef";
                swprintf_s(path, L"%ls\\Temp\\Fx", winDir);
                size_t len = wcslen(path);
                for (int i = 11; i >= 0; i--) path[len++] = hex[(m >> (i * 4)) & 0xF];
                path[len] = L'\0';
                wcscat_s(path, L".tmp");
                HANDLE hFile = DRV_CreateFileW(path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                    nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
                if (hFile != INVALID_HANDLE_VALUE) {
                    HANDLE hMap = DRV_CreateFileMappingW(hFile, nullptr, PAGE_READWRITE, 0, FLUSHCOMM_SECTION_SIZE, nullptr);
                    DRV_CloseHandle(hFile);
                    if (hMap) {
                        g_section_handle = hMap;
                    }
                }
            }
        }
#endif
        /* Named section fallback: OpenFileMapping / CreateFileMapping with name. */
        if (!g_section_handle) {
            WCHAR secName[64];
            swprintf_s(secName, L"Global\\%06X%ws", (unsigned)FLUSHCOMM_SECTION_SEED, get_obf_suffix());
            for (int retry = 0; retry < FLUSHCOMM_SECTION_RETRY_COUNT && !g_section_handle; ++retry) {
                if (retry > 0)
                    custom_nt::delay_ms(FLUSHCOMM_SECTION_RETRY_DELAY_MS);
                g_section_handle = DRV_OpenFileMappingW(FILE_MAP_READ | FILE_MAP_WRITE, FALSE, secName);
                if (!g_section_handle) {
                    swprintf_s(secName, L"%06X%ws", (unsigned)FLUSHCOMM_SECTION_SEED, get_obf_suffix());
                    g_section_handle = DRV_OpenFileMappingW(FILE_MAP_READ | FILE_MAP_WRITE, FALSE, secName);
                }
                if (!g_section_handle) {
                    swprintf_s(secName, L"Local\\%06X%ws", (unsigned)FLUSHCOMM_SECTION_SEED, get_obf_suffix());
                    g_section_handle = DRV_OpenFileMappingW(FILE_MAP_READ | FILE_MAP_WRITE, FALSE, secName);
                }
            }
            if (!g_section_handle) {
                swprintf_s(secName, L"Global\\%06X%ws", (unsigned)FLUSHCOMM_SECTION_SEED, get_obf_suffix());
                g_section_handle = DRV_CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, FLUSHCOMM_SECTION_SIZE, secName);
            }
        }
        if (g_section_handle) {
            g_shared_buf = DRV_MapViewOfFile(g_section_handle, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, FLUSHCOMM_SECTION_SIZE);
            if (!g_shared_buf) {
                DRV_CloseHandle(g_section_handle);
                g_section_handle = nullptr;
            }
        }
#if FLUSHCOMM_REJECT_REGISTRY_FALLBACK
        /* EAC: MmCopyVirtualMemory detected (UC 496628) - reject registry fallback, require section */
        if (!g_shared_buf) {
            WCHAR diagName[64];
            swprintf_s(diagName, L"Global\\%06X%ws", (unsigned)FLUSHCOMM_SECTION_SEED, get_obf_suffix());
            printf("[DRIVER] Section open failed. Tried: %ls (MAGIC-derived name). If driver.sys was built separately, do a full Rebuild Solution so driver+usermode share the same FLUSHCOMM_MAGIC.\n", diagName);
            return false;
        }
#endif
#endif
        if (!g_shared_buf) {
#if !FLUSHCOMM_REJECT_REGISTRY_FALLBACK
            /* Last resort: NtAllocateVirtualMemory + registry (no VirtualAlloc in call stack) */
            g_shared_buf = custom_nt::alloc(SHARED_BUF_SIZE);
            if (!g_shared_buf) return false;
            if (!write_registry(g_shared_buf, GetCurrentProcessId()))
                return false;
#else
            return false;
#endif
        }

        int hooked_idx = read_hooked_device_index();
        /* Anti-pattern: small random delay before first open to avoid instant CreateFile (some ACs flag) */
        custom_nt::delay_ms((DWORD)(GetTickCount64() % 120));
        for (int attempt = 0; attempt < max_attempts; attempt++) {
            /* Try devices: prefer hooked_idx, then try others */
            int indices[3] = { hooked_idx, (hooked_idx + 1) % 3, (hooked_idx + 2) % 3 };
            for (int i = 0; i < 3; i++) {
                int idx = indices[i];
                driver_handle = open_hooked_device(idx);
                if (driver_handle == INVALID_HANDLE_VALUE) continue;
                bool handshakeOk = false;
#if FLUSHCOMM_USE_IOCTL_PING
                /* Legacy: IOCTL PING */
                ULONG64 pingOut = 0;
                DWORD returned = 0;
#if FLUSHCOMM_USE_DIRECT_SYSCALL
                { IO_STATUS_BLOCK iosb = { 0 }; handshakeOk = NT_SUCCESS(sys_NtDeviceIoControlFile(driver_handle, 0, nullptr, nullptr, &iosb, IOCTL_REXCOMM_PING, nullptr, 0, &pingOut, sizeof(pingOut))) && iosb.Information >= sizeof(ULONG64) && pingOut == FLUSHCOMM_MAGIC; }
#else
                handshakeOk = (DRV_DeviceIoControl(driver_handle, IOCTL_REXCOMM_PING, nullptr, 0, &pingOut, sizeof(pingOut), &returned, nullptr) && returned >= sizeof(ULONG64) && pingOut == FLUSHCOMM_MAGIC);
#endif
#else
                /* FlushFileBuffers handshake - no IOCTL; uses shared buffer + REQ_INIT */
                BYTE* buf = (BYTE*)g_shared_buf;
                ZeroMemory(buf, g_section_handle ? FLUSHCOMM_SECTION_SIZE : SHARED_BUF_SIZE);
                *(ULONG64*)buf = FLUSHCOMM_MAGIC;
                NTSTATUS* pStatus;
                if (g_section_handle) {
                    *(ULONG*)(buf + 8) = (ULONG)REQ_INIT;
                    pStatus = (NTSTATUS*)(buf + FLUSHCOMM_STATUS_OFFSET);
                } else {
                    ((REQUEST_DATA*)(buf + 16))->MaggicCode = (ULONG64*)buf;
                    ((REQUEST_DATA*)(buf + 16))->Type = REQ_INIT;
                    ((REQUEST_DATA*)(buf + 16))->Arguments = nullptr;
                    ((REQUEST_DATA*)(buf + 16))->Status = (NTSTATUS*)(buf + 64);
                    pStatus = (NTSTATUS*)(buf + 64);
                }
                *pStatus = (NTSTATUS)0xDEADBEEF;
                custom_nt::flush_handle(driver_handle);
                handshakeOk = (*pStatus == STATUS_SUCCESS);
#endif
                if (handshakeOk)
                    return true;
                DRV_CloseHandle(driver_handle);
                driver_handle = INVALID_HANDLE_VALUE;
            }
            if (attempt < max_attempts - 1)
                custom_nt::delay_ms(delay_ms + (DWORD)(GetTickCount64() % 150));
        }
#if FLUSHCOMM_USE_ALPC_FALLBACK
        /* ALPC fallback: no device handle needed; port name from FLUSHCOMM_MAGIC (unique per build) */
        if (g_shared_buf && driver_handle == INVALID_HANDLE_VALUE && try_alpc_connect())
            return true;
#endif
        return false;
    }

    static bool send_request(REQUEST_TYPE type, PVOID args) {
        if (!g_shared_buf) return false;
#if FLUSHCOMM_USE_ALPC_FALLBACK
        if (g_use_alpc)
            return send_request_alpc(type, args);
#endif
        if (driver_handle == INVALID_HANDLE_VALUE) return false;
        // Single throttle point for all driver ops (read/write/mouse). Per-op only - no double throttle in read_physical.
#if FLUSHCOMM_THROTTLE_MS || FLUSHCOMM_JITTER_MS
        static DWORD g_last_send_tick = 0;
        DWORD now = GetTickCount();
        if (g_last_send_tick != 0) {
            DWORD throttle = (DWORD)FLUSHCOMM_THROTTLE_MS;
            DWORD jitter = (DWORD)((FLUSHCOMM_JITTER_MS > 0) ? (GetTickCount() % (FLUSHCOMM_JITTER_MS + 1)) : 0);
            DWORD minGap = throttle + jitter;
            if (now - g_last_send_tick < minGap)
                custom_nt::delay_ms(minGap - (now - g_last_send_tick));
        }
        g_last_send_tick = GetTickCount();
#endif
        BYTE* buf = (BYTE*)g_shared_buf;
        SIZE_T bufSize = g_section_handle ? FLUSHCOMM_SECTION_SIZE : SHARED_BUF_SIZE;
        ZeroMemory(buf, bufSize);
        *(ULONG64*)buf = FLUSHCOMM_MAGIC;
        if (g_section_handle) {
            *(ULONG*)(buf + 8) = (ULONG)type;
            if (args) memcpy(buf + 16, args, 64);
        } else {
            ((REQUEST_DATA*)(buf + 16))->MaggicCode = (ULONG64*)buf;
            ((REQUEST_DATA*)(buf + 16))->Type = (ULONG)type;
            ((REQUEST_DATA*)(buf + 16))->Arguments = args ? (buf + FLUSHCOMM_STATUS_OFFSET) : nullptr;
            ((REQUEST_DATA*)(buf + 16))->Status = (NTSTATUS*)(buf + 64);
            if (args) memcpy(buf + FLUSHCOMM_STATUS_OFFSET, args, 64);
        }
#if FLUSHCOMM_USE_FLUSH_BUFFERS
        custom_nt::flush_handle(driver_handle);
#else
        /* IOCTL path disabled by default - set FLUSHCOMM_USE_FLUSH_BUFFERS 1 to avoid detected vector */
        DWORD returned = 0;
#if FLUSHCOMM_USE_DIRECT_SYSCALL
        { IO_STATUS_BLOCK iosb = { 0 }; sys_NtDeviceIoControlFile(driver_handle, 0, nullptr, nullptr, &iosb, IOCTL_REXCOMM, nullptr, 0, nullptr, 0); }
#else
        DRV_DeviceIoControl(driver_handle, IOCTL_REXCOMM, nullptr, 0, nullptr, 0, &returned, nullptr);
#endif
#endif
        return true;
    }

    void read_physical(PVOID address, PVOID buffer, DWORD size) {
        // Throttle only in send_request() - do NOT throttle here. Each read_physical calls send_request
        // (once or per chunk); double throttle was causing 4-8ms per read and 1-6 FPS (hundreds of reads/frame).
        const std::lock_guard<std::mutex> lock(g_driver_mutex);
#if FLUSHCOMM_USE_SECTION
        if (g_section_handle) {
            BYTE* buf = (BYTE*)g_shared_buf;
            for (DWORD offset = 0; offset < size; ) {
                DWORD chunk = (DWORD)((SIZE_T)(size - offset) < SECTION_DATA_SIZE ? (size - offset) : (DWORD)SECTION_DATA_SIZE);
                REQUEST_READ r = { 0 };
                r.ProcessId = (ULONG)process_id;
                r.Src = (PVOID)((BYTE*)address + offset);
                r.Size = chunk;
                r.bPhysicalMem = 1;
                send_request(REQ_READ, &r);
                // Check status: driver writes at FLUSHCOMM_STATUS_OFFSET
                NTSTATUS* pStatus = (NTSTATUS*)(buf + FLUSHCOMM_STATUS_OFFSET);
                if (*pStatus != STATUS_SUCCESS) {
                    // Read failed - zero out buffer chunk to indicate failure
                    ZeroMemory((BYTE*)buffer + offset, chunk);
                    break;
                }
                memcpy((BYTE*)buffer + offset, (BYTE*)g_shared_buf + SECTION_DATA_OFFSET, chunk);
                offset += chunk;
                if (offset >= size) break;
            }
        } else
#endif
        {
            REQUEST_READ r = { 0 };
            r.ProcessId = (ULONG)process_id;
            r.Dest = buffer;
            r.Src = address;
            r.Size = size;
            r.bPhysicalMem = 1;
            send_request(REQ_READ, &r);
        }
    }

    /* Batch reads removed - not used and causing issues. Using individual reads instead. */

    void write_physical(PVOID address, PVOID buffer, DWORD size) {
        const std::lock_guard<std::mutex> lock(g_driver_mutex);
#if FLUSHCOMM_USE_SECTION
        if (g_section_handle) {
            for (DWORD offset = 0; offset < size; ) {
                DWORD chunk = (DWORD)((SIZE_T)(size - offset) < SECTION_DATA_SIZE ? (size - offset) : (DWORD)SECTION_DATA_SIZE);
                memcpy((BYTE*)g_shared_buf + SECTION_DATA_OFFSET, (BYTE*)buffer + offset, chunk);
                REQUEST_WRITE r = { 0 };
                r.ProcessId = (ULONG)process_id;
                r.Dest = (PVOID)((BYTE*)address + offset);
                r.Size = chunk;
                r.bPhysicalMem = 1;
                send_request(REQ_WRITE, &r);
                offset += chunk;
                if (offset >= size) break;
            }
        } else
#endif
        {
            REQUEST_WRITE r = { 0 };
            r.ProcessId = (ULONG)process_id;
            r.Dest = address;
            r.Src = buffer;
            r.Size = size;
            r.bPhysicalMem = 1;
            send_request(REQ_WRITE, &r);
        }
    }

    void move_mouse(long x, long y, unsigned short button) {
        const std::lock_guard<std::mutex> lock(g_driver_mutex);
        REQUEST_MOUSE_MOVE r = { 0 };
        r.DeltaX = (LONG)x;
        r.DeltaY = (LONG)y;
        r.ButtonFlags = (USHORT)button;
        send_request(REQ_MOUSE_MOVE, &r);
    }

    uintptr_t fetch_cr3(uintptr_t base_for_validation = 0) {
        uintptr_t cr3_value = 0;
        REQUEST_GET_DIR_BASE r = { 0 };
        r.ProcessId = (ULONG)process_id;
        r.InBase = (ULONGLONG)base_for_validation;
        const std::lock_guard<std::mutex> lock(g_driver_mutex);
#if FLUSHCOMM_USE_SECTION
        if (g_section_handle) {
            send_request(REQ_GET_DIR_BASE, &r);
            cr3_value = *(uintptr_t*)((BYTE*)g_shared_buf + SECTION_DATA_OFFSET);
        } else
#endif
        {
            r.OutCr3 = (ULONGLONG*)&cr3_value;
            send_request(REQ_GET_DIR_BASE, &r);
        }
        return cr3_value;
    }

    uintptr_t find_image() {
        uintptr_t image_address = 0;
        REQUEST_MAINBASE r = { 0 };
        r.ProcessId = (ULONG)process_id;
        const std::lock_guard<std::mutex> lock(g_driver_mutex);
#if FLUSHCOMM_USE_SECTION
        if (g_section_handle) {
            send_request(REQ_MAINBASE, &r);
            image_address = *(uintptr_t*)((BYTE*)g_shared_buf + SECTION_DATA_OFFSET);
        } else
#endif
        {
            r.OutAddress = (ULONGLONG*)&image_address;
            send_request(REQ_MAINBASE, &r);
        }
        return image_address;
    }

    uintptr_t get_guarded_region() {
        uintptr_t guarded_address = 0;
        const std::lock_guard<std::mutex> lock(g_driver_mutex);
#if FLUSHCOMM_USE_SECTION
        if (g_section_handle) {
            send_request(REQ_GET_GUARDED_REGION, nullptr);
            guarded_address = *(uintptr_t*)((BYTE*)g_shared_buf + SECTION_DATA_OFFSET);
        } else
#endif
        {
            REQUEST_GET_GUARDED_REGION r = { 0 };
            r.OutAddress = (ULONGLONG*)&guarded_address;
            send_request(REQ_GET_GUARDED_REGION, &r);
        }
        return guarded_address;
    }

    /* Resolve PID by name via driver EPROCESS list walk (no NtQuerySystemInformation/Toolhelp32). Returns 0 if driver unavailable or not found. */
    static DWORD get_pid_by_name(const wchar_t* process_name) {
        if (!process_name || driver_handle == INVALID_HANDLE_VALUE || !g_shared_buf) return 0;
        const wchar_t* base = process_name;
        for (const wchar_t* p = process_name; *p; p++) { if (*p == L'\\' || *p == L'/') base = p + 1; }
        REQUEST_GET_PID_BY_NAME r = { { 0 }, 0 };
        for (int i = 0; i < REQUEST_GET_PID_BY_NAME_NAMELEN - 1 && base[i]; i++)
            r.Name[i] = (char)(base[i] >= 0 && base[i] < 128 ? base[i] : '?');
        r.Name[REQUEST_GET_PID_BY_NAME_NAMELEN - 1] = '\0';
        {
            const std::lock_guard<std::mutex> lock(g_driver_mutex);
            send_request(REQ_GET_PID_BY_NAME, &r);
        }
        BYTE* buf = (BYTE*)g_shared_buf;
        NTSTATUS st = 0;
        DWORD pid = 0;
#if FLUSHCOMM_USE_SECTION
        if (g_section_handle) {
            pid = *(ULONG*)(buf + SECTION_DATA_OFFSET);
            st = *(NTSTATUS*)(buf + FLUSHCOMM_STATUS_OFFSET);
        } else
#endif
        {
            memcpy(&r, buf + FLUSHCOMM_STATUS_OFFSET, sizeof(r));
            pid = r.OutPid;
            st = *(NTSTATUS*)(buf + 64);
        }
        return (st == 0 && pid) ? pid : 0;
    }

    /* Stealth enum: driver EPROCESS walk first (if loaded), then NtQuerySystemInformation, NTFS, NtGetNextProcess, CreateToolhelp32Snapshot last */
    INT32 find_process(LPCTSTR process_name) {
        DWORD pid = get_pid_by_name((const wchar_t*)process_name);
        if (pid) { process_id = pid; return (INT32)pid; }
        pid = process_enum::find_process_stealth((const wchar_t*)process_name);
        if (pid) { process_id = pid; return (INT32)pid; }
        return 0;
    }

    void close_driver() {
        if (driver_handle != INVALID_HANDLE_VALUE) {
            DRV_CloseHandle(driver_handle);
            driver_handle = INVALID_HANDLE_VALUE;
        }
        if (g_shared_buf) {
#if FLUSHCOMM_USE_SECTION
            if (g_section_handle) {
                DRV_UnmapViewOfFile(g_shared_buf);
                DRV_CloseHandle(g_section_handle);
                g_section_handle = nullptr;
            } else
#endif
                custom_nt::free_mem(g_shared_buf);
            g_shared_buf = nullptr;
        }
    }

    /* Spoofer: usermode hooks (RtlGetVersion, NtQuerySystemInformation) - DISABLED: inline hooks
     * can crash if RtlGetVersion/NtQuerySystemInformation have non-standard prologues (rel call etc) */
    bool spoofer_control(bool enable, int target_pid = -1) {
        (void)target_pid;
        (void)enable;
        return true;  /* was: spoofer::install() - causes instant crash on some systems */
    }

    void spoofer_enable(bool on = true) { spoofer_control(on, -1); }
}

bool is_valid(const uint64_t address)
{
    if (address == 0 || address == 0xCCCCCCCCCCCCCCCC || address == 0xFFFFFFFFFFFFFFFF)
        return false;
    if (address <= 0x400000 || address > 0x7FFFFFFFFFFFFFFF)
        return false;
    return true;
}

template <typename T>
T read(uint64_t address) {
    T buffer{};
    if (is_valid(address))
        DotMem::read_physical((PVOID)address, &buffer, sizeof(T));
    return buffer;
}

template <typename T>
void write(uint64_t address, T buffer) {
    if (is_valid(address))
        DotMem::write_physical((PVOID)address, &buffer, sizeof(T));
}
