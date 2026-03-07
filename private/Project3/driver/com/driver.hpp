#pragma once
/* Legacy alternate driver client. For FlushComm/EAC use utilities/impl/driver.hpp (MAGIC-derived security, no literal device names). */
#include <Windows.h>
#include <TlHelp32.h>
#include "../../utilities/process_enum.hpp"
#include <cstdint>
#include <winioctl.h>
#ifdef __has_include
#  if __has_include("../../flush_comm_config.h")
#    include "../../flush_comm_config.h"
#  endif
#endif

uintptr_t virtualaddy;
uintptr_t cr3;

/* IOCTL/security: when flush_comm_config.h present use MAGIC-derived (no single public literal). Else legacy values for alternate kernel. */
#ifdef FLUSHCOMM_CODE_SECURITY
#define CODE_SECURITY            FLUSHCOMM_CODE_SECURITY
#else
#define CODE_SECURITY            0x457c1d6u  /* legacy alternate kernel; for FlushComm use utilities/impl/driver.hpp */
#endif
#define CODE_RW                  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x47536, METHOD_BUFFERED, FILE_SPECIAL_ACCESS)
#define CODE_BA                  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x36236, METHOD_BUFFERED, FILE_SPECIAL_ACCESS)
#define CODE_GET_GUARDED_REGION  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x13437, METHOD_BUFFERED, FILE_SPECIAL_ACCESS)
#define CODE_GET_DIR_BASE        CTL_CODE(FILE_DEVICE_UNKNOWN, 0x13438, METHOD_BUFFERED, FILE_SPECIAL_ACCESS)

typedef struct _RW {
    INT32 security;
    INT32 process_id;
    ULONGLONG address;
    ULONGLONG buffer;
    ULONGLONG size;
    BOOLEAN write;
} RW, *PRW;

typedef struct _BA {
    INT32 security;
    INT32 process_id;
    ULONGLONG* address;
} BA, *PBA;

typedef struct _GA {
    INT32 security;
    ULONGLONG* address;
} GA, *PGA;

typedef struct _MEMORY_OPERATION_DATA {
    uint32_t pid;
    ULONGLONG* cr3;          // pointeur, comme dans le kernel
} MEMORY_OPERATION_DATA, *PMEMORY_OPERATION_DATA;

namespace mem {
    HANDLE driver_handle = INVALID_HANDLE_VALUE;
    INT32 process_id = 0;

    /* Decode device path at runtime so no literal GUID/device name in .rdata. */
    static void get_device_path(wchar_t* out, bool global) {
        const unsigned char k = 0xB3u;  /* Per-file key; not shared with main FlushComm. */
        /* Encoded \\.\{d6579ab0-c95b-4463-9135-41b8cf16e4e8} then \\.\Global\{...} */
        static const unsigned char enc_local[] = {
            (unsigned char)(L'\\'^k),(unsigned char)(L'\\'^(k+1)),(unsigned char)(L'.'^(k+2)),
            (unsigned char)(L'\\'^(k+3)),(unsigned char)(L'\\'^(k+4)),(unsigned char)(L'{'^(k+5)),
            (unsigned char)(L'd'^(k+6)),(unsigned char)(L'6'^(k+7)),(unsigned char)(L'5'^(k+8)),
            (unsigned char)(L'7'^(k+9)),(unsigned char)(L'9'^(k+10)),(unsigned char)(L'a'^(k+11)),
            (unsigned char)(L'b'^(k+12)),(unsigned char)(L'0'^(k+13)),(unsigned char)(L'-'^(k+14)),
            (unsigned char)(L'c'^(k+15)),(unsigned char)(L'9'^(k+16)),(unsigned char)(L'5'^(k+17)),
            (unsigned char)(L'b'^(k+18)),(unsigned char)(L'-'^(k+19)),(unsigned char)(L'4'^(k+20)),
            (unsigned char)(L'4'^(k+21)),(unsigned char)(L'6'^(k+22)),(unsigned char)(L'3'^(k+23)),
            (unsigned char)(L'-'^(k+24)),(unsigned char)(L'9'^(k+25)),(unsigned char)(L'1'^(k+26)),
            (unsigned char)(L'3'^(k+27)),(unsigned char)(L'5'^(k+28)),(unsigned char)(L'-'^(k+29)),
            (unsigned char)(L'4'^(k+30)),(unsigned char)(L'1'^(k+31)),(unsigned char)(L'b'^(k+32)),
            (unsigned char)(L'8'^(k+33)),(unsigned char)(L'c'^(k+34)),(unsigned char)(L'f'^(k+35)),
            (unsigned char)(L'1'^(k+36)),(unsigned char)(L'6'^(k+37)),(unsigned char)(L'e'^(k+38)),
            (unsigned char)(L'4'^(k+39)),(unsigned char)(L'e'^(k+40)),(unsigned char)(L'8'^(k+41)),
            (unsigned char)(L'}'^(k+42)),0,0
        };
        static const unsigned char enc_global[] = {
            (unsigned char)(L'\\'^k),(unsigned char)(L'\\'^(k+1)),(unsigned char)(L'.'^(k+2)),
            (unsigned char)(L'\\'^(k+3)),(unsigned char)(L'\\'^(k+4)),(unsigned char)(L'G'^(k+5)),
            (unsigned char)(L'l'^(k+6)),(unsigned char)(L'o'^(k+7)),(unsigned char)(L'b'^(k+8)),
            (unsigned char)(L'a'^(k+9)),(unsigned char)(L'l'^(k+10)),(unsigned char)(L'\\'^(k+11)),
            (unsigned char)(L'{'^(k+12)),(unsigned char)(L'd'^(k+13)),(unsigned char)(L'6'^(k+14)),
            (unsigned char)(L'5'^(k+15)),(unsigned char)(L'7'^(k+16)),(unsigned char)(L'9'^(k+17)),
            (unsigned char)(L'a'^(k+18)),(unsigned char)(L'b'^(k+19)),(unsigned char)(L'0'^(k+20)),
            (unsigned char)(L'-'^(k+21)),(unsigned char)(L'c'^(k+22)),(unsigned char)(L'9'^(k+23)),
            (unsigned char)(L'5'^(k+24)),(unsigned char)(L'b'^(k+25)),(unsigned char)(L'-'^(k+26)),
            (unsigned char)(L'4'^(k+27)),(unsigned char)(L'4'^(k+28)),(unsigned char)(L'6'^(k+29)),
            (unsigned char)(L'3'^(k+30)),(unsigned char)(L'-'^(k+31)),(unsigned char)(L'9'^(k+32)),
            (unsigned char)(L'1'^(k+33)),(unsigned char)(L'3'^(k+34)),(unsigned char)(L'5'^(k+35)),
            (unsigned char)(L'-'^(k+36)),(unsigned char)(L'4'^(k+37)),(unsigned char)(L'1'^(k+38)),
            (unsigned char)(L'b'^(k+39)),(unsigned char)(L'8'^(k+40)),(unsigned char)(L'c'^(k+41)),
            (unsigned char)(L'f'^(k+42)),(unsigned char)(L'1'^(k+43)),(unsigned char)(L'6'^(k+44)),
            (unsigned char)(L'e'^(k+45)),(unsigned char)(L'4'^(k+46)),(unsigned char)(L'e'^(k+47)),
            (unsigned char)(L'8'^(k+48)),(unsigned char)(L'}'^(k+49)),0,0
        };
        const unsigned char* enc = global ? enc_global : enc_local;
        size_t len = global ? 50 : 43;  /* \\.\{guid} = 43, \\.\Global\{guid} = 50 */
        for (size_t i = 0; i < len; i++) out[i] = (wchar_t)(enc[i] ^ (k + (unsigned char)i));
        out[len] = L'\0';
    }

    bool find_driver() {
        wchar_t path[64];
        get_device_path(path, false);
        driver_handle = CreateFileW(path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (driver_handle == INVALID_HANDLE_VALUE) {
            get_device_path(path, true);
            driver_handle = CreateFileW(path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        }
        return (driver_handle != INVALID_HANDLE_VALUE);
    }

    void read_physical(PVOID address, PVOID buffer, DWORD size) {
        RW args = { 0 };
        args.security = CODE_SECURITY;
        args.process_id = process_id;
        args.address = (ULONGLONG)address;
        args.buffer = (ULONGLONG)buffer;
        args.size = size;
        args.write = FALSE;

        DWORD returned;
        DeviceIoControl(driver_handle, CODE_RW, &args, sizeof(args), nullptr, 0, &returned, nullptr);
    }

    void write_physical(PVOID address, PVOID buffer, DWORD size) {
        RW args = { 0 };
        args.security = CODE_SECURITY;
        args.process_id = process_id;
        args.address = (ULONGLONG)address;
        args.buffer = (ULONGLONG)buffer;
        args.size = size;
        args.write = TRUE;

        DWORD returned;
        DeviceIoControl(driver_handle, CODE_RW, &args, sizeof(args), nullptr, 0, &returned, nullptr);
    }

    uintptr_t fetch_cr3() {
        uintptr_t cr3_value = 0;
        MEMORY_OPERATION_DATA args = { 0 };
        args.pid = process_id;
        args.cr3 = (ULONGLONG*)&cr3_value;

        DWORD returned;
        DeviceIoControl(driver_handle, CODE_GET_DIR_BASE, &args, sizeof(args), nullptr, 0, &returned, nullptr);
        return cr3_value;
    }

    uintptr_t find_image() {
        uintptr_t image_address = 0;
        BA args = { 0 };
        args.security = CODE_SECURITY;
        args.process_id = process_id;
        args.address = (ULONGLONG*)&image_address;

        DWORD returned;
        DeviceIoControl(driver_handle, CODE_BA, &args, sizeof(args), nullptr, 0, &returned, nullptr);
        return image_address;
    }

    uintptr_t get_guarded_region() {
        uintptr_t guarded_address = 0;
        GA args = { 0 };
        args.security = CODE_SECURITY;
        args.address = (ULONGLONG*)&guarded_address;

        DWORD returned;
        DeviceIoControl(driver_handle, CODE_GET_GUARDED_REGION, &args, sizeof(args), nullptr, 0, &returned, nullptr);
        return guarded_address;
    }

    INT32 find_process(LPCTSTR process_name) {
        DWORD pid = process_enum::find_process_stealth((const wchar_t*)process_name);
        if (pid) { process_id = pid; return (INT32)pid; }
        return 0;
    }

    void close_driver() {
        if (driver_handle != INVALID_HANDLE_VALUE) {
            CloseHandle(driver_handle);
            driver_handle = INVALID_HANDLE_VALUE;
        }
    }
}

// Templates de read/write simples
template <typename T>
T read(uint64_t address) {
    T buffer = { };
    mem::read_physical((PVOID)address, &buffer, sizeof(T));
    return buffer;
}

template <typename T>
bool write(uint64_t address, T value) {
    mem::write_physical((PVOID)address, &value, sizeof(T));
    return true;
}