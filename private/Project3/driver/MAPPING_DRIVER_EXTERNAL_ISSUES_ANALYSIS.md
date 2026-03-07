# Deep Analysis: Mapping / Driver / External Communication Issues

## Symptom

kdmapper loads the driver successfully, but `find_driver()` fails with:
```
registry/Beep ok but driver did not respond to REQ_INIT
```

This means: CreateFile(\\.\Beep) succeeds, but the REQ_INIT handshake (FlushFileBuffers + shared buffer) never completes—`*pStatus` stays 0xDEADBEEF.

---

## Root Cause 1: NULL DriverObject from kdmapper (PRIMARY)

**kdmapper passes `param1=0, param2=0`** to DriverEntry by default (main.cpp:224).

- `DriverObject` = 0 (NULL)
- `RegistryPath` = 0 (NULL)

**FlushComm_Init** requires a valid DriverObject when `FLUSHCOMM_USE_FILEOBJ_HOOK=1`:

```c
// flush_comm.cpp:125-129
if (!DriverObject) {
    message("FILE_OBJECT hook requires DriverObject\n");
    return STATUS_INVALID_PARAMETER;
}
```

**Effect:**
- FlushComm_Init fails immediately
- No section is created
- No FILE_OBJECT hook is installed on Beep
- No fake device is created
- Usermode opens the real Beep; FlushFileBuffers goes to the real Beep driver
- Real Beep ignores our protocol; it just completes the IRP
- Our FlushComm_FlushHandler never runs
- `*pStatus` stays 0xDEADBEEF

**Fix:** Use `IoCreateDriver` when DriverObject is NULL to obtain a valid DriverObject, then run FlushComm init with it.

---

## Root Cause 2: Section Namespace Mismatch (when RC1 is fixed)

**Driver creates:** `\BaseNamedObjects\WdfCtl_xxx`

**Usermode opens:** `"WdfCtl_xxx"` (bare) or `"Local\WdfCtl_xxx"` or `"Global\WdfCtl_xxx"`

**Windows object namespace (from Tyranid / MSDN):**
- Session 0 (services): uses `\BaseNamedObjects` (global)
- Session 1+ (user): uses `\Session\{ID}\BaseNamedObjects`
- `Global\X` = `\BaseNamedObjects\Global\X` (cross-session visible)
- `Local\X` = session-specific BNO
- Bare `X` = session-specific BNO

**The kernel** creates `\BaseNamedObjects\WdfCtl_xxx` in the system / global BNO.

**User-mode** (session 1) opening `"WdfCtl_xxx"` (no prefix) resolves to `\Sessions\1\BaseNamedObjects\WdfCtl_xxx` — a different path. The section is not found.

**Fix:**
1. Driver: create section at `\BaseNamedObjects\Global\WdfCtl_xxx` so it is global.
2. Usermode: try `"Global\WdfCtl_xxx"` first (most likely to work cross-session).

---

## Root Cause 3: Buffer Layout Mismatch (section vs registry)

When **section** works:
- Usermode: `buf+8` = type, `buf+80` = status (flush_comm.hpp FLUSHCOMM_DATA_OFFSET=88)
- Driver: `base+8` = type, `base+80` = pStatus

When **registry** fallback:
- Usermode: REQUEST_DATA at buf+16, Status at buf+64
- Driver registry path: expects REQUEST_DATA at sharedBufAddr+16, Status in reqData.Status

Layout is different. If usermode thinks it has section (e.g. from stale state) but driver uses registry, or vice versa, handshake can fail. Keeping section as primary and registry as last resort reduces this; both sides must consistently know which mode is active.

---

## Root Cause 4: FILE_OBJECT Redirect Timing

The redirect happens in **IRP_MJ_CREATE completion**:

1. User CreateFile(\\.\Beep) → IRP to Beep.
2. Our hook (Hook_CreateWithCompletion) adds completion, forwards to real Beep.
3. Real Beep completes; our completion runs.
4. We set `fileObj->DeviceObject = g_fake_device`.

If the completion routine never runs (e.g. create fails, or our hook failed), the FILE_OBJECT still points to the real Beep, and FlushFileBuffers would go to the real device.

---

## Root Cause 5: MajorFunction Hook vs FILE_OBJECT Hook

- **MajorFunction hook** (FLUSHCOMM_USE_FILEOBJ_HOOK=0): directly patches Beep’s MajorFunction. Simpler, works even with NULL DriverObject for our handler logic, but:
  - Caused PC crash in testing (possible conflicts with kdmapper or pool state).
  - More detectable by anti-cheat.

- **FILE_OBJECT hook** (FLUSHCOMM_USE_FILEOBJ_HOOK=1): requires a valid DriverObject to create the fake device. Cleaner but breaks when DriverObject is NULL.

---

## Fix Summary

| Issue | Fix |
|-------|-----|
| NULL DriverObject | Th3Spl-style CreateDriver_ObCreatePath (ObCreateObject+ObInsertObject) - bypasses PsLoadedModuleList, EtwTiLogDriverObjectLoad |
| Section namespace | Driver creates `\BaseNamedObjects\Global\WdfCtl_xxx` |
| Usermode section open | Try `Global\WdfCtl_xxx` first |
| Layout mismatch | Keep section primary, registry fallback; document which layout each path uses |
| IoCreateDriver ETW/telemetry | Use custom path - no ntoskrnl IoCreateDriver call, no EtwTiLogDriverObjectLoad |
| Driver name enumeration | Random name `\Driver\%08X` from KeQueryUnbiasedInterruptTime |

---

## References

- kdmapper README: "DriverObject and RegistryPath are NULL" in manual map
- KDMAPPER_CRASH_ANALYSIS.md
- Tyranid: A Brief History of BaseNamedObjects
- MSDN: Kernel object namespaces, Global\ / Local\ prefix
