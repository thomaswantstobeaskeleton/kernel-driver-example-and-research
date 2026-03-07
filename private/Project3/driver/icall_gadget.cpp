#include "../flush_comm_config.h"
#if FLUSHCOMM_USE_ICALL_GADGET

#include "icall_gadget.hpp"
#include "defines.h"
#include "page_evasion.hpp"
#include "routine_obfuscate.h"

static PVOID g_gadget_addr = nullptr;
static PVOID g_stub_addr = nullptr;
static HANDLE g_stub_section = nullptr;

/* x64 stub: rcx=func, rdx=arg1, r8=arg2. We need func(arg1,arg2) via gadget.
 * mov rax, rcx; mov rcx, rdx; mov rdx, r8; jmp [rip+0]; [gadget_addr] */
static const UCHAR g_stub_template[] = {
    0x48, 0x89, 0xC8,             /* mov rax, rcx   ; rax = func */
    0x48, 0x89, 0xD1,             /* mov rcx, rdx   ; rcx = arg1 */
    0x4C, 0x89, 0xC2,             /* mov rdx, r8   ; rdx = arg2 */
    0xFF, 0x25, 0x00, 0x00, 0x00, 0x00,  /* jmp [rip+0] */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00  /* gadget addr (PATCH) */
};
#define ICALL_STUB_SIZE sizeof(g_stub_template)
#define ICALL_GADGET_PATCH_OFFSET 14

/* Pattern for "call rax" (FF D0) in ntoskrnl .text */
static bool scan_for_call_rax(PUCHAR base, DWORD size, PVOID* out) {
    for (DWORD i = 0; i < size - 2; i++) {
        if (base[i] == 0xFF && base[i + 1] == 0xD0) {
            *out = base + i;
            return true;
        }
    }
    return false;
}

PVOID icall_gadget_find(void) {
    PDRIVER_OBJECT nt = nullptr;
    WCHAR ntNameBuf[20];
    routine_obf_decode_to_wide(OBF_NtoskrnlExe, sizeof(OBF_NtoskrnlExe), ntNameBuf, 20);
    UNICODE_STRING name;
    RtlInitUnicodeString(&name, ntNameBuf);
    if (!NT_SUCCESS(ObReferenceObjectByName(&name, OBJ_CASE_INSENSITIVE, nullptr, 0,
            *globals::IoDriverObjectType, KernelMode, nullptr, (PVOID*)&nt)))
        return nullptr;
    PUCHAR base = (PUCHAR)nt->DriverStart;
    ObDereferenceObject(nt);
    PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)base;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return nullptr;
    PIMAGE_NT_HEADERS pe = (PIMAGE_NT_HEADERS)(base + dos->e_lfanew);
    PIMAGE_SECTION_HEADER sec = IMAGE_FIRST_SECTION(pe);
    for (WORD i = 0; i < pe->FileHeader.NumberOfSections; i++, sec++) {
        if (memcmp(sec->Name, ".text", 5) == 0) {
            PVOID found = nullptr;
            if (scan_for_call_rax(base + sec->VirtualAddress, sec->Misc.VirtualSize, &found))
                return found;
            break;
        }
    }
    return nullptr;
}

/* Allocate executable memory via section. Map into System process so it persists after kdmapper exits. */
static PVOID alloc_executable_stub(SIZE_T size) {
    LARGE_INTEGER secSize;
    secSize.QuadPart = size;
    OBJECT_ATTRIBUTES oa;
    InitializeObjectAttributes(&oa, nullptr, OBJ_KERNEL_HANDLE, nullptr, nullptr);
    HANDLE hSection = nullptr;
    NTSTATUS status = ZwCreateSection(&hSection, SECTION_MAP_READ | SECTION_MAP_WRITE | SECTION_MAP_EXECUTE,
        &oa, &secSize, PAGE_EXECUTE_READWRITE, SEC_COMMIT, nullptr);
    if (!NT_SUCCESS(status) || !hSection)
        return nullptr;
    PVOID baseAddr = nullptr;
    SIZE_T viewSize = size;
    /* Map into System process (PID 4) - persists after kdmapper exits */
    PEPROCESS sysProc = nullptr;
    if (NT_SUCCESS(safe_PsLookupProcessByProcessId((HANDLE)(ULONG_PTR)4, &sysProc)) && sysProc) {
        HANDLE hProc = nullptr;
        constexpr ACCESS_MASK vmAccess = 0x0008u | 0x0010u | 0x0020u;  /* VM_OPERATION|VM_READ|VM_WRITE */
        if (NT_SUCCESS(ObOpenObjectByPointer(sysProc, OBJ_KERNEL_HANDLE, nullptr, vmAccess,
                *PsProcessType, KernelMode, &hProc)) && hProc) {
            status = ZwMapViewOfSection(hSection, hProc, &baseAddr, 0, 0, nullptr,
                &viewSize, ViewUnmap, 0, PAGE_EXECUTE_READWRITE);
            ZwClose(hProc);
        }
        ObDereferenceObject(sysProc);
    }
    if (!NT_SUCCESS(status) || !baseAddr) {
        status = ZwMapViewOfSection(hSection, ZwCurrentProcess(), &baseAddr, 0, 0, nullptr,
            &viewSize, ViewUnmap, 0, PAGE_EXECUTE_READWRITE);
    }
    if (!NT_SUCCESS(status) || !baseAddr) {
        ZwClose(hSection);
        return nullptr;
    }
    g_stub_section = hSection;
    return baseAddr;
}

bool icall_gadget_init(void) {
    if (g_stub_addr) return true;
    g_gadget_addr = icall_gadget_find();
    if (!g_gadget_addr) return false;

    g_stub_addr = alloc_executable_stub(ICALL_STUB_SIZE);
    if (!g_stub_addr) return false;

    RtlCopyMemory(g_stub_addr, g_stub_template, ICALL_STUB_SIZE);
    *(PVOID*)((PUCHAR)g_stub_addr + ICALL_GADGET_PATCH_OFFSET) = g_gadget_addr;
    return true;
}

typedef NTSTATUS(*icall_stub_t)(icall_func2_t func, PVOID arg1, PVOID arg2);

NTSTATUS icall_invoke_2(icall_func2_t func, PVOID arg1, PVOID arg2) {
    if (!func) return STATUS_INVALID_PARAMETER;
    if (g_stub_addr && g_gadget_addr) {
        icall_stub_t stub = (icall_stub_t)g_stub_addr;
        return stub(func, arg1, arg2);
    }
    return func(arg1, arg2);
}

#endif /* FLUSHCOMM_USE_ICALL_GADGET */
