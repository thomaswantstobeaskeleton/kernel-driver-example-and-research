# Aether.Mapper vs LegitMemory – Detection / Stealth Comparison

This document compares the two custom mappers from a **detection and evasion** perspective based on a full code review of both projects.

---

## 1. Overview

| Aspect | Aether.Mapper (1) | LegitMemory (1) |
|--------|-------------------|------------------|
| **Language** | C# (.NET) | C++ (native) |
| **Vulnerable driver** | Gigabyte **gdrv.sys** (\Device\GIO) | **PdFwKrnl.sys** (AMD PDFW, same IOCTL as AMD driver) |
| **Primary use in repo** | Load gdrv, run trace cleaners, optional manual map (commented out) | Load PdFwKrnl, **patch kernel** (SeValidateImageHeader/Data, PatchGuard), then revert |
| **Driver loading** | NtLoadDriver + registry (no sc.exe) | **sc create / sc start / sc stop / sc delete** (cmdline) |

---

## 2. Aether.Mapper – Detection Surface

### 2.1 Runtime / Binary

- **.NET managed assembly**: Easily decompiled (dnSpy, dotPeek). All logic, strings, and API names are recoverable. No native obfuscation.
- **P/Invoke**: Direct `DllImport("ntdll.dll")` for `NtLoadDriver`, `NtUnloadDriver`, `RtlAdjustPrivilege`, `NtQuerySystemInformation`, `NtCreateFile`, `NtClose`. These names appear in metadata and IAT.
- **Literal strings in code**: e.g. `@"SYSTEM\CurrentControlSet\Services"`, `"\Registry\Machine\..."`, `"\??\"`, `"\Device\GIO"`, `"ntdll.dll"`, `"ntoskrnl.exe"`, `"WdFilter.sys"`, `"PiDDBLock"`, `"PiDDBCacheTable"`, symbol names. No runtime obfuscation or encryption of strings.
- **Gigabyte gdrv**: Device name `\Device\GIO` and IOCTLs (`0xc3502808`, `0xC3502004`, `0xC3502008`) are well-known and on many blocklists (e.g. LOLDrivers, HVCI/vulnerable driver blocklist).
- **Driver payload**: Stored in resources and decoded with a **single XOR key** (`0x571d77a5`). Trivial to recover the original driver binary; key is in the binary.

### 2.2 Network and External Resources

- **Symbol server**: Downloads PDB from `http://msdl.microsoft.com/download/symbols` via `HttpClient.GetByteArrayAsync`. Creates predictable HTTP traffic (e.g. `ntkrnlmp.pdb` or similar) and writes PDB to disk. EDR/NDR can detect symbol download and file creation.
- **Symbol usage**: Uses DbgHelp (`SymInitialize`, `SymLoadModuleEx`, `SymFromName`) to resolve kernel symbols. No obfuscation of symbol names.

### 2.3 Driver Loading and Service Creation

- **Clean**: Uses **NtLoadDriver** and **NtUnloadDriver** with registry keys (`RegCreateKey`, `RegSetValueEx` for ImagePath/Type). No `sc.exe` or `CreateProcess` for service control.
- **Random service name**: 8-character alphanumeric via `Rand.GenerateName(8)` (cryptographic RNG). Good for reducing static signatures.
- **Pre-clean**: Cleans previous temp `.sys` files and service keys, and prefetch files for the process. Reduces leftover artifacts.

### 2.4 Trace Cleaning (Post-Load Stealth)

- **PiDDB**: Cleans PiDDBCacheTable (lookup by driver name + timestamp, unlink, delete from AVL, decrement DeleteCount).
- **WdFilter**: Cleans WdFilter RuntimeDrivers list (MpBmDocOpenRules, MpFreeDriverInfoEx, RuntimeDriversArray/Count/List).
- **KernelHashBucketList**: Cleans g_KernelHashBucketList (ci.dll).
- **MmUnloadedDrivers**: Zeros driver name in driver section so MmUnloadedDrivers doesn’t record the vulnerable driver name after unload.

All of this **reduces detection** after the driver has been used, compared to leaving traces.

### 2.5 No Kernel Patching

- Does **not** patch SeValidateImageHeader, SeValidateImageData, or PatchGuard. Avoids PatchGuard and BSOD risk; no direct kernel integrity violations.

---

## 3. LegitMemory – Detection Surface

### 3.1 Runtime / Binary

- **C++ native**: No .NET; harder to decompile and no managed metadata. Better from a “static analysis” perspective than Aether.
- **Vulnerable driver**: **PdFwKrnl.sys** (AMD PDFW). IOCTL `0x80002014` (FILE_DEVICE_AMD_PDFW, PDFW_MEMCPY). Less ubiquitous than Intel iqvw64e or Gigabyte gdrv on blocklists, but still a known BYOVD.
- **Literal strings**: Device name, driver name, service name `"byovd_provider"`, paths like `"C:\\Windows\\System32\\MicroNT.sys"`, kernel path `"c:\\windows\\system32\\ntoskrnl.exe"`, and pattern strings (e.g. `"SeValidateImageHeader"`, `"PatchGuard"`) in `kernel_utils.cpp`. No obfuscation.

### 3.2 Driver Loading and Service Creation

- **Noisy**: Uses **`system("sc create ...")`**, **`system("sc start ...")`**, **`system("sc stop ...")`**, **`system("sc delete ...")`**. Each call spawns `sc.exe` with a clear command line (e.g. `sc create byovd_provider binpath="..." type=kernel`). EDR/ETW can log:
  - Process creation (sc.exe)
  - Command line (driver path, service name)
  - Parent process
- **Fixed service name**: `"byovd_provider"` is a clear signature.
- **Device open**: `CreateFileA("\\\\.\\" + _device_name)` with device name from constructor (e.g. `"PdFwKrnl"`). No indirection.

### 3.3 Kernel Patching (Critical for “Undetected”)

- **Main flow in `LegitMemory.cpp`**:
  - Reads ntoskrnl base via `EnumDeviceDrivers`.
  - Resolves offsets for **SeValidateImageHeader**, **SeValidateImageData**, **ret** (for stub), **PatchGuard**, **PatchGuardValue** using pattern scan on `ntoskrnl.exe` on disk.
  - **Writes kernel memory** to:
    - Replace SeValidateImageHeader with a “ret” stub.
    - Replace SeValidateImageData with a “ret” stub.
    - Patch PatchGuard-related pointer to point at PatchGuardValue.
  - These are **PatchGuard-protected** structures. On modern Windows (Vista+), **Kernel Patch Protection (PatchGuard)** will detect such modifications and **trigger a bugcheck (BSOD)** within minutes. So this code path is **not suitable for stealth**; it is a PoC that will crash the system.
- **No trace cleaning**: No PiDDB, MmUnloadedDrivers, WdFilter, or KernelHashBucketList cleaning. After unloading the driver, traces remain.

### 3.4 Other APIs

- **EnumDeviceDrivers** (Psapi): Common for getting kernel base; can be hooked or logged by security products.
- **Pattern scanning**: Reads `ntoskrnl.exe` from disk and scans for byte patterns. File read of system binary is visible; pattern strings are in the binary.

---

## 4. Side-by-Side Evasion Comparison

| Factor | Aether.Mapper | LegitMemory |
|--------|----------------|-------------|
| **Binary reversibility** | High (.NET) | Lower (C++) |
| **API visibility** | DllImport names in metadata | CreateFileA, DeviceIoControl, system(), EnumDeviceDrivers in IAT/imports |
| **String literals** | Many (paths, names, symbols) | Many (paths, driver/service names, patterns) |
| **Vulnerable driver** | gdrv (Gigabyte) – blocklisted in many environments | PdFwKrnl (AMD) – less common, still known |
| **Driver storage** | XOR in resources (single key) | External file (BYOVD) – no embedded blob in exe |
| **Service creation** | NtLoadDriver + registry (quiet) | sc create/start (noisy, process creation) |
| **Service/driver name** | Random 8-char | Fixed "byovd_provider" |
| **Trace cleaning** | PiDDB, WdFilter, KernelHashBucketList, MmUnloadedDrivers | None |
| **Prefetch / temp clean** | Yes | No |
| **Kernel patching** | None | Yes (SeValidate*, PatchGuard) → **BSOD risk** |
| **Network** | Symbol server HTTP download | None in reviewed path |
| **Symbol resolution** | PDB download + DbgHelp (detectable) | Local file pattern scan (no symbol server) |

---

## 5. Verdict: Which Is “More Undetected”?

### 5.1 If “undetected” means: less likely to be flagged by EDR/AV and stable (no BSOD)

- **Aether.Mapper** is **safer and more “operationally undetected”** in these ways:
  - Does **not** patch the kernel; no PatchGuard trigger.
  - **Trace cleaning** (PiDDB, WdFilter, KernelHashBucketList, MmUnloadedDrivers) reduces forensic traces after the driver is used.
  - **NtLoadDriver** + registry instead of `sc.exe` avoids noisy process creation and command-line logging.
  - **Random service name** and prefetch/temp cleanup reduce static and artifact-based detection.

- **LegitMemory** in the provided code:
  - **Kernel patching will cause BSOD** on PatchGuard-enabled systems; the main flow is not viable for stealth.
  - **sc create/start** is very visible (process + cmdline).
  - **No trace cleaning** leaves more evidence.
  - Fixed names (`byovd_provider`, PdFwKrnl) are easy to signature.

So **for “more undetected” in the sense of “usable and less detectable in practice”**, **Aether.Mapper** is ahead **provided** you accept its downsides (see below).

### 5.2 If “undetected” means: harder to reverse and fewer static indicators

- **LegitMemory** has advantages:
  - **Native C++**: No .NET metadata; analysis is harder than for Aether.
  - **No symbol server**: No HTTP to Microsoft; no PDB download or DbgHelp symbol names in a single obvious flow.
  - **BYOVD on disk**: Driver not embedded in the exe (no XOR’d resource to extract).

- **Aether.Mapper** is weaker here:
  - **.NET** makes logic and strings easy to recover.
  - **Symbol server + DbgHelp** and literal symbol names give clear behavioral and string signatures.
  - **Embedded gdrv + fixed XOR** is a known pattern and recoverable.

So **for “more undetected” in the sense of “harder to analyze and fewer obvious static/behavioral signatures in the binary itself”**, **LegitMemory** (with the **kernel-patching PoC removed** and **sc.exe replaced** by NtLoadDriver + registry) could be a better base, **if** you then add trace cleaning and avoid PatchGuard.

### 5.3 Summary Table

| Criterion | Better choice |
|-----------|----------------|
| Avoid BSOD / stability | **Aether** (no kernel patching) |
| Trace cleaning | **Aether** (full set; LegitMemory has none) |
| Quiet driver load | **Aether** (NtLoadDriver vs sc.exe) |
| Binary / reversibility | **LegitMemory** (C++ vs .NET) |
| No symbol server / PDB | **LegitMemory** |
| Driver blocklist / notoriety | Depends on environment (gdrv vs PdFwKrnl) |
| Usable as a “stealth” mapper today | **Aether** (if you accept .NET and symbol download) |

**Overall**: For a **working, “more undetected” mapper** in the sense of **not crashing and leaving fewer traces**, **Aether.Mapper** is the better choice **as-is**. For a **native, no–symbol-server, less reversible** base that could be made more undetected, **LegitMemory** would need the **kernel-patching code removed**, **sc.exe replaced with NtLoadDriver + registry**, **randomized service/driver names**, and **trace cleaning added**; then it could potentially surpass Aether in stealth, at the cost of more development.

---

## 6. References (paths reviewed)

- **Aether.Mapper**: `Aether.Mapper (1)\` — Program.cs, Engine\Mapper.cs, Engine\GdrvDriver.cs, Engine\TraceCleaners\*, Runtime\*, Interop\Ntdll.cs, Engine\Symbols\*, Core\Security.cs, Core\Rand.cs, AppServiceProvider.cs.
- **LegitMemory**: `LegitMemory (1)\LegitMemory\` — LegitMemory.cpp, byovd.cpp/h, amd.cpp/h, kernel_utils.cpp/h, loadup.h, pattern.h.
