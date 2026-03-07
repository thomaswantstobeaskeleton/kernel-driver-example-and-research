# LargePageDrivers Setup

Enables signed codecave execution for PING (liveness check). **Use LargePageDrivers for codecave when possible** – when `beep.sys` is in LargePageDrivers, the driver writes minimal shellcode to Beep's `.data` section; PING runs from signed memory (RIP in valid module). Preferred over MDL-based .text cave (FLUSHCOMM_SIGNED_CODECAVE_ONLY=1).

## Requirements

- **Run as Administrator**
- **Reboot** after applying changes

## Option 1: PowerShell (Recommended)

```powershell
# Right-click PowerShell -> Run as Administrator
cd path\to\Project3\scripts
.\setup_largepage_drivers.ps1
```

Handles merging with existing LargePageDrivers. Prompts for reboot.

## Option 2: Registry File

1. Double-click `setup_largepage_drivers.reg`
2. Confirm merge
3. **Reboot**

Note: Overwrites `LargePageDrivers` if it contains other entries. Use PowerShell to preserve existing drivers.

## Verify

After reboot, the driver will log `"Codecave installed (.data, LargePageDrivers)"` when FLUSHCOMM_USE_CODECAVE=1. If not configured, PING falls back to inline handler (no failure).

## Revert

Remove `beep.sys` from `HKLM\SYSTEM\CurrentControlSet\Control\Session Manager\Memory Management\LargePageDrivers` (REG_MULTI_SZ), then reboot.
