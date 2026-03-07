# EAC Anti-Detection Reference

Design notes for reducing detection by Fortnite's Easy Anti-Cheat. Implement incrementally.

---

## 1. Implemented Mitigations

### 1.1 Process Enumeration (Least → Most Detected)

| Method | Risk | Status |
|--------|------|--------|
| **Direct syscall** (SSN from ntdll, bypass hooks) | Lowest | Implemented in `direct_syscall.hpp` |
| `NtGetNextProcess` + `NtQueryInformationProcess` | Low – fewer hooks than snapshot | Implemented |
| `NtQuerySystemInformation(SystemProcessInformation)` | Medium | Alternative; bulk fetch |
| `CreateToolhelp32Snapshot` | High | Fallback only |
| `EnumProcesses` | Medium | Still in debugger-detection.hpp |

**Research**: NtGetNextProcess iterates kernel process list; less commonly hooked than CreateToolhelp32Snapshot. Direct syscall (SSN from ntdll at runtime; adjacent-function fallback if hooked) bypasses ntdll hooks entirely. No public technique names in code or comments.

**`driver.hpp`**: `find_process()` uses `sys_NtGetNextProcess` (direct syscall when SSN valid, else normal call).

**Future**: Replace `EnumProcesses` in `protections/debugger-detection.hpp` with `NtGetNextProcess` or `NtQuerySystemInformation(SystemProcessInformation)`.

### 1.2 Memory Read Path (Driver)

| Method | Risk | Status |
|--------|------|--------|
| `KeStackAttachProcess` | High – EAC can monitor attach/detach | Reduced |
| Physical path (MmMapIoSpace + manual page walk) | Lower – no attach | Primary when CR3 cached |

**`driver.cpp`**: `frw()` now tries **physical path first** when CR3 is cached. `KeStackAttachProcess` is only used as fallback (e.g. first read before cache, or when physical translation fails).

### 1.3 Module Enumeration

| API | Risk | Status |
|-----|------|--------|
| `CreateToolhelp32Snapshot(TH32CS_SNAPMODULE)` | High | Replaced |
| `EnumProcessModules` | High | Replaced |
| PEB walk | Lower | Implemented in `peb_modules.hpp` |

### 1.4 Pool Tags

- Base tags: `'Fls '`, `'Cc  '`, `'Io  '`, `'Mm  '` (Flush/Cache/I/O/Memory manager–like; not from public cheat/mapper lists). Optional runtime rotation via `FLUSHCOMM_POOL_TAG_ROTATE`.

### 1.5 Device Communication

- FlushComm uses Beep/Null IRP hook (no new device name).
- Registry path and section name are **built at runtime** from `FLUSHCOMM_SECTION_SEED` (derived from `FLUSHCOMM_MAGIC`); no public literals (e.g. MdmTrace, WdfCtl) in the binary.

### 1.6 Flush communication – non-public methods

Anti-detection and obfuscation for flush communication are **intentionally non-documented and not taken from public implementations**:

- **Naming**: Registry path and section name are derived at runtime from `FLUSHCOMM_MAGIC` (e.g. `SOFTWARE\%06X\<suffix>`, `Global\%06X<suffix>`). No fixed strings such as "MdmTrace" or "WdfCtl_" appear.
- **String obfuscation**: Per-byte key `OBF_KEY + (i & 0xF)` in `flush_comm_obfuscate.h` (no 0x5A). API-resolve, trace cleaner, and driver device paths use **FLUSHCOMM_OBF_BASE** (derived from `FLUSHCOMM_MAGIC`) so no single literal key is a project-wide signature; vary `FLUSHCOMM_MAGIC_XOR` per build.
- **Protocol/layout**: Shared buffer layout and magic are project-specific; do not copy from public tutorials.

Do not replace these with well-known public constants or patterns.

### 1.7 Non-public methods (mapper, driver, communication, read/write)

Anti-detection and obfuscation across **mapper, driver, communication, and read/write** use non-documented, non-public methods where possible:

- **Trace cleaner** (`driver/trace_cleaner.hpp`): "ntoskrnl.exe" and vuln driver names decoded at runtime; key = **FLUSHCOMM_OBF_BASE** (no fixed 0x5A).
- **Pool tags** (`driver/page_evasion.hpp`): Tags chosen from a set that is not the commonly cited Nls/Envl/MmPr/Orwk; optional rotation at init.
- **Magic / IOCTL** (`flush_comm_config.h`): `FLUSHCOMM_MAGIC_XOR` and IOCTL function codes are non-public constants (no 0x5A in magic XOR).
- **Kdmapper** (`kdmapper/include/kdm_obfuscate.hpp`): Device path, ntoskrnl names, and Eneio path use non-public XOR keys (no 0x5A/0x53). IOCTL and case numbers stored xor'd with non-public keys.
- **Direct syscall** (`utilities/direct_syscall.hpp`): SSN extraction and adjacent-function fallback implemented without naming public techniques in comments or code.
- **Read/write** (`driver.cpp`): Physical path preferred when CR3 cached; IOCTL codes and structures are project-specific. No public magic or layout patterns.
- **Section layout** (`flush_comm_config.h`): Shared-buffer offsets derived from named constants (`FLUSHCOMM_HEADER_*_SZ`, `FLUSHCOMM_STATUS_OFFSET`, `FLUSHCOMM_DATA_OFFSET`); no literal 88/80 in code.
- **Spoofer hooks** (`protections/spoofer_hooks.hpp`): Resolve RtlGetVersion/NtQuerySystemInformation via **api_resolve** (no literal "ntdll.dll" or API names in binary). Comments avoid publishing exact stub byte patterns.
- **CODE_SECURITY** (`flush_comm_config.h`): RW/BA/GA security field derived from `FLUSHCOMM_MAGIC` (no literal 0x4E8A2C91).
- **Trace cleaner** (`driver/trace_cleaner.hpp`): Vuln driver names (iqvw64e, capcom, etc.) encoded at compile time; decoded at runtime when matching (no literals in .rdata).
- **Driver device paths** (`driver/flush_comm.cpp`): `\Device\Beep`, `\Device\Null`, `\Device\PEAuth` and codecave base names decoded at runtime from encoded arrays (no literals in .rdata).

Do not reintroduce public literals, well-known XOR keys (e.g. 0x5A), or public technique names.

### 1.8 Overlay Window (Own Overlay Path)

- Window class/title avoid game-specific strings: `Win32HostWindow` / `Window` instead of `FortniteOverlayClass` / `Fortnite Overlay`.
- WndProc and custom message names use neutral identifiers (`HostWndProc`, `WM_REQUEST_FG`).
- Window styles (WS_EX_LAYERED, WS_EX_TOPMOST, WS_EX_TRANSPARENT) remain required for functionality; combination is common for overlays but unavoidable.

---

## 2. Remaining Vectors & Recommendations

### 2.1 Usermode Traces

| Location | Current | Recommendation |
|----------|---------|----------------|
| `entrypoint.cpp:183` | `CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS)` | Use `NtGetNextProcess` or shared helper |
| `entrypoint.cpp:538` | `CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS)` | Same |
| `debugger-detection.hpp` | `EnumProcesses`, `OpenProcess` | `NtGetNextProcess` / `NtQuerySystemInformation` |

### 2.2 Kernel Traces (Implemented)

**`driver/trace_cleaner.hpp`**: Clears MmUnloadedDrivers for vuln drivers (iqvw64e, capcom, etc.) at DriverEntry. Pattern-scan based; run after FlushComm init.

- **MmUnloadedDrivers** – cleared for iqvw64e.sys, capcom.sys.
- **PiDDBCacheTable** – requires AVL traversal; not yet implemented.

### 2.3 MmCopyVirtualMemory

- FlushComm uses `MmCopyVirtualMemory` for usermode↔kernel shared buffer. EAC may trace.
- **Alternative**: Shared memory section (physical pages mapped into both address spaces) – larger refactor.

### 2.4 Spoofer Hooks (Implemented)

**`protections/spoofer_hooks.hpp`**: Usermode inline hooks for:

- `RtlGetVersion` – trampoline pass-through (spoof logic can be added).
- `NtQuerySystemInformation` – trampoline pass-through (filter SystemModuleInformation for driver hide).

**`DotMem::spoofer_enable(true)`** installs the hooks at startup.

### 2.5 Behavioral

- **Read timing**: Consider adding small jitter to read intervals to avoid fixed-period patterns.
- **Thread context**: Return address spoofing in `spoof.h` helps mask caller.

---

## 3. Detection Vectors Summary

| Category | Implemented | Pending |
|----------|-------------|---------|
| Process enum | NtGetNextProcess (find_process) | EnumProcesses in debugger-detection |
| Module enum | PEB walk | - |
| Memory R/W | Physical-first when CR3 cached | - |
| Pool tags | Benign tags | - |
| Driver traces | - | PiDDBCache, MmUnloadedDrivers |
| Spoofer hooks | - | RtlGetVersion, NtQuerySystemInformation |
| Snapshot APIs | Replaced in find_process | Other snapshot usages in entrypoint |

---

## 4. Files Modified

| File | Changes |
|------|---------|
| `utilities/impl/driver.hpp` | NtGetNextProcess-based `find_process` |
| `driver/driver.cpp` | Physical path preferred in `frw()`, extract `frw_physical()` |
| `utilities/overlay/render.h` | Neutral window class/title, WndProc/message names |