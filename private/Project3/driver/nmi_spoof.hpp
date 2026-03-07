#pragma once
/* HalPreprocessNmi hook for NMI stack walking evasion.
 * Spoofs interrupted RIP/RSP and CurrentThread as idle when NMI hits.
 * Based on lxkast/frame, Trakimas - see NMI_STACK_WALKING_RESEARCH.md.
 * Enable with FLUSHCOMM_USE_NMI_SPOOF 1 in flush_comm_config.h */

#include <ntifs.h>
#include "../flush_comm_config.h"

#if FLUSHCOMM_USE_NMI_SPOOF

#pragma pack(push, 1)
typedef struct _MACHINE_FRAME_NMI {
    ULONGLONG Rip;
    USHORT SegCs;
    USHORT Fill1[3];
    ULONG EFlags;
    ULONG Fill2;
    ULONGLONG Rsp;
    USHORT SegSs;
    USHORT Fill3[3];
} MACHINE_FRAME_NMI, *PMACHINE_FRAME_NMI;

typedef struct _KNMI_HANDLER_CALLBACK {
    struct _KNMI_HANDLER_CALLBACK* Next;
    BOOLEAN(*Callback)(PVOID Context, BOOLEAN Handled);
    PVOID Context;
    PVOID Handle;
} KNMI_HANDLER_CALLBACK, *PKNMI_HANDLER_CALLBACK;

typedef struct _KTSS64_NMI {
    ULONG Reserved0;
    ULONGLONG Rsp0;
    ULONGLONG Rsp1;
    ULONGLONG Rsp2;
    ULONGLONG Ist[8];
    ULONGLONG Reserved1;
    USHORT Reserved2;
    USHORT IoMapBase;
} KTSS64_NMI, *PKTSS64_NMI;

typedef struct _KPRCB_NMI {
    ULONG MxCsr;
    UCHAR LegacyNumber;
    UCHAR ReservedMustBeZero;
    UCHAR InterruptRequest;
    UCHAR IdleHalt;
    PVOID CurrentThread;
    PVOID NextThread;
    PVOID IdleThread;
} KPRCB_NMI, *PKPRCB_NMI;

typedef VOID(*HalPreprocessNmi_t)(ULONG arg1);
#define HAL_PRIVATE_DISPATCH_HalPreprocessNmi_OFFSET 0x3e8
#pragma pack(pop)

/* Initialize NMI spoofing. Call early in DriverEntry, before FlushComm.
 * Returns STATUS_SUCCESS if hook installed, error otherwise. */
NTSTATUS nmi_spoof_init(void);

/* Restore original HalPreprocessNmi. Call on unload if applicable. */
void nmi_spoof_unhook(void);

#endif /* FLUSHCOMM_USE_NMI_SPOOF */
