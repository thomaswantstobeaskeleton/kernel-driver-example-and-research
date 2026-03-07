# Implementation Prompt: IOCTL Alternatives

**Use this prompt to instruct an AI (or yourself) to implement the IOCTL alternatives from `IOCTL_ALTERNATIVES_RESEARCH.md`.**

---

## Copy-Paste Prompt (below the line)

---

Implement the IOCTL alternatives described in `private/Project3/driver/IOCTL_ALTERNATIVES_RESEARCH.md`: **KSocket (WSK)** and **FILE_OBJECT DeviceObject hooking**.

### Requirements

1. **Read the research document first** – Use it as the authoritative reference for architecture, pros/cons, and external links.

2. **Integrate with existing FlushComm** – The project already uses:
   - Beep/Null/PEAuth MajorFunction hook (IRP_MJ_DEVICE_CONTROL + IRP_MJ_FLUSH_BUFFERS)
   - Section-based shared memory (no MmCopyVirtualMemory)
   - FlushFileBuffers for request signaling
   - Direct syscall for PING
   - Codecave for signed PING when LargePageDrivers is configured

   New channels must coexist; add config flags to enable/disable each.

3. **KSocket (WSK) implementation**
   - Driver: Use WSK to create a TCP server on a configurable loopback port (e.g. 127.0.0.1, port from config or dynamic).
   - Driver: Reuse the existing FlushComm request protocol (magic, REQUEST_TYPE, args, data area) over the socket stream. Implement simple framing (length-prefixed or fixed-size headers) so messages can be parsed reliably.
   - Usermode: Add a transport that connects via Winsock (`socket`, `connect`, `send`, `recv`) to the driver’s port. Use the same request/response layout as the existing section + FlushFileBuffers path.
   - Config: Add `FLUSHCOMM_USE_WSK` (0/1) and `FLUSHCOMM_WSK_PORT` (or 0 for dynamic). When enabled, usermode should prefer the WSK path over DeviceIoControl/FlushFileBuffers when connecting.
   - References: wbenny/KSOCKET, MiroKaku/libwsk, Microsoft WSK docs.

4. **FILE_OBJECT DeviceObject hook implementation**
   - Driver: Implement the Spectre-style technique:
     - Enumerate open handles via `NtQuerySystemInformation(SystemHandleInformation)`.
     - Find FILE_OBJECTs whose `DeviceObject` matches the hooked driver’s device (Beep/Null/PEAuth).
     - Create fake `DRIVER_OBJECT` and `DEVICE_OBJECT` with `ObCreateObject` (see Spectre’s FileObjHook.cpp).
     - Set `MajorFunction[IRP_MJ_DEVICE_CONTROL]` and `IRP_MJ_FLUSH_BUFFERS` on the fake driver to your existing FlushComm handlers.
     - Replace `FILE_OBJECT->DeviceObject` with your fake device for matching handles.
   - This replaces (or optionally runs alongside) the direct MajorFunction hook. When FILE_OBJECT hook is active, the real Beep MajorFunction stays unmodified; IRPs are redirected via the fake device.
   - Config: Add `FLUSHCOMM_USE_FILEOBJ_HOOK` (0/1). When 1, use FILE_OBJECT redirection instead of swapping Beep’s MajorFunction.
   - Handle edge cases: new handles created after hook (consider `IRP_MJ_CREATE` hook or periodic re-enumeration if feasible).

5. **Compatibility**
   - Ensure both alternatives work with section-based shared memory for read/write. WSK and FILE_OBJECT hook are communication *paths*; the actual request processing (frw, fba, CR3, mouse) stays in the existing `FlushComm_ProcessSharedBuffer` flow.
   - PING can be sent over WSK (driver responds on socket) or via the hooked path (FILE_OBJECT or MajorFunction).
   - Build must succeed; both paths should be guarded by `#if FLUSHCOMM_USE_WSK` and `#if FLUSHCOMM_USE_FILEOBJ_HOOK`.

6. **Testing**
   - Document how to test each path (WSK: connect from usermode, verify read/write/CR3; FILE_OBJECT: verify Beep MajorFunction unchanged, handles redirected).
   - Add brief notes to the research doc or a new `IOCTL_ALTERNATIVES_IMPL.md` describing what was implemented and any deviations from the research.

### Deliverables

- Driver changes: WSK server module, FILE_OBJECT hook module, config integration.
- Usermode changes: WSK transport in `utilities/impl/driver.hpp` (or equivalent).
- Config: New defines in `flush_comm_config.h`.
- Docs: Implementation notes and test steps.

Implement incrementally: WSK first, then FILE_OBJECT hook. Ensure the existing FlushComm path remains the default when new options are disabled.
