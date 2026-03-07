#pragma once
/* ICALL-GADGET: redirect execution through ntoskrnl indirect-call gadget.
 * When NMI hits during frw/fba, return address = gadget (signed module) instead of our driver.
 * Combines with LargePageDrivers codecave (PING runs from signed Beep).
 * Set FLUSHCOMM_USE_ICALL_GADGET 1 in flush_comm_config.h to enable. */

#include "../flush_comm_config.h"

#if FLUSHCOMM_USE_ICALL_GADGET

#include "includes.hpp"

/* Find "call rax" (FF D0) or "call [rax]" (FF 10) gadget in ntoskrnl .text.
 * Returns gadget VA or 0. Per-build patterns - add more as needed. */
PVOID icall_gadget_find(void);

/* Init: find and cache gadget. Call from DriverEntry/FlushComm_Init. */
bool icall_gadget_init(void);

/* Invoke func(rcx, rdx) via gadget - return address on stack = gadget. */
typedef NTSTATUS(*icall_func2_t)(PVOID, PVOID);
NTSTATUS icall_invoke_2(icall_func2_t func, PVOID arg1, PVOID arg2);

#endif /* FLUSHCOMM_USE_ICALL_GADGET */
