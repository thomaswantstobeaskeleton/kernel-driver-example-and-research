# EAC Current Detection, Heuristics, and Custom/Non-Documented Remediation

Research from **UnknownCheats** and up-to-date sources (2022–2025) on Easy Anti-Cheat detection, what gets flagged, and how to replace all "can still be detected" usage with **custom, non-public, non-documented** methods.

---

## 1. EAC Current Build & Detection Vectors (UC, Up-to-Date)

### 1.1 Usermode

| Vector | What EAC does | UC / source |
|--------|----------------|-------------|
| **ntdll hooks** | Hooks Nt* in ntdll; calls through ntdll are logged or blocked. | UC 561479 (EAC ICB bypass), UC 563804 (syscall hooking), UC 678400 (undetected injector Dec 2024). |
| **Instrumentation callback (ICB)** | Registers ProcessInstrumentationCallback; inspects **return address** when syscall returns. If return address is **not** in ntdll (e.g. from direct syscall stub), can flag. | UC 561479, UC 253247 (instrumentation callbacks, last post Aug 2024). |
| **Process handle creation** | Callbacks on process handle open (similar to Sysmon 10). CreateToolhelp32Snapshot, NtGetNextProcess (one handle per process), OpenProcess → all trigger. | UC 424340, PEAs / NtQuerySystemInformation alternatives; EDR writeups. |
| **CreateToolhelp32Snapshot** | Well-known cheat/malware API; heavily monitored. | PROCESS_ENUM_EAC_RESEARCH.md; UC process enum threads. |
| **Reg* / CreateFileW / VirtualAlloc** | Driver/cheat install and comm patterns; registry + VirtualAlloc path **documented as detected on EAC** in project config. | Project DETECTION_AND_API_USAGE.md; UC 590262 (encrypt syscalls, kernel/external). |
| **MinHook / Detours / pointer swap** | EAC detects common hook libs; MH_ERROR_MEMORY_PROTECT and non-functional hooks when EAC enabled. | UC 435413 (hook in-game with EAC). |

### 1.2 Kernel

| Vector | What EAC does | UC / source |
|--------|----------------|-------------|
| **Integrity checks** | Kernel driver integrity (LoadImage/CreateProcess notification); compares .sys to copy; 10–40 s or instant (e.g. Rust) kick if patched. Uses obfuscated code + possible virtualized code. | UC 609412 (bypass EAC integrity checks, Nov 2023); Swiftik: VM, MmCopyMemory, SHA1, detours. |
| **NtQuerySystemInformation** | EAC uses SystemCodeIntegrityInformation, SystemKernelDebuggerInformation; can hook or monitor from kernel. | UC 410088 (debug EasyAntiCheat.sys). |
| **PsIsProcessBeingDebugged** | Debugger checks. | UC 410088. |
| **Unsigned drivers** | Scans for unsigned drivers; DseFix-style load blocked (e.g. 30007). | UC 635703 (May 2024). |
| **MmUnloadedDrivers / PiDDB** | Looks for vuln driver names; must clear when using manual mapping. | EAC_DETECTION_RESEARCH_UC.md; UC 438804. |

### 1.3 Heuristics & Flagging (UC Consensus)

- **Syscall origin**: Return address / call stack not in ntdll → suspicious; **indirect syscall** (jump to syscall instruction **inside** ntdll) gives legitimate-looking return address. (LayeredSyscall 2024: VEH + indirect syscall for callstack spoofing.)
- **Process enumeration**: Prefer **no process handles** (NtQuerySystemInformation with SystemProcessInformation); avoid NtGetNextProcess / Toolhelp32 for primary path.
- **Encryption**: UC recommends encrypting syscall traffic (e.g. AES-256) for kernel–usermode comm; avoid plain IOCTL.
- **No public patterns**: Literal API names, public syscall stub bytes, single fixed obfuscation keys → sig/heuristic risk.

---

## 2. “Can Still Be Detected” → Custom/Non-Documented Fix

### 2.1 NtGetNextProcess (process handle per call)

| Current | Risk | Remediation (custom / non-public) |
|---------|------|-----------------------------------|
| find_process_nt, enumerate_stealth use NtGetNextProcess; each iteration opens a process handle. | EAC/EDR process handle callbacks. | **Do not use for primary path.** Use **NtQuerySystemInformation(SystemProcessInformation)** only: single buffer, **no process handles**. find_process_stealth = kernel (when driver) then **sysinfo only**. enumerate_stealth = **walk sysinfo buffer** and callback(pid, imageName); no NtGetNextProcess, no NTFS. Keep find_process_nt / find_process_ntfs only for explicit fallbacks outside “stealth” path. |

**Implementation:** process_enum.hpp: find_process_stealth calls only find_process_sysinfo (after kernel); enumerate_stealth implemented via sys_NtQuerySystemInformation + buffer walk, no NtGetNextProcess or NTFS.

### 2.2 NtQuerySystemInformation “can still be hooked in ntdll”

| Current | Risk | Remediation |
|---------|------|-------------|
| We use **direct syscall** for NtQuerySystemInformation (direct_syscall.hpp). | If we called through ntdll, hooks would run. | **Already custom:** We use **direct syscall** (SSN + stub); no ntdll call path. Optional: **indirect syscall** (jump to ntdll’s syscall instruction) so return address is inside ntdll and ICB sees “legitimate” stack. |

### 2.3 Instrumentation callback (return address not in ntdll)

| Current | Risk | Remediation |
|---------|------|-------------|
| Direct syscall from our stub → return address in our module. | EAC ICB can flag non-ntdll return address. | **Indirect syscall:** Resolve ntdll’s Nt* function, find `syscall; ret` (or equivalent), **jmp** to that instruction with same SSN/args. Return address then in ntdll. (UC pkowner646, LayeredSyscall 2024.) Optional follow-up: callstack spoofing / VEH. |

### 2.4 CreateFileW (driver open / NTFS fallback)

| Current | Risk | Remediation |
|---------|------|-------------|
| NtOpenFile preferred; CreateFileW only when NtOpenFile fails. | CreateFileW is monitored. | Keep NtOpenFile as only path where possible; avoid CreateFileW in hot path. If section open fails, document that CreateFileW fallback is higher risk. |

### 2.5 DeviceIoControl

| Current | Risk | Remediation |
|---------|------|-------------|
| Used when FLUSHCOMM_USE_FLUSH_BUFFERS=0. | Well-known driver comm. | Prefer **NtFlushBuffersFile** (custom_nt) only; already done. Keep DeviceIoControl/NtDeviceIoControlFile as opt-in legacy. |

### 2.6 VirtualAlloc (registry fallback + direct_syscall stub)

| Current | Risk | Remediation |
|---------|------|-------------|
| driver.hpp: g_shared_buf = VirtualAlloc when section fails. direct_syscall: alloc_executable uses VirtualAlloc + VirtualProtect. | EAC detects “registry + VirtualAlloc” path; VirtualAlloc in IAT/call stack. | **Custom:** Use **NtAllocateVirtualMemory** (custom_nt::alloc) for shared buffer. For stub: **NtAllocateVirtualMemory** + **NtProtectVirtualMemory** (runtime resolve) instead of VirtualAlloc/VirtualProtect. No kernel32 in these paths. |

### 2.7 RegCreateKeyExW / RegSetValueExW / RegQueryValueExW / RegOpenKeyExW

| Current | Risk | Remediation |
|---------|------|-------------|
| driver.hpp handshake; kdmapper service. | Driver/cheat install pattern; documented APIs. | **Custom:** **NtOpenKey**, **NtQueryValueKey**, **NtSetValueKey**, **NtCreateKey** (ntdll, runtime resolve). Path format: `\Registry\Machine\...` for HKLM. Implement in custom_nt.hpp and use in driver handshake so no Reg* in call stack. |

### 2.8 CreateFileMappingW / MapViewOfFile

| Current | Risk | Remediation |
|---------|------|-------------|
| Section-based driver comm. | Common but less hook-heavy than Reg* / VirtualAlloc. | **Optional:** **NtOpenSection** + **NtMapViewOfSection** (ntdll); same semantics, no kernel32. Larger refactor; lower priority than Reg* and VirtualAlloc. |

---

## 3. Implementation Status (This Project)

| Item | Status |
|------|--------|
| Process find: sysinfo-only (no NtGetNextProcess in find_process_stealth) | Done |
| Enumerate: sysinfo buffer walk only (no NtGetNextProcess in enumerate_stealth) | Done |
| Driver shared buffer: VirtualAlloc → custom_nt::alloc | Done |
| direct_syscall alloc_executable → NtAllocateVirtualMemory + NtProtectVirtualMemory | Done |
| Registry: Reg* → NtOpenKey / NtQueryValueKey / NtSetValueKey / NtCreateKey | Documented; optional next step |
| Indirect syscall (return address in ntdll) | Documented; optional for ICB-heavy targets |
| CreateFileW | NtOpenFile preferred; CreateFileW fallback only |
| CreateFileMappingW/MapViewOfFile → NtOpenSection/NtMapViewOfSection | Optional; lower priority |

---

## 4. References (UnknownCheats, Up-to-Date)

- **UC 561479** – EAC’s Instrumentation Callback Bypass (Dec 2022); indirect syscall suggestion (pkowner646).
- **UC 253247** – Instrumentation callbacks / syscall callbacks (Jan 2018; active Aug 2024); direct syscalls not caught when ICB checks for ntdll return address (Skyfail).
- **UC 678400** – Undetected injector EAC (22.12.2024).
- **UC 590262** – Steps to bypassing EAC kernel/external; encrypt syscalls.
- **UC 609412** – Bypass EasyAntiCheat integrity checks (Nov 2023); kernel integrity, ZwQuerySystemInformation SystemBigPoolInformation.
- **UC 424340** – Enumerate processes; ZwQuerySystemInformation SystemProcessInformation (kernel); NtQuerySystemInformation usermode.
- **UC 635703** – EAC scanning unsigned drivers (May 2024).
- **UC 438804** – EAC detect kernel drivers bypasses; MmUnloadedDrivers, PiDDB.
- **White Knight Labs** – LayeredSyscall (2024): VEH + indirect syscall, callstack spoofing.
- **Project** – DETECTION_AND_API_USAGE.md, PROCESS_ENUM_EAC_RESEARCH.md, EAC_DETECTION_RESEARCH_UC.md.
