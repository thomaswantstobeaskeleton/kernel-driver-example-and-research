#pragma once
/* Single source of truth - driver and usermode MUST use same values.
 * No public literals (MdmTrace, WdfCtl, etc.): names derived from FLUSHCOMM_MAGIC at build/runtime.
 * Registry/section suffix: derived at runtime from (FLUSHCOMM_MAGIC>>12)&0xFFFFFF (no literal in binary).
 * Override file is legacy; path building does not use FLUSHCOMM_OBFUSCATION_SUFFIX. */
#include "flush_comm_obfuscation_override.h"
#define FLUSHCOMM_REG_SUFFIX     FLUSHCOMM_OBFUSCATION_SUFFIX
/* Registry path and section name are built at runtime from FLUSHCOMM_SECTION_SEED and suffix
 * so no "MdmTrace" / "WdfCtl" literals appear in binary. Format: SOFTWARE\\%06X\\suffix, Global\\%06X suffix */
#define FLUSHCOMM_SECTION_SEED   ((ULONG)((FLUSHCOMM_MAGIC >> 8) & 0xFFFFFFu))

/* Magic: XOR with non-zero value per build to reduce signature scans.
 * Auto-generated at build time via Directory.Build.targets (flush_comm_magic_xor_auto.h).
 * Override: /DFLUSHCOMM_MAGIC_XOR=0x12345678 or set FlushCommDisableMagicXorAuto=true to skip auto. */
#define FLUSHCOMM_MAGIC_BASE     0x6C3E9A2F1B847D50ull
#if __has_include("flush_comm_magic_xor_auto.h")
#include "flush_comm_magic_xor_auto.h"
#endif
#ifndef FLUSHCOMM_MAGIC_XOR
#define FLUSHCOMM_MAGIC_XOR      0x9D3C9E1B
#endif
#define FLUSHCOMM_MAGIC          (FLUSHCOMM_MAGIC_BASE ^ FLUSHCOMM_MAGIC_XOR)
/* RW/BA/GA security field: derived from MAGIC (no public literal 0x4E8A2C91 in code) */
#define FLUSHCOMM_CODE_SECURITY  ((ULONG)((FLUSHCOMM_MAGIC ^ (FLUSHCOMM_MAGIC >> 32)) & 0xFFFFFFFFu))

/* Obfuscation base key derived from MAGIC - varies per FLUSHCOMM_MAGIC_XOR so no single literal (e.g. 0x5A/0x9D) becomes a project-wide signature. Used by api_resolve, trace_cleaner, driver path encode. */
#define FLUSHCOMM_OBF_BASE  ((unsigned char)((FLUSHCOMM_MAGIC >> 8) & 0xFF))

#define FLUSHCOMM_IOCTL_FUNC     0x7A4
#define FLUSHCOMM_IOCTL_PING     0x7A3

/* Anti-detection: use FlushFileBuffers (IRP_MJ_FLUSH_BUFFERS) for all requests - no DeviceIoControl/IOCTL.
 * When 1: handshake and all requests use FlushFileBuffers only; driver does NOT install IRP_MJ_DEVICE_CONTROL.
 * Eliminates IOCTL as a detection vector. Keep 1 to avoid any IOCTL usage. */
#define FLUSHCOMM_USE_FLUSH_BUFFERS  1

/* When 0: handshake uses FlushFileBuffers+REQ_INIT (no IOCTL). When 1: legacy IOCTL PING (not recommended). */
#define FLUSHCOMM_USE_IOCTL_PING  0

/* Throttling: min ms between send_request calls. Set 0 to disable.
 * Light throttle (1ms + jitter) for anti-detection without killing menu FPS (single throttle in send_request only). */
#ifndef FLUSHCOMM_THROTTLE_MS
#define FLUSHCOMM_THROTTLE_MS    1
#endif

/* Jitter: random 0..N ms added to throttle. Set 0 to disable.
 * 1ms jitter = 1-2ms gap per op; keeps FPS high while varying timing per build. */
#ifndef FLUSHCOMM_JITTER_MS
#define FLUSHCOMM_JITTER_MS      1
#endif

/* Use NtDeviceIoControlFile direct syscall instead of DeviceIoControl (bypasses ntdll hooks). */
#define FLUSHCOMM_USE_DIRECT_SYSCALL  1

/* ALPC fallback: when 1, driver creates an ALPC port (signal-only, section for data) and usermode
 * tries ALPC connect if FlushFileBuffers handshake fails. Unique design - see ALPC_UNIQUE_DESIGN.md.
 * Port name derived from FLUSHCOMM_MAGIC so it is not a literal spread online. Default 0 until tested. */
#ifndef FLUSHCOMM_USE_ALPC_FALLBACK
#define FLUSHCOMM_USE_ALPC_FALLBACK  0
#endif

/* Use section object for shared memory - no MmCopyVirtualMemory. Both sides map same pages. */
#define FLUSHCOMM_USE_SECTION  1
#define FLUSHCOMM_SECTION_SIZE 512

/* File-backed section: no named object in \\BaseNamedObjects\\Global - uses temp file instead.
 * Less common than CreateFileMappingW with name; path derived from MAGIC (unique per build).
 * When 1: try file-backed first; fall back to named section if file path fails. */
#ifndef FLUSHCOMM_USE_FILEBACKED_SECTION
#define FLUSHCOMM_USE_FILEBACKED_SECTION  1
#endif
/* Section layout: derived from named sizes (no public literal 88/80 in code). [0:magic][magic:args][args:status][status:data] */
#define FLUSHCOMM_HEADER_MAGIC_SZ  8
#define FLUSHCOMM_HEADER_ARGS_SZ   72
#define FLUSHCOMM_HEADER_STATUS_SZ 8
#define FLUSHCOMM_STATUS_OFFSET    (FLUSHCOMM_HEADER_MAGIC_SZ + FLUSHCOMM_HEADER_ARGS_SZ)
#define FLUSHCOMM_DATA_OFFSET      (FLUSHCOMM_STATUS_OFFSET + FLUSHCOMM_HEADER_STATUS_SZ)

/* EAC: reject registry fallback - MmCopyVirtualMemory path is detected (UC 496628).
 * When 1: if section open fails, find_driver returns false (no VirtualAlloc+registry).
 * When 0: allow legacy fallback (uses MmCopyVirtualMemory - higher detection risk). */
#ifndef FLUSHCOMM_REJECT_REGISTRY_FALLBACK
#define FLUSHCOMM_REJECT_REGISTRY_FALLBACK  1
#endif

/* Section open retries: driver may create section slightly after map. Retry with delay. */
#define FLUSHCOMM_SECTION_RETRY_COUNT  10
#define FLUSHCOMM_SECTION_RETRY_DELAY_MS  250
/* Section name is built at runtime: \\BaseNamedObjects\\Global\\%06X<suffix> (no public literals). */

/* Codecave: run PING from Beep's signed .data when available. When 0, PING runs inline (works without LargePageDrivers).
 * When 1: uses LargePageDrivers .data if beep.sys in registry + reboot; else falls back to inline. Research: low
 * detection risk; RIP in signed module reduces NMI/RIP-based detection. Set 0 if driver fails REQ_INIT. */
#ifndef FLUSHCOMM_USE_CODECAVE
#define FLUSHCOMM_USE_CODECAVE  1
#endif

/* Signed-areas only: when 1, codecave uses ONLY LargePageDrivers .data (no MDL write to .text).
 * MDL approach modifies page protection on signed driver - more detectable. Set 1 for max stealth.
 * Requires: beep.sys in LargePageDrivers registry + reboot. When 0, falls back to MDL if .data fails. */
#define FLUSHCOMM_SIGNED_CODECAVE_ONLY  1

/* IOCTL alternatives: WSK (kernel TCP server) and FILE_OBJECT DeviceObject hook.
 * When WSK enabled: driver listens on loopback, usermode connects via Winsock. No DeviceIoControl.
 * When FILE_OBJECT hook enabled: redirect FILE_OBJECT->DeviceObject to fake device; real MajorFunction unchanged.
 * FILE_OBJECT=0 uses direct MajorFunction hook - more reliable with kdmapper when section/handshake fails. */
#define FLUSHCOMM_USE_WSK            0   /* 1 = use WSK TCP server - NOT RECOMMENDED: EAC detects socket/thread patterns */
#define FLUSHCOMM_WSK_PORT           0   /* 0 = use default 41891; non-zero = explicit port */
#define FLUSHCOMM_USE_FILEOBJ_HOOK  1   /* 1 = FILE_OBJECT redirect; 0 = MajorFunction hook */

/* Anti-detection: pool tag rotation at runtime (varies per boot). Requires page_evasion_init
 * before any allocation and using EVASION_POOL_TAG_*_R in driver code.
 * Research: ACs scan for known malicious tags; fixed benign tags (Nls, Envl, etc.) behave like
 * normal drivers. Rotation is evasion behavior and can be heuristically flagged. Default 0. */
#define FLUSHCOMM_POOL_TAG_ROTATE  0

/* Usermode: patch ntdll!EtwEventWrite to no-op (reduces ETW telemetry).
 * WARNING: Some ACs detect ETW patches - default 0. Set 1 only if research shows target AC doesn't check.
 * Include utilities/etw_patch.hpp and call EtwPatch::Init() early in main. */
#define FLUSHCOMM_PATCH_ETW  0

/* Usermode: lazy/dynamic import for driver APIs (CreateFileW, DeviceIoControl, Reg*, etc).
 * Resolve via GetProcAddress at runtime - reduces IAT visibility. Requires refactoring driver.hpp
 * to use LazyImport::* when enabled. Set 1 to enable; include utilities/lazy_import.hpp. */
#define FLUSHCOMM_USE_LAZY_IMPORT  1

/* Anti-detection: use synchronous mouse (do_move) instead of IoQueueWorkItem (move_async).
 * When 1: mouse runs in IRP caller context - no worker thread with mapped code on stack.
 * Slightly more latency per request but avoids EAC ScanSystemThreads detection. */
#define FLUSHCOMM_MOUSE_SYNC  1

/* Trace cleaner: clear MmUnloadedDrivers for vuln drivers (kdmapper). Some ACs detect tampering.
 * Set 0 to disable - exposes vuln driver traces. Default 1. See DETECTION_RISKS_AND_STATUS.md. */
#ifndef FLUSHCOMM_TRACE_CLEANER
#define FLUSHCOMM_TRACE_CLEANER  1
#endif

/* Wdfilter trace cleanup: clear RuntimeDriverList etc. High risk - Wdfilter monitored. Default 0. */
#ifndef FLUSHCOMM_TRACE_CLEANER_WDFILTER
#define FLUSHCOMM_TRACE_CLEANER_WDFILTER  0
#endif

/* NMI stack spoofing: hook HalPreprocessNmi, spoof RIP/RSP as idle thread when NMI hits.
 * Evades anti-cheat NMI stack walking. Requires signature scans (KiNmiCallbackListHead, PoIdle).
 * Set 1 to enable. See NMI_STACK_WALKING_RESEARCH.md. */
#define FLUSHCOMM_USE_NMI_SPOOF  0

/* ICALL-GADGET: redirect frw/fba/etc through ntoskrnl gadget so NMI stack shows signed module.
 * Combines with LargePageDrivers codecave (PING already runs from signed Beep). When 1, hot-path
 * handlers invoke via gadget. Requires per-build gadget discovery. Default 0. */
#ifndef FLUSHCOMM_USE_ICALL_GADGET
#define FLUSHCOMM_USE_ICALL_GADGET  1
#endif

/* PFN zeroing: zero physical page contents of unloaded vuln drivers before clearing MmUnloadedDrivers.
 * Best-effort: pages may already be unmapped; we skip those. Reduces residual data in physical memory.
 * Set 0 to disable if issues occur. */
#ifndef FLUSHCOMM_PFN_ZEROING
#define FLUSHCOMM_PFN_ZEROING  1
#endif
