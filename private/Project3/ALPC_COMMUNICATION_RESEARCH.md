# ALPC Communication Research (2024-2025)

Research into ALPC (Advanced Local Procedure Call) as an alternative to IOCTL/FlushFileBuffers for kernel driver ↔ usermode communication.

---

## 1. ALPC Overview

**ALPC (Advanced Local Procedure Call)** is an undocumented Windows IPC mechanism introduced in Windows Vista, replacing the synchronous LPC protocol. It enables high-speed, asynchronous message communication between processes on the same machine.

### Key APIs

| API | Purpose | Location |
|-----|---------|----------|
| `NtAlpcCreatePort` | Create ALPC server port | ntdll.dll (undocumented) |
| `NtAlpcConnectPort` | Connect client to ALPC port | ntdll.dll (undocumented) |
| `NtAlpcSendWaitReceivePort` | Send/receive messages | ntdll.dll (undocumented) |
| `NtAlpcCreatePortSection` | Create shared memory section | ntdll.dll (undocumented) |
| `NtAlpcCreateResourceReserve` | Reserve resources | ntdll.dll (undocumented) |

**Kernel-side**: ALPC operations are handled by `ntoskrnl.exe` internal functions (`Alpcp*`). Drivers can create ALPC ports via `ZwAlpcCreatePort` (kernel-mode wrapper).

---

## 2. Detection Status (2024-2025 Research)

### 2.1 Anti-Cheat Detection

**UnknownCheats Research**: No specific mentions of ALPC being detected by EAC/BattlEye in recent discussions (2024-2025). However, this doesn't mean it's undetected - it may simply be less commonly used.

**Key Finding**: ALPC is used extensively by **Windows itself** and by **antivirus/EDR systems**. This means:
- ✅ ALPC traffic is **legitimate** Windows behavior (not inherently suspicious)
- ⚠️ ACs may monitor ALPC connections for **abnormal patterns** (unusual port names, connection timing, message sizes)
- ⚠️ ACs may detect **kernel-level ALPC manipulation** (modifying ALPC structures)

### 2.2 Kernel-Level Attacks (2024 Research)

**ALPChecker Research (2024)**:
- Demonstrated **three kernel-level attacks** on ALPC:
  1. **Spoofing**: Modify ALPC structures to create illegitimate connections
  2. **Blinding**: Disrupt legitimate ALPC connections without closing them
  3. **Kernel memory manipulation**: Modify ALPC port structures directly

**Implication**: If we use ALPC, we're using a mechanism that **can be attacked from kernel level**. However, we're **not attacking** ALPC - we're using it legitimately. The research shows ACs/EDRs can be **targeted via ALPC**, not that using ALPC is detected.

### 2.3 Recent Vulnerabilities

| CVE | Year | Description | Impact |
|-----|------|-------------|--------|
| **CVE-2025-64721** | 2025 | Sandbox escape via ALPC heap manipulation | Sandboxie |
| **CVE-2022-38029** | 2022 | Use-after-free in ALPC port creation | Race condition |
| **CVE-2023-21674** | 2023 | Chrome sandbox escape via ALPC | In-the-wild exploit |

**Note**: These CVEs exploit **vulnerabilities in ALPC implementation**, not detection of ALPC usage itself.

---

## 3. Comparison: ALPC vs Current Approach

### 3.1 Current Approach (FlushFileBuffers + Shared Section)

| Aspect | Current Method |
|--------|----------------|
| **Mechanism** | `FlushFileBuffers()` → `IRP_MJ_FLUSH_BUFFERS` |
| **Shared Memory** | Named section (`WdfCtl_xxx`) mapped in both address spaces |
| **Signaling** | FlushFileBuffers triggers driver IRP handler |
| **Detection Risk** | Low - FlushFileBuffers is legitimate file I/O operation |
| **IOCTL Usage** | None (FLUSHCOMM_USE_FLUSH_BUFFERS=1) |
| **Device Handle** | Still need handle to `\\.\Beep` (or Null/PEAuth) |
| **Complexity** | Medium - section creation, FlushFileBuffers IRP hook |

**Pros**:
- ✅ No IOCTL (avoided detected vector)
- ✅ Uses legitimate Windows file I/O mechanism
- ✅ Shared section avoids `MmCopyVirtualMemory` (EAC traces it)
- ✅ Already implemented and working

**Cons**:
- ⚠️ Still requires device handle (`\\.\Beep` etc.)
- ⚠️ FlushFileBuffers is file-system focused (not IPC-focused)
- ⚠️ Requires hooking IRP_MJ_FLUSH_BUFFERS on existing device

### 3.2 ALPC Approach

| Aspect | ALPC Method |
|--------|-------------|
| **Mechanism** | `NtAlpcCreatePort` (driver) + `NtAlpcConnectPort` (usermode) |
| **Shared Memory** | ALPC port sections (built-in ALPC feature) |
| **Signaling** | `NtAlpcSendWaitReceivePort` messages |
| **Detection Risk** | Unknown - ALPC is legitimate but less common for cheats |
| **IOCTL Usage** | None |
| **Device Handle** | Not needed - ALPC ports are independent |
| **Complexity** | High - undocumented APIs, complex message structures |

**Pros**:
- ✅ No IOCTL
- ✅ No device handle needed (completely independent)
- ✅ Legitimate Windows IPC mechanism (used by Windows/AV/EDR)
- ✅ Built-in shared memory support
- ✅ Asynchronous messaging (potentially faster)

**Cons**:
- ⚠️ **Undocumented APIs** - requires reverse engineering or research
- ⚠️ **Complex implementation** - ALPC message structures are complex
- ⚠️ **Unknown detection status** - no clear evidence it's undetected
- ⚠️ **Kernel-side complexity** - driver must create/manage ALPC port
- ⚠️ **Port name** - must choose benign-looking port name (ACs may scan)
- ⚠️ **Message structure** - ACs may detect unusual message patterns

---

## 4. Detection Vectors for ALPC

### 4.1 Potential Detection Methods

| Vector | Description | Mitigation |
|--------|-------------|------------|
| **Port name scanning** | ACs enumerate ALPC ports, detect suspicious names | Use benign port name (e.g., `\\RPC Control\\WdfCtl_xxx`) |
| **Connection timing** | Instant connection after driver load | Add random delay before connect |
| **Message patterns** | Unusual message sizes/structures | Match Windows ALPC message format |
| **Kernel callbacks** | ACs may hook `Alpcp*` functions | Use direct syscalls or kernel callbacks |
| **Port enumeration** | `NtQueryInformationProcess` / `NtQuerySystemInformation` | Hide port from enumeration (requires kernel manipulation) |

### 4.2 Comparison with Current Approach

| Detection Vector | FlushFileBuffers | ALPC |
|------------------|------------------|------|
| **IOCTL usage** | ✅ None | ✅ None |
| **Device handle** | ⚠️ Required (`\\.\Beep`) | ✅ Not needed |
| **Port/name visibility** | ✅ No port (uses file handle) | ⚠️ Port name visible |
| **Message structure** | ✅ Simple shared buffer | ⚠️ Complex ALPC messages |
| **Kernel hooks** | ⚠️ IRP hook on Beep | ⚠️ ALPC port creation |
| **Legitimacy** | ✅ File I/O (common) | ✅ IPC (common, but less for cheats) |

---

## 5. Implementation Complexity

### 5.1 ALPC Implementation Requirements

**Usermode**:
1. Resolve `NtAlpcConnectPort`, `NtAlpcSendWaitReceivePort` (undocumented)
2. Build ALPC connection attributes (`ALPC_PORT_ATTRIBUTES`)
3. Build ALPC message structures (`PORT_MESSAGE` header + data)
4. Handle ALPC port sections for shared memory
5. Implement message send/receive loop

**Kernel**:
1. Resolve `ZwAlpcCreatePort` (kernel wrapper)
2. Create ALPC port with benign name
3. Handle `AlpcpAcceptConnectPort` for client connections
4. Process ALPC messages (`PORT_MESSAGE` structures)
5. Manage ALPC port sections
6. Handle port cleanup on unload

**Estimated LOC**: ~500-800 lines (usermode + kernel)

### 5.2 Current Implementation

**Usermode**: ~100 lines (section mapping + FlushFileBuffers)
**Kernel**: ~200 lines (IRP_MJ_FLUSH_BUFFERS handler + section management)

**Total**: ~300 lines

---

## 6. Recommendation

### 6.1 Current Status Assessment

**Current approach (FlushFileBuffers)**:
- ✅ **Already implemented** and working
- ✅ **No IOCTL** (meets requirement)
- ✅ **Low detection risk** (legitimate file I/O)
- ⚠️ Still requires device handle (minor vector)

**ALPC approach**:
- ❓ **Unknown detection status** (no clear evidence it's better)
- ⚠️ **High implementation complexity** (undocumented APIs)
- ⚠️ **Port name visibility** (new detection vector)
- ✅ **No device handle** (advantage)
- ✅ **No IOCTL** (meets requirement)

### 6.2 Recommendation: **Keep Current Approach**

**Reasons**:
1. **Current approach already avoids IOCTL** - meets the primary requirement
2. **Lower complexity** - FlushFileBuffers is simpler than ALPC
3. **Proven working** - already implemented and functional
4. **Lower risk** - file I/O is more common than custom ALPC ports
5. **ALPC detection unknown** - no evidence it's better, and adds complexity

### 6.3 When to Consider ALPC

Consider ALPC if:
- ✅ Current approach becomes detected (no evidence yet)
- ✅ Need to eliminate device handle requirement (currently minor risk)
- ✅ Willing to invest in complex implementation
- ✅ Can obfuscate port name and message patterns effectively

---

## 7. Alternative: Hybrid Approach

**Option**: Use ALPC for **initial connection** (no device handle), then switch to FlushFileBuffers for **data transfer** (simpler).

**Pros**:
- No device handle for connection
- Simpler data transfer (current FlushFileBuffers)
- Reduces ALPC complexity (only connection, not full IPC)

**Cons**:
- Still need device handle for FlushFileBuffers
- Adds complexity without clear benefit

**Verdict**: Not recommended - current approach is sufficient.

---

## 8. References

1. **ALPChecker Research (2024)**: Kernel-level ALPC attacks and detection
2. **DEF CON 32 (2024)**: "Defeating magic by magic: Using ALPC security features to compromise RPC services"
3. **CVE-2025-64721**: Sandbox escape via ALPC heap manipulation
4. **CVE-2022-38029**: Use-after-free in ALPC port creation
5. **UnknownCheats**: Kernel communication methods discussions (no ALPC-specific detection mentions)
6. **Offensive Windows IPC Internals 3: ALPC** (csandker.io, 2022)

---

## 9. Conclusion

**ALPC is a legitimate Windows IPC mechanism** that could theoretically replace IOCTL/FlushFileBuffers. However:

- ✅ **Current approach already avoids IOCTL** (primary goal met)
- ⚠️ **ALPC adds complexity** without proven detection benefit
- ⚠️ **Port name visibility** introduces new detection vector
- ✅ **FlushFileBuffers is simpler** and already working

**Recommendation**: **Keep current FlushFileBuffers approach**. ALPC could be implemented as a future option if current approach becomes detected, but there's no evidence that's necessary yet.
