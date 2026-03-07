# Deep Research: Why base=0 and cr3=0 for Fortnite

This document provides a comprehensive analysis of why `find_image()` and `fetch_cr3()` return zero when targeting Fortnite (PID 8160 in your case), based on code flow analysis, EAC/VBS research, and known bypass techniques.

---

## Executive Summary

**Root cause**: Fortnite runs with **EasyAntiCheat (EAC)**, which implements CR3 protection and memory shielding. On Windows 11 with **VBS/HVCI (Memory Integrity)** enabled—common on modern systems—the kernel encrypts `UserDirectoryTableBase` (CR3) in EPROCESS. The driver's current CR3 retrieval methods fail under these conditions. Both base and CR3 resolution depend on succeeding at least once; if CR3 fails, the PEB fallback for base also fails.

---

## 1. Communication Flow (Verified Working)

| Step | Component | Status |
|------|-----------|--------|
| 1 | kdmapper loads driver.sys | ✓ "Driver loading completed" |
| 2 | FlushComm hooks Beep IRP_MJ_DEVICE_CONTROL | ✓ "FlushComm device available" |
| 3 | find_driver() → PING returns FLUSHCOMM_MAGIC | ✓ "Driver Loaded Successfully" |
| 4 | find_process("FortniteClient-Win64-Shipping.exe") | ✓ PID 8160 |
| 5 | send_request(REQ_MAINBASE) / REQ_GET_DIR_BASE | Handler invoked |
| 6 | Driver reads SharedBuffer/SharedPid from registry | ✓ Cheat process |
| 7 | MmCopyVirtualMemory reads request from usermode | ✓ |
| 8 | **fba(8160)** or **get_cr3_cached(8160)** | **Returns 0** |
| 9 | Result copied back to usermode | base=0, cr3=0 |

The failure occurs **inside** `fba()` or `get_cr3_cached()` when operating on Fortnite's PID. The FlushComm path and request/response mechanics are working.

---

## 2. CR3 Retrieval Failure Points

The driver uses `get_cr3_cached(pid)` which calls `get_cr3_internal()`. Three methods are tried:

### Method 1: EPROCESS.UserDirectoryTableBase (i_user_dirbase = 0x388)

```cpp
dtb = *(PULONGLONG)((PUCHAR)target_process + globals::offsets::i_user_dirbase);
if ((dtb >> 0x38) == 0x40) dtb = 0;  // Encrypted - discard
```

**Why it fails for Fortnite:**

- With **VBS/HVCI** enabled, Windows encrypts the DTB. The high nibble `0x40` indicates encrypted value.
- EAC-protected processes often run under additional isolation.
- Even without encryption, offset **0x388** is for Windows 11 22H2/23H2 (build 22631). If the build detection is wrong, the wrong offset is used → garbage or 0.

### Method 2: KeStackAttachProcess + __readcr3()

```cpp
KeStackAttachProcess(target_process, &apc_state);
dtb = __readcr3() & 0xFFFFFFFFFFFFF000ULL;
KeUnstackDetachProcess(&apc_state);
```

**Why it can fail:**

- **EAC hooks context switches** and maintains a whitelist of threads allowed to access protected memory ([0avx, UC]).
- Attaching to Fortnite may be blocked, return an error, or cause `__readcr3()` to yield an invalid/encrypted value.
- Anti-cheat may detect and flag `KeStackAttachProcess` to protected game processes.

### Method 3: MmPfn bruteforce

```cpp
// Requires: pte_base, mm_pfn_database from pattern scans
// Iterate MmGetPhysicalMemoryRanges, match PFN to target EPROCESS
```

**Why it can fail:**

- Pattern scans for `init_pte_base()` and `init_mmpfn_database()` may fail on some builds or with HVCI.
- `_MMPFN` layout and decryption (`((flags | 0xF...) >> 0xd) | 0xFFFF...`) are version-sensitive.
- VBS/HVCI can change kernel memory layout and break assumptions.
- EAC may alter or protect relevant kernel structures.

---

## 3. Base Address Retrieval Failure Points

`fba()` (Get Base Address) flow:

1. **PsLookupProcessByProcessId(8160)** → Get EPROCESS
2. **PsGetProcessSectionBaseAddress(process)** → Primary method
3. If that returns 0: **PEB walk** via `translate_linear` + physical read, which **requires CR3**

### Possible failures

| Point | Cause |
|-------|--------|
| PsLookupProcessByProcessId | EAC can make protected processes harder to resolve; in practice it often still works. |
| PsGetProcessSectionBaseAddress | May return 0 or wrong value for protected/EAC processes. |
| PEB fallback | Depends on `get_cr3_cached()`. If CR3=0, PEB walk fails → base stays 0. |

So if CR3 cannot be obtained, base resolution fails as well.

---

## 4. EAC CR3 Protection (External Research)

From UnknownCheats, GitHub (aylers/EAC-CR3-Fix, paidtoomuch/hv.sol-fortnite), and security writeups:

1. **Encrypted DTB**: With VBS, `UserDirectoryTableBase` is encrypted; direct read is useless.
2. **Context-switch hooks**: EAC restricts which threads can access game memory.
3. **CR3 shuffling**: Some setups change CR3 periodically; rely on up-to-date resolution.
4. **Bypass approach**: Derive CR3 from **process base address** by walking the **physical page tables** (MmPfnDatabase), instead of reading EPROCESS/KPROCESS.

**Relevant implementation**: [aylers/EAC-CR3-Fix](https://github.com/aylers/EAC-CR3-Fix)

- Uses `PsGetProcessSectionBaseAddress` to get the base.
- Uses `dirbase_from_base_address(base)` to find the physical CR3 by scanning page tables that map that base.
- Bypasses EPROCESS.UserDirectoryTableBase and `KeStackAttachProcess`.

---

## 5. Windows Version and Offsets

| Build | Version | i_user_dirbase | i_peb |
|-------|---------|----------------|-------|
| 22631 | Win11 23H2 | 0x388 | 0x550 |
| 26100 | Win11 24H2 | 0x388* | 0x2e0 |

\*24H2 has different EPROCESS layout; current code may need updates for 24H2.

Your system (22631) should use 23H2 offsets. If `get_offsets()` or build detection is wrong, wrong offsets → failed reads.

---

## 6. Verification Checklist

Before implementing fixes, verify:

- [ ] **VBS/HVCI status**: Windows Security → Device security → Core isolation → Memory integrity
- [ ] **EAC state**: Is Fortnite fully in lobby with EAC loaded?
- [ ] **Driver build**: Rebuild driver.sys and reload via kdmapper; ensure no stale copy
- [ ] **Run as Admin**: Registry `HKLM\SOFTWARE\rexcomm` requires admin
- [ ] **Driver logs**: Use DbgView to capture `DbgPrint` from the driver (see below)

---

## 7. Recommended Fixes

### A. Add DTB-from-base-address fallback (primary)

Implement a path similar to aylers/EAC-CR3-Fix:

1. Call `PsGetProcessSectionBaseAddress(process)`.
2. If non-null, use `dirbase_from_base_address(base)` to derive CR3 by walking page tables via MmPfnDatabase.
3. Use this CR3 when EPROCESS/KeStackAttach/MmPfn methods fail.

This avoids encrypted DTB and attaches to the target process.

### B. Driver diagnostics (already added)

In `driver/driver.cpp`, set `#define FLUSHCOMM_DEBUG 1` (line ~21) and rebuild. Run **DbgView** as Administrator, enable **Capture Kernel**, then reproduce the issue. You will see:

- `REQ_MAINBASE pid=...` / `REQ_GET_DIR_BASE pid=...` when requests arrive
- `fba: PsLookupProcessByProcessId failed` or `PsGetProcessSectionBaseAddress=0x...`
- `get_cr3: Method1 DTB encrypted` / `Method1 EPROCESS dtb=0` / `Method2 __readcr3 returned 0` / `Method3 pte_base=0 - skip`
- `fba: image_base still 0 after PEB fallback` if base fails

This pinpoints exactly which path fails. Set back to `0` for normal use (no logging overhead).

### C. Temporarily disable VBS/HVCI (testing only)

For testing:

1. Windows Security → Device security → Core isolation → Memory integrity → Off
2. Reboot
3. Retry; if base/cr3 work, the problem is likely VBS-related.

### D. Test with a non-EAC process

Try a simple target (e.g. `notepad.exe`) to confirm base/CR3 work for unprotected processes. If they do, the issue is EAC/VBS-specific.

---

## 8. File Reference

| Purpose | Path |
|---------|------|
| CR3 logic | `driver/driver.cpp` – `get_cr3_internal`, `get_cr3_cached` |
| Base logic | `driver/driver.cpp` – `fba()` |
| FlushComm handler | `driver/driver.cpp` – `FlushComm_HookHandler`, REQ_MAINBASE, REQ_GET_DIR_BASE |
| EPROCESS offsets | `driver/driver.cpp` – `get_offsets()` |
| Usermode driver API | `utilities/impl/driver.hpp` – `find_image`, `fetch_cr3`, `send_request` |
| CR3 research | `driver/CR3_METHODS_RESEARCH.md` |

---

## 9. Summary

| Issue | Cause | Mitigation |
|-------|--------|------------|
| cr3=0 | Encrypted DTB (VBS), EAC blocking attach, or failed MmPfn | Derive CR3 from base via page-table walk |
| base=0 | PsGetProcessSectionBaseAddress fails or PEB path needs CR3 | Use base-derived CR3 and/or improve base resolution |
| Both 0 | Cascading failure: CR3 fails → PEB fallback fails | Fix CR3 first; base often follows |

Implementing `dirbase_from_base_address` and integrating it as a fallback in `get_cr3_internal` / `get_cr3_cached` is the most impactful change for Fortnite with EAC and VBS enabled.
