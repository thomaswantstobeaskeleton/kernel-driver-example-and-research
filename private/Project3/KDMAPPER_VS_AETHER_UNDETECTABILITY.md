# Kdmapper vs Aether.Mapper – Undetectability Comparison

Source: [TheCruZ/kdmapper](https://github.com/TheCruZ/kdmapper) (cloned into `kdmapper_src`) vs Aether.Mapper (C# mapper with trace cleaning).

---

## Summary verdict

**For best undetectability in a *modified* build:**  
**Kdmapper (with your existing customizations)** has better undetectability than stock Aether.Mapper.

**Stock TheCruZ kdmapper** vs **stock Aether.Mapper:**  
**Kdmapper** still has the stronger base: native C++, no .NET/symbol server, and the codebase is designed for runtime resolution and obfuscation once you re-apply your patches. **Aether** has better trace cleaning out of the box but a larger detection surface (\.NET, PDB downloads, literals, gdrv blocklist).

---

## 1. Kdmapper (TheCruZ upstream – stock)

### Strengths
- **Native C++** – No .NET, no managed metadata; harder to decompile and no DllImport/ntdll in metadata.
- **Design for stealth** – README and codebase support:
  - NtLoadDriver / NtUnloadDriver (no `sc.exe`).
  - Hooks **NtAddAtom** as a kernel call gate (undocumented pattern).
  - Cleans **MmUnloadedDrivers**, **PiDDB**, **WdFilter** RuntimeDrivers, **g_KernelHashBucketList** (ci.dll).
  - Uses **iqvw64e.sys** (Intel) or **eneio**-style driver; well-known but widely used and documented.
- **No symbol server** – No HTTP PDB downloads; pattern scan or PDB offsets (e.g. SymbolsFromPDB) only.
- **Single binary** – No runtime dependency on .NET or external DLLs.

### Weaknesses (stock)
- **Literal strings** in the open-source code (device path, registry, names) unless you re-apply **string obfuscation** (XOR at runtime).
- **IAT** – Stock build can have NtLoadDriver/NtUnloadDriver in IAT unless you use **runtime resolution** (GetProcAddress / PEB walk) as in your previous modified kdmapper.
- **Intel driver** – iqvw64e.sys is on many blocklists (e.g. HVCI, Microsoft vulnerable driver blocklist); loader already supports eneio fallback.

---

## 2. Aether.Mapper (C# / .NET)

### Strengths
- **Trace cleaning** – Same kernel-side cleaning as (modified) kdmapper: PiDDB, MmUnloadedDrivers, WdFilter, KernelHashBucketList.
- **NtLoadDriver + registry** – No `sc.exe`; random service name (e.g. 8-char).
- **Optional backends** – Section+event (no IOCTL) or gdrv.sys (Gigabyte); section path is less conventional.

### Weaknesses (detection surface)
- **.NET** – Easy to decompile (dnSpy, dotPeek); logic and strings recoverable.
- **DllImport** – NtLoadDriver, NtUnloadDriver, RtlAdjustPrivilege, etc. appear in metadata/IAT.
- **Literal strings** – Registry paths, device names, symbol names, no runtime obfuscation in stock.
- **Symbol server** – Can download PDB from Microsoft (e.g. msdl.microsoft.com); visible to EDR/NDR and disk.
- **Gigabyte gdrv** – `\Device\GIO` and known IOCTLs are on many blocklists.
- **Embedded driver** – Single XOR key in resources; trivial to recover the driver binary.

---

## 3. Side-by-side (concise)

| Factor                | Kdmapper (stock)     | Aether.Mapper (stock) |
|----------------------|----------------------|------------------------|
| Language              | C++                  | C# (.NET)              |
| Reversibility         | Lower                | High (decompile)       |
| NtLoadDriver / no sc | Yes                  | Yes                    |
| Trace cleaning       | Yes (same set)       | Yes (same set)         |
| Runtime API resolve  | No (stock)           | No (DllImport)         |
| String obfuscation    | No (stock)           | No                     |
| PEB ntdll             | No (stock)           | No                     |
| Symbol server / PDB   | No                   | Yes (optional)         |
| Vulnerable driver     | Intel / eneio        | gdrv / section         |

With **your previous kdmapper modifications** (runtime resolve, PEB ntdll, XOR strings, optional lazy import), kdmapper had **better undetectability** than Aether (see `THREE_MAPPERS_UNDOCUMENTED_ANTIDETECTION_COMPARISON.md`).

---

## 4. Recommendation

- **Use kdmapper** (from `kdmapper_src`) as the main mapper if you can re-apply your anti-detection changes (runtime resolution, string obfuscation, PEB-based ntdll, no literals for device/registry/kernel names).
- **Aether.Mapper** is useful for trace cleaning logic and section/event design, but as a .NET binary with literals and optional symbol server it has a **larger detection surface** than a well-modified native kdmapper.
- **LegitMemory** (in this repo) has very aggressive kernel use (PatchGuard/SeValidate*) but almost no anti-detection (sc.exe, literals, fixed names) and is not a drop-in mapper for your driver; it also requires PdFwKrnl.sys.

---

## 5. References

- [TheCruZ/kdmapper](https://github.com/TheCruZ/kdmapper) – Intel iqvw64e.sys manual map, NtLoadDriver, trace cleaning.
- `THREE_MAPPERS_UNDOCUMENTED_ANTIDETECTION_COMPARISON.md` – Kdmapper vs LegitMemory vs Aether (undocumented methods + anti-detection).
- `AETHER_VS_LEGITMEMORY_DETECTION_RESEARCH.md` – Aether vs LegitMemory detection surface.
