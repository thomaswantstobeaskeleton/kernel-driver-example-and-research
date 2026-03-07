# KDMapper / Driver / Communication Crash Research

## Summary of Findings

Deep research into PC crashes when using kdmapper + driver + FlushComm identified several root causes and mitigations.

---

## 1. ObInsertObject Double-Dereference (CRITICAL – FIXED)

**Source:** [ReactOS CORE-17904](https://jira.reactos.org/browse/CORE-17904), OSR/Windows kernel docs

**Problem:** When `ObInsertObject` **fails**, it **automatically dereferences** the object. Calling code must **NOT** call `ObMakeTemporaryObject` or `ObfDereferenceObject` on the same object afterward.

**Our bug:** In `CreateDriver_ObCreatePath`, on ObInsertObject failure we were:
```c
pObMakeTemporaryObject(drvObj);
ObfDereferenceObject(drvObj);  // DOUBLE DEREFERENCE – crash!
```

**Fix:** Remove both calls on failure. Only free our pool allocations (svcBuf, nameBuf2). ObInsertObject already cleaned up the object.

---

## 2. NULL DriverObject / RegistryPath (Handled)

**Source:** [kdmapper Issue #40](https://github.com/TheCruZ/kdmapper/issues/40), [Valthrun wiki](https://wiki.valth.run/drivers/implementation/kernel/troubleshooting/driver_mapper_errors)

**Problem:** kdmapper passes `DriverEntry(0, 0)` – both params are NULL. Code that dereferences them without checks causes **page fault in nonpaged area** BSOD.

**Our handling:** CreateDriver_ObCreatePath creates a proper DriverObject via ObCreateObject + ObInsertObject (Th3Spl-style) when DriverObject is NULL.

---

## 3. IoDriverObjectType Resolution Under Manual Map

**Problem:** `MmGetSystemRoutineAddress` does **not** resolve data exports. `IoDriverObjectType` is a variable, not a function.

**Our approach:** Use linked import `*globals::IoDriverObjectType` from ntoskrnl.lib. kdmapper's `ResolveImports` patches IAT; data imports in the PE Import Directory should be resolved like functions. If resolution fails, `driverType` is NULL and we return `STATUS_NOT_FOUND`.

---

## 4. Trace Cleaner on Manual Map (Mitigated)

**Problem:** `trace_cleaner_run()` does pattern scans on ntoskrnl for `MmUnloadedDrivers`. Wrong patterns or `ExFreePool` on invalid pointers can corrupt kernel state and BSOD.

**Mitigation:** Skip trace_cleaner when `DriverObject == NULL` (manual map). Only run when normally loaded.

---

## 5. Return from DriverEntry Quickly

**Source:** kdmapper README, multiple UC threads

**Recommendation:** Return from DriverEntry as fast as possible to avoid PatchGuard or unexpected interactions. Avoid infinite loops; use threads for long work.

**Our flow:** CreateDriver calls `EntryPoint(drvObj, NULL)` synchronously – RealDriverInit runs before return. This is acceptable; we do not block indefinitely.

---

## 6. /GS and Security Check

**Source:** Th3Spl IoCreateDriver README, kdmapper

**Status:** Driver already has `BufferSecurityCheck=false` and `SpectreMitigation=false` in vcxproj. No change needed.

---

## 7. FlushFileBuffers / Beep Hook Communication

**Source:** [boom-cr3/Shared-FlushFileBuffers-Communication](https://github.com/boom-cr3/Shared-FlushFileBuffers-Communication)

**Mechanism:** Hook `IRP_MJ_FLUSH_BUFFERS` on Beep; usermode calls `FlushFileBuffers` on `\\.\Beep`. Shared section or registry buffer for request/response. No inherent crash risk if hook and buffer layout are correct.

**Synchronization:** Original method is not thread-safe by default; proper locking (e.g. mutex) should be used if multiple threads send requests.

---

## 8. Section Namespace (Already Fixed)

**Problem:** Driver created `\BaseNamedObjects\WdfCtl_xxx`; usermode tried bare `WdfCtl_xxx` (session-local). Mismatch.

**Fix:** Driver creates `\BaseNamedObjects\Global\WdfCtl_xxx`. Usermode tries `Global\WdfCtl_xxx` first, then bare, then `Local\`.

---

## 9. Th3Spl vs Our CreateDriver

| Aspect | Th3Spl | Ours |
|--------|--------|------|
| IoDriverObjectType | Linked import | `*globals::IoDriverObjectType` |
| ObInsertObject fail | ObMakeTemporary + ObfDereference (potential bug) | Fixed: no dereference |
| Driver extension size | EXTENDED_DRIVER_EXTENSION | DRIVER_EXTENSION (WDK standard) |
| Pool | ExAllocatePool2 | ExAllocatePoolWithTag |
| ClearFlag DO_DEVICE_INITIALIZING | Yes | N/A for DRIVER_OBJECT |

---

## 10. Further Debugging Steps

If crash persists after ObInsertObject fix:

1. **Test kdmapper standalone:** Run `kdmapper.exe --driver intel --indPages driver.sys` from cmd. Note if crash happens before or after "success".
2. **Try without --indPages:** Use pool allocation instead of independent pages.
3. **Try eneio backend:** `--driver eneio` if Intel is blocklisted (e.g. HVCI).
4. **Enable FLUSHCOMM_DEBUG:** Set to 1 in driver.cpp; use DbgView to capture DbgPrint before crash.
5. **Check BSOD bugcheck code:** PAGE_FAULT_IN_NONPAGED_AREA, SYSTEM_SERVICE_EXCEPTION, etc. point to different causes.
6. **Defer init to thread:** Spawn `PsCreateSystemThread` to run RealDriverInit, return from DriverEntry immediately. Reduces time in entry and mapper interactions.
