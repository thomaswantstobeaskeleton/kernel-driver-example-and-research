# Custom Vulnerable Driver – Research & Structure

## Why Custom Vuln Driver

- **No public signatures**: Intel/eneio are well-known; ACs maintain blocklists.
- **Big pool spoof**: Custom driver can avoid predictable pool tags/sizes.
- **Mapper exe rename**: Works with any vuln driver; custom adds flexibility.
- **Long-term**: Private driver with unique IOCTL/device path is harder to fingerprint.

## Requirements

1. **Signed driver**: Must pass DSE (Driver Signature Enforcement). Options:
   - EV code signing certificate (costly).
   - Leaked/abused certificate (risky, revocable).
   - Windows test signing (not suitable for production).
2. **Exploitable IOCTL**: Physical read/write, map/unmap, or similar.
3. **No public disclosure**: Keep driver private; no upload to VirusTotal, GitHub, etc.

## Structure for Third Driver Path

To add a custom driver alongside Intel/eneio:

### 1. Driver Backend Interface

```cpp
// driver_backend.cpp
if (driverName == "custom") {
    return custom_driver::Map(driverPath, ...);
}
```

### 2. custom_driver.cpp (placeholder)

- `Load()`: Copy driver to temp, register service, start.
- `Unload()`: Stop service, delete file.
- `ReadMemory`/`WriteMemory`: Via custom IOCTL.
- `MapIoSpace`/`UnmapIoSpace`: Or equivalent for manual mapping.
- `ClearMmUnloadedDrivers`/`ClearPiDDBCacheTable`: Same as Intel.

### 3. Obfuscation

- Custom device path (GUID or benign-looking).
- XOR/encrypted IOCTL codes.
- Random service/driver name per run.

### 4. includes.h / LoadDriver

- Add `"custom"` to drivers array when custom driver available.
- `--driver custom` support in mapper.

## Implementation Effort

- **High**: New driver development, signing, testing.
- **Medium**: Integration with kdmapper (backend interface exists).
- **Risk**: Certificate revocation if driver is leaked.

## Status

- **Placeholder only** – no custom driver implementation.
- Use Intel/eneio for now; add custom when private signed driver is available.
