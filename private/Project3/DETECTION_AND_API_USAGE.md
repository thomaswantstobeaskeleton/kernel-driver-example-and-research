# Detection Risk and Windows API Usage

Summary: what could be detected, which **documented** APIs we use as **first or second fallback** only, and a full list of Windows API usage.

---

## 1. Methods That Could Currently Be Detected

| Method / API | Where used | Detection risk | Why |
|--------------|------------|----------------|-----|
| **NtQuerySystemInformation** | process_enum find_process_sysinfo (1st usermode); debugger-detection SystemKernelDebuggerInformation; kdmapper; spoofer_hooks | **Lower** | No process handle per process; single buffer. Can still be hooked in ntdll. |
| **NTFS enum** (NtOpenFile + NtQueryInformationFile) | process_enum enum_pids_ntfs; find_process_ntfs | **Lower** | Uncommon path; then we open handles when resolving name (get_process_path). |
| **CreateFileW** (driver open) | driver.hpp open (section/registry path); NTFS fallback in process_enum | **Medium** | Monitored; we prefer NtOpenFile, CreateFileW only as fallback. |
| **DeviceIoControl** | driver.hpp when FLUSHCOMM_USE_FLUSH_BUFFERS=0 (legacy IOCTL) | **Medium** | Well-known driver comm; we use NtFlushBuffersFile (custom_nt) by default. |
| **VirtualAlloc** | direct_syscall stub (replaced with custom_nt::alloc + NtProtectVM); debugger-detection; kdmapper | **Medium** | Driver registry fallback now uses custom_nt::alloc (no VirtualAlloc in that path). |
| **RegCreateKeyExW / RegSetValueExW / RegQueryValueExW / RegOpenKeyExW** | driver.hpp handshake; kdmapper service | **Medium** | Driver/cheat install pattern. |
| **CreateFileMappingW / MapViewOfFile** | driver.hpp section-based comm | **Lower** | Common; section name/path derived from MAGIC. |
| **EPROCESS walk** (driver) | process_list_kernel.hpp; DotMem::get_pid_by_name | **Lowest** | Custom kernel walk; no usermode process handle or documented enum API. |

---

## 2. Documented APIs Used Only as 1st or 2nd Fallback (Process Find)

We use **two** process-find strategies as “first” or “second” fallback:

- **1st fallback (when driver loaded):** kernel **EPROCESS list walk** (ActiveProcessLinks + ImageFileName). This is **not** a documented Windows API; it uses kernel internals and our own offsets.
- **2nd fallback (when driver not loaded):** **NtQuerySystemInformation** (class `SystemProcessInformation`).

So the **only documented API** we use as first or second fallback for process finding is:

| Documented API | Role | Doc source |
|----------------|------|------------|
| **NtQuerySystemInformation** | 2nd fallback (1st in usermode) for finding process by name. Single buffer, no process handles. | winternl.h / ntdll; Microsoft docs (e.g. SystemProcessInformation). |

All other process-enum methods (NtGetNextProcess, NTFS) are used **after** these two. Process path and handle close use **custom NT-only** wrappers (NtQueryInformationProcess ProcessImageFileName, NtOpenProcess, NtClose); no OpenProcess, QueryFullProcessImageNameW, CloseHandle, Sleep, FlushFileBuffers, or Toolhelp32 in the main chain.

---

## 3. All Windows API Usage (Project-Wide)

### Process enumeration and path (utilities/process_enum.hpp, direct_syscall, custom_nt, entrypoint)

| API / wrapper | Use |
|---------------|-----|
| NtQuerySystemInformation (direct syscall) | find_process_sysinfo (1st usermode); find_process_stealth and enumerate_stealth use **sysinfo only** (no process handles). |
| custom_nt (NtOpenProcess, ProcessImageFileName, NtClose, NtAllocateVirtualMemory, NtFreeVirtualMemory, NtProtectVirtualMemory, NtDelayExecution, NtFlushBuffersFile) | get_process_path, get_process_id, close_handle, alloc/free_mem, protect_exec, delay_ms, flush_handle – no kernel32/psapi in hot path. |
| NtGetNextProcess / find_process_nt / enum_pids_ntfs | Available for explicit use; **not** in find_process_stealth or enumerate_stealth (EAC: no handle-based enum in primary path). |
| NtOpenFile | NTFS enum (open \\.\Ntfs or \??\... paths). |
| NtQueryInformationFile | NTFS FileProcessIdsUsingFileInformation. |
| NtCreateFile | Typedef only in process_enum (NTFS). |
| CreateFileW | NTFS enum fallback (\\.\Ntfs) when NtOpenFile fails. |
| RtlInitUnicodeString / InitializeObjectAttributes | NTFS and NtOpenProcess setup (winternl-style). |

### Module / export resolution (utilities/api_resolve.hpp, kdmapper api_resolve)

| API | Use |
|-----|-----|
| GetModuleHandleW | api_resolve fallback when PEB walk fails. |
| GetProcAddress | api_resolve fallback when PEB export parse fails. |
| (PEB walk + PE export parse) | Preferred; no GetModuleHandle/GetProcAddress when successful. |

### Driver communication (utilities/impl/driver.hpp)

| API | Use |
|-----|-----|
| CreateFileW | Open driver device (prefer NtOpenFile when available). |
| NtOpenFile | Preferred over CreateFileW for device open. |
| custom_nt::flush_handle (NtFlushBuffersFile) | Request signaling (primary when FLUSHCOMM_USE_FLUSH_BUFFERS=1); no FlushFileBuffers. |
| DeviceIoControl | Legacy IOCTL path when not using flush. |
| NtDeviceIoControlFile | Direct syscall for IOCTL/PING when used. |
| CreateFileMappingW | Section-based shared memory. |
| MapViewOfFile | Map section. |
| custom_nt::alloc | Registry fallback buffer (no VirtualAlloc); stub alloc via custom_nt::alloc + protect_exec. |
| RegCreateKeyExW / RegSetValueExW / RegQueryValueExW / RegOpenKeyExW | Handshake / section registration. |
| custom_nt::close_handle (NtClose) | Driver handle where used in our code paths. |
| OpenProcessToken / GetCurrentProcess | Token query (e.g. admin check). |
| GetCurrentProcessId | Registry write. |
| GetTickCount64 | Throttle / jitter (timing only). |
| custom_nt::delay_ms (NtDelayExecution) | Retry and throttle; no Sleep. |

### Other usermode (entrypoint, protections, kdmapper, framework)

| API | Use |
|-----|-----|
| NtOpenProcess | Epic/Fortnite process terminate (runtime resolve). |
| NtQueryInformationProcess | kdmapper getParentProcess (ProcessBasicInformation only; no Toolhelp32). |
| RegOpenKeyExA / RegQueryValueExA | entrypoint (e.g. BIOS manufacturer). |
| RegOpenKeyExW / RegQueryValueExW | debugger-detection (e.g. BIOS). |
| custom_nt::alloc + protect_exec | direct_syscall stub (no VirtualAlloc/VirtualProtect). VirtualAlloc/VirtualProtect elsewhere: debugger-detection buffer; entrypoint guard page. |
| NtQuerySystemInformation | debugger-detection (SystemKernelDebuggerInformation). |
| CreateFileW / DeviceIoControl / VirtualAlloc / CloseHandle | kdmapper (intel_driver, eneio_driver, etc.). |
| RegCreateKeyW / RegOpenKeyW | kdmapper service. |
| GetModuleHandleA / GetProcAddress | kdmapper kdm_api_resolve (PEB first, then fallback). |
| LoadLibraryA / GetProcAddress | imgui_impl_win32 (XInput). |

### Kernel (driver)

Kernel usage is NT-style and internal (e.g. Zw*/Nt*, Ps*, Mm*, Ob*, RtlInitUnicodeString, Io*, Ke*). Not listed here as “Windows API” in the usermode sense; see driver sources and WDK headers.

---

## 4. Order of Process-Find Fallbacks (Reminder)

1. **EPROCESS walk** (driver) – custom; no documented API.
2. **NtQuerySystemInformation(SystemProcessInformation)** – direct syscall; **only** usermode path for find/enumerate (no NtGetNextProcess, no NTFS in stealth path).
3. *(find_process_nt / NTFS / Toolhelp32 not used in find_process_stealth or enumerate_stealth; path/close/alloc/delay/flush/protect use custom_nt.)*

---

## 5. Research: UnknownCheats & Up-to-Date Assessment

Research (UnknownCheats, EDR/AC writeups, 2022–2025) on whether our **custom NT / direct-syscall** approach is **better and more undetected** than public APIs:

### Process enumeration

| Topic | Finding | Source / date |
|-------|--------|----------------|
| **CreateToolhelp32Snapshot** | High detection risk: creates a handle per process, triggers process-access callbacks (e.g. Sysmon 10, EAC). Well-known cheat/malware API. | UC process-enum threads; PEAs “Process Enumeration Alternatives”; malware/EDR writeups. |
| **NtQuerySystemInformation(SystemProcessInformation)** | **Preferred over Toolhelp32**: single buffer, **no process handles** → does not trigger ProcessAccess / handle-based callbacks. Stealthier for EDR/AC. | UC (e.g. enumerate processes thread); TheWover gist (get image name without opening handle); cocomelonc/malware tricks 2025. |
| **Direct syscall for NtQuerySystemInformation** | Bypasses **ntdll hooks**; ACs hook Nt* in ntdll, so calling through ntdll is visible. Our direct syscall does not go through hooked ntdll. | UC direct syscall / EAC bypass threads; White Knight Labs LayeredSyscall (2024). |

**Verdict:** Using **NtQuerySystemInformation** (and our **direct syscall** for it) as first usermode fallback and **removing Toolhelp32** is **better and more undetected** than CreateToolhelp32Snapshot + Process32FirstW/NextW.

### Direct syscalls vs ntdll hooks

| Topic | Finding | Source / date |
|-------|--------|----------------|
| **ntdll hooks** | EAC and similar ACs hook Nt* in ntdll. Calling NtOpenProcess, NtClose, etc. through ntdll can be logged or blocked. | UC “EAC’s Instrumentation Callback Bypass” (2022); UC “Instrumentation callbacks / Syscall callbacks” (2018, still referenced Aug 2024). |
| **Direct syscalls** | Bypass usermode hooks by not calling through ntdll; syscall runs in kernel. | UC; “Bypassing User-Mode Hooks and Direct Invocation of System Calls” (red team papers). |
| **Instrumentation callbacks (ICB)** | Some ACs use ICB to inspect syscall return address. If return address is **not** in ntdll (e.g. from our stub), it can be flagged. UC: “it still won’t catch any **manual** syscalls since it relies on the return address to be in ntdll.” | UC Skyfail (2018); EAC ICB bypass thread (2022). |
| **Indirect syscalls** | Jumping to the `syscall` instruction **inside** ntdll gives a “legitimate” return address; 2024 techniques (e.g. LayeredSyscall) combine this with callstack spoofing. | White Knight Labs 2024; UC. |

**Verdict:** **Direct syscalls** (and our custom_nt runtime resolution from ntdll without IAT) are **better than calling documented kernel32/psapi APIs** and avoid ntdll **hook** execution. For ACs that use ICB + return-address checks, indirect syscalls or callstack spoofing are the next step; our current approach is still an improvement over OpenProcess, CloseHandle, Sleep, FlushFileBuffers, etc.

### Custom NT replacements (NtClose, NtDelayExecution, NtFlushBuffersFile, NtAllocateVirtualMemory, etc.)

| Topic | Finding | Source / date |
|-------|--------|----------------|
| **Sleep vs NtDelayExecution** | Sleep is a kernel32 wrapper around NtDelayExecution. Using NtDelayExecution via GetProcAddress(ntdll) avoids kernel32 in the call path and IAT; listed as timing evasion in sandbox/evasion docs. | Unprotect Project; evasions.checkpoint.com (timing). |
| **CloseHandle vs NtClose** | Same idea: no CloseHandle in IAT or stack; reduces documented API surface and hook points. | Standard NT bypass practice; UC syscall discussions. |
| **FlushFileBuffers vs NtFlushBuffersFile** | Driver comm via FlushFileBuffers is a known pattern; using NtFlushBuffersFile (runtime resolve or syscall) avoids kernel32. | Consistent with “use NT layer instead of Win32” on UC. |
| **HeapAlloc vs NtAllocateVirtualMemory** | HeapAlloc/VirtualAlloc are common in IAT and often monitored. NtAllocateVirtualMemory / NtFreeVirtualMemory via ntdll (or syscall) reduces Win32 footprint. | UC and malware-dev sources on “no kernel32 in hot path.” |

**Verdict:** Replacing **Sleep**, **CloseHandle**, **FlushFileBuffers**, and **HeapAlloc/HeapFree** with **custom_nt** (NtDelayExecution, NtClose, NtFlushBuffersFile, NtAllocateVirtualMemory/NtFreeVirtualMemory) is **better and more undetected** than using the public APIs, especially with runtime resolution (no IAT) and, where we have it, direct syscall.

### Summary

- **Process enum:** NtQuerySystemInformation (direct syscall) + no Toolhelp32 is **better and more undetected** than CreateToolhelp32Snapshot.
- **Handles / memory / delay / flush:** Custom NT wrappers (NtClose, NtDelayExecution, NtFlushBuffersFile, NtAllocateVirtualMemory/NtFreeVirtualMemory) with runtime resolve (and direct syscall where implemented) are **better and more undetected** than CloseHandle, Sleep, FlushFileBuffers, HeapAlloc/HeapFree.
- **Caveat:** Anti-cheats that use **instrumentation callbacks** and **return-address / callstack** checks can still infer “unusual” syscall origin. Our approach is a clear improvement over documented APIs; further hardening (e.g. indirect syscalls, callstack spoofing) is optional and not required for the “use custom, non-public methods” goal.

*Sources: unknowncheats.me (process enumeration, EAC ICB bypass, instrumentation callbacks, direct syscalls); Unprotect/evasion docs (NtDelayExecution); White Knight Labs (LayeredSyscall 2024); TheWover/cocomelonc (NtQuerySystemInformation without handles).*

**EAC-specific:** See **EAC_DETECTION_AND_REMEDIATION.md** for EAC current detection/heuristics, “can still be detected” → custom fixes (sysinfo-only enum, custom_nt alloc/protect, registry NT optional, indirect syscall optional).
