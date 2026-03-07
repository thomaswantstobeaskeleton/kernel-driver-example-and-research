# FPS 1-6 and ESP/Menu Flicker – Research & Fix

## Symptoms

- FPS display always shows 1–6 regardless of actual performance  
- ESP and menu flicker  
- “Something else was taking priority”

---

## Root Cause 1: CacheLevels Monopolizing the Driver

**Mechanism:** `CacheLevels()` runs in a detached thread and performs hundreds of driver reads per iteration (level/actor iteration). Both the render loop (`actorloop`) and CacheLevels use `DotMem::read_physical()` → `send_request()` → kernel driver. Driver handling is serialized per process.

**Result:** CacheLevels consumed most of the driver capacity, so `actorloop` read slowly → ~1–6 FPS and erratic ESP updates.

**Fixes applied:**
1. `SetThreadPriority(THREAD_PRIORITY_BELOW_NORMAL)` → `THREAD_PRIORITY_LOWEST` for CacheLevels thread  
2. `SetThreadPriority(THREAD_PRIORITY_ABOVE_NORMAL)` for render thread (menu/ESP)  
3. `ITERATION_MS_BUDGET` reduced from 60 ms → 35 ms → 18 ms (~1 frame @ 60fps)  
4. `Sleep(1)` → `Sleep(2)` in level loop (every 2 levels) and actor loop (every 4 actors) – stronger yield  
5. Inter-iteration `Sleep` increased from 200–299 ms to 350–449 ms  
6. **Menu pause:** when `menus.ShowMenu` is true, CacheLevels skips work entirely (Sleep 450–529 ms) – no driver contention when menu visible  

---

## Root Cause 2: DeltaTime Not Set for All Overlay Modes

**Mechanism:** DeltaTime (and thus `ImGui::GetIO().Framerate`) was only updated when using the own overlay. For Discord/CrosshairX, it depended on `ImGui_ImplWin32_NewFrame()`, which could be wrong if the hijacked window’s timing differed.

**Result:** Incorrect or stale DeltaTime → wrong FPS display.

**Fix:** DeltaTime is now computed every frame with `QueryPerformanceCounter` for all overlay types, and clamped to 1/60–10 s for stability.

---

## ESP Flicker

Flicker was largely a side effect of driver contention: slow or inconsistent reads produced unstable ESP data. Lowering CacheLevels’ priority and reducing its driver usage improves ESP stability.

---

## Files Modified

- `utilities/sdk/cache/actorloop.h` – CacheLevels priority and timing changes  
- `utilities/overlay/render.h` – Unified DeltaTime calculation  
