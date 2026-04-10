# kernel-driver-example-and-research

**FlushComm** — a fully custom, highly-evasive kernel driver focused on covert user-mode ↔ kernel-mode communication, physical memory manipulation, CR3/DTB resolution, and mouse injection. Built as a research and example project for kernel-mode development with heavy emphasis on anti-detection, signature dilution, runtime obfuscation, and EAC/Fortnite-grade stealth.

This driver is the core artifact in `private/Project3/driver/`. Everything else in the repository (mappers, research docs, utilities) exists to support, load, or analyze **FlushComm**.

## Driver Overview

**FlushComm** is a modern, production-oriented kernel driver (`FILE_DEVICE_UNKNOWN`) that provides a secure, multi-channel communication interface between a user-mode client and the kernel. Its primary goals are:

- Stealthy read/write to any process memory (physical path preferred)
- Reliable CR3/DTB retrieval even under protected/Pg-protected environments
- Mouse movement injection with timing jitter
- Multiple layered communication methods designed to survive IOCTL monitoring, MajorFunction hooking, and ETW tracing
- Extensive anti-analysis, anti-signature, and anti-forensic features

It was designed specifically with EAC/BattlEye in mind but is general-purpose for any kernel-mode research requiring an extremely low detection surface.

## Core Features

### 1. Communication Channels (layered, fallback-aware — **IOCTL is NOT primary**)

The driver deliberately avoids traditional `IRP_MJ_DEVICE_CONTROL` as the main path because it is heavily monitored by modern anti-cheats. Instead, it uses a **primary hot-path** based on shared memory + signaling that leaves almost no traceable IOCTL footprint.

#### Primary Communication Method (Active Hot Path)
- **FlushFileBuffers + Shared Memory Section**
  - User-mode calls `FlushFileBuffers` on the driver’s device handle → triggers `IRP_MJ_FLUSH_BUFFERS` in the kernel.
  - Data exchange happens entirely through a kernel-allocated shared section (`ZwCreateSection` + `MapViewOfFile` into user-mode).
  - Requests and responses live directly in `g_section_view` (zero-copy).
  - The driver processes the buffer via `FlushComm_ProcessSharedBuffer()`.
  - No IOCTL buffers, no `MmCopyVirtualMemory` in the hot path.
  - This is the **default and preferred** method (`FLUSHCOMM_USE_FLUSH_BUFFERS`).

#### Fallback Communication Methods (in order of preference)
1. **Registry Fallback**
   - Activated automatically if shared section creation/mapping fails.
   - Shared buffer address + PID are written to obfuscated registry keys (`OBF_SharedBuffer`, `OBF_SharedPid`).
   - Data is transferred using `MmCopyVirtualMemory` (higher detection surface, therefore only fallback).
2. **Legacy IOCTL (IRP_MJ_DEVICE_CONTROL)**
   - Only enabled if `FLUSHCOMM_USE_FLUSH_BUFFERS` is explicitly disabled at compile time.
   - Uses custom control codes protected by `CODE_SECURITY` / `FLUSHCOMM_CODE_SECURITY`.
   - Kept as a compatibility fallback only.
3. **FILE_OBJECT DeviceObject Swapping (stealth enhancement)**
   - Hooks `IRP_MJ_CREATE` on known devices (e.g. `\.\Beep`, `\.\Null`) and swaps `FILE_OBJECT->DeviceObject` to a fake device.
   - Preserves the original driver’s `MajorFunction[IRP_MJ_DEVICE_CONTROL]` untouched.
   - Enabled via `FLUSHCOMM_USE_FILEOBJ_HOOK = 1`.
4. **WSK (Winsock Kernel) Server** — currently disabled (`FLUSHCOMM_USE_WSK = 0`) due to thread-scanning risks.

All channels are protected by the same magic value + security code validation. The driver dynamically chooses the best available path at runtime.

### 2. CR3 / DTB Retrieval Engine (multi-method, fallback-heavy)

CR3 resolution is one of the most detection-sensitive operations in the driver. **FlushComm** implements the exact prioritized fallback chain documented in `CR3_METHODS_RESEARCH.md` to stay under EAC radar:

#### Current Retrieval Order (lowest → highest detection risk)
1. **Derive DTB from Process Base Address** (preferred)
   - Uses `PsGetProcessSectionBaseAddress` + custom `dirbase_from_base_address()` / physical page-table walk.
   - No `KeStackAttachProcess`, no `__readcr3`, no EPROCESS read.
   - Lowest detection surface.
2. **MmPfn Database Bruteforce**
   - Scans the `_MMPFN` database for the target process’s PTE.
   - Decrypts DTB if needed.
   - No attach, no `__readcr3`.
3. **EPROCESS → KPROCESS.UserDirectoryTableBase**
   - Direct read from `EPROCESS` (offset varies by Windows build: 0x280 / 0x388 etc.).
   - Checks for encryption flag `(dtb >> 0x38) == 0x40`; if encrypted, falls through.
4. **KeStackAttachProcess + __readcr3()** (absolute last resort)
   - Only used when all prior methods fail.
   - Explicitly avoided in EAC-targeted builds because it is heavily signatured.

Additional supporting methods researched (and available as compile-time options):
- Low Stub physical scan (for HVCI environments where only physical R/W is possible).
- Full `EPROCESS` list enumeration via `PsInitialSystemProcess` + `ActiveProcessLinks`.

All methods feed into a small LRU CR3 cache and are validated by checking for the `MZ` magic at the process image base after translation.

### 3. Memory Read/Write Engine
- **Preferred path**: Physical memory via `MmMapIoSpaceEx` / `MmUnmapIoSpace` (dynamically resolved) + full PML4/PDPT/PD/PT walking.
- Falls back to virtual attach only when physical mapping is impossible.
- Explicitly avoids `MmCopyVirtualMemory` in hot paths (traced by EAC).

### 4. Mouse Injection
- `IOCTL_MOUSE_MOVE` (or equivalent via shared buffer) with random jitter (0-2 ms) and minimum 5 ms interval.
- Executes in a one-shot system worker routine to avoid persistent threads.
- Fully asynchronous.

### 5. Anti-Detection & Evasion Layer

The driver implements **layered evasion** documented in depth across the research files:
- Runtime routine & string obfuscation (`routine_obfuscate.h`)
- Signature dilution / codecave padding (`signature_dilution.hpp`, `codecave.cpp`)
- Custom pool tags
- Trace cleaning (`trace_cleaner.hpp`)
- Driver hiding from `PsLoadedModuleList`
- Dynamic device name / symbolic link
- iCall gadgets, NMI spoofing, page evasion, FILE_OBJECT hooking, etc.
- All major features are toggleable via compile-time defines.

### 6. Build & Loading
- Visual Studio 2022 + WDK
- Supports `sc create/start` **and** kdmapper-style manual mapping (`DriverEntry` handles `DriverObject == NULL` case)
- Lightweight init for PatchGuard safety

## Research & Design Documentation (inside the driver folder)

The dozens of `.md` files are **not generic notes** — they are the living design documents that shaped every decision:
- `CR3_METHODS_RESEARCH.md` — full fallback chain and EAC considerations
- `IOCTL_ALTERNATIVES_UPDATED_RESEARCH.md` / `IOCTL_ALTERNATIVES_IMPL.md` — why FlushFileBuffers + shared section became primary
- `COMMUNICATION_VERIFICATION.md`, `ANTI_DETECTION_IMPLEMENTATION.md`, `EVASION_IMPLEMENTATION_SUMMARY.md`, `EAC_FORTNITE_DETECTION_RESEARCH.md`, etc.

Reading these files is required to understand the “why” behind every code path.

## Repository Structure (Driver-Centric View)

All driver-related content lives in **`private/Project3/driver/`**:
- Core: `driver.cpp`, `flush_comm.cpp`, `file_obj_hook.cpp`, `wsk_server.cpp`, `codecave.cpp`, `nmi_spoof.cpp`, `icall_gadget.cpp`
- Headers: `defines.h`, `flush_comm.hpp`, `memory.hpp`, `mouse_inject.hpp`, etc.
- Research: 20+ `.md` files (see list above)
- Subfolders: `com/`, `build/`

## Disclaimer

For research and educational purposes only. Use only on systems you own or have explicit authorization to test on. Comply with all applicable laws.

---

## Repository structure

| Path | Description |
|------|-------------|
| **`private/Project3/`** | Main project and research content |
| **`private/Project3/Aether.Mapper (1)/`** | C# (.NET) mapper using Gigabyte **gdrv.sys** (`\Device\GIO`). NtLoadDriver + registry, trace cleaners (PiDDB, WdFilter, KernelHashBucketList, MmUnloadedDrivers). Optional symbol server. |
| **`private/Project3/LegitMemory (1)/`** | C++ mapper using **PdFwKrnl.sys** (AMD PDFW). Uses `sc create/start`; patches kernel (SeValidateImageHeader/Data, PatchGuard). PoC only — PatchGuard will BSOD. |
| **`private/Project3/kdmapper_src/`** | [TheCruZ/kdmapper](https://github.com/TheCruZ/kdmapper) — C++ mapper using Intel **iqvw64e.sys**. NtLoadDriver, NtAddAtom hook, full trace cleaning. See `kdmapper_src/README.MD` for build and usage. |
| **`private/Project3/dependencies/`** | Config and selection logic for driver mapper choice. |

## Research documents

- **[AETHER_VS_LEGITMEMORY_DETECTION_RESEARCH.md](private/Project3/AETHER_VS_LEGITMEMORY_DETECTION_RESEARCH.md)** — Aether.Mapper vs LegitMemory: detection surface, evasion comparison, and verdict (which is “more undetected” and in what sense).
- **[KDMAPPER_VS_AETHER_UNDETECTABILITY.md](private/Project3/KDMAPPER_VS_AETHER_UNDETECTABILITY.md)** — Kdmapper vs Aether.Mapper: strengths/weaknesses, side-by-side comparison, and recommendation (e.g. modified kdmapper vs stock Aether).

## Summary (from research)

- **Aether.Mapper**: Better operational stealth (NtLoadDriver, trace cleaning, no kernel patching). Weaker vs static analysis (.NET, symbol server, literals, gdrv blocklist).
- **LegitMemory**: Native C++, no symbol server; kernel patching causes BSOD and is not suitable for stealth as-is; uses noisy `sc.exe` and fixed names.
- **Kdmapper**: Strong base (native C++, no .NET/PDB download); with runtime resolution and string obfuscation can exceed stock Aether in undetectability.

## Building

- **Aether.Mapper**: Open the .NET solution in Visual Studio; build (e.g. x64 Release). Requires .NET SDK / Visual Studio.
- **Kdmapper**: See [kdmapper_src/README.MD](private/Project3/kdmapper_src/README.MD). Needs Visual Studio, Windows SDK, and WDK. Supports PDB offsets or pattern scan.
- **LegitMemory**: Open the C++ solution; requires Windows SDK and WDK. Uses AMD PdFwKrnl.sys (BYOVD).

## Requirements and notes

- Disable vulnerable driver blocklist if you need to load BYOVD (e.g. [Microsoft KB5020779](https://support.microsoft.com/en-us/topic/kb5020779-the-vulnerable-driver-blocklist-after-the-october-2022-preview-release-3fcbe13a-6013-4118-b584-fcfbc6a09936)).
- Driver entry should return quickly; run ongoing work in a thread to avoid PatchGuard and stability issues.
- For research and education only; ensure compliance with local laws and target system authorization.

## References

- [TheCruZ/kdmapper](https://github.com/TheCruZ/kdmapper) — Intel iqvw64e.sys manual map, NtLoadDriver, trace cleaning.
- Research docs in `private/Project3/` (Aether vs LegitMemory, Kdmapper vs Aether).
