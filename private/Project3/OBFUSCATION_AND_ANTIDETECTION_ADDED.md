# Obfuscation and Anti-Detection Additions

**Date:** Feb 2026  
**Scope:** Usermode utilities, direct syscall, process enum, lazy import, entrypoint, ETW patch.

---

## 1. API String Obfuscation (`utilities/api_resolve.hpp`)

**Purpose:** Reduce static signature detection. Plain strings like `"ntdll.dll"`, `"NtGetNextProcess"`, `"CreateFileW"` in the binary are common detection targets for AC/EDR.

**Implementation:**
- Compile-time XOR encryption (key 0x5A + index)
- `OBF_A("string")` for char, `OBF_W(L"string")` for wchar_t
- `get_module_w()`, `get_proc_a()` decrypt at runtime before GetModuleHandle/GetProcAddress

**Files updated:**
- `utilities/direct_syscall.hpp` – ntdll, kernel32, NtGetNextProcess, NtDeviceIoControlFile, QueryFullProcessImageNameW
- `utilities/process_enum.hpp` – ntdll, NtQueryInformationFile, NtOpenFile, QueryFullProcessImageNameW
- `utilities/lazy_import.hpp` – kernel32, advapi32, CreateFileW, DeviceIoControl, Reg*, OpenFileMappingW, MapViewOfFile, FlushFileBuffers, CloseHandle
- `utilities/impl/driver.hpp` – NtOpenFile (fallback path)
- `utilities/etw_patch.hpp` – ntdll, EtwEventWrite

---

## 2. Debugger/Instrumentation String Obfuscation (`entrypoint.cpp`)

**Purpose:** Hide debugger and tool names from static scans.

**Updated:**
- Debugger window names: ollydbg, ida, x64dbg, windbg, process hacker, cheat engine
- Suspicious process names: wireshark, tcpview, procmon, procexp, autoruns, filemon, regmon, apimonitor

**Method:** `_xor_()` (xorstr) – compile-time encryption, decrypt on use.

---

## 3. Already in Place (Unchanged)

| Feature | Location | Status |
|--------|----------|--------|
| Process enum stealth | `process_enum.hpp` | NTFS → NtGetNextProcess → Toolhelp |
| Direct syscall | `direct_syscall.hpp` | SSN extraction, stub allocation RW→RX |
| Lazy import | `lazy_import.hpp` | FLUSHCOMM_USE_LAZY_IMPORT 1 |
| Fortnite process names | entrypoint | _xor_(L"FortniteClient-Win64-Shipping.exe") |
| Codecave | driver | FLUSHCOMM_USE_CODECAVE 1 |
| FlushFileBuffers | driver | No IOCTL path |
| Section shared memory | driver | No MmCopyVirtualMemory |

---

## 4. Detection Vectors Mitigated

| Vector | Mitigation |
|--------|------------|
| Static string scan (API names) | api_resolve OBF_A/OBF_W |
| Static string scan (tool names) | xorstr _xor_ |
| IAT visibility | Lazy import (driver APIs), api_resolve (ntdll/kernel32) |
| CreateToolhelp32Snapshot | Replaced with NTFS + NtGetNextProcess |
| NtDeviceIoControlFile hooks | Direct syscall |
| RWX memory | alloc_executable uses RW then VirtualProtect to RX |

---

## 5. Optional Future Improvements

- **PEB walk** – Resolve GetProcAddress/GetModuleHandle via PEB→Ldr→export parsing (full IAT bypass). Higher complexity.
- **VirtualAlloc obfuscation** – Resolve VirtualAlloc/VirtualFree via api_resolve if needed.
- **Kdmapper** – Same api_resolve pattern could be applied to intel_driver, eneio_driver, service.cpp for consistency.
