# CreateToolhelp32Snapshot & EAC – Detection and Alternatives

Research on whether EAC detects CreateToolhelp32Snapshot and on alternative process-enumeration methods (UnknownCheats + up-to-date sources). **Conclusion: EPROCESS (our custom driver) first when loaded; then NtQuerySystemInformation (no handle per process). Toolhelp32 is worst – creates a handle on each process during iteration, triggering EAC callback and logging.**

---

## 1. Why EAC Detects Toolhelp32 (Handle-Based Detection)

- **CreateToolhelp32Snapshot / Process32First/Next** effectively **create a handle on each process** you iterate. EAC registers callbacks for process/object operations; when you enumerate with these APIs, **EAC's callback fires** and they **log the event plus basic info** (module, size, entrypoint offset, etc.). So detection is not only "API name" but **handle creation per process**.
- **NtQuerySystemInformation(SystemProcessInformation)** lets you iterate processes **without opening a handle**: one call returns a buffer with all process data (including image names); we walk the buffer in memory and **never open process handles**.
- **Our custom EPROCESS method** (driver-side walk via ActiveProcessLinks + ImageFileName) uses no usermode process-enumeration API and **no usermode handle**; the driver walks kernel structures and returns only the PID.

---

## 2. Which Is More Detected by EAC?

### CreateToolhelp32Snapshot / Process32First

- **Heavily monitored** by AV/EDR and anti-cheats: well-documented, commonly used by malware and cheats for process enumeration and injection (Sensei, aldeid, 2024 research).
- **No special privileges** required; easy to call, so security products and EAC commonly **hook or monitor** these APIs (UnknownCheats, general EAC discussions).
- **IAT/imports**: “CreateToolhelp32Snapshot” / “Process32First” in the binary are easy to scan; dynamic resolution reduces but does not remove monitoring of the call path.

### NtQuerySystemInformation (SystemProcessInformation)

- **Native API** (ntdll); **less heavily monitored** than documented Kernel32 APIs (Niraj Kharel, cocomelonc, 2024–2025).
- **Used by Windows** (Task Manager, Process Explorer); calling it directly **bypasses the documented APIs** that EDR/anti-cheat typically instrument (Stack Overflow: Toolhelp32/EnumProcesses use NtQuerySystemInformation internally).
- **No Toolhelp32 in the call stack**; resolved at runtime (no IAT). Considered a **stealthier** option for process enumeration.

### UnknownCheats / EAC context

- EAC is described as flagging **suspicious activity** (driver mapping, injection, process cache in ci.dll) rather than a single API; **which API you use for enumeration** still matters for profile (EAC process tracking thread, enumerate processes thread).
- No post explicitly says “EAC detects CreateToolhelp32Snapshot” or “EAC does not detect NtQuerySystemInformation,” but the consensus elsewhere is: **native/ntdll APIs are less monitored than Toolhelp32.**

### Conclusion (up-to-date)

- **Best**: Our **custom EPROCESS walk** (driver) – no usermode handle, no usermode enumeration API.
- **Next**: **NtQuerySystemInformation** – iterate processes without opening a handle per process.
- **Worst**: **CreateToolhelp32Snapshot** – creates a handle per process during iteration, triggering EAC callback and logging.
- **Current project**: When the driver is loaded we use **EPROCESS first** (DotMem::get_pid_by_name); otherwise **NtQuerySystemInformation first**, then NtGetNextProcess / NTFS (both open handles when resolving name), **CreateToolhelp32Snapshot last**.

---

## 3. Alternatives We Use (Order and Handle Usage)

| Method | Order | Handle? | Notes |
|--------|-------|--------|-------|
| **EPROCESS walk (driver)** | 1st when driver loaded | No | Custom kernel walk (ActiveProcessLinks + ImageFileName); no usermode handle or enumeration API. |
| **NtQuerySystemInformation(SystemProcessInformation)** | 1st usermode / 2nd overall | No | Single buffer, walk in memory; no process handle per process; used by Task Manager/Process Explorer. |
| **NtGetNextProcess** | 3rd | Yes (per process) | Returns handle per process; then GetProcessId + path; can trigger EAC callback like Toolhelp32. |
| **NTFS FileProcessIdsUsingFileInformation** | 4th | Yes (for name) | Gets PIDs from NTFS device; then OpenProcess/QueryFullProcessImageName per PID to match name. |
| **CreateToolhelp32Snapshot** | Last | Yes (per process) | Creates handle per process during iteration → EAC callback + logging (module, size, entrypoint, etc.); last resort only. |

---

## 4. NtQuerySystemInformation (Alternative Not Tried Before)

### Why it’s a good alternative

- **Native API** in ntdll; lower level than kernel32 Toolhelp32.
- **Single call** (plus buffer-size probe) returns all processes; no per-process handle open like NtGetNextProcess.
- **Widely used** by Windows (Task Manager, Process Explorer); looks like normal system behavior.
- **Documented** (e.g. Microsoft, Geoff Chappell); structure layout is known for walking the list.

### How it works

- **Class**: `SystemProcessInformation` (5).
- **Flow**: Call with NULL/small buffer to get `STATUS_INFO_LENGTH_MISMATCH` and required size; allocate; call again; walk linked list via `NextEntryOffset` in `SYSTEM_PROCESS_INFORMATION`.
- **Per process**: `UniqueProcessId`, `ImageName` (UNICODE_STRING in the same buffer). Name can be after last backslash (e.g. `\...\FortniteClient-Win64-Shipping.exe`).
- **Caveat**: EAC/EDR can hook ntdll; direct syscall would bypass that (like we do for NtGetNextProcess). For this change we use the function via dynamic resolve (no IAT); direct-syscall version can be added later if needed.

### Implementation

- Added `find_process_sysinfo()` in `process_enum.hpp`: resolve `NtQuerySystemInformation` at runtime, allocate buffer, walk list, match by image name (case-insensitive).
- **Order**: When driver loaded, **EPROCESS** first (DotMem::find_process). Otherwise **NtQuerySystemInformation** (first, no handle) → NtGetNextProcess → NTFS → CreateToolhelp32Snapshot (last; handle per process → EAC callback).

---

## 5. Detection / Stealth Comparison (High Level)

| Vector | Toolhelp32 | NtQuerySystemInformation | EPROCESS (driver) | NTFS | NtGetNextProcess |
|--------|------------|---------------------------|-------------------|------|-------------------|
| IAT / import | Yes (if linked) | No (dynamic resolve) | N/A (kernel) | No | No |
| Well-known “cheat” API | Yes | Less so | Custom | Rare | Less so |
| Hooks | Often monitored | Can be hooked in ntdll | N/A | NtOpenFile/NtQueryInformationFile | Often hooked; we use direct syscall |
| Handle per process | Yes (EAC callback) | No | No | OpenProcess for name | Yes |
| EAC logs event + info | Yes | No | No | When opening for path | Yes |

---

## 6. References

- Microsoft: CreateToolhelp32Snapshot, Process32First/Next, NtQuerySystemInformation (winternl).
- UnknownCheats: process enumeration, EAC bypass discussions (e.g. enumerate processes, kernel/userland).
- Geoff Chappell: SYSTEM_PROCESS_INFORMATION layout, SystemProcessInformation.
- tbhaxor / Niraj Kharel: process listing with NtQuerySystemInformation.
- LloydLabs: NTFS FileProcessIdsUsingFileInformation for PIDs.

---

## 7. Summary

- **Toolhelp32** creates a handle on each process during iteration → EAC callback and logging (event + module/size/entrypoint). Use last only.
- **NtQuerySystemInformation** returns all processes in one buffer; we walk it in memory – **no process handles**.
- **EPROCESS (driver)** – custom kernel walk; no usermode handle or enumeration API; we use it first when the driver is loaded.
- **Order in code**: EPROCESS (when driver loaded) → NtQuerySystemInformation → NtGetNextProcess → NTFS → CreateToolhelp32Snapshot (fallback).
