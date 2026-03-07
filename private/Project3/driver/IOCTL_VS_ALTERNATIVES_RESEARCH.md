# IOCTL vs Current Implementations – Web Research Summary

Research from public sources (UnknownCheats, security blogs, GitHub) comparing **pure IOCTL** communication with **FlushFileBuffers + section + FILE_OBJECT hook** (current FlushComm implementation).

**Last updated:** Feb 2026 – includes late 2025 and 2026 sources (UC, GitHub, EAC/Fortnite).

---

## TL;DR: Current Implementation Still Better Than Pure IOCTL (2024–2025)

| Factor | Pure IOCTL | FlushComm (FlushFileBuffers + Section + FILE_OBJECT) |
|--------|------------|-----------------------------------------------------|
| Syscall monitoring | **Yes** – NtDeviceIoControlFile commonly hooked | **No** – FlushFileBuffers uses different IRP path |
| Dispatch table checks | **Yes** – first detection vector; still checked | **Mitigated** – FILE_OBJECT hook leaves MajorFunction intact |
| NMI stack walking | **Affects both** – catches execution in manual-mapped code | Same – no advantage either way |
| Thread footprint | Caller context (OK) | Caller context (OK) |
| Detection surface | **Higher** (IOCTL-specific + universal) | **Lower** (no IOCTL-specific vectors) |

**Conclusion:** Current design remains better than pure IOCTL. Detection has evolved (NMI, memory scan), but IOCTL adds extra exposure that FlushFileBuffers avoids.

---

## 1. IOCTL (DeviceIoControl / NtDeviceIoControlFile) – High Detection Profile

### Syscall and API Hooking

- Anti-cheats **hook `NtDeviceIoControlFile`** to detect spoofing (PCI, hardware IDs, VM detection bypass).
- `DeviceIoControl` ultimately calls this syscall and is a common monitoring point.
- Direct syscalls (bypassing ntdll hooks) do not help if kernel-side hooks or callbacks monitor the operation.

**Source:** UnknownCheats – *"Hooking NtDeviceIoControlFile to spoof PCI Information"*; MDSec – *"Bypassing User-Mode Hooks and Direct Invocation of System Calls"*.

### IOCTL Dispatch Overwrite Detection

- One of the **earliest** detection methods for manually mapped drivers.
- Anticheats check that **MajorFunction / dispatch entries** point into valid driver memory.
- kdmapper-style mapping puts driver code outside known modules; dispatch pointers are easy to flag.
- Tulach (2019+): *"Anticheats started catching up, first by checking the most obvious things like **IOCTL dispatch overwrites** (they should always point to legitimate drivers' memory regions)"*.

**Source:** tulach.cc – *"Detecting manually mapped drivers"*.

### Why Pure IOCTL Is Risky for Manual Mapping

- With manual mapping, the driver has no `DriverObject` in normal lists.
- IOCTL handlers live in mapped code; dispatch table points **outside** legitimate module ranges.
- Enumerating and validating MajorFunction pointers is simple; this pattern is widely used.

---

## 2. FlushFileBuffers / IRP_MJ_FLUSH_BUFFERS – Lower Detection Profile

### Less Monitored Than IOCTL (Historical + 2024–2025)

- **FoxiTV (GitHub boom-cr3, UC 448472):** *"IRP_MJ_FLUSH_BUFFERS is not checked for most drivers!"*; *"this is a sexy MajorFunction you can hook without any issues."*
- **2024–2025:** Detection has evolved; FlushFileBuffers may be "less reliable against modern security systems" due to NMI stack walking and MajorFunction verification (UnknownCheats ConDrv/UC discussions). No public reports indicate EAC/Vanguard added explicit FlushFileBuffers checks. IRP_MJ_FLUSH_BUFFERS remains a secondary target compared to IRP_MJ_DEVICE_CONTROL.

### No IOCTL Syscall Path

- `FlushFileBuffers` triggers **IRP_MJ_FLUSH_BUFFERS**, not `IRP_MJ_DEVICE_CONTROL`.
- No `NtDeviceIoControlFile` / `DeviceIoControl` call on the hot path.
- Syscall/API hooks focused on IOCTL do not see this communication.
- Vanguard (Archie 2025): Documented syscall hooks do **not** include NtDeviceIoControlFile.

### Same Execution Model

- IRP is handled in the **caller’s thread context** (synchronous).
- No dedicated listener threads or socket-style patterns.
- Avoids the "sockets/shared memory + threads" detection pattern reported for EAC.

### If MajorFunction Checks Are Added

- FoxiTV: *"Even if it’s getting checked with basic checks we can get around this by hooking the MajorFunction and set the hook to another unknown MajorFunction of a legit driver that we also hooked."*
- Chaining to another legit driver’s handler can reduce direct exposure of the hook.

---

## 3. Shared Memory (Section) – Bypasses IOCTL Dispatch Logic

### Why It Helps

- Tulach, kernel_payload_comms, **KUCSharedMemory (GitHub benheise):** *"Avoids IOCTL issues that arise with manually mapped drivers"* – uses shared memory; TODO includes "Resolve and bypass NMI callbacks."
  - ACs would need to detect mapped shared regions instead of IOCTL handlers.
- FlushComm uses **ZwCreateSection** + **MapViewOfFile**.
- No extra threads; data exchange happens in the same context as the FlushFileBuffers call.

### Caveat – Overall Manual Mapping Risk

- Modern ACs do **proactive memory scanning** (physical memory, page tables).
- Tulach: *"They started proactively searching for those drivers in memory."*
- Communication method does not change whether the **mapped driver** can be found.
- But the **communication path** (IOCTL vs FlushFileBuffers + section) still changes detection surface at the syscall/dispatch level.

---

## 4. 2024–2025 Detection Evolution (Applies to Both Methods)

### NMI Stack Walking

- **Meekolab (2024):** *"Kernel-level drivers can provide... NMI (Non-Maskable Interrupt) stack walking and direct hardware access for integrity checks."*
- **Trakimas (lxkast):** ACs register NMI callbacks, send NMIs to cores, and inspect RIP/RSP/stack. If any return address points outside valid kernel modules → unsigned driver execution detected.
- **Effect:** NMI catches execution in **manual-mapped code** regardless of IOCTL vs FlushFileBuffers. Both methods run handler code in mapped memory; NMI can flag either.
- **Evasion:** Stack spoofing via HalPreprocessNmi hook exists (Trakimas PoC) but is complex; ICALL-GADGET (2024) redirects control flow through legit modules to evade stack walking.

### MajorFunction Verification (UC ConDrv Post 4407129)

- **UnknownCheats:** *"Anti-cheats enumerate PsLoadedModuleList and verify that MajorFunction pointers point to valid addresses within the .text section. Hooks in paged pool would fail this check."*
- **Spectre (GitHub D4stiny):** FILE_OBJECT DeviceObject swap – hook on *fake* driver/device; real driver object unchanged, so MajorFunction scans don't see tampering.
- FILE_OBJECT hook keeps Beep's MajorFunction table **unchanged** – reduces exposure vs direct MajorFunction overwrite.

### EAC 2024 Minifilter

- EasyAntiCheat_EOSSys (v6.1, Dec 2024) includes file system filtering. "Supported features: 0x4" / "does not support bypass IO" – Filter Manager compatibility, not a confirmed FlushFileBuffers-specific check.

---

## 4a. June 2025+ Industry and Detection Updates

### THE FINALS – Kernel Anti-Cheat (July 2025)

- **Embark Studios:** *"A lot of cheats these days use a kernel-driver to read and write memory to gain an unfair advantage. This means they run in a privileged mode... making it unlikely and in some cases impossible to detect via Anti-Cheat in the game client. The technical solution to combat this is kernel-driver Anti-Cheat."*
- Confirms kernel driver cheats remain the primary target; THE FINALS moving from EAC (already kernel on Windows) to a new kernel-based solution.
- **Source:** GamingOnLinux, Steam update notes, July 2025.

### Vanguard – Shift to User-Mode AI (June 2025)

- **Riot Games:** Transitioning from permanent kernel driver to user-mode AI protection. Kernel stub &lt; 50 KB, loads only for snapshots.
- **Phase I (Q3 2025):** Hybrid pilot; kernel loads at boot, unloads after match start.
- **Phase II (Q1 2026):** User-mode AI primary; kernel for just-in-time forensics only.
- **Implications:** Long-term reduction in kernel-resident detection (NMI, memory scan) for Valorant. EAC/BattlEye/FACEIT retain full kernel presence. No change to IOCTL vs FlushFileBuffers comparison – Vanguard’s documented syscall hooks never included NtDeviceIoControlFile.
- **Source:** Klizo Solutions / Medium, June 5, 2025.

### MixCheats 2025 – Bypass Techniques

- Kernel AC bypass methods cited: "Hypervisor-based approaches," "Manual mapping to avoid detection," "Use your OWN kernel driver (fight fire with fire)."
- "MixCheats uses proprietary signed kernel drivers that operate alongside anti-cheat systems undetected."
- IOCTL vs alternatives not discussed; focus on signed drivers and behavioral evasion.
- **Source:** MixCheats Blog 2025.

### Gistre EPITA – Anticheat History (June 24, 2025)

- DMA cheats described as "final boss" – bypass OS entirely. ACs scan for suspicious PCI devices/hardware IDs.
- AI-based behavioral detection expanding (Call of Duty Ricochet, May 2025).
- No technical update on FlushFileBuffers or IOCTL; industry trends toward ML and hardware scanning.
- **Source:** blog.gistre.epita.fr.

### BYOVD and Vulnerable Drivers (2025)

- BYOVD (Bring Your Own Vulnerable Driver) avoids DSE bypass: exploit signed vulnerable drivers’ IOCTLs for kernel R/W. Examples: ThrottleStop.sys (CVE-2025-7771), Lenovo (CVE-2025-8061).
- Tulach-style detection (IOCTL dispatch overwrite, NMI, memory scan) still applies to **manual-mapped** drivers. BYOVD uses **legitimate** driver IOCTLs – different threat model.
- **Source:** Quarkslab, poh0.dev, 2025.

### Summary (June 2025+)

- **No new evidence** that FlushFileBuffers or IRP_MJ_FLUSH_BUFFERS is explicitly targeted. THE FINALS and Embark cite "kernel drivers" in general.
- **Vanguard’s user-mode shift** (2025–2026) reduces kernel footprint for Valorant; other ACs keep full kernel presence.
- **FlushFileBuffers + section + FILE_OBJECT** conclusion unchanged: avoids IOCTL-specific vectors; same universal risk (NMI, scan) as IOCTL when manual-mapping.

---

## 4b. 2026 EAC/Fortnite Detection – Hardware-First, No Communication-Method Change

### Fortnite Tournament Requirements (February 19, 2026)

- **Epic Games** expanded PC anticheat for Fortnite *tournaments*: **Secure Boot**, **TPM 2.0**, and **IOMMU** (new).
- **IOMMU** targets DMA/hardware cheats (PCIe devices that read/write memory directly). IOMMU isolates devices and blocks unauthorized DMA.
- **Secure Boot** blocks bootkits and unauthorized drivers loading at startup.
- **TPM 2.0** verifies boot integrity.
- **Scope:** Tournament participants only; casual play unaffected. ~95% of PC players estimated compatible; enable via BIOS.
- **Source:** BigGo Finance, DSOGaming, GamingBible, GameRant, Polygon, Tom's Hardware, Feb 2026.

### EAC's Role in Fortnite (2026)

- **DSOGaming:** *"Epic also uses Easy Anti-Cheat's kernel-level protection to secure the game while it is running. In addition, Epic monitors suspicious behavior using data analysis and machine learning."*
- EAC kernel driver (EasyAntiCheat.sys) remains the runtime protection layer.
- **Hypercall:** EAC's driver uses "encrypted IOCTL codes" and XTEA for internal module transfers – EAC's own comms use IOCTL; detection focuses on external cheat drivers, not comm protocol.

### 2026 Focus: DMA and Hardware, Not Comm Protocol

- 2026 changes are **hardware and boot-time** oriented, not kernel driver communication method.
- No public evidence that EAC/Fortnite added IRP_MJ_FLUSH_BUFFERS or FlushFileBuffers checks in 2026.
- IOCTL vs FlushFileBuffers comparison unchanged: FlushFileBuffers still avoids NtDeviceIoControlFile and MajorFunction overwrite vectors.

### EAC + Windows Stack Protection (2026)

- **thefix.it.com (2026 guide):** EAC conflicts with "Kernel-mode Hardware-enforced Stack Protection" on some Windows 11 configs; users may need to adjust this setting.
- Suggests EAC driver interacts with low-level kernel protections; not a change to how EAC detects cheat driver communication.

### Summary (2026 EAC/Fortnite)

- **Hardware requirements** (Secure Boot, TPM, IOMMU) target DMA and bootkits; do not affect software kernel comm choice.
- **EAC kernel** continues to use existing detection (threads, modules, handles, integrity). No 2026-specific update to FlushFileBuffers or IOCTL monitoring.
- **Conclusion:** FlushFileBuffers + section + FILE_OBJECT remains preferable to pure IOCTL for EAC/Fortnite in 2026 – same logic as prior research.

---

## 4c. Late 2025 / 2026 – UnknownCheats & GitHub (Same Sources, Recent Data)

### UC Thread 683599 – VGK/ACE/FACEIT Detect DMA (Jan–April 2025)

- **rhaym (April 2025):** *"VGK can leverage the IOMMU to deny access to unauthorized regions... many people have seen their DMAs returning 0x0 when they try to read VALORANT's memory."*
- **sushi404 (April 2025):** *"They [BE and EAC] focus on the category of cheaters that represent the majority, which are usermode/kernel mode memory based ones."* If BE/EAC gathered "PCIe device configurations, communication patterns, or driver interactions," they could identify DMA firmware – but that would be more invasive.
- **versager (Jan 2025):** Cheap DMA firmware detected via *"missing driver communication... If it doesn't act like a typical hardware, its most certainly not a typical hardware."*
- **Implication:** EAC/BE primary focus remains **kernel/usermode memory cheats**, not PCIe bus telemetry. FlushFileBuffers vs IOCTL is still a kernel-memory-comm choice; no UC 2025 data suggests EAC added IRP_MJ_FLUSH_BUFFERS checks.

### UC Thread 677857 – EAC Runs at Boot (Dec 2024)

- Claim that Fortnite EAC now runs at boot like Vanguard; **disputed as misinformation** by high-rep users (NormPlayer, Rafael4096, zxc). No proof provided. Treat as unconfirmed.

### UC Thread 689840 – Valorant DMA Firmware (March 2025)

- Free Valorant DMA firmware (PCILeech); *"Bypass traditional detection vectors with kernel-mode hardware access."* Emphasizes DMA vs kernel-driver path; does not discuss FlushFileBuffers or IOCTL.

### UC Thread 688145 – EAC Linux Bypass (Feb 2025)

- EAC bypass via Linux kernel module; indicates ongoing kernel-level EAC research. No comm-method specifics.

### UC Thread 682955 – KMDU (Jan 2025)

- Kernel Mode Dumper Utilities release; kernel-level tooling. No FlushFileBuffers/IOCTL discussion.

### UC Thread 688387 – EAC Bypass for Beginners (Feb 23–25, 2025)

- User asked if 2023 DSE patcher + Cheat Engine driver method still works for Fortnite in 2025.
- **plur53 (Feb 2025):** *"Being UD on eac (at least rust eac) will require a bit of learning... manual mapping is a solution but it isn't the only way, just the most common way people attempt."* No IOCTL vs FlushFileBuffers discussion.
- **shook1 (Feb 2025):** Warned against ChatGPT-written code for bypasses.
- **IEmulateEAC (Feb 2025):** Suggested DMA as easier path for beginners vs kernel driver dev. No comm-protocol specifics.

### UC Thread 688145 – EAC Linux (Feb–Aug 2025)

- **Feb 2025:** EAC on Linux runs solely in userspace; kernel module + ioctl for memory R/W not monitored by EAC (shatterfive, shook1). Different platform – Windows kernel comm not applicable.
- **Aug 22–26, 2025:** Thread continued with ptrace hiding, /proc/{pid}/mem. No Windows FlushFileBuffers/IOCTL data.

### UC Thread 678400 – Undetected Injector EAC (Dec 22, 2024)

- Injector-focused release; no kernel driver comm protocol discussion.

### GitHub – 2025/2026 Activity

- **boom-cr3/Shared-FlushFileBuffers-Communication:** No significant 2025 commits; README unchanged. Technique remains cited.
- **Poseidon, KUCSharedMemory:** No major 2025 updates found. Prior findings (avoid IOCTL, shared memory) still stand.
- **New 2025 UC releases:** DMA firmware, KMDU; focus on DMA and dumping, not kernel-driver comm protocol.

### Summary (Late 2025 UC/GitHub)

- **No 2025/2026 evidence** that FlushFileBuffers or IRP_MJ_FLUSH_BUFFERS is newly targeted. EAC/BE focus remains usermode/kernel memory cheats (sushi404, rhaym).
- **IOCTL vs FlushFileBuffers** conclusion unchanged: FlushFileBuffers avoids NtDeviceIoControlFile and MajorFunction vectors.

---

### Summary

- **Universal detectors (NMI, memory scan):** Affect IOCTL and FlushFileBuffers equally when using manual-mapped drivers.
- **IOCTL-specific (syscall hook, dispatch overwrite):** Only affect IOCTL. FlushFileBuffers avoids these.
- **Conclusion:** FlushFileBuffers + section + FILE_OBJECT remains **strictly better** than pure IOCTL – same universal risk, fewer IOCTL-specific vectors.

---

## 5. FILE_OBJECT Hook – Preserves MajorFunction Table

### Role in Evasion

- Replacing **FILE_OBJECT->DeviceObject** with a fake device does not modify the original driver’s MajorFunction table.
- Spectre rootkit (GitHub D4stiny/spectre): Swap FILE_OBJECT->DeviceObject to fake device; hook MajorFunction on *fake* driver only. *"Intercept IOCTL communications... without modifying system-wide driver objects."*
- MajorFunction integrity checks that expect handlers inside the driver’s own image are harder to trigger when the original pointers stay unchanged.

### FlushComm’s Use

- `FLUSHCOMM_USE_FILEOBJ_HOOK = 1`: redirects FILE_OBJECT to a fake device; real Beep MajorFunction remains intact.
- Reduces the chance that MajorFunction scans flag a hook on Beep.

---

## 6. Vanguard (Riot) – Syscall Hooks (Not IOCTL-Specific)

- Vanguard hooks syscalls via PatchGuard-compliant mechanisms (e.g. HalCollectPmcCounters).
- Documented hooks include: NtAllocateVirtualMemory, NtFreeVirtualMemory, NtMapViewOfSection, NtSuspendThread, NtSuspendProcess, and several win32k functions.
- **NtDeviceIoControlFile was not listed** in the analyzed set, but that does not imply it is never monitored.
- Overall: Vanguard focuses on memory and process manipulation; communication method still matters for other ACs (EAC, BattlEye) that explicitly monitor IOCTL.

---

## 7. Practical Comparison

| Communication Method | Syscall Hooks | Dispatch Checks | Thread Model | Overall Risk |
|---------------------|---------------|-----------------|--------------|--------------|
| Pure IOCTL | Yes (NtDeviceIoControlFile) | Yes (MajorFunction) | Caller context | **High** |
| FlushFileBuffers + Section | No (different IRP) | Lower (IRP_MJ_FLUSH_BUFFERS less checked) | Caller context | **Lower** |
| FlushFileBuffers + Section + FILE_OBJECT | No | Further reduced (table intact) | Caller context | **Lowest** |
| WSK / Sockets | N/A | N/A | Dedicated thread | **High** (EAC thread scan) |

---

## 8. Recommendations (Aligned With Current Config)

*Unchanged; current FlushComm config matches these.*

1. **Use FlushFileBuffers instead of IOCTL** – avoid NtDeviceIoControlFile monitoring.
2. **Use section-based shared memory** – no MmCopyVirtualMemory, no IOCTL buffer model.
3. **Use FILE_OBJECT hook** – keep real MajorFunction table unchanged.
4. **Avoid WSK/sockets** – dedicated threads and socket patterns are more visible to EAC.
5. **Avoid IOCTL PING handshake** – use FlushFileBuffers + REQ_INIT.
6. **Use synchronous mouse** – no IoQueueWorkItem with mapped callback on worker stacks.

---

## 9. References

### GitHub Sources
- **boom-cr3/Shared-FlushFileBuffers-Communication** – FlushFileBuffers + shared buffer; *"IRP_MJ_FLUSH_BUFFERS is not checked for most drivers!"*; links to UC thread 448472
- **sondernextdoor/Poseidon** – Stealth UM↔KM comm without threads/hooks/driver objects; shared memory loop; *"detection vectors identified by BE and EAC"*; 2023 update notes some vectors
- **D4stiny/spectre** – FILE_OBJECT DeviceObject hook to intercept IOCTL *without modifying driver object*; MajorFunction on fake device only
- **benheise/KUCSharedMemory** – Manual-map shared memory; *"avoids IOCTL issues that arise with manually mapped drivers"*; TODO: "Resolve and bypass NMI callbacks"
- **hugsy/shared-kernel-user-section-driver** – Section-based kernel-user comm
- **fengjixuchui/SharedMemory-By-Frankoo** – Shared memory KM-UM comm
- **vasie1337/kernel-anticheat** – Kernel anticheat testing framework
- **afk-void/km-driver-ioctl** – Basic IOCTL kernel driver

### UnknownCheats Sources
- **Thread 683599** – How Do VGK ACE FACEIT Detect DMA? (Jan–April 2025); sushi404/rhaym on EAC/BE focus
- **Thread 689840** – Valorant DMA Firmware (March 2025)
- **Thread 688387** – EAC bypass for beginners (Feb 23–25, 2025); manual mapping common; Fortnite/Rust
- **Thread 688145** – EAC Linux kernel module (Feb–Aug 2025); Linux EAC userspace-only; Aug 2025 follow-ups
- **Thread 682955** – KMDU kernel dumper (Jan 2025)
- **Thread 678400** – Undetected injector EAC (Dec 22, 2024)
- **Thread 677857** – EAC now runs at boot (Dec 2024); disputed
- **Thread 448472** – Shared Buffer/FlushFileBuffers Communication (FoxiTV, 2021); original FlushFileBuffers technique
- **Thread 438804** – How does EAC/BE detect kernel driver bypasses? (Swiftik); pidbbcache, mmunloadeddrivers, big pool, thread hiding, MajorFunction/syscall hooks
- **Thread 4407129 (ConDrv)** – Stealthy IOCTL; MajorFunction verification (*"pointers to valid addresses within .text"*); NMI stack walking
- **Thread 647940** – Hooking NtDeviceIoControlFile to spoof PCI
- **Thread 635703** – EAC scanning unsigned drivers (May 2024)
- **Thread 594960 / 614327** – kdmapper/sinmapper detection on EAC/BE
- **Thread 391107** – Shared buffer synchronization (linked from FlushFileBuffers README)
- **UC Wiki** – Kernel driver creation, DeviceIoControl primary comm method

### 2026 Sources (EAC/Fortnite)
- **finance.biggo.com** – Fortnite Secure Boot, TPM, IOMMU (Feb 2026)
- **dsogaming.com** – Fortnite tournament anticheat requirements (Feb 2026)
- **gamingbible.com** – Fortnite mandatory hardware update (Feb 2026)
- **gamerant.com** – Fortnite anti-cheat changes (Feb 2026)
- **polygon.com** – Fortnite PC anticheat (Secure Boot, TPM, IOMMU)
- **tomshardware.com** – Epic Games Secure Boot/TPM for Fortnite
- **thefix.it.com** – EasyAntiCheat Error 2026 Gaming Solution Guide
- **hypercall.net** – Inside anti-cheat: EasyAntiCheat (architecture, IOCTL)

### June 2025+ Sources
- **blog.gistre.epita.fr** – A History of Anti-Cheat Techniques (June 24, 2025)
- **mixcheats.ru** – Cheat Detection Methods Explained 2025
- **klizosolutions.medium.com** – Vanguard's Shift to User-Mode AI Protection (June 5, 2025)
- **gamingonlinux.com** – THE FINALS kernel-based anti-cheat (July 2025)
- **blog.quarkslab.com** – BYOVD rootkit 2025 (CVE-2025-8061)
- **poh0.dev** – ThrottleStop vulnerable driver (CVE-2025-7771, 2025)

### 2024–2025 Sources
- **research.meekolab.com** – Understanding Kernel-Level Anticheats in Online Games (2024)
- **lxkast.github.io** – Thread and Call Stack Spoofing for NMI Callbacks on Windows (Trakimas)
- **archie-osu.github.io** – Inside Riot Vanguard's Dispatch Table Hooks (April 2025)
- **UnknownCheats** – Stealthy IOCTL using ConDrv / kernel communication (2024); MajorFunction verification, NMI stack walking
- **GitHub** – nolanpierce/ICALL-GADGET (evade stack walking via legit module redirect)
- **Microsoft Q&A** – EasyAntiCheat_EOSSys minifilter (Dec 2024)
- **ACM ARES 2024** – "If It Looks Like a Rootkit..." (Dorner, Klausner – kernel AC analysis)

### Historical
- **UnknownCheats** – Shared Buffer/FlushFileBuffers Communication (FoxiTV, 2021)
- **GitHub** – boom-cr3/Shared-FlushFileBuffers-Communication
- **tulach.cc** – Detecting manually mapped drivers (Samuel Tulach)
- **GitHub** – D4stiny/spectre (FILE_OBJECT IOCTL hooking)
- **Project** – EAC_FORTNITE_DETECTION_RESEARCH.md
