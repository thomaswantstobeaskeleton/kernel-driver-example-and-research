#include "codecave.hpp"
#include "defines.h"
#include "includes.hpp"
#include "page_evasion.hpp"
#include "flush_comm.hpp"
#include "../flush_comm_config.h"

/* Minimal PE structures - avoid ntimage.h include order issues in driver build */
#ifndef _PE_CODECAVE_LOCAL_
#define _PE_CODECAVE_LOCAL_

/* Derived from chars so no literal 0x5A4D as single constant in binary. */
#define PE_IMAGE_DOS_SIGNATURE   ((USHORT)((('M' << 8) | 'Z')))
#define PE_IMAGE_NT_SIGNATURE    0x00004550
#define PE_IMAGE_SCN_MEM_EXECUTE 0x20000000

#pragma pack(push, 4)
typedef struct _PE_DOS_HEADER {
    USHORT e_magic;
    USHORT e_cblp;
    USHORT e_cp;
    USHORT e_crlc;
    USHORT e_cparhdr;
    USHORT e_minalloc;
    USHORT e_maxalloc;
    USHORT e_ss;
    USHORT e_sp;
    USHORT e_csum;
    USHORT e_ip;
    USHORT e_cs;
    USHORT e_lfarlc;
    USHORT e_ovno;
    USHORT e_res[4];
    USHORT e_oemid;
    USHORT e_oeminfo;
    USHORT e_res2[10];
    LONG   e_lfanew;
} PE_DOS_HEADER, *PPE_DOS_HEADER;

typedef struct _PE_FILE_HEADER {
    USHORT Machine;
    USHORT NumberOfSections;
    ULONG  TimeDateStamp;
    ULONG  PointerToSymbolTable;
    ULONG  NumberOfSymbols;
    USHORT SizeOfOptionalHeader;
    USHORT Characteristics;
} PE_FILE_HEADER, *PPE_FILE_HEADER;

typedef struct _PE_SECTION_HEADER {
    UCHAR  Name[8];
    union {
        ULONG PhysicalAddress;
        ULONG VirtualSize;
    } Misc;
    ULONG  VirtualAddress;
    ULONG  SizeOfRawData;
    ULONG  PointerToRawData;
    ULONG  PointerToRelocations;
    ULONG  PointerToLinenumbers;
    USHORT NumberOfRelocations;
    USHORT NumberOfLinenumbers;
    ULONG  Characteristics;
} PE_SECTION_HEADER, *PPE_SECTION_HEADER;

/* x64: OptionalHeader is 240 bytes */
#define PE_SIZEOF_OPTIONAL_HEADER64 240
#define PE_FIRST_SECTION(nt) ((PPE_SECTION_HEADER)((PUCHAR)(nt) + sizeof(ULONG) + sizeof(PE_FILE_HEADER) + (nt)->FileHeader.SizeOfOptionalHeader))

typedef struct _PE_NT_HEADERS64 {
    ULONG       Signature;
    PE_FILE_HEADER FileHeader;
    UCHAR       OptionalHeader[PE_SIZEOF_OPTIONAL_HEADER64];
} PE_NT_HEADERS64, *PPE_NT_HEADERS64;
#pragma pack(pop)

#endif

#define message(...) DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "[Codecave] " __VA_ARGS__)

/* PING shellcode x64: write magic to SystemBuffer, set IoStatus, call IoCompleteRequest.
   IRP in rdx. SystemBuffer=+0x18, IoStatus.Status=+0x30, IoStatus.Information=+0x38.
   Uses r8 for magic (volatile) to avoid clobbering buffer addr in rax.
   Last 10 bytes are placeholder for IoCompleteRequest address (patched at runtime). */
static const UCHAR g_ping_shellcode[] = {
    0x48, 0x8B, 0x42, 0x18,             /* mov rax, [rdx+0x18]   ; SystemBuffer */
    0x48, 0x85, 0xC0,                   /* test rax, rax */
    0x74, 0x14,                         /* jz +0x14 (skip) */
    0x49, 0xB8, 0x2D, 0x1E, 0x8C, 0x21, 0x60, 0x23, 0x00, 0x59,  /* mov r8, 0x59002360218c1e2d */
    0x4C, 0x89, 0x00,                   /* mov [rax], r8 */
    0x48, 0xC7, 0x42, 0x38, 0x08, 0x00, 0x00, 0x00,  /* mov qword [rdx+0x38], 8 */
    0x48, 0xC7, 0x42, 0x30, 0x00, 0x00, 0x00, 0x00,  /* mov qword [rdx+0x30], 0 */
    /* skip: */
    0x48, 0x89, 0xD1,                   /* mov rcx, rdx ; Irp */
    0x31, 0xD2,                         /* xor edx, edx ; IO_NO_INCREMENT */
    0x48, 0xB8, 0xEF, 0xBE, 0xAD, 0xDE, 0xEF, 0xBE, 0xAD, 0xDE,  /* mov rax, addr (PATCH) */
    0xFF, 0xD0,                         /* call rax */
    0x31, 0xC0,                         /* xor eax, eax */
    0xC3                                /* ret */
};
#define PING_SHELLCODE_SIZE sizeof(g_ping_shellcode)
#define PING_PATCH_OFFSET 38   /* Offset to the 8-byte IoCompleteRequest addr placeholder */
#define PING_MAGIC_OFFSET 11   /* Offset to the 8-byte magic value in mov r8, imm64 */

PVOID FindCodecave(PVOID ImageBase, SIZE_T* OutSize) {
    if (!ImageBase || !OutSize) return nullptr;
    *OutSize = 0;

    __try {
        PPE_DOS_HEADER dos = (PPE_DOS_HEADER)ImageBase;
        if (dos->e_magic != PE_IMAGE_DOS_SIGNATURE) return nullptr;

        PPE_NT_HEADERS64 nt = (PPE_NT_HEADERS64)((PUCHAR)ImageBase + dos->e_lfanew);
        if (nt->Signature != PE_IMAGE_NT_SIGNATURE) return nullptr;

        PPE_SECTION_HEADER section = PE_FIRST_SECTION(nt);
        for (USHORT i = 0; i < nt->FileHeader.NumberOfSections; i++, section++) {
            if (!(section->Characteristics & PE_IMAGE_SCN_MEM_EXECUTE))
                continue;

            PUCHAR start = (PUCHAR)ImageBase + section->VirtualAddress;
            SIZE_T size = section->Misc.VirtualSize;
            if (size < MIN_CAVE_SIZE) continue;

            /* Scan for run of 0x00 or 0xCC (typical padding) */
            SIZE_T run = 0;
            PUCHAR caveStart = nullptr;
            for (SIZE_T j = 0; j < size; j++) {
                UCHAR b = start[j];
                if (b == 0x00 || b == 0xCC) {
                    if (run == 0) caveStart = start + j;
                    run++;
                } else {
                    if (run >= MIN_CAVE_SIZE) {
                        *OutSize = run;
                        return caveStart;
                    }
                    run = 0;
                }
            }
            if (run >= MIN_CAVE_SIZE) {
                *OutSize = run;
                return caveStart;
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
    return nullptr;
}

/* Use MDL + MmProtectMdlSystemAddress to write to read-only .text. Flow:
   1. IoAllocateMdl describes the cave region
   2. MmProbeAndLockPages locks physical pages (IoModifyAccess = R+W)
   3. MmMapLockedPagesSpecifyCache creates a writable mapping of same physical pages
   4. MmProtectMdlSystemAddress(PAGE_READWRITE) allows write to mapped view
   5. Write shellcode to mapped VA (same physical mem as CaveAddr)
   6. MmProtectMdlSystemAddress(PAGE_EXECUTE_READ) restore
   7. MmUnmapLockedPages, MmUnlockPages, IoFreeMdl */
bool InstallPingShellcode(PVOID CaveAddr, SIZE_T CaveSize, PVOID IoCompleteRequestAddr, ULONG64 Magic) {
    if (!CaveAddr || CaveSize < PING_SHELLCODE_SIZE || !IoCompleteRequestAddr)
        return false;

    PMDL pMdl = IoAllocateMdl(CaveAddr, (ULONG)PING_SHELLCODE_SIZE, FALSE, FALSE, NULL);
    if (!pMdl) return false;

    __try {
        MmProbeAndLockPages(pMdl, KernelMode, IoModifyAccess);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        IoFreeMdl(pMdl);
        return false;
    }

    PVOID mapped = MmMapLockedPagesSpecifyCache(pMdl, KernelMode, MmCached, NULL, FALSE, NormalPagePriority);
    if (!mapped) {
        MmUnlockPages(pMdl);
        IoFreeMdl(pMdl);
        return false;
    }

    NTSTATUS status = MmProtectMdlSystemAddress(pMdl, PAGE_READWRITE);
    if (!NT_SUCCESS(status)) {
        MmUnmapLockedPages(mapped, pMdl);
        MmUnlockPages(pMdl);
        IoFreeMdl(pMdl);
        return false;
    }

    RtlCopyMemory(mapped, g_ping_shellcode, PING_SHELLCODE_SIZE);
    *(PVOID*)((PUCHAR)mapped + PING_PATCH_OFFSET) = IoCompleteRequestAddr;
    *(ULONG64*)((PUCHAR)mapped + PING_MAGIC_OFFSET) = Magic;

    MmProtectMdlSystemAddress(pMdl, PAGE_EXECUTE_READ);
    MmUnmapLockedPages(mapped, pMdl);
    MmUnlockPages(pMdl);
    IoFreeMdl(pMdl);
    return true;
}

/* ---------- LargePageDrivers + .data section approach ----------
   When beep.sys is in LargePageDrivers, it loads on a 2MB page; .data becomes writable.
   Direct write works - no MDL needed. See CODECAVE_RESEARCH.md.
   Path/value from routine_obfuscate.h - no literal in .rdata. */
#include "routine_obfuscate.h"

static bool SectionNameMatches(PCUCHAR Name, const char* Match) {
    for (int i = 0; i < 8; i++) {
        char c = (char)Name[i];
        if (Match[i] == 0) return (c == 0 || c == ' ');
        if (c != Match[i]) return false;
    }
    return true;
}

PVOID FindDataSection(PVOID ImageBase, SIZE_T* OutSize) {
    if (!ImageBase || !OutSize) return nullptr;
    *OutSize = 0;

    __try {
        PPE_DOS_HEADER dos = (PPE_DOS_HEADER)ImageBase;
        if (dos->e_magic != PE_IMAGE_DOS_SIGNATURE) return nullptr;

        PPE_NT_HEADERS64 nt = (PPE_NT_HEADERS64)((PUCHAR)ImageBase + dos->e_lfanew);
        if (nt->Signature != PE_IMAGE_NT_SIGNATURE) return nullptr;

        PPE_SECTION_HEADER section = PE_FIRST_SECTION(nt);
        for (USHORT i = 0; i < nt->FileHeader.NumberOfSections; i++, section++) {
            if (SectionNameMatches(section->Name, ".data") && section->Misc.VirtualSize >= PING_SHELLCODE_SIZE) {
                *OutSize = section->Misc.VirtualSize;
                return (PUCHAR)ImageBase + section->VirtualAddress;
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
    return nullptr;
}

bool IsDriverInLargePageList(PCUNICODE_STRING DriverFileName) {
    HANDLE hKey = nullptr;
    WCHAR pathBuf[96], valBuf[24];
    routine_obf_decode_to_wide(OBF_LPG_RegPath, sizeof(OBF_LPG_RegPath), pathBuf, 96);
    routine_obf_decode_to_wide(OBF_LargePageDrivers, sizeof(OBF_LargePageDrivers), valBuf, 24);
    UNICODE_STRING path, valName;
    RtlInitUnicodeString(&path, pathBuf);
    RtlInitUnicodeString(&valName, valBuf);

    OBJECT_ATTRIBUTES oa;
    InitializeObjectAttributes(&oa, &path, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);

    if (!NT_SUCCESS(ZwOpenKey(&hKey, KEY_READ, &oa)))
        return false;

    ULONG resultLen = 0;
    NTSTATUS status = ZwQueryValueKey(hKey, &valName, KeyValuePartialInformation, NULL, 0, &resultLen);
    if (status != STATUS_BUFFER_TOO_SMALL && status != STATUS_BUFFER_OVERFLOW) {
        ZwClose(hKey);
        return false;
    }

    PKEY_VALUE_PARTIAL_INFORMATION info = (PKEY_VALUE_PARTIAL_INFORMATION)ExAllocatePoolWithTag(PagedPool, resultLen, EVASION_POOL_TAG_REG_R);
    if (!info) {
        ZwClose(hKey);
        return false;
    }

    status = ZwQueryValueKey(hKey, &valName, KeyValuePartialInformation, info, resultLen, &resultLen);
    ZwClose(hKey);
    if (!NT_SUCCESS(status) || info->Type != REG_MULTI_SZ || info->DataLength < 2) {
        ExFreePoolWithTag(info, EVASION_POOL_TAG_REG_R);
        return false;
    }

    PWCH buf = (PWCH)info->Data;
    ULONG lenChars = info->DataLength / sizeof(WCHAR);
    bool found = false;

    for (ULONG i = 0; i < lenChars;) {
        if (buf[i] == 0) { i++; continue; }
        PWCH strStart = &buf[i];
        ULONG j = i;
        while (j < lenChars && buf[j]) j++;
        UNICODE_STRING entry;
        entry.Buffer = strStart;
        entry.Length = entry.MaximumLength = (USHORT)((j - i) * sizeof(WCHAR));
        if (RtlCompareUnicodeString(DriverFileName, &entry, TRUE) == 0) {
            found = true;
            break;
        }
        i = j + 1;
    }

    ExFreePoolWithTag(info, EVASION_POOL_TAG_REG_R);
    return found;
}

bool InstallPingShellcodeToData(PVOID DataAddr, SIZE_T DataSize, PVOID IoCompleteRequestAddr, ULONG64 Magic) {
    if (!DataAddr || DataSize < PING_SHELLCODE_SIZE || !IoCompleteRequestAddr)
        return false;
    __try {
        RtlCopyMemory(DataAddr, g_ping_shellcode, PING_SHELLCODE_SIZE);
        *(PVOID*)((PUCHAR)DataAddr + PING_PATCH_OFFSET) = IoCompleteRequestAddr;
        *(ULONG64*)((PUCHAR)DataAddr + PING_MAGIC_OFFSET) = Magic;
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool TrySetupCodecave(PDRIVER_OBJECT pDrv, PCWSTR driverBaseName) {
    if (!pDrv || !pDrv->DriverSection || !driverBaseName)
        return false;

    if (g_ping_codecave)
        return true;  /* Already set */

    typedef VOID(*IoCompleteRequest_t)(PIRP, CCHAR);
    IoCompleteRequest_t pIoComplete = (IoCompleteRequest_t)get_system_routine_obf(OBF_IoCompleteRequest, sizeof(OBF_IoCompleteRequest));
    if (!pIoComplete) {
        message("IoCompleteRequest not found\n");
        return false;
    }

    PLDR_DATA_TABLE_ENTRY entry = (PLDR_DATA_TABLE_ENTRY)pDrv->DriverSection;
    PVOID imageBase = entry->DllBase;
    if (!imageBase) return false;

    UNICODE_STRING driverFileName;
    RtlInitUnicodeString(&driverFileName, driverBaseName);

    __try {
        /* 1. LargePageDrivers + .data: safe if driver is in list (registry + reboot required) */
        if (IsDriverInLargePageList(&driverFileName)) {
            SIZE_T dataSize = 0;
            PVOID dataAddr = FindDataSection(imageBase, &dataSize);
            if (dataAddr && InstallPingShellcodeToData(dataAddr, dataSize, pIoComplete, DEFAULT_MAGGICCODE)) {
                g_ping_codecave = (NTSTATUS(*)(PDEVICE_OBJECT, PIRP))dataAddr;
                message("Codecave installed (.data, LargePageDrivers)\n");
                return true;
            }
        }

#if !FLUSHCOMM_SIGNED_CODECAVE_ONLY
        /* 2. MDL + executable codecave: writes to .text, changes protection - more detectable.
         * Skipped when FLUSHCOMM_SIGNED_CODECAVE_ONLY=1 (signed areas only). */
        SIZE_T caveSize = 0;
        PVOID caveAddr = FindCodecave(imageBase, &caveSize);
        if (caveAddr && InstallPingShellcode(caveAddr, caveSize, pIoComplete, DEFAULT_MAGGICCODE)) {
            g_ping_codecave = (NTSTATUS(*)(PDEVICE_OBJECT, PIRP))caveAddr;
            message("Codecave installed (MDL cave)\n");
            return true;
        }
#endif
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        message("Codecave setup exception - using inline handler\n");
    }
    return false;
}
