# Anti-Detection Status (2024–2025)

Reference: current anti-cheat detection vectors (EAC, BattlEye, etc.) and how this project addresses them. Sourced from UnknownCheats, security research, and Microsoft/elastic documentation.

## 0. Detected Vectors Avoided (Default Build)

With default config the following are **not used**:

| Vector | Avoidance |
|--------|-----------|
| **IOCTL / DeviceIoControl** | FLUSHCOMM_USE_FLUSH_BUFFERS=1: all requests and handshake use FlushFileBuffers only. Driver does not install IRP_MJ_DEVICE_CONTROL. LazyImport does not resolve DeviceIoControl. |
| **CreateFileW (device open)** | open_hooked_device() tries NtOpenFile(\Device\Beep etc.) first; CreateFileW only as fallback. |
| **CreateToolhelp32Snapshot** | Removed from process enum. find_process_stealth uses NTFS PIDs then NtGetNextProcess only. enumerate_stealth uses NTFS then NtGetNextProcess. |
| **DeviceIoControl in IAT** | When FLUSH_BUFFERS=1, LazyImport::Init() does not resolve or require DeviceIoControl. |

---

## 1. Detection Vectors (Research Summary)

### 1.1 Kernel / Mapper

| Vector | Description | Source |
|--------|-------------|--------|
| **MmUnloadedDrivers** | List of unloaded drivers; ACs scan for vuln driver names (iqvw64e, capcom, eneio, etc.) | UC 438804, x64DriverCleaner, EAC |
| **PiDDBCacheTable** | Kernel cache of driver load history; same names | UC, 64KernelDriverCleaner |
| **Big pool / pool tags** | `ZwQuerySystemInformation(SystemBigPoolInformation)` and pool tag scanning; known tags = fingerprint | UC 673385, pool tag research |
| **Manual map regions** | Allocations that are executable but not backed by a known module | UC 700798, PUBG 2025 |
| **Vuln driver file/device** | CreateFile on `\\.\Nal`, `\\.\GLCKIo`; IOCTL codes; device names in binary | Static analysis, IAT |
| **Thread / call stack** | Hidden threads, stack walk for RIP in unmapped regions | EAC, NMI-style callbacks |
| **ETW / telemetry** | EtwEventWrite; some ACs detect patches | Usermode hooks |

### 1.2 Driver (Mapped / FlushComm)

| Vector | Description | Source |
|--------|-------------|--------|
| **MmCopyVirtualMemory** | EAC traces; UC 496628 | EAC |
| **DeviceIoControl** | IOCTL usage and codes | IAT, behavioral |
| **Pool tags** | ExAllocatePoolWithTag with known cheat tags | Pool scan |
| **Hooking** | MajorFunction overwrite, syscall hooks in ntoskrnl/win32k | Integrity checks |
| **Section / registry** | Named section, registry path for comm | Enumeration |

### 1.3 Usermode (External)

| Vector | Description | Source |
|--------|-------------|--------|
| **CreateFileW / NtOpenFile** | Opening `\\.\Beep` etc. – string in .rdata, timing | IAT, string scan, behavior |
| **DeviceIoControl / NtDeviceIoControlFile** | Direct syscall bypasses ntdll hooks | MDSec, Palo Alto direct syscall detection |
| **Process / module enum** | CreateToolhelp32Snapshot, EnumProcesses – commonly hooked | EAC, BE |
| **Window class/title** | “Overlay”, “Fortnite” in window name | String scan |

---

## 2. Current Mitigations in This Project

### 2.1 Mapper (kdmapper)

| Mitigation | Status | Notes |
|------------|--------|--------|
| **ClearMmUnloadedDrivers** | Done | intel_driver.cpp; removes vuln driver from list |
| **ClearPiDDBCacheTable** | Done | intel_driver.cpp |
| **IOCTL XOR** | Done | kdm_obfuscate.hpp – no literal 0x80862007 |
| **Device path XOR (Intel)** | Done | GetDevicePath() – no `\\.\Nal` in binary |
| **Device path XOR (Eneio)** | Done | GetEneioDevicePath() – no `\\.\GLCKIo` in binary |
| **Case number XOR** | Done | CaseMemCopy etc. – no 0x33, 0x30, etc. literals |
| **ntoskrnl name XOR** | Done | GetNtoskrnlName() / GetNtkrnlmpName() |
| **Random driver dump name** | Done | GetDriverNameW() – per-run name |
| **Lazy import** | Optional | KDMAPPER_USE_LAZY_IMPORT – CreateFileW, etc. via GetProcAddress |
| **Driver file overwrite after map** | Done | Reduces file artifacts |

### 2.2 Driver (FlushComm / mapped)

| Mitigation | Status | Notes |
|------------|--------|--------|
| **FlushFileBuffers (no IOCTL)** | On | FLUSHCOMM_USE_FLUSH_BUFFERS 1 – IRP_MJ_FLUSH_BUFFERS |
| **Section shared memory** | On | FLUSHCOMM_USE_SECTION 1 – no MmCopyVirtualMemory |
| **Reject registry fallback** | On | FLUSHCOMM_REJECT_REGISTRY_FALLBACK 1 |
| **FILE_OBJECT hook** | On | Redirect vs MajorFunction overwrite |
| **Physical read path** | On | MmMapIoSpaceEx; no MmCopyMemory |
| **Trace cleaner** | On | FLUSHCOMM_TRACE_CLEANER 1 – MmUnloadedDrivers cleared |
| **ntoskrnl.exe obfuscated** | Done | trace_cleaner – runtime decode, no literal |
| **PFN zeroing** | On | FLUSHCOMM_PFN_ZEROING 1 – zero vuln driver pages before clear |
| **Codecave (signed .data)** | On | FLUSHCOMM_USE_CODECAVE 1 – PING from Beep’s .data |
| **Benign pool tags** | Done | page_evasion.hpp – tsNl, lvEn, rPmM, kwrO |
| **Magic XOR** | On | FLUSHCOMM_MAGIC_BASE ^ FLUSHCOMM_MAGIC_XOR |
| **Random suffix (section/reg)** | On | Directory.Build.targets / override |
| **Throttle + jitter** | On | FLUSHCOMM_THROTTLE_MS / JITTER_MS |
| **Sync mouse** | On | FLUSHCOMM_MOUSE_SYNC 1 – no worker thread |
| **ICALL-GADGET** | Optional | FLUSHCOMM_USE_ICALL_GADGET – redirect through ntoskrnl |

### 2.3 Usermode (External)

| Mitigation | Status | Notes |
|------------|--------|--------|
| **Lazy import** | On | FLUSHCOMM_USE_LAZY_IMPORT 1 – CreateFileW, Reg*, etc. |
| **Direct syscall (PING)** | On | FLUSHCOMM_USE_DIRECT_SYSCALL 1 – NtDeviceIoControlFile |
| **Device path obfuscation** | Done | Build `\\.\` + decoded name at runtime; no full literal |
| **Delay before first open** | Done | 0–120 ms Sleep before first CreateFile to reduce pattern |
| **Process enum** | Done | NtGetNextProcess in find_process (driver.hpp) |
| **Window class/title** | Done | Win32HostWindow / “Window” (render.h) |
| **Registry value name** | Done | OBF_HookedDevice decoded at runtime |
| **Section name** | Done | WdfCtl_ + suffix (benign-looking) |

---

## 3. Optional / Disabled (Trade-offs)

| Item | Default | Reason |
|------|---------|--------|
| **FLUSHCOMM_PATCH_ETW** | 0 | Some ACs detect ntdll ETW patch |
| **FLUSHCOMM_POOL_TAG_ROTATE** | 0 | Rotation can be heuristically suspicious |
| **FLUSHCOMM_TRACE_CLEANER_WDFILTER** | 0 | Wdfilter monitored; high risk |
| **FLUSHCOMM_USE_NMI_SPOOF** | 0 | HAL hooks risky; Vanguard/EMACLAB scan |
| **Batch reads** | Removed | Was unused; single reads + throttle |

---

## 4. Recommendations

1. **Per-build variation**: Use different FLUSHCOMM_THROTTLE_MS/JITTER_MS and FLUSHCOMM_MAGIC_XOR per build to reduce single-signature value.
2. **Pre-AC load**: Load driver/mapper before starting the game when possible; AC may scan at init.
3. **Mapper process name**: Rename kdmapper.exe to something benign (e.g. IntelCpHDCP.exe) if desired.
4. **Replace remaining snapshot APIs**: entrypoint.cpp still uses CreateToolhelp32Snapshot in a couple of places; consider NtGetNextProcess or NtQuerySystemInformation where applicable.
5. **EAC unsigned driver scan (May 2024)**: EAC scans for unsigned drivers; DseFix-style load is reported detected. Use vulnerable signed driver + mapper trace cleanup (MmUnloadedDrivers, PiDDB) as implemented.

---

## 5. Files Touched (This Pass)

| File | Change |
|------|--------|
| `utilities/impl/driver.hpp` | Device path built at runtime (XOR decode); 0–120 ms delay before first open |
| `driver/trace_cleaner.hpp` | `ntoskrnl.exe` decoded at runtime (XOR), no literal |
| `kdmapper_src/kdmapper/include/kdm_obfuscate.hpp` | GetEneioDevicePath() for `\\.\GLCKIo` |
| `kdmapper_src/kdmapper/eneio_driver.cpp` | Use GetEneioDevicePath(); log message no longer contains literal path |

---

## 6. Alternative Communication Methods Researched

### 6.1 ALPC (Advanced Local Procedure Call)

**Status**: Researched but **not implemented** (see `ALPC_COMMUNICATION_RESEARCH.md`)

**Summary**:
- ALPC is an undocumented Windows IPC mechanism used by Windows/AV/EDR systems
- Could replace IOCTL/FlushFileBuffers with `NtAlpcCreatePort` (driver) + `NtAlpcConnectPort` (usermode)
- **Pros**: No device handle needed, legitimate Windows IPC, no IOCTL
- **Cons**: High complexity (undocumented APIs), port name visibility (new detection vector), unknown detection status
- **Decision**: Current FlushFileBuffers approach already avoids IOCTL and is simpler. ALPC adds complexity without proven benefit.

**Detection Risk**: Unknown - no evidence ALPC is detected, but also no evidence it's better than current approach.

---

## 7. References (High Level)

- UnknownCheats: kernel driver hiding (639518), EAC kernel bypass (590262, 438804), pool allocation (673385), manual map (700798).
- x64DriverCleaner, 64KernelDriverCleaner: MmUnloadedDrivers, PiDDBCacheTable.
- MDSec / Palo Alto: direct syscall and usermode hook bypass.
- Microsoft / Elastic: vulnerable driver abuse, BYOVD.
- ALPC Research: ALPChecker (2024), DEF CON 32 (2024), CVE-2025-64721, CVE-2022-38029.
- Project docs: EAC_ANTIDETECTION.md, MAPPER_AND_DRIVER_HIDING_RESEARCH.md, DETECTION_RISKS_AND_STATUS.md, ALPC_COMMUNICATION_RESEARCH.md.
