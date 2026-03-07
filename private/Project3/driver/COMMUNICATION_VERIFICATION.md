# FlushComm Communication Verification

Verified flow for driver ↔ usermode communication with current config.

## Config Summary (flush_comm_config.h)

| Setting | Value | Effect |
|---------|-------|--------|
| FLUSHCOMM_USE_SECTION | 1 | Section-based shared memory (no MmCopyVirtualMemory) |
| FLUSHCOMM_USE_FLUSH_BUFFERS | 1 | FlushFileBuffers triggers requests (not DeviceIoControl for REQ_*) |
| FLUSHCOMM_USE_DIRECT_SYSCALL | 1 | NtDeviceIoControlFile for PING (bypasses ntdll hooks) |
| FLUSHCOMM_USE_LAZY_IMPORT | 1 | APIs resolved via GetProcAddress at runtime |
| FLUSHCOMM_PATCH_ETW | 0 | Disabled by default; some ACs detect ETW patches. Set 1 to enable. |

## Startup Order

1. **main()** → `EtwPatch::Init()` (patch ETW)
2. **main()** → `DotMem::find_driver()`
3. **find_driver()** → `LazyImport::Init()` (resolve CreateFileW, Reg*, etc.)
4. **find_driver()** → `DRV_OpenFileMappingW(L"Global\FlushComm_22631")` (section mode)
5. If section exists (driver loaded) → `DRV_MapViewOfFile` → `g_shared_buf`, `g_section_handle` set
6. If section missing → `VirtualAlloc` + `write_registry` (fallback path)
7. **read_hooked_device_index()** → reads registry for Beep/Null/PEAuth index
8. **open_hooked_device(idx)** → `DRV_CreateFileW(L"\\\\.\\Beep")` (or Null/PEAuth)
9. **PING** → `sys_NtDeviceIoControlFile(IOCTL_REXCOMM_PING)` or `DRV_DeviceIoControl`
10. On success → `driver_handle` valid, return true

## Request Flow (Section Mode)

1. Usermode writes to `g_shared_buf` (mapped section):
   - [0:8] FLUSHCOMM_MAGIC
   - [8:12] REQUEST_TYPE
   - [16:80] args (64 bytes)
   - [88:] data area (driver writes results here)
2. `DRV_FlushFileBuffers(driver_handle)` → IRP_MJ_FLUSH_BUFFERS
3. Driver reads from `g_section_view` (same backing memory)
4. Driver processes, writes result to `base + FLUSHCOMM_DATA_OFFSET` (88)
5. Usermode reads from `g_shared_buf + SECTION_DATA_OFFSET` (88)

## Layout Alignment

| Offset | Driver (flush_comm.hpp) | Usermode (driver.hpp) |
|--------|-------------------------|------------------------|
| 0–8 | magic | magic |
| 8–16 | type (ULONG) + pad | type (ULONG) |
| 16–80 | args | args (64 bytes) |
| 80–88 | status | (section mode: no explicit status at 64) |
| 88+ | FLUSHCOMM_DATA_OFFSET | SECTION_DATA_OFFSET |

Driver uses `(REQUEST_TYPE)*(ULONG*)(base+8)`, `argsPtr = base+16`, `pStatus = base+80`, `dataArea = base+88`. ✓

## Section Name

- **Driver**: `\BaseNamedObjects\FlushComm_22631`
- **Usermode**: `Global\FlushComm_22631` (opens global namespace object)

## Lazy Import APIs Used

- CreateFileW, DeviceIoControl, RegCreateKeyExW, RegSetValueExW, RegQueryValueExW, RegOpenKeyExW, RegCloseKey
- OpenFileMappingW, MapViewOfFile, UnmapViewOfFile, FlushFileBuffers, CloseHandle

`LazyImport::Init()` must run before any `DRV_*` call; it is the first step in `find_driver()`.

## KDMapper Compatibility

✅ kdmapper loads driver → DriverEntry → FlushComm_Init → creates section, hooks Beep. Usermode opens `\\.\Beep` (not our driver device) and the section. No IOCTL to our driver's own device.
