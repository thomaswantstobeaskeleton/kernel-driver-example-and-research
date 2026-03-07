# Kdmapper vs LegitMemory vs Aether – Non-Public / Undocumented Methods & Anti-Detection

Comparison of **undocumented APIs and kernel internals** and **anti-detection patterns** across the three mappers.

---

## 1. Kdmapper (modified)

### Undocumented / non-public methods

| Area | What it uses | Documented? |
|------|----------------|-------------|
| **Kernel structures** | PiDDB (PiDDBLock, PiDDBCacheTable), RtlAvlTable, PiDDBCacheEntry, RtlDeleteElementGenericTableAvl, RtlLookupElementGenericTableAvl, ExAcquireResourceExclusiveLite, ExReleaseResourceLite | **No** – internal ntoskrnl/Windows kernel |
| **Kernel structures** | MmUnloadedDrivers (device → driver object → DriverSection → BaseDllName) | **No** – internal layout |
| **Kernel structures** | g_KernelHashBucketList, g_HashCacheLock (ci.dll) | **No** – Code Integrity internals |
| **Kernel structures** | WdFilter (MpBmDocOpenRules, MpFreeDriverInfoEx, RuntimeDriversList, RuntimeDriversArray, RuntimeDriversCount) | **No** – WdFilter.sys internals |
| **Kernel** | NtAddAtom hook as kernel call gate (patch then call) | **No** – exploit pattern, not a public API |
| **Pattern scanning** | FindPatternInSectionAtKernel, ResolveRelativeAddress on ntoskrnl/WdFilter | **No** – byte patterns and offsets, build-dependent |
| **PEB layout** | __readgsqword(0x60) (PEB), Ldr at +0x18, InMemoryOrderModuleList +0x20, entry layout (+0x30 base, +0x48/+0x50 for path) | **No** – PEB/LDR layout is undocumented (documented only high-level) |

### Anti-detection patterns

| Pattern | Present? | Notes |
|---------|----------|--------|
| **Runtime API resolution** | **Yes** | NtLoadDriver, NtUnloadDriver, RtlAdjustPrivilege, NtQueryInformationProcess (and NtAddAtom, etc.) resolved via GetProcAddress at runtime – **no IAT entries** for these |
| **PEB-based ntdll** | **Yes** | PebGetNtdll() walks PEB/LDR to find ntdll by name – **no GetModuleHandleA("ntdll.dll")** in hot path (bypasses user-mode hooks on GetModuleHandle) |
| **String obfuscation** | **Yes** | Device path (\\.\Nal), registry paths, kernel names (ntoskrnl, WdFilter, ci), export names (ExAllocatePoolWithTag, etc.), IOCTL/case numbers, pool tag – **XOR at runtime**, no literals in .rdata |
| **Optional lazy import** | **Yes** | KDMAPPER_USE_LAZY_IMPORT for CreateFileW – avoids IAT entry |
| **Random service/driver name** | **Yes** | Per-run random name for service and temp file |
| **No sc.exe** | **Yes** | NtLoadDriver + registry only |

**Summary:** **High** undocumented kernel usage (PiDDB, MmUnloadedDrivers, HashBucket, WdFilter, NtAddAtom gate) and **high** anti-detection (runtime resolve, PEB ntdll, string obfuscation, optional lazy import, no sc.exe).

---

## 2. LegitMemory

### Undocumented / non-public methods

| Area | What it uses | Documented? |
|------|----------------|-------------|
| **Kernel patching** | SeValidateImageHeader, SeValidateImageData – patched to “ret” | **No** – internal kernel image validation |
| **Kernel patching** | PatchGuard, PatchGuardValue – pointer/check tampering | **No** – Kernel Patch Protection internals (extremely non-public) |
| **Pattern scanning** | Byte patterns on ntoskrnl on disk (SeValidateImageHeader, SeValidateImageData, ret, PatchGuard, patchguardvalue in .rdata) | **No** – build-dependent patterns |

### Anti-detection patterns

| Pattern | Present? | Notes |
|---------|----------|--------|
| **Runtime API resolution** | **No** | Uses #pragma comment(lib, "ntdll.lib") and direct calls; loadup.h has NtLoadDriver but byovd uses sc.exe |
| **PEB / no GetModuleHandle** | **No** | Uses standard APIs |
| **String obfuscation** | **No** | Literal driver name, device name, service name "byovd_provider", paths |
| **Lazy import** | **No** | CreateFileA, DeviceIoControl, system() in IAT |
| **Random service name** | **No** | Fixed "byovd_provider" |
| **No sc.exe** | **No** | Uses system("sc create ..."), system("sc start ...") – very visible |

**Summary:** **Very high** on **undocumented kernel** (PatchGuard/SeValidate* – among the most internal and dangerous), but **almost no** anti-detection (no IAT hiding, no obfuscation, sc.exe, literals).

---

## 3. Aether.Mapper

### Undocumented / non-public methods

| Area | What it uses | Documented? |
|------|----------------|-------------|
| **Kernel structures** | PiDDB (PiDDBLock, PiDDBCacheTable), RtlAvlTable, ListEntry, PiDDBCacheEntry, LookupElementGenericTableAvl, DeleteElementGenericTableAvl | **No** – same ntoskrnl internals as kdmapper |
| **Kernel structures** | MmUnloadedDrivers (object → DeviceObject → DriverObject → DriverSection → BaseDllName) | **No** – internal layout |
| **Kernel structures** | WdFilter (MpBmDocOpenRules, MpFreeDriverInfoEx, RuntimeDriversList/Array/Count) | **No** – WdFilter internals |
| **Kernel structures** | KernelHashBucketList (ci.dll) | **No** – Code Integrity internals |
| **Symbol resolution** | DbgHelp (SymInitialize, SymLoadModuleEx, SymFromName) + PDB from Microsoft symbol server | **Documented** – public APIs and public server |

### Anti-detection patterns

| Pattern | Present? | Notes |
|---------|----------|--------|
| **Runtime API resolution** | **No** | DllImport("ntdll.dll") – NtLoadDriver, NtUnloadDriver, RtlAdjustPrivilege in IAT/metadata |
| **PEB / no GetModuleHandle** | **No** | Standard .NET interop |
| **String obfuscation** | **No** | Literal paths, device "\Device\GIO", registry, symbol names |
| **Lazy import** | **No** | All imports static |
| **Random service name** | **Yes** | Rand.GenerateName(8) |
| **No sc.exe** | **Yes** | NtLoadDriver + registry |
| **Driver obfuscation** | **Weak** | Single XOR key in resources – easily reversed |

**Summary:** **High** undocumented **kernel** usage (same trace-cleaning internals as kdmapper), but **low** anti-detection (no IAT hiding, no PEB walk, no string obfuscation, .NET, symbol server).

---

## 4. Ranked comparison

### By “most non-public and undocumented methods and anti-detection patterns” (combined)

| Rank | Mapper | Undocumented usage | Anti-detection | Combined |
|------|--------|--------------------|----------------|----------|
| **1** | **Kdmapper (modified)** | High (PiDDB, MmUnloadedDrivers, HashBucket, WdFilter, NtAddAtom gate, PEB layout, pattern scan) | High (runtime resolve, PEB ntdll, string XOR, optional lazy import, no sc.exe, random name) | **Most** |
| **2** | **Aether** | High (same kernel internals for trace cleaning) | Low (only random name + NtLoadDriver; .NET, literals, symbol server) | **Middle** |
| **3** | **LegitMemory** | Very high (PatchGuard/SeValidate* – most “non-public” kernel tricks) | None (sc.exe, literals, direct imports) | **Kernel: most extreme; anti-detection: least** |

### By category

- **Most undocumented kernel internals (structures + behavior):**  
  **Kdmapper** and **Aether** both use the same set (PiDDB, MmUnloadedDrivers, WdFilter, KernelHashBucketList). **LegitMemory** goes further in one direction by touching **PatchGuard/SeValidate*** (no public or documented support).

- **Most anti-detection patterns (IAT hiding, obfuscation, PEB, no sc.exe):**  
  **Kdmapper** is the only one with runtime API resolution, PEB-based ntdll, string obfuscation, and optional lazy import. **Aether** has NtLoadDriver + random name only. **LegitMemory** has none.

---

## 5. Direct answer

**Which of the three uses the most non-public and non-documented methods and anti-detection patterns?**

**Kdmapper (modified)** uses the **most** of **both**:

- **Undocumented / non-public:** Same kernel internals as Aether (PiDDB, MmUnloadedDrivers, WdFilter, KernelHashBucketList) plus NtAddAtom call gate, PEB/LDR layout, and pattern scanning. LegitMemory uses more “extreme” kernel tricks (PatchGuard) but almost nothing else.
- **Anti-detection:** Runtime resolution of Nt*/Rtl* (no IAT), PEB walk for ntdll, XOR-obfuscated strings (device, registry, kernel names, IOCTLs, pool tag), optional lazy import, NtLoadDriver (no sc.exe), random service name. Aether and LegitMemory have little or no comparable patterns.

So overall, **kdmapper** has the **largest combined set** of non-public/undocumented methods and anti-detection patterns. **LegitMemory** has the single most aggressive undocumented kernel use (PatchGuard/SeValidate*) but no real anti-detection. **Aether** relies on the same undocumented kernel structures as kdmapper for cleaning but with minimal anti-detection (no IAT hiding, no obfuscation).
