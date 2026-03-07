# ICALL-GADGET Research

## Overview

**ICALL-GADGET** is a technique to evade NMI-based stack walking detection by redirecting control flow through legitimate kernel modules. Anti-cheats use Non-Maskable Interrupts (NMIs) to capture the call stack; if any return address points outside valid signed modules, it indicates unsigned/mapped driver execution.

## How It Works

1. **Indirect call gadgets**: Find `call rax`/`call [rax]`-style instructions in signed kernel modules (ntoskrnl, hal, etc.).
2. **Redirect flow**: Instead of calling our handler directly, set up a chain: our code → gadget in signed module → our real logic.
3. **Stack appearance**: When NMI hits, the stack shows return addresses only within signed modules.

## Implementation Approaches

### 1. Gadget Discovery

- Scan ntoskrnl/hal for `FF D0` (call rax), `FF 10` (call [rax]), `41 FF Dx` variants.
- Validate gadget is in executable section of signed module.
- Build a table of usable gadgets per Windows build.

### 2. Trampoline Setup

- Allocate small stub in our driver that:
  - Sets `rax` (or target register) to our real handler.
  - Jumps to gadget in signed module.
- The gadget then calls our handler; return address on stack = gadget address (valid).

### 3. Integration with FlushComm

- PING handler or read/write path could be invoked via ICALL-GADGET instead of direct call.
- Requires: find gadget at runtime, patch our stub with gadget address, redirect IRP handler through stub.

## References

- **nolanpierce/ICALL-GADGET** (GitHub): Exploit for redirecting control flow through legit kernel module.
- **Lukas Trakimas**: "Thread and Call Stack Spoofing for NMI Callbacks on Windows" – HalPreprocessNmi hook alternative.
- **Elastic EDR evasion**: Call gadgets in ntdll to break call stack signatures.

## Status

- **Framework implemented** – `icall_gadget.hpp/cpp`; gadget discovery + `icall_invoke_2` (currently direct-call fallback).
- **Config**: `FLUSHCOMM_USE_ICALL_GADGET 0` – enable when NMI stack walking is a detection vector.
- **Combines with LargePageDrivers**: PING runs from signed Beep .data; ICALL-GADGET for frw/fba/etc.
- **Alternative**: FLUSHCOMM_USE_CODECAVE + LargePageDrivers already runs PING from signed Beep; RIP in valid module.
- **NMI spoof**: FLUSHCOMM_USE_NMI_SPOOF 0 – HalPreprocessNmi hook; some ACs detect HAL tampering.

## Recommendation

For maximum stealth: combine LargePageDrivers codecave (RIP in signed module) with optional ICALL-GADGET for other code paths if NMI stack walking becomes a detection vector.
