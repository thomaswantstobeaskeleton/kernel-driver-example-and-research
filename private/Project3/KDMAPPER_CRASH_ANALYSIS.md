# KDMapper Crash Analysis

## Root Cause Summary

**Primary cause: DriverObject and RegistryPath are NULL** when kdmapper calls DriverEntry.  
KDMapper manually maps the driver and calls the entry point with `param1=0, param2=0` (DriverObject, RegistryPath). Any dereference of these NULL pointers causes "page fault in nonpaged area" BSOD.

## KDMapper Load Sequence (typical fork with cleaning)

1. Load vulnerable driver (e.g. iqvw64e.sys)
2. **Clean PiDDBCacheTable**
3. **Clean g_KernelHashBucketList**
4. Clean MmUnloadedDrivers
5. Allocate image in kernel, copy PE, resolve imports
6. **Call DriverEntry(0, 0)** ← crash here if we use DriverObject
7. Unload vulnerable driver

## Crash Points in Our Driver

| Location | Risk | Trigger |
|----------|------|---------|
| `IoCreateDevice(drv_obj, ...)` | **CRASH** | drv_obj is NULL → immediate page fault |
| `drv_obj->MajorFunction[i]` | **CRASH** | Any use of NULL DriverObject |
| `drv_obj->DriverSection` | **CRASH** | HideDriver (we disabled) |
| `RtlGetVersion` | Low | Kernel API, usually safe |
| `MmGetSystemRoutineAddress` | Low | Works from manual map |
| `load_dynamic_functions` | Low | Runs after IoCreateDevice would fail |

## Why "Crashed Earlier"

- **Before fixes**: HideDriver could corrupt kernel lists → crash *after* g_KernelHashBucketList cleaned (delayed, when kernel touches corrupted list).
- **After fixes**: We skip HideDriver, so we reach `IoCreateDevice(NULL, ...)` and crash **immediately** during initialize_driver.
- **Result**: Crash moves from "after cleaning" to "during DriverEntry" = "earlier" in the user's perception.

## Solution: IoCreateDriver Pattern

Use `IoCreateDriver` when DriverObject is NULL to obtain a valid driver object from the kernel:

```c
// If kdmapper passes NULL, use IoCreateDriver to get real DriverObject
if (!DriverObject) {
    UNICODE_STRING drv_name;
    RtlInitUnicodeString(&drv_name, L"\\Driver\\YourDriverName");
    return IoCreateDriver(&drv_name, &RealInitialization);
}
// Else use normal path
return RealInitialization(DriverObject, RegistryPath);
```

**Caveat** (kdmapper issue #76): IoCreateDriver from manually mapped/pool memory may fail with `STATUS_OBJECT_PATH_SYNTAX_BAD` on some Windows builds due to pool-allocation checks. Fallback: fail gracefully or try alternate driver names.

## Implemented Fix (IoCreateDriver)

When `DriverObject` is NULL (kdmapper), we now call `IoCreateDriver(L"FrozenKernel", &initialize_driver)`. The kernel creates a valid driver object and invokes `initialize_driver` with it, allowing IoCreateDevice to succeed.

**If IoCreateDriver fails** (e.g. STATUS_OBJECT_PATH_SYNTAX_BAD on some builds): the driver returns the error. Try normal driver load (SCM) or a kdmapper fork that passes a valid DriverObject.

## Alternative: FlushComm (No Device) Path

The Project3 driver has **FlushComm** mode: no IoCreateDevice, uses FlushFileBuffers + registry. That path avoids DriverObject entirely and is kdmapper-compatible. We switched to device mode for usermode compatibility—could add a kdmapper-only path that uses FlushComm when DriverObject is NULL.

## References

- kdmapper issue #40: NULL pointers in manual map
- kdmapper issue #76: IoCreateDriver STATUS_OBJECT_PATH_SYNTAX_BAD
- TheCruZ: "entry point receive null pointers in manual map"
- Valthrun kdmapper wiki: load sequence and cleaning order
