# PFN Zeroing & Big Pool Spoof – Research

**Updated 2025–2026:** Web research did not find specific public reports of PFN zeroing or big pool spoof detection by EAC/BattlEye in 2025–2026. EAC is known to use SystemBigPoolInformation (0x42) and stack-walk big pool entries; big pool spoofing is an ongoing arms race. PFN zeroing (NullPfn, UC 3020747) remains a known technique with no recent public detection reports.

---

## PFN Zeroing

### Purpose

After freeing driver memory (e.g. vuln driver unload, mapper cleanup), the physical page frame (PFN) may still contain residual data. ACs can scan PFN database or dump physical memory to find traces.

### Method (UC 3020747)

1. Get PFN index for the freed virtual address (via PTE → PFN).
2. Zero the MMPFN structure or the physical page content.
3. Requires: MmPfnDatabase, correct PFN layout per Windows build.

### Implementation Location

- **Mapper**: After `Unload()` of vuln driver, before exit. Would need kernel-mode or vuln driver to perform zeroing.
- **Our driver**: When trace_cleaner clears MmUnloadedDrivers, we don't have direct access to vuln driver's former pages. The mapper runs in usermode and uses vuln driver for kernel ops; after unload the vuln driver is gone.
- **Vuln driver before unload**: Add a "cleanup" IOCTL that zeros its own PFNs before service stop. Requires modifying vuln driver or using FillMemory to zero from kernel before unload.

### Status

- **Physical page zeroing implemented** – `trace_cleaner.hpp` `pfn_zero_pages()`: for each MmUnloadedDrivers vuln entry, uses `MmGetPhysicalAddress` + `MmMapIoSpace` to zero physical page contents before clearing. Best-effort: skips unmapped pages.
- **Placeholder**: Add to trace_cleaner or mapper when vuln driver supports pre-unload cleanup.
- **2025–2026**: No public reports of PFN zeroing detection; technique remains viable but undocumented.

---

## Big Pool Spoof

### Purpose

`ZwQuerySystemInformation(SystemBigPoolInformation)` returns large pool allocations. Our driver's 2MB guarded region (TnoC tag) can be detected. Spoofing = modify the big pool table so our allocation appears as something else (e.g. different tag, or unlink from enumeration).

### Method

1. Find big pool table in kernel (undocumented).
2. Locate our allocation (by VA or tag).
3. Either: unlink entry, change tag to benign, or spoof size/address.

### Risks

- Big pool table structure is undocumented; varies by build.
- Modifying kernel tables can trigger PatchGuard or integrity checks.
- Some ACs checksum the table.

### Status

- **Not implemented** – high risk, structure varies.
- **Current mitigation**: Use `--indPages` (independent pages) to avoid pool allocation; project uses pool due to indPages BSOD on some systems. Guarded region (TnoC) is still queried via `fget_guarded_region` for section-based comm.
- **2025–2026**: EAC uses SystemBigPoolInformation (0x42) and stack-walks big pool entries. Spoofing the big pool table is a known evasion; EAC may monitor for spoofing attempts. Arms race – no definitive “detected” or “safe” status.
