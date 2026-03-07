# Physical Memory Bypass & Stealth Requirements

Reference for EAC-aware driver design. Implement incrementally.

---

## 1. Physical Memory Access (Core Bypass)

### 1.1 CR3 Retrieval

| Method | Detection Risk | Notes |
|--------|----------------|-------|
| **KeStackAttach + __readcr3** | Detected if not cached | Cache result per-process; avoid repeated attach |
| **MmGetVirtualForPhysical + PFN swap** | Lower if no attach | Preferred for stealth; no process attach |
| **EPROCESS.UserDirectoryTableBase** | Encrypted on Win11+ | Check `(dtb >> 0x38) == 0x40`; fall through |

**Current driver**: Uses all three (EPROCESS→attach→MmPfn). Cache CR3 per PID to reduce attach frequency.

### 1.2 Virtual → Physical Translation

- **Never**: `MmCopyMemory`, `ReadProcessMemory`, `VirtualAlloc`
- **Do**: Manual page-table walk

```
CR3 & ~0xFFF → PML4 physical base
PML4[i] → PDPTE (physical)
PDPTE[j] → PDE
PDE[k] → PTE
PTE → frame number → physical address
```

- **Mask**: `CR3 & 0xFFFFFFFFFFFFF000` (PML4 base)
- **Performance**: Cache CR3/PML4 per process; target ~150ns VA→PA vs WinAPI ~1000ns

### 1.3 Read/Write to Physical RAM

| API | Use |
|-----|-----|
| `MmGetPhysicalAddress` | VA → PA (kernel VA only; for target use manual walk) |
| `MmMapIoSpace` / `MmMapIoSpaceEx` | Map physical page for R/W |
| `MmUnmapIoSpace` | Unmap after use |

**Flow**: `target_VA` → manual walk → `physical_addr` → `MmMapIoSpace` → read/write → `MmUnmapIoSpace`.

---

## 2. Spoofer (EAC Identifier Handling)

### 2.1 Routines to Hook

- `RtlGetProcessVersion`
- `RtlGetVersion`
- `NtQuerySystemInformation` (e.g. `SystemModuleInformation`, `SystemKernelDebuggerInformation`)
- All variants (32/64-bit offsets, different contexts)

### 2.2 Hooking Strategy

- **Inline hooks** (no trampolines) to intercept
- **Shadow copy** of hooked routine in driver memory (avoids tracebacks)
- Ensure thread RIP stays within valid modules (no NMIs on `PAGE_NOACCESS`)

---

## 3. Stealth Rules

### 3.1 Memory Writes

- **All writes**: Use `MmGetPhysicalAddress` + `MmMapIoSpace` (no virtual writes into target)
- **No**: Direct `*(uint64_t*)addr = x` on target address space

### 3.2 Caching

- Cache CR3 / PML4 per process to avoid page faults during translation
- Avoid repeated `KeStackAttach` / `__readcr3`

### 3.3 Avoid

- PFN bruteforcing (or minimize; EAC scans for patterns)
- Large pool allocations (EAC scans `KMALLOC`)
- Writing from `PAGE_NOACCESS` regions (validate RIP before NMI)

### 3.4 Exceptions

- Custom `#PF` (page fault) handling where appropriate
- Validate thread context before firing NMIs

### 3.5 Driver Mapping

- Map driver sections as `PAGE_NOACCESS`
- Use `MmMapIoSpace` to hide executable mappings
- Validate writes against EAC signature checks

---

## 4. Implementation Status

1. **Phase 1**: GUID device (done), CR3 caching ✅
2. **Phase 2**: Manual VA→PA walk, `MmMapIoSpace` read/write path ✅
3. **Phase 3**: Remove MmCopyMemory - replaced with MmMapIoSpace ✅
4. **Phase 4**: Spoofer hooks (requires reversal of EAC query paths)
5. **Phase 5**: Section hiding, `PAGE_NOACCESS` + `MmMapIoSpace`

---

## 5. Files to Modify

| File | Changes |
|------|---------|
| `driver/memory.hpp` | Add `va_to_pa()`, `read_phys()`, `write_phys()` using manual walk + `MmMapIoSpace` |
| `driver/driver.cpp` | CR3 cache, route `frw`/`fba` through physical path |
| `driver/spoofer*` | Hooks for `RtlGetVersion`, `NtQuerySystemInformation`, etc. |
| New: `driver/phys_mem.hpp` | VA→PA translation, physical R/W helpers |
