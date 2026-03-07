# Deep Research: KDMapper Crash at "Driver Loading Completed"

## Crash Timeline

User sees **"Driver loading completed"** as last output, then PC crashes (BSOD).

### LoadDriver Sequence
1. Spawn kdmapper process
2. Wait for kdmapper exit (mapper result 0 = success)
3. Sleep(3000)
4. `open_hooked_device()` – CreateFile on `\\.\Beep` (or Null/PEAuth)
5. `printf("Driver loading completed")`
6. Return to entrypoint

### Entrypoint Sequence (after LoadDriver)
7. Sleep(2500)
8. `find_driver()` – opens section, CreateFile on device, FlushFileBuffers handshake

---

## Potential Crash Triggers (by timing)

| Trigger | When | Location |
|---------|------|----------|
| **MappedInitWorker** | During/after DriverEntry return | Driver: FlushComm_Init, IoGetDeviceObjectPointer, InterlockedExchangePointer, section creation |
| **Intel driver unload** | Right after MapDriver returns | kdmapper: service::StopAndRemove, NtUnloadDriver |
| **open_hooked_device** | Step 4 in LoadDriver | CreateFile → IRP_MJ_CREATE to Beep (we did NOT hook CREATE) |
| **find_driver** | Step 8 in entrypoint | CreateFile + FlushFileBuffers → IRP_MJ_FLUSH_BUFFERS to our handler |

---

## Research Findings

### 1. Manual Map + IRP Handler (Tulach)
- Anticheats check that dispatch routines point to **legitimate drivers' memory regions**
- Our handler lives in **manually mapped memory** (pool allocation)
- RIP in our handler points **outside any loaded module**
- Windows kernel does **not** validate callback addresses; it just calls the pointer
- No direct evidence this causes BSOD, but it is a detection vector

### 2. IoDriverObjectType (Mitigated)
- `ObReferenceObjectByName` with `*globals::IoDriverObjectType` was replaced by **IoGetDeviceObjectPointer**
- IoGetDeviceObjectPointer uses `\Device\Beep` path – no IoDriverObjectType
- Removes dependency on kdmapper IAT resolving data symbols

### 3. Pool vs --indPages
- **--indPages** (independent pages) can cause BSOD on some systems
- Pool allocation is the default and tends to be more stable
- Switched to pool allocation

### 4. Intel Driver Unload
- kdmapper calls NtUnloadDriver to unload the vulnerable driver
- Our mapped driver lives in separate kernel pool; unload should not touch it
- ClearMmUnloadedDrivers uses pattern scans – wrong patterns could corrupt state
- No clear proof this causes the crash

### 5. CreateFile on `\\.\Beep`
- We only hook **IRP_MJ_FLUSH_BUFFERS** and **IRP_MJ_DEVICE_CONTROL**
- CreateFile sends **IRP_MJ_CREATE** – still handled by original Beep
- Opening the device should not invoke our handler
- **SKIP_VERIFY_OPEN** can confirm whether open_hooked_device is involved

### 6. Deferred Init → Sync Init (Fixed)
- Previously: DriverEntry spawned MappedInitWorker thread, returned immediately
- Crash occurred ~2s after "Driver loading completed" – during entrypoint Sleep(2500)
- **Change:** Switched to synchronous init – MappedInitSync() runs directly in DriverEntry

### 7. Section View in KDMapper Process (Root Cause – Fixed)
- **Problem:** ZwMapViewOfSection(ZwCurrentProcess(), …) mapped the section into the kdmapper process
- kdmapper exits right after MapDriver returns; kdmapper process terminates
- g_section_view pointed into kdmapper’s address space – that space is torn down on process exit
- When find_driver ran ~2s later and triggered our IRP handler, the handler accessed g_section_view → PAGE_FAULT
- **Fix:** Map section into the System process (PID 4) instead – it never exits
- Use ZwOpenProcess + CLIENT_ID {4,0} to get a handle, ZwMapViewOfSection(hSection, hSystem, …)
- Store g_section_process; in the IRP handler, call KeStackAttachProcess(g_section_process) before using g_section_view
- Section view remains valid indefinitely; attach/detach ensures correct address space when handling IRPs

---

## Isolation Tests

### Test 1: SKIP_VERIFY_OPEN
Set `SKIP_VERIFY_OPEN 1` in includes.h (or `/DSKIP_VERIFY_OPEN=1`).
- If crash **stops** → CreateFile in LoadDriver was the trigger
- If crash **continues** → Trigger is elsewhere (MappedInitWorker, Intel unload, or find_driver)

### Test 2: Run kdmapper standalone
```cmd
kdmapper.exe --driver intel "path\to\driver.sys"
```
- If crash before "success" → Crash in DriverEntry or MappedInitWorker
- If crash after "success" → Crash in Intel unload or later

### Test 3: Try eneio backend
Use `--driver eneio` if Intel is blocked or suspect.
- Eneio works with HVCI; different exploit, same concept

### Test 4: Normal load (NtLoadDriver)
- Load driver via SCM + NtLoadDriver (no kdmapper)
- Needs test signing or DSE disabled
- If normal load works → Crash is specific to manual mapping

### Test 5: BSOD bugcheck code
- Check `C:\Windows\Minidump\` for `.dmp` files
- Bugcheck codes: `PAGE_FAULT_IN_NONPAGED_AREA`, `SYSTEM_SERVICE_EXCEPTION`, etc.
- Faulting module and offset narrow down the cause

---

## Recommendations

1. **Run with SKIP_VERIFY_OPEN=1** to see if CreateFile is the trigger.
2. **Capture a minidump** and analyze the faulting address.
3. **Try eneio backend** if Intel may be blocked.
4. **Consider NtLoadDriver path** – normal load avoids manual mapping issues.
5. **Disable security software** (AV, anticheat) during tests.
6. **Disable Memory Integrity** (HVCI) if enabled.
