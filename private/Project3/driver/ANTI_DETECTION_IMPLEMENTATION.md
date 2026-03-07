# Anti-Detection Implementation Summary

Implemented improvements from research (2024–2026). Higher-risk mitigations (NMI/ICALL) documented as future work.

---

## Implemented (Lower Risk – Config & Runtime)

### 1. Magic XOR Obfuscation

- **Config:** `FLUSHCOMM_MAGIC_XOR` defaults to `0x5A3C9E1B` (was 0).
- **Effect:** Magic in shared buffer is XOR'd; reduces static signature scans.
- **Override:** `/DFLUSHCOMM_MAGIC_XOR=0x12345678` for per-build variation; `/DFLUSHCOMM_MAGIC_XOR=0` to disable.

### 2. Pool Tag Rotation

- **Config:** `FLUSHCOMM_POOL_TAG_ROTATE 1` (was 0).
- **Effect:** Pool tags vary per boot (Nls, Envl, MmPr, Orwk, Fls, Cc, Io, Mm).
- **Code:** All `ExAllocatePoolWithTag` / `ExFreePoolWithTag` use `EVASION_POOL_TAG_*_R`.
- **Order:** `page_evasion_init()` moved before `FlushComm_Init()` so tags are set before allocations.

### 3. ETW Patch Disabled by Default

- **Config:** `FLUSHCOMM_PATCH_ETW 0` (was 1).
- **Reason:** Some ACs detect ntdll!EtwEventWrite patches.
- **Override:** Set `FLUSHCOMM_PATCH_ETW 1` in config and call `EtwPatch::Init()` if you accept the risk.

### 4. Section/Registry Suffix Override

- **Config:** `FLUSHCOMM_OBFUSCATION_SUFFIX` – compile-time override for section and registry suffix.
- **Default:** `L"22631"` (Windows 11 23H2 build).
- **Override:** Pass the same value when building driver and usermode:
  ```
  /DFLUSHCOMM_OBFUSCATION_SUFFIX=L\"Xy7a9\"
  ```
- **Effect:** Avoids fixed `WdfCtl_22631` and `MdmTrace\22631` as static signatures.

### 5. Automatic Random Suffix (Release Builds)

**Implemented:** `Directory.Build.targets` generates `flush_comm_obfuscation_override.h` with a random 6-char suffix before each Release build of the driver.

- **Trigger:** Release|x64 build of Public.sln (driver project runs first, generates suffix for both).
- **Disable:** Set `FlushCommRandomizeSuffix=false` in the project or via `/p:FlushCommRandomizeSuffix=false`.
- **Default file:** `flush_comm_obfuscation_override.h` is committed with `L"22631"` for Debug/first builds.

---

## Higher Risk – Future Mitigations (Documented)

These require deeper changes and are not yet implemented.

### NMI Stack Walking

- **Issue:** ACs use NMI callbacks; RIP in manual-mapped code can be detected.
- **Feasibility:** See **NMI_STACK_WALKING_RESEARCH.md** for full analysis.
- **Recommended:** HalPreprocessNmi hook (lxkast/frame, Trakimas) – swap machine frame RIP/RSP to PoIdle + idle thread; add restoration callback. *Moderate effort*, public PoC available.
- **Alternatives:**
  1. **ICALL-GADGET** (nolanpierce): Repo unavailable; would need independent gadget research.
  2. **Signed codecave expansion:** Move more logic into Beep’s signed .data; size-limited.

### Fake Device MajorFunction

- **Issue:** Our fake device’s MajorFunction points into mapped code; device enumeration + MajorFunction checks could detect it.
- **Option (FoxiTV):** Chain to another legit driver’s MajorFunction; hook a second driver and redirect through it.

### BYOVD Alternative

- Use signed vulnerable drivers (e.g. CVE-2025-7771) instead of manual mapping for kernel R/W.
- Different threat model; avoids NMI exposure from mapped code.

---

## Config Summary

| Setting | Before | After | Effect |
|---------|--------|-------|--------|
| FLUSHCOMM_MAGIC_XOR | 0 | 0x5A3C9E1B | Magic obfuscation |
| FLUSHCOMM_POOL_TAG_ROTATE | 0 | 1 | Per-boot pool tag variation |
| FLUSHCOMM_PATCH_ETW | 1 | 0 | Avoid ETW patch detection |
| FLUSHCOMM_OBFUSCATION_SUFFIX | (none) | L"22631" default; override via /D | Section/registry suffix control |

---

## References

- UC ConDrv (4407129): MajorFunction verification, NMI stack walking
- Trakimas lxkast: HalPreprocessNmi stack spoofing
- nolanpierce/ICALL-GADGET: Evade stack walking via legit module redirect
- FoxiTV: Chain to second driver’s MajorFunction
- IOCTL_VS_ALTERNATIVES_RESEARCH.md, EAC_FORTNITE_DETECTION_RESEARCH.md
