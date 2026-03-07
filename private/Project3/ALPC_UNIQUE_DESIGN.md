# ALPC Fallback – Unique Design (Not from Public Implementations)

This describes a **custom** driver↔usermode path used only when the primary FlushComm (section + FlushFileBuffers) fails. The design avoids patterns from public ALPC examples and cheat tutorials.

---

## 1. Design Principles

- **Signal-only ALPC**: ALPC is used only to signal “request ready” and “response done”. No request/response payload in ALPC messages; no ALPC port sections.
- **Data stays in the existing section**: Same named section as FlushComm (e.g. `Global\WdfCtl_xxx`). Request (magic, type, args) and response (status, data) live only in that section. No second shared memory mechanism.
- **Port name is build-unique**: Port name is derived from `FLUSHCOMM_MAGIC` (or build suffix), not a fixed string. No literal like `"CheatPort"` or `"WdfCtl"` in the ALPC path. Format is `\RPC Control\` + a short token derived from the magic/suffix so it differs per build and is not searchable as a known name.
- **Minimal message payload**: Message carries at most a single byte (e.g. type or ack). All real data is read/written from the section by both sides. This differs from “send full request in PORT_MESSAGE” examples.
- **Single client**: Driver accepts one client and uses one connection port for the lifetime of the driver. No multi-client or named-pipe-style patterns.

---

## 2. Flow (Unique to This Project)

1. **Driver init**  
   Create the named section (unchanged). If ALPC fallback is enabled, create an ALPC server port with the unique name and start a **single** system thread that:
   - Accepts one connection (e.g. `ZwAlpcAcceptConnectPort`).
   - Loops: wait for a message (e.g. `ZwAlpcSendWaitReceivePort`), call **FlushComm_ProcessSectionBuffer()** (process request from `g_section_view`), then send a reply (e.g. reply API or send back a minimal message).

2. **Usermode**  
   - Try primary: open section, open hooked device (Beep/Null/PEAuth), handshake via FlushFileBuffers.  
   - If that fails (e.g. no device, handshake timeout): **ALPC fallback**  
     - Open the **same** section by name (already known from build/config).  
     - Connect to the ALPC port (name = same unique derivation as driver).  
     - Handshake: write REQ_INIT + magic to section, send one minimal ALPC message, wait for reply.  
     - If handshake succeeds, mark “use ALPC” and use ALPC for all later requests.

3. **Per-request (ALPC path)**  
   - Usermode: write magic + type + args to section; send one minimal ALPC message; wait for reply.  
   - Driver: on receive, call `FlushComm_ProcessSectionBuffer()` (reads type/args from section, writes status/data to section), send reply.  
   - Usermode: read status and data from section after reply.

---

## 3. What We Do *Not* Do (Avoid Public Patterns)

- No “ALPC port section” for data; no passing section handles in connection attributes for the main data path (we use the existing named section only).
- No “send full request/response in ALPC message”; no large or structured payloads in PORT_MESSAGE.
- No fixed, human-readable port name; no names that appear in public tutorials or paste sites.
- No reuse of any published ALPC cheat or kernel-communication code; structures and flow are defined only for this project.

---

## 4. Detection / Ops Notes

- ALPC is normal Windows IPC; using it only for a small signal and reusing the same section as FlushComm keeps the data path identical and avoids a second, custom shared-memory mechanism.
- Port name and message size are kept minimal to reduce distinctiveness.
- This path is **fallback only**; primary remains section + FlushFileBuffers.

---

## 5. Implementation Notes

- **FlushComm_ProcessSectionBuffer()**: Implemented in the driver. Same logic as the section branch of `FlushComm_ProcessSharedBuffer()`, but takes no IRP; only reads/writes `g_section_view` and returns. Used by the FlushFileBuffers handler; ready for use by an ALPC worker thread when the kernel ALPC server is added.
- **Kernel ALPC server** (optional, not shipped by default): Would resolve `ZwAlpcCreatePort`, `ZwAlpcAcceptConnectPort`, `ZwAlpcSendWaitReceivePort` via `MmGetSystemRoutineAddress` from ntoskrnl; create a system thread that accepts one client and loops: wait receive → `FlushComm_ProcessSectionBuffer()` → reply. Port name must match usermode (derived from `FLUSHCOMM_MAGIC`).
- **Usermode ALPC fallback**: Implemented when `FLUSHCOMM_USE_ALPC_FALLBACK` is 1. Resolves `NtAlpcConnectPort` and `NtAlpcSendWaitReceivePort` at runtime; port name is `\RPC Control\Svc` + 8 hex digits of `(FLUSHCOMM_MAGIC & 0xFFFFFFFF)`. If the driver does not create the ALPC port, connect fails and the process keeps using only the primary FlushComm path (or exits if that also failed).
