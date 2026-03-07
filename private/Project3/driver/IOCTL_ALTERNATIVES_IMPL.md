# IOCTL Alternatives – Implementation Notes

## KDMapper Compatibility

**Yes – all IOCTL alternatives work with kdmapper manual mapping.**

- **MajorFunction hook**: Usermode opens `\\.\Beep`; kdmapper loads our driver → we hook Beep → IRPs reach our handler. No IOCTL to our driver's own device.
- **FILE_OBJECT hook**: Same flow; fake device created with kdmapper's DriverObject. Redirect happens on create.
- **WSK**: Socket path only; no DeviceIoControl.
- **Section + FlushFileBuffers**: Shared memory + flush for signaling; no traditional IOCTL.

See `ANTI_DETECTION_MEASURES.md` for details.

---

## Summary

Both **FILE_OBJECT DeviceObject hook** and **WSK (Winsock Kernel)** alternatives are implemented behind config flags. By default both are off; existing FlushComm behavior is unchanged.

---

## 1. FILE_OBJECT DeviceObject Hook

### Config

- `FLUSHCOMM_USE_FILEOBJ_HOOK` (default: 0)

### Behavior

- Hooks `IRP_MJ_CREATE` on Beep/Null/PEAuth instead of `IRP_MJ_DEVICE_CONTROL`.
- On successful create, replaces `FILE_OBJECT->DeviceObject` with a fake device.
- Real `MajorFunction[IRP_MJ_DEVICE_CONTROL]` stays unchanged.
- Uses `IoCreateDevice` on our driver to create the fake device (no ObCreateObject).

### Files

- `driver/file_obj_hook.cpp`, `file_obj_hook.hpp`
- `flush_comm.cpp` – FILE_OBJECT init when flag is set

### Enable

Set `#define FLUSHCOMM_USE_FILEOBJ_HOOK 1` in `flush_comm_config.h`.

### Tests

1. Build driver with flag enabled.
2. Load driver, open Beep, send PING.
3. Confirm PING works and `MajorFunction[IRP_MJ_DEVICE_CONTROL]` on the real Beep driver is still the original handler.

---

## 2. WSK (Winsock Kernel)

### Config

- `FLUSHCOMM_USE_WSK` (default: 0)
- `FLUSHCOMM_WSK_PORT` (default: 0 → use 41891)

### Behavior (Current)

- Skeleton WSK server:
  - Resolves WSK routines via `MmGetSystemRoutineAddress`.
  - Registers as WSK client, captures provider, creates listen socket.
- No bind/listen/accept flow or request handling yet.
- Port is written to registry `WskPort` when init succeeds.

### Files

- `driver/wsk_server.cpp`, `wsk_server.hpp`
- `flush_comm.cpp` – WSK init when flag is set

### Enable

Set `#define FLUSHCOMM_USE_WSK 1` in `flush_comm_config.h`.

### Remaining Work

- Implement bind/listen/accept (listen socket is created; bind/listen not yet called).
- Handle incoming connections and process the FlushComm protocol.
- Add usermode Winsock transport in `driver.hpp` (connect, send/recv).
- Define framing for the request/response protocol over TCP.

---

## 3. Usermode

No changes yet. `driver.hpp` always uses DeviceIoControl / FlushFileBuffers. A WSK transport path will be added once the driver WSK path is fully implemented.

---

## 4. Build

- Driver builds with both flags 0 or 1.
- FILE_OBJECT hook is functional.
- WSK is a skeleton and needs the steps above for full use.
