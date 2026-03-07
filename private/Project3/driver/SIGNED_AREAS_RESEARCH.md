# Signed-Areas-Only: Anti-Detection Research

## Goal

Use only **signed, legitimate driver memory** for execution to reduce RIP-based and stack-walk detection by kernel anti-cheats (EAC, BattlEye, etc.).

---

## Detection Vectors (What ACs Check)

| Vector | Risk | Mitigation |
|--------|------|------------|
| **RIP in valid module** | High | Execute PING from Beep's .data via codecave |
| **PsLoadedModuleList** | High | Manual-mapped driver not listed; avoid execution from it where possible |
| **Stack trace** | Medium | Call stack shows caller; PING codecave is self-contained (no call to our driver) |
| **Page protection changes** | Medium | MDL + MmProtectMdlSystemAddress on .text = detectable; prefer LargePageDrivers |
| **Pool allocations** | Medium | Avoid RWX pool; section-based shared memory instead of MmCopyVirtualMemory |

---

## Implemented Strategy

### 1. Codecave for PING (Signed Execution)

- **Path**: When usermode sends PING (liveness check), handler runs from **Beep.sys** (signed) instead of mapped driver.
- **Requirement**: `beep.sys` in `HKLM\SYSTEM\CurrentControlSet\Control\Session Manager\Memory Management\LargePageDrivers` + reboot.
- **Why**: On 2MB large pages, .data shares page with .text → becomes RW; we write minimal shellcode, no MDL or protection tweaks.

### 2. FLUSHCOMM_SIGNED_CODECAVE_ONLY=1

When set:
- **Use only** LargePageDrivers .data for codecave.
- **Skip** MDL-based codecave (writes to .text, changes protection via MmProtectMdlSystemAddress → more detectable).
- If LargePageDrivers not configured: no codecave; PING runs inline in mapped driver (fallback).

### 3. Section-Based Shared Memory (No MmCopyVirtualMemory)

- Driver creates named section; usermode maps it.
- Read/write uses direct access to mapped pages; no `MmCopyVirtualMemory` (less suspicious, no cross-process copy).

### 4. FlushFileBuffers + Direct Syscall

- `FlushFileBuffers()` triggers `IRP_MJ_FLUSH_BUFFERS` instead of `DeviceIoControl`.
- Direct `NtDeviceIoControlFile` syscall bypasses ntdll hooks for PING.

---

## What Stays in Mapped Driver (Limitation)

| Component | Location | Why |
|-----------|----------|-----|
| **FlushComm_HookHandler** | Mapped driver | IRP dispatch entry point; I/O manager calls our hook |
| **FlushComm_ProcessSharedBuffer** | Mapped driver | Full request handling (frw, fba, CR3, mouse) – too large for codecave |
| **FlushComm_FlushHandler** | Mapped driver | Same as above |
| **PING (when codecave installed)** | **Beep .data** | Minimal, self-contained; RIP in signed module ✓ |

**Bottom line**: Only PING can run from signed space. Full request processing must run in mapped driver; code caves are too small for that logic.

---

## Setup for Signed PING

1. Open Registry → `HKLM\SYSTEM\CurrentControlSet\Control\Session Manager\Memory Management`
2. Create `LargePageDrivers` (REG_MULTI_SZ) if missing
3. Add `beep.sys` (or `null.sys` if using Null driver)
4. Reboot

---

## Compatibility

- Works with: `FLUSHCOMM_USE_SECTION`, `FLUSHCOMM_USE_FLUSH_BUFFERS`, `FLUSHCOMM_USE_DIRECT_SYSCALL`
- Section path, flush path, and direct syscall are independent of codecave
- When `FLUSHCOMM_SIGNED_CODECAVE_ONLY=0`: falls back to MDL codecave if LargePageDrivers not set

---

## References

- [VollRagm/lpmapper](https://github.com/VollRagm/lpmapper) – LargePageDrivers abuse
- [Abusing LargePageDrivers](https://vollragm.github.io/posts/abusing-large-page-drivers/)
- [rogerxiii/kernel-codecave-poc](https://github.com/rogerxiii/kernel-codecave-poc)
- UC: [lpmapper - Execute shellcode in drivers .data section](https://www.unknowncheats.me/forum/anti-cheat-bypass/495784-lpmapper-execute-shellcode-drivers-data-section.html)
