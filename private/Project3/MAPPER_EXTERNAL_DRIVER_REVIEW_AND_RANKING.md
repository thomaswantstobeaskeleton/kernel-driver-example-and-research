# Mapper, External (Usermode), and Driver – Structure, Methods, and Detection Ranking

Detailed review of **communication methods**, **read/write**, **pool**, and **buffer** usage across the mapper, external cheat (usermode), and kernel driver, with fallbacks, multiple methods, and a **least detected → most detected** ranking. Includes UnknownCheats and project research.

**Last updated:** Feb 2026.

---

## 1. EXTERNAL (Usermode) – Structure and Methods

### 1.1 Communication Methods (Usermode → Driver)

| Method | Config / Condition | Description | Fallback / Multiple |
|--------|-------------------|-------------|---------------------|
| **FlushFileBuffers + section** | `FLUSHCOMM_USE_FLUSH_BUFFERS 1`, `FLUSHCOMM_USE_SECTION 1` (default) | Primary path. Open hooked device (Beep/Null/PEAuth), map section (`MapViewOfFile`), write request to shared buffer, call `FlushFileBuffers(handle)` to signal; driver handles IRP_MJ_FLUSH_BUFFERS and processes from `g_section_view`. No IOCTL, no DeviceIoControl in hot path. | **Yes:** Device open tries Beep → Null → PEAuth (registry `HookedDevice` index + round-robin). Handshake uses REQ_INIT in section; no IOCTL PING when `FLUSHCOMM_USE_IOCTL_PING 0`. |
| **NtOpenFile (device)** | Always (when opening device) | Prefer `NtOpenFile` with `\Device\Beep` (etc.) to avoid `CreateFileW` in call stack; falls back to `CreateFileW` if NtOpenFile fails. | **Yes:** NtOpenFile first, then CreateFileW. |
| **Section creation** | `FLUSHCOMM_USE_SECTION 1` | **File-backed (preferred):** `CreateFileW` temp file `%SystemRoot%\Temp\Fx<12 hex>.tmp` (MAGIC-derived), `CreateFileMappingW`, `MapViewOfFile`. **Named section fallback:** `OpenFileMappingW` / `CreateFileMappingW` with `Global\%06X<suffix>` or `Local\...`, then map. | **Yes:** File-backed first (`FLUSHCOMM_USE_FILEBACKED_SECTION 1`), then named section (Global/Local) with retries. |
| **Registry (shared buffer location)** | When section used | `SOFTWARE\<FLUSHCOMM_SECTION_SEED>\<suffix>` – driver writes hooked device index and (legacy) shared buffer address/PID. Usermode reads `HookedDevice` to know which device to open. | Single path when section present; no registry for buffer when section used (buffer = section view). |
| **IOCTL path** | `FLUSHCOMM_USE_FLUSH_BUFFERS 0` (disabled by default) | `DeviceIoControl(handle, IOCTL_REXCOMM, ...)` or direct syscall `NtDeviceIoControlFile`. **Not used in default config** – EAC/UC: IOCTL is a known vector. | Fallback only if someone enables it; direct syscall available (`FLUSHCOMM_USE_DIRECT_SYSCALL 1`) to bypass ntdll hooks. |
| **ALPC fallback** | `FLUSHCOMM_USE_ALPC_FALLBACK 1` (default 0) | If FlushFileBuffers handshake fails, connect to driver ALPC port `\RPC Control\Svc<8 hex from MAGIC>`, send REQ_INIT; data still in section. Signal-only ALPC + section for payload. | **Yes:** Used only when device handshake fails; section must already be open. |
| **Registry + VirtualAlloc (legacy)** | `FLUSHCOMM_REJECT_REGISTRY_FALLBACK 0` | Last resort: `VirtualAlloc` buffer, write address/PID to registry; driver uses MmCopyVirtualMemory to read/write. **EAC detected** (UC 496628); default is reject (1). | **Yes:** Only if section fails and reject is 0; not recommended. |

**Summary – External communication:**  
Primary = **section + FlushFileBuffers** (no IOCTL, no MmCopyVirtualMemory on section path). Multiple device targets (Beep/Null/PEAuth), file-backed or named section, optional ALPC when handshake fails. IOCTL and registry/VirtualAlloc are fallbacks or disabled.

---

### 1.2 Read / Write Methods (Usermode Side)

| Method | Where | Description |
|--------|--------|-------------|
| **read_physical** | `driver.hpp` | Sends REQ_READ with `bPhysicalMem=1` via `send_request`. With section: chunks up to `SECTION_DATA_SIZE`, driver writes result into section at `FLUSHCOMM_DATA_OFFSET`; usermode copies from `g_shared_buf + SECTION_DATA_OFFSET`. Without section: REQUEST_READ.Dest = usermode buffer (driver uses MmCopyVirtualMemory – detected path). |
| **write_physical** | `driver.hpp` | REQ_WRITE, `bPhysicalMem=1`. With section: copy data into section at DATA_OFFSET, send request; driver reads from section. Without section: Src = usermode buffer (MmCopyVirtualMemory). |
| **fetch_cr3** | `driver.hpp` | REQ_GET_DIR_BASE; with section result in section at DATA_OFFSET; else `OutCr3` in request. |
| **find_image** | `driver.hpp` | REQ_MAINBASE; result in section or OutAddress. |
| **get_guarded_region** | `driver.hpp` | REQ_GET_GUARDED_REGION; driver finds BigPool guarded region (tag match), returns VA. |

All go through **one** underlying mechanism: **FlushFileBuffers + shared buffer (section or registry)**. No separate “multiple read methods” on usermode – the variety is on the **driver** side (physical vs virtual, CR3 methods).

---

### 1.3 Pool / Buffer Methods (Usermode)

| Item | Description |
|------|-------------|
| **Shared buffer** | Section: `MapViewOfFile` of file-backed or named section (512 bytes, layout: magic, type, args, status, data). No usermode pool for comm buffer. |
| **Registry fallback buffer** | If section off and reject fallback 0: `VirtualAlloc` 512 bytes – not pool, process heap/virtual. |
| **Throttling** | `FLUSHCOMM_THROTTLE_MS` + `FLUSHCOMM_JITTER_MS` – no extra buffers; single global tick in `send_request`. |
| **Lazy import** | `FLUSHCOMM_USE_LAZY_IMPORT 1`: resolved at runtime; no special pool, standard process memory. |

Usermode does **not** use kernel pool; “buffer” = section view or VirtualAlloc.

---

### 1.4 External – Fallbacks and Multiple Methods Summary

- **Communication:** Section (file-backed → named) → device open (NtOpenFile → CreateFileW) → Beep/Null/PEAuth → FlushFileBuffers. Optional ALPC if handshake fails. IOCTL/registry path off by default.
- **Read/Write:** Single logical path (request type + section or legacy buffer); actual read/write implementation is in driver (physical/virtual, CR3 order).
- **Buffers:** Section only in recommended config; no kernel pool on usermode.

---

## 2. DRIVER – Structure and Methods

### 2.1 Communication Methods (Driver Side)

| Method | Config / Condition | Description | Fallback / Multiple |
|--------|---------------------|-------------|---------------------|
| **IRP_MJ_FLUSH_BUFFERS** | `FLUSHCOMM_USE_FLUSH_BUFFERS 1` | Handler `FlushComm_FlushHandler`; reads request from `g_section_view` (or legacy registry buffer). Processes via `FlushComm_ProcessSharedBuffer`; no IRP_MJ_DEVICE_CONTROL in hot path. | **Yes:** Can still have IOCTL handler for legacy PING when `FLUSHCOMM_USE_IOCTL_PING 1` (default 0). |
| **FILE_OBJECT hook** | `FLUSHCOMM_USE_FILEOBJ_HOOK 1` | Redirect Beep (or other) `FILE_OBJECT->DeviceObject` to fake device; real MajorFunction table unchanged. IRP_MJ_CREATE patches FILE_OBJECT; FLUSH_BUFFERS goes to fake device. | **Yes:** Alternative is direct MajorFunction hook (more detectable). |
| **Section creation** | `FLUSHCOMM_USE_SECTION 1` | Driver: file-backed section (open `\??\...\Temp\Fx<12hex>.tmp`) or named `\BaseNamedObjects\Global\%06X%ws`; map in System process or current. `g_section_view` = shared buffer. | **Yes:** File-backed first, then named section. |
| **WSK (kernel socket)** | `FLUSHCOMM_USE_WSK 1` (default **0**) | Kernel TCP server; EAC ScanSystemThreads detects socket/listener threads. **Disabled** in config. | N/A – not used. |
| **IOCTL (IRP_MJ_DEVICE_CONTROL)** | When FlushBuffers not sole path | Legacy io_controller; CODE_RW, CODE_BA, CODE_GET_GUARDED_REGION, CODE_GET_DIR_BASE, etc. Not used for main comm when FlushFileBuffers enabled. | Present for compatibility; not primary. |
| **Codecave PING** | `FLUSHCOMM_USE_CODECAVE 1` | PING runs from signed Beep .data (LargePageDrivers) so RIP in signed module; optional for REQ_INIT. | **Yes:** Falls back to inline PING if codecave unavailable (`FLUSHCOMM_SIGNED_CODECAVE_ONLY 1` = no MDL .text). |
| **ALPC server** | `FLUSHCOMM_USE_ALPC_FALLBACK 1` (0 default) | Driver creates ALPC port; worker thread processes section when message received. | **Yes:** Only if ALPC fallback enabled; section still used for data. |

---

### 2.2 Read / Write Methods (Driver Side)

| Method | Where | Description | Detection Note |
|--------|--------|-------------|----------------|
| **Physical read/write (primary)** | `driver.cpp` `read()` / `write()` | `MmMapIoSpaceEx` map physical page, read/write, `MmUnmapIoSpace`. **No MmCopyVirtualMemory** – EAC traces it (UC 496628). Used for CR3 walk and for `frw_physical`. | Least detected – no attach, no MmCopyVirtualMemory. |
| **frw_physical** | `driver.cpp` | Uses cached CR3; `translate_linear` for VA→PA; then `read()`/`write()` on physical. For caller buffer in another process: `MmCopyVirtualMemory` (dest_proc → kernel temp, then kernel temp ↔ physical). MmCopyVirtualMemory only for usermode buffer copy, not for game memory. | Physical path preferred; MmCopyVirtualMemory used only for copying to/from usermode buffer. |
| **frw_virtual (KeStackAttachProcess)** | `driver.cpp` | Attach to target, RtlCopyMemory in attached context. Uses kernel temp buffer (ExAllocatePoolWithTag EVASION_POOL_TAG_COPY_R). | **More detected** – UC: EAC detects attach + __readcr3. Used only when physical path fails or no CR3 cache. |
| **frw() order** | `driver.cpp` | 1) CR3 cached → frw_physical. 2) Else PsLookupProcessByProcessId → frw_virtual. 3) If virtual fails, retry with frw_physical after CR3 cache populated. | **Multiple:** Physical first, virtual fallback, then physical again. |
| **Main base (fba)** | `driver.cpp` | PsGetProcessSectionBaseAddress; if 0, PEB walk via physical (translate_linear + read) for Ldr → InLoadOrderLinks → DllBase. | Two paths: API first, then physical PEB. |
| **Guarded region (fget_guarded_region)** | `driver.cpp` | ZwQuerySystemInformation(SystemBigPoolInformation) via runtime-resolved `get_ZwQuerySystemInformation_fn()`; scan for NonPaged, Size 0x200000, tag match (obfuscated). Returns VA. | BigPool enum – ZwQuerySystemInformation detected by some ACs; resolved at runtime, no IAT. |

---

### 2.3 CR3 / Dirbase Methods (Driver) – Order and Ranking

| Order | Method | Description | Detection (Least → Most) |
|-------|--------|-------------|---------------------------|
| **1** | **dirbase_from_base_address** (physical scan) | MmGetPhysicalMemoryRanges; scan physical pages; for each candidate CR3 walk page tables from process base; validate with MZ at base. Bypasses encrypted DTB. | **Least detected** – no attach, no __readcr3, no EPROCESS read. |
| **2** | **MmPfn bruteforce** | MmPfnDatabase + pattern scan; find PFN whose decrypted pointer = target EPROCESS; CR3 = PFN<<12. Validates with validate_cr3_with_base if base known. | **Low** – no attach; uses internal kernel structures. |
| **3** | **EPROCESS UserDirectoryTableBase** | Read `target_process + i_user_dirbase`; use if not encrypted (high bits != CR3_ENC_MARKER). Validate with MZ if base known. | **Medium** – direct EPROCESS read; encrypted on some builds. |
| **4** | **KeStackAttachProcess + __readcr3** | Attach to target, `__readcr3() & PMASK`. UC 619886: “__readcr3 is detected”; use only when 1–3 fail. | **Most detected** – explicit attach + CR3 read. |

CR3 is cached (cr3_cache_lookup / cr3_cache_store) so repeated use doesn’t re-run Method 4.

---

### 2.4 Pool Methods (Driver)

| Usage | Tag / Source | Description |
|-------|----------------|-------------|
| **Copy buffers (frw_virtual, frw_physical)** | `EVASION_POOL_TAG_COPY_R` | NonPagedPool, temp for attach path or MmCopyVirtualMemory staging. Tags: `Io  `, `Fls `, etc. (page_evasion.hpp – byte-built, no 4-char literal). |
| **BigPool list (fget_guarded_region)** | `EVASION_POOL_TAG_LIST_R` | ExAllocatePoolWithTag(NonPagedPool, infoLen) for ZwQuerySystemInformation output. |
| **Registry (flush_comm, codecave)** | `EVASION_POOL_TAG_REG_R` | PagedPool for ZwQueryValueKey KeyValuePartialInformation. |
| **Mouse (mouse_inject.hpp)** | `EVASION_POOL_TAG_LIST_R`, `EVASION_POOL_TAG_WORK_R` | Device list array; work context for async (when FLUSHCOMM_MOUSE_SYNC 0). |
| **create_driver.hpp** | `CREATE_DRIVER_POOL_TAG` | PagedPool for service name buffers. |
| **NMI spoof** | `EVASION_POOL_TAG_COPY_R` | NMI core info array. |
| **Trace cleaner** | `ExFreePool(e->Buffer)` | Frees Buffer of UNLOADED_DRIVER_X64 (no tag in call – structure’s pool was system-allocated). |
| **Dump (driver.cpp)** | `EVASION_POOL_TAG_COPY_R` | DUMP_BLOCK_SIZE. |

**Pool tag rotation:** `FLUSHCOMM_POOL_TAG_ROTATE 0` (default) – fixed benign tags. Rotation would be evasion heuristic (EMACLAB: “known malicious tags” blocklist); fixed tags = normal driver behavior.

---

### 2.5 Buffer Methods (Driver)

| Buffer | Description |
|--------|-------------|
| **Section view** | `g_section_view` – kernel mapping of shared section; request type, args, status, data at FLUSHCOMM_DATA_OFFSET. Primary for request/response. |
| **Registry** | Legacy: shared buffer VA/PID in registry; driver reads via ZwQueryValueKey (ExAllocatePoolWithTag for result). |
| **IRP buffer** | When IOCTL used: `irp->AssociatedIrp.SystemBuffer` (METHOD_BUFFERED). |
| **Physical read/write** | No persistent buffer; MmMapIoSpaceEx per page, then unmap. Temp_buf in frw only for attach path or MmCopyVirtualMemory staging. |

---

## 3. MAPPER (Kdmapper) – Structure and Methods

### 3.1 Communication Methods (Mapper → Vulnerable Driver)

| Method | Backend | Description | Fallback / Multiple |
|--------|---------|-------------|---------------------|
| **DeviceIoControl (IOCTL)** | Intel (iqvw64e) / Eneio | Open device (CreateFileW on vulnerable driver device); send IOCTLs with case_number (obfuscated) for MemCopy, Fill, GetPhysicalAddress, MapIoSpace, UnmapIoSpace, CallKernelFunction. | **Yes:** Two backends – Intel or Eneio (`--driver`); lazy import can hide CreateFileW/DeviceIoControl. |
| **CreateFileW (device)** | Both | Path from kdm_obfuscate (e.g. device path); random driver name. | Single open; retries in Load. |
| **Service Control Manager (SCM)** | service.cpp | CreateService, StartService to load vulnerable driver from temp path; StopAndRemove on unload. | Standard SCM; no alternative loader in this mapper. |

Mapper **only** uses IOCTL to the **vulnerable** driver (iqvw64e/eneio). It does **not** talk to the **mapped** cheat driver; the cheat driver uses FlushComm (section + FlushFileBuffers).

---

### 3.2 Read / Write Methods (Mapper)

| Method | Where | Description |
|--------|--------|-------------|
| **ReadMemory** | intel_driver.cpp / eneio_driver.cpp | Read kernel VA: MapIoSpace (physical from GetPhysicalAddress) or driver-specific read IOCTL; copy out; UnmapIoSpace. |
| **WriteMemory** | Same | Write kernel VA: map physical, write, unmap. |
| **WriteToReadOnlyMemory** | Same | Change protection then write (driver-specific). |
| **MemCopy** | intel_driver | Single IOCTL: source/dest/length; vulnerable driver does kernel copy. |
| **CallKernelFunction** | Both | Call arbitrary kernel function via vulnerable driver (e.g. ExAllocatePoolWithTag, ExFreePool, driver entry). |

These are for **mapping** (copying PE, resolving imports, calling DriverEntry), not for game memory. Game memory is handled by the **mapped** driver via FlushComm.

---

### 3.3 Pool / Buffer Methods (Mapper)

| Method | Where | Description |
|--------|--------|-------------|
| **AllocatePool (kernel)** | kdmapper.cpp / intel_driver | `driver::AllocatePool(NonPagedPool, image_size)` → CallKernelFunction(ExAllocatePoolWithTag, NonPagedPool, size, **'Vad '**). Used when mode = AllocatePool (default). |
| **MmAllocateIndependentPagesEx** | Same | When mode = AllocateIndependentPages; different kernel API for allocation (no BigPool entry for some sizes). |
| **ClearBigPoolEntryForAddress** | intel_driver / eneio_driver / driver_backend | Called after MapDriver when mode = AllocatePool. **Current impl:** stub (returns true); comment: zeroing BigPool entry / ExpPoolBigPageTable is version-dependent. Tag 'Vad ' chosen to look benign. |
| **VirtualAlloc (usermode)** | kdmapper.cpp | Local image copy (PE), relocs, imports; then WriteMemory to kernel. |
| **FreePool / MmFreeIndependentPages** | kdmapper.cpp | On free: CallKernelFunction(ExFreePool) or MmFreeIndependentPages. |

Mapper uses **one** allocation mode per run: either **AllocatePool** (ExAllocatePoolWithTag, tag 'Vad ') or **AllocateIndependentPages**. Pool mode = allocation appears in BigPool; ClearBigPoolEntryForAddress is intended to reduce EAC pool-scan visibility but is not fully implemented.

---

### 3.4 Mapper Buffers

| Buffer | Description |
|--------|-------------|
| **Local image** | VirtualAlloc (usermode) – headers, sections, relocs, imports; then written to kernel via WriteMemory. |
| **Kernel image** | Either pool or independent pages; driver entry called; optionally freed after map. |
| **IOCTL buffers** | COPY_MEMORY_BUFFER_INFO, FILL_MEMORY_BUFFER_INFO, etc. – stack/local in mapper process; DeviceIoControl sends to vuln driver. |

---

## 4. RANKING: LEAST DETECTED → MOST DETECTED

Using project docs (EAC_FORTNITE_DETECTION_RESEARCH.md, DETECTION_RISKS_AND_STATUS.md, EAC_DETECTION_RESEARCH_UC.md) and UC-style reasoning.

### 4.1 Communication (Usermode ↔ Driver)

| Rank | Method | Reason |
|------|--------|--------|
| **1 (least)** | **Section + FlushFileBuffers** | No IOCTL; IRP_MJ_FLUSH_BUFFERS “not checked for most drivers” (UC 448472); no dedicated thread; handled in caller context. |
| **2** | **FILE_OBJECT hook** | MajorFunction table of real driver unchanged; harder to detect than direct MajorFunction hook. |
| **3** | **Direct syscall (NtDeviceIoControlFile)** | Bypasses ntdll hooks; instrumentation callbacks can still see non-ntdll return address. |
| **4** | **IOCTL (DeviceIoControl)** | Known AC vector; commonly hooked and monitored. |
| **5** | **ALPC** | Less common; port and section combo unique here; thread on driver side can be scanned. |
| **6** | **Registry + VirtualAlloc (MmCopyVirtualMemory)** | EAC traces MmCopyVirtualMemory (UC 496628); registry + shared VA is legacy and detected. |
| **7 (most)** | **WSK / kernel sockets** | EAC ScanSystemThreads detects socket/listener threads; worker thread RIP in unmapped code. **Disabled.** |

### 4.2 Read / Write (Driver)

| Rank | Method | Reason |
|------|--------|--------|
| **1 (least)** | **Physical (MmMapIoSpaceEx + translate_linear)** | No KeStackAttachProcess, no __readcr3, no MmCopyVirtualMemory for game memory; EAC traces MmCopyVirtualMemory. |
| **2** | **CR3 Method 1 (dirbase_from_base_address)** | Physical scan from base; no attach, no EPROCESS DTB read. |
| **3** | **CR3 Method 2 (MmPfn)** | No attach; internal kernel structures. |
| **4** | **CR3 Method 3 (EPROCESS UserDirectoryTableBase)** | Direct read; can be encrypted. |
| **5** | **MmCopyVirtualMemory (only for usermode buffer)** | Used only to copy to/from caller buffer; not for target process memory; still a known API. |
| **6** | **frw_virtual (KeStackAttachProcess + RtlCopyMemory)** | UC: EAC detects attach and __readcr3; used as fallback. |
| **7 (most)** | **CR3 Method 4 (KeStackAttachProcess + __readcr3)** | Explicit attach + CR3 read; last resort. |

### 4.3 Pool / Buffers

| Rank | Item | Reason |
|------|------|--------|
| **1 (least)** | **Section (shared buffer)** | No kernel pool for comm; both sides map same section. |
| **2** | **Fixed benign tags (Fls , Cc  , Io  , Mm  )** | Normal driver-like; not in typical “malicious tag” blocklists (EMACLAB). |
| **3** | **Mapper 'Vad ' (ExAllocatePoolWithTag)** | Benign-looking; BigPool entry still present; ClearBigPoolEntryForAddress currently stub. |
| **4** | **Pool tag rotation** | Evasion heuristic; “why vary tags?” – default off. |
| **5 (most)** | **Suspicious/known tags** | e.g. old "File", "gLpC" – fixed in project; blocklisted tags = high risk. |

### 4.4 Mapper-Specific

| Rank | Item | Reason |
|------|------|--------|
| **1 (least)** | **AllocateIndependentPages** | No BigPool for image in some cases; fewer pool-scan hits. |
| **2** | **AllocatePool + ClearBigPoolEntryForAddress** | Pool visible until cleared; clear is version-dependent (stub). |
| **3** | **AllocatePool only** | BigPool scan can see allocation (EAC scans BigPool – UC 438804). |
| **4** | **Vulnerable driver IOCTL** | Required for mapping; vuln driver (iqvw64e/eneio) itself is known and trace-cleaned. |
| **5 (most)** | **Public mapper + wdfilter/vuln traces** | KdMapper detected (UC 594960, 614327); trace cleaner and custom build reduce but do not eliminate risk. |

---

## 5. FALLBACKS AND MULTIPLE METHODS – QUICK REFERENCE

| Component | Communication | Read/Write | Pool/Buffer |
|-----------|----------------|------------|-------------|
| **External** | Section (file→named) → FlushFileBuffers; device Beep/Null/PEAuth; optional ALPC; IOCTL/registry off. | Single request path; driver does physical/virtual. | Section only (no kernel pool). |
| **Driver** | FlushBuffers + section; FILE_OBJECT hook; codecave PING; optional ALPC; WSK off. | Physical first → virtual (attach) fallback → physical again; CR3: Method 1→2→3→4. | EVASION_POOL_TAG_*; fixed tags; trace cleaner ExFreePool. |
| **Mapper** | IOCTL to vuln driver only; Intel or Eneio backend. | ReadMemory/WriteMemory/CallKernelFunction for mapping. | AllocatePool ('Vad ') or MmAllocateIndependentPages; ClearBigPool stub. |

---

## 6. REFERENCES

- **Project:** EAC_FORTNITE_DETECTION_RESEARCH.md, DETECTION_RISKS_AND_STATUS.md, EAC_DETECTION_RESEARCH_UC.md, CR3_METHODS_RESEARCH.md, IOCTL_ALTERNATIVES_UPDATED_RESEARCH.md.
- **UnknownCheats:** 438804 (EAC kernel bypass, Swiftik), 448472 (FlushFileBuffers), 496628 (MmCopyVirtualMemory), 619886 (__readcr3 detected), 594960/614327 (kdmapper detection).
- **Driver:** driver.cpp, flush_comm.cpp, flush_comm.hpp, page_evasion.hpp, trace_cleaner.hpp, routine_obfuscate.h.
- **Usermode:** utilities/impl/driver.hpp, utilities/impl/alpc_fallback.hpp, flush_comm_config.h.
- **Mapper:** kdmapper_src/kdmapper/kdmapper.cpp, intel_driver.cpp, driver_backend.cpp, eneio_driver.cpp.
