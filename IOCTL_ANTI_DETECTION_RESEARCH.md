# IOCTL Anti-Detection Research

## 1. Files Using IOCTL Codes (Project Overview)

| File | Purpose | IOCTL Codes |
|------|---------|-------------|
| `flush_comm_config.h` | **Single source** for FlushComm | `FLUSHCOMM_IOCTL_FUNC` (0x7A4), `FLUSHCOMM_IOCTL_PING` (0x7A3) |
| `driver/flush_comm.hpp` | FlushComm protocol defines | Pulls from config; `IOCTL_REXCOMM`, `IOCTL_REXCOMM_PING` |
| `driver/flush_comm.cpp` | Registry setup, no IOCTL storage | Only `HookedDevice` written |
| `driver/driver.cpp` | `FlushComm_HookHandler` | Compares `ioctl == IOCTL_REXCOMM_PING` / `IOCTL_REXCOMM` |
| `utilities/impl/driver.hpp` | Usermode driver comm | Uses same via `flush_comm_config.h`; `DeviceIoControl` |
| `driver/driver.cpp` (io_controller) | Pasuhq-style driver path | `FILE_DEVICE_UNKNOWN` codes: RW=0x8A12, BA=0x9B34, etc. |
| `driver/com/driver.hpp` | Alternative driver interface | `0x47536`, `0x36236`, etc. – different device |

### IOCTL Codes That Can Be Changed

| Location | Codes | Changeable? | How |
|----------|-------|-------------|-----|
| `flush_comm_config.h` | `FLUSHCOMM_IOCTL_FUNC` (0x7A4), `FLUSHCOMM_IOCTL_PING` (0x7A3) | **Yes** | Edit config; driver + usermode both include it. Use function codes 1–126 for lower visibility. |
| `driver/driver.cpp` | `IOCTL_FUNC_RW` (0x8A12), `IOCTL_FUNC_BA` (0x9B34), etc., `IOCTL_MOUSE_MOVE` (0x27336) | **Yes** | Edit driver.cpp; if using `driver/com/driver.hpp`, update that file too. FlushComm path uses `REQ_*` types, not these. |
| `driver/com/driver.hpp` | CODE_RW (0x47536), CODE_BA (0x36236), etc. | **Yes** | Must match driver.cpp when using direct device path. |
| KDMapper Intel/ENE IO | Mapper-specific IOCTLs | **No** | Fixed by signed vulnerable driver (capcom.sys, GLCKIo, etc.). |

### Current Detection Risks

- **0x999 / 0x998** are uncommon function codes; 0x000–0x7F (0–127) are typical for Microsoft drivers.
- **Fixed values** in binary → easy signature-based detection.
- **`DeviceIoControl`** can be hooked in usermode; some AC monitors calls to `\\.\Beep` with unusual codes.

---

## 2. Research Sources (UnknownCheats and Others)

### UnknownCheats (https://www.unknowncheats.me)

- **Kernel Communication Methods** – [forum post](https://www.unknowncheats.me/forum/anti-cheat-bypass/565891-kernel-communication-methods.html)
- **Stealthy IOCTL via ConDrv.sys** – Temp hook on IRP_MJ_CREATE; challenges from:
  - `PsLoadedModuleList` enumeration
  - MajorFunction pointer validation (`.text` section)
  - NMI stack-walking on unsigned code
  - Invalid stack traces from paged pool
- **Hooking NtDeviceIoControlFile** – AC monitors this to detect driver comms and spoofing.

### Other References

- **MDSec – Bypassing User-Mode Hooks** – Direct syscalls skip NTDLL hooks.
- **x86matthew – NtDeviceIoControlFile syscall** – Direct syscall invocation for stealth.

---

## 3. Anti-Detection Techniques

### 3.1 Randomize IOCTL Codes at Runtime

- **Driver init:** Pick function codes in range `1–126` (typical for real drivers).
- **Store in registry:** `HKLM\SOFTWARE\rexcomm` → `IoctlMain`, `IoctlPing` (REG_QWORD).
- **Usermode:** Read codes from registry and build `CTL_CODE(FILE_DEVICE_BEEP, fn, ...)` at runtime.
- **Benefits:** No fixed signatures; varies per install.

### 3.2 NtDeviceIoControlFile Direct Syscall

- **Problem:** `DeviceIoControl` → `NtDeviceIoControlFile`; AC can hook `NtDeviceIoControlFile` in ntdll.
- **Mitigation:** Direct syscall stub that invokes `syscall` without calling ntdll.
- **Tools:** Syscall number resolution; inline asm / shellcode for `mov r10, rcx; mov eax, <syscall#>; syscall; ret`.
- **Limitation:** Kernel / mini-filters and ETW still see the activity.

### 3.3 Call Pattern Obfuscation

- **Throttling:** Avoid rapid bursts (e.g. cap calls within 4 ms).
- **Jitter:** 1–3 ms delay between calls to mimic normal usage.
- **Retries:** Spread retries with 500–600 ms delays instead of tight loops.

### 3.4 Realistic IOCTL Construction

- Use `FILE_DEVICE_BEEP` (already in use).
- Use `METHOD_BUFFERED`, `FILE_ANY_ACCESS` (already in use).
- Use function codes in `1–126` instead of 0x998/0x999.

### 3.5 Alternative: IRP_MJ_FLUSH_BUFFERS

- `FlushFileBuffers` uses IRP_MJ_FLUSH_BUFFERS instead of IRP_MJ_DEVICE_CONTROL.
- Generally less monitored by AC.
- Requires changing both usermode calls and driver dispatch hooks.

### 3.6 Kernel-Side Considerations

- **PsLoadedModuleList:** Your driver is mapped by kdmapper; hiding/cleaning traces is already in place.
- **MajorFunction validation:** Handler must live in valid module `.text`. Beep hook runs in signed Beep.sys context when forwarded.
- **Stack traces:** Avoid execution from paged pool or unsigned allocations; prefer inlined handlers or codecaves in signed drivers.

---

## 4. Recommended Implementation Order

1. ~~**Randomize IOCTL codes**~~ – Future: Driver writes `IoctlMain`/`IoctlPing` to registry.
2. **Throttling + jitter** – ✅ Implemented in `send_request()` via `FLUSHCOMM_THROTTLE_MS` / `FLUSHCOMM_JITTER_MS`.
3. **NtDeviceIoControlFile direct syscall** – ✅ Implemented in `direct_syscall.hpp`; used when `FLUSHCOMM_USE_DIRECT_SYSCALL`.
4. **FlushFileBuffers / IRP_MJ_FLUSH_BUFFERS** – ✅ Implemented; driver hooks `IRP_MJ_FLUSH_BUFFERS`; usermode uses `FlushFileBuffers` when `FLUSHCOMM_USE_FLUSH_BUFFERS`.
5. **Section-based shared memory** – Future: `ZwCreateSection` + `MapViewOfFile` for true shared memory; would eliminate MmCopyVirtualMemory for control struct.

## 5. Implemented Anti-Detection (flush_comm_config.h)

| Config | Default | Effect |
|--------|---------|--------|
| `FLUSHCOMM_USE_FLUSH_BUFFERS` | 1 | Use `FlushFileBuffers` instead of `DeviceIoControl` for requests |
| `FLUSHCOMM_THROTTLE_MS` | 2 | Min ms between `send_request` calls |
| `FLUSHCOMM_JITTER_MS` | 2 | Random 0..N ms added to throttle |
| `FLUSHCOMM_USE_DIRECT_SYSCALL` | 1 | Use `NtDeviceIoControlFile` syscall for PING/fallback |

---

## 6. CTL_CODE Layout (Reference)

```
CTL_CODE(DeviceType, Function, Method, Access)
  = (DeviceType << 16) | (Access << 14) | (Function << 2) | Method
```

For `FILE_DEVICE_BEEP` (0x1), `METHOD_BUFFERED` (0), `FILE_ANY_ACCESS` (0):
- `(0x1 << 16) | (0 << 14) | (fn << 2) | 0`
- Function codes 0x999/0x998 → large; use 1–126 for lower visibility.

---

## 7. Registry Keys (Current vs Proposed)

| Key | Current | Proposed |
|-----|---------|----------|
| `SharedBuffer` | ✓ | ✓ |
| `SharedPid` | ✓ | ✓ |
| `HookedDevice` | ✓ | ✓ |
| `IoctlMain` | ✗ | Add (randomized at driver init) |
| `IoctlPing` | ✗ | Add (randomized at driver init) |
