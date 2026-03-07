# FPS/ESP Lag – Deep Research: Driver Communication Bottleneck

## Executive Summary

**Root cause identified:** `FLUSHCOMM_THROTTLE_MS` (2ms) + `FLUSHCOMM_JITTER_MS` (2ms) in `flush_comm_config.h` adds **2–4ms minimum delay between every driver request**. With 500–2500+ reads per frame (actorloop + CacheLevels), this limits throughput to ~250–500 requests/second → **0.2–2 FPS**.

## Mitigations Implemented (throttle kept at 2/2)

1. **REQ_READ_BATCH** – Driver reads up to 35 (addr, size) pairs and returns concatenated results in one request.
2. **get_camera** – Uses 2 batch requests instead of 5 individual reads; also fetches UWorld Seconds for IsEnemyVisible.
3. **Camera caching** – `camera_postion` set once per frame; ProjectWorldToScreen uses cached (saves ~700 reads).
4. **Player array batch** – PlayerState + PawnPrivate for 35 players in 2 batch requests instead of 70 individual reads.
5. **GetBoneLocation** – Bone + ComponentToWorld in 1 batch after bone_array lookup.
6. **IsEnemyVisible** – Uses cached UWorld Seconds from get_camera; only reads mesh+0x328 per call.

---

## Architecture Overview

```
┌─────────────────┐     send_request()      ┌──────────────────┐
│  actorloop      │ ──────────────────────► │  FlushFileBuffers │
│  CacheLevels    │   (shared section)       │  → IRP_MJ_FLUSH   │
│  (read<> calls) │ ◄────────────────────  │  → frw()          │
└─────────────────┘     g_shared_buf        └──────────────────┘
        │                        │                    │
        │    g_driver_mutex      │                    │
        └────────────────────────┴────────────────────┘
                    Serialized per process
```

- **actorloop**: Render thread, ~35 players × 50–100 reads/player = 1750–3500 reads/frame
- **CacheLevels**: Background thread, hundreds of reads per iteration
- **Both** use `DotMem::read_physical()` → `send_request()` → driver
- **Throttle** in `send_request()`: `Sleep(minGap)` when `now - g_last_send_tick < 2–4ms`

---

## The Throttle (flush_comm_config.h)

```c
#define FLUSHCOMM_THROTTLE_MS    2   // min ms between send_request
#define FLUSHCOMM_JITTER_MS      2   // +0..2ms random
```

**Effect:** Every `read<>` waits 2–4ms before sending.  
**Math:** 1000 reads × 3ms avg = **3 seconds per frame** → 0.33 FPS.

---

## Read Volume (actorloop.h)

| Component        | Reads/frame (approx) |
|-----------------|----------------------|
| Cache refresh   | ~15                  |
| FOV (when open)| 1 (or 0 if menu)     |
| Per player     | 50–100 (bones, vis, etc.) |
| 35 players     | 1750–3500            |
| **Total**      | **~1800–3500**       |

At 2–4ms/read: **3.6–14 seconds per frame** → 0.07–0.28 FPS.

---

## Why Menu Open Works

When `menus.ShowMenu` is true:
- actorloop does **light path**: clear lists, render_elements (watermark + FOV), Present
- **No** cache refresh, **no** player loop → **~0 driver reads**
- CacheLevels sleeps 450–529ms
- **Result:** No throttle, no contention → high FPS

---

## Fix

**Disable throttle** for normal operation:

```c
#define FLUSHCOMM_THROTTLE_MS    0
#define FLUSHCOMM_JITTER_MS     0
```

**Trade-off:** Config comment says "EAC: vary per build" for anti-detection. If the target AC detects rapid request patterns, you can set 1/1 as a compromise (still ~500 req/s vs 250 with 2/2).

---

## Other Findings

1. **Section mode** (FLUSHCOMM_USE_SECTION=1): No MmCopyVirtualMemory; driver writes directly to shared section. Low overhead.
2. **Mutex** (`g_driver_mutex`): Serializes actorloop vs CacheLevels. Expected; not the bottleneck.
3. **CacheLevels** timing: Already tuned (550–699ms sleep, 10ms budget). Helps but throttle dominates.
4. **No batched read API**: Each `read<T>` is one round-trip. Batching would require driver/usermode changes.

---

## Files

| File | Role |
|------|------|
| `flush_comm_config.h` | **FLUSHCOMM_THROTTLE_MS, FLUSHCOMM_JITTER_MS** |
| `utilities/impl/driver.hpp` | `send_request()` applies throttle |
| `utilities/sdk/cache/actorloop.h` | read<> calls, player loop |
| `driver/driver.cpp` | FlushComm_ProcessSharedBuffer, frw() |
