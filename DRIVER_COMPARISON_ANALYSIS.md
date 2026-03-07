# Driver Comparison: undetecteddrv vs Project3 driver

## Executive Summary

The new **undetecteddrv** (Pasuhq, DeviceIoControl) differs significantly from the old **Project3** driver (FlushFileBuffers, registry). Several issues in the new driver could cause failures the old one does not have, especially under kdmapper and on systems with VBS/hypervisor.

---

## 1. Bootstrap & Entry Flow (kdmapper compatibility)

| Aspect | Old (Project3) | New (undetecteddrv) | Risk |
|--------|----------------|---------------------|------|
| **Imports** | Direct (RtlGetVersion, MmGetSystemRoutineAddress, IoCreateDevice, etc.) | All dynamic via `load_kernel_api()` | **HIGH**: kdmapper may not resolve imports when header is skipped; dynamic resolution fails |
| **get_kernel_base** | KPCR `__readgsqword(0x18)+0x38` → IDT base | `__sidt()` → IDT base | Both should work; __sidt is more portable |
| **Entry** | Direct: version_check → load_dynamic_functions → FlushComm_Init | Thread + 1s delay → IoCreateDriver(initialize_driver) | Extra complexity; thread may not run if DriverEntry fails early |

**Root cause of 0xc0000001**: `load_kernel_api()` fails under kdmapper because:
- IAT may not be usable (MmGetSystemRoutineAddress invalid)
- IDT method should work but depends on correct `get_kernel_base()` result

**Recommendation**: Use direct imports for bootstrap-critical APIs (like the old driver) so kdmapper can resolve them, or ensure `--copy-header` is used if IAT parsing is required.

---

## 2. get_process_cr3 – Order & Fallbacks

### Old driver (Project3) – robust order
1. **EPROCESS.UserDirectoryTableBase** – with encrypted DTB check `(dirBase >> 0x38) == 0x40`; if encrypted, fall through
2. **KeStackAttachProcess + __readcr3** – reliable when direct DTB is encrypted
3. **MmPfn bruteforce** – no attach, for stealth

### New driver (undetecteddrv) – weaker order
1. **Cache**
2. **MmPfn bruteforce**
3. **EPROCESS fallback** – NO encrypted DTB check, NO KeStackAttach fallback

### Issues in new driver
- **Encrypted DTB (VBS/HVCI)**: `(dirBase >> 0x38) == 0x40` means hypervisor-encrypted DTB. Old driver skips it; new driver uses it as-is → wrong CR3.
- **Missing KeStackAttachProcess + __readcr3**: Old driver uses this as fallback when DTB is encrypted. New driver lacks it entirely.
- **dirBase masking**: Old driver uses `dirBase &= 0xFFFFFFFFFFFFF000ULL`; new driver does not mask in the fallback path.
- **dirBase not reset**: Old driver sets `dirBase = 0` at start; new driver reuses global `dirBase`, which can carry over bad values from previous calls.

---

## 3. init_mmpfn_database – Crash on Failed Pattern

```cpp
auto search = search_pattern(..., ".text", "B9 ? ? ? ? 48 8B 05 ? ? ? ? 48 89 43 18") + 5;
auto resolved_base = search + *reinterpret_cast<int32_t*>(search + 3) + 7;
mm_pfn_database = *reinterpret_cast<uintptr_t*>(resolved_base);
```

If `search_pattern` returns 0 (no match), `search` becomes 5 and the code dereferences `(int32_t*)(5+3)` → address 8. That is invalid and causes a crash. Old driver has the same pattern but same bug.

**Fix**: Check `search_pattern(...) != 0` before using the result.

---

## 4. get_kernel_base Implementation

| | Old | New |
|---|-----|-----|
| **IDT source** | KPCR+0x38 (IdtBase) | `__sidt(idtr)` |
| **Descriptor parse** | `((d0>>32)&0xFFFF0000) + (d0&0xFFFF) + (d1<<32)` | `(d0&0xFFFF) + (((d0>>48)&0xFFFF)<<16) + ((d1&0xFFFFFFFF)<<32)` |
| **Find kernel base** | LEA pattern scan (48 8D 1D ... FF) | `get_kernel_base_from_address()` backward MZ scan |

Both parsings are correct for x64 IDT. The new driver’s backward MZ scan can hit HAL before ntoskrnl if the first IDT entry points into HAL. The old driver’s LEA pattern is more tied to ntoskrnl. Under kdmapper, both may be fragile; the new approach is simpler but less precise.

---

## 5. includes.hpp / globals

| | Old (Project3) | New (undetecteddrv) |
|---|----------------|---------------------|
| **i_peb** | Present in offsets | **Missing** – any use would fault |
| **i_user_dirbase default** | 0x388 (Win11) in driver.cpp | 0x28 (Win10) in includes.hpp |
| **PsGetProcessPeb** | Not declared | Declared (dllimport) |
| **LDR_DATA_TABLE_ENTRY** | Simpler layout | Has `ContextInformation`, `OriginalBase`, `LoadTime` – different layout |

Version-specific offsets are set in `mousemove::get_offsets()` for both drivers; the default values differ but get overwritten.

---

## 6. CODE_GET_DIR_BASE Guard

New driver:
```cpp
if (!DynamicMmMapIoSpaceEx) { status = STATUS_NOT_SUPPORTED; }
else { status = get_process_cr3(req); }
```

`get_process_cr3` can succeed via the EPROCESS fallback without `MmMapIoSpaceEx`. The PFN method uses `MmGetVirtualForPhysical` (in `phys_to_virt`), not `MmMapIoSpaceEx`. Blocking on `DynamicMmMapIoSpaceEx` can reject valid CR3 requests when only the EPROCESS path would work.

---

## 7. MEMORY_OPERATION_DATA Semantics

- **Usermode (driver.hpp)**: `ULONGLONG* cr3` – passes `&cr3_value` (output pointer)
- **Driver**: `uintptr_t cr3` – treats it as address to write result

Driver uses `RtlCopyMemory((void*)x->cr3, &dirBase, 8)` to write into usermode buffer. In METHOD_BUFFERED, this works in the calling process context but bypasses `ProbeForWrite`. Layout is compatible (same size), semantics match.

---

## 8. Old Driver get_kernel_base Infinite Loop

Old driver:
```cpp
for (;; align_base -= 0x1000) {
    // ... scan for LEA pattern
    if (condition) return address;
}
```

No explicit exit; if the pattern is never found, this loops indefinitely. Same function in both drivers conceptually, but the new one uses a bounded scan (512 pages) in `get_kernel_base_from_address`.

---

## Recommended Fixes for undetecteddrv

1. **get_process_cr3**:
   - Reset `dirBase = 0` at start.
   - Try EPROCESS first; add encrypted DTB check `(dirBase >> 0x38) == 0x40` and fall through if true.
   - Add KeStackAttachProcess + __readcr3 fallback (optionally wrapped in __try/__except).
   - Mask result with `0xFFFFFFFFFFFFF000ULL` in all success paths.
   - Reorder: EPROCESS (with encryption check) → KeStackAttach → MmPfn → cache as last resort.

2. **init_mmpfn_database**:
   - Check `search_pattern(...) != 0` (or equivalent) before `+ 5` and dereference.
   - On failure, set `mm_pfn_database = 0` and return, and ensure `get_process_cr3` can fall back to other methods.

3. **CODE_GET_DIR_BASE**:
   - Call `get_process_cr3` even when `DynamicMmMapIoSpaceEx` is null, and let it fail internally if needed. Or gate only the physical mapping path, not the EPROCESS path.

4. **kdmapper compatibility**:
   - Consider switching to direct imports for bootstrap (DriverEntry, version check, device creation) so loading succeeds without dynamic resolution.
