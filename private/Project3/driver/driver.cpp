#include <ntifs.h>
#include <windef.h>
#include <ntimage.h>
#include <cstdint>
#include <intrin.h>
#include "defines.h"
/* spoof.h removed: Barracudach CallSpoofer is publicly spread; SPOOF_CALL not used. Use custom impl if needed. */
#include <stdio.h>
#include <stdarg.h>
#include "includes.hpp"
#include "memory.hpp"
#include "utility.hpp"
#include "flush_comm.hpp"
#include "process_list_kernel.hpp"
#include "../flush_comm_obfuscate.h"
#include "mouse_inject.hpp"
#include "trace_cleaner.hpp"
#include "routine_obfuscate.h"
#include "page_evasion.hpp"
#include "codecave.hpp"
#include "signature_dilution.hpp"
#if FLUSHCOMM_USE_NMI_SPOOF
#include "nmi_spoof.hpp"
#endif
#if FLUSHCOMM_USE_ICALL_GADGET
#include "icall_gadget.hpp"
#define FRW_INVOKE(x, d) icall_invoke_2((icall_func2_t)frw, (PVOID)(x), (PVOID)(d))
#define FBA_INVOKE(x)   icall_invoke_2((icall_func2_t)fba, (PVOID)(x), nullptr)
#else
#define FRW_INVOKE(x, d) frw((x), (d))
#define FBA_INVOKE(x)   fba((x))
#endif
#include <ntstrsafe.h>

#pragma once

/* Set to 1 to enable DbgPrint diagnostics for base/cr3 failures (view with DbgView) */
#define FLUSHCOMM_DEBUG 0

#if FLUSHCOMM_DEBUG
#define message(...) DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "[KM] " __VA_ARGS__)
#else
#define message(...) ((void)0)
#endif
#if FLUSHCOMM_DEBUG
#define dbg_req(...) DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "[FlushComm] " __VA_ARGS__)
#else
#define dbg_req(...) ((void)0)
#endif

/* Single definitions - avoid LNK2005 multiply defined symbols */
PSERVICE_CALLBACK_ROUTINE g_MouseServiceCallback = nullptr;
PDEVICE_OBJECT g_MouseDeviceObject = nullptr;

namespace globals {
    ULONG u_ver_build = 0;
    ULONG u_ver_major = 0;
    namespace offsets {
        int i_image_file_name = 0x5a8;
        int i_active_threads = 0x5f0;
        int i_active_process_links = 0x448;
        int i_peb = 0x550;
        int i_user_dirbase = 0x388;
        int i_unique_process_id = 0x440;  /* EPROCESS.UniqueProcessId - for list walk PID by name */
    }
}

typedef struct _MOUSE_REQUEST {
    ULONG key;
    LONG x;
    LONG y;
    USHORT button_flags;
} MOUSE_REQUEST, * PMOUSE_REQUEST;

namespace mousemove
{
    PDEVICE_OBJECT g_device_object = nullptr;
    UNICODE_STRING g_device_name, g_symbolic_link;

    bool get_offsets()
    {
        message("Checking offsets for Windows build: %d\n", globals::u_ver_build);

        // Windows 11 24H2 (build 26100) - EPROCESS layout changed significantly
        if (globals::u_ver_build >= 26100)
        {
            message("WARNING: Windows 11 24H2 (build %d) - offsets changed, not yet supported. Use 23H2.\n", globals::u_ver_build);
            /* 24H2: Token 0x248, ActiveProcessLinks 0x1d8, PEB 0x2e0, etc. */
            globals::offsets::i_image_file_name = 0x5a8;  /* placeholder - verify */
            globals::offsets::i_active_process_links = 0x1d8;
            globals::offsets::i_active_threads = 0x5f0;   /* placeholder - verify */
            globals::offsets::i_peb = 0x2e0;
            globals::offsets::i_user_dirbase = 0x388;      /* verify for 24H2 */
            globals::offsets::i_unique_process_id = 0x440;
            return true;
        }
        // Windows 11 23H2 (build 22631)
        else if (globals::u_ver_build >= 22631)
        {
            globals::offsets::i_image_file_name = 0x5a8;
            globals::offsets::i_active_process_links = 0x448;
            globals::offsets::i_active_threads = 0x5f0;
            globals::offsets::i_peb = 0x550;
            globals::offsets::i_user_dirbase = 0x388;  /* KPROCESS.UserDirectoryTableBase */
            globals::offsets::i_unique_process_id = 0x440;
            message("Using offsets for Windows 11 23H2 (build %d)\n", globals::u_ver_build);
            return true;
        }
        // Windows 11 22H2 (build 22621)
        else if (globals::u_ver_build >= 22621)
        {
            globals::offsets::i_image_file_name = 0x5a8;
            globals::offsets::i_active_process_links = 0x448;
            globals::offsets::i_active_threads = 0x5f0;
            globals::offsets::i_peb = 0x550;
            globals::offsets::i_user_dirbase = 0x388;
            globals::offsets::i_unique_process_id = 0x440;
            message("Using offsets for Windows 11 22H2\n");
            return true;
        }
        // Windows 11 21H2 (build 22000)
        else if (globals::u_ver_build >= 22000)
        {
            globals::offsets::i_image_file_name = 0x5a8;
            globals::offsets::i_active_process_links = 0x448;
            globals::offsets::i_active_threads = 0x5f0;
            globals::offsets::i_peb = 0x550;
            globals::offsets::i_user_dirbase = 0x388;
            globals::offsets::i_unique_process_id = 0x440;
            message("Using offsets for Windows 11 21H2\n");
            return true;
        }
        // Windows 10 22H2 (build 19045)
        else if (globals::u_ver_build == 19045)
        {
            globals::offsets::i_image_file_name = 0x5a8;
            globals::offsets::i_active_process_links = 0x448;
            globals::offsets::i_active_threads = 0x5f0;
            globals::offsets::i_peb = 0x550;
            globals::offsets::i_user_dirbase = 0x28;  /* Win10 uses DirectoryTableBase */
            globals::offsets::i_unique_process_id = 0x440;
            message("Using offsets for Windows 10 22H2\n");
            return true;
        }
        // Windows 10 21H2/21H1 (build 19044/19043)
        else if (globals::u_ver_build >= 19043 && globals::u_ver_build <= 19044)
        {
            globals::offsets::i_image_file_name = 0x5a8;
            globals::offsets::i_active_process_links = 0x448;
            globals::offsets::i_active_threads = 0x5f0;
            globals::offsets::i_peb = 0x550;
            globals::offsets::i_user_dirbase = 0x28;
            globals::offsets::i_unique_process_id = 0x440;
            message("Using offsets for Windows 10 21H1/21H2\n");
            return true;
        }
        // Windows 10 20H2 (build 19042)
        else if (globals::u_ver_build == 19042)
        {
            globals::offsets::i_image_file_name = 0x5a8;
            globals::offsets::i_active_process_links = 0x448;
            globals::offsets::i_active_threads = 0x5f0;
            globals::offsets::i_peb = 0x550;
            globals::offsets::i_user_dirbase = 0x28;
            globals::offsets::i_unique_process_id = 0x440;
            message("Using offsets for Windows 10 20H2\n");
            return true;
        }
        // Windows 10 2004 (build 19041)
        else if (globals::u_ver_build == 19041)
        {
            globals::offsets::i_image_file_name = 0x5a8;
            globals::offsets::i_active_process_links = 0x448;
            globals::offsets::i_active_threads = 0x5f0;
            globals::offsets::i_peb = 0x550;
            globals::offsets::i_user_dirbase = 0x28;
            globals::offsets::i_unique_process_id = 0x440;
            message("Using offsets for Windows 10 2004\n");
            return true;
        }
        // Windows 10 1909 (build 18363)
        else if (globals::u_ver_build == 18363)
        {
            globals::offsets::i_image_file_name = 0x450;
            globals::offsets::i_active_process_links = 0x2f0;
            globals::offsets::i_active_threads = 0x498;
            globals::offsets::i_peb = 0x550;
            globals::offsets::i_user_dirbase = 0x28;
            globals::offsets::i_unique_process_id = 0x2e8;
            message("Using offsets for Windows 10 1909\n");
            return true;
        }
        // Windows 10 1903 (build 18362)
        else if (globals::u_ver_build == 18362)
        {
            globals::offsets::i_image_file_name = 0x450;
            globals::offsets::i_active_process_links = 0x2f0;
            globals::offsets::i_active_threads = 0x498;
            globals::offsets::i_peb = 0x550;
            globals::offsets::i_user_dirbase = 0x28;
            globals::offsets::i_unique_process_id = 0x2e8;
            message("Using offsets for Windows 10 1903\n");
            return true;
        }
        // Windows 10 1809 (build 17763)
        else if (globals::u_ver_build == 17763)
        {
            globals::offsets::i_image_file_name = 0x450;
            globals::offsets::i_active_process_links = 0x2f0;
            globals::offsets::i_unique_process_id = 0x2e8;
            globals::offsets::i_active_threads = 0x498;
            globals::offsets::i_peb = 0x550;
            globals::offsets::i_user_dirbase = 0x28;
            message("Using offsets for Windows 10 1809\n");
            return true;
        }
        // Windows 10 1803 (build 17134)
        else if (globals::u_ver_build == 17134)
        {
            globals::offsets::i_image_file_name = 0x450;
            globals::offsets::i_active_process_links = 0x2f0;
            globals::offsets::i_active_threads = 0x498;
            globals::offsets::i_peb = 0x550;
            globals::offsets::i_user_dirbase = 0x28;
            globals::offsets::i_unique_process_id = 0x2e8;
            message("Using offsets for Windows 10 1803\n");
            return true;
        }
        // Fallback for other Win10/11 builds (e.g. preview builds)
        else if (globals::u_ver_build >= 17134)
        {
            message("WARNING: Unknown build %d, using Win11 23H2 offsets\n", globals::u_ver_build);
            globals::offsets::i_image_file_name = 0x5a8;
            globals::offsets::i_active_process_links = 0x448;
            globals::offsets::i_active_threads = 0x5f0;
            globals::offsets::i_peb = 0x550;
            globals::offsets::i_user_dirbase = (globals::u_ver_build >= 22000) ? 0x388 : 0x28;
            globals::offsets::i_unique_process_id = 0x440;
            return true;
        }

        message("ERROR: Unsupported Windows build number: %d\n", globals::u_ver_build);
        return false;
    }

    bool b_version_check()
    {
        OSVERSIONINFOW v_info{};
        v_info.dwOSVersionInfoSize = sizeof(OSVERSIONINFOW);

        NTSTATUS status = RtlGetVersion(&v_info);
        if (!NT_SUCCESS(status))
        {
            message("RtlGetVersion failed with status: 0x%X\n", status);
            return false;
        }

        message("Detected Windows version: %d.%d Build %d\n",
            v_info.dwMajorVersion,
            v_info.dwMinorVersion,
            v_info.dwBuildNumber);

        if (v_info.dwMajorVersion < 10)
        {
            message("Unsupported Windows version (needs Windows 10+)\n");
            return false;
        }

        globals::u_ver_major = v_info.dwMajorVersion;
        globals::u_ver_build = v_info.dwBuildNumber;

        if (!get_offsets())
        {
            message("No offsets available for this Windows build\n");
            return false;
        }

        return true;
    }

    NTSTATUS unsupported_dispatch(PDEVICE_OBJECT device_object, PIRP irp)
    {
        UNREFERENCED_PARAMETER(device_object);

        irp->IoStatus.Status = STATUS_NOT_SUPPORTED;
        IoCompleteRequest(irp, IO_NO_INCREMENT);
        return irp->IoStatus.Status;
    }
}

UNICODE_STRING name, link;

typedef struct _SYSTEM_BIGPOOL_ENTRY {
    PVOID VirtualAddress;
    ULONG_PTR NonPaged : 1;
    ULONG_PTR SizeInBytes;
    UCHAR Tag[4];
} SYSTEM_BIGPOOL_ENTRY, * PSYSTEM_BIGPOOL_ENTRY;

typedef struct _SYSTEM_BIGPOOL_INFORMATION {
    ULONG Count;
    SYSTEM_BIGPOOL_ENTRY AllocatedInfo[1];
} SYSTEM_BIGPOOL_INFORMATION, * PSYSTEM_BIGPOOL_INFORMATION;

typedef enum _SYSTEM_INFORMATION_CLASS {
    SystemBigPoolInformationClass = 0x40 | 2,  /* 0x42 - no single literal in binary for scan */
} SYSTEM_INFORMATION_CLASS;

/* IoDriverObjectType from ntoskrnl.lib via includes.hpp (globals::IoDriverObjectType) */

extern "C" PVOID NTAPI PsGetProcessSectionBaseAddress(PEPROCESS Process);
/* ZwQuerySystemInformation resolved at runtime via routine_obfuscate (no IAT) */

/* Must match driver.hpp - rebuild both if changed */
#define IOCTL_FUNC_RW           0x8A12
#define IOCTL_FUNC_BA           0x9B34
#define IOCTL_FUNC_GET_GUARDED  0x1C56
#define IOCTL_FUNC_GET_DIR_BASE 0x2D78
#define IOCTL_FUNC_SPOOFER_CTRL 0x4FA1
#define CODE_SECURITY           FLUSHCOMM_CODE_SECURITY

#define CODE_RW                 CTL_CODE(FILE_DEVICE_UNKNOWN, IOCTL_FUNC_RW, METHOD_BUFFERED, FILE_SPECIAL_ACCESS)
#define CODE_BA                 CTL_CODE(FILE_DEVICE_UNKNOWN, IOCTL_FUNC_BA, METHOD_BUFFERED, FILE_SPECIAL_ACCESS)
#define CODE_GET_GUARDED_REGION CTL_CODE(FILE_DEVICE_UNKNOWN, IOCTL_FUNC_GET_GUARDED, METHOD_BUFFERED, FILE_SPECIAL_ACCESS)
#define CODE_GET_DIR_BASE       CTL_CODE(FILE_DEVICE_UNKNOWN, IOCTL_FUNC_GET_DIR_BASE, METHOD_BUFFERED, FILE_SPECIAL_ACCESS)
#define CODE_SPOOFER_CTRL       CTL_CODE(FILE_DEVICE_UNKNOWN, IOCTL_FUNC_SPOOFER_CTRL, METHOD_BUFFERED, FILE_SPECIAL_ACCESS)
#define IOCTL_MOUSE_MOVE        CTL_CODE(FILE_DEVICE_UNKNOWN, 0x27336, METHOD_BUFFERED, FILE_SPECIAL_ACCESS)

#define WIN_1803 17134
#define WIN_1809 17763
#define WIN_1903 18362
#define WIN_1909 18363
#define WIN_2004 19041
#define WIN_20H2 19569
#define WIN_21H1 20180
#define WIN_22H2 19045

#define PAGE_OFFSET_SIZE 12
/* x64 PTE/PDE/PDPTE/PML4E: bits 12-51 are physical frame (52-bit PA, 4KB aligned). Must match read() MAX_PHYS_PAGE.
 * Derived: no literal 0xFFFFFFFFFFFFF000 in binary - built from PAGE_OFFSET_SIZE and 52-bit PA mask. */
#define PMASK  (((~(ULONGLONG)((1ULL << PAGE_OFFSET_SIZE) - 1)) & 0x000FFFFFFFFFFFFFULL))
/* 9-bit index mask (PML4/PDPT/PD/PT levels): (1<<9)-1 - avoid literal 0x1ff */
#define IDX_9  ((1ULL << 9) - 1)
/* Large-page PDE bit, CR3 encrypted marker (VBS/EAC), MmPfn decryption - avoid public literals */
#define PDE_LARGE_BIT   (1ULL << 7)
#define CR3_ENC_MARKER  (1ULL << 6)
#define MMPFN_DEC_HI    (0xFULL << 60)
#define MMPFN_DEC_SH    13
#define MMPFN_DEC_EXT   (0xFFFFULL << 48)

typedef struct _RW {
    INT32 security;
    INT32 process_id;
    ULONGLONG address;
    ULONGLONG buffer;
    ULONGLONG size;
    BOOLEAN write;
} RW, * PRW;

typedef struct _BA {
    INT32 security;
    INT32 process_id;
    ULONGLONG* address;
} BA, * PBA;

typedef struct _GA {
    INT32 security;
    ULONGLONG* address;
} GA, * PGA;

typedef struct _MEMORY_OPERATION_DATA {
    uint32_t pid;
    uintptr_t cr3;
} MEMORY_OPERATION_DATA, * PMEMORY_OPERATION_DATA;

/* MmMapIoSpaceEx + MmUnmapIoSpace only - no MmCopyMemory (EAC traces it) */
PVOID(*DynamicMmMapIoSpaceEx)(PHYSICAL_ADDRESS, SIZE_T, ULONG) = NULL;
VOID(*DynamicMmUnmapIoSpace)(PVOID, SIZE_T) = NULL;

NTSTATUS load_dynamic_functions() {
    message("Loading dynamic functions...\n");

    DynamicMmMapIoSpaceEx = (PVOID(*)(PHYSICAL_ADDRESS, SIZE_T, ULONG))get_system_routine_obf(OBF_MmMapIoSpaceEx, sizeof(OBF_MmMapIoSpaceEx));
    if (!DynamicMmMapIoSpaceEx) {
        message("ERROR: Failed to get MmMapIoSpaceEx\n");
        return STATUS_UNSUCCESSFUL;
    }

    DynamicMmUnmapIoSpace = (VOID(*)(PVOID, SIZE_T))get_system_routine_obf(OBF_MmUnmapIoSpace, sizeof(OBF_MmUnmapIoSpace));
    if (!DynamicMmUnmapIoSpace) {
        message("ERROR: Failed to get MmUnmapIoSpace\n");
        return STATUS_UNSUCCESSFUL;
    }

    message("Dynamic functions loaded successfully\n");
    return STATUS_SUCCESS;
}

/* Max valid physical address (52-bit PA on x64) - mapping beyond can BSOD */
#define MAX_PHYS_PAGE 0x000FFFFFFFFFF000ULL

/* Read from physical address via MmMapIoSpace (no MmCopyMemory - EAC traces it) */
NTSTATUS read(PVOID target_address, PVOID buffer, SIZE_T size, SIZE_T* bytes_read) {
    if (!target_address || !buffer || size == 0)
        return STATUS_INVALID_PARAMETER;

    /* Page-align: map containing page, offset within it */
    ULONG64 phys = (ULONG64)target_address;
    ULONG64 page_base = phys & ~0xFFFULL;
    ULONG64 offset = phys & 0xFFF;
    /* Reject null or invalid physical range - MmMapIoSpaceEx can BSOD on bad PA */
    if (page_base == 0 || page_base > MAX_PHYS_PAGE)
        return STATUS_INVALID_PARAMETER;
    /* Request must fit in remainder of page */
    if (offset + size > PAGE_SIZE)
        return STATUS_INVALID_PARAMETER;

    PHYSICAL_ADDRESS pa = { .QuadPart = (LONGLONG)page_base };
    PVOID mapped = DynamicMmMapIoSpaceEx(pa, PAGE_SIZE, PAGE_READONLY);
    if (!mapped)
        return STATUS_UNSUCCESSFUL;

    __try {
        RtlCopyMemory(buffer, (PUCHAR)mapped + offset, size);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        DynamicMmUnmapIoSpace(mapped, PAGE_SIZE);
        return STATUS_ACCESS_VIOLATION;
    }
    DynamicMmUnmapIoSpace(mapped, PAGE_SIZE);

    if (bytes_read)
        *bytes_read = size;
    return STATUS_SUCCESS;
}

NTSTATUS write(PVOID target_address, PVOID buffer, SIZE_T size, SIZE_T* bytes_written) {
    if (!target_address || !buffer || !bytes_written)
        return STATUS_INVALID_PARAMETER;

    ULONG64 phys = (ULONG64)target_address;
    ULONG64 page_base = phys & ~0xFFFULL;
    if (page_base == 0 || page_base > MAX_PHYS_PAGE)
        return STATUS_INVALID_PARAMETER;

    PHYSICAL_ADDRESS AddrToWrite = { 0 };
    AddrToWrite.QuadPart = (LONGLONG)(target_address);

    PVOID pmapped_mem = DynamicMmMapIoSpaceEx(AddrToWrite, size, PAGE_READWRITE);

    if (!pmapped_mem)
        return STATUS_UNSUCCESSFUL;
    __try {
        unsigned char* dest = (unsigned char*)pmapped_mem;
        unsigned char* src = (unsigned char*)buffer;
        for (SIZE_T i = 0; i < size; i++) {
            dest[i] = src[i];
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        DynamicMmUnmapIoSpace(pmapped_mem, size);
        return STATUS_ACCESS_VIOLATION;
    }
    *bytes_written = size;
    DynamicMmUnmapIoSpace(pmapped_mem, size);
    return STATUS_SUCCESS;
}

/* Returns KPROCESS.UserDirectoryTableBase offset; Win11 21H2+ uses 0x388, older use 0x28 */
INT32 get_winver() {
    RTL_OSVERSIONINFOW ver = { 0 };
    ver.dwOSVersionInfoSize = sizeof(ver);
    if (!NT_SUCCESS(RtlGetVersion(&ver)))
        return 0x28;
    if (ver.dwBuildNumber >= 22000) return 0x388;  /* Win11: UserDirectoryTableBase */
    return 0x28;  /* Win10: DirectoryTableBase */
}

volatile uint64_t g_MmPfnDatabase = 0;
volatile uint64_t g_PXE_BASE = 0;
volatile uint64_t g_idx = 0;
uintptr_t dirBase = 0;

void initDefinesCR3() {
    KDDEBUGGER_DATA64 kdBlock = { 0 };
    CONTEXT context = { 0 };
    context.ContextFlags = CONTEXT_FULL;
    (RtlCaptureContext)(&context);

    PDUMP_HEADER dumpHeader = (PDUMP_HEADER)ExAllocatePoolWithTag(NonPagedPool, DUMP_BLOCK_SIZE, EVASION_POOL_TAG_COPY_R);
    if (dumpHeader) {
        (KeCapturePersistentThreadState)(&context, NULL, 0, 0, 0, 0, 0, dumpHeader);
        RtlCopyMemory(&kdBlock, (PUCHAR)dumpHeader + KDDEBUGGER_DATA_OFFSET, sizeof(kdBlock));

        ExFreePoolWithTag(dumpHeader, EVASION_POOL_TAG_COPY_R);

        g_MmPfnDatabase = *(ULONG64*)(kdBlock.MmPfnDatabase);

        ULONG64 g_PTE_BASE = kdBlock.PteBase;
        ULONG64 g_PDE_BASE = g_PTE_BASE + ((g_PTE_BASE & 0xffffffffffff) >> 9);
        ULONG64 g_PPE_BASE = g_PTE_BASE + ((g_PDE_BASE & 0xffffffffffff) >> 9);
        g_PXE_BASE = g_PTE_BASE + ((g_PPE_BASE & 0xffffffffffff) >> 9);
        g_idx = (g_PTE_BASE >> 39) - 0x1FFFE00;
    }
}

uintptr_t get_kernel_base() {
    const auto idtbase = *reinterpret_cast<uint64_t*>(__readgsqword(0x18) + 0x38);
    const auto descriptor_0 = *reinterpret_cast<uint64_t*>(idtbase);
    const auto descriptor_1 = *reinterpret_cast<uint64_t*>(idtbase + 8);
    const auto isr_base = ((descriptor_0 >> 32) & 0xFFFF0000) + (descriptor_0 & 0xFFFF) + (descriptor_1 << 32);
    auto align_base = isr_base & PMASK;

    for (; ; align_base -= 0x1000) {
        for (auto* search_base = reinterpret_cast<uint8_t*>(align_base); search_base < reinterpret_cast<uint8_t*>(align_base) + 0xFF9; search_base++) {
            if (search_base[0] == 0x48 &&
                search_base[1] == 0x8D &&
                search_base[2] == 0x1D &&
                search_base[6] == 0xFF) {
                const auto relative_offset = *reinterpret_cast<int*>(&search_base[3]);
                const auto address = reinterpret_cast<uint64_t>(search_base + relative_offset + 7);
                if ((address & 0xFFF) == 0) {
                    if (*reinterpret_cast<uint16_t*>(address) == FLUSHCOMM_PE_MAGIC_MZ) {
                        return address;
                    }
                }
            }
        }
    }
}

intptr_t search_pattern(void* module_handle, const char* section, const char* signature_value) {
    static auto in_range = [](auto x, auto a, auto b) { return (x >= a && x <= b); };
    static auto get_bits = [](auto  x) { return (in_range((x & (~0x20)), 'A', 'F') ? ((x & (~0x20)) - 'A' + 0xa) : (in_range(x, '0', '9') ? x - '0' : 0)); };
    static auto get_byte = [](auto  x) { return (get_bits(x[0]) << 4 | get_bits(x[1])); };

    const auto dos_headers = reinterpret_cast<PIMAGE_DOS_HEADER>(module_handle);
    const auto nt_headers = reinterpret_cast<PIMAGE_NT_HEADERS>(reinterpret_cast<uintptr_t>(module_handle) + dos_headers->e_lfanew);
    const auto section_headers = reinterpret_cast<PIMAGE_SECTION_HEADER>(nt_headers + 1);

    auto range_start = 0ui64;
    auto range_end = 0ui64;
    for (auto cur_section = section_headers; cur_section < section_headers + nt_headers->FileHeader.NumberOfSections; cur_section++) {
        if (strcmp(reinterpret_cast<const char*>(cur_section->Name), section) == 0) {
            range_start = reinterpret_cast<uintptr_t>(module_handle) + cur_section->VirtualAddress;
            range_end = range_start + cur_section->Misc.VirtualSize;
        }
    }

    if (range_start == 0)
        return 0u;

    auto first_match = 0ui64;
    auto pat = signature_value;
    for (uintptr_t cur = range_start; cur < range_end; cur++) {
        if (*pat == '\0') {
            return first_match;
        }
        if (*(uint8_t*)pat == '\?' || *reinterpret_cast<uint8_t*>(cur) == get_byte(pat)) {
            if (!first_match)
                first_match = cur;

            if (!pat[2])
                return first_match;

            if (*(uint16_t*)pat == 16191 || *(uint8_t*)pat != '\?') {
                pat += 3;
            }
            else {
                pat += 2;
            }
        }
        else {
            pat = signature_value;
            first_match = 0;
        }
    }
    return 0u;
}

#pragma warning(push)
#pragma warning(disable:4201)

typedef union {
    struct {
        uint64_t reserved1 : 3;
        uint64_t page_level_write_through : 1;
        uint64_t page_level_cache_disable : 1;
        uint64_t reserved2 : 7;
        uint64_t address_of_page_directory : 36;
        uint64_t reserved3 : 16;
    };
    uint64_t flags;
} cr3;
static_assert(sizeof(cr3) == 0x8);

typedef union {
    struct {
        uint64_t present : 1;
        uint64_t write : 1;
        uint64_t supervisor : 1;
        uint64_t page_level_write_through : 1;
        uint64_t page_level_cache_disable : 1;
        uint64_t accessed : 1;
        uint64_t dirty : 1;
        uint64_t large_page : 1;
        uint64_t global : 1;
        uint64_t ignored_1 : 2;
        uint64_t restart : 1;
        uint64_t page_frame_number : 36;
        uint64_t reserved1 : 4;
        uint64_t ignored_2 : 7;
        uint64_t protection_key : 4;
        uint64_t execute_disable : 1;
    };

    uint64_t flags;
} pt_entry_64;
static_assert(sizeof(pt_entry_64) == 0x8);
#pragma warning(pop)

static uint64_t pte_base = 0;
static uint64_t pde_base = 0;
static uint64_t ppe_base = 0;
static uint64_t pxe_base = 0;
static uint64_t self_mapidx = 0;
static uint64_t mm_pfn_database = 0;

uint64_t get_dirbase() {
    return __readcr3() & PMASK;
}

void* phys_to_virt(uint64_t phys) {
    PHYSICAL_ADDRESS phys_addr = { .QuadPart = (int64_t)(phys) };
    return reinterpret_cast<void*>(MmGetVirtualForPhysical(phys_addr));
}

void init_pte_base() {
    cr3 system_cr3 = { .flags = get_dirbase() };
    uint64_t dirbase_phys = system_cr3.address_of_page_directory << 12;
    pt_entry_64* pt_entry = reinterpret_cast<pt_entry_64*>(phys_to_virt(dirbase_phys));
    if (!pt_entry) return;
    /* Validate mapped range so pt_entry[idx] access doesn't fault (512 entries = one 4KB page). */
    if (!MmIsAddressValid(pt_entry) || !MmIsAddressValid(&pt_entry[IDX_9]))
        return;
    for (uint64_t idx = 0; idx < (1ULL << 9); idx++) {
        if (pt_entry[idx].page_frame_number == system_cr3.address_of_page_directory) {
            pte_base = (idx + 0x1FFFE00ui64) << 39ui64;
            pde_base = (idx << 30ui64) + pte_base;
            ppe_base = (idx << 30ui64) + pte_base + (idx << 21ui64);
            pxe_base = (idx << 12ui64) + ppe_base;
            self_mapidx = idx;
            break;
        }
    }
}

#define CR3_CACHE_SIZE 16
#define CR3_CACHE_INVALID_PID 0

typedef struct _CR3_CACHE_ENTRY {
    ULONG pid;
    ULONGLONG cr3;
} CR3_CACHE_ENTRY;

static CR3_CACHE_ENTRY g_cr3_cache[CR3_CACHE_SIZE];
static KSPIN_LOCK g_cr3_cache_lock;
static ULONG g_cr3_cache_next = 0;
static BOOLEAN g_cr3_cache_inited = FALSE;

static void cr3_cache_init(void) {
    if (g_cr3_cache_inited) return;
    KeInitializeSpinLock(&g_cr3_cache_lock);
    for (int i = 0; i < CR3_CACHE_SIZE; i++)
        g_cr3_cache[i].pid = CR3_CACHE_INVALID_PID;
    g_cr3_cache_inited = TRUE;
}

/* Returns cached CR3 or 0 if not found */
static ULONGLONG cr3_cache_lookup(ULONG pid) {
    KIRQL irql;
    ULONGLONG cr3 = 0;
    KeAcquireSpinLock(&g_cr3_cache_lock, &irql);
    for (int i = 0; i < CR3_CACHE_SIZE; i++) {
        if (g_cr3_cache[i].pid == pid) {
            cr3 = g_cr3_cache[i].cr3;
            break;
        }
    }
    KeReleaseSpinLock(&g_cr3_cache_lock, irql);
    return cr3;
}

/* Store CR3 for pid (round-robin eviction) */
static void cr3_cache_store(ULONG pid, ULONGLONG cr3) {
    KIRQL irql;
    KeAcquireSpinLock(&g_cr3_cache_lock, &irql);
    ULONG idx = g_cr3_cache_next % CR3_CACHE_SIZE;
    g_cr3_cache[idx].pid = pid;
    g_cr3_cache[idx].cr3 = cr3;
    g_cr3_cache_next++;
    KeReleaseSpinLock(&g_cr3_cache_lock, irql);
}

uintptr_t init_mmpfn_database() {
    if (mm_pfn_database) return mm_pfn_database;
    /* Pattern 1: MmGetVirtualForPhysical - Win10/11 common */
    auto raw = search_pattern(reinterpret_cast<void*>(get_kernel_base()), ".text", "B9 ? ? ? ? 48 8B 05 ? ? ? ? 48 89 43 18");
    if (raw) {
        auto search = raw + 5;
        if (MmIsAddressValid(reinterpret_cast<void*>(search + 10))) {
            auto resolved_base = search + *reinterpret_cast<int32_t*>(search + 3) + 7;
            if (MmIsAddressValid(reinterpret_cast<void*>(resolved_base + 7))) {
                mm_pfn_database = *reinterpret_cast<uintptr_t*>(resolved_base);
                if (mm_pfn_database) return mm_pfn_database;
            }
        }
    }
    /* Pattern 2: Alternate MmPfnDatabase ref - "mov rax, [MmPfnDatabase]" style */
    raw = search_pattern(reinterpret_cast<void*>(get_kernel_base()), ".text", "48 8B 05 ? ? ? ? 48 63 C9 48 8D");
    if (raw) {
        auto search = raw + 3;
        if (MmIsAddressValid(reinterpret_cast<void*>(search + 7))) {
            auto resolved_base = search + 4 + *reinterpret_cast<int32_t*>(search) + 4;
            if (MmIsAddressValid(reinterpret_cast<void*>(resolved_base + 7))) {
                uintptr_t val = *reinterpret_cast<uintptr_t*>(resolved_base);
                if (val && (val > 0xFFFF800000000000ULL)) {  /* kernel address */
                    mm_pfn_database = val;
                    return mm_pfn_database;
                }
            }
        }
    }
    /* Pattern 3: 4C 8B 15 = mov r10, [rel] */
    raw = search_pattern(reinterpret_cast<void*>(get_kernel_base()), ".text", "4C 8B 15 ? ? ? ? 4D 85 D2 0F 84");
    if (raw) {
        auto search = raw + 3;
        if (MmIsAddressValid(reinterpret_cast<void*>(search + 7))) {
            auto resolved_base = search + 4 + *reinterpret_cast<int32_t*>(search) + 4;
            if (MmIsAddressValid(reinterpret_cast<void*>(resolved_base + 7))) {
                uintptr_t val = *reinterpret_cast<uintptr_t*>(resolved_base);
                if (val && (val > 0xFFFF800000000000ULL)) {
                    mm_pfn_database = val;
                    return mm_pfn_database;
                }
            }
        }
    }
    return 0;
}

/* Derive CR3 (physical PML4) from process base address by walking physical page tables.
 * Bypasses encrypted EPROCESS DTB - works with EAC/VBS when PsGetProcessSectionBaseAddress succeeds. */
static ULONGLONG dirbase_from_base_address(void* virt_base_ptr) {
    if (!virt_base_ptr) return 0;
    ULONGLONG virt_base = (ULONGLONG)(uintptr_t)virt_base_ptr;
    if (virt_base < 0x10000 || virt_base > 0x7FFFFFFFFFFF) return 0;  /* User-mode VA range */

    UINT64 pml4_idx = (virt_base >> 39) & IDX_9;
    UINT64 pdpt_idx = (virt_base >> 30) & IDX_9;
    UINT64 pd_idx   = (virt_base >> 21) & IDX_9;
    UINT64 pt_idx   = (virt_base >> 12) & IDX_9;

    auto mem_range = MmGetPhysicalMemoryRanges();
    if (!mem_range) return 0;

    SIZE_T readsize = 0;
    ULONGLONG pages_tried = 0;
    /* MAGIC-derived cap: avoid literal 200000 - varies per build so no fixed signature */
    const ULONGLONG MAX_PAGES = 150000 + (FLUSHCOMM_MAGIC & 0x1FFFF);
    const int MC_LIMIT = 150 + (int)((FLUSHCOMM_MAGIC >> 16) & 0x3F);
    for (int mc = 0; mc < MC_LIMIT && pages_tried < MAX_PAGES; mc++) {
        if (mem_range[mc].BaseAddress.QuadPart == 0 && mem_range[mc].NumberOfBytes.QuadPart == 0)
            break;
        ULONGLONG base_phys = mem_range[mc].BaseAddress.QuadPart;
        ULONGLONG size = mem_range[mc].NumberOfBytes.QuadPart;
        for (ULONGLONG off = 0; off < size && pages_tried < MAX_PAGES; off += (1ULL << PAGE_OFFSET_SIZE)) {
            pages_tried++;
            ULONGLONG cand_cr3 = base_phys + off;
            ULONGLONG pml4e = 0;
            if (!NT_SUCCESS(read(PVOID(cand_cr3 + 8 * pml4_idx), &pml4e, sizeof(pml4e), &readsize)) || (pml4e & 1) == 0)
                continue;
            ULONGLONG pdpt_phys = pml4e & PMASK;
            ULONGLONG pdpte = 0;
            if (!NT_SUCCESS(read(PVOID(pdpt_phys + 8 * pdpt_idx), &pdpte, sizeof(pdpte), &readsize)) || (pdpte & 1) == 0)
                continue;
            ULONGLONG pd_phys = pdpte & PMASK;
            ULONGLONG pde = 0;
            if (!NT_SUCCESS(read(PVOID(pd_phys + 8 * pd_idx), &pde, sizeof(pde), &readsize)) || (pde & 1) == 0)
                continue;
            if (pde & PDE_LARGE_BIT) {
                ULONGLONG phys = (pde & PMASK) + (virt_base & ((1ULL << 21) - 1));
                if (phys) {
                    USHORT mz = 0;
                    if (NT_SUCCESS(read(PVOID(phys), &mz, sizeof(mz), &readsize)) && mz == FLUSHCOMM_PE_MAGIC_MZ)
                        return cand_cr3;  /* Valid PE header at base = correct process */
                }
                continue;
            }
            ULONGLONG pt_phys = pde & PMASK;
            ULONGLONG pte = 0;
            if (!NT_SUCCESS(read(PVOID(pt_phys + 8 * pt_idx), &pte, sizeof(pte), &readsize)) || (pte & 1) == 0)
                continue;
            ULONGLONG phys = (pte & PMASK) + (virt_base & ((1ULL << PAGE_OFFSET_SIZE) - 1));
            if (phys) {
                USHORT mz = 0;
                if (NT_SUCCESS(read(PVOID(phys), &mz, sizeof(mz), &readsize)) && mz == FLUSHCOMM_PE_MAGIC_MZ)
                    return cand_cr3;  /* Valid PE header at base = correct process */
            }
        }
    }
    return 0;
}

static BOOLEAN validate_cr3_with_base(ULONGLONG cr3_val, ULONGLONG base_va);  /* impl after translate_linear */

/* Internal: resolve CR3 for target_process, return in *out_cr3. Caller holds ref.
 * base_override: when non-zero, use for validation (usermode base from find_image); else use PsGetProcessSectionBaseAddress.
 * Order: 1=physical scan (least detected), 2=MmPfn, 3=EPROCESS, 4=KeStackAttach (most detected). UC: __readcr3/attach is detected by EAC. */
static NTSTATUS get_cr3_internal(PEPROCESS target_process, PULONGLONG out_cr3, ULONGLONG base_override) {
    KAPC_STATE apc_state = { 0 };
    BOOLEAN attached = FALSE;
    ULONGLONG dtb = 0;

    __try {
    /* Prefer usermode base when provided (EAC spoofs PsGetProcessSectionBaseAddress) */
    ULONGLONG base_addr = base_override ? base_override : (ULONGLONG)PsGetProcessSectionBaseAddress(target_process);

    /* Method 1: physical scan from base - wrap in __try so MmMapIoSpaceEx/read fault doesn't BSOD (e.g. VBS). */
    if (base_addr) {
        __try {
            dtb = dirbase_from_base_address((void*)base_addr);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            dtb = 0;
            dbg_req("get_cr3: Method1 dirbase_from_base exception\n");
        }
        if (dtb) {
            *out_cr3 = dtb;
            dbg_req("get_cr3: Method1 dirbase_from_base OK base=0x%llx cr3=0x%llx\n", base_addr, dtb);
            return STATUS_SUCCESS;
        }
        dbg_req("get_cr3: Method1 dirbase_from_base failed for base=0x%llx\n", base_addr);
    } else {
        dbg_req("get_cr3: Method1 PsGetProcessSectionBaseAddress=0\n");
    }

    /* Method 2: MmPfn bruteforce - NULL check + PFN cap + MmIsAddressValid(cur) to avoid BSOD. */
    if (!pte_base) init_pte_base();
    if (!mm_pfn_database) init_mmpfn_database();
    if (pte_base && mm_pfn_database) {
        auto mem_range = MmGetPhysicalMemoryRanges();
        if (mem_range) {
            auto cr3_ptebase = self_mapidx * 8 + pxe_base;
            /* MAGIC-derived cap: (1<<20) - (MAGIC&0xFFFF) avoids literal 0x100000 */
            const ULONGLONG max_pfn_per_range = (1ULL << 20) - (FLUSHCOMM_MAGIC & 0xFFFF);
            const int MC_LIMIT_M2 = 150 + (int)((FLUSHCOMM_MAGIC >> 18) & 0x3F);
            for (int mc = 0; mc < MC_LIMIT_M2; mc++) {
                if (mem_range[mc].BaseAddress.QuadPart == 0 && mem_range[mc].NumberOfBytes.QuadPart == 0)
                    break;
                auto start_pfn = mem_range[mc].BaseAddress.QuadPart >> 12;
                auto end_pfn = start_pfn + (mem_range[mc].NumberOfBytes.QuadPart >> 12);
                if (end_pfn - start_pfn > max_pfn_per_range)
                    end_pfn = start_pfn + max_pfn_per_range;
                for (auto i = start_pfn; i < end_pfn; i++) {
                    auto cur = reinterpret_cast<_MMPFN*>(mm_pfn_database + sizeof(_MMPFN) * i);
                    if (!MmIsAddressValid(cur))
                        continue;
                    if (!cur->flags || cur->flags == 1 || cur->pte_address != cr3_ptebase)
                        continue;
                    auto decrypted = ((cur->flags | MMPFN_DEC_HI) >> MMPFN_DEC_SH) | MMPFN_DEC_EXT;
                    if (MmIsAddressValid((void*)decrypted) && (PEPROCESS)decrypted == target_process) {
                        ULONGLONG cand = (ULONGLONG)(i << 12);
                        if (base_addr && !validate_cr3_with_base(cand, base_addr))
                            continue;  /* reject wrong CR3 */
                        *out_cr3 = cand;
                        dbg_req("get_cr3: Method2 MmPfn OK cr3=0x%llx\n", *out_cr3);
                        return STATUS_SUCCESS;
                    }
                }
            }
        } else {
            dbg_req("get_cr3: Method2 MmGetPhysicalMemoryRanges returned NULL - skip\n");
        }
    } else {
        dbg_req("get_cr3: Method2 pte_base=%llx mm_pfn=%llx - skip\n", (ULONGLONG)pte_base, (ULONGLONG)mm_pfn_database);
    }

    /* Method 3: EPROCESS UserDirectoryTableBase - fallback when not encrypted */
    PUCHAR dtb_ptr = (PUCHAR)target_process + globals::offsets::i_user_dirbase;
    if (MmIsAddressValid(dtb_ptr) && MmIsAddressValid(dtb_ptr + sizeof(ULONGLONG) - 1))
        dtb = *(PULONGLONG)dtb_ptr;
    else
        dtb = 0;
    if (dtb && (dtb >> (PAGE_OFFSET_SIZE + 44)) != CR3_ENC_MARKER) {
        ULONGLONG cand = dtb & PMASK;
        if (!base_addr || validate_cr3_with_base(cand, base_addr)) {
            *out_cr3 = cand;
            dbg_req("get_cr3: Method3 EPROCESS OK cr3=0x%llx\n", *out_cr3);
            return STATUS_SUCCESS;
        }
    }

    /* Method 4: KeStackAttachProcess + __readcr3 - LAST (UC: EAC detects __readcr3/attach; use only when 1–3 fail). */
    __try {
        KeStackAttachProcess(target_process, &apc_state);
        attached = TRUE;
        dtb = __readcr3() & PMASK;
        KeUnstackDetachProcess(&apc_state);
        attached = FALSE;
        if (dtb) {
            *out_cr3 = dtb;
            dbg_req("get_cr3: Method4 KeStackAttach OK cr3=0x%llx\n", *out_cr3);
            return STATUS_SUCCESS;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        if (attached) KeUnstackDetachProcess(&apc_state);
    }

    dbg_req("get_cr3: all methods failed\n");
    return STATUS_UNSUCCESSFUL;
    } /* end __try */
    __except (EXCEPTION_EXECUTE_HANDLER) {
        if (attached) KeUnstackDetachProcess(&apc_state);
        dbg_req("get_cr3: exception (bad offset or physical read)\n");
        return STATUS_UNSUCCESSFUL;
    }
}

/* Get CR3 for pid; uses cache to avoid repeated attach/EPROCESS read.
 * base_override: when non-zero, use for validation (usermode base from find_image). */
static ULONGLONG get_cr3_cached(ULONG pid, ULONGLONG base_override) {
    cr3_cache_init();

    ULONGLONG cr3 = cr3_cache_lookup(pid);
    if (cr3)
        return cr3;

    PEPROCESS target = NULL;
    if (!NT_SUCCESS(safe_PsLookupProcessByProcessId((HANDLE)pid, &target)) || !target) {
        dbg_req("get_cr3_cached: PsLookupProcessByProcessId failed pid=%u\n", pid);
        return 0;
    }

    NTSTATUS status = get_cr3_internal(target, &cr3, base_override);
    ObDereferenceObject(target);

    if (NT_SUCCESS(status) && cr3) {
        cr3_cache_store(pid, cr3);
        return cr3;
    }
    return 0;
}

NTSTATUS get_process_cr3(PMEMORY_OPERATION_DATA x) {
    ULONGLONG cr3 = get_cr3_cached(x->pid, 0);
    if (!cr3) {
        message("get_process_cr3: Failed to find CR3\n");
        return STATUS_UNSUCCESSFUL;
    }
    RtlCopyMemory((void*)x->cr3, &cr3, sizeof(cr3));
    return STATUS_SUCCESS;
}

struct cache {
    uintptr_t Address;
    UINT64 Value;
};

static cache cached_pml4e[512];

UINT64 read_cached(UINT64 address, cache* cached_entry, SIZE_T* readsize) {
    if (cached_entry->Address == address) {
        return cached_entry->Value;
    }

    read(PVOID(address), &cached_entry->Value, sizeof(cached_entry->Value), readsize);

    cached_entry->Address = address;

    return cached_entry->Value;
}

UINT64 translate_linear(UINT64 directoryTableBase, UINT64 virtualAddress) {
    /* CR3 lower 12 bits contain PCID/flags - must mask to get 4KB-aligned PML4 base */
    directoryTableBase &= PMASK;

    UINT64 pageOffset = virtualAddress & ((1ULL << PAGE_OFFSET_SIZE) - 1);
    UINT64 pte = (virtualAddress >> 12) & IDX_9;
    UINT64 pt = (virtualAddress >> 21) & IDX_9;
    UINT64 pd = (virtualAddress >> 30) & IDX_9;
    UINT64 pdp = (virtualAddress >> 39) & IDX_9;

    SIZE_T readsize = 0;
    UINT64 pdpe = 0;

    pdpe = read_cached(directoryTableBase + 8 * pdp, &cached_pml4e[pdp], &readsize);
    if ((pdpe & 1) == 0)
        return 0;

    UINT64 pde = 0;

    read(PVOID((pdpe & PMASK) + 8 * pd), &pde, sizeof(pde), &readsize);
    if ((pde & 1) == 0)
        return 0;

    if (pde & PDE_LARGE_BIT) {  /* PDPT 1GB page */
        return (pde & PMASK) + (virtualAddress & ((1ULL << 30) - 1));
    }

    UINT64 pteAddr = 0;

    read(PVOID((pde & PMASK) + 8 * pt), &pteAddr, sizeof(pteAddr), &readsize);
    if ((pteAddr & 1) == 0)
        return 0;

    if (pteAddr & PDE_LARGE_BIT) {  /* PD 2MB page */
        return (pteAddr & PMASK) + (virtualAddress & ((1ULL << 21) - 1));
    }

    UINT64 finalAddr = 0;

    read(PVOID((pteAddr & PMASK) + 8 * pte), &finalAddr, sizeof(finalAddr), &readsize);
    finalAddr &= PMASK;

    if (finalAddr == 0)
        return 0;

    return finalAddr + pageOffset;
}

/* Validate that CR3 correctly translates base_va to a physical page starting with MZ (PE header).
 * Use this to reject wrong CR3 from Methods 2/3/4 - only Method 1 (dirbase_from_base) pre-validates. */
static BOOLEAN validate_cr3_with_base(ULONGLONG cr3_val, ULONGLONG base_va) {
    if (!cr3_val || !base_va) return FALSE;
    UINT64 phys = (UINT64)translate_linear((UINT64)cr3_val, base_va);
    if (!phys) return FALSE;
    USHORT mz = 0;
    SIZE_T rs = 0;
    return NT_SUCCESS(read(PVOID(phys), &mz, sizeof(mz), &rs)) && (mz == FLUSHCOMM_PE_MAGIC_MZ);
}

ULONG64 find_min(INT32 g, SIZE_T f) {
    INT32 h = (INT32)f;
    ULONG64 result = 0;

    result = (((g) < (h)) ? (g) : (h));

    return result;
}

/* KeStackAttachProcess-based read/write - bypasses physical translation.
 * When attached to target, x->buffer (caller addr) is invalid - use kernel temp buffer. */
static NTSTATUS frw_virtual(PRW x, PEPROCESS process) {
    PVOID temp_buf = ExAllocatePoolWithTag(NonPagedPool, x->size, EVASION_POOL_TAG_COPY_R);
    if (!temp_buf)
        return STATUS_INSUFFICIENT_RESOURCES;

    /* For write: copy source from caller (valid now) to temp before attaching */
    if (x->write)
        RtlCopyMemory(temp_buf, PVOID(x->buffer), x->size);

    KAPC_STATE apc_state = { 0 };
    KeStackAttachProcess(process, &apc_state);

    NTSTATUS status = STATUS_SUCCESS;
    __try {
        if (x->write) {
            RtlCopyMemory(PVOID(x->address), temp_buf, x->size);
        } else {
            RtlCopyMemory(temp_buf, PVOID(x->address), x->size);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        status = GetExceptionCode();
        if (status == STATUS_ACCESS_VIOLATION)
            message("frw_virtual: Access violation at 0x%llx\n", x->address);
    }

    KeUnstackDetachProcess(&apc_state);

    if (NT_SUCCESS(status) && !x->write)
        RtlCopyMemory(PVOID(x->buffer), temp_buf, x->size);

    ExFreePoolWithTag(temp_buf, EVASION_POOL_TAG_COPY_R);
    return status;
}

/* Physical read/write path - no KeStackAttachProcess (EAC monitors attach).
 * dest_proc: process owning x->buffer (caller). When non-NULL, use MmCopyVirtualMemory so kernel
 * can correctly read/write usermode buffers in the caller process. */
static NTSTATUS frw_physical(PRW x, ULONGLONG process_dirbase, PEPROCESS dest_proc) {
    SIZE_T total_size = x->size;
    SIZE_T bytes_processed = 0;

    typedef NTSTATUS(*MmCopyVirtualMemory_t)(PEPROCESS, PVOID, PEPROCESS, PVOID, SIZE_T, KPROCESSOR_MODE, PSIZE_T);
    static MmCopyVirtualMemory_t pMmCopyVirtualMemory = nullptr;
    if (dest_proc && !pMmCopyVirtualMemory)
        pMmCopyVirtualMemory = (MmCopyVirtualMemory_t)get_system_routine_obf(OBF_MmCopyVirtualMemory, sizeof(OBF_MmCopyVirtualMemory));

    PVOID temp_buf = dest_proc ? ExAllocatePoolWithTag(NonPagedPool, (SIZE_T)total_size, EVASION_POOL_TAG_COPY_R) : nullptr;
    if (dest_proc && !temp_buf)
        return STATUS_INSUFFICIENT_RESOURCES;

    if (x->write && dest_proc && pMmCopyVirtualMemory) {
        SIZE_T cpy = 0;
        if (!NT_SUCCESS(pMmCopyVirtualMemory(dest_proc, PVOID(x->buffer), PsGetCurrentProcess(), temp_buf,
            (SIZE_T)total_size, KernelMode, &cpy))) {
            ExFreePoolWithTag(temp_buf, EVASION_POOL_TAG_COPY_R);
            return STATUS_UNSUCCESSFUL;
        }
    }

    while (bytes_processed < total_size) {
        ULONG64 current_address = (ULONG64)x->address + bytes_processed;
        INT64 physical_address = translate_linear(process_dirbase, current_address);

        if (!physical_address) {
            if (temp_buf) ExFreePoolWithTag(temp_buf, EVASION_POOL_TAG_COPY_R);
            message("frw_physical: Failed to translate address 0x%llx\n", current_address);
            return STATUS_UNSUCCESSFUL;
        }

        ULONG64 bytes_in_page = PAGE_SIZE - (physical_address & ((1ULL << PAGE_OFFSET_SIZE) - 1));
        ULONG64 bytes_to_process = min(bytes_in_page, total_size - bytes_processed);
        SIZE_T bytes_transferred = 0;
        PVOID io_buf = temp_buf ? (PUCHAR)temp_buf + bytes_processed : PVOID((ULONG64)x->buffer + bytes_processed);

        if (x->write) {
            NTSTATUS st = write(PVOID(physical_address), io_buf, bytes_to_process, &bytes_transferred);
            if (!NT_SUCCESS(st)) {
                if (temp_buf) ExFreePoolWithTag(temp_buf, EVASION_POOL_TAG_COPY_R);
                message("frw_physical: Write failed at 0x%llx\n", current_address);
                return st;
            }
        } else {
            NTSTATUS st = read(PVOID(physical_address), io_buf, bytes_to_process, &bytes_transferred);
            if (!NT_SUCCESS(st)) {
                if (temp_buf) ExFreePoolWithTag(temp_buf, EVASION_POOL_TAG_COPY_R);
                message("frw_physical: Read failed at 0x%llx\n", current_address);
                return st;
            }
        }

        bytes_processed += bytes_transferred;
        if (bytes_transferred < bytes_to_process)
            break;
    }

    if (!x->write && temp_buf && dest_proc && pMmCopyVirtualMemory) {
        SIZE_T cpy = 0;
        pMmCopyVirtualMemory(PsGetCurrentProcess(), temp_buf, dest_proc, PVOID(x->buffer), (SIZE_T)total_size, KernelMode, &cpy);
    }
    if (temp_buf) ExFreePoolWithTag(temp_buf, EVASION_POOL_TAG_COPY_R);
    return STATUS_SUCCESS;
}

/* dest_proc: process that owns x->buffer (caller). Required for FlushComm - kernel can't write to usermode addr in another process. */
NTSTATUS frw(PRW x, PEPROCESS dest_proc) {
    if (x->security != CODE_SECURITY) {
        message("frw: Invalid security code\n");
        return STATUS_UNSUCCESSFUL;
    }

    if (!x->process_id) {
        message("frw: Invalid process ID\n");
        return STATUS_UNSUCCESSFUL;
    }

    /* Prefer physical path when CR3 cached - avoids KeStackAttachProcess (EAC detection vector) */
    ULONGLONG process_dirbase = get_cr3_cached(x->process_id, 0);
    if (process_dirbase) {
        NTSTATUS status = frw_physical(x, process_dirbase, dest_proc);
        if (NT_SUCCESS(status))
            return STATUS_SUCCESS;
    }

    PEPROCESS process = NULL;
    NTSTATUS status = safe_PsLookupProcessByProcessId((HANDLE)x->process_id, &process);
    if (!NT_SUCCESS(status) || !process) {
        message("frw: Failed to lookup process %d\n", x->process_id);
        return STATUS_UNSUCCESSFUL;
    }

    /* Fallback: KeStackAttachProcess - needed when no CR3 cache or physical failed */
    status = frw_virtual(x, process);
    ObDereferenceObject(process);

    if (NT_SUCCESS(status))
        return STATUS_SUCCESS;

    /* Second fallback: physical path (cache now populated by get_cr3 in frw_virtual path) */
    process_dirbase = get_cr3_cached(x->process_id, 0);
    if (!process_dirbase) {
        message("frw: Failed to get CR3 for process %d\n", x->process_id);
        return STATUS_UNSUCCESSFUL;
    }
    return frw_physical(x, process_dirbase, dest_proc);
}

NTSTATUS fba(PBA x) {
    ULONGLONG image_base = 0;
    if (x->security != CODE_SECURITY)
        return STATUS_UNSUCCESSFUL;

    if (!x->process_id)
        return STATUS_UNSUCCESSFUL;

    PEPROCESS process = NULL;
    if (!NT_SUCCESS(safe_PsLookupProcessByProcessId((HANDLE)x->process_id, &process))) {
        dbg_req("fba: PsLookupProcessByProcessId failed pid=%d\n", x->process_id);
        return STATUS_UNSUCCESSFUL;
    }

    image_base = (ULONGLONG)PsGetProcessSectionBaseAddress(process);
    dbg_req("fba: PsGetProcessSectionBaseAddress=%llx\n", image_base);

        if (!image_base) {
            ULONGLONG process_dirbase = get_cr3_cached(x->process_id, 0);
            PPEB peb = NULL;
            PUCHAR peb_ptr = (PUCHAR)process + globals::offsets::i_peb;
            if (MmIsAddressValid(peb_ptr))
                peb = *(PPEB*)peb_ptr;

            if (process_dirbase && peb) {
                __try {
                    SIZE_T readsize = 0;
                    ULONGLONG ldr_rva = 0x18;
                    ULONGLONG list_rva = 0x10;
                    ULONGLONG dllbase_rva = 0x30;

                    INT64 phy = translate_linear(process_dirbase, (ULONGLONG)peb);
                    if (phy) {
                        ULONGLONG ldr = 0;
                        read(PVOID(phy + ldr_rva), &ldr, sizeof(ldr), &readsize);
                        if (ldr) {
                            phy = translate_linear(process_dirbase, ldr);
                            if (phy) {
                                ULONGLONG flink = 0;
                                read(PVOID(phy + list_rva), &flink, sizeof(flink), &readsize);
                                if (flink) {
                                    phy = translate_linear(process_dirbase, flink);
                                    if (phy) {
                                        read(PVOID(phy + dllbase_rva), &image_base, sizeof(image_base), &readsize);
                                    }
                                }
                            }
                        }
                    }
                } __except (EXCEPTION_EXECUTE_HANDLER) {
                    image_base = 0;
                }
            }
        }

    ObDereferenceObject(process);

    if (!image_base) {
        dbg_req("fba: image_base still 0 after PEB fallback\n");
        return STATUS_UNSUCCESSFUL;
    }

    dbg_req("fba: success base=0x%llx\n", image_base);
    RtlCopyMemory(x->address, &image_base, sizeof(image_base));
    return STATUS_SUCCESS;
}

NTSTATUS fget_guarded_region(PGA x) {
    if (x->security != CODE_SECURITY)
        return STATUS_UNSUCCESSFUL;

    ZwQuerySystemInformation_t pZwQSI = get_ZwQuerySystemInformation_fn();
    if (!pZwQSI) return STATUS_PROCEDURE_NOT_FOUND;
    ULONG infoLen = 0;
    NTSTATUS status = pZwQSI((ULONG)SystemBigPoolInformationClass, &infoLen, 0, &infoLen);
    PSYSTEM_BIGPOOL_INFORMATION pPoolInfo = 0;

    while (status == STATUS_INFO_LENGTH_MISMATCH)
    {
        if (pPoolInfo)
            ExFreePoolWithTag(pPoolInfo, EVASION_POOL_TAG_LIST_R);

        pPoolInfo = (PSYSTEM_BIGPOOL_INFORMATION)ExAllocatePoolWithTag(NonPagedPool, infoLen, EVASION_POOL_TAG_LIST_R);
        status = pZwQSI((ULONG)SystemBigPoolInformationClass, pPoolInfo, infoLen, &infoLen);
    }

    if (pPoolInfo)
    {
        for (unsigned int i = 0; i < pPoolInfo->Count; i++)
        {
            SYSTEM_BIGPOOL_ENTRY* Entry = &pPoolInfo->AllocatedInfo[i];
            PVOID VirtualAddress;
            VirtualAddress = (PVOID)((uintptr_t)Entry->VirtualAddress & ~1ull);
            SIZE_T SizeInBytes = Entry->SizeInBytes;
            BOOLEAN NonPaged = Entry->NonPaged;

            if (Entry->NonPaged && Entry->SizeInBytes == 0x200000) {
                UCHAR expectedTag[8];
                routine_obf_decode_narrow(OBF_TagTnoC, 4, expectedTag, sizeof(expectedTag));
                if (memcmp(Entry->Tag, expectedTag, 4) == 0) {
                    RtlCopyMemory((void*)x->address, &Entry->VirtualAddress, sizeof(Entry->VirtualAddress));
                    return STATUS_SUCCESS;
                }
            }

        }

        ExFreePoolWithTag(pPoolInfo, EVASION_POOL_TAG_LIST_R);
    }

    return STATUS_SUCCESS;
}

NTSTATUS io_controller(PDEVICE_OBJECT device_obj, PIRP irp) {
    UNREFERENCED_PARAMETER(device_obj);
#if FLUSHCOMM_DEBUG
    if (device_obj && device_obj->DriverObject && device_obj->DriverObject->DriverStart) {
        PDRIVER_OBJECT drv = device_obj->DriverObject;
        SIZE_T sz = drv->DriverSize ? drv->DriverSize : 0x10000;
        if (!is_rip_in_range(drv->DriverStart, sz))
            message("io_controller: RIP 0x%llx outside driver image\n", (ULONG64)_ReturnAddress());
    }
#endif
    NTSTATUS status = STATUS_SUCCESS;
    ULONG bytes = 0;

    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(irp);
    ULONG code = stack->Parameters.DeviceIoControl.IoControlCode;
    ULONG size = stack->Parameters.DeviceIoControl.InputBufferLength;

    if (code == CODE_RW) {
        if (size == sizeof(RW)) {
            PRW req = (PRW)(irp->AssociatedIrp.SystemBuffer);
            status = frw(req, IoGetRequestorProcess(irp));
            bytes = sizeof(RW);
        }
        else {
            status = STATUS_INFO_LENGTH_MISMATCH;
            bytes = 0;
        }
    }
    else if (code == CODE_BA) {
        if (size == sizeof(BA)) {
            PBA req = (PBA)(irp->AssociatedIrp.SystemBuffer);
            status = fba(req);
            bytes = sizeof(BA);
        }
        else {
            status = STATUS_INFO_LENGTH_MISMATCH;
            bytes = 0;
        }
    }
    else if (code == CODE_GET_GUARDED_REGION) {
        if (size == sizeof(GA)) {
            PGA req = (PGA)(irp->AssociatedIrp.SystemBuffer);
            status = fget_guarded_region(req);
            bytes = sizeof(GA);
        }
        else {
            status = STATUS_INFO_LENGTH_MISMATCH;
            bytes = 0;
        }
    }
    else if (code == CODE_GET_DIR_BASE) {
        if (size == sizeof(MEMORY_OPERATION_DATA)) {
            PMEMORY_OPERATION_DATA req = (PMEMORY_OPERATION_DATA)(irp->AssociatedIrp.SystemBuffer);
            status = get_process_cr3(req);
            bytes = sizeof(MEMORY_OPERATION_DATA);
        }
        else {
            status = STATUS_INFO_LENGTH_MISMATCH;
            bytes = 0;
        }
    }
    else if (code == CODE_SPOOFER_CTRL) {
        /* Stub - EAC spoofer not implemented in Project3 */
        status = STATUS_SUCCESS;
        bytes = 0;
    }
    else if (code == IOCTL_MOUSE_MOVE) {
        if (size >= sizeof(MOUSE_REQUEST)) {
            PMOUSE_REQUEST req = (PMOUSE_REQUEST)(irp->AssociatedIrp.SystemBuffer);
            /* Mouse move - stub: can be extended for actual mouse injection */
            message("IOCTL_MOUSE_MOVE: x=%ld y=%ld flags=0x%x\n", req->x, req->y, req->button_flags);
            status = STATUS_SUCCESS;
            bytes = sizeof(MOUSE_REQUEST);
        }
        else {
            status = STATUS_INFO_LENGTH_MISMATCH;
            bytes = 0;
        }
    }
    else {
        status = STATUS_INVALID_DEVICE_REQUEST;
        bytes = 0;
    }

    irp->IoStatus.Status = status;        // ← irp
    irp->IoStatus.Information = bytes;    // ← irp
    IoCompleteRequest(irp, IO_NO_INCREMENT);
    return status;
}

NTSTATUS unsupported_dispatch(PDEVICE_OBJECT device_obj, PIRP irp) {
    UNREFERENCED_PARAMETER(device_obj);

    irp->IoStatus.Status = STATUS_NOT_SUPPORTED;
    IoCompleteRequest(irp, IO_NO_INCREMENT);

    return irp->IoStatus.Status;
}

NTSTATUS dispatch_handler(PDEVICE_OBJECT device_obj, PIRP irp) {
    UNREFERENCED_PARAMETER(device_obj);

    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(irp);

    switch (stack->MajorFunction) {
    case IRP_MJ_CREATE:
        message("IRP_MJ_CREATE received\n");
        break;
    case IRP_MJ_CLOSE:
        message("IRP_MJ_CLOSE received\n");
        break;
    default:
        break;
    }

    IoCompleteRequest(irp, IO_NO_INCREMENT);
    return irp->IoStatus.Status;
}

void unload_drv(PDRIVER_OBJECT drv_obj) {
    message("Unloading driver...\n");
    NTSTATUS status = { };

    status = IoDeleteSymbolicLink(&link);

    if (!NT_SUCCESS(status))
        return;

    IoDeleteDevice(drv_obj->DeviceObject);
    message("Driver unloaded successfully\n");
}

NTSTATUS NTAPI IopInvalidDeviceRequest(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP Irp)
{
    UNREFERENCED_PARAMETER(DeviceObject);
    Irp->IoStatus.Status = STATUS_INVALID_DEVICE_REQUEST;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_INVALID_DEVICE_REQUEST;
}

void InitializeIoDriverObjectType() {
    WCHAR driverTypeNameBuf[16];
    routine_obf_decode_to_wide(OBF_Driver, sizeof(OBF_Driver), driverTypeNameBuf, 16);
    UNICODE_STRING driverTypeName;
    RtlInitUnicodeString(&driverTypeName, driverTypeNameBuf);
    ObReferenceObjectByName(&driverTypeName, OBJ_CASE_INSENSITIVE, NULL, 0, *globals::IoDriverObjectType, KernelMode, NULL, (PVOID*)&globals::IoDriverObjectType);
}

NTSTATUS LoadDriverIntoSignedMemory(PDRIVER_OBJECT DriverObject) {
    WCHAR driverIdeBuf[16];
    routine_obf_decode_to_wide(OBF_DriverIDE, sizeof(OBF_DriverIDE), driverIdeBuf, 16);
    UNICODE_STRING signedDriverName;
    RtlInitUnicodeString(&signedDriverName, driverIdeBuf);

    PDRIVER_OBJECT signedDriverObject;
    NTSTATUS status = ObReferenceObjectByName(&signedDriverName, OBJ_CASE_INSENSITIVE, NULL, 0, *globals::IoDriverObjectType, KernelMode, NULL, (PVOID*)&signedDriverObject);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    if (signedDriverObject->DriverSection) {
        DriverObject->DriverSection = signedDriverObject->DriverSection;
    }
    else {
        ObDereferenceObject(signedDriverObject);
        return STATUS_UNSUCCESSFUL;
    }

    ObDereferenceObject(signedDriverObject);
    return STATUS_SUCCESS;
}

void HideDriver(PDRIVER_OBJECT DriverObject) {
    PLDR_DATA_TABLE_ENTRY entry = (PLDR_DATA_TABLE_ENTRY)DriverObject->DriverSection;
    if (!entry) {
        message("WARNING: Cannot hide driver - DriverSection is NULL\n");
        return;
    }
    RemoveEntryList(&entry->InLoadOrderLinks);
    RemoveEntryList(&entry->InMemoryOrderLinks);
    RemoveEntryList(&entry->InInitializationOrderLinks);
    InitializeListHead(&entry->InLoadOrderLinks);
    InitializeListHead(&entry->InMemoryOrderLinks);
    InitializeListHead(&entry->InInitializationOrderLinks);
    message("Driver hidden from PsLoadedModuleList\n");
}

NTSTATUS DispatchCreate(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    UNREFERENCED_PARAMETER(DeviceObject);

    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;

    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

NTSTATUS initialize_driver(_In_ PDRIVER_OBJECT drv_obj, _In_ PUNICODE_STRING path) {
    UNREFERENCED_PARAMETER(path);
    if (!drv_obj) return STATUS_INVALID_PARAMETER;
    NTSTATUS status = STATUS_SUCCESS;
    PDEVICE_OBJECT device_obj = NULL;

    /* Device/link name from FLUSHCOMM_MAGIC - prefixes decoded at runtime (no \Device\ or \DosDevices\Global\ literal in .rdata) */
    static WCHAR devBuf[64], linkBuf[64];
    WCHAR devPrefix[20], linkPrefix[24];
    routine_obf_decode_to_wide(OBF_DevicePrefix, sizeof(OBF_DevicePrefix), devPrefix, 20);
    routine_obf_decode_to_wide(OBF_LinkPrefix, sizeof(OBF_LinkPrefix), linkPrefix, 24);
    ULONG ma = (ULONG)(FLUSHCOMM_MAGIC & 0xFFFFFFFF);
    ULONG mb = (ULONG)(FLUSHCOMM_MAGIC >> 32);
    if (!NT_SUCCESS(RtlStringCbPrintfW(devBuf, sizeof(devBuf), L"%ws{%08X-%04X-%04X-%04X-%08X%04X}",
        devPrefix, ma, (USHORT)(ma >> 16), (USHORT)(ma), (USHORT)(mb), mb, (USHORT)(mb >> 16))))
        return STATUS_BUFFER_OVERFLOW;
    if (!NT_SUCCESS(RtlStringCbPrintfW(linkBuf, sizeof(linkBuf), L"%ws{%08X-%04X-%04X-%04X-%08X%04X}",
        linkPrefix, ma, (USHORT)(ma >> 16), (USHORT)(ma), (USHORT)(mb), mb, (USHORT)(mb >> 16))))
        return STATUS_BUFFER_OVERFLOW;
    RtlInitUnicodeString(&name, devBuf);
    RtlInitUnicodeString(&link, linkBuf);

    status = IoCreateDevice(drv_obj, 0, &name, FILE_DEVICE_UNKNOWN, FILE_DEVICE_SECURE_OPEN, FALSE, &device_obj);
    if (!NT_SUCCESS(status)) {
        message("[-] IoCreateDevice failed: 0x%X\n", status);
        return status;
    }

    /* HideDriver disabled - under kdmapper DriverSection is fake; RemoveEntryList corrupts
       kernel lists → BSOD after g_KernelHashBucketList cleaned */

    status = IoCreateSymbolicLink(&link, &name);
    if (!NT_SUCCESS(status)) {
        message("[-] IoCreateSymbolicLink failed: 0x%X\n", status);
        IoDeleteDevice(device_obj);
        return status;
    }

    for (int i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; i++) {
        drv_obj->MajorFunction[i] = IopInvalidDeviceRequest;
    }

    drv_obj->MajorFunction[IRP_MJ_CREATE] = dispatch_handler;
    drv_obj->MajorFunction[IRP_MJ_CLOSE] = dispatch_handler;
    drv_obj->MajorFunction[IRP_MJ_DEVICE_CONTROL] = io_controller;
    drv_obj->DriverUnload = unload_drv;

    device_obj->Flags |= DO_BUFFERED_IO;
    device_obj->Flags &= ~DO_DEVICE_INITIALIZING;

    status = load_dynamic_functions();
    if (!NT_SUCCESS(status)) {
        message("[-] load_dynamic_functions failed: 0x%X\n", status);
        IoDeleteSymbolicLink(&link);
        IoDeleteDevice(device_obj);
        return status;
    }



    message("[+] Driver initialized successfully\n");
    /* Do not null DriverSection - can cause issues under kdmapper */
    return STATUS_SUCCESS;
}


IRP_MJ_DEVICE_CONTROL_HANDLER g_orig_device_control = nullptr;

/* Worker routine for mouse inject - runs in system context, adds jitter & rate limit */
void mouse_inject::mouse_work_routine(PDEVICE_OBJECT DeviceObject, PVOID Context) {
    UNREFERENCED_PARAMETER(DeviceObject);
    mouse_inject::MOUSE_WORK_CTX* ctx = (mouse_inject::MOUSE_WORK_CTX*)Context;
    if (!ctx) return;

    /* Delay jitter: 0-2ms random to break fixed timing patterns */
    LARGE_INTEGER jitter;
    jitter.QuadPart = -(LONGLONG)((RtlRandomEx(nullptr) % (mouse_inject::JITTER_MAX_MS + 1)) * 10000);
    KeDelayExecutionThread(KernelMode, FALSE, &jitter);

    /* Rate limit: enforce min 5ms between moves (human-like) */
    LARGE_INTEGER now;
    KeQuerySystemTime(&now);
    LONGLONG elapsed = now.QuadPart - mouse_inject::g_last_move_time.QuadPart;  /* 100ns units */
    if (mouse_inject::g_last_move_time.QuadPart != 0 && elapsed < mouse_inject::MIN_MOVE_INTERVAL_100NS) {
        LARGE_INTEGER wait;
        wait.QuadPart = -(mouse_inject::MIN_MOVE_INTERVAL_100NS - elapsed);
        KeDelayExecutionThread(KernelMode, FALSE, &wait);
    }

    mouse_inject::do_move(ctx->DeltaX, ctx->DeltaY, ctx->ButtonFlags);
    KeQuerySystemTime(&mouse_inject::g_last_move_time);
    ExFreePoolWithTag(ctx, EVASION_POOL_TAG_WORK_R);
}

/* DeviceIoControl-based communication - hook IRP_MJ_DEVICE_CONTROL */
NTSTATUS FlushComm_HookHandler(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(Irp);
    ULONG ioctl = irpSp->Parameters.DeviceIoControl.IoControlCode;

    /* IOCTL_REXCOMM_PING: when codecave installed, run from signed driver (RIP in valid module) */
    if (ioctl == IOCTL_REXCOMM_PING) {
        if (g_ping_codecave)
            return g_ping_codecave(DeviceObject, Irp);
        /* Fallback: inline handler */
        PVOID outBuf = Irp->AssociatedIrp.SystemBuffer;
        ULONG outLen = irpSp->Parameters.DeviceIoControl.OutputBufferLength;
        if (outBuf && outLen >= sizeof(ULONG64)) {
            *(ULONG64*)outBuf = DEFAULT_MAGGICCODE;
            Irp->IoStatus.Information = sizeof(ULONG64);
        }
        Irp->IoStatus.Status = STATUS_SUCCESS;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return STATUS_SUCCESS;
    }

    if (ioctl != IOCTL_REXCOMM) {
        if (g_orig_device_control)
            return g_orig_device_control(DeviceObject, Irp);
        Irp->IoStatus.Status = STATUS_INVALID_DEVICE_REQUEST;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return STATUS_INVALID_DEVICE_REQUEST;
    }

    FlushComm_ProcessSharedBuffer(DeviceObject, Irp);
    return STATUS_SUCCESS;
}

/* IRP_MJ_FLUSH_BUFFERS: alternative to DeviceIoControl - FlushFileBuffers triggers same processing */
NTSTATUS FlushComm_FlushHandler(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    FlushComm_ProcessSharedBuffer(DeviceObject, Irp);
    return STATUS_SUCCESS;
}

/* Inner section processing: reads type/args from base, writes status to base+80. If Irp non-null, completes it. Used by IRP path and ALPC fallback. */
static NTSTATUS FlushComm_ProcessSectionInner(PUCHAR base, PIRP Irp, PDEVICE_OBJECT DeviceObject) {
    NTSTATUS* pStatus = (NTSTATUS*)(base + FLUSHCOMM_STATUS_OFFSET);
    __try {
        __try {
            ULONG64 magic = *(ULONG64*)base;
            if (magic != DEFAULT_MAGGICCODE) {
                if (Irp) { Irp->IoStatus.Status = STATUS_SUCCESS; IoCompleteRequest(Irp, IO_NO_INCREMENT); }
                return STATUS_SUCCESS;
            }
            REQUEST_TYPE reqType = (REQUEST_TYPE)*(ULONG*)(base + 8);
            PVOID argsPtr = base + 16;
            PVOID dataArea = base + FLUSHCOMM_DATA_OFFSET;
            SIZE_T dataSize = FLUSHCOMM_DATA_SIZE;
            NTSTATUS resultStatus = STATUS_SUCCESS;

            switch (reqType) {
            case REQ_READ: {
                REQUEST_READ r;
                RtlCopyMemory(&r, argsPtr, sizeof(r));
                r.Size = (ULONG)min((SIZE_T)r.Size, dataSize);
                RW x = { CODE_SECURITY, (INT32)r.ProcessId, (ULONGLONG)r.Src, (ULONGLONG)dataArea, r.Size, FALSE };
                resultStatus = FRW_INVOKE(&x, nullptr);
                break;
            }
            case REQ_WRITE: {
                REQUEST_WRITE r;
                RtlCopyMemory(&r, argsPtr, sizeof(r));
                r.Size = (ULONG)min((SIZE_T)r.Size, dataSize);
                RW x = { CODE_SECURITY, (INT32)r.ProcessId, (ULONGLONG)r.Dest, (ULONGLONG)dataArea, r.Size, TRUE };
                resultStatus = FRW_INVOKE(&x, nullptr);
                break;
            }
            case REQ_MAINBASE: {
                REQUEST_MAINBASE r;
                RtlCopyMemory(&r, argsPtr, sizeof(r));
                ULONGLONG image_base = 0;
                BA x = { CODE_SECURITY, (INT32)r.ProcessId, &image_base };
                resultStatus = FBA_INVOKE(&x);
                if (NT_SUCCESS(resultStatus) && image_base)
                    *(ULONGLONG*)dataArea = image_base;
                break;
            }
            case REQ_GET_DIR_BASE: {
                REQUEST_GET_DIR_BASE r;
                RtlCopyMemory(&r, argsPtr, sizeof(r));
                ULONGLONG cr3_val = get_cr3_cached(r.ProcessId, r.InBase);
                if (cr3_val)
                    *(ULONGLONG*)dataArea = cr3_val;
                resultStatus = cr3_val ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
                break;
            }
            case REQ_GET_GUARDED_REGION: {
                REQUEST_GET_GUARDED_REGION r;
                RtlCopyMemory(&r, argsPtr, sizeof(r));
                GA x = { CODE_SECURITY, (ULONGLONG*)dataArea };
                resultStatus = fget_guarded_region(&x);
                break;
            }
            case REQ_MOUSE_MOVE: {
                REQUEST_MOUSE_MOVE r;
                RtlCopyMemory(&r, argsPtr, sizeof(r));
                LARGE_INTEGER tiny_delay;
                tiny_delay.QuadPart = -(LONGLONG)((RtlRandomEx(nullptr) % 31) * 100);
                KeDelayExecutionThread(KernelMode, FALSE, &tiny_delay);
#if FLUSHCOMM_MOUSE_SYNC
                mouse_inject::move(r.DeltaX, r.DeltaY, r.ButtonFlags);
#else
                mouse_inject::move_async(DeviceObject, r.DeltaX, r.DeltaY, r.ButtonFlags);
#endif
                resultStatus = STATUS_SUCCESS;
                break;
            }
            case REQ_GET_PID_BY_NAME: {
                REQUEST_GET_PID_BY_NAME r;
                RtlCopyMemory(&r, argsPtr, sizeof(r));
                r.Name[REQUEST_GET_PID_BY_NAME_NAMELEN - 1] = '\0';
                ULONG pid = find_pid_by_image_name(r.Name);
                *(ULONG*)dataArea = pid;
                resultStatus = pid ? STATUS_SUCCESS : STATUS_NOT_FOUND;
                break;
            }
            case REQ_INIT:
                resultStatus = STATUS_SUCCESS;
                break;
            default:
                resultStatus = STATUS_INVALID_PARAMETER;
                break;
            }
            *pStatus = resultStatus;
            if (Irp) { Irp->IoStatus.Status = STATUS_SUCCESS; IoCompleteRequest(Irp, IO_NO_INCREMENT); }
            return resultStatus;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            *pStatus = STATUS_UNSUCCESSFUL;
            if (Irp) { Irp->IoStatus.Status = STATUS_SUCCESS; IoCompleteRequest(Irp, IO_NO_INCREMENT); }
            return STATUS_UNSUCCESSFUL;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return STATUS_UNSUCCESSFUL;
    }
}

NTSTATUS FlushComm_ProcessSectionBuffer(void) {
#if FLUSHCOMM_USE_SECTION
    if (!g_section_view || !g_section_process) return STATUS_UNSUCCESSFUL;
    KAPC_STATE apc = { 0 };
    KeStackAttachProcess(g_section_process, &apc);
    NTSTATUS st = FlushComm_ProcessSectionInner((PUCHAR)g_section_view, nullptr, nullptr);
    KeUnstackDetachProcess(&apc);
    return st;
#else
    return STATUS_NOT_SUPPORTED;
#endif
}

void FlushComm_ProcessSharedBuffer(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
#if FLUSHCOMM_USE_SECTION
    /* Lazy map: when init mapping failed (HVCI blocks System/ZwCurrentProcess), map on first request
     * into the caller process (usermode cheat). We're in that process's context during the IRP. */
    if (g_section_handle && !g_section_view && !g_section_process) {
        SIZE_T viewSize = FLUSHCOMM_SECTION_SIZE;
        NTSTATUS st = ZwMapViewOfSection(g_section_handle, ZwCurrentProcess(), &g_section_view, 0, 0, NULL, &viewSize, ViewUnmap, 0, PAGE_READWRITE);
        if (NT_SUCCESS(st) && g_section_view) {
            g_section_process = PsGetCurrentProcess();
            ObReferenceObject(g_section_process);
        }
    }
    if (g_section_view && g_section_process) {
        KAPC_STATE apc = { 0 };
        KeStackAttachProcess(g_section_process, &apc);
        __try {
            FlushComm_ProcessSectionInner((PUCHAR)g_section_view, Irp, DeviceObject);
        }
        __finally {
            KeUnstackDetachProcess(&apc);
        }
        return;
    }
#endif

    /* Fallback: registry + MmCopyVirtualMemory (original path) */
    typedef NTSTATUS(*MmCopyVirtualMemory_t)(PEPROCESS, PVOID, PEPROCESS, PVOID, SIZE_T, KPROCESSOR_MODE, PSIZE_T);
    static MmCopyVirtualMemory_t pMmCopyVirtualMemory = (MmCopyVirtualMemory_t)get_system_routine_obf(OBF_MmCopyVirtualMemory, sizeof(OBF_MmCopyVirtualMemory));
    if (!pMmCopyVirtualMemory) {
        Irp->IoStatus.Status = STATUS_NOT_SUPPORTED;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return;
    }

    UNICODE_STRING valBuf, valPid;
    WCHAR valBufStr[16], valPidStr[16];
    obf_decode_str(OBF_SharedBuffer, OBF_SharedBuffer_LEN, valBufStr, 16);
    obf_decode_str(OBF_SharedPid, OBF_SharedPid_LEN, valPidStr, 16);
    RtlInitUnicodeString(&valBuf, valBufStr);
    RtlInitUnicodeString(&valPid, valPidStr);

    ULONG64 sharedBufAddr = 0, sharedPid = 0;
    if (!NT_SUCCESS(RegReadQword(&g_flushcomm_reg_path, &valBuf, &sharedBufAddr)) || !sharedBufAddr ||
        !NT_SUCCESS(RegReadQword(&g_flushcomm_reg_path, &valPid, &sharedPid)) || !sharedPid) {
        Irp->IoStatus.Status = STATUS_SUCCESS;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return;
    }

    PEPROCESS userProc = nullptr;
    if (!NT_SUCCESS(safe_PsLookupProcessByProcessId((HANDLE)sharedPid, &userProc))) {
        Irp->IoStatus.Status = STATUS_SUCCESS;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return;
    }

    REQUEST_DATA reqData = { 0 };
    SIZE_T copied = 0;
    PVOID reqDataAddr = (PVOID)(sharedBufAddr + 16);
    NTSTATUS status = pMmCopyVirtualMemory(userProc, reqDataAddr,
        PsGetCurrentProcess(), &reqData, sizeof(REQUEST_DATA), KernelMode, &copied);
    ObDereferenceObject(userProc);
    if (!NT_SUCCESS(status) || copied != sizeof(REQUEST_DATA)) {
        Irp->IoStatus.Status = STATUS_SUCCESS;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return;
    }

    ULONG64 magicVal = 0;
    if (reqData.MaggicCode) {
        safe_PsLookupProcessByProcessId((HANDLE)sharedPid, &userProc);
        if (userProc) {
            pMmCopyVirtualMemory(userProc, reqData.MaggicCode, PsGetCurrentProcess(), &magicVal, sizeof(ULONG64), KernelMode, &copied);
            ObDereferenceObject(userProc);
        }
    }
    if (magicVal != DEFAULT_MAGGICCODE) {
        Irp->IoStatus.Status = STATUS_SUCCESS;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return;
    }

    if (!NT_SUCCESS(safe_PsLookupProcessByProcessId((HANDLE)sharedPid, &userProc)))
        userProc = nullptr;

    NTSTATUS resultStatus = STATUS_SUCCESS;
    switch ((REQUEST_TYPE)reqData.Type) {
    case REQ_READ: {
        REQUEST_READ r;
        if (reqData.Arguments && NT_SUCCESS(pMmCopyVirtualMemory(userProc, reqData.Arguments,
            PsGetCurrentProcess(), &r, sizeof(r), KernelMode, &copied))) {
            RW x = { CODE_SECURITY, (INT32)r.ProcessId, (ULONGLONG)r.Src, (ULONGLONG)r.Dest, r.Size, FALSE };
            resultStatus = FRW_INVOKE(&x, userProc);  /* userProc = caller, owns r.Dest */
            safe_PsLookupProcessByProcessId((HANDLE)sharedPid, &userProc);
            if (userProc) {
                pMmCopyVirtualMemory(PsGetCurrentProcess(), &r, userProc, reqData.Arguments, sizeof(r), KernelMode, &copied);
                ObDereferenceObject(userProc);
            }
        }
        break;
    }
    case REQ_WRITE: {
        REQUEST_WRITE r;
        if (reqData.Arguments && NT_SUCCESS(pMmCopyVirtualMemory(userProc, reqData.Arguments,
            PsGetCurrentProcess(), &r, sizeof(r), KernelMode, &copied))) {
            RW x = { CODE_SECURITY, (INT32)r.ProcessId, (ULONGLONG)r.Dest, (ULONGLONG)r.Src, r.Size, TRUE };
            resultStatus = FRW_INVOKE(&x, userProc);  /* userProc = caller, owns r.Src (source buffer) */
        }
        break;
    }
    case REQ_MAINBASE: {
        REQUEST_MAINBASE r;
        if (reqData.Arguments && NT_SUCCESS(pMmCopyVirtualMemory(userProc, reqData.Arguments,
            PsGetCurrentProcess(), &r, sizeof(r), KernelMode, &copied))) {
            dbg_req("REQ_MAINBASE pid=%u\n", r.ProcessId);
            ULONGLONG image_base = 0;
            BA x = { CODE_SECURITY, (INT32)r.ProcessId, &image_base };
            resultStatus = FBA_INVOKE(&x);
            dbg_req("REQ_MAINBASE fba status=0x%X image_base=0x%llx\n", resultStatus, image_base);
            if (NT_SUCCESS(resultStatus) && image_base && r.OutAddress) {
                safe_PsLookupProcessByProcessId((HANDLE)sharedPid, &userProc);
                if (userProc) {
                    pMmCopyVirtualMemory(PsGetCurrentProcess(), &image_base, userProc, r.OutAddress, sizeof(image_base), KernelMode, &copied);
                    ObDereferenceObject(userProc);
                }
            }
        }
        break;
    }
    case REQ_GET_DIR_BASE: {
        REQUEST_GET_DIR_BASE r;
        if (reqData.Arguments && NT_SUCCESS(pMmCopyVirtualMemory(userProc, reqData.Arguments,
            PsGetCurrentProcess(), &r, sizeof(r), KernelMode, &copied))) {
            dbg_req("REQ_GET_DIR_BASE pid=%u base=0x%llx\n", r.ProcessId, r.InBase);
            ULONGLONG cr3_val = get_cr3_cached(r.ProcessId, r.InBase);
            dbg_req("REQ_GET_DIR_BASE cr3=0x%llx\n", cr3_val);
            if (cr3_val && r.OutCr3) {
                safe_PsLookupProcessByProcessId((HANDLE)sharedPid, &userProc);
                if (userProc) {
                    pMmCopyVirtualMemory(PsGetCurrentProcess(), &cr3_val, userProc, r.OutCr3, sizeof(cr3_val), KernelMode, &copied);
                    ObDereferenceObject(userProc);
                }
                resultStatus = STATUS_SUCCESS;
            } else {
                resultStatus = STATUS_UNSUCCESSFUL;
            }
        }
        break;
    }
    case REQ_GET_GUARDED_REGION: {
        REQUEST_GET_GUARDED_REGION r;
        if (reqData.Arguments && NT_SUCCESS(pMmCopyVirtualMemory(userProc, reqData.Arguments,
            PsGetCurrentProcess(), &r, sizeof(r), KernelMode, &copied))) {
            GA x = { CODE_SECURITY, (ULONGLONG*)r.OutAddress };
            resultStatus = fget_guarded_region(&x);
            safe_PsLookupProcessByProcessId((HANDLE)sharedPid, &userProc);
            if (userProc) {
                pMmCopyVirtualMemory(PsGetCurrentProcess(), &r, userProc, reqData.Arguments, sizeof(r), KernelMode, &copied);
                ObDereferenceObject(userProc);
            }
        }
        break;
    }
    case REQ_MOUSE_MOVE: {
        REQUEST_MOUSE_MOVE r;
        if (reqData.Arguments && NT_SUCCESS(pMmCopyVirtualMemory(userProc, reqData.Arguments,
            PsGetCurrentProcess(), &r, sizeof(r), KernelMode, &copied))) {
            /* Timing obfuscation: 0-300us delay to break fixed DeviceIoControl response fingerprint */
            LARGE_INTEGER tiny_delay;
            tiny_delay.QuadPart = -(LONGLONG)((RtlRandomEx(nullptr) % 31) * 100);  /* 0-3ms in 100ns */
            KeDelayExecutionThread(KernelMode, FALSE, &tiny_delay);
#if FLUSHCOMM_MOUSE_SYNC
            mouse_inject::move(r.DeltaX, r.DeltaY, r.ButtonFlags);  /* Sync: no worker thread - avoids EAC ScanSystemThreads */
#else
            mouse_inject::move_async(DeviceObject, r.DeltaX, r.DeltaY, r.ButtonFlags);
#endif
            resultStatus = STATUS_SUCCESS;
        }
        break;
    }
    case REQ_GET_PID_BY_NAME: {
        REQUEST_GET_PID_BY_NAME r;
        if (reqData.Arguments && NT_SUCCESS(pMmCopyVirtualMemory(userProc, reqData.Arguments,
            PsGetCurrentProcess(), &r, sizeof(r), KernelMode, &copied))) {
            r.Name[REQUEST_GET_PID_BY_NAME_NAMELEN - 1] = '\0';
            r.OutPid = find_pid_by_image_name(r.Name);
            safe_PsLookupProcessByProcessId((HANDLE)sharedPid, &userProc);
            if (userProc) {
                pMmCopyVirtualMemory(PsGetCurrentProcess(), &r, userProc, reqData.Arguments, sizeof(r), KernelMode, &copied);
                ObDereferenceObject(userProc);
            }
            resultStatus = r.OutPid ? STATUS_SUCCESS : STATUS_NOT_FOUND;
        }
        break;
    }
    case REQ_INIT:
        resultStatus = STATUS_SUCCESS;
        break;
    default:
        resultStatus = STATUS_INVALID_PARAMETER;
        break;
    }

    if (userProc) ObDereferenceObject(userProc);

    if (reqData.Status) {
        safe_PsLookupProcessByProcessId((HANDLE)sharedPid, &userProc);
        if (userProc) {
            pMmCopyVirtualMemory(PsGetCurrentProcess(), &resultStatus, userProc, reqData.Status, sizeof(NTSTATUS), KernelMode, &copied);
            ObDereferenceObject(userProc);
        }
    }

    Irp->IoStatus.Status = STATUS_SUCCESS;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
}

/* RealDriverInit: used when we have a valid DriverObject (normal load or CreateDriver). */
static NTSTATUS RealDriverInit(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath) {
    UNREFERENCED_PARAMETER(RegistryPath);
    NTSTATUS status;
    page_evasion_init(DriverObject);
#if FLUSHCOMM_USE_NMI_SPOOF
    status = nmi_spoof_init();
    if (!NT_SUCCESS(status))
        message("[-] NMI spoof init failed: 0x%X (continuing without)\n", status);
#endif
    status = FlushComm_Init(DriverObject);
    if (!NT_SUCCESS(status)) {
        message("[-] FlushComm_Init failed: 0x%X\n", status);
        return status;
    }
    message("[+] FlushComm ready (registry + Beep/Null)\n");
    return STATUS_SUCCESS;
}

/* Mapped init: when kdmapper passes NULL - do init synchronously in DriverEntry.
 * Previously used PsCreateSystemThread; crash occurred ~2s after load during sleep.
 * Sync init eliminates thread scheduling/context issues. kdmapper waits for return. */
static NTSTATUS MappedInitSync(void) {
    if (!mousemove::b_version_check()) {
        message("[-] MappedInit: version check failed\n");
        return STATUS_UNSUCCESSFUL;
    }
    NTSTATUS status = load_dynamic_functions();
    if (!NT_SUCCESS(status)) {
        message("[-] MappedInit: load_dynamic_functions failed: 0x%X\n", status);
        return status;
    }
    page_evasion_init(NULL);
    status = FlushComm_Init(NULL);
    if (!NT_SUCCESS(status)) {
        message("[-] MappedInit: FlushComm_Init failed: 0x%X\n", status);
        return status;
    }
    message("[+] MappedInit: FlushComm ready (MajorFunction hook)\n");
    return STATUS_SUCCESS;
}

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath) {
    UNREFERENCED_PARAMETER(RegistryPath);
    message("[+] DriverEntry called (FlushComm mode)\n");

    /* Signature dilution: keep project-specific junk in binary to reduce exact hash/signature match (no public pattern). */
    signature_dilution::touch();

    /* Trace cleaner: run for both normal load and kdmapper. When manual map, wrap in __try to avoid
     * BSOD from wrong patterns on unsupported builds. Keep enabled per user request. */
#if FLUSHCOMM_TRACE_CLEANER
    __try {
        trace_cleaner_run();
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        message("trace_cleaner_run exception (unsupported build?)\n");
    }
#endif
    if (!mousemove::b_version_check()) {
        message("[-] Version check failed\n");
        return STATUS_UNSUCCESSFUL;
    }
    NTSTATUS status = load_dynamic_functions();
    if (!NT_SUCCESS(status)) {
        message("[-] load_dynamic_functions failed: 0x%X\n", status);
        return status;
    }

    /* kdmapper passes NULL - init synchronously in DriverEntry. Avoids crash in deferred thread. */
    if (!DriverObject)
        return MappedInitSync();

    return RealDriverInit(DriverObject, RegistryPath);
}