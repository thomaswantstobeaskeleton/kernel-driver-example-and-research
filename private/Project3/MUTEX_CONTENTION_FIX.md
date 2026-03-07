# Mutex Contention Fix - Rendering Thread Priority

## Problem Identified

**Root Cause**: CacheLevels thread was blocking the rendering thread by holding `g_driver_mutex` during throttle sleeps.

### The Issue

1. **Every `read<>` call** locks `g_driver_mutex` for the entire read operation
2. **Throttle sleep (2-4ms)** was happening INSIDE the mutex lock (in `send_request()`)
3. **CacheLevels** does many reads in tight loops (levels → actors → items)
4. **Rendering thread** (actorloop) blocks waiting for the mutex
5. **Result**: Overlay FPS drops to ~10 FPS even though Fortnite runs fine

### Why Menu Works

When menu is open:
- CacheLevels sleeps (no mutex contention)
- actorloop does minimal reads (no mutex contention)
- **Result**: High FPS

### Why ESP Lags After Startup

Once ESP activates:
- CacheLevels starts doing many reads
- Each read holds mutex for 2-4ms (throttle) + driver processing
- Rendering thread blocks waiting for mutex
- **Result**: Overlay FPS crashes

## Solution Implemented

### 1. Move Throttle Outside Mutex for Reads

**Before**: Throttle sleep happened INSIDE mutex lock
```cpp
void read_physical(...) {
    lock_guard<mutex> lock(g_driver_mutex);  // Mutex acquired
    // ... throttle sleep happens here (2-4ms) ...
    send_request(...);  // Still holding mutex
}
```

**After**: Throttle sleep happens BEFORE mutex acquisition
```cpp
void read_physical(...) {
    // Throttle BEFORE mutex - releases mutex between reads
    if (needs_throttle) Sleep(...);
    
    lock_guard<mutex> lock(g_driver_mutex);  // Mutex acquired
    send_request(...);  // Quick operation, mutex released soon
}
```

**Impact**: 
- CacheLevels releases mutex between reads
- Rendering thread can acquire mutex between CacheLevels reads
- **Mutex contention eliminated**

### 2. Increased Yields in CacheLevels

**Before**: `Sleep(3)` every 2-4 actors
**After**: `Sleep(5)` every 2 actors

**Impact**: More frequent mutex releases, better rendering thread access

### 3. Kept Throttle for Writes/Other Ops

Writes and other operations (mouse, CR3, etc.) are infrequent, so they keep throttling in `send_request()` to maintain anti-detection.

## Expected Results

### Before Fix
- CacheLevels holds mutex for 2-4ms per read
- 50 reads in batch = 100-200ms mutex hold time
- Rendering thread blocks → overlay FPS drops to ~10

### After Fix
- CacheLevels releases mutex between reads
- Rendering thread gets mutex access every ~2-4ms
- Overlay FPS should improve significantly (target: 60+ FPS)

## Files Modified

1. **`utilities/impl/driver.hpp`**
   - Moved throttle from `send_request()` to `read_physical()` BEFORE mutex
   - Kept throttle in `send_request()` for writes/other ops

2. **`utilities/sdk/cache/actorloop.h`**
   - Increased yield sleeps in CacheLevels (3ms → 5ms)
   - Changed yield frequency (every 4 actors → every 2 actors)

## Testing

1. **Monitor overlay FPS** after ESP appears (should be 60+ FPS)
2. **Check ESP accuracy** (should be unchanged)
3. **Verify CacheLevels** still works (items/chests still appear)
4. **Test menu responsiveness** (should remain smooth)

## Technical Details

### Mutex Behavior
- `std::mutex` is fair/FIFO - priority doesn't help
- Solution: Release mutex more frequently (between reads)
- Throttle BEFORE mutex = mutex held for shorter periods

### Thread Priorities
- CacheLevels: `THREAD_PRIORITY_LOWEST` (already set)
- Rendering thread: Normal priority
- **Note**: Mutex fairness means priority doesn't help - we need to release mutex more often

### Anti-Detection
- Throttle still applied (before mutex for reads, in send_request for writes)
- Same throttle timing (2-4ms) maintained
- No detection risk increase
