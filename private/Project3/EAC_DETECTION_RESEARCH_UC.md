# EAC Driver / External Cheat Detection – UnknownCheats Research

Summary of how EAC detects drivers and external cheats (from UnknownCheats and related research), and how this project avoids publicly spread implementations.

---

## 1. EAC Detection Vectors (UC Research)

### 1.1 Driver / Mapper

| Vector | Description | UC / Notes |
|--------|-------------|------------|
| **Unsigned drivers** | EAC scans for unsigned drivers; DseFix/KDU-style load can be blocked (e.g. error 30007). | UC 635703, 438804 |
| **MmUnloadedDrivers** | List of unloaded drivers; ACs look for vuln driver names (iqvw64e, capcom, eneio, etc.). Must clear when using manual mapping. | UC 438804, x64DriverCleaner |
| **PiDDB cache** | EAC checks PiDDB; vuln driver entries should be removed. | UC 438804 |
| **Big pool** | EAC can scan big pool allocations; spoofing big pool table entries is sometimes recommended. | UC 438804 |
| **KdMapper / public mappers** | KdMapper has detectable wdfilter/vuln driver traces; SinMapper’s READ_EXECUTE section patches are detected. Prefer custom mapping and full trace cleanup. | UC 614327, 594960 |

### 1.1b MmUnloadedDrivers clearing and Fortnite EAC (UC, up-to-date)

- **UC consensus (thread 438804, Swiftik Jan 2021):** "On EAC, you **must clear** pidbbcache and mmunloadeddrivers." So EAC checks for *uncleared* mapper/vuln driver traces; not clearing = detection/kick. Recommendation on UC is to clear.
- **Tampering (clearing) itself:** No UC thread found stating that **Fortnite EAC** detects the *act* of clearing MmUnloadedDrivers (e.g. integrity/checksum on the list). Project doc notes **EMACLAB** may detect tampering (MmLastUnloadedDriver/checksum); that is a different AC.
- **Conclusion:** For Fortnite EAC, UC guidance is to **clear** MmUnloadedDrivers and PiDDB. No up-to-date UC source says Fortnite EAC detects clearing; tampering caveat applies to other ACs. Keep trace cleaner enabled for EAC/Fortnite unless future reversal shows EAC validating list integrity.

### 1.2 Execution / Threads

| Vector | Description | UC / Notes |
|--------|-------------|------------|
| **Threads in unsigned memory** | Threads running in unmapped/unsigned memory are a detection vector; hiding or unlinking from lists is often needed. | UC 438804 |
| **Call stack / start address** | Call stacks and start addresses should be spoofed; strings removed. | UC 438804 |
| **ScanSystemThreads** | EAC detects socket/listener and worker-thread patterns (e.g. WSK). Prefer sync mouse / single-thread comm. | Project config |

### 1.3 Communication / Usermode

| Vector | Description | UC / Notes |
|--------|-------------|------------|
| **DeviceIoControl / IOCTL** | IOCTL is a known vector; FlushFileBuffers (IRP_MJ_FLUSH_BUFFERS) avoids it. | Project: FLUSHCOMM_USE_FLUSH_BUFFERS |
| **MmCopyVirtualMemory** | EAC traces MmCopyVirtualMemory; shared section avoids it. | UC 496628, project: section path |
| **Guarded region reads** | Can be detected; physical memory read path preferred where applicable. | UC / driver physical path |

### 1.4 CR3 / Dirbase Reading

| Vector | Description | UC / Notes |
|--------|-------------|------------|
| **KeStackAttachProcess + __readcr3** | EAC detects attach and __readcr3 for CR3; high overhead and known vector. | UC 619886: "__readcr3 is detected"; use as last resort only. |
| **Alternatives** | Physical scan (dirbase_from_base), MmPfn bruteforce, EPROCESS UserDirectoryTableBase; bruteforce CR3. | Project: order 1→2→3→4 (least to most detected); Method 4 only when 1–3 fail. |

### 1.5 Process / Module Enumeration

| Vector | Description | UC / Notes |
|--------|-------------|------------|
| **CreateToolhelp32Snapshot** | Heavily hooked/monitored; avoid for stealth. | PROCESS_ENUM_EAC_RESEARCH.md; used last (reliability fallback). |
| **NtQuerySystemInformation** | Often considered stealthier; no Toolhelp32 in call stack. | Project: 1st in find_process_stealth; then NTFS, NtGetNextProcess, Toolhelp32. |
| **Direct syscall** | Bypasses ntdll hooks; instrumentation can still detect. | Project: direct_syscall.hpp; NtGetNextProcess uses it. |

---

## 2. Avoid Public Implementations – What We Use

Anti-detection and obfuscation use **non-documented, non-public** methods where possible:

### 2.1 Obfuscation Keys

- **No single project-wide literal** (e.g. 0x5A from public tutorials, or a fixed 0x9D in every module).
- **FLUSHCOMM_OBF_BASE** in `flush_comm_config.h`: `(FLUSHCOMM_MAGIC >> 8) & 0xFF`. Key varies with `FLUSHCOMM_MAGIC_XOR` per build.
- **api_resolve fallback**: When `flush_comm_config.h` is not in include path (e.g. Frozen Public), obfuscation base is **build-time derived** from `__TIME__` (no literal 0x9D in source).
- **Used in**: api_resolve (APIRES_OBF_KEY), trace_cleaner (TRACE_OBF_KEY), driver device paths/codecave in `flush_comm.cpp` (_DPK).
- **flush_comm_obfuscate.h**: Precomputed arrays stay build-consistent; key there can be switched to a MAGIC-derived value with regenerated arrays if desired.
- **Kdmapper** (`kdm_obfuscate.hpp`): Uses non-public keys (no 0x5A/0x53). When built with this solution, KDM_OBF_KEY = FLUSHCOMM_OBF_BASE (MAGIC-derived). When built with this solution, consider passing a MAGIC-derived key via build so it isn’t a fixed literal.

### 2.2 No Literal API / Module Names in Binary

- **Spoofer** (`spoofer_hooks.hpp`): Resolves `RtlGetVersion` and `NtQuerySystemInformation` via **api_resolve** (obfuscated "ntdll.dll" and function names). No plain "ntdll.dll" / "RtlGetVersion" in .rdata.
- **Process enum, direct syscall, driver usermode**: Use api_resolve for ntdll/kernel32 and proc names (encoded at compile time).
- **Kdmapper lazy_import** (`lazy_import.hpp`): When `KDMAPPER_USE_LAZY_IMPORT` is set, module and proc names ("ntdll.dll", "kernel32.dll", "NtQuerySystemInformation", "CreateFileW", "DeviceIoControl", "VirtualAlloc", "VirtualFree") are **encoded** with KDM_OBF_KEY; decoded at runtime. No literal strings in .rdata.

### 2.3 Naming and Layout

- **Registry/section**: Built from `FLUSHCOMM_SECTION_SEED` + suffix; no "MdmTrace", "WdfCtl" literals. Suffix should be varied per build (e.g. Directory.Build.targets or script from MAGIC).
- **Shared buffer layout**: Project-specific; no literal 88/80; offsets via named constants.
- **CODE_SECURITY**: Derived from `FLUSHCOMM_MAGIC`; no literal 0x4E8A2C91.

### 2.4 Signature / Code Dilution

- **Purpose:** ACs (including EAC) can match drivers by code hash or byte-pattern signatures. Adding project-specific “junk” code and data dilutes the binary so it no longer matches a single known signature; constants and layout vary with `FLUSHCOMM_MAGIC` per build.
- **Implementation:** `signature_dilution.hpp` – MAGIC-derived constants (`_SD_V`, `_SD_W`, `_SD_CNT`), no public literals (no 0x11111111, no fixed 1000-iteration loops, no “JunkFunction” naming). One `touch()` call from DriverEntry keeps dilution in the binary. Not copied from public “jnk”/payson-style implementations (see [payson jnk.cpp](https://raw.githubusercontent.com/paysonism/v2-payson-ioctl-cheat-driver/refs/heads/main/km/Payson%20IOCTL/jnk.cpp)); project uses its own layout and naming.

### 2.5 Trace Cleaner

- **Vuln driver names**: Encoded at compile time with TRACE_OBF_KEY (now FLUSHCOMM_OBF_BASE); decoded at runtime. No plain "iqvw64e.sys" etc. in .rdata.
- **ntoskrnl name**: Decoded at runtime with same key; no "ntoskrnl.exe" literal.
- **MmUnloadedDrivers pattern**: Public pattern (e.g. 0x4C 0x8D 0x05) is still used to locate the list; alternative (e.g. export or different opcode) would reduce public pattern reuse.

### 2.6 Communication and Read/Write

- **FlushFileBuffers only** by default; no DeviceIoControl in hot path.
- **Section** for shared memory (no MmCopyVirtualMemory on that path).
- **Physical path** preferred when CR3 cached (no KeStackAttachProcess in hot path).
- **Direct syscall** for NtDeviceIoControlFile / NtGetNextProcess (bypass ntdll hooks); stub pattern is standard Windows syscall sequence (documented as tradeoff).

### 2.7 Optional Steps Implemented (Non-Detected)

- **Suffix**: No literal in binary. Registry/section suffix derived at runtime from `(FLUSHCOMM_MAGIC >> 12) & 0xFFFFFF` (6 hex chars). Override file no longer used for path building.
- **Kdmapper**: When built with main project, `kdm_obfuscate.hpp` uses **FLUSHCOMM_OBF_BASE** for KDM_OBF_KEY (same MAGIC-derived key as driver).
- **MmUnloadedDrivers scan**: Try **48 8D 0D** (lea rcx) first, then **4C 8D 05** (lea r8) to reduce overlap with public cleaners.

### 2.8 What Remains “Public” or Shared

**Addressed (no literal/public pattern in binary):**
- **Syscall stub**: Stub bytes and get_ssn prologue are **encoded** with STUB_OBF_KEY; decoded at runtime. No literal 4C 8B D1 B8 0F 05 C3 in .rdata.
- **Trace cleaner**: Pattern bytes **encoded** with TRACE_OBF_KEY; decoded at runtime. No literal 48 8D 0D / 4C 8D 05 in .rdata.
- **Kdmapper**: **kdm_api_resolve.hpp** provides GetNtdllModule(), GetNtAddAtomProc(), GetNtAddAtomName() with encoded strings. No literal "ntdll.dll" or "NtAddAtom" in .rdata.
- **Kdmapper lazy_import**: Module/proc names for NtQuerySystemInformation, CreateFileW, DeviceIoControl, VirtualAlloc, VirtualFree are **encoded** with KDM_OBF_KEY; no literal "ntdll.dll" / "NtQuerySystemInformation" etc. in .rdata.
- **api_resolve**: When config is missing, obfuscation base is derived from `__TIME__` (no fixed 0x9D fallback).
- **PE DOS magic (MZ)**: In driver/codecave, 0x5A4D is **derived** as `(('M'<<8)|'Z')` (FLUSHCOMM_PE_MAGIC_MZ / PE_IMAGE_DOS_SIGNATURE) so no single 0x5A4D literal as constant signature.
- **driver/com/driver.hpp**: Legacy alternate client; when `flush_comm_config.h` is included, CODE_SECURITY uses FLUSHCOMM_CODE_SECURITY (MAGIC-derived). Device path is **decoded at runtime** from encoded arrays (per-file key); no literal GUID or "\\\\.\\" in .rdata.
- **Signature dilution**: `signature_dilution.hpp` uses only MAGIC-derived constants and loop counts; no copy of public junk (no 0x11111111, JunkFunction1, or fixed 1000-iteration pattern).

**Unavoidable tradeoff:** Executed syscall instruction sequence follows Windows convention; MmUnloadedDrivers clearing semantics may be detected (we avoid literal bytes/strings).

---

## 3. Checklist – Before Adding New Anti-Detection

- Prefer **MAGIC-derived or build-time varied** keys; avoid a single literal (e.g. 0x5A, or one fixed byte) across the project. When config is absent, derive key from build time (e.g. `__TIME__`) not a fixed 0x9D.
- **No literal** "ntdll.dll", "NtGetNextProcess", "NtQuerySystemInformation", device names, or registry/section names in .rdata; use api_resolve / lazy_import encoded resolve / runtime-built strings / encoded arrays decoded at runtime.
- **No literal** public byte patterns (syscall stub, trace-cleaner LEA) in .rdata; encode with project key, decode at runtime.
- **No literal** single constant signatures (e.g. 0x5A4D) where avoidable; use derived form (e.g. MZ from character literals).
- **No copy-paste** from public UC/tutorial code; derive or use project-specific values.
- **Document** when a known technique is used and why (e.g. MmUnloadedDrivers clearing semantics).

---

## 4. References

- UnknownCheats: [EAC detect kernel drivers bypasses](https://www.unknowncheats.me/forum/anti-cheat-bypass/438804-eac-detect-kernel-drivers-bypasses.html), [EAC scanning unsigned drivers](https://www.unknowncheats.me/forum/anti-cheat-bypass/635703-eac-scanning-unsigned-drivers.html), KdMapper detection (614327, 594960).
- Project: `EAC_ANTIDETECTION.md`, `DETECTION_RISKS_AND_STATUS.md`, `PROCESS_ENUM_EAC_RESEARCH.md`, `ALPC_COMMUNICATION_RESEARCH.md`.

**EAC current detection and remediation:** See **EAC_DETECTION_AND_REMEDIATION.md** for EAC build/heuristics/flagging (UC, up-to-date), “can still be detected” → custom/non-documented fixes (sysinfo-only process enum, custom_nt alloc/protect, optional registry NT and indirect syscall).
