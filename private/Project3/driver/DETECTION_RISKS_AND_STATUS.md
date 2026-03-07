# Detection Risks – Audit & Status

Audit of implementations that could be flagged or detected by anti-cheats (EAC, BattlEye, Vanguard, EMACLAB). Last updated Feb 2026.

---

## Summary

| Item | Risk | Status | Action |
|------|------|--------|--------|
| NMI spoof (HalPreprocessNmi) | **High** – HalPrivateDispatchTable scanned by Vanguard, EMACLAB | **Disabled** | `FLUSHCOMM_USE_NMI_SPOOF 0` |
| ETW patch | **Medium** – Some ACs detect ntdll patches | **Disabled** | `FLUSHCOMM_PATCH_ETW 0` |
| Trace cleaner (MmUnloadedDrivers) | **Medium** – Tampering detectable; but vuln trace = primary threat | **Enabled** | `FLUSHCOMM_TRACE_CLEANER 1` (default) |
| WSK / Sockets | **High** – EAC ScanSystemThreads detects socket threads | **Disabled** | `FLUSHCOMM_USE_WSK 0` |
| Direct syscall | **Low–Medium** – Instrumentation callbacks can detect | Enabled | Tradeoff vs ntdll hooks |
| FILE_OBJECT hook | **Low** – Less detectable than MajorFunction; still modifies CREATE | Enabled | Best option for comm |
| Pool tag "File" (spoof.h) | **Medium** – Suspicious cheat-like tag | **Fixed** | Use benign tag |
| Pool tag 'gLpC' (codecave) | **Low** – Non-evasion tag | **Fixed** | Use EVASION_POOL_TAG_REG_R |
| ExAllocatePool (no tag) | **Low** – Untagged allocations can be scanned | **Fixed** | Use tagged allocations |
| Pool tag rotation | **Low** – Fixed tags = normal; rotation = evasion heuristic | **Disabled** | `FLUSHCOMM_POOL_TAG_ROTATE 0` (default) |
| Codecave (beep.sys .data) | **Low–Medium** – Modifies signed driver; integrity checks | Enabled | Needed for PING from signed mem |

---

## 1. NMI Spoof – **DISABLED**

- **Risk:** HalPrivateDispatchTable tampering checked by Vanguard, EMACLAB (2025).
- **Status:** `FLUSHCOMM_USE_NMI_SPOOF 0`
- **Reason:** Hook pointer in manually mapped memory is detectable.

---

## 2. ETW Patch – **DISABLED**

- **Risk:** Some ACs detect ntdll!EtwEventWrite patches.
- **Status:** `FLUSHCOMM_PATCH_ETW 0`
- **Reason:** Patch is a known detection vector.

---

## 3. Trace Cleaner (MmUnloadedDrivers)

- **Risk:** EMACLAB checks MmUnloadedDrivers. Zeroing entries without updating MmLastUnloadedDriver/checksum can cause integrity mismatches. Tampering may be detected.
- **Status:** Configurable via `FLUSHCOMM_TRACE_CLEANER` (default 1).
- **Tradeoff:** Disabling exposes vuln driver (kdmapper) traces. Enabling may trigger tampering checks. Standard practice for mappers is to clear.
- **Recommendation:** Keep enabled unless targeting ACs that specifically detect MmUnloadedDrivers tampering.

---

## 4. WSK / Socket Communication – **DISABLED**

- **Risk:** EAC ScanSystemThreads detects socket/listener thread patterns.
- **Status:** `FLUSHCOMM_USE_WSK 0`
- **Reason:** Config comment: "EAC detects socket/thread patterns"

---

## 5. Direct Syscall (NtDeviceIoControlFile)

- **Risk:** Instrumentation callbacks can detect direct syscalls (return address not in ntdll).
- **Status:** Enabled. Alternative is DeviceIoControl via ntdll (also hooked by ACs).
- **Recommendation:** Keep; common bypass approach.

---

## 6. FILE_OBJECT Hook

- **Risk:** Modifies Beep's `IRP_MJ_CREATE` and `FILE_OBJECT->DeviceObject`. Less invasive than full MajorFunction hook.
- **Status:** Enabled. Preferred over direct MajorFunction hook.
- **Recommendation:** Keep; research indicates lower detection than MajorFunction.

---

## 7. Suspicious Pool Tags – **FIXED**

- **spoof.h:** Used `"File"` – cheat-like, scannable.
- **codecave.cpp:** Used `'gLpC'` – not in evasion set.
- **driver.cpp:** `ExAllocatePool` (no tag) for dump and BigPool – untagged allocations.
- **Fix:** Use `EVASION_POOL_TAG_*_R` or benign tags (`'kwrO'` for spoof.h).

---

## 8. Pool Tag Rotation

- **Risk:** Runtime rotation is evasion behavior; normal drivers use fixed tags. ACs (e.g. EMACLAB) scan BigPool for known malicious tags.
- **Status:** `FLUSHCOMM_POOL_TAG_ROTATE 0` (default).
- **Recommendation:** Fixed benign tags (tsNl, lvEn, rPmM, kwrO) behave like normal drivers; rotation can be heuristically flagged.

---

## 9. Codecave (Beep .data)

- **Risk:** Writes to signed driver memory. EMACLAB does self-integrity; third-party driver checks possible.
- **Status:** Enabled. `FLUSHCOMM_SIGNED_CODECAVE_ONLY 1` (no .text MDL).
- **Recommendation:** Keep; improves PING execution from signed memory.

---

## Research Conclusion (Feb 2026)

### FLUSHCOMM_TRACE_CLEANER → **Keep enabled (1)**
- EAC, BattlEye, EMACLAB read MmUnloadedDrivers for vuln driver info (capcom, iqvw64e, etc.).
- UC/EAC guidance: "must clear traces including MmUnloadedDrivers" when using mappers.
- Cleaning removes vuln driver entries so ACs don't find them. Primary threat is vuln driver presence.
- Tampering detection exists but is secondary; not cleaning = guaranteed vuln trace visibility.
- **Default: 1**

### FLUSHCOMM_POOL_TAG_ROTATE → **Disable (0)**
- EMACLAB: "Iterate BigPool list, searches for known malicious tags" – blocklist approach.
- Our tags (tsNl, lvEn, rPmM, kwrO) are benign Windows-style; won't match malicious blocklist.
- Fixed tags = normal driver behavior. Rotation = evasion pattern ("why vary tags at boot?").
- Safer to behave like a normal driver than use evasion heuristics.
- **Default: 0**

---

## Config Additions

- `FLUSHCOMM_TRACE_CLEANER` – 1 = run trace cleaner (default), 0 = skip (exposes vuln traces).
- `FLUSHCOMM_POOL_TAG_ROTATE` – 0 = fixed benign tags (default), 1 = rotate per boot.

---

## References

- NMI_STACK_WALKING_RESEARCH.md
- EAC_FORTNITE_DETECTION_RESEARCH.md
- ANTI_DETECTION_MEASURES.md
- crvv.dev/emac-anticheat-driver-part6
- archie-osu.github.io/2025/04/11/vanguard-research
