# FPS/ESP Lag - Root Cause Analysis & Optimizations

## Problem Summary

After initial reads complete, ESP appears but is extremely laggy with FPS dropping to ~10 or lower. The overlay becomes unusable.

## Root Cause Identified

**Primary Issue: Excessive driver reads with throttling**

1. **Read Volume**: ~1800-3500 reads per frame (35 players × 50-100 reads/player)
2. **Throttle**: 2-4ms delay per read (`FLUSHCOMM_THROTTLE_MS` + `FLUSHCOMM_JITTER_MS`)
3. **Math**: 3500 reads × 3ms avg = **10.5 seconds per frame** → **0.095 FPS**

### Why Menu Works Fine

When menu is open:
- No cache refresh reads (~15 saved)
- No player loop reads (~1750-3500 saved)
- CacheLevels thread sleeps (hundreds of reads saved)
- **Result**: ~0 driver reads → no throttle → high FPS

### Why ESP Delays After Startup

1. **Initial read pause**: 10 seconds (reduced from 90)
2. **Startup ramp**: 2 seconds gradual ESP enable
3. **CacheLevels delay**: Starts 12 seconds after launch
4. **Once ESP activates**: Full read load hits → throttle bottleneck → FPS crashes

## Optimizations Implemented

### 1. Bone Array & ComponentToWorld Caching

**Problem**: `GetBoneLocation()` reads `bone_array` and `ComponentToWorld` every call, even though these rarely change per mesh.

**Solution**: Cache `bone_array` and `ComponentToWorld` per mesh address.

**Impact**: 
- Saves **2 reads per GetBoneLocation call**
- With 19 skeleton bones + 2 box bones = 21 calls/player
- **42 reads saved per player** × 35 players = **1,470 reads/frame saved**

**Code**: `utilities/sdk/render/functions.h`
- Added `cached_bone_arrays` and `cached_component_to_world` maps
- Cache cleared every 5 seconds to prevent memory growth

### 2. Eliminated Redundant Root Bone Read

**Problem**: Root bone (index 0) was read twice per player:
- Once as `base_bone` (line 943)
- Again as `root_bone` (line 962)

**Solution**: Reuse `base_bone` for `root_bone` since they're the same bone.

**Impact**: 
- Saves **3 reads per player** (1 bone_array + 1 ComponentToWorld + 1 bone transform)
- **105 reads/frame saved** (35 players)

**Code**: `utilities/sdk/cache/actorloop.h` line ~960

### 3. Early Exit Checks Before Expensive Reads

**Problem**: Expensive bone reads happened before checking if player is valid (dying, team check, etc.)

**Solution**: Moved early exit checks (`read<char>(CurrentActor + 0x758)`, `is_despawning`, `teamcheck`) before `GetBoneLocation()` calls.

**Impact**: 
- Skips bone reads for invalid players
- Saves **~6 reads per filtered player**
- With ~10-15 filtered players/frame = **60-90 reads/frame saved**

**Code**: `utilities/sdk/cache/actorloop.h` lines ~930-960

## Total Impact

| Optimization | Reads Saved/Frame | Cumulative |
|-------------|-------------------|------------|
| Bone caching | ~1,470 | 1,470 |
| Redundant root bone | ~105 | 1,575 |
| Early exits | ~75 | **~1,650** |

**Before**: ~3,500 reads/frame × 3ms = 10.5s/frame → 0.095 FPS  
**After**: ~1,850 reads/frame × 3ms = 5.55s/frame → **0.18 FPS**

**Improvement**: ~2x faster, but still throttled.

## Remaining Bottleneck

**The throttle is still the primary bottleneck:**

Even with optimizations, 1,850 reads × 3ms = 5.55 seconds per frame → 0.18 FPS.

### Options to Further Improve

1. **Reduce throttle** (if AC allows):
   - `FLUSHCOMM_THROTTLE_MS = 1` (was 2) → ~2x faster
   - `FLUSHCOMM_THROTTLE_MS = 0` → ~10x faster (risky for detection)

2. **Reduce player count**:
   - Already capped at 35, but could reduce to 20-25 during ramp-up

3. **Reduce skeleton detail**:
   - Currently 19 bones for full skeleton
   - Could reduce to 8-10 key bones

4. **Batch reads** (requires driver changes):
   - Read multiple addresses in one request
   - Would require implementing `REQ_READ_BATCH` again

## Files Modified

1. `utilities/sdk/render/functions.h`
   - Added bone_array and ComponentToWorld caching
   - Added cache clearing logic

2. `utilities/sdk/cache/actorloop.h`
   - Eliminated redundant root bone read
   - Moved early exit checks before expensive reads
   - Added cache clearing call

## Testing Recommendations

1. Monitor FPS after ESP appears (should be ~2x better)
2. Check memory usage (caches should clear every 5 seconds)
3. Verify ESP accuracy (caching shouldn't affect correctness)
4. Test with different player counts (10, 20, 35)

## Next Steps

If FPS is still too low:
1. Consider reducing `FLUSHCOMM_THROTTLE_MS` to 1ms
2. Reduce skeleton bone count during ramp-up
3. Implement frame-based read budgeting (max reads per frame)
