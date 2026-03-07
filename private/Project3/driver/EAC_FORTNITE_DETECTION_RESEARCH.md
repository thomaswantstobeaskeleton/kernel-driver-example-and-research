# EAC/Fortnite Detection Research – Current Methods

Research from web sources (UnknownCheats, GitHub, Elastic Security, EAC reversing) on whether FlushComm's current communication methods are detected by Easy Anti-Cheat in Fortnite.

**Last updated:** Feb 2026 – includes late 2025 and 2026 sources.

---

## Socket / WSK Communication – **DETECTED**

**Source:** UnknownCheats – Swiftik (2021, high-rep user)

> *"If you're going to use sockets or shared memory for your communication, you will need to hide your threads."*

Socket/WSK communication creates kernel or system threads that EAC monitors. EAC `ScanSystemThreads()` walks thread stacks and checks that return addresses (RIP) lie in known system modules.

**Recommendation:** Keep `FLUSHCOMM_USE_WSK 0`. WSK (Winsock Kernel) uses listen/accept patterns that rely on worker or dedicated threads, which EAC can detect unless thread hiding (spoof/unlink, hide call stack, spoof start address) is implemented.

---

## Shared Memory – **THREAD-DEPENDENT**

The same post states that **shared memory** communication also requires **thread hiding** when used with dedicated communication threads.

**FlushComm section mode:** Uses `ZwCreateSection` (driver) and `MapViewOfFile` (usermode). **No dedicated communication thread** – IRPs are handled in the caller’s context when `FlushFileBuffers` is invoked. The usermode thread enters kernel mode to handle the IRP and returns. The section is only a data buffer; no listener thread is involved.

**Conclusion:** Section-based shared memory as used (no socket/listener thread) does **not** use the same model as the “sockets or shared memory” pattern that Swiftik says needs thread hiding.

---

## Beep Hook / FlushFileBuffers – **NOT EXPLICITLY DISCUSSED**

Public sources do not show EAC specifically checking for Beep IRP hooks or `FlushFileBuffers` usage. Several repos (e.g. `Shared-FlushFileBuffers-Communication`) use this pattern for kernel communication. The IRP is handled synchronously in the usermode caller’s thread context.

---

## MajorFunction Hooking – **KNOWN RISK**

**Source:** UnknownCheats, EAC reversing

- EAC monitors syscall hooks in `win32kbase` / `ntoskrnl` and uses `.data` pointer / VMT checks.
- MajorFunction hooks (replacing `IRP_MJ_DEVICE_CONTROL` / `IRP_MJ_FLUSH_BUFFERS`) can be discovered by scanning driver function tables.
- A 2020/2021 UC post notes that EAC’s “December update tapped” a syscall hook.
- **Mitigation:** `FILE_OBJECT` hook (redirecting `FILE_OBJECT->DeviceObject`) leaves the original MajorFunction table intact and can reduce hook-based detection.

---

## Thread Detection – **RELEVANT TO MOUSE INJECTION**

**Source:** EACReversing `systemthread.c`, Elastic Security

`ScanSystemThreads()`:

- Walks stack frames of system threads (from thread ID 4 onward).
- Validates that each frame’s RIP maps to known kernel modules.
- Flags threads with stack frames pointing to non-module memory (e.g. pool, manual-mapped code).

**FlushComm mouse injection:** Uses `IoQueueWorkItem` (DelayedWorkQueue). Work runs on a **Windows system worker thread**, but the callback `mouse_work_routine` is in the manual-mapped driver. During a stack walk, EAC can see a frame pointing to unlisted driver code.

**Risk:** Medium. Worker threads themselves are normal; the risk is RIP pointing to manual-mapped code. Using a codecave in a signed driver for mouse injection would reduce this, but the main request path remains in the mapped driver.

---

## Other Known EAC Checks

| Vector | EAC Behavior | FlushComm Status |
|--------|--------------|------------------|
| **PidbbCache / MmUnloadedDrivers** | Scan for mapper traces | Trace cleaner in place |
| **Big pool** | Scan big pool table | kdmapper pool mode; optional spoofing |
| **Strings/signatures** | String and signature scans | Registry XOR, obfuscation |
| **Manual mapping** | Check for uncleared traces | Trace cleaner |

---

## Summary: Current FlushComm Methods

| Method | EAC Detection Risk | Notes |
|--------|-------------------|------|
| **Beep MajorFunction hook** | Medium | Possible driver function-table scan |
| **FlushFileBuffers** | Low | No public EAC-specific mentions |
| **Section shared memory** | Low | No extra threads, handled in caller context |
| **Direct syscall (PING)** | Low | Bypasses usermode hooks |
| **WSK (socket)** | **High** | Do not enable; requires full thread hiding |
| **IoQueueWorkItem (mouse)** | Medium | Worker thread callback in mapped code can show up in stack |
| **FILE_OBJECT hook** | Lower | Keeps original MajorFunction intact |

---

## Recommendations (Implemented)

1. **Keep `FLUSHCOMM_USE_WSK 0`** – Done. Config comment warns about EAC detection.
2. **Prefer section + FlushFileBuffers** – Already in use.
3. **Prefer FILE_OBJECT hook** – **Implemented:** `FLUSHCOMM_USE_FILEOBJ_HOOK 1`. Real Beep MajorFunction unchanged.
4. **Mouse injection** – **Implemented:** `FLUSHCOMM_MOUSE_SYNC 1`. Uses synchronous `move()` instead of `move_async` (IoQueueWorkItem). No worker thread with mapped code on stack.
5. **Trace cleaning** – Already in use.
6. **Section name** – `FlushComm_22631` may be signature material; consider build-time variation if needed.

---

## 2026 EAC/Fortnite Updates

### Fortnite Tournament Requirements (February 19, 2026)

- **Epic Games** expanded PC anticheat for Fortnite *tournaments*: **Secure Boot**, **TPM 2.0**, **IOMMU**.
- **IOMMU** (new) targets DMA/hardware cheats – PCIe devices that read/write memory directly. IOMMU isolates devices and blocks unauthorized DMA.
- **Secure Boot** blocks bootkits and unauthorized drivers at startup.
- **Scope:** Tournament participants only; casual play unchanged. ~95% of PC players compatible.
- **Source:** BigGo Finance, DSOGaming, GamingBible, GameRant, Polygon, Tom's Hardware, Feb 2026.

### EAC Kernel Role Unchanged

- DSOGaming (2026): *"Epic uses Easy Anti-Cheat's kernel-level protection to secure the game while it is running. Epic monitors suspicious behavior using data analysis and machine learning."*
- 2026 focus is **hardware** (DMA, bootkits), not kernel driver communication protocol.
- **No 2026 evidence** that EAC added IRP_MJ_FLUSH_BUFFERS or FlushFileBuffers-specific checks.
- EAC's own driver uses encrypted IOCTL internally (Hypercall); detection targets external cheat drivers (threads, modules, handles, integrity), not whether they use IOCTL vs FlushFileBuffers.

### Implications for FlushComm

- FlushFileBuffers + section + FILE_OBJECT conclusion **unchanged** for Fortnite/EAC in 2026.
- Hardware requirements (Secure Boot, TPM, IOMMU) do not affect software kernel comm choice.

---

## Late 2025 UnknownCheats (Same Sources, Recent Data)

### UC 683599 – VGK/ACE/FACEIT Detect DMA (Jan–April 2025)

- **sushi404 (April 2025):** *"They [BE and EAC] focus on the category of cheaters that represent the majority, which are usermode/kernel mode memory based ones."* EAC/BE could detect DMA with more PCIe/driver telemetry but that would be more invasive.
- **rhaym (April 2025):** VGK uses IOMMU to deny DMA access to game memory. EAC/BE *"focus on detecting more common cheating methods"* – kernel/usermode memory.
- **versager (Jan 2025):** Cheap DMA detected via *"missing driver communication... If it doesn't act like a typical hardware, its most certainly not a typical hardware."*

### UC 677857 – EAC Runs at Boot (Dec 2024)

- Claim: Fortnite EAC now runs at boot like Vanguard. **Disputed as misinformation** by high-rep users; no proof. Unconfirmed.

### UC 688387 – EAC Bypass Beginners (Feb 23–25, 2025)

- plur53: manual mapping is "the most common way" for EAC; Fortnite/Rust. No comm-protocol specifics.

### UC 688145 – EAC Linux (Feb–Aug 2025)

- EAC on Linux runs solely in userspace; kernel module + ioctl not monitored. Aug 2025: ptrace, /proc mem. Different platform; no Windows FlushFileBuffers/IOCTL data.

### UC 682955, 678400 (2025)

- KMDU kernel dumper (Jan 2025); undetected injector EAC (Dec 22, 2024). No FlushFileBuffers/IOCTL specifics.

### Implication

- EAC/BE primary focus remains **kernel/usermode memory cheats** (sushi404). No 2025 UC data indicates EAC added IRP_MJ_FLUSH_BUFFERS or FlushFileBuffers-specific checks. FlushFileBuffers vs IOCTL conclusion unchanged.

---

## UnknownCheats & GitHub – Technical Sources

### EAC Detection (UC Thread 438804 – Swiftik)

- **pidbbcache / MmUnloadedDrivers** – Must clear mapper/vulnerable driver traces
- **Big pool** – EAC scans; spoof entries recommended
- **Threads** – Sockets/shared memory with dedicated threads need hiding: unlink, spoof call stack, randomize start address
- **Hooks** – Syscall hooks in win32kbase/ntoskrnl; route to legitimate driver memory, not suspicious locations

### ConDrv Stealth IOCTL (UC Post 4407129)

- **MajorFunction verification** – ACs verify pointers point to valid .text addresses; hooks in paged pool fail
- **NMI stack walking** – EAC can detect calls to unsigned code via NMI; applies to any unsigned kernel execution
- **Stack trace** – Dynamic dispatch handlers outside registered modules can be flagged

### EAC Unsigned Driver Scan (UC Thread 635703, May 2024)

- EAC actively scans for unsigned drivers loaded via DseFix etc.; can block game launch

### kdmapper Detection (UC Threads 594960, 614327)

- Public kdmapper detected on EAC/BE; wdfilter traces, mapper fingerprints
- Sinmapper READ_EXECUTE patches detected on both EAC and BE

### GitHub – Alternative Comm Methods

| Project | Method | Notes |
|---------|--------|-------|
| **boom-cr3/Shared-FlushFileBuffers** | FlushFileBuffers + shared buffer | IRP_MJ_FLUSH_BUFFERS "not checked for most drivers"; UC 448472 |
| **Poseidon** | Shared memory loop, .data hook | No threads/hooks/driver objects; 2023: BE/EAC detection vectors exist |
| **KUCSharedMemory** | Manual-map + shared memory | "Avoids IOCTL issues with manually mapped drivers"; TODO: NMI bypass |
| **Spectre** | FILE_OBJECT DeviceObject swap | Intercept IOCTL without modifying real driver object |
| **SharedMemory-By-Frankoo** | Section-based shared memory | Synced read/write KM-UM |

---

## References

- **UnknownCheats:** 683599 (VGK/ACE/EAC DMA, Jan–Apr 2025), 688387 (EAC bypass beginners, Feb 2025), 688145 (EAC Linux, Feb–Aug 2025), 682955 (KMDU, Jan 2025), 678400 (injector, Dec 2024), 677857 (EAC boot, Dec 2024), 438804 (EAC/BE kernel bypass), 4407129 (ConDrv), 448472 (FlushFileBuffers), 635703 (unsigned driver scan), 594960/614327 (kdmapper)
- **GitHub:** boom-cr3/Shared-FlushFileBuffers-Communication, sondernextdoor/Poseidon, D4stiny/spectre, benheise/KUCSharedMemory
- **2026:** BigGo, DSOGaming, GamingBible, GameRant, Polygon, Tom's Hardware (Fortnite anticheat Feb 2026)
- **2026:** thefix.it.com (EAC error guide), hypercall.net (EAC architecture)
- EACReversing: `EasyAntiCheat.sys/systemthread.c` – ScanSystemThreads
- Elastic Security Labs: "Doubling Down: Detecting In-Memory Threats with Kernel ETW Call Stacks"
- GitHub: `Shared-FlushFileBuffers-Communication`, `boom-cr3`
