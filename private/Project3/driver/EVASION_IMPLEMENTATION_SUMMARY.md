# Evasion Implementation Summary

## Implemented

### 1. Mapper exe rename
- **Config**: `dependencies/configs/mapper_config.h` – `MAPPER_EXE_BENIGN_NAME`
- **Usage**: Set to `"IntelCpHDCP.exe"` (or other benign name), build kdmapper with that output name
- **Loader**: `includes.h` LoadDriver() looks for benign name first, then kdmapper.exe, mapper.exe

### 2. Download URL obfuscation
- **Default**: `skCrypt("https://...")` in `includes.h` – compile-time encrypted
- **Override**: `#define MAPPER_DOWNLOAD_URL` before includes.h for custom URL (use skCrypt for obfuscation)

### 3. String encryption in mapper
- **kdm_obfuscate.hpp**: `GetNtoskrnlName()`, `GetNtkrnlmpName()` – XOR-decrypted at runtime
- **intel_driver.cpp**, **eneio_driver.cpp**: Use obfuscated names instead of literals

### 4. ETW patch
- **Config**: `FLUSHCOMM_PATCH_ETW 0` (disabled – some ACs detect)
- **Implementation**: `utilities/etw_patch.hpp` – patch ntdll!EtwEventWrite
- **Enable**: Set to 1 in flush_comm_config.h; `EtwPatch::Init()` called from entrypoint.cpp

### 5. Wdfilter trace cleanup
- **Config**: `FLUSHCOMM_TRACE_CLEANER_WDFILTER 0` (disabled – Wdfilter monitored)
- **trace_cleaner.hpp**: `trace_clean_wdfilter()` placeholder – RuntimeDriverList cleanup
- **Enable**: Set to 1 for Wdfilter cleanup (high risk)

### 6. g_KernelHashBucketList cleanup
- **trace_cleaner.hpp**: `trace_clean_hash_bucket()` placeholder – called after MmUnloadedDrivers
- **Status**: Placeholder; add pattern per build when symbols unavailable

### 7. Trace cleaner (enabled)
- **Config**: `FLUSHCOMM_TRACE_CLEANER 1`
- **Behavior**: Runs for both normal load and kdmapper; wrapped in __try to avoid BSOD on unsupported builds
- **Clears**: MmUnloadedDrivers (vuln drivers), PiDDB placeholder, g_KernelHashBucketList placeholder

### 8. MmLastUnloadedDriver checksum
- **trace_cleaner.hpp**: Comment added – some ACs verify; add checksum fix if needed per target

### 9. LargePageDrivers for codecave
- **Setup**: `scripts/setup_largepage_drivers.ps1`, `scripts/README_LargePageDrivers.md`
- **Config**: `FLUSHCOMM_USE_CODECAVE 1`, `FLUSHCOMM_SIGNED_CODECAVE_ONLY 1`
- **Flow**: When beep.sys in LargePageDrivers + reboot → PING runs from signed Beep .data

## Research / Placeholder

### PFN zeroing (physical page contents)
- **trace_cleaner.hpp**: `pfn_zero_pages()` zeros physical pages of unmapped vuln drivers before clearing MmUnloadedDrivers. Config: `FLUSHCOMM_PFN_ZEROING 1` (default)

### Big pool spoof
- **PFN_AND_BIGPOOL_RESEARCH.md**: Big pool table modification; high risk, structure varies

### ICALL-GADGET (optional)
- **icall_gadget.hpp/cpp**: Framework for redirecting frw/fba via ntoskrnl gadget; `FLUSHCOMM_USE_ICALL_GADGET 0` (default)
- **Combines with LargePageDrivers**: PING runs from signed Beep; ICALL-GADGET for other hot paths when NMI is a vector

### Custom vuln driver
- **CUSTOM_VULN_DRIVER_RESEARCH.md**: Structure for third driver path; requires signed private driver

## Target Name Auto-Match

- **mapper_config.props**: Single source – edit `MapperOutputName` (kdmapper or IntelCpHDCP).
- **Build**: kdmapper outputs `$(MapperOutputName).exe`; loader uses same via generated `mapper_output.h`.
- **Directory.Build.targets**: Generates `mapper_output.h` before compile.

## Config Quick Reference

| Define | Default | Purpose |
|--------|---------|---------|
| MapperOutputName (props) | kdmapper | Mapper output name – single source |
| MAPPER_EXE_BENIGN_NAME | from props | Auto-generated; loader uses it |
| FLUSHCOMM_PATCH_ETW | 0 | ETW patch (detection risk) |
| FLUSHCOMM_TRACE_CLEANER | 1 | MmUnloadedDrivers cleanup |
| FLUSHCOMM_TRACE_CLEANER_WDFILTER | 0 | Wdfilter cleanup (high risk) |
| FLUSHCOMM_USE_CODECAVE | 1 | PING from signed Beep |
| FLUSHCOMM_SIGNED_CODECAVE_ONLY | 1 | LargePageDrivers only, no MDL |
| FLUSHCOMM_USE_ICALL_GADGET | 1 | Redirect frw/fba via ntoskrnl gadget for NMI evasion |
| FLUSHCOMM_PFN_ZEROING | 1 | Zero physical page contents of unloaded vuln drivers |
