# IOCTL & MmCopyVirtualMemory Detection – Deep Research (UnknownCheats 2022–2026)

**Purpose:** Document IOCTL and MmCopyVirtualMemory detection status from UnknownCheats and similar sources, plus current project usage.

---

## Executive Summary (UnknownCheats Consensus)

| Technique | Detection Status | UC Sources |
|-----------|------------------|------------|
| **IOCTL / DeviceIoControl / NtDeviceIoControlFile** | **Heavily detected** | ConDrv thread (bxtnope, heapallocation), UC 438804, 647940, 565891 |
| **MmCopyVirtualMemory** | **Detected** (EAC/Fortnite); flagged/bans reported | UC 496628 (HadessUC, 0xEXP, xtremegamer1); mutable "detected on EAC" |
| **MmCopyMemory** | **EAC can cause BSOD** when used from non-legit memory | UC 496628 (xtremegamer1, novak80) |
| **MajorFunction hooks** (IRP_MJ_DEVICE_CONTROL) | **Scanned** – pointers must be in .text of legit driver | bxtnope: "pagedpool != .text, boom flagged"; heapallocation: NMI stackwalking |
| **Physical memory read** (MmMapIoSpace, CR3, MDL) | **Preferred** – avoids MmCopyVirtualMemory | UC 496628 (mutable, Frostiest), driver.cpp "no MmCopyMemory (EAC traces it)" |
| **FlushFileBuffers / IRP_MJ_FLUSH_BUFFERS** | **Less monitored** than IRP_MJ_DEVICE_CONTROL | FoxiTV/boom-cr3, UC 448472 |
| **Section shared memory** | **Avoids MmCopyVirtualMemory** for UM↔KM data | KUCSharedMemory, project design |

---

## Part 1: IOCTL Usage in This Project

### Active Path (FlushComm – `utilities/impl/driver.hpp`)

| Component | IOCTL Used? | Details |
|-----------|-------------|---------|
| **Handshake** | **No** | `FLUSHCOMM_USE_IOCTL_PING = 0` → FlushFileBuffers + REQ_INIT in shared section |
| **All requests** (read, write, mouse, cr3, base, guarded) | **No** | `FLUSHCOMM_USE_FLUSH_BUFFERS = 1` → `DRV_FlushFileBuffers(driver_handle)` |
| **Data transfer** | **No** | Section-based shared memory (`ZwCreateSection` + `MapViewOfFile`); no IOCTL buffers |
| **NtDeviceIoControlFile** | **Dead code** | Only in `#else` when `FLUSHCOMM_USE_FLUSH_BUFFERS = 0` |
| **DeviceIoControl** | **Lazy-imported** | Resolved at runtime; not called when FlushFileBuffers path is active |

**Conclusion:** The main cheat path uses **zero IOCTL calls**. All communication is via:
1. `FlushFileBuffers` → IRP_MJ_FLUSH_BUFFERS (not IRP_MJ_DEVICE_CONTROL)
2. Section shared memory for request/response data

### Legacy Path (`driver/com/driver.hpp`)

| Component | IOCTL Used? | Details |
|-----------|-------------|---------|
| **All operations** | **Yes** | Pure `DeviceIoControl` for CODE_RW, CODE_BA, CODE_GET_DIR_BASE, CODE_GET_GUARDED_REGION |
| **Device** | GUID-based | `\\.\{d6579ab0-c95b-4463-9135-41b8cf16e4e8}` – different driver than Beep/FlushComm |

This is an alternate driver implementation (likely Intel/vuln driver style). Not used by the main FlushComm path.

### Driver-Side (Kernel)

| Component | IOCTL Handled? | Details |
|-----------|----------------|---------|
| **FlushComm_HookHandler** | Yes (fallback) | Handles IOCTL_REXCOMM_PING and IOCTL_REXCOMM when IRP_MJ_DEVICE_CONTROL is dispatched |
| **FlushComm_FlushHandler** | No | Handles IRP_MJ_FLUSH_BUFFERS – **primary path** when usermode calls FlushFileBuffers |
| **driver.cpp** | Yes | Legacy IOCTL codes (CODE_RW, CODE_BA, etc.) for the alternate driver |

When usermode uses FlushFileBuffers, the driver receives IRP_MJ_FLUSH_BUFFERS and processes via `FlushComm_ProcessSharedBuffer` – no IOCTL dispatch.

---

## Part 1b: MmCopyVirtualMemory Usage in This Project

| Path | MmCopyVirtualMemory Used? | Details |
|------|---------------------------|---------|
| **Section path** (primary) | **No** | `frw(&x, nullptr)` – dest_proc=nullptr; driver writes directly to `g_section_view`; no cross-process copy |
| **Registry fallback** (no section) | **Yes** | Full REQ_* protocol uses MmCopyVirtualMemory for REQUEST_DATA, args, results |
| **frw_physical** (section) | **No** | `io_buf = (ULONGLONG)dataArea` (section); no temp_buf, no MmCopyVirtualMemory |
| **frw_physical** (registry) | **Yes** | dest_proc set; copies user buffer ↔ kernel temp via MmCopyVirtualMemory |
| **Physical read** (`read()`) | **No** | Uses MmMapIoSpaceEx + direct read; comment: "no MmCopyMemory (EAC traces it)" |

**Conclusion:** Primary path (section + FlushFileBuffers) avoids both IOCTL and MmCopyVirtualMemory. Registry fallback uses both.

---

## Part 2: Undetected Alternatives (UnknownCheats 2022–2026)

### 1. FlushFileBuffers + Section (Current Implementation) ✅

| Aspect | Status |
|--------|--------|
| **IOCTL** | None on hot path |
| **Syscall** | No NtDeviceIoControlFile |
| **IRP** | IRP_MJ_FLUSH_BUFFERS (less monitored than IRP_MJ_DEVICE_CONTROL) |
| **Data** | Section shared memory (no MmCopyVirtualMemory) |
| **Detection** | Lower than pure IOCTL; same universal risk (NMI, memory scan) |

**Sources:** IOCTL_VS_ALTERNATIVES_RESEARCH.md, FoxiTV/boom-cr3, UC 448472, EAC/Fortnite 2026 research.

---

### 1a. UnknownCheats: IOCTL Heavily Detected (Direct Quotes)

**ConDrv thread (UC 4407129, bxtnope):**
> "EAC enumerates PsLoadedModuleList for all registered drivers, then they verify that all entries in the MajorFunction point to valid addresses within the .text section, therefore your hook on IRP_MJ_CREATE (pagedpool) != .text, boom flagged. On top of that, they see the stack trace of irp, which finds your dynamically allocated DispatchDeviceIoControl, which doesn't belong to a module, another flag."

**ConDrv thread (heapallocation):**
> "Easy Anti Cheats NMI stackwalking will detect calls out to an random region that is not signed."

**UC 438804 (Swiftik):**
> "If you're going to use sockets or shared memory for your communication, you will need to hide your threads... Threads: spoof the entry, unlinking, hide call stack, spoof start address."

**UC 565891 (Kernel Communication Methods):**
> IOCTL, threads, unbacked executable memory listed as detection vectors.

---

### 1b. UnknownCheats: MmCopyVirtualMemory Detected

**UC 496628 (HadessUC):**
> "Detected on Fortnite EAC but I don't know about BattleEye."

**UC 496628 (0xEXP):**
> "ITS DETECTED feel free to get banned if you use it!"

**UC 496628 (mutable):**
> "It's detected on EAC (Won't ban you will only flag you) but I'm pretty sure it's not on BE."

**UC 496628 (xtremegamer1):**
> "EAC somehow intercepts MmCopyMemory requests and causes a bluescreen. I implemented the MmCopyMemory routine in my driver and it worked fine on unprotected games and crashed for EAC protected ones."

**UC 496628 (novak80):**
> "My mmcopyvirtualmemory comes from legit memory and I have never bluescreened in 2 years... If they have added something like this, it is because it comes from outside legit memory."

**UC 496628 (Buddye – counter):**
> "MmCopyVirtualMemory itself is NOT detected on BE or EAC. A lot of things people on this forum think is detected, isn't."

**UC 496628 (mutable):**
> "There's a thread somewhere by frostiest about reading with phys, just paste that. Only issue is that you will have to deal with non paged memory."

**Verdict:** Mixed opinions; multiple users report detection on Fortnite/EAC. Physical read recommended as safer alternative.

---

### 2. WSK (Winsock Kernel) / Loopback Sockets

| Aspect | Details |
|--------|---------|
| **Mechanism** | Driver uses WSK to create TCP server on 127.0.0.1; usermode connects via Winsock |
| **IOCTL** | None – network stack, not I/O manager |
| **Pros** | Different code path; no NtDeviceIoControlFile; standard socket APIs |
| **Cons** | **EAC detects socket/thread patterns** (ScanSystemThreads); loopback visible via netstat, ETW, WFP |
| **Config** | `FLUSHCOMM_USE_WSK = 0` – **NOT RECOMMENDED** per flush_comm_config.h |
| **References** | wbenny/KSOCKET, MiroKaku/libwsk, Microsoft WSK docs |

**Verdict:** Avoid. EAC thread/socket scanning makes this higher risk than FlushFileBuffers.

---

### 3. Data Pointer Hooking (ntoskrnl .data / FILE_OBJECT swap)

| Aspect | Details |
|--------|---------|
| **Mechanism** | Swap pointers in ntoskrnl .data or FILE_OBJECT->DeviceObject to redirect execution |
| **Examples** | DataptrHooks, data-ptr-comm (hcmzah), NtCompareSigningLevel-hook, DataCommunication (Sinclairq) |
| **Pros** | No IOCTL; harder to find than MajorFunction hooks; per-handle redirection |
| **Cons** | **PatchGuard** – most implementations trigger BSOD on Windows 2004+; GuardDispatchScanner validates data pointers |
| **FILE_OBJECT** | Spectre (D4stiny) – swap FILE_OBJECT->DeviceObject to fake device; real MajorFunction intact |

**Verdict:** ntoskrnl .data hooks are PatchGuard-risky. FILE_OBJECT DeviceObject swap is **already implemented** in this project (`FLUSHCOMM_USE_FILEOBJ_HOOK = 1`) – redirects to fake device without modifying real Beep MajorFunction.

---

### 4. Section-Based Shared Memory (Already Used) ✅

| Aspect | Details |
|--------|---------|
| **Mechanism** | ZwCreateSection + MapViewOfFile; both sides map same pages |
| **IOCTL** | None for data – only signaling (FlushFileBuffers) |
| **References** | KUCSharedMemory (benheise), SharedMemory-By-Frankoo, hugsy/shared-kernel-user-section-driver |
| **Detection** | Avoids IOCTL buffer model; ACs would need to detect mapped shared regions |

---

### 5. Large Page / Codecave (Already Used) ✅

| Aspect | Details |
|--------|---------|
| **Mechanism** | Copy shellcode into signed driver .data (e.g. beep.sys) via LargePageDrivers |
| **Purpose** | PING runs from signed memory – RIP in valid module for NMI stack walking |
| **References** | VollRagm LargePageDrivers, project setup_largepage_drivers.ps1 |

---

### 6. BYOVD (Bring Your Own Vulnerable Driver)

| Aspect | Details |
|--------|---------|
| **Mechanism** | Exploit signed vulnerable driver IOCTLs (e.g. ThrottleStop CVE-2025-7771, Lenovo CVE-2025-8061) |
| **IOCTL** | Uses driver's existing IOCTL – no custom driver |
| **Pros** | No manual mapping; driver in PsLoadedModuleList |
| **Cons** | Different threat model; driver-specific; CVEs get patched |

---

### 7. ALPC (Advanced Local Procedure Call)

| Aspect | Details |
|--------|---------|
| **Scope** | Primarily usermode–usermode; kernel–usermode less documented |
| **References** | pTerrance/alpc-km-um |
| **Verdict** | Niche; more research needed for KM–UM |

---

### 8. DMA (Hardware)

| Aspect | Details |
|--------|---------|
| **Mechanism** | PCIe device reads/writes memory directly; bypasses OS |
| **IOCTL** | None – hardware path |
| **Detection** | 2026 Fortnite: IOMMU, Secure Boot, TPM target DMA; EAC/BE focus on kernel/usermode memory cheats |
| **References** | UC 683599 (VGK/ACE/FACEIT), Valorant DMA firmware |

---

## Part 3: Comparison Matrix

| Method | IOCTL | NtDeviceIoControlFile | Detection Risk | Status in Project |
|--------|-------|------------------------|----------------|-------------------|
| **FlushFileBuffers + Section** | No | No | Low | ✅ Active |
| **FILE_OBJECT hook** | N/A | N/A | Lower (MajorFunction intact) | ✅ Active |
| **Pure IOCTL** | Yes | Yes | High | ❌ Legacy only |
| **WSK / Sockets** | No | No | High (thread/socket scan) | ❌ Disabled |
| **Data ptr (ntoskrnl)** | No | No | PatchGuard risk | ❌ Not used |
| **BYOVD** | Yes (driver's) | Yes | Medium | ❌ Not used |
| **DMA** | No | No | Hardware-focused | ❌ Different domain |

---

## Part 4: Recommendations (UnknownCheats-Informed)

1. **Avoid IOCTL entirely** – Heavily detected; MajorFunction scan, NMI stackwalking, IRP stack trace. Current project already uses FlushFileBuffers.
2. **Avoid MmCopyVirtualMemory** – Multiple UC reports of detection on EAC/Fortnite. Project's section path avoids it; ensure section never falls back to registry path.
3. **Use physical memory read** – MmMapIoSpaceEx, CR3 translation; driver already does this. "No MmCopyMemory (EAC traces it)" per driver.cpp.
4. **Keep section + FlushFileBuffers** – Section avoids MmCopyVirtualMemory for data; FlushFileBuffers avoids NtDeviceIoControlFile.
5. **Do not enable WSK** – EAC ScanSystemThreads detects socket/thread patterns (Swiftik UC 438804).
6. **Avoid ntoskrnl data pointer hooks** – PatchGuard risk.
7. **Ensure section init succeeds** – If section fails, registry fallback uses MmCopyVirtualMemory heavily.

---

## References (UnknownCheats)

- **UC 496628** – MmCopyVirtualMemory Detected On EAC/BE? (Apr 2022) – HadessUC, 0xEXP, mutable, xtremegamer1, Buddye, novak80, Frostiest
- **UC 438804** – How does EAC/BE detect kernel drivers bypasses? (Jan 2021) – Swiftik: pidbbcache, mmunloadeddrivers, big pools, threads, sockets, shared memory
- **UC 4407129** – Stealthy IOCTL communication using ConDrv.sys (Jun 2025) – bxtnope: MajorFunction .text check, pagedpool flag; heapallocation: NMI stackwalking
- **UC 565891** – Kernel Communication Methods – IOCTL, threads, unbacked memory as vectors
- **UC 647940** – Hooking NtDeviceIoControlFile to spoof PCI
- **UC 448472** – Shared Buffer/FlushFileBuffers Communication (FoxiTV) – IRP_MJ_FLUSH_BUFFERS "not checked for most drivers"
- **Project:** flush_comm_config.h, utilities/impl/driver.hpp, driver/flush_comm.hpp
- **Web:** UnknownCheats (683599, 688387, 4407129), GitHub (Poseidon, KUCSharedMemory, spectre, boom-cr3)
- **2025–2026:** BYOVD (Quarkslab, poh0.dev), Vanguard user-mode shift (Klizo), Fortnite tournament requirements (Feb 2026)
- **Microsoft:** IRP_MJ_FLUSH_BUFFERS, IRP_MJ_DEVICE_CONTROL, WSK Introduction
