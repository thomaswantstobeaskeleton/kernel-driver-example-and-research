# Crash Fix: PC Crash After Pressing Enter (Inject)

## Problem
PC crashes (likely BSOD) when pressing Enter in Fortnite to inject/start the cheat.

## Root Causes Addressed

### 1. Driver: Invalid Physical Address → BSOD
**Cause**: `MmMapIoSpaceEx` can cause a system crash if given invalid or device physical addresses (e.g. 0 or out-of-range).

**Fix** (`driver/driver.cpp`):
- **Physical address validation** in `read()` and `write()`:
  - Reject physical page base `0` (null)
  - Reject physical addresses above `0x000FFFFFFFFFF000` (52-bit PA max on x64)
- **Exception handling** in `read()`/`write()`: wrap `RtlCopyMemory` / write loop in `__try/__except` so any access fault returns `STATUS_ACCESS_VIOLATION` instead of propagating and crashing the system.

### 2. Driver: Unhandled Fault in Request Handler → BSOD
**Cause**: If `frw()` / `translate_linear()` / `read_cached()` faulted (e.g. bad CR3 or corrupted page tables), the exception could propagate and cause a BSOD.

**Fix** (`driver/driver.cpp`):
- **FlushComm handler**: Wrapped the entire request switch in `__try/__except(EXCEPTION_EXECUTE_HANDLER)`.
- On exception: set `*pStatus = STATUS_UNSUCCESSFUL`, complete the IRP, then run `__finally` (detach process). No BSOD; usermode sees a failed read.

### 3. Usermode: Detached Thread Use-After-Scope
**Cause**: `std::thread([&]() { Sleep(READ_PAUSE_MS + 2000); CacheLevels(); }).detach()` captured `READ_PAUSE_MS` by reference. If the main thread left scope before the sleep finished, the lambda could read freed stack → undefined behavior / crash.

**Fix** (`core/entrypoint.cpp`):
- Capture by value: `const DWORD pause_ms = READ_PAUSE_MS; std::thread([pause_ms]() { Sleep(pause_ms + 2000); CacheLevels(); }).detach();`
- Other branch: `std::thread([]() { Sleep(3000); CacheLevels(); }).detach();` (no capture needed).

## Files Modified

| File | Changes |
|------|--------|
| `driver/driver.cpp` | Physical PA validation in `read()`/`write()`, `__try/__except` in read/write and in FlushComm request handler |
| `core/entrypoint.cpp` | Detached thread lambda captures pause by value |

## Testing

1. Rebuild the driver and usermode app.
2. Load driver, start Fortnite, get to lobby.
3. Run the cheat, press Enter when prompted (with Fortnite focused).
4. Confirm no PC/BSOD crash; overlay should start and reads may fail gracefully if game is still loading.

## If Crashes Persist

- Check Windows Event Viewer → System for bugcheck code (e.g. DRIVER_IRQL_NOT_LESS_OR_EQUAL, PAGE_FAULT_IN_NONPAGED_AREA).
- Enable `FLUSHCOMM_DEBUG` in the driver and use DbgView to see which request or address fails.
- Ensure the driver is rebuilt and reloaded after these changes.
