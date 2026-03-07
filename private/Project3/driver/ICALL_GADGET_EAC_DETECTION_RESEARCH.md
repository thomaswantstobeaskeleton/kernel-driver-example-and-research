# ICALL-GADGET vs EAC Detection – Research

## Summary

**No public evidence that EAC specifically detects ICALL-GADGET.** The technique is designed to evade NMI-based stack walking by routing execution through ntoskrnl gadgets. However, there are **secondary detection risks** (stub allocation, execution patterns) and **uncertainty** about how much it actually helps against current EAC.

---

## What ICALL-GADGET Does

1. Finds `call rax` (FF D0) or `call [rax]` (FF 10) in ntoskrnl `.text`
2. Allocates a small stub (section, mapped into System process)
3. Stub: loads handler into rax, args into rcx/rdx, jumps to gadget
4. Gadget executes `call rax` → handler runs; **return address on stack = gadget (signed module)**
5. When NMI walks the stack, the immediate caller of our handler appears to be ntoskrnl

---

## EAC Detection Vectors (Relevant to ICALL)

| Vector | ICALL-GADGET Status |
|--------|---------------------|
| **NMI stack walking** | Intended to evade – return address is gadget in ntoskrnl |
| **Big pool scanning** | **Risk** – stub is allocated via `ZwCreateSection`; EAC scans for suspicious large allocations |
| **MmUnloadedDrivers / PidBbCache** | Unrelated – ICALL doesn’t change mapper traces |
| **HalPrivateDispatchTable** | **No risk** – ICALL does not hook HAL; no table modification |
| **Thread / call stack inspection** | Partially addressed – stack chain goes through gadget |
| **Hooking detection** | **No risk** – no syscall/VMT hooks; uses indirect call only |

---

## Community Guidance (UnknownCheats)

> "If you're going to hook a syscall in win32kbase/ntoskrnl (.data ptr or vmt swap), **you should route it to another memory destination which is located in a legit driver** (forward it to some useless function etc), and from there to your driver. This doesn't seem to be needed at BE, but **it's very much recommended for EAC**."
> — Swiftik, UC 438804

ICALL-GADGET follows this pattern: execution is routed through a legitimate kernel module (ntoskrnl) before reaching our code.

---

## Detection Risks

### 1. Stub allocation

- Stub is created with `ZwCreateSection` and mapped into System process
- Executable, non–disk-backed memory
- EAC’s big pool scanning can flag such allocations
- Mitigation: your project already uses trace cleaner and PFN zeroing; stub is small (~22 bytes)

### 2. Gadget fingerprinting

- `call rax` (FF D0) appears many times in ntoskrnl
- Hard to fingerprint a single gadget
- No reports of EAC scanning for or blacklisting specific gadgets

### 3. Execution timing

- NMI can fire at any moment
- If it fires while RIP is inside our handler (frw/fba), RIP is still in mapped driver
- ICALL mainly helps when the stack is inspected and the **caller** is checked; the immediate return address is the gadget
- Effectiveness depends on how EAC’s stack walker works (RIP vs return addresses vs full chain)

### 4. HalPreprocessNmi vs ICALL

- **HalPreprocessNmi hook**: Strong spoofing, but the hook itself is a known target (HalPrivateDispatchTable checks by Vanguard, EMACLAB)
- **ICALL-GADGET**: No HAL modification; only uses existing ntoskrnl code
- ICALL avoids the main weakness of NMI spoof (HAL table tampering)

---

## Comparison with NMI Spoof

| Aspect | ICALL-GADGET | HalPreprocessNmi Spoof |
|--------|--------------|-------------------------|
| HAL modification | None | Hook in HalPrivateDispatchTable |
| Table tampering checks | Not affected | Detectable (Vanguard, EMACLAB) |
| Stack appearance | Return addr = gadget | RIP/RSP spoofed to idle |
| Allocation risk | Stub section | Hook in mapped driver |
| PatchGuard | No impact | No impact (HAL table is hookable) |

---

## Conclusion

- **Direct ICALL detection**: No public evidence that EAC detects ICALL-GADGET itself.
- **Design**: Aligns with EAC bypass advice (routing through legitimate driver code).
- **Risks**: Stub allocation (big pool), and unclear benefit if EAC checks current RIP rather than only return addresses.
- **Recommendation**: Reasonable to keep ICALL enabled (`FLUSHCOMM_USE_ICALL_GADGET 1`) as an extra layer. If bans or detection appear, try disabling it and compare. Stub allocation risk is lower than HalPreprocessNmi’s table-tampering risk.

---

## Sources

- UnknownCheats 438804 – EAC kernel bypass detection vectors
- UnknownCheats 561479 – EAC instrumentation callback bypass
- Lukas Trakimas – Thread and Call Stack Spoofing for NMI Callbacks
- NMI_STACK_WALKING_RESEARCH.md – HalPrivateDispatchTable checks (Vanguard, EMACLAB)
- Elastic / GBHackers – Call gadget evasion for EDR (usermode; kernel case is analogous)
