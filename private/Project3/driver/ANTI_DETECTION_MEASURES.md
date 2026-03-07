# Anti-Detection Measures & Obfuscation Techniques

Research-backed techniques for reducing kernel driver and usermode detection by EAC, BattlEye, and similar anti-cheats.

---

## KDMapper + IOCTL Alternatives Compatibility

**Yes – you can still manually map with kdmapper when using these IOCTL alternatives.**

| Method | Works with KDMapper? | Why |
|--------|---------------------|-----|
| **MajorFunction hook** | ✅ Yes | Usermode opens `\\.\Beep` (existing device). kdmapper loads our driver → we hook Beep. IRPs go to our handler. No IOCTL to our driver's device. |
| **FILE_OBJECT hook** | ✅ Yes | Same flow. We create a fake device via `IoCreateDevice(DriverObject, ...)`. kdmapper provides DriverObject in DriverEntry. FILE_OBJECTs redirect to our fake device. |
| **WSK (socket)** | ✅ Yes | No DeviceIoControl at all. Driver registers as WSK client and listens. Usermode connects via Winsock. Completely independent of IOCTL. |
| **Section + FlushFileBuffers** | ✅ Yes | Shared memory + `FlushFileBuffers` for signaling. No traditional IOCTL to our device. PING still uses Beep's handle, but that's the hooked path. |

The confusion comes from: *"IOCTL to manually mapped driver = crash"*. That applies when usermode opens **our** driver's device (e.g. `\\.\OurGUIDDevice`). We don't do that. We use Beep (or WSK). kdmapper only needs to load our code and call DriverEntry; communication goes through Beep or sockets.

---

## Implemented Measures

| Technique | Status | Location |
|-----------|--------|----------|
| IOCTL obfuscation | Done | `flush_comm_config.h` – custom codes |
| Registry value names XOR | Done | `flush_comm_obfuscate.h` |
| Benign pool tags | Done | `page_evasion.hpp` – Nls, Envl, MmPr, Orwk |
| Direct syscall (PING) | Done | `direct_syscall.hpp` – bypass ntdll hooks |
| FlushFileBuffers path | Done | Alternative to DeviceIoControl for requests |
| Section shared memory | Done | No MmCopyVirtualMemory |
| Throttling + jitter | Done | `FLUSHCOMM_THROTTLE_MS`, `FLUSHCOMM_JITTER_MS` |
| Signed-area codecave | Done | PING from Beep .data when LargePageDrivers set |
| Trace cleaner | Done | MmUnloadedDrivers cleared for vuln driver |
| FILE_OBJECT hook | Done | Reduces MajorFunction-based detection |
| WSK skeleton | Done | Non-IOCTL path (bind/listen pending) |
| **ETW patch (usermode)** | Done | `utilities/etw_patch.hpp` – `FLUSHCOMM_PATCH_ETW` |
| **Lazy import skeleton** | Done | `utilities/lazy_import.hpp` – `FLUSHCOMM_USE_LAZY_IMPORT` |

---

## Additional Techniques (Research-Backed)

### 1. ETW Patching (Usermode) – Implemented

- Patch `ntdll!EtwEventWrite` early to no-op (xor rax,rax; ret).
- Reduces ETW telemetry (often used by AC/EDR).
- **Risk**: Some ACs detect ETW patches; enable via `FLUSHCOMM_PATCH_ETW 1`.

### 2. Lazy / Dynamic Import – Skeleton Ready

- `utilities/lazy_import.hpp`: resolve CreateFileW, DeviceIoControl, Reg\*, etc. via GetProcAddress.
- Hides APIs from IAT static analysis. Set `FLUSHCOMM_USE_LAZY_IMPORT 1` and refactor driver.hpp to use `LazyImport::*`.

### 3. String Encryption

- **xorstr** / **skCrypter**: Compile-time string encryption.
- Strings not stored plainly in `.rdata`; decryption at runtime.
- **skCrypter**: Supports kernel mode, randomized per build.

### 4. Callback Evasion

- ACs scan PsSetCreateProcessNotifyRoutine, PsSetLoadImageNotifyRoutine, etc.
- Our driver avoids registering these.
- WSK/NPI registration is less common as a detection target.

### 5. Pool Tag Randomization

- **Implemented**: `FLUSHCOMM_POOL_TAG_ROTATE` in page_evasion – picks from benign tags (Nls, Envl, MmPr, Orwk, Fls, Cc, Io, Mm) via `KeQueryPerformanceCounter` at init.
- Requires `page_evasion_init()` before any allocation and using `EVASION_POOL_TAG_*_R` in code.

### 6. Request Timing Obfuscation

- Variable delays between requests (already via `FLUSHCOMM_JITTER_MS`).
- Human-like patterns instead of fixed intervals.

### 7. RIP / Stack Validation

- Execute hot paths from signed modules (codecave).
- Avoid execution from pool or clearly unmapped regions during sensitive operations.

### 8. Big Pool / Thread Spoofing (Advanced)

- **Big Pool**: EAC scans `NtQuerySystemInformation(SystemBigPoolInformation)` for suspicious allocations; kdmapper pool mode clears these; custom mappers may need to spoof or avoid big pool.
- **Thread hiding**: Unlink threads from kernel lists or spoof stack traces/start addresses if AC monitors system thread creation.

### 9. PE Header / Control Flow Hiding

- Manually mapped drivers often zero PE headers to avoid signature scanning.
- MSVC security features (CFG, /GS) in mapped code can be heuristic indicators – consider stripping or adjusting in custom builds.

### 10. VAD / PTE / Page Spoofing (Advanced, Version-Dependent)

- **MiUnlinkPage**, PTE manipulation, VAD unlinking: hides pages from pool/VA scanners.
- Requires resolving undocumented ntoskrnl symbols; patterns vary per Windows build.
- See: Kernel-VAD-Injector, revers.engineering/hiding-drivers. Our `page_evasion.hpp` documents this.

### 11. PiDDBCacheTable Cleanup (Optional Extension)

- Trace cleaner already clears MmUnloadedDrivers. PiDDBCacheTable requires pattern scan for PiDDBLock + RtlEnumerateGenericTableAvl.
- See: TraceCleaner, x64DriverCleaner, PiDDBCacheTable repos. Placeholder in `trace_cleaner.hpp`.

---

## Detection Vectors to Avoid

| Vector | Mitigation |
|--------|------------|
| IOCTL code scan | Custom codes, FlushFileBuffers, WSK |
| MajorFunction modification | FILE_OBJECT hook (real MajorFunction unchanged) |
| Pool tag scan | Benign tags, runtime rotation (FLUSHCOMM_POOL_TAG_ROTATE) |
| PsLoadedModuleList | Mapped driver not in list; execution from codecave/signed areas |
| MmUnloadedDrivers | Trace cleaner – clears vuln driver entries |
| PiDDBCacheTable | Optional PiDDB cleanup (pattern scan per build) |
| Big pool scan | kdmapper pool mode; avoid suspicious allocation sizes |
| Callback enumeration | No process/image notify callbacks |
| ETW telemetry | Optional ETW patch (FLUSHCOMM_PATCH_ETW) |
| Static strings | Registry names XOR (flush_comm_obfuscate.h), xorstr |
| IAT analysis | Direct syscall (NtDeviceIoControlFile), lazy import (FLUSHCOMM_USE_LAZY_IMPORT) |

---

## References

- EVASION_TECHNIQUES_RESEARCH.md
- IOCTL_ALTERNATIVES_RESEARCH.md
- SIGNED_AREAS_RESEARCH.md
- skCrypter: https://github.com/skadro-official/skCrypter
- xorstr: https://github.com/JustasMasiulis/xorstr
- EAC integrity bypass (secret.club)
- UC: EAC instrumentation callback bypass
