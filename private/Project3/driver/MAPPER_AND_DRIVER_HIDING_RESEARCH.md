# Mapper & Driver Hiding Techniques – Research (2024–2026)

Research from UnknownCheats, GitHub, security blogs, and project docs. Ordered by feasibility and detection impact.

---

## Part 1: Mapper (kdmapper) Hiding Techniques

### 1.1 Already Implemented in This Project

| Technique | Location | Notes |
|-----------|----------|-------|
| **IOCTL XOR obfuscation** | `kdm_obfuscate.hpp` | IOCTL_ENC ^ IOCTL_KEY at runtime; hides 0x80862007 literal |
| **Device path XOR** | `GetDevicePath()` | Hides `\\.\Nal` string in binary |
| **Case number obfuscation** | `kdm_obfuscate.hpp` | 0x33, 0x30, 0x25, 0x19, 0x1A XOR'd; decoded at runtime |
| **Random driver name** | `GetDriverNameW()` | Per-run variation for mapper .sys dump |
| **Driver dump + overwrite** | `intel_driver::Unload()` | Cleans file traces post-mapping |
| **Lazy importer** | `lazy_import.hpp` | CreateFileW, DeviceIoControl, VirtualAlloc, NtQuerySystemInformation via GetProcAddress |
| **ClearMmUnloadedDrivers** | `intel_driver.cpp` | Removes vuln driver from MmUnloadedDrivers |
| **ClearPiDDBCacheTable** | `intel_driver.cpp` | Removes vuln driver from PiDDB cache |
| **Intel + eneio fallback** | `includes.h` | Tries Intel first, eneio if blocked (HVCI compatibility) |

### 1.2 Additional Mapper Techniques (UC / Community)

#### Trace Cleanup Extensions

| Structure | Status | Notes |
|-----------|--------|------|
| **MmUnloadedDrivers** | Done (mapper + driver) | EAC scans; must clear vuln entries |
| **PiDDBCacheTable** | Done (mapper) | PiDDBLock + RtlDeleteElementGenericTableAvl |
| **g_KernelHashBucketList** | Not in project | Some cleaners target this; kdmapper forks may have it |
| **Wdfilter traces** | Not in project | RuntimeDriverList, RuntimeDriverCount, RuntimeDriverArray – EAC scans |
| **MmLastUnloadedDriver** | Risk | Zeroing without updating can cause integrity mismatch; some ACs check |

**Source:** UC 438804 (Swiftik), x64DriverCleaner, 64KernelDriverCleaner, kdmapper forks.

#### Allocation & Pool

| Technique | Feasibility | Notes |
|-----------|-------------|-------|
| **--indPages** | Exists | Independent pages; evades pool-based scans; can BSOD on some systems (project uses pool) |
| **Big pool spoof** | Hard | EAC scans big pool; spoof entry instead of avoiding. Requires pool table manipulation |
| **PFN zeroing** | Medium | After free, zero PFN so physical scan doesn't find remnants. UC 438804 (zach898) |
| **Avoid big pool** | Medium | Use alternative allocation; "find other methods yourself" (Swiftik) |

**Source:** UC 438804, UC 3020747 (PFN link).

#### Obfuscation & Fingerprint

| Technique | Feasibility | Notes |
|-----------|-------------|-------|
| **ETW patch** | Medium | Patch ntdll!EtwEventWrite; some ACs detect. Project has FLUSHCOMM_PATCH_ETW 0 |
| **String encryption** | Easy | Encrypt "ntoskrnl.exe", "Nal", log strings. Use skCrypt or similar |
| **Mapper process name** | Easy | Rename kdmapper.exe to benign name (e.g. IntelCpHDCP.exe) |
| **CreateProcess flags** | Easy | CREATE_NO_WINDOW already used; consider CREATE_SUSPENDED + NtResumeThread for timing |
| **Download URL obfuscation** | Easy | Encrypt/hash mapper download URL |

#### Vulnerable Driver Alternatives

| Driver | Status | Notes |
|--------|--------|-------|
| **iqvw64e.sys** (Intel) | Blocklisted | Memory Integrity blocks; Windows 11 22H2+ |
| **eneio64.sys** | HVCI-compatible | CVE-2020-12446; still loads on Win11 22H2–24H2 (Mar 2025) |
| **rtkio.sys** | HVCI-authorized | Alternative vuln driver |
| **Capcom.sys** | Legacy | SeLoadDriver abuse; well-known |
| **RTCore64.sys** | Known | Physical memory; likely blocklisted |
| **Custom vuln driver** | Best | Own signed driver with custom IOCTL; no public sigs |

**Source:** LOLDrivers, xacone.github.io/eneio-driver, UC 513753.

#### Execution Model

| Technique | Feasibility | Notes |
|-----------|-------------|-------|
| **Manual map without thread** | Hard | Hook functions instead of CreateRemoteThread; "not undetected" (UC 665677) |
| **--free flag** | Exists | kdmapper option; may help; effectiveness unclear (UC 700798) |
| **Pre-AC load** | Critical | Load driver before game/AC starts; AC may scan at init |

### 1.3 Mapper Detection Vectors (What ACs Check)

- **Memory allocation** – Regions where manually mapped driver resides (UC 700798, PUBG May 2025)
- **Vuln driver traces** – MmUnloadedDrivers, PiDDB, Wdfilter
- **CreateProcess** – Mapper spawning; benign name + CREATE_NO_WINDOW helps
- **File artifacts** – Driver dump path; overwrite + random name helps
- **IAT** – CreateFileW, DeviceIoControl; lazy import helps
- **Static signatures** – IOCTL codes, device path, case numbers; XOR helps

---

## Part 2: Driver (FlushComm) Hiding Techniques

### 2.1 Already Implemented

| Technique | Config | Notes |
|-----------|--------|------|
| **FlushFileBuffers** | FLUSHCOMM_USE_FLUSH_BUFFERS 1 | No IOCTL; IRP_MJ_FLUSH_BUFFERS |
| **Section shared memory** | FLUSHCOMM_USE_SECTION 1 | No MmCopyVirtualMemory |
| **Reject registry fallback** | FLUSHCOMM_REJECT_REGISTRY_FALLBACK 1 | No MmCopyVirtualMemory path |
| **FILE_OBJECT hook** | FLUSHCOMM_USE_FILEOBJ_HOOK 1 | Redirect vs MajorFunction overwrite |
| **Physical memory read** | driver.cpp | MmMapIoSpaceEx; no MmCopyMemory |
| **Trace cleaner** | FLUSHCOMM_TRACE_CLEANER 1 | MmUnloadedDrivers in mapped driver |
| **Codecave** | FLUSHCOMM_USE_CODECAVE 1 | PING from signed beep.sys .data |
| **Sync mouse** | FLUSHCOMM_MOUSE_SYNC 1 | No worker threads |
| **Benign pool tags** | EVASION_POOL_TAG_* | kwrO, tsNl, lvEn, rPmM |
| **Magic XOR** | FLUSHCOMM_MAGIC_XOR | Per-build protocol obfuscation |
| **Random suffix** | Directory.Build.targets | Section/registry names vary per build |

### 2.2 Additional Driver Techniques (UC / Research)

#### Execution Evasion

| Technique | Feasibility | Notes |
|-----------|-------------|-------|
| **NMI stack spoof** | High risk | Hook HalPreprocessNmi; Vanguard/EMACLAB scan HAL. Disabled |
| **ICALL-GADGET** | Research | Redirect through legit module to evade stack walk; no public impl |
| **LargePageDrivers** | Done | Codecave uses signed .data; no MDL on .text |
| **Route hooks through legit** | Medium | If hooking, forward to useless legit func first (Swiftik UC 438804) |

#### Thread & Callback

| Technique | Feasibility | Notes |
|-----------|-------------|-------|
| **Thread hiding** | Hard | Unlink from list; spoof start address, call stack. EAC ScanSystemThreads |
| **No WSK/sockets** | Done | Disabled; socket threads detected |
| **No IoQueueWorkItem** | Done | Sync mouse; no worker with mapped code on stack |

#### Memory & Pool

| Technique | Feasibility | Notes |
|-----------|-------------|-------|
| **Pool tag rotation** | Disabled | Evasion heuristic; fixed benign tags safer (DETECTION_RISKS) |
| **Avoid RWX** | Done | alloc_executable: RW then VirtualProtect RX |
| **Page hiding** | Hard | Unlink from VAD; "do your research" (XxxBrotato) |

#### Trace & Integrity

| Technique | Feasibility | Notes |
|-----------|-------------|-------|
| **MmLastUnloadedDriver** | Risk | Zeroing entries without checksum update can trigger tampering check |
| **PiDDB from driver** | Optional | trace_cleaner has placeholder; kdmapper does it |
| **Wdfilter cleanup** | Not in project | RuntimeDriverList etc.; some mappers clear |

### 2.3 Driver Detection Vectors

- **Manual-mapped execution** – RIP outside valid modules; NMI stack walking
- **MajorFunction** – Pointers outside .text; FILE_OBJECT mitigates
- **IRP stack trace** – DispatchDeviceIoControl in paged pool; FlushFileBuffers uses different path
- **MmCopyVirtualMemory** – Detected (UC 496628); section path avoids
- **IOCTL** – Heavily detected; FlushFileBuffers avoids
- **Pool allocations** – Malicious tags; benign tags used

---

## Part 3: Recommendations

### Mapper – High Impact

1. **Add Wdfilter trace cleanup** – If kdmapper supports it; RuntimeDriverList scan by EAC
2. **Encrypt mapper download URL** – Reduce static sig
3. **Rename mapper exe** – Benign name (e.g. IntelCpHDCP.exe)
4. **String encrypt** – Remaining literals in mapper (ntoskrnl, device names)
5. **Test eneio first on HVCI** – When Intel blocked; eneio HVCI-compatible

### Mapper – Medium Impact

6. **PFN zeroing** – After freeing driver memory; see UC 3020747
7. **ETW patch** – Optional; some ACs detect
8. **--free** – Test if helps; PUBG 2025

### Driver – High Impact

9. **Ensure section always used** – FLUSHCOMM_REJECT_REGISTRY_FALLBACK 1 (done)
10. **LargePageDrivers setup** – For codecave; PING from signed memory
11. **Trace cleaner** – Keep enabled; vuln trace = primary threat

### Driver – Already Optimal

- FlushFileBuffers, section, FILE_OBJECT, physical read, sync mouse, benign tags
- NMI spoof, WSK, pool rotation – disabled for good reason

---

## Part 4: References

### UnknownCheats
- UC 438804 – How does EAC/BE detect kernel drivers bypasses? (Swiftik, zach898)
- UC 496628 – MmCopyVirtualMemory detected
- UC 513753 – Find Vulnerable Drivers
- UC 614327 – KdMapper info and detection
- UC 639518 – Hiding Kernel Driver
- UC 665677 – Manual map creating thread
- UC 686506 – Hiding physical/driver memory
- UC 700798 – kdmapper banned by PUBG
- UC 3020747 – PFN zeroing (zach898 link)

### GitHub / External
- CPScript/x64DriverCleaner
- DErDYAST1R/64KernelDriverCleaner
- VollRagm/lpmapper, LargePageDrivers
- xacone.github.io/eneio-driver
- lxkast.github.io – NMI stack spoofing
- LOLDrivers – Vuln driver DB

### Project
- EVASION_TECHNIQUES_RESEARCH.md
- DETECTION_RISKS_AND_STATUS.md
- ANTI_DETECTION_AUDIT.md
- trace_cleaner.hpp, kdm_obfuscate.hpp
