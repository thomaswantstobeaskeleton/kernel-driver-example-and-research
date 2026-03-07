# kernel-driver-example-and-research

Kernel driver examples and research: BYOVD (Bring Your Own Vulnerable Driver) mappers, trace cleaning, and detection/evasion analysis.

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
