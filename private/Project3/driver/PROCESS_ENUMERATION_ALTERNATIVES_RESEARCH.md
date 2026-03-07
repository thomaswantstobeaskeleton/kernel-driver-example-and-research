# Process Enumeration Alternatives to CreateToolhelp32Snapshot

Research on lesser-detected methods for enumerating processes. CreateToolhelp32Snapshot is heavily monitored by AV/EDR and anti-cheats. Alternatives ordered by detection risk (least → most detected).

**Last updated:** Feb 2026

---

## Summary

| Method | Detection Risk | Pros | Cons |
|--------|----------------|------|------|
| **NTFS \ntfs\ trick** | **Lowest** | Non-typical API path; NtQueryInformationFile rarely monitored for process enum | Returns PIDs only; no process names without OpenProcess+QueryFullProcessImageName |
| **NtGetNextProcess** (direct syscall) | **Low** | Bypasses ntdll hooks; less commonly hooked than snapshot APIs | Iteration-based; requires OpenProcess/QueryFullProcessImageName for names; some builds may crash |
| **NtQuerySystemInformation(SystemProcessInformation)** | **Low–Medium** | Native API; bulk fetch; less monitored than Toolhelp | Returns full SYSTEM_PROCESS_INFORMATION; needs two-pass (NULL for size); undocumented struct |
| **CreateToolhelp32Snapshot** | **High** | Well-documented; reliable | Heavily hooked; common malware/cheat indicator |

---

## 1. NTFS Base Device (LloydLabs – Lowest Detection)

**Source:** [LloydLabs/process-enumeration-stealth](https://github.com/LloydLabs/process-enumeration-stealth)

### How It Works
- Open `\\.\ntfs` (or `\device\ntfs`) with `NtCreateFile` / `NtOpenFile` (`GENERIC_READ | SYNCHRONIZE`).
- Call `NtQueryInformationFile` with `FileProcessIdsUsingFileInformation` (FileInformationClass).
- Returns `PFILE_PROCESS_IDS_USING_FILE_INFORMATION` – a list of PIDs using the NTFS base device (essentially all processes).

### Why It’s Stealthy
- **Non-typical path:** Process enumeration is normally done via Toolhelp, EnumProcesses, NtQuerySystemInformation, or NtGetNextProcess.
- **File API, not process API:** NtQueryInformationFile is used for file metadata; using it for process IDs is unusual and rarely instrumented.
- **No snapshot or bulk process query:** Avoids the patterns that EDR/AC monitor.

### Limitations
- Returns **PIDs only**. To get process names/exe paths you must `OpenProcess` + `QueryFullProcessImageNameW` for each PID (or similar), which adds its own detection surface.
- `FileProcessIdsUsingFileInformation` value and `FILE_PROCESS_IDS_USING_FILE_INFORMATION` layout are not fully documented; may vary across Windows versions.

### Implementation Sketch
```c
// FileInformationClass for process IDs (undocumented; from LloydLabs PoC)
#define FileProcessIdsUsingFileInformation 47  // verify per build

UNICODE_STRING path;
RtlInitUnicodeString(&path, L"\\??\\GLOBALROOT\\Device\\Ntfs");
// or L"\\device\\ntfs"
OBJECT_ATTRIBUTES oa;
InitializeObjectAttributes(&oa, &path, OBJ_CASE_INSENSITIVE, NULL, NULL);
HANDLE hNtfs = ...;
NtCreateFile(&hNtfs, GENERIC_READ|SYNCHRONIZE, &oa, &iosb, ...);

// Buffer: PFILE_PROCESS_IDS_USING_FILE_INFORMATION (check size; LloydLabs uses ~64KB)
NtQueryInformationFile(hNtfs, &iosb, buf, bufsize, FileProcessIdsUsingFileInformation);

// buf->ProcessIdList[i], buf->NumberOfProcessIdsInList
```

---

## 2. NtGetNextProcess (Direct Syscall – Low Detection)

**Source:** EAC_ANTIDETECTION.md, direct_syscall.hpp, tulach/UC research

### How It Works
- Resolve `NtGetNextProcess` from ntdll.
- Extract SSN (Hell's Gate / Halo's Gate) to build a direct syscall stub; bypasses ntdll hooks.
- Iterate: `NtGetNextProcess(NULL, ...)` → handle; then `NtGetNextProcess(handle, ...)` until `STATUS_NO_MORE_ENTRIES`.

### Why It’s Stealthy
- **Direct syscall:** Return address is not in ntdll; instrumentation callbacks can detect this, but many implementations skip it.
- **Less hooked:** NtGetNextProcess is less commonly hooked than CreateToolhelp32Snapshot.
- **Iteration-based:** No bulk snapshot; each call is a single “get next” operation.

### Limitations
- **Crashes:** Project comment in `driver.hpp` notes: *"Use CreateToolhelp32Snapshot - reliable, avoids crashes from direct syscall/NtGetNextProcess path"*. Crashes may occur on some Windows builds (e.g. Win11 23H2).
- **Names:** Need `OpenProcess` + `QueryFullProcessImageNameW` per process for exe path.
- **OpenProcess:** Can be hooked; consider direct syscall for NtOpenProcess if needed.

### Already Implemented
- `direct_syscall.hpp`: `sys_NtGetNextProcess`, `enumerate_processes_nt()`.
- `find_process()` in `driver.hpp` was reverted to CreateToolhelp32Snapshot due to crashes.

### Recommendation
- Use as primary when stable on target OS.
- Fall back to NtQuerySystemInformation or CreateToolhelp32Snapshot if crashes occur.

---

## 3. NtQuerySystemInformation(SystemProcessInformation) (Low–Medium Detection)

**Source:** cocomelonc, nirajkharel, malware/EDR research

### How It Works
- Resolve `NtQuerySystemInformation` from ntdll.
- First call with NULL buffer to get required size (`STATUS_INFO_LENGTH_MISMATCH`).
- Allocate buffer, call again with `SystemProcessInformation` (value 5).
- Parse `SYSTEM_PROCESS_INFORMATION` linked list.

### Why It’s Stealthy
- **Native API:** Lower level than Toolhelp/EnumProcesses; less commonly monitored.
- **Single bulk query:** One syscall returns all process info.
- **Documentation:** Semi-documented; struct layout varies by Windows version.

### Limitations
- **Undocumented struct:** `SYSTEM_PROCESS_INFORMATION` layout can change; offsets must be verified per build.
- **Buffer sizing:** Process count grows; initial estimate may be too small; need retry with larger buffer.
- **Hooks:** NtQuerySystemInformation can still be hooked; direct syscall improves stealth.

### Implementation Notes
- `SystemProcessInformation` = 5.
- List is linked via `NextEntryOffset`; 0 means end.
- Each entry has `ImageName`, `ProcessId`, `NumberOfThreads`, etc.

---

## 4. EnumProcesses (Medium Detection)

- PSAPI; well-known; often hooked.
- Returns PIDs only; needs `EnumProcessModules` / `GetModuleBaseName` for names – additional monitored APIs.
- **Recommendation:** Prefer NtGetNextProcess or NtQuerySystemInformation.

---

## 5. CreateToolhelp32Snapshot (High Detection)

- Kernel32/tlhelp32; very common in malware and cheats.
- Heavily hooked by EDR and anti-cheats.
- **Recommendation:** Use only as last resort or fallback.

---

## Recommended Implementation Order

1. **NTFS trick** – For PIDs only; combine with lazy OpenProcess/QueryFullProcessImageName when names needed.
2. **NtGetNextProcess (direct syscall)** – Primary when stable; fallback to (3) if crashes.
3. **NtQuerySystemInformation(SystemProcessInformation)** – Bulk fetch; direct syscall if possible.
4. **CreateToolhelp32Snapshot** – Fallback only.

---

## Project Usage

| Location | Current | Recommended |
|----------|---------|-------------|
| `utilities/impl/driver.hpp` `find_process()` | CreateToolhelp32Snapshot | NtGetNextProcess (with CreateToolhelp32Snapshot fallback if crash) |
| `entrypoint.cpp` `ScanForSuspiciousMemory()` | CreateToolhelp32Snapshot | NtQuerySystemInformation or NtGetNextProcess |
| `entrypoint.cpp` `IsProcessOpen()` | CreateToolhelp32Snapshot | Same helper as find_process |
| `com/driver.hpp` | CreateToolhelp32Snapshot | Same helper |
| `kdmapper/main.cpp` | CreateToolhelp32Snapshot | NtGetNextProcess or NtQuerySystemInformation (mapper runs pre-game) |

---

## References

- [LloydLabs/process-enumeration-stealth](https://github.com/LloydLabs/process-enumeration-stealth) – NTFS \ntfs\ approach
- [cocomelonc – malware tricks 55](https://cocomelonc.github.io/malware/2025/12/25/malware-tricks-55.html) – NtQuerySystemInformation
- EAC_ANTIDETECTION.md – NtGetNextProcess, find_process
- direct_syscall.hpp – sys_NtGetNextProcess, enumerate_processes_nt
