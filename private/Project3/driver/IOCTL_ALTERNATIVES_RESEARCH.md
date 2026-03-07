# IOCTL Alternatives Research: KSocket (WSK) & Data Pointer Hooking

## Overview

This document researches alternatives to `DeviceIoControl` / `NtDeviceIoControlFile` for kernel–usermode communication, with focus on **KSocket (Winsock Kernel)** and **data pointer hooking** (FILE_OBJECT DeviceObject redirection).

---

## 1. KSocket / WSK (Winsock Kernel)

### What It Is

**Winsock Kernel (WSK)** is the kernel-mode network programming interface. It lets drivers use TCP/UDP sockets similar to usermode Winsock2. No `DeviceIoControl` is involved—communication is purely over the network stack.

### How It Works as IOCTL Alternative

| Component | Role |
|-----------|------|
| **Kernel driver** | Uses WSK to create a TCP server (`bind` + `listen` + `accept`) or client (`connect` + `send`/`recv`) |
| **Usermode app** | Uses normal Winsock (`connect`, `send`, `recv`) to `127.0.0.1:port` |
| **Data path** | Network stack (loopback) instead of I/O manager → IRP → IOCTL handler |

### Pros

- **Different code path**: No `NtDeviceIoControlFile`, no IRP_MJ_DEVICE_CONTROL
- **No IOCTL surface**: AC scanning for IOCTL handlers won't see this
- **Standard APIs**: Usermode uses `socket`/`connect`/`send`/`recv`—common, less suspicious than `DeviceIoControl` patterns
- **Async I/O**: WSK supports overlapped I/O

### Cons

- **Network visibility**: Loopback traffic visible via `netstat`, WFP, ETW (Microsoft-Windows-TCPIP)
- **More setup**: WSK client registration, provider, socket lifecycle
- **Firewall/AV**: Some products monitor loopback; less common but possible
- **Driver surface**: Driver must register as WSK client, which can be enumerated

### Implementation References

- **wbenny/KSOCKET** – Basic WSK TCP client/server (HTTP GET, listen on port): https://github.com/wbenny/KSOCKET  
- **MiroKaku/libwsk** – Full BSD socket-like API for WSK: https://github.com/MiroKaku/libwsk  
- **Microsoft WSK docs** – https://learn.microsoft.com/en-us/windows-hardware/drivers/network/introduction-to-winsock-kernel  

### Feasibility for FlushComm-Style Usage

| Aspect | Notes |
|--------|-------|
| Protocol | Need custom framing (magic, type, args) over TCP stream |
| Latency | Loopback is fast; comparable to buffered IOCTL |
| Compatibility | Requires WSK registration in driver; more moving parts than IRP hook |
| Stealth | Avoids IOCTL dispatch path; loopback traffic may still be observable |

---

## 2. Data Pointer Hooking (FILE_OBJECT DeviceObject Redirection)

### What It Is

Instead of hooking `MajorFunction[IRP_MJ_DEVICE_CONTROL]` on a *real* driver (easy to scan), you redirect I/O by swapping the **`DeviceObject` pointer** inside the **`FILE_OBJECT`** for open handles. The IRP then goes to *your* device/driver, not the original one.

### How It Works (Spectre Rootkit Approach)

1. **Enumerate handles** – `NtQuerySystemInformation(SystemHandleInformation)` to get open handles
2. **Find target FILE_OBJECTs** – Filter by type (file) and by `DeviceObject` matching the target device (e.g. Beep, Afd, etc.)
3. **Create fake objects** – Use `ObCreateObject` to create:
   - Fake `DRIVER_OBJECT` (copy of real one, or minimal)
   - Fake `DEVICE_OBJECT` (linked to fake driver)
4. **Hook fake driver** – Set `MajorFunction[IRP_MJ_DEVICE_CONTROL]` (and others) on your fake driver to your handler
5. **Redirect FILE_OBJECTs** – Overwrite `FILE_OBJECT->DeviceObject` with your fake device

When the app calls `NtDeviceIoControlFile`:

- `IoGetRelatedDeviceObject(FileObject)` returns *your* fake device
- `IoCallDriver` uses the fake device's driver → *your* MajorFunction handler
- The real driver is never invoked for those handles

### Why It Evades Classic Detection

| Detection | Why It Fails |
|-----------|--------------|
| **MajorFunction scan** | AC scans real `DRIVER_OBJECT`s. Your hook lives in a *fake* driver not in PsLoadedModuleList. |
| **Driver object integrity** | Real driver's MajorFunction is untouched. |
| **Per-handle** | Hook is in `FILE_OBJECT->DeviceObject`. To find it, AC must enumerate all handles → FILE_OBJECT → DeviceObject. |

### Cons

- **Handle enumeration** | Need to find the right open handles (e.g. Beep); new handles created after hook may still use real device unless you also hook `IRP_MJ_CREATE`
- **ObCreateObject** | Undocumented/internal; may change across builds
- **Complexity** | More code than a simple MajorFunction swap
- **Handle lifetime** | If app closes and reopens, new FILE_OBJECT may need re-hooking

### Reference

- **D4stiny/spectre** – “Hooking IOCTL Communication via Hooking File Objects”: https://github.com/D4stiny/spectre/wiki/Hooking-IOCTL-Communication-via-Hooking-File-Objects  

### Feasibility for FlushComm

| Aspect | Notes |
|--------|-------|
| Current design | FlushComm hooks Beep’s MajorFunction directly. FILE_OBJECT hook would *replace* that: usermode still opens Beep, but we redirect to our fake device. |
| Detection trade-off | Harder to find than MajorFunction hook; requires handle/FILE_OBJECT enumeration. |
| Compatibility | Requires ObCreateObject, SYSTEM_HANDLE_INFORMATION, correct object layout. |

---

## 3. Other IOCTL-Related Options

### METHOD_NEITHER / Direct Buffer

- **Buffering**: I/O manager passes user-mode buffer pointers directly; no `SystemBuffer`.
- **Still IOCTL**: Same `NtDeviceIoControlFile` path; only buffer handling changes.
- **Use case**: Large buffers, avoid double copy. Not an alternative to IOCTL itself.

### Section-Based Shared Memory (Already Used)

- **Current FlushComm**: Uses `ZwCreateSection` + `MapViewOfFile` for shared memory.
- **Data path**: Usermode writes → section; driver reads. No IOCTL for data; IOCTL/FlushFileBuffers only used as *signal*.
- **Relation**: Complements IOCTL; doesn’t replace the signaling mechanism.

### ALPC (Advanced Local Procedure Call)

- **Scope**: Primarily usermode–usermode. Kernel–usermode ALPC is possible but less documented.
- **References**: pTerrance/alpc-km-um, Windows ALPC internals.
- **Verdict**: Interesting for future research; more niche than WSK for driver comms.

---

## 4. Comparison Summary

| Method | IOCTL Used? | Detection Surface | Complexity |
|--------|-------------|-------------------|------------|
| **Current (MajorFunction hook)** | Yes (Beep IRP_MJ_DEVICE_CONTROL) | MajorFunction scan, IRP path | Low |
| **FlushFileBuffers** (current) | No for data; yes for PING | IRP_MJ_FLUSH_BUFFERS, section | Low |
| **WSK / KSocket** | No | Loopback, WSK registration | High |
| **FILE_OBJECT DeviceObject hook** | Yes (IRP still sent) | Per-handle; harder to enumerate | Medium–High |

---

## 5. Recommendations

1. **FlushFileBuffers + section** – Already in use; minimizes IOCTL use for data. Keep.
2. **KSocket** – Consider for a secondary channel if you want a fully non-IOCTL path. Evaluate loopback/ETW visibility.
3. **FILE_OBJECT hook** – Strong option to reduce MajorFunction-based detection. Worth a proof-of-concept alongside the current Beep MajorFunction hook.
