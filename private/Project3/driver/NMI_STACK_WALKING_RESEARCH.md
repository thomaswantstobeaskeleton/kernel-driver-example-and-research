# NMI Stack Walking Evasion – Research & Feasibility

Research on evading anti-cheat NMI (Non-Maskable Interrupt) stack walking when executing in manually mapped drivers. Sources: lxkast/frame, Trakimas blog, UC, ICALL-GADGET, DErDYAST1R/NmiCallbackBlocker.

**Last updated:** Feb 2026

**Implementation:** `nmi_spoof.cpp` / `nmi_spoof.hpp`. Enable with `FLUSHCOMM_USE_NMI_SPOOF 1` in `flush_comm_config.h`.

---

## How NMI Detection Works

1. ACs register NMI callbacks via `KeRegisterNmiCallback`.
2. NMIs are sent to cores (`HalSendNMI` or similar).
3. On NMI, Windows calls each callback. The interrupted state is captured: **RIP**, **RSP**, and call stack.
4. If RIP or any return address points outside valid kernel modules → unsigned driver execution → detection.

**Key structures:**
- **MACHINE_FRAME** (IRETQ frame): pushed by CPU on interrupt; holds old RIP, RSP, RFLAGS, CS, SS.
- **KiNmiCallbackListHead**: linked list of NMI handler callbacks.
- **TSS Ist[3]**: NMI interrupt stack; machine frame is at `TSS.Ist[3] - sizeof(MACHINE_FRAME)`.

---

## Method 1: HalPreprocessNmi Hook (lxkast/frame)

### Approach
- Hook `HalPreprocessNmi` in the HAL private dispatch table (`HalPrivateDispatchTable`).
- Before NMI callbacks run, replace machine frame RIP/RSP with valid values (e.g. `PoIdle`).
- Replace `CurrentThread` with idle thread so the stack trace looks like idle.
- Add a restoration callback to the **end** of `KiNmiCallbackListHead` to restore state before IRET.

### Pros
- **PatchGuard:** HAL private dispatch table is documented as overridable; no PatchGuard violation.
- **Proven:** lxkast/frame PoC exists; Trakimas describes it in detail.
- **Conceptually straightforward:** Swap frame, restore after callbacks.

### Cons
- **Signature scanning:** `KiNmiCallbackListHead` found via pattern `48 8B 3D ? ? ? ? 41 8A F4`; `PoIdle` via `40 55 53 41 56`. Patterns can break across Windows versions.
- **HAL hook detection:** ACs may check HAL dispatch table; pointers outside valid HAL/ntoskrnl could be flagged.
- **Our handler in mapped code:** `HalPreprocessNmiHook` itself runs from our driver. If AC checks the **call stack to our hook** (who called HalPreprocessNmi), that path is still kernel → our code. The spoofing helps the *interrupted* thread; our hook is in the NMI path, not the interrupted context. Trakimas focuses on spoofing the interrupted thread; the hook runs at HIGH_LEVEL and is part of the NMI handling path.
- **Version sensitivity:** Offsets (TSS Ist index, machine frame layout) and signatures vary with Windows build.

### Implementation Feasibility: **Moderate**
- Need: HAL dispatch resolve, signature scans for `KiNmiCallbackListHead` and `PoIdle`, per-CPU restore data.
- lxkast/frame code is a usable reference; adaptation for manual-mapped use is required.

---

## Method 2: ICALL-GADGET (nolanpierce)

### Approach
- Use indirect-call gadgets in legitimate kernel modules.
- Redirect control flow so execution appears to come from signed code.
- Stack return addresses stay within known modules.

### Pros
- No HAL hook; less obvious kernel modification.
- Execution path goes through legit modules.

### Cons
- **ICALL-GADGET repo:** 404; no public implementation available.
- **Complexity:** Requires gadget discovery, control-flow manipulation, and careful integration with request handling.
- **Manual mapping:** Our handler must still run somewhere; the trick is making the call chain look valid. Full integration is non-trivial.

### Implementation Feasibility: **Low** (without source)
- No reference implementation; would require independent research and custom gadget-finding.

---

## Method 3: NMI Callback Blocker (DErDYAST1R)

### Approach
- Find `NMI_IN_PROGRESS` in kernel memory.
- Modify processor affinity via DKOM so NMIs are not delivered to certain cores.

### Cons
- **PatchGuard:** DKOM and affinity tampering are highly likely to trigger PatchGuard.
- **AC detection:** Modifying NMI delivery is an obvious evasion and likely monitored.

### Implementation Feasibility: **Not recommended**

---

## Method 4: Signed Codecave (Already Implemented)

- PING runs from Beep’s signed `.data` (LargePageDrivers).
- Main handler (`FlushComm_ProcessSharedBuffer`) still runs in mapped driver.
- NMI during handler execution would still see mapped code on the stack.

### Limitation
- Codecave is too small for full request handling; only PING benefits.

---

## Recommended Path: HalPreprocessNmi (Method 1)

Given public PoC and documentation, **HalPreprocessNmi hook + machine-frame spoofing** is the most realistic option.

### Integration Outline

1. **Dependencies**
   - Resolve `HalPrivateDispatchTable` via `MmGetSystemRoutineAddress`.
   - Signature-scan `KiNmiCallbackListHead` (with fallbacks for different builds).
   - Signature-scan `PoIdle` for idle-thread RIP spoofing.

2. **Hook placement**
   - Install hook during `DriverEntry` (after trace cleaner, before FlushComm).
   - Store `HalPreprocessNmiOriginal`; replace with our hook.

3. **Spoofing logic**
   - In hook (when `arg1 == 0`): locate machine frame via `KeGetPcr()->TssBase->Ist[3]`.
   - Save RIP/RSP/CurrentThread/NextThread.
   - Set RIP = `PoIdle`, RSP = idle thread stack − 0x38, CurrentThread = IdleThread.
   - Append restoration callback to end of NMI callback list.

4. **Restoration callback**
   - Restore saved RIP, RSP, CurrentThread.
   - Unlink our callback from the list.
   - Return `handled` unchanged.

5. **Unload**
   - Restore original `HalPreprocessNmi` in HAL dispatch table.

### Risks
- **HAL table checks:** Some ACs may validate HAL dispatch pointers; hook address must be plausible (e.g. in a loaded driver).
- **Signature breaks:** Patterns are build-specific; may need maintenance for new Windows versions.
- **Manual mapping:** Our hook code is in mapped memory; if AC inspects the HAL table and follows the pointer, it may see a non-module address. Mitigation: use a codecave in a signed driver for the hook (e.g. lpmapper-style) so the hook itself lives in signed space.

### Codecave for Hook
- If the hook is small enough, it could be placed in Beep’s `.data` codecave (like PING).
- The hook would call into our mapped driver for the main logic (finding frame, updating callback list, etc.), which reintroduces mapped code on the stack. Full evasion would require keeping the whole spoof path in signed memory, which is harder.

### Summary
- **Feasibility:** HalPreprocessNmi spoofing is implementable with moderate effort.
- **Best reference:** [lxkast/frame](https://github.com/lxkast/frame), [Trakimas write-up](https://lxkast.github.io/posts/Thread-and-call-stack-spoofing-for-NMI-callbacks-on-Windows/).
- **Effort:** ~1–2 days for a first version; additional time for version coverage and testing.

---

## Is NMI Walking (and HalPreprocessNmi Spoof) Detected? (2025–2026 Research)

### NMI Stack Walking: Actively Used by Anti-Cheats

- **EAC, EMACLAB, Meekolab:** Use NMI callbacks + thread stack walking to detect unsigned drivers. NMI fires on cores, AC captures RIP/RSP/call stack; addresses outside valid kernel modules → unsigned execution → detection.
- **EMACLAB** (EMAC-Driver-x64.sys, March 2025): Explicitly includes "Checks if some functions from **HalPrivateDispatchTable are being tampered**" in its integrity checks. NMI callback logic is virtualized; APC/work-item style `EmacVerifyKernelThreadsStackTrace` walks system threads.
- **Trakimas:** "This table and other HAL structures are usually monitored by anti-cheats and anti-rootkits. It's far easier to detect a manually mapped driver if these tables store pointers outside of valid regions than detecting execution with NMIs in the first place."

### HalPrivateDispatchTable: Known Detection Vector

- **Vanguard** (April 2025, archie-osu): Actively scans the entire `HalPrivateDispatchTable` for pointers into `vgk.sys` (and presumably flags non-module pointers). Documented hooks: `HalCollectPmcCounters` (0x248), `KiClearLastBranchRecordStack` (0x400). Any table entry pointing outside known modules is detectable.
- **HalPreprocessNmi** lives at offset **0x3e8** in the same table. A hook that points to manually mapped driver memory will fail a simple "is this address in ntoskrnl/hal/vgk/loaded modules?" check.
- **EMACLAB:** Explicitly checks for HalPrivateDispatchTable tampering.

### HalPreprocessNmi Spoof: Spoof Works, Hook Is Detectable

| Aspect | Status |
|--------|--------|
| **Spoof effectiveness** | Evades NMI stack trace inspection: interrupted thread looks idle. |
| **Hook placement** | HalPrivateDispatchTable modification is a known detection target. |
| **Manual mapping** | Hook pointer in mapped memory = non-module address = detectable by table scans. |
| **IDTR/alternatives** | IDTR swapping causes PatchGuard races; unstable vs EAC (UC 710851). |

### Mitigations (Partial)

1. **Signed codecave for hook:** Put a minimal trampoline in Beep's codecave; trampoline calls into mapped driver. Hook address is in a valid module, but the call chain still reaches mapped code.
2. **Full codecave spoof:** Implement the whole spoof logic in the codecave. Very tight; may not fit and increases complexity.
3. **Risk vs target:** Against ACs that do not scan HalPrivateDispatchTable, the spoof can still be useful. Against Vanguard/EMACLAB-style table checks, the hook itself is a detection risk.

### Conclusion

- **NMI stack walking:** Actively used; spoofing the interrupted state (RIP/RSP/CurrentThread) is effective.
- **HalPreprocessNmi hook:** Effective for spoofing, but the hook is detectable when ACs validate HalPrivateDispatchTable.
- **Practical advice:** Keep NMI spoof disabled (`FLUSHCOMM_USE_NMI_SPOOF 0`) when targeting Vanguard or ACs with similar table checks. Enable for environments where HalPrivateDispatchTable tampering is not checked.

### Sources (2025–2026)

- **Trakimas** – [Thread and Call Stack Spoofing for NMI Callbacks on Windows](https://lxkast.github.io/posts/Thread-and-call-stack-spoofing-for-NMI-callbacks-on-Windows/)
- **EMACLAB** – [crvv.dev/emac-anticheat-driver-part6](https://crvv.dev/emac-anticheat-driver-part6/) – HalPrivateDispatchTable tampering check
- **Vanguard** – [archie-osu.github.io/2025/04/11/vanguard-research](https://archie-osu.github.io/2025/04/11/vanguard-research.html) – Full HalPrivateDispatchTable scan
- **UC 710851** – NMI & IDTR Patchguard discussion
- **revers.engineering** – Fun with another PatchGuard-compliant Hook (HalCollectPmcCounters)

---

## References

- **lxkast/frame** – PoC kernel driver for NMI stack spoofing
- **Trakimas** – [Thread and Call Stack Spoofing for NMI Callbacks on Windows](https://lxkast.github.io/posts/Thread-and-call-stack-spoofing-for-NMI-callbacks-on-Windows/)
- **ICALL-GADGET** – nolanpierce (repo unavailable; concept only)
- **NmiCallbackBlocker** – DErDYAST1R (DKOM approach; not recommended)
- **UC ConDrv (4407129)** – MajorFunction checks, NMI stack walking
- **Meekolab** – Kernel-level anticheat NMI usage
