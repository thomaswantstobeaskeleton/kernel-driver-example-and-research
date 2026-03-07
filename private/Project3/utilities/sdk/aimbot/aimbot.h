#include <Windows.h>
#include <cstdint>
#include <iostream>
#include <Psapi.h>
#include "../utils/settings.h"
#include "../impl/driver.hpp"


uintptr_t Memoryaim = 0x528 + 0x8;
bool memory_event(fvector newpos)
{
    write<fvector>(CachePointers.PlayerController + Memoryaim, newpos);
    return true;
}


namespace memoryk
{
    uintptr_t NetConnection = 0x528;
    uintptr_t RotationInput = NetConnection + 0x8;
    //UNetDriver * NetDriver0x38 8
}
namespace SonyDriverHelper {
    class api {
    public:
        static void Init();

        static void MouseMove(float x, float y);

        static bool GetKey(int id);
    };
}
__forceinline auto RandomFloat(float a, float b) -> float
{
    float random = ((float)rand()) / (float)RAND_MAX;
    float diff = b - a;
    float r = random * diff;
    return a + r;
}
float custom_fabsf(float x) {

    __m128 x_vec = _mm_set_ss(x);
    x_vec = _mm_and_ps(x_vec, _mm_castsi128_ps(_mm_set1_epi32(0x7FFFFFFF)));
    return _mm_cvtss_f32(x_vec);
}
class memory_t
{
public:
    bool memory_event(fvector newpos)
    {
        write<fvector>(CachePointers.PlayerController + memoryk::RotationInput, newpos);
        return true;
    }
    fvector GetLocation(fvector Location)
    {
        fvector Loc = fvector(Location.x, Location.y, Location.z);
        return Loc;
    }
    fvector CalcRotation(fvector& zaz, fvector& daz) {
        fvector dalte = zaz - daz;
        fvector ongle;
        float hpm = sqrtf(dalte.x * dalte.x + dalte.y * dalte.y);
        ongle.y = atan(dalte.y / dalte.x) * 57.295779513082f;
        ongle.x = (atan(dalte.z / hpm) * 57.295779513082f) * -1.f;
        if (dalte.x >= 0.f) ongle.y += 180.f;
        return ongle;
    }
}; memory_t memoryt;

// SendInput - usermode API, no game memory (evades memory-based AC)
__forceinline void mouse_move_sendinput(int dx, int dy)
{
    INPUT input = { 0 };
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_MOVE;
    input.mi.dx = dx;
    input.mi.dy = dy;
    input.mi.mouseData = 0;
    input.mi.dwExtraInfo = 0;
    input.mi.time = 0;
    SendInput(1, &input, sizeof(INPUT));
}

// Ease-in-out: slow at start/end, faster in middle (evades linear movement detection)
__forceinline float ease_in_out(float t)
{
    return t < 0.5f ? 2.f * t * t : 1.f - (-2.f * t + 2.f) * (-2.f * t + 2.f) / 2.f;
}

// Shared mouse movement logic for Kernel + SendInput (anti-detection applied)
__forceinline void mouse_move_aim(float step_x, float step_y, bool use_kernel)
{
    float move_x = step_x, move_y = step_y;
    
    // Step variance - humans don't move at constant increments (evades regular-interval detection)
    if (aimbot.legit_step_variance > 0) {
        const float v = aimbot.legit_step_variance;
        move_x *= RandomFloat(1.f - v, 1.f + v);
        move_y *= RandomFloat(1.f - v, 1.f + v);
    }
    // Per-axis variance - X and Y rarely move at identical rates
    if (aimbot.legit_per_axis_variance) {
        move_x *= RandomFloat(0.92f, 1.08f);
        move_y *= RandomFloat(0.92f, 1.08f);
    }
    // Overshoot - humans often overshoot then correct
    if (aimbot.legit_overshoot > 0 && RandomFloat(0.f, 1.f) < 0.2f) {
        const float o = 1.f + aimbot.legit_overshoot;
        move_x *= RandomFloat(1.f, o);
        move_y *= RandomFloat(1.f, o);
    }
    // Jitter
    if (aimbot.legit_random_movement && aimbot.legit_jitter_strength > 0) {
        const float j = aimbot.legit_jitter_strength * 2.f;
        move_x += RandomFloat(-j, j);
        move_y += RandomFloat(-j, j);
    }
    const int ix = static_cast<int>(move_x), iy = static_cast<int>(move_y);
    if (ix == 0 && iy == 0) return;
    if (use_kernel) {
        if (DotMem::driver_handle != INVALID_HANDLE_VALUE)
            DotMem::move_mouse(ix, iy, 0);
        else
            SonyDriverHelper::api::MouseMove(static_cast<float>(ix), static_cast<float>(iy));
    } else {
        mouse_move_sendinput(ix, iy);
    }
}

inline void move(fvector2d Head2D)
{
    const fvector2d ScreenCenter = { static_cast<double>(globals.ScreenWidth) / 2, static_cast<double>(globals.ScreenHeight) / 2 };
    const float raw_delta_x = static_cast<float>(Head2D.x - ScreenCenter.x);
    const float raw_delta_y = static_cast<float>(Head2D.y - ScreenCenter.y);

    if (raw_delta_x == 0 && raw_delta_y == 0)
        return;

    // Method 0: Kernel - driver moves mouse (bypasses usermode hooks, most stealth)
    if (aimbot.method == 0)
    {
        const float smooth = (aimbot.legit_mode || aimbot.human_aim)
            ? static_cast<float>(aimbot.legit_human_smoothing) : static_cast<float>(aimbot.smoothsize);
        float step_x = raw_delta_x / smooth;
        float step_y = raw_delta_y / smooth;
        static float accum_t_k = 0.f;
        if (aimbot.legit_aim_easing) {
            const float dt = 1.f / smooth;
            accum_t_k = (accum_t_k < 1.f) ? (accum_t_k + dt) : 1.f;
            const float eased = ease_in_out(accum_t_k) - ease_in_out((accum_t_k - dt) > 0 ? accum_t_k - dt : 0.f);
            step_x = raw_delta_x * eased;
            step_y = raw_delta_y * eased;
            if (accum_t_k >= 0.99f) accum_t_k = 0.f;
        }
        if (aimbot.human_aim || aimbot.legit_mode) {
            static ULONGLONG first_lock_time = 0, last_move_time = 0;
            static bool had_target = false;
            const ULONGLONG now = GetTickCount64();
            if (now - last_move_time > 200) { had_target = false; accum_t_k = 0.f; }
            last_move_time = now;
            if (!had_target) { had_target = true; first_lock_time = now; }
            if (now - first_lock_time < static_cast<ULONGLONG>(aimbot.legit_reaction_time)) return;
        }
        if (aimbot.legit_hesitation_chance > 0 && RandomFloat(0.f, 1.f) < aimbot.legit_hesitation_chance) return;
        mouse_move_aim(step_x, step_y, true);
    }
    // Method 1: Mouse (SendInput) - external input, no memory write
    else if (aimbot.method == 1)
    {
        const float smooth = (aimbot.legit_mode || aimbot.human_aim)
            ? static_cast<float>(aimbot.legit_human_smoothing) : static_cast<float>(aimbot.smoothsize);
        float step_x = raw_delta_x / smooth;
        float step_y = raw_delta_y / smooth;
        static float accum_t_m = 0.f;
        if (aimbot.legit_aim_easing) {
            const float dt = 1.f / smooth;
            accum_t_m = (accum_t_m < 1.f) ? (accum_t_m + dt) : 1.f;
            const float eased = ease_in_out(accum_t_m) - ease_in_out((accum_t_m - dt) > 0 ? accum_t_m - dt : 0.f);
            step_x = raw_delta_x * eased;
            step_y = raw_delta_y * eased;
            if (accum_t_m >= 0.99f) accum_t_m = 0.f;
        }
        if (aimbot.human_aim || aimbot.legit_mode) {
            static ULONGLONG first_lock_time = 0, last_move_time = 0;
            static bool had_target = false;
            const ULONGLONG now = GetTickCount64();
            if (now - last_move_time > 200) { had_target = false; accum_t_m = 0.f; }
            last_move_time = now;
            if (!had_target) { had_target = true; first_lock_time = now; }
            if (now - first_lock_time < static_cast<ULONGLONG>(aimbot.legit_reaction_time)) return;
        }
        if (aimbot.legit_hesitation_chance > 0 && RandomFloat(0.f, 1.f) < aimbot.legit_hesitation_chance) return;
        mouse_move_aim(step_x, step_y, false);
    }
    // Method 2: Memory write - legacy, detected by EAC
    else if (aimbot.method == 2)
    {
        const float AimSpeed = aimbot.smoothsize;
        fvector2d Target = { raw_delta_x / AimSpeed, raw_delta_y / AimSpeed };
        Target.x = (Target.x + ScreenCenter.x > ScreenCenter.x * 2 || Target.x + ScreenCenter.x < 0) ? 0 : Target.x;
        Target.y = (Target.y + ScreenCenter.y > ScreenCenter.y * 2 || Target.y + ScreenCenter.y < 0) ? 0 : Target.y;

        float offset_x = RandomFloat(Target.x - 1, Target.x + 1);
        float offset_y = RandomFloat(Target.y - 1, Target.y + 1);
        fvector new_rotation = { -offset_y / 5.f, offset_x / 5.f, 0 };
        memory_event(new_rotation);
    }
}