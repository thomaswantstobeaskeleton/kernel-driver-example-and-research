# Anti-Detection: Private/Non-Public Methods for EAC/Fortnite

Replacements for publicly spread implementations. No xorstr, skCrypt, system(tasklist), LloydLabs NTFS-first, etc.

---

## 1. String Obfuscation

**Removed:** xorstr, skCrypt (publicly spread, signatured)

**Replaced with:** `OBF_STR()` / `str_obfuscate.hpp`
- Key from `FLUSHCOMM_OBF_BASE` (MAGIC-derived)
- Formula: `c^(K+(i%17))` – differs from xorstr (__TIME__) and skCrypt
- No literal keys in .rdata; unique per build

---

## 2. Debugger Detection

**Removed:** `system("tasklist /m x64dbg.dll")`, `IsDebuggerPresent()`, `CheckRemoteDebuggerPresent()`, `GetThreadContext` DR0–DR3, `FindWindowA` – all heavily hooked/signatured

**Replaced with:**
- **is_debugger_or_tool_detected()** – `nt_debugger_attached()` (NtQueryInformationProcess class 7/30) + `debugger_window_present()` (EnumWindows + GetWindowTextW)
- **entrypoint IsDebuggerPresentAdvanced** – thin wrapper calling `is_debugger_or_tool_detected()` only
- **InitializeProtection:** `TerminateProcess(GetCurrentProcess())` – silent exit, no MessageBoxA (monitored)
- **debugger_check_decl.hpp** – minimal forward decl; full logic in `debugger_check.cpp` (avoids include-order issues)

---

## 3. Process Enumeration

**Deprioritized:** LloydLabs NTFS FileProcessIds (public blog, known pattern)

**New order:**
1. NtQuerySystemInformation(SystemProcessInformation) – native, no Toolhelp32
2. NtGetNextProcess (direct syscall)
3. NTFS FileProcessIds – fallback only
4. CreateToolhelp32Snapshot – last resort

**NTFS paths (`utilities/process_enum.hpp`):** No literal "Ntfs"/"Device"/"GLOBALROOT" in binary. Tokens encoded with `APIRES_OBF_KEY`; paths `\??\GLOBALROOT\Device\Ntfs`, `\Device\Ntfs`, `\NTFS\`, and CreateFileW fallback `\\.\Ntfs` are built at runtime from decoded tokens.

---

## 4. IPC / Shared Memory

**Primary:** File-backed section (`FLUSHCOMM_USE_FILEBACKED_SECTION=1`)
- No named object in `\BaseNamedObjects\Global\*`
- Path: `%SystemRoot%\Temp\Fx<12hex>.tmp` (MAGIC-derived)
- See `FILEBACKED_IPC_DESIGN.md`

---

## 5. Existing Custom / Less Public

- **direct_syscall:** SSN from ntdll prologue; stub bytes obfuscated via FLUSHCOMM_OBF_BASE
- **api_resolve:** PEB-based module/proc resolution first; fallback to GetModuleHandleW/GetProcAddress. Obfuscated strings (APIRES_OBF_A/W). Bypasses kernel32 hooks.
- **PEB-based API resolution:** Walk PEB->Ldr->InMemoryOrderModuleList for module base; parse PE export table for proc. No GetProcAddress/GetModuleHandle in critical path.
- **trace_cleaner:** Driver names + pattern bytes XOR-encoded; no literals
- **FlushFileBuffers** handshake: No IOCTL; section + IRP hook

---

## 6. VM / Tool Detection (Registry, No WMI)

- **bios_or_baseboard_vm_string_detected:** Registry-based (HKLM\HARDWARE\DESCRIPTION\System\BIOS) – **no WMI**. WMI (CoCreateInstance, IWbemServices) is heavily monitored. Same data via RegQueryValueEx.
- **vmDetection::isVM:** Silent exit (TerminateProcess). No MessageBox, no system(). Removed dangerous `system("taskkill /F /IM svchost.exe")`.
- **entrypoint IsVirtualMachine:** Registry path and VM strings via OBF_STR
- **cpu_known_vm_vendors / vm_mac_prefix_detected:** XOR-encoded vendor strings and MAC prefixes (key 0xA7)
- **Tool hash checks:** FileGrab, Process Hacker, etc. via OBF_STR
- **Trusted DLL paths:** Obfuscated in isTrustedDllPath

---

## 7. Debugger / Window / Memory (Custom Methods)

- **debugger_window_present:** EnumWindows + GetWindowTextW (resolved via api_resolve) – avoids FindWindowA (common debugger check, often hooked). Case-insensitive substring match.
- **DetectManualMappedDll:** NtQueryVirtualMemory (direct syscall) – avoids VirtualQuery (kernel32 path, often monitored).
- **GetSelfModuleName:** PEB-based (peb_modules::get_self_module_name) – walks PEB->Ldr for ImageBaseAddress; no GetModuleFileNameA.
- **detectInjectedDlls:** No system("cls") – removed cmd spawn.
- **clear_console_no_spawn():** GetStdHandle + FillConsoleOutputCharacter + SetConsoleCursorPosition – no system("cls") spawning cmd.exe. Used in back(), ShowWelcomeMessage().
- **terminate_epic_fortnite_processes:** `enumerate_processes_nt` (NtGetNextProcess direct syscall) – no CreateToolhelp32Snapshot/Process32First/Next. Path basename extraction for exe name match.
- **spoofer_hooks:** Trampoline encoding `mov rax,addr; jmp rax` (48 B8 FF E0) – avoids 0xFF 0x25 (jmp [rip+0]) pattern commonly signatured.

---

## 8. Win11 23H2 / API Replacements

- **GetAdaptersInfo** for MAC-based VM detection (no winsock2 dep; works on Win11 23H2). Resolved via api_resolve. PIP_ADAPTER_INFO avoids winsock2/PIP_ADAPTER_ADDRESSES include-order issues.
- **file_hash_nt.hpp:** NtOpenFile + NtReadFile + inline SHA256. No CreateFileA, no CryptoAPI (CryptAcquireContext/CryptCreateHash). Custom implementation, no crypt32/advapi32 in hash path.
- **PEB/LDR offsets:** Verified stable for Win10/11 23H2 x64 (0x60 TEB, 0x10 ImageBase, 0x20 InMemoryOrder, 0x30 DllBase, 0x48 FullDllName).

---

## 9. Custom Anti-Debug Techniques (Non-Public, Original)

### 9a. NtSetInformationThread(ThreadHideFromDebugger) via direct SSN
- Extracts syscall number from ntdll prologue at runtime
- Builds executable stub (4C 8B D1 B8 xx xx xx xx 0F 05 C3) in RW->RX memory
- Never calls the ntdll export → hooks on NtSetInformationThread are bypassed
- Class 0x11 makes the calling thread invisible to attached debuggers
- Called once at startup (`init_custom_anti_debug`)

### 9b. Kernel Debugger Detection (SystemKernelDebuggerInformation)
- NtQuerySystemInformation class (0x20+3) → SYSTEM_KERNEL_DEBUGGER_INFORMATION; class value from expression (no literal 0x23 in .rdata)
- Checks `DebuggerEnabled && !DebuggerNotPresent`
- Catches WinDbg kernel-mode, HyperDbg, etc.
- Not commonly used in user-mode anti-cheat (non-public)

### 9c. DebugObject Type Enumeration
- NtQueryObject(NULL, ObjectAllTypesInformation) → scans all kernel object types
- Looks for "DebugObject" type with TotalNumberOfObjects > 0
- If any DebugObject exists system-wide, a debugger is likely attached
- Hard to spoof: requires kernel object manager manipulation

### 9d. ProcessInstrumentationCallback Detection
- NtQueryInformationProcess class (0x20+8) → PROCESS_INSTRUMENTATION_CALLBACK_INFORMATION; class value from expression (no literal 0x28 in .rdata)
- Returns non-null Callback when advanced debuggers set instrumentation callbacks
- Catches debuggers that use ETW/instrumentation for tracing

### 9e. .text Section Self-Integrity (FNV-1a)
- On first call: hashes first 16KB of executable .text section using FNV-1a
- Subsequent calls: compares current hash to baseline
- Detects software breakpoints (0xCC/int3) and runtime patches
- No crypto dependency (custom FNV-1a, not CRC32/SHA)

### 9f. RDTSC Timing Anti-Debug
- Uses `__rdtscp` (TSC register read) instead of QueryPerformanceCounter
- TSC is read directly in ring-3, less commonly intercepted than QPC
- Threshold 6 billion cycles – generous for 1–5 GHz CPUs
- Single-step / breakpoint causes TSC gap 10x+ above normal

---

## 10. Codebase Hardening (Public API Removal)

| File | What was removed | Replaced with |
|------|-----------------|---------------|
| `render.h` | `GetModuleHandleA(NULL)` ×2 | `peb_modules::get_image_base_address()` |
| `render.h` | `MessageBoxA` ×3, hardcoded FindWindowA strings | Removed / OBF_STR |
| `imgui_impl_win32.cpp` | `GetModuleHandleA("ntdll.dll")`, `LoadLibraryA`, `GetProcAddress` | `api_resolve` PEB-based; XInput DLL/proc names (xinput1_4.dll, XInputGetCapabilities, etc.) via APIRES_OBF_A – no literal in primary path |
| `entrypoint.cpp` | `GetModuleHandle(nullptr)` ×2 (VerifyCodeIntegrity, ProtectMemoryRegions) | `peb_modules::get_image_base_address()` – no GetModuleHandle in IAT |
| `entrypoint.cpp` | xor_encrypt key `0x42` (public literal), debugger names | **FLUSHCOMM_OBF_BASE**-derived `_DBG_NAME_KEY`; debugger names (ollydbg, ida64, x64dbg, cheatengine) encoded at compile time with same key – no literal `0x42` in .rdata |
| `process_enum.hpp` | `OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, ...)` in get_process_path | **NtOpenProcess** resolved at runtime via api_resolve; used in get_process_path with CLIENT_ID/OBJECT_ATTRIBUTES – OpenProcess only as fallback (no IAT in primary path) |
| `includes.h` | `GetModuleFileNameA` ×2, `MessageBoxA` | `peb_modules::get_self_module_name()`, silent exit |
| `console.h` | `system("taskkill /F /IM NVIDIA Share.exe")` | `TerminateProcess(GetCurrentProcess())` |
| `entrypoint.cpp` | QPC timing anti-debug, `MessageBoxA`, hardcoded `FindWindowA` | RDTSC, `console.error`, `OBF_STR` |
| `Auth/skStr.h`, `protections/skStr.h`, `framework/xorstr.hpp` | Full skCrypt / _xor_ implementations | Gutted (use `OBF_STR`/`OBF_WSTR`) |

---

## 11. Driver / Mapper / External (Non-Public Methods)

### Usermode driver communication (`utilities/impl/driver.hpp`)
- **DRV_OBF_KEY:** Now uses `FLUSHCOMM_OBF_BASE` (MAGIC-derived) instead of hardcoded `0x9D` – no single literal signature for device path decode (Beep, Null, PEAuth).
- **NtOpenFile** preferred over CreateFileW for opening hooked device – path built at runtime from same key.
- **Registry/section:** Paths built from `FLUSHCOMM_SECTION_SEED` + suffix; no "MdmTrace"/"WdfCtl" literals.

### Debugger / anti-debug (`protections/debugger-detection.hpp`)
- **hide_thread_from_debugger():** NtSetInformationThread(ThreadHideFromDebugger) stub is built from **obfuscated bytes** (same pattern as `direct_syscall.hpp`: `enc[i] ^ (STUB_OBF_KEY + i)`); no literal `4C 8B D1 B8 0F 05 C3` in .rdata. Uses `alloc_executable()` (RW then RX) to avoid RWX heuristics.

### Kernel driver (`driver/`)
- **Device/link names:** No hardcoded GUID in binary. Prefixes `\Device\` and `\DosDevices\Global\` encoded as **OBF_DevicePrefix** / **OBF_LinkPrefix** in routine_obfuscate.h – decoded at runtime; format string uses `%ws` + GUID from FLUSHCOMM_MAGIC so no literal `\Device\` or `\DosDevices\Global\` in .rdata. **create_driver.hpp:** Driver object name uses **OBF_DriverNamePrefix** (`\Driver\`) – decoded at runtime; no literal `\Driver\` in .rdata. Usermode uses Beep/Null/PEAuth (hooked), so kernel device name is build-specific only.
- **flush_comm_config.h:** IOCTL, MAGIC, section/registry names all derived from `FLUSHCOMM_MAGIC`/`FLUSHCOMM_OBF_BASE`; no public literals.
- **trace_cleaner.hpp:** Vuln driver names (iqvw64e, capcom, dsefix, etc.) encoded with `TRACE_OBF_KEY` (= FLUSHCOMM_OBF_BASE); decode at runtime.
- **flush_comm_obfuscate.h:** SharedBuffer, SharedPid, HookedDevice encoded with `OBF_KEY` = FLUSHCOMM_OBF_BASE.
- **routine_obfuscate.h:** All kernel routine names resolved via **MmGetSystemRoutineAddress** are encoded at compile time. **ZwQuerySystemInformation** resolved at runtime via **get_ZwQuerySystemInformation_fn()** – no IAT; used in fget_guarded_region (BigPool scan). (key ROUTINE_OBF_KEY = FLUSHCOMM_OBF_BASE) and decoded at runtime – no literal `"MmCopyVirtualMemory"`, `"ObCreateObject"`, `"HalPrivateDispatchTable"`, `"IoCompleteRequest"`, `"SystemRoot"`, `"LargePageDrivers"`, `"WskPort"`, `"WskRegister"`, etc. in .rdata. Pool tag comparison (`TnoC`) and LPG registry path/value, `\Driver\IDE`, `Driver` type name also encoded. **PsLookupProcessByProcessId** resolved at runtime via **get_system_routine_obf(OBF_PsLookupProcessByProcessId)** – no IAT import; **safe_PsLookupProcessByProcessId()** used in driver.cpp, flush_comm.cpp, icall_gadget.cpp (EAC hook bypass). **OBF_NtoskrnlExe**, **OBF_RegPathCurrentVersion** (CurrentVersion registry path), **OBF_MouHID**, **OBF_MouClass** – no literal `"ntoskrnl.exe"`, `\Registry\Machine\...\CurrentVersion`, `\Driver\MouHID`, `\Driver\MouClass` in .rdata; used in icall_gadget.cpp, flush_comm.cpp (file-backed section SystemRoot read), mouse_inject.hpp. **flush_comm.cpp** hex suffix built from character constants (no `L"0123456789abcdef"`). Win11 23H2 compatible.
- **FLUSHCOMM_USE_FLUSH_BUFFERS = 1:** No DeviceIoControl/IOCTL in handshake – FlushFileBuffers only (IOCTL not used as detection vector).
- **FLUSHCOMM_USE_SECTION = 1:** No MmCopyVirtualMemory; section-based shared memory (EAC UC 496628).
- **Pool tags:** **MAGIC-derived** (page_evasion.hpp) – evasion_tag_reg/list/copy/work computed from FLUSHCOMM_MAGIC so each build gets different 4-byte tags; no fixed public set (Fls/Cc/Io/Mm). Tags are driver-like (0x20–0x7E). Rotation (FLUSHCOMM_POOL_TAG_ROTATE) uses same MAGIC-derived formula for rot_tags[4..7]. **CREATE_DRIVER_POOL_TAG** in create_driver.hpp = create_driver_pool_tag() (no literal 'wDfc').
- **SystemBigPoolInformation:** Class value expressed as (0x40 | 2) – no literal 0x42 in binary for ZwQuerySystemInformation(BigPool) scan.
- **Trace cleaner – structure-based fallback:** Before the public LEA opcode scan (48 8D 0D / 4C 8D 05), **trace_clean_via_structure_probe()** runs: scans ntoskrnl **.data and .rdata only** for 8-byte pointers; validates each as a candidate UNLOADED_DRIVER_X64 array (Length/MaxLength/Buffer/StartAddress/EndAddress plausible, count ≤ 50). No LEA pattern in this path – custom, non-documented. Win11 23H2 compatible. If structure probe finds a valid array, LEA scan is skipped.

### Mapper (kdmapper)
- **Pool allocation:** **GetMapperPoolTag()** (kdm_obfuscate.hpp) – tag derived from XOR_KEY so no 'Vad '/'Vadv' literal (EAC/UC: public mapper tags known). Intel and Eneio backends use same custom tag. **ExAllocatePoolWithTag** / **ExFreePool** – resolved via **GetExAllocatePoolWithTagName()** and **GetExFreePoolName()** (kdm_api_resolve.hpp), encoded at compile time; no literal "ExAllocatePoolWithTag" or "ExFreePool" in .rdata (string scan).
- **Case number key:** CN_KEY = (0x12u + 0x95u) & 0xFF so no single 0xA7 byte in source. When flush_comm_config.h absent, KDM_OBF_KEY derived from __TIME__ (no 0x9D literal).
- **Service/registry (service.cpp):** No literal "SYSTEM\\CurrentControlSet\\Services\\", "ImagePath", "Type", "\\??\\", or "\\Registry\\Machine\\System\\CurrentControlSet\\Services\\" in .rdata – **kdm_obfuscate** provides **GetServicesKeyPrefix()**, **GetImagePathValueName()**, **GetTypeValueName()**, **GetNtPathPrefix()**, **GetRegistryDriverPathPrefix()** (decoded at runtime). **GetSeLoadDriverPrivilegeId()** returns 10 via (3+7) so no literal 10 for SE_LOAD_DRIVER_PRIVILEGE.
- **Kernel module/export names (intel_driver):** **GetKernelModuleNameWdFilter()**, **GetKernelModuleNameCi()** – no "WdFilter.sys" / "ci.dll" in .rdata. **GetExAcquireResourceExclusiveLiteName()**, **GetExReleaseResourceLiteName()**, **GetRtlDeleteElementGenericTableAvlName()**, **GetRtlLookupElementGenericTableAvlName()** – PiDDB/HashBucket export names encoded; intel_driver uses these for GetKernelModuleExport.
- **Random driver name (GetDriverNameW):** No literal "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789" – chars generated from rand() % 62 and single-char literals ('a','A','0').
- **kdm_api_resolve::GetNtdllModule():** **PebGetNtdll()** first – PEB walk (Ldr→InMemoryOrderModuleList, base name compare with decoded "ntdll.dll"); **GetModuleHandleA** only as fallback. Reduces IAT/hook surface for ntdll resolution.
- **getParentProcess():** Uses **NtQueryInformationProcess(ProcessBasicInformation)** first – reads `InheritedFromUniqueProcessId` for current process; no CreateToolhelp32Snapshot/Process32First/Next on success. Fallback to Toolhelp32 only if NT path fails (e.g. older OS). Win11 23H2 compatible.
- **NtQueryInformationProcess:** Resolved at runtime via **kdm_api_resolve::GetNtQueryInformationProcessProc()** – encoded name, no literal `"NtQueryInformationProcess"` in .rdata.
- **KDSymbolsHandler (PDB/offsets updater):** Runs updater via **CreateProcessW** (no `_wsystem` / cmd.exe spawn) – fewer process-tree traces.
- **includes.h LoadDriver():** Driver backend names for mapper command line built with **OBF_STR("intel")** / **OBF_STR("eneio")** – no literal `"intel"`/`"eneio"` in .rdata; retry message uses same decoded name.
- **kdm_obfuscate.hpp:** Device path, IOCTL, case numbers, ntoskrnl/ntkrnlmp names – all XOR-encoded; key from `FLUSHCOMM_OBF_BASE` when built with Project3, else `KDM_OBF_KEY` (no public 0x5A). **GetEneioDriverFileName()** and **GetMapperBackendNameIntel/Eneio()** – no literal `eneio64.sys` or `intel`/`eneio` in .rdata.
- **intel_driver:** Random driver name at runtime (no literal iqvw64e in usermode); resource/copy to temp.
- **eneio_driver:** Uses kdm_obfuscate for device path, driver filename (GetDriverPath), and CLI backend comparison; lazy import when `KDMAPPER_USE_LAZY_IMPORT` set.
- **service.cpp:** **RtlAdjustPrivilege**, **NtLoadDriver**, **NtUnloadDriver** resolved at runtime via **kdm_api_resolve** (GetRtlAdjustPrivilegeProc, GetNtLoadDriverProc, GetNtUnloadDriverProc) – no IAT entries for these names; encoded API names, no literals in .rdata.
- **Usermode driver.hpp:** Device path prefix built from character constants – no literal `L"\\\\.\\"` or `L"\\Device\\"` in .rdata; `get_device_path_w` / `get_device_native_w` write prefix char-by-char.
- **entrypoint (terminate_epic_fortnite_processes):** Process handle for terminate obtained via **NtOpenProcess** resolved at runtime (api_resolve::get_proc_a with APIRES_OBF_A("NtOpenProcess")) – no **OpenProcess** in IAT for this path; uses local _term_client_id to avoid include-order dependency.

### Win11 23H2
- ProcessBasicInformation and PROCESS_BASIC_INFORMATION layout unchanged; NtQueryInformationProcess SSN stable.
- Driver LDR/PEB offsets (driver/includes.hpp globals::offsets) – verify per build if needed; 0x5a8, 0x5f0, 0x448, 0x550, 0x388 used for current Win10/11.

---

## 12. Not Used (Avoid)

- WSK/sockets – EAC detects thread patterns
- MmCopyVirtualMemory – EAC UC 496628
- Registry for shared buffer – MmCopy path
- system(), CreateToolhelp32Snapshot as primary – monitored
- IsDebuggerPresent / CheckRemoteDebuggerPresent – heavily hooked
- QueryPerformanceCounter for timing checks – well-known public pattern
- MessageBoxA – trivially monitored
