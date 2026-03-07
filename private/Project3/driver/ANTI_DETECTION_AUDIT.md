# Anti-Detection Audit – Least to Most Detected

**Date:** Feb 2026  
**Scope:** Driver (FlushComm), usermode (core, utilities), mapper (kdmapper), protections.  
**Sources:** Project research docs, UnknownCheats, tulach.cc, VollRagm, 2024–2026 web research.

---

## Executive Summary

Techniques in use are ordered from **least detected** to **most detected**. Disabled or alternative techniques are listed where relevant. Recommendations follow each section.

---

## 1. LEAST DETECTED (Low Risk / Unmonitored)

| Technique | Location | Why Low Risk |
|-----------|----------|--------------|
| **FlushFileBuffers (IRP_MJ_FLUSH_BUFFERS)** | `flush_comm_config.h`, driver | No `DeviceIoControl` / `NtDeviceIoControlFile`; IOCTL path not used. IRP_MJ_FLUSH_BUFFERS historically and currently less monitored than IRP_MJ_DEVICE_CONTROL. |
| **Section-based shared memory** | `FLUSHCOMM_USE_SECTION 1` | No `MmCopyVirtualMemory`; direct mapping. Avoids cross-process copy patterns and pool-based comm. |
| **Reject registry fallback** | `FLUSHCOMM_REJECT_REGISTRY_FALLBACK 1` | EAC: MmCopyVirtualMemory detected (UC 496628). When section fails, abort instead of VirtualAlloc+registry path. |
| **Section retry** | `FLUSHCOMM_SECTION_RETRY_COUNT 4`, `FLUSHCOMM_SECTION_RETRY_DELAY_MS 150` | Driver may create section slightly after map; retry avoids false failure. |
| **Magic XOR obfuscation** | `FLUSHCOMM_MAGIC_XOR 0x5A3C9E1B` | Per-build XOR reduces static signature scans of shared buffer protocol. |
| **Throttling + jitter** | `FLUSHCOMM_THROTTLE_MS 2`, `FLUSHCOMM_JITTER_MS 2` | Reduces bursty I/O patterns. Override via `/DFLUSHCOMM_THROTTLE_MS=3` for per-build variation. |
| **Benign pool tags** | `EVASION_POOL_TAG_*`, kwrO, tsNl, etc. | Fixed, Windows-style tags; not in AC blocklists. |
| **Lazy import (usermode)** | `FLUSHCOMM_USE_LAZY_IMPORT 1` | `CreateFileW`, `DeviceIoControl`, etc. resolved at runtime; less IAT visibility. |
| **Random suffix (Release)** | `Directory.Build.targets` | Registry/section names vary per build; reduces static string signatures. |

**Recommendations:** Keep all. These are baseline best practices.

---

## 2. LOW–MEDIUM DETECTION RISK

| Technique | Location | Risk Notes |
|-----------|----------|------------|
| **Direct syscall (PING)** | `FLUSHCOMM_USE_DIRECT_SYSCALL 1` | Bypasses ntdll hooks; instrumentation callbacks can detect return address not in ntdll, but tradeoff vs hooked path is favorable. |
| **FILE_OBJECT hook** | `FLUSHCOMM_USE_FILEOBJ_HOOK 1` | Modifies `FILE_OBJECT->DeviceObject` instead of MajorFunction directly. Less obvious than full IRP dispatch overwrite; CREATE handler redirects to fake device. |
| **Synchronous mouse** | `FLUSHCOMM_MOUSE_SYNC 1` | IRP runs in caller context; no worker threads, avoids ScanSystemThreads patterns. |
| **Codecave (LargePageDrivers)** | `FLUSHCOMM_USE_CODECAVE 1` (enabled) | When enabled: PING runs from Beep’s signed `.data`. Low–medium: modifies signed driver; integrity checks possible. Inline fallback if LargePageDrivers not set. |
| **Trace cleaner** | `FLUSHCOMM_TRACE_CLEANER 1` | Clears MmUnloadedDrivers (vuln driver traces). Some ACs check MmUnloadedDrivers tampering; primary threat is leaving vuln traces visible. Net gain to enable. |

**Recommendations:** Keep FILE_OBJECT, direct syscall, sync mouse, trace cleaner. Codecave enabled; add `beep.sys` to LargePageDrivers for maximum stealth (optional; inline fallback if not set).

---

## 3. MEDIUM DETECTION RISK

| Technique | Location | Risk Notes |
|-----------|----------|------------|
| **Manual mapping** | kdmapper | Driver not in PsLoadedModuleList; MajorFunction and execution in unmapped regions are detectable. Mitigated by FlushFileBuffers (no IOCTL dispatch), FILE_OBJECT redirect, and section comm. |
| **CreateToolhelp32Snapshot** | `entrypoint.cpp`, `utilities/impl/driver.hpp`, `com/driver.hpp`, kdmapper | High-profile API; often hooked. PEB-based module enum is preferred where applicable. |
| **MajorFunction hook (indirect)** | Driver (Beep IRP_MJ_CREATE) | Even via FILE_OBJECT redirect, CREATE handler and dispatch chain involve our code. ACs can validate dispatch pointers; FILE_OBJECT approach is less obvious than direct MajorFunction overwrite. |
| **Spoofer hooks (usermode)** | `spoofer_hooks.hpp` | **Disabled** (`SPOOFER_ENABLE_HOOKS 0`). RtlGetVersion/NtQuerySystemInformation inline hooks; can crash on some Windows builds; ACs may detect ntdll patches. |

**Recommendations:**  
1. Replace `CreateToolhelp32Snapshot` with `NtGetNextProcess` or `NtQuerySystemInformation(SystemProcessInformation)` where process enumeration is needed (see `EAC_ANTIDETECTION.md`).  
2. Keep spoofer hooks disabled unless tested on target build.  
3. Manual mapping is foundational; focus on reducing other vectors.

---

## 4. HIGHER DETECTION RISK (Mitigated or Disabled)

| Technique | Location | Status | Notes |
|-----------|----------|--------|-------|
| **NMI spoof** | `FLUSHCOMM_USE_NMI_SPOOF 0` | Disabled | HalPrivateDispatchTable hook; Vanguard, EMACLAB scan for HAL tampering. |
| **ETW patch** | `FLUSHCOMM_PATCH_ETW 0` | Disabled | ntdll!EtwEventWrite patch is a known detection vector. |
| **WSK / sockets** | `FLUSHCOMM_USE_WSK 0` | Disabled | EAC ScanSystemThreads detects socket/listener thread patterns. |
| **Pool tag rotation** | `FLUSHCOMM_POOL_TAG_ROTATE 0` | Disabled | Evasion heuristic; normal drivers use fixed tags. |

---

## 5. MAPPER-SPECIFIC (kdmapper)

| Technique | Status | Detection Notes |
|-----------|--------|------------------|
| IOCTL XOR | Enabled | Intel driver IOCTL obfuscated; hides literal codes from static analysis. |
| Case number obfuscation | Enabled | 0x33, 0x30, 0x25, etc. XOR’d at compile time. |
| Device path XOR | Enabled | Hides `\\.\Nal` in binary. |
| Random driver name | Enabled | Per-run variation. |
| Driver dump + overwrite | Enabled | Cleans file traces post-mapping. |
| Lazy importer | Enabled | IAT hiding for CreateFileW, DeviceIoControl, VirtualAlloc, etc. |

**Note:** Intel vuln driver (Nal) is a fixed binary; struct/IOCTL swap inside it is not possible. Mapper-side obfuscation is maxed for our constraints.

---

## 6. REMAINING DETECTION VECTORS (Unaddressed)

| Vector | Current State | Recommendation |
|--------|---------------|----------------|
| **NMI stack walking** | No mitigation (NMI spoof disabled) | If AC uses NMI callbacks, RIP/RSP during handler execution can point to mapped code. Accept risk or research ICALL-GADGET–style indirect execution (no public impl). |
| **CreateToolhelp32Snapshot** | Used in entrypoint, driver.hpp, com, kdmapper | Migrate to NtGetNextProcess / NtQuerySystemInformation where feasible. |
| **EnumProcesses** | `debugger-detection.hpp` | Replace with NtGetNextProcess or NtQuerySystemInformation (EAC_ANTIDETECTION.md). |
| **PsLoadedModuleList / MajorFunction validation** | Mitigated | Manual map = no module entry; FlushFileBuffers + FILE_OBJECT reduce IOCTL dispatch exposure. Codecave (when enabled) moves PING to signed memory. |

---

## 7. Ordered List: Least → Most Detected

1. Magic XOR, throttling, jitter, benign pool tags  
2. Lazy import, random suffix, section-based comm  
3. FlushFileBuffers (no IOCTL path)  
4. Direct syscall (PING), FILE_OBJECT hook  
5. Sync mouse, trace cleaner  
6. Mapper obfuscation (IOCTL XOR, case XOR, device path XOR, lazy import)  
7. Manual mapping (mitigated by 1–6)  
8. CreateToolhelp32Snapshot, EnumProcesses  
9. Spoofer hooks (inline ntdll patches) – **disabled**  
10. Trace cleaner tampering (secondary risk; primary = vuln trace visibility)  
11. NMI spoof, ETW patch, WSK, pool tag rotation – **disabled**

---

## 8. Recommended Changes

### Immediate (Low Effort)

- **Replace CreateToolhelp32Snapshot** where process enumeration is needed: use `NtGetNextProcess` or `NtQuerySystemInformation(SystemProcessInformation)` with direct syscall.  
- **Replace EnumProcesses** in `debugger-detection.hpp` with same pattern.

### Optional (Max Stealth)

- **Codecave enabled:** Add `beep.sys` to `LargePageDrivers` (registry + reboot) for signed PING. Without it, inline fallback is used.

### Do Not Enable

- NMI spoof – HalPrivateDispatchTable scanning (Vanguard, EMACLAB).  
- ETW patch – known ntdll patch vector.  
- WSK – socket/thread patterns.  
- Pool tag rotation – evasion heuristic.

### Keep As-Is

- FlushFileBuffers, section, FILE_OBJECT, direct syscall, sync mouse, trace cleaner, lazy import, mapper obfuscation.

---

## References

- `DETECTION_RISKS_AND_STATUS.md`  
- `IOCTL_VS_ALTERNATIVES_RESEARCH.md`  
- `EAC_ANTIDETECTION.md`  
- `NMI_STACK_WALKING_RESEARCH.md`  
- tulach.cc – Detecting manually mapped drivers  
- VollRagm – LargePageDrivers abuse  
- EAC/BattlEye 2025 detection methods (web)
