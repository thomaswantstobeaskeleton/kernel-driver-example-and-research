#include "flush_comm.hpp"
#include "page_evasion.hpp"
#include "codecave.hpp"
#include "../flush_comm_config.h"
#include "../flush_comm_obfuscate.h"
#include "routine_obfuscate.h"
#include "defines.h"
#include "includes.hpp"
#include <ntstrsafe.h>
#if FLUSHCOMM_USE_FILEOBJ_HOOK
#include "file_obj_hook.hpp"
#endif
#if FLUSHCOMM_USE_WSK
#include "wsk_server.hpp"
#endif
#if FLUSHCOMM_USE_ICALL_GADGET
#include "icall_gadget.hpp"
#endif

#define message(...) DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "[FlushComm] " __VA_ARGS__)

NTSTATUS(*g_ping_codecave)(PDEVICE_OBJECT, PIRP) = nullptr;
IRP_MJ_FLUSH_BUFFERS_HANDLER g_orig_flush_buffers = nullptr;

#if FLUSHCOMM_USE_SECTION
PVOID g_section_view = nullptr;
HANDLE g_section_handle = nullptr;
PEPROCESS g_section_process = nullptr;  /* Process we mapped into - attach before accessing (System never exits) */
#endif

NTSTATUS RegReadQword(PCUNICODE_STRING path, PCUNICODE_STRING valueName, PULONG64 outVal) {
    HANDLE hKey = nullptr;
    OBJECT_ATTRIBUTES oa;
    InitializeObjectAttributes(&oa, (PUNICODE_STRING)path, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);

    NTSTATUS status = ZwOpenKey(&hKey, KEY_READ, &oa);
    if (!NT_SUCCESS(status)) return status;

    ULONG resultLen = 0;
    status = ZwQueryValueKey(hKey, (PUNICODE_STRING)valueName, KeyValuePartialInformation, NULL, 0, &resultLen);
    if (status != STATUS_BUFFER_TOO_SMALL && status != STATUS_BUFFER_OVERFLOW) {
        ZwClose(hKey);
        return status;
    }

    PKEY_VALUE_PARTIAL_INFORMATION info = (PKEY_VALUE_PARTIAL_INFORMATION)ExAllocatePoolWithTag(PagedPool, resultLen, EVASION_POOL_TAG_REG_R);
    if (!info) {
        ZwClose(hKey);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    status = ZwQueryValueKey(hKey, (PUNICODE_STRING)valueName, KeyValuePartialInformation, info, resultLen, &resultLen);
    ZwClose(hKey);
    if (NT_SUCCESS(status) && info->DataLength >= sizeof(ULONG64)) {
        *outVal = *(PULONG64)info->Data;
    }
    ExFreePoolWithTag(info, EVASION_POOL_TAG_REG_R);
    return status;
}

bool RegWriteQword(PCUNICODE_STRING path, PCUNICODE_STRING valueName, PVOID data, ULONG size) {
    HANDLE hKey = nullptr;
    OBJECT_ATTRIBUTES oa;
    InitializeObjectAttributes(&oa, (PUNICODE_STRING)path, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);

    NTSTATUS status = ZwOpenKey(&hKey, KEY_ALL_ACCESS, &oa);
    if (!NT_SUCCESS(status)) return false;

    status = ZwSetValueKey(hKey, (PUNICODE_STRING)valueName, 0, REG_QWORD, data, size);
    ZwClose(hKey);
    return NT_SUCCESS(status);
}

/* Read REG_SZ into buffer (null-terminated, max bufChars) */
static NTSTATUS RegReadString(PCUNICODE_STRING path, PCUNICODE_STRING valueName, WCHAR* buf, ULONG bufChars) {
    HANDLE hKey = nullptr;
    OBJECT_ATTRIBUTES oa;
    InitializeObjectAttributes(&oa, (PUNICODE_STRING)path, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);
    NTSTATUS status = ZwOpenKey(&hKey, KEY_READ, &oa);
    if (!NT_SUCCESS(status)) return status;
    ULONG resultLen = 0;
    status = ZwQueryValueKey(hKey, (PUNICODE_STRING)valueName, KeyValuePartialInformation, NULL, 0, &resultLen);
    if (status != STATUS_BUFFER_TOO_SMALL && status != STATUS_BUFFER_OVERFLOW) {
        ZwClose(hKey);
        return status;
    }
    PKEY_VALUE_PARTIAL_INFORMATION info = (PKEY_VALUE_PARTIAL_INFORMATION)ExAllocatePoolWithTag(PagedPool, resultLen, EVASION_POOL_TAG_REG_R);
    if (!info) {
        ZwClose(hKey);
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    status = ZwQueryValueKey(hKey, (PUNICODE_STRING)valueName, KeyValuePartialInformation, info, resultLen, &resultLen);
    ZwClose(hKey);
    if (!NT_SUCCESS(status)) {
        ExFreePoolWithTag(info, EVASION_POOL_TAG_REG_R);
        return status;
    }
    ULONG dataLen = info->DataLength;
    ULONG copyChars = min(bufChars - 1, dataLen / sizeof(WCHAR));
    if (copyChars > 0)
        RtlCopyMemory(buf, info->Data, copyChars * sizeof(WCHAR));
    buf[copyChars] = L'\0';
    ExFreePoolWithTag(info, EVASION_POOL_TAG_REG_R);
    return STATUS_SUCCESS;
}

static WCHAR g_flushcomm_reg_path_buf[128];
UNICODE_STRING g_flushcomm_reg_path = { 0 };

/* Suffix derived from MAGIC at runtime - no literal in .rdata (avoids signature from generated override).
 * Hex digits built from char constants - no L"0123456789abcdef" in .rdata. */
static WCHAR g_obf_suffix[8];
static void fill_obf_suffix(void) {
    ULONG v = (ULONG)((FLUSHCOMM_MAGIC >> 12) & 0xFFFFFFu);
    for (int i = 5; i >= 0; i--) {
        g_obf_suffix[i] = (WCHAR)((v & 0xF) < 10 ? (L'0' + (v & 0xF)) : (L'a' + (v & 0xF) - 10));
        v >>= 4;
    }
    g_obf_suffix[6] = L'\0';
}

NTSTATUS FlushComm_Init(PDRIVER_OBJECT DriverObject) {
#if FLUSHCOMM_USE_ICALL_GADGET
    if (icall_gadget_init())
        message("ICALL-GADGET initialized (frw/fba via ntoskrnl)\n");
#endif
    fill_obf_suffix();
    /* Registry path and section name from FLUSHCOMM_SECTION_SEED + suffix (no literal suffix in binary) */
    RtlStringCbPrintfW(g_flushcomm_reg_path_buf, sizeof(g_flushcomm_reg_path_buf),
        L"\\Registry\\Machine\\SOFTWARE\\%06X\\%ws", (ULONG)FLUSHCOMM_SECTION_SEED, g_obf_suffix);
    RtlInitUnicodeString(&g_flushcomm_reg_path, g_flushcomm_reg_path_buf);

    OBJECT_ATTRIBUTES oa;
    InitializeObjectAttributes(&oa, &g_flushcomm_reg_path, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);
    HANDLE hKey = nullptr;
    if (NT_SUCCESS(ZwCreateKey(&hKey, KEY_ALL_ACCESS, &oa, 0, NULL, REG_OPTION_NON_VOLATILE, NULL))) {
        ZwClose(hKey);
    }

#if FLUSHCOMM_USE_SECTION
    HANDLE hSection = nullptr;
#if FLUSHCOMM_USE_FILEBACKED_SECTION
    /* File-backed: no named object - usermode creates file first; driver opens and maps same file. */
    WCHAR regPathBuf[80];
    routine_obf_decode_to_wide(OBF_RegPathCurrentVersion, sizeof(OBF_RegPathCurrentVersion), regPathBuf, 80);
    UNICODE_STRING regPath;
    RtlInitUnicodeString(&regPath, regPathBuf);
    WCHAR valNameBuf[16];
    routine_obf_decode_to_wide(OBF_SystemRoot, sizeof(OBF_SystemRoot), valNameBuf, 16);
    UNICODE_STRING valName;
    RtlInitUnicodeString(&valName, valNameBuf);
    WCHAR sysRoot[MAX_PATH] = { 0 };
    if (NT_SUCCESS(RegReadString(&regPath, &valName, sysRoot, MAX_PATH)) && sysRoot[0]) {
        WCHAR filePath[MAX_PATH];
        WCHAR ntPath[MAX_PATH];
        ULONG64 m = FLUSHCOMM_MAGIC & 0xFFFFFFFFFFFFull;
        RtlStringCbPrintfW(filePath, sizeof(filePath), L"%ws\\Temp\\Fx%012llx.tmp", sysRoot, m);
        RtlStringCbPrintfW(ntPath, sizeof(ntPath), L"\\??\\%ws", filePath);
        UNICODE_STRING ntPathU;
        RtlInitUnicodeString(&ntPathU, ntPath);
        OBJECT_ATTRIBUTES fileOa;
        InitializeObjectAttributes(&fileOa, &ntPathU, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);
        HANDLE hFile = nullptr;
        IO_STATUS_BLOCK iosb = { 0 };
        if (NT_SUCCESS(ZwOpenFile(&hFile, FILE_READ_DATA | FILE_WRITE_DATA | FILE_READ_ATTRIBUTES, &fileOa, &iosb, FILE_SHARE_READ, FILE_NON_DIRECTORY_FILE)) && hFile) {
            LARGE_INTEGER secSize;
            secSize.QuadPart = FLUSHCOMM_SECTION_SIZE;
            NTSTATUS secStatus = ZwCreateSection(&hSection, SECTION_MAP_READ | SECTION_MAP_WRITE, NULL, &secSize, PAGE_READWRITE, SEC_COMMIT, hFile);
            ZwClose(hFile);
            if (NT_SUCCESS(secStatus) && hSection) {
                message("File-backed section: %ws\n", filePath);
            } else {
                if (hSection) ZwClose(hSection);
                hSection = nullptr;
            }
        }
    }
#endif
    /* Named section fallback: ZwOpenSection / ZwCreateSection in \\BaseNamedObjects\\Global */
    if (!hSection) {
        LARGE_INTEGER secSize;
        secSize.QuadPart = FLUSHCOMM_SECTION_SIZE;
        WCHAR secNameBuf[64];
        RtlStringCbPrintfW(secNameBuf, sizeof(secNameBuf), L"\\BaseNamedObjects\\Global\\%06X%ws",
            (ULONG)FLUSHCOMM_SECTION_SEED, g_obf_suffix);
        UNICODE_STRING secName;
        RtlInitUnicodeString(&secName, secNameBuf);
        UCHAR secSdBuf[SECURITY_DESCRIPTOR_MIN_LENGTH];
        RtlZeroMemory(secSdBuf, sizeof(secSdBuf));
        PSECURITY_DESCRIPTOR pSec = nullptr;
        if (NT_SUCCESS(RtlCreateSecurityDescriptor((PSECURITY_DESCRIPTOR)secSdBuf, SECURITY_DESCRIPTOR_REVISION))) {
            RtlSetDaclSecurityDescriptor((PSECURITY_DESCRIPTOR)secSdBuf, TRUE, NULL, FALSE);  /* NULL DACL = allow all */
            pSec = (PSECURITY_DESCRIPTOR)secSdBuf;
        }
        OBJECT_ATTRIBUTES secOa;
        InitializeObjectAttributes(&secOa, &secName, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, pSec, NULL);
        NTSTATUS secStatus = ZwOpenSection(&hSection, SECTION_MAP_READ | SECTION_MAP_WRITE, &secOa);
        if (!NT_SUCCESS(secStatus) || !hSection) {
            secStatus = ZwCreateSection(&hSection, SECTION_MAP_READ | SECTION_MAP_WRITE, &secOa, &secSize, PAGE_READWRITE, SEC_COMMIT, NULL);
            if (secStatus == STATUS_OBJECT_NAME_COLLISION && !hSection) {
                secStatus = ZwOpenSection(&hSection, SECTION_MAP_READ | SECTION_MAP_WRITE, &secOa);
            }
        }
        if (!NT_SUCCESS(secStatus) || !hSection) {
            message("Section create/open failed: 0x%X (HVCI/security may block; try Rebuild)\n", secStatus);
        }
    }
    /* Mapping: same for file-backed and named section */
    if (hSection) {
        NTSTATUS secStatus;
        HANDLE hMapProcess = nullptr;
        if (NT_SUCCESS(safe_PsLookupProcessByProcessId((HANDLE)(ULONG_PTR)4, &g_section_process)) && g_section_process) {
            constexpr ACCESS_MASK vmAccess = 0x0008u | 0x0010u | 0x0020u;
            if (NT_SUCCESS(ObOpenObjectByPointer(g_section_process, OBJ_KERNEL_HANDLE, nullptr, vmAccess, *PsProcessType, KernelMode, &hMapProcess)) && hMapProcess) {
                SIZE_T viewSize = FLUSHCOMM_SECTION_SIZE;
                secStatus = ZwMapViewOfSection(hSection, hMapProcess, &g_section_view, 0, 0, NULL, &viewSize, ViewUnmap, 0, PAGE_READWRITE);
                ZwClose(hMapProcess);
                if (NT_SUCCESS(secStatus) && g_section_view) {
                    g_section_handle = hSection;
                }
            }
            if (!g_section_handle) {
                ObDereferenceObject(g_section_process);
                g_section_process = nullptr;
            }
        }
        if (!g_section_handle) {
            SIZE_T viewSize = FLUSHCOMM_SECTION_SIZE;
            secStatus = ZwMapViewOfSection(hSection, ZwCurrentProcess(), &g_section_view, 0, 0, NULL, &viewSize, ViewUnmap, 0, PAGE_READWRITE);
            if (NT_SUCCESS(secStatus) && g_section_view) {
                g_section_handle = hSection;
                g_section_process = PsGetCurrentProcess();
                ObReferenceObject(g_section_process);
            }
        }
        if (!g_section_handle) {
            g_section_handle = hSection;
        }
    }
#endif

#if FLUSHCOMM_USE_WSK
    if (NT_SUCCESS(WskServer_Init(0))) {
        ULONG64 port = WskServer_GetPort();
        WCHAR wskPortBuf[16];
        routine_obf_decode_to_wide(OBF_WskPort, sizeof(OBF_WskPort), wskPortBuf, 16);
        UNICODE_STRING valName;
        RtlInitUnicodeString(&valName, wskPortBuf);
        RegWriteQword(&g_flushcomm_reg_path, &valName, &port, sizeof(port));
    }
#endif

    /* Device paths - decoded at runtime (no Beep/Null/PEAuth literals in .rdata). Key from config - no single literal signature. */
#define _DPK  (FLUSHCOMM_OBF_BASE)
    static const UCHAR dev_path_0[] = { _DPK^'\\',_DPK^'D',_DPK^'e',_DPK^'v',_DPK^'i',_DPK^'c',_DPK^'e',_DPK^'\\',_DPK^'B',_DPK^'e',_DPK^'e',_DPK^'p',0 };
    static const UCHAR dev_path_1[] = { _DPK^'\\',_DPK^'D',_DPK^'e',_DPK^'v',_DPK^'i',_DPK^'c',_DPK^'e',_DPK^'\\',_DPK^'N',_DPK^'u',_DPK^'l',_DPK^'l',0 };
    static const UCHAR dev_path_2[] = { _DPK^'\\',_DPK^'D',_DPK^'e',_DPK^'v',_DPK^'i',_DPK^'c',_DPK^'e',_DPK^'\\',_DPK^'P',_DPK^'E',_DPK^'A',_DPK^'u',_DPK^'t',_DPK^'h',0 };
    static const UCHAR* const dev_path_enc[] = { dev_path_0, dev_path_1, dev_path_2 };
    WCHAR devicePathBuf[32];

    extern NTSTATUS FlushComm_HookHandler(PDEVICE_OBJECT, PIRP);
    extern NTSTATUS FlushComm_FlushHandler(PDEVICE_OBJECT, PIRP);

#if FLUSHCOMM_USE_FILEOBJ_HOOK
    /* FILE_OBJECT hook requires DriverObject. When NULL (kdmapper), use MajorFunction path below. */
    if (DriverObject) {
        FileObjHook_SetOurDriver(DriverObject);
        FileObjHook_SetHandlers(FlushComm_HookHandler, FlushComm_FlushHandler);
#if !FLUSHCOMM_USE_FLUSH_BUFFERS
        DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = FlushComm_HookHandler;
#endif
        DriverObject->MajorFunction[IRP_MJ_FLUSH_BUFFERS] = FlushComm_FlushHandler;
    }
#endif

    for (int i = 0; i < 3; i++) {
        int j = 0;
        while (dev_path_enc[i][j] && j < 31) { devicePathBuf[j] = (WCHAR)(dev_path_enc[i][j] ^ _DPK); j++; }
        devicePathBuf[j] = L'\0';
        UNICODE_STRING devicePath;
        RtlInitUnicodeString(&devicePath, devicePathBuf);

        PFILE_OBJECT fileObj = nullptr;
        PDEVICE_OBJECT pDev = nullptr;
        NTSTATUS status = IoGetDeviceObjectPointer(&devicePath, FILE_READ_DATA, &fileObj, &pDev);
        if (!NT_SUCCESS(status) || !pDev || !fileObj) {
            message("Device %ws not found (0x%X)\n", devicePathBuf, status);
            continue;
        }

        PDRIVER_OBJECT pDrv = pDev->DriverObject;

#if FLUSHCOMM_USE_FILEOBJ_HOOK
        if (DriverObject) {
            status = FileObjHook_Init(pDev, pDrv);
            ObDereferenceObject(fileObj);
            if (!NT_SUCCESS(status)) {
                message("FileObjHook_Init failed: 0x%X\n", status);
                continue;
            }
        } else
#endif
        /* MajorFunction hook: when DriverObject is NULL (kdmapper) or FILEOBJ_HOOK disabled.
         * When FLUSH_BUFFERS only: do not hook IRP_MJ_DEVICE_CONTROL (avoids IOCTL vector entirely). */
        {
#if !FLUSHCOMM_USE_FLUSH_BUFFERS
            PVOID* pMajorFunc = (PVOID*)&pDrv->MajorFunction[IRP_MJ_DEVICE_CONTROL];
            g_orig_device_control = (IRP_MJ_DEVICE_CONTROL_HANDLER)InterlockedExchangePointer(pMajorFunc, FlushComm_HookHandler);
#endif
            PVOID* pFlushFunc = (PVOID*)&pDrv->MajorFunction[IRP_MJ_FLUSH_BUFFERS];
            g_orig_flush_buffers = (IRP_MJ_FLUSH_BUFFERS_HANDLER)InterlockedExchangePointer(pFlushFunc, FlushComm_FlushHandler);

#if FLUSHCOMM_USE_CODECAVE
            static const UCHAR base_0[] = { _DPK^'b',_DPK^'e',_DPK^'e',_DPK^'p',_DPK^'.',_DPK^'s',_DPK^'y',_DPK^'s',0 };
            static const UCHAR base_1[] = { _DPK^'n',_DPK^'u',_DPK^'l',_DPK^'l',_DPK^'.',_DPK^'s',_DPK^'y',_DPK^'s',0 };
            static const UCHAR base_2[] = { _DPK^'p',_DPK^'e',_DPK^'a',_DPK^'u',_DPK^'t',_DPK^'h',_DPK^'.',_DPK^'s',_DPK^'y',_DPK^'s',0 };
            static const UCHAR* const base_enc[] = { base_0, base_1, base_2 };
            WCHAR baseNameBuf[16];
            { int k = 0; while (base_enc[i][k] && k < 15) { baseNameBuf[k] = (WCHAR)(base_enc[i][k] ^ _DPK); k++; } baseNameBuf[k] = L'\0'; }
            TrySetupCodecave(pDrv, baseNameBuf);
#endif
            ObDereferenceObject(fileObj);
        }
        message("Hooked %ws (%s)\n", devicePathBuf, (DriverObject && FLUSHCOMM_USE_FILEOBJ_HOOK) ? "FILE_OBJECT redirect" : "MajorFunction");

        /* Write which device we hooked so usermode opens the same one */
        WCHAR valNameBuf[16];
        obf_decode_str(OBF_HookedDevice, OBF_HookedDevice_LEN, valNameBuf, 16);
        UNICODE_STRING valName;
        RtlInitUnicodeString(&valName, valNameBuf);
        ULONG64 idx = (ULONG64)i;
        RegWriteQword(&g_flushcomm_reg_path, &valName, &idx, sizeof(idx));

        return STATUS_SUCCESS;
    }
#undef _DPK
    message("No suitable driver found for FlushComm\n");
    return STATUS_OBJECT_NAME_NOT_FOUND;
}
