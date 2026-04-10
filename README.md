# kernel-driver-example-and-research

**FlushComm** — a fully custom, highly-evasive kernel driver focused on covert user-mode <-> kernel-mode communication, physical memory manipulation, CR3/DTB resolution, and mouse injection. Built as a research and example project for kernel-mode development with heavy emphasis on anti-detection, signature dilution, runtime obfuscation, and EAC/Fortnite-grade stealth.

This driver is the core artifact in `private/Project3/driver/`. Everything else in the repository (mappers, research docs, utilities) exists to support, load, or analyze **FlushComm**.

## Driver Overview

**FlushComm** is a modern, production-oriented kernel driver (`FILE_DEVICE_UNKNOWN`) that provides a secure, multi-channel communication interface between a user-mode client and the kernel. Its primary goals are:

- Stealthy read/write to any process memory (physical path preferred)
- Reliable CR3/DTB retrieval even under protected/Pg-protected environments
- Mouse movement injection with timing jitter
- Multiple fallback communication methods to survive IOCTL monitoring or hook-based detection
- Extensive anti-analysis, anti-signature, and anti-forensic features

It was designed specifically with EAC/BattlEye in mind but is general-purpose for any kernel-mode research requiring low detection surface.

## Core Features

### 1. Communication Channels (multi-layered, fallback-aware)
- **Primary**: IOCTL-based (`IRP_MJ_DEVICE_CONTROL`) using custom control codes protected by `CODE_SECURITY` / `FLUSHCOMM_CODE_SECURITY`
  - `0x8A12` – Read/Write memory
  - `0x9B34` – Get process base address
  - `0x1C56` – Locate guarded-region pool
  - `0x2D78` – Get process CR3/DTB
  - `0x27336` – Mouse move injection
- **Shared Section** (`FLUSHCOMM_USE_SECTION`): Maps a kernel-allocated section directly into the user-mode process for zero-copy request processing
- **Registry Fallback**: Reads shared buffer address + PID from obfuscated registry keys (`OBF_SharedBuffer`, `OBF_SharedPid`) and uses `MmCopyVirtualMemory`
- **ALPC / Flush Buffers** alternative trigger via `IRP_MJ_FLUSH_BUFFERS`

All channels are validated with the same magic + security code to prevent unauthorized access.

### 2. Memory Read/Write Engine
- Prefers **physical memory access** using `MmMapIoSpaceEx` / `MmUnmapIoSpace` (dynamically resolved) to bypass hooked high-level APIs
- Falls back to `KeStackAttachProcess` + virtual read/write only when necessary
- Full linear address translation (`translate_linear`) with PML4/PDPT/PD/PT walking and a small LRU CR3 cache
- CR3 resolution uses **four redundant methods** (physical base scan → PFN database brute → EPROCESS `UserDirectoryTableBase` → `__readcr3` fallback)
- Validates CR3 correctness by checking for `MZ` magic at the process image base

### 3. Mouse Injection
- `IOCTL_MOUSE_MOVE` with jitter (0-2 ms random delay) and minimum 5 ms interval enforcement
- Runs in a one-shot system worker routine (`mouse_work_routine`) to avoid persistent threads
- Fully asynchronous and timing-aware to defeat simple timing-based detection

### 4. Anti-Detection & Evasion Layer (the real heart of the driver)

The driver implements **layered evasion** documented across the research .md files in the same folder:

- **Runtime routine & string obfuscation** (`routine_obfuscate.h`, `get_system_routine_obf`) — no static IAT entries or string literals
- **Signature dilution / codecave** (`signature_dilution.hpp`, `codecave.cpp`) — junk data and padding to break static signatures
- **Custom pool tags** (`EVASION_POOL_TAG_*`) everywhere
- **Trace cleaning** (enabled via `FLUSHCOMM_TRACE_CLEANER`)
- **Driver hiding** from `PsLoadedModuleList` (disabled under kdmapper to avoid conflicts)
- **Dynamic device name & symbolic link** generated from `FLUSHCOMM_MAGIC` + runtime GUID
- **iCall gadget support** (`icall_gadget.cpp`) for indirect calls where needed
- **NMI spoofing** (optional via `nmi_spoof.cpp`)
- **Page evasion** (`page_evasion.hpp`)
- **File object hook** (`file_obj_hook.cpp`) for additional hiding
- **WSK server** (`wsk_server.cpp`) as a potential socket-based comms fallback

All major evasion techniques are toggleable via compile-time defines and have dedicated research documents explaining their implementation and detection surface.

### 5. Build & Loading
- Visual Studio 2022 solution (`driver.sln` / `Project3.sln`)
- WDK + Windows SDK required
- Supports both traditional `sc create/start` and **kdmapper-style** manual mapping (DriverEntry handles `DriverObject == NULL` case via `MappedInitSync()`)
- `FLUSHCOMM_DEBUG` define controls verbose logging

**Important**: The driver is intentionally lightweight on init (quick return from `DriverEntry`) so it can be used safely even with PatchGuard-sensitive environments. Long-running work is offloaded to workers.

## Repository Structure (Driver-Centric View)

All driver-related content lives in **`private/Project3/driver/`**:

- **Core source**: `driver.cpp`, `codecave.cpp`, `file_obj_hook.cpp`, `wsk_server.cpp`, `nmi_spoof.cpp`, `icall_gadget.cpp`, `flush_comm.cpp`
- **Headers**: `includes.hpp`, `defines.h`, `mouse_inject.hpp`, `trace_cleaner.hpp`, `signature_dilution.hpp`, etc.
- **Research & Design Docs** (highly recommended reading):
  - `EVASION_IMPLEMENTATION_SUMMARY.md`
  - `ANTI_DETECTION_IMPLEMENTATION.md` / `ANTI_DETECTION_MEASURES.md`
  - `IOCTL_ALTERNATIVES_RESEARCH.md` + `IOCTL_ALTERNATIVES_IMPL.md`
  - `CR3_METHODS_RESEARCH.md`
  - `CODECAVE_RESEARCH.md`
  - `MAPPER_AND_DRIVER_HIDING_RESEARCH.md`
  - `STEALTH_REQUIREMENTS.md`
  - `EAC_FORTNITE_DETECTION_RESEARCH.md` and many more

Subfolders (`framework/`, `com/`, `memory/`, `protections/`, `utilities/`, `scripts/`) contain supporting code for the above features.

## Research Focus

This entire repository exists to explore and document **how far a modern kernel driver can be pushed** while remaining undetected. The dozens of `.md` files in the driver folder are not generic notes — they are the living design documents, crash analyses, detection surface audits, and iteration logs that shaped every decision in `driver.cpp`.

## Disclaimer

For research and educational purposes only. Use on systems you own or have explicit authorization to test on. Comply with all applicable laws.

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
