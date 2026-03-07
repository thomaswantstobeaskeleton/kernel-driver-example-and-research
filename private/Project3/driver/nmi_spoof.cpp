#include "nmi_spoof.hpp"
#include "page_evasion.hpp"
#include "defines.h"
#include "includes.hpp"
#include "utility.hpp"
#include "../flush_comm_config.h"
#include "routine_obfuscate.h"

#if FLUSHCOMM_USE_NMI_SPOOF

#define nmi_dbg(...) DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "[NmiSpoof] " __VA_ARGS__)

/* Resolve RIP-relative address from instruction */
static ULONG_PTR resolve_rip_offset(uintptr_t instruction, ULONG offset_pos, ULONG inst_len) {
    LONG rel = *(PLONG)(instruction + offset_pos);
    return (ULONG_PTR)(instruction + inst_len + rel);
}

/* Per-CPU saved state for restoration */
typedef struct _NMI_CORE_INFO {
    ULONGLONG prev_rip;
    ULONGLONG prev_rsp;
    PVOID prev_current_thread;
    PVOID prev_next_thread;
    UCHAR prev_running;
} NMI_CORE_INFO;

static NMI_CORE_INFO* g_nmi_core_infos = nullptr;
static PKNMI_HANDLER_CALLBACK g_callback_parent = nullptr;
static HalPreprocessNmi_t g_hal_preprocess_nmi_original = nullptr;
static PKNMI_HANDLER_CALLBACK g_nmi_list_head = nullptr;
static PVOID g_PoIdle = nullptr;

/* Restore frame and unlink from NMI callback list. Runs as last NMI callback. */
static BOOLEAN RestoreFrameCallback(PVOID context, BOOLEAN handled) {
    UNREFERENCED_PARAMETER(context);

    if (!g_nmi_core_infos) return handled;

    PKPCR kpcr = KeGetPcr();
    PKPRCB_NMI kprcb = (PKPRCB_NMI)kpcr->CurrentPrcb;
    PKTSS64_NMI tss = (PKTSS64_NMI)kpcr->TssBase;
    if (!tss || tss->Ist[3] == 0) return handled;

    PMACHINE_FRAME_NMI machine_frame = (PMACHINE_FRAME_NMI)(tss->Ist[3] - sizeof(MACHINE_FRAME_NMI));
    ULONG processor_index = KeGetCurrentProcessorNumberEx(nullptr);

    if (processor_index < KeQueryActiveProcessorCount(nullptr)) {
        machine_frame->Rip = g_nmi_core_infos[processor_index].prev_rip;
        machine_frame->Rsp = g_nmi_core_infos[processor_index].prev_rsp;
        kprcb->CurrentThread = g_nmi_core_infos[processor_index].prev_current_thread;
        kprcb->NextThread = g_nmi_core_infos[processor_index].prev_next_thread;
        if (kprcb->IdleThread) {
            *(PUCHAR)((uintptr_t)kprcb->IdleThread + 0x71) = g_nmi_core_infos[processor_index].prev_running;
        }
    }

    if (g_callback_parent)
        g_callback_parent->Next = nullptr;

    return handled;
}

static KNMI_HANDLER_CALLBACK g_restore_callback = {
    nullptr,
    RestoreFrameCallback,
    nullptr,
    nullptr
};

static VOID HalPreprocessNmiHook(ULONG arg1) {
    if (g_hal_preprocess_nmi_original)
        g_hal_preprocess_nmi_original(arg1);

    if (arg1 == 1) return;
    if (!g_nmi_list_head || !g_PoIdle || !g_nmi_core_infos) return;

    /* Validate before traversing */
    if (!MmIsAddressValid(g_nmi_list_head)) return;

    /* Append restore callback to end of NMI list */
    g_callback_parent = nullptr;
    PKNMI_HANDLER_CALLBACK cur = g_nmi_list_head;
    while (cur) {
        g_callback_parent = cur;
        cur = cur->Next;
    }
    if (g_callback_parent)
        g_callback_parent->Next = &g_restore_callback;

    PKPCR kpcr = KeGetPcr();
    PKPRCB_NMI kprcb = (PKPRCB_NMI)kpcr->CurrentPrcb;
    PKTSS64_NMI tss = (PKTSS64_NMI)kpcr->TssBase;
    if (!tss || tss->Ist[3] == 0) return;

    PMACHINE_FRAME_NMI machine_frame = (PMACHINE_FRAME_NMI)(tss->Ist[3] - sizeof(MACHINE_FRAME_NMI));
    ULONG processor_index = KeGetCurrentProcessorNumberEx(nullptr);
    if (processor_index >= KeQueryActiveProcessorCount(nullptr)) return;

    /* Save original state */
    g_nmi_core_infos[processor_index].prev_rip = machine_frame->Rip;
    g_nmi_core_infos[processor_index].prev_rsp = machine_frame->Rsp;
    g_nmi_core_infos[processor_index].prev_current_thread = kprcb->CurrentThread;
    g_nmi_core_infos[processor_index].prev_next_thread = kprcb->NextThread;
    g_nmi_core_infos[processor_index].prev_running = kprcb->IdleThread ?
        *(PUCHAR)((uintptr_t)kprcb->IdleThread + 0x71) : 0;

    /* Spoof as idle thread - RSP 0x38 under InitialStack when in PoIdle (Trakimas) */
    PVOID initStack = *(PVOID*)((uintptr_t)kprcb->IdleThread + 0x28);  /* KTHREAD.InitialStack */
    machine_frame->Rip = (ULONGLONG)g_PoIdle;
    machine_frame->Rsp = initStack ? (ULONGLONG)((PUCHAR)initStack - 0x38) : 0;

    kprcb->CurrentThread = kprcb->IdleThread;
    kprcb->NextThread = nullptr;
    if (kprcb->IdleThread)
        *(PUCHAR)((uintptr_t)kprcb->IdleThread + 0x71) = 1;  /* KTHREAD.Running = true */
}

NTSTATUS nmi_spoof_init(void) {
    /* Resolve HalPrivateDispatchTable - no literal in .rdata */
    PVOID hal_table = get_system_routine_obf(OBF_HalPrivateDispatchTable, sizeof(OBF_HalPrivateDispatchTable));
    if (!hal_table) {
        nmi_dbg("HalPrivateDispatchTable not found\n");
        return STATUS_NOT_FOUND;
    }

    HalPreprocessNmi_t* pHalPreprocessNmi = (HalPreprocessNmi_t*)((uintptr_t)hal_table + HAL_PRIVATE_DISPATCH_HalPreprocessNmi_OFFSET);
    if (!MmIsAddressValid(pHalPreprocessNmi) || !*pHalPreprocessNmi) {
        nmi_dbg("HalPreprocessNmi pointer invalid\n");
        return STATUS_INVALID_ADDRESS;
    }

    /* Signature scan KiNmiCallbackListHead - "48 8B 3D ? ? ? ? 41 8A F4" in KiProcessNMI */
    uintptr_t nt_base = get_kernel_base();
    if (!nt_base) {
        nmi_dbg("get_kernel_base failed\n");
        return STATUS_UNSUCCESSFUL;
    }

    intptr_t nmi_inst = search_pattern((void*)nt_base, ".text", "48 8B 3D ? ? ? ? 41 8A F4");
    if (!nmi_inst) {
        nmi_dbg("KiNmiCallbackListHead signature not found\n");
        return STATUS_NOT_FOUND;
    }
    /* resolve_rip_offset returns address of KiNmiCallbackListHead variable; dereference for first callback */
    ULONG_PTR list_head_addr = resolve_rip_offset((uintptr_t)nmi_inst, 3, 7);
    g_nmi_list_head = MmIsAddressValid((PVOID)list_head_addr) ?
        *(PKNMI_HANDLER_CALLBACK*)list_head_addr : nullptr;

    /* Signature scan PoIdle - "40 55 53 41 56" */
    intptr_t poidle_addr = search_pattern((void*)nt_base, ".text", "40 55 53 41 56");
    if (!poidle_addr) {
        nmi_dbg("PoIdle signature not found\n");
        return STATUS_NOT_FOUND;
    }
    g_PoIdle = (PVOID)poidle_addr;

    /* Allocate per-CPU state */
    ULONG nproc = KeQueryActiveProcessorCount(nullptr);
    g_nmi_core_infos = (NMI_CORE_INFO*)ExAllocatePoolWithTag(NonPagedPool,
        nproc * sizeof(NMI_CORE_INFO), EVASION_POOL_TAG_COPY_R);
    if (!g_nmi_core_infos) {
        nmi_dbg("ExAllocatePool failed\n");
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    RtlZeroMemory(g_nmi_core_infos, nproc * sizeof(NMI_CORE_INFO));

    /* Install hook */
    g_hal_preprocess_nmi_original = *pHalPreprocessNmi;
    InterlockedExchangePointer((PVOID*)pHalPreprocessNmi, HalPreprocessNmiHook);

    nmi_dbg("NMI spoof installed (PoIdle=%p)\n", g_PoIdle);
    return STATUS_SUCCESS;
}

void nmi_spoof_unhook(void) {
    if (!g_hal_preprocess_nmi_original) return;

    PVOID hal_table = get_system_routine_obf(OBF_HalPrivateDispatchTable, sizeof(OBF_HalPrivateDispatchTable));
    if (hal_table) {
        HalPreprocessNmi_t* pHalPreprocessNmi = (HalPreprocessNmi_t*)((uintptr_t)hal_table + HAL_PRIVATE_DISPATCH_HalPreprocessNmi_OFFSET);
        if (MmIsAddressValid(pHalPreprocessNmi))
            InterlockedExchangePointer((PVOID*)pHalPreprocessNmi, g_hal_preprocess_nmi_original);
    }
    g_hal_preprocess_nmi_original = nullptr;

    if (g_nmi_core_infos) {
        ExFreePoolWithTag(g_nmi_core_infos, EVASION_POOL_TAG_COPY_R);
        g_nmi_core_infos = nullptr;
    }
}

#endif /* FLUSHCOMM_USE_NMI_SPOOF */
