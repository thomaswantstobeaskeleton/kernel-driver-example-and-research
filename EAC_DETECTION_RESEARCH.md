# EAC Detection Research: Least-Detected CR3/Base Methods for Fortnite

Research from UnknownCheats, security blogs, and GitHub (Feb 2025) on which kernel techniques Fortnite’s Easy Anti-Cheat (EAC) detects most/least.

---

## Detection Risk Ranking (Least → Most Detected)

| Rank | Method | Detection Risk | Why |
|------|--------|-----------------|-----|
| **1** | **Physical memory scan (dirbase_from_base_address)** | **Lowest** | No attach, no EPROCESS CR3 read, no context switch. Uses MmMapIoSpaceEx to walk physical page tables. EAC does not routinely monitor physical mapping of non-game RAM. |
| **2** | **MmPfn bruteforce** | **Low** | No KeStackAttachProcess. Reads MmPfnDatabase (kernel structure) and matches PFN to EPROCESS. Indirect; no direct attach or __readcr3. |
| **3** | **EPROCESS UserDirectoryTableBase** | **Medium** | Simple struct read, no attach. Fails when DTB is encrypted (VBS). EAC knows EPROCESS is used for cheating, but direct reads are harder to hook than KeStackAttach. |
| **4** | **PsGetProcessSectionBaseAddress** | **Medium** | Standard kernel API. EAC can hook it but does not appear to do so routinely. Used by many legitimate components. |
| **5** | **KeStackAttachProcess + __readcr3** | **Highest** | Well-known technique. EAC can detect it via thread context: offset 0xB8 on threads changes when attached. Often monitored and flagged. |

---

## Evidence by Method

### KeStackAttachProcess (Most Detected)

- **UC 619886** (“How to Get CR3 Without KeStackAttachProcess”): EAC is known to detect KeStackAttachProcess; alternatives are used specifically to avoid it.
- **KANKOSHEV/Detect-KeAttachProcess**: Detection relies on thread offset 0xB8 storing the attached process instead of the current process.
- **0avx / UC**: EAC uses context-switch hooks and thread whitelisting; attachment-based methods are a primary target.

**Verdict:** Avoid when possible. Use only as last resort.

---

### EPROCESS / UserDirectoryTableBase

- EPROCESS CR3 read is a common alternative to KeStackAttachProcess.
- With VBS/HVCI, UserDirectoryTableBase is encrypted (high byte 0x40), so this path often fails and is skipped.
- EAC monitors CR3 access; direct EPROCESS reads are detectable but typically less instrumented than KeStackAttachProcess.

**Verdict:** Prefer over KeStackAttachProcess, but may fail on VBS systems.

---

### Physical Memory Scan (dirbase_from_base_address)

- **UC 591802** (“Search physical memory to find target process’s cr3”): Describes scanning physical memory for CR3 without attaching.
- **UC 444289** (“read process physical memory, no attach”): Physical R/W without attach is a known evasion strategy.
- **UC 597070** (“Physical RW with EAC Decryption”): Physical R/W plus EAC decryption is used to avoid detection.
- **MmMapIoSpaceEx**: Physical mapping is typically not monitored like MmCopyMemory; EAC uses MmCopyMemory internally and is suspected to trace its use.

**Verdict:** Best option for stealth: no attach, no EPROCESS CR3 read, relies on physical page-table walk. Depends on PsGetProcessSectionBaseAddress returning a valid base.

---

### MmPfn Bruteforce

- **MapleSwan/enum_real_dirbase**: MmPfn scan to enumerate CR3 without KeStackAttachProcess.
- Does not attach; uses MmPfnDatabase and physical memory layout to match PFN → EPROCESS.
- More involved than EPROCESS read, but avoids KeStackAttachProcess and direct CR3 attach-based reads.

**Verdict:** Good stealth properties; pattern scans and layout can be fragile across builds.

---

### MmCopyMemory / MmCopyVirtualMemory

- **UC 496628** (“MmCopyVirtualMemory Detected On EAC/BE?”): EAC uses MmCopyMemory for its own scans and may trace/monitor its usage.
- Your driver uses **MmMapIoSpaceEx** instead of MmCopyMemory for physical reads (“EAC traces it” in comments).
- For usermode ↔ kernel copy, you use MmCopyVirtualMemory over the shared buffer; this is standard and less likely to be singled out than MmCopyMemory on arbitrary game memory.

**Verdict:** Physical R/W via MmMapIoSpaceEx is preferable to MmCopyMemory for cheat-style access.

---

## Recommended Order for Your Driver

Given your current implementation and EAC:

1. **Method 1.5 (dirbase_from_base_address)** – Physical scan when PsGetProcessSectionBaseAddress succeeds. Lowest detection risk.
2. **Method 1 (EPROCESS)** – Use when DTB is not encrypted; avoid if (dtb >> 0x38) == 0x40.
3. **Method 3 (MmPfn bruteforce)** – Fallback when 1 and 1.5 fail; no attach.
4. **Method 2 (KeStackAttachProcess)** – Last resort; highest detection risk.

---

## Sources

| Source | URL/Topic |
|-------|-----------|
| UC 619886 | How to Get CR3 Without KeStackAttachProcess |
| UC 591802 | Search physical memory for target CR3 |
| UC 444289 | Read physical memory, no attach |
| UC 496628 | MmCopyVirtualMemory detection EAC/BE |
| UC 597070 | Physical RW with EAC decryption |
| KANKOSHEV | Detect-KeAttachProcess (0xB8 offset) |
| 0avx | EasyAntiCheat CR3 protection |
| aylers/EAC-CR3-Fix | dirbase_from_base_address approach |

---

## Summary

For Fortnite with EAC:

- **Most stealthy:** Physical memory scan (dirbase_from_base_address) using PsGetProcessSectionBaseAddress + MmMapIoSpaceEx.
- **Most risky:** KeStackAttachProcess + __readcr3.

Your current ordering (1.5 before 2) aligns with this. Consider deprioritizing or disabling Method 2 if stability is acceptable without it.
