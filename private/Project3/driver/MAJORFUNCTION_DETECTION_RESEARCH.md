# MajorFunction[14] Detection Research

Research on the snippet: `DriverObject->MajorFunction[14] = (PDRIVER_DISPATCH)&sub_140005C10;`

---

## What is MajorFunction[14]?

- **IRP_MJ_DEVICE_CONTROL = 14** (0x0E)
- `MajorFunction[14]` is the IOCTL/DeviceIoControl handler for a driver
- When usermode calls `DeviceIoControl` or `NtDeviceIoControlFile`, the I/O manager dispatches to this function

---

## What Your Snippet Could Be

Without knowing the **binary** (which .sys or .exe), it could be:

| Context | Meaning | Detection risk |
|---------|---------|----------------|
| **EAC's DriverEntry** | EAC setting its own IOCTL handler – normal driver init | None – expected |
| **Cheat driver init** | Your driver registering its handler | Normal – your DriverObject |
| **Detection/scan code** | Unlikely – detection usually **reads** and compares, doesn't **write** | N/A |

The address `sub_140005C10` (IDA style, base 0x140000000) suggests a 64-bit PE. To interpret it you need: which module this is from (EasyAntiCheat.sys, your driver, etc.).

---

## How EAC/BE Detect MajorFunction Hooks

**Source:** secret.club – "Bypassing kernel function pointer integrity checks" (vmcall, 2019)

EAC and BattlEye use **blind integrity checks** on the MajorFunction table:

```c
// EAC/BE pseudo-code
for (each major_function in driver->MajorFunction) {
    if (major_function < driver_text_start || major_function >= driver_text_end) {
        // ANOMALY – pointer outside driver's .text section
        // Likely hooked – flag/ban
    }
}
```

**Detection logic:**
- Each MajorFunction entry must point **inside** the driver's executable (.text) section
- If you replace `MajorFunction[IRP_MJ_DEVICE_CONTROL]` with your hook (in pool or mapped driver), the pointer points **outside** the original driver's .text → **detected**
- This check is easy to implement and commonly used

---

## Is Direct MajorFunction[14] Hook Undetected?

**No.** Directly overwriting Beep’s (or any target driver’s) `MajorFunction[14]` with your handler is detectable because:

1. Your handler lives in pool/mapped memory or another module
2. EAC scans Beep’s MajorFunction – your pointer is outside Beep’s .text
3. Integrity check flags it

---

## Evasion: FILE_OBJECT Redirect (What You Use)

**Source:** Spectre rootkit wiki, D4stiny – "Hooking IOCTL Communication via Hooking File Objects"

Instead of modifying the **target** driver’s MajorFunction:

1. Create your **own** fake device + driver object
2. Hook **IRP_MJ_CREATE** on the target (e.g. Beep) – only to redirect `FILE_OBJECT->DeviceObject`
3. On create success, replace `FileObject->DeviceObject` with your fake device
4. Set your handlers on **your** DriverObject’s MajorFunction

**Result:**
- Beep’s `MajorFunction[14]` (IRP_MJ_DEVICE_CONTROL) stays **unchanged**
- EAC scanning Beep sees original pointers → **passes** integrity check
- IOCTLs go to your fake device because the FILE_OBJECT points there

---

## Your Current Setup

With `FLUSHCOMM_USE_FILEOBJ_HOOK 1`:

- You hook only `IRP_MJ_CREATE` (index 0) on Beep
- You do **not** touch `MajorFunction[14]` on Beep
- IRP_MJ_DEVICE_CONTROL and IRP_MJ_FLUSH_BUFFERS on Beep remain original
- All IOCTL/Flush traffic is redirected via `FILE_OBJECT->DeviceObject` swap

So your design is aligned with evasion: MajorFunction integrity checks on Beep should not see any tampering.

---

## Summary

| Approach | EAC/BE detection |
|----------|------------------|
| Direct `MajorFunction[14]` hook on Beep | **Detected** – pointer outside .text |
| FILE_OBJECT redirect (your method) | **Not detected** by MajorFunction integrity – Beep’s table unchanged |

Your snippet is likely normal driver init (EAC or another component). The important point: **do not** use direct MajorFunction[14] hooks on target drivers; use FILE_OBJECT redirect instead (which you already do).
