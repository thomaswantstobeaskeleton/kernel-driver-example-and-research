# Codecave / Shellcode-in-Module Research

## Summary of Approaches

### 1. **LargePageDrivers + .data Section** (RECOMMENDED – works without MDL)

**Source:** [VollRagm/lpmapper](https://github.com/VollRagm/lpmapper), [blog post](https://vollragm.github.io/posts/abusing-large-page-drivers/)

**How it works:**
- Windows has a registry key `HKLM\SYSTEM\CurrentControlSet\Control\Session Manager\Memory Management\LargePageDrivers` (multi-string).
- Drivers listed there are loaded on **2MB large pages** instead of 4KB.
- Small drivers (e.g. **beep.sys**) fit entirely in one 2MB page, so `.text` and `.data` share the same page.
- Page protection is applied to the whole 2MB, so both become **read + write + execute**.
- The `.data` section is normally RW but not X; on a large page it becomes RWX.
- Write shellcode into `.data` → no MDL, no protection tweaks. Point the dispatch to that address.

**Setup (one-time, requires reboot):**
1. Open Registry → `HKLM\SYSTEM\CurrentControlSet\Control\Session Manager\Memory Management`
2. Create multi-string value `LargePageDrivers`
3. Add `beep.sys` (or `null.sys`, or `*` for all drivers – not advised)
4. Reboot

**Targets:** beep.sys, null.sys, or any small driver whose `.data` fits in the large page with `.text`.

---

### 2. **Code Cave in .text** (Crashes in our setup)

**Source:** [rogerxiii/kernel-codecave-poc](https://github.com/rogerxiii/kernel-codecave-poc)

**Idea:** Find padding (0x00 / 0xCC) inside `.text`, inject shellcode there. RIP stays in a signed module.

**Issue:** `.text` is read-only. Direct writes cause `ATTEMPTED_WRITE_TO_READONLY_MEMORY` (0xBE). MDL + `MmProtectMdlSystemAddress` also crashed, likely because driver image pages are treated differently. Not viable without solving this.

---

### 3. **Pool Allocation (PAGE_EXECUTE_READWRITE)**

Allocate pool, mark executable, put shellcode there, and jump to it. Works technically, but RIP is in pool, not inside a signed module, so anti-cheat can flag it.

---

### 4. **Inline Handler**

Current behavior: PING handler runs in our manual-mapped code. Stable, but RIP is outside valid signed modules.

---

## Recommended Path: LargePageDrivers + .data

- One-time setup: add `beep.sys` to LargePageDrivers and reboot.
- At runtime: locate Beep’s `.data` (PE parsing), write shellcode there, set `IRP_MJ_DEVICE_CONTROL` to that address.
- `.data` is writable when Beep is on a large page; no MDL or protection tricks.
- Code executes inside Beep’s image, so RIP stays in a valid signed module.

## Implementation in This Driver

The driver now supports the **LargePageDrivers + .data** approach:

1. When hooking Beep/Null/PEAuth, it checks whether that driver (e.g. `beep.sys`) is in the LargePageDrivers registry list.
2. If yes: finds the `.data` section, writes PING shellcode there, and sets the dispatch to that address.
3. If no (or write fails): uses the inline PING handler; no crash.

**One-time setup for codecave:**
- Add `beep.sys` (or `null.sys`) to `HKLM\SYSTEM\CurrentControlSet\Control\Session Manager\Memory Management\LargePageDrivers` (REG_MULTI_SZ)
- Reboot

## References

- [lpmapper](https://github.com/VollRagm/lpmapper)
- [Abusing LargePageDrivers](https://vollragm.github.io/posts/abusing-large-page-drivers/)
- [kernel-codecave-poc](https://github.com/rogerxiii/kernel-codecave-poc)
- [UC: lpmapper release](https://www.unknowncheats.me/forum/anti-cheat-bypass/495784-lpmapper-execute-shellcode-drivers-data-section.html)
