#pragma once
/* Codecave: Execute handler from legitimate driver's executable space to reduce RIP-based detection.
 * RIP spoofing: ensure IOCTL handlers execute from within signed driver .text (not pool/shellcode).
 * When FLUSHCOMM_DEBUG, verify callers are in expected range. */
#include <ntifs.h>
#include <intrin.h>

/* Returns true if _ReturnAddress() is within [base, base+size) - used for RIP verification */
inline bool is_rip_in_range(PVOID base, SIZE_T size) {
    uintptr_t rip = (uintptr_t)_ReturnAddress();
    uintptr_t b = (uintptr_t)base;
    return (rip >= b && rip < b + size);
}

#define MIN_CAVE_SIZE 64   /* Minimum bytes for PING shellcode */

/* Find executable codecave in driver image. Returns address and size, or 0 if not found. */
PVOID FindCodecave(PVOID ImageBase, SIZE_T* OutSize);

/* Install PING shellcode at cave address. Patch with IoCompleteRequest and Magic. Returns true if installed. */
bool InstallPingShellcode(PVOID CaveAddr, SIZE_T CaveSize, PVOID IoCompleteRequestAddr, ULONG64 Magic);

/* LargePageDrivers approach: write to .data section (writable when driver on 2MB large page).
   Requires HKLM\...\Memory Management\LargePageDrivers = beep.sys and reboot. */
bool IsDriverInLargePageList(PCUNICODE_STRING DriverFileName);
PVOID FindDataSection(PVOID ImageBase, SIZE_T* OutSize);
bool InstallPingShellcodeToData(PVOID DataAddr, SIZE_T DataSize, PVOID IoCompleteRequestAddr, ULONG64 Magic);

/* Try to install codecave in driver; set g_ping_codecave on success. Call before hooking. */
bool TrySetupCodecave(PDRIVER_OBJECT pDrv, PCWSTR driverBaseName);
