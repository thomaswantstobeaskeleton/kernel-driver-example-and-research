# File-Backed Section IPC – Design (No Public Implementation)

Alternative to named sections for driver↔usermode shared memory. Uses a **file on disk** as backing instead of `CreateFileMappingW` / `OpenFileMappingW` with a name. No object in `\BaseNamedObjects\Global\*`.

---

## 1. Design principles

- **No named section object**: Nothing in `\BaseNamedObjects\Global\*`; avoids scans for known section names.
- **File-backed mapping**: Both sides map the same file. Usermode: `CreateFile` → `CreateFileMapping` (with file handle, no name). Driver: `ZwOpenFile` → `ZwCreateSection` (with file handle).
- **Path is build-unique**: `%SystemRoot%\Temp\Fx` + 12 hex chars of `(FLUSHCOMM_MAGIC & 0xFFFFFFFFFFFF)` + `.tmp`. Different per build.
- **Fallback**: If file-backed fails, falls back to named section (inverted flow: usermode Create, driver ZwOpenSection).

---

## 2. Flow

1. **Usermode (before LoadDriver)**  
   - `GetWindowsDirectoryW` → e.g. `C:\Windows`  
   - Path = `C:\Windows\Temp\Fx<12hex>.tmp`  
   - `CreateFileW(path, OPEN_ALWAYS)` → create or open file  
   - `CreateFileMappingW(hFile, NULL, PAGE_READWRITE, 0, 512, NULL)` → unnamed mapping (file-backed)  
   - `MapViewOfFile` → shared buffer  
   - Handshake fails (driver not loaded).

2. **LoadDriver**  
   Driver loads and runs `FlushComm_Init`.

3. **Driver init**  
   - Read `SystemRoot` from `HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion`  
   - Path = `%SystemRoot%\Temp\Fx<12hex>.tmp`  
   - `ZwOpenFile(\??\C:\Windows\Temp\Fx....tmp)` → file exists from usermode  
   - `ZwCreateSection(&hSection, ..., hFile)` → section backed by file  
   - Map section into System or caller process (same logic as named section).

4. **Usermode (after LoadDriver)**  
   - Reuses existing handle and view (or remaps).  
   - Handshake succeeds.

---

## 3. Differences from public approaches

| Aspect              | Named section (common)     | This file-backed design                |
|---------------------|---------------------------|----------------------------------------|
| Object location     | `\BaseNamedObjects\Global\*` | N/A – no named object              |
| Backing             | Page file                 | File on disk                          |
| Creation            | CreateFileMappingW(name)  | CreateFileMappingW(hFile, NULL)       |
| Path discovery      | Section name              | `%SystemRoot%\Temp\Fx<magic>.tmp`    |
| Known cheat usage   | Common                    | Uncommon for driver IPC               |

---

## 4. Detection considerations

- File creation in `%SystemRoot%\Temp` is normal; many processes use it.
- File extension `.tmp` matches typical temp usage.
- No scan of `\BaseNamedObjects\` for this IPC path.
- Path is per-build and not a fixed string.

---

## 5. Config

```c
#define FLUSHCOMM_USE_FILEBACKED_SECTION  1   /* Try file-backed first; fall back to named section */
```

Set to `0` to disable and use only named sections.

---

## 6. Not from public sources

Design is project-specific. Not taken from:

- `hugsy/shared-kernel-user-section-driver`
- `Deputation/kernel_payload_comms`
- pTerrance/alpc-km-um
- Any other public cheat/kernel IPC examples

File-backed shared memory is standard Windows usage, but the flow and path choice are tailored to this project.
