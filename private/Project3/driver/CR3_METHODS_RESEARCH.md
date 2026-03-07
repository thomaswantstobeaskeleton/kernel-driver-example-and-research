# CR3 Retrieval Methods – Research Summary

Research from UnknownCheats, GitHub, and security blogs on obtaining process CR3/DirectoryTableBase for kernel drivers.

---

## 1. KeStackAttachProcess + __readcr3()

**Source:** UnknownCheats, Microsoft docs, general kernel dev

**How it works:** Attach to target process → CR3 register is switched to that process’s PML4 → read with `__readcr3()`.

**Pros:**
- No MmPfn scan
- No pattern scans
- Straightforward
- Works for normal processes

**Cons:**
- Some anti-cheats check use of `__readcr3`
- Fails on protected/PPL processes

**Code:**
```cpp
KeStackAttachProcess(process, &apc_state);
dirbase = __readcr3() & 0xFFFFFFFFFFFFF000ULL;
KeUnstackDetachProcess(&apc_state);
```

---

## 2. EPROCESS + KPROCESS.UserDirectoryTableBase

**Source:** UnknownCheats, Vergilius, Geoff Chappell

**How it works:** Use `PsLookupProcessByProcessId` to get EPROCESS, read `UserDirectoryTableBase` from KPROCESS (embedded at start of EPROCESS).

**Offset:** Usually `EPROCESS + 0x28` for x64 Windows 10/11 (KPROCESS.DirectoryTableBase).

**Pros:**
- Simple struct read
- No attach
- No scanning

**Cons:**
- May be encrypted on some builds
- Offset can change with Windows version

**Version-specific offsets (UnknownCheats get_winver):**
| Build   | Offset |
|---------|--------|
| 1803    | 0x278  |
| 1903+   | 0x280  |
| 2004+   | 0x388  |

---

## 3. Encrypted DTB check (UnknownCheats)

**Source:** [UC thread 619886](https://www.unknowncheats.me/forum/anti-cheat-bypass/619886-cr3-kestackattachprocess.html)

If `(process_dirbase >> 0x38) == 0x40`, the DTB from EPROCESS can be encrypted. Use KeStackAttachProcess + __readcr3 when this happens.

---

## 4. MmPfn bruteforce

**Source:** [MapleSwan/enum_real_dirbase](https://github.com/MapleSwan/enum_real_dirbase)

**How it works:**
1. Resolve MmPfn database via pattern scan in ntoskrnl.
2. Init PTE self-map via `init_pte_base()`.
3. Iterate `MmGetPhysicalMemoryRanges()`.
4. For each PFN, read `_MMPFN` and check `pte_address == cr3_ptebase`.
5. Decrypt EPROCESS: `((flags | 0xF000000000000000) >> 0xd) | 0xFFFF000000000000`.
6. Match to target process; CR3 = `pfn << 12`.

**Pros:**
- Works without KeStackAttachProcess
- Avoids __readcr3
- No EPROCESS offset dependence

**Cons:**
- Version-sensitive patterns and decryption
- Can break with VBS/HVCI
- Heavy scan

---

## 5. Low Stub (HVCI, physical R/W only)

**Source:** [xacone – KASLR leak 24H2](https://xacone.github.io/kaslr_leak_24h2.html)

**How it works:** On HVCI systems, a Low Stub exists in the first MB of physical RAM (0x0–0x100000). At `LowStub + 0x90` is `KPROCESSOR_STATE`, and at `SpecialRegisters + 0x10` is CR3.

**Pros:**
- Works when you only have physical memory access
- No KeStackAttachProcess or EPROCESS reads

**Cons:**
- Needs physical memory read
- Must locate Low Stub (scan first MB)

---

## 6. Derive from process base (paidtoomuch/hv.sol-fortnite)

**Source:** [GitHub](https://github.com/paidtoomuch/hv.sol-fortnite)

**How it works:** Use `PsGetProcessSectionBaseAddress` for base, then a custom `pml4::dirbase_from_base_address(base)` to obtain DTB from virtual base. Likely walks page tables or uses similar physical mapping logic.

---

## 7. EPROCESS list enumeration

**Source:** UnknownCheats (hun7er999)

Walk the EPROCESS list until you find the target process, then read `DirectoryTableBase` from KPROCESS.

---

## Recommended order for this driver (EAC / UC 619886)

Per EAC_DETECTION_RESEARCH_UC.md and UC 619886: **__readcr3 and KeStackAttachProcess are detected by EAC** – use as last resort only.

| Order | Method | Detection | Notes |
|-------|--------|-----------|-------|
| 1 | **Physical scan (dirbase_from_base_address)** | Least | From process base via page-table walk; bypasses encrypted DTB. |
| 2 | **MmPfn bruteforce** | Low | No attach; avoid __readcr3. Pattern/decryption version-sensitive. |
| 3 | **EPROCESS UserDirectoryTableBase** | Low | Direct read when not encrypted (check `dtb>>56 != 0x40`). |
| 4 | **KeStackAttachProcess + __readcr3** | Most (EAC) | UC 619886: detected. Use only when 1–3 fail. |
