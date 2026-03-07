#pragma once

#include <ctime>
#include <string>
#include <vector>
#include <cmath>
#include <map>
#include <windows.h>
#include <shlobj.h>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "D3DX11.lib")

#define INITGUID
#include <d3d11.h>
#include <dxgi.h>

#include "../../../utilities/str_obfuscate.hpp"
#include "../../../dependencies/loader/console.h"
#include "../../../dependencies/configs/configs.h"
#include "../../../dependencies/configs/driver_mapper_selection.h"
#include "../../../framework/images.h"
#include "../../../framework/fonts.h"
#include "../../../framework/settings.h"
#include "../../../framework/font_awesome.h"
#include "../../../framework/logo.hpp"
#include "../../../framework/crypt.h"
#include "../../../framework/imgui.h"
#include "../../../framework/imgui_internal.h"
#include "../../../framework/imgui_impl_win32.h"

/* =========================================================
   GLOBAL STYLE CONSTANTS (JINXWARE)
   ========================================================= */

namespace style {
    inline ImColor bg_main = ImColor(30, 40, 80);  // Dark blue main background
    inline ImColor bg_panel = ImColor(40, 60, 120); // Medium blue panel
    inline ImColor border = ImColor(60, 80, 140);   // Light blue border

    // 🔵 BLUE ACCENT - #58b9ff
    inline ImColor accent = ImColor(255, 102, 153); // Pink

    inline ImColor text = ImColor(255, 255, 255, 230);
    inline ImColor text_dim = ImColor(200, 220, 255, 200);

    constexpr float rounding = 8.f;
    constexpr float padding = 38.f;
}

/* =========================================================
   ICON TEXTURES
   ========================================================= */

extern ID3D11ShaderResourceView* icon_aimbot_texture;
extern ID3D11ShaderResourceView* icon_visual_texture;
extern ID3D11ShaderResourceView* icon_misc_texture;

ID3D11ShaderResourceView* zLogo = nullptr;
ImFont* mainfont;

/* =========================================================
   UI ELEMENTS
   ========================================================= */

namespace elements {

    struct anim_state {
        float value = 0.f;
    };

    static std::map<ImGuiID, anim_state> anims;
    static std::map<ImGuiID, unsigned int> checkbox_last_toggle_frame;

    bool checkbox(const char* label, bool* v) {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window->SkipItems) return false;

        ImGuiContext& g = *GImGui;
        ImGuiID id = window->GetID(label);

        ImVec2 pos = window->DC.CursorPos;
        float size = 14.f;

        ImRect bb(pos, ImVec2(pos.x + size + 200.f, pos.y + size));
        ImGui::ItemSize(bb);
        if (!ImGui::ItemAdd(bb, id)) return false;

        bool hovered, held;
        bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);
        // Per-frame debounce: prevents double-toggle when low FPS processes multiple events in one frame
        if (pressed) {
            unsigned int frame = g.FrameCount;
            if (checkbox_last_toggle_frame[id] != frame) {
                checkbox_last_toggle_frame[id] = frame;
                *v = !*v;
            }
        }

        auto& anim = anims[id];
        anim.value = ImLerp(anim.value, *v ? 1.f : 0.f, g.IO.DeltaTime * 12.f);

        ImDrawList* dl = window->DrawList;

        dl->AddRectFilled(pos, ImVec2(pos.x + size, pos.y + size),
            style::bg_panel, 3.f);
        dl->AddRect(pos, ImVec2(pos.x + size, pos.y + size),
            style::border, 3.f);

        if (anim.value > 0.01f) {
            dl->AddRectFilled(
                pos,
                ImVec2(pos.x + size, pos.y + size),
                ImColor(style::accent.Value.x,
                    style::accent.Value.y,
                    style::accent.Value.z,
                    anim.value),
                3.f
            );

            dl->AddLine(
                ImVec2(pos.x + 3, pos.y + size * 0.55f),
                ImVec2(pos.x + size * 0.45f, pos.y + size - 3),
                ImColor(255, 255, 255, int(255 * anim.value)), 1.6f
            );
            dl->AddLine(
                ImVec2(pos.x + size * 0.45f, pos.y + size - 3),
                ImVec2(pos.x + size - 3, pos.y + 3),
                ImColor(255, 255, 255, int(255 * anim.value)), 1.6f
            );
        }

        dl->AddText(
            ImVec2(pos.x + size + 10.f, pos.y - 1.f),
            style::text,
            label
        );

        return pressed;
    }

    bool slider_float(const char* label, float* v, float min, float max) {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window->SkipItems) return false;

        ImGuiContext& g = *GImGui;
        ImGuiID id = window->GetID(label);

        ImVec2 pos = window->DC.CursorPos;
        float width = 200.f;
        float height = 6.f;

        ImRect bb(ImVec2(pos.x, pos.y + 20),
            ImVec2(pos.x + width, pos.y + 20 + height));

        ImGui::ItemSize(ImVec2(width, 35));
        if (!ImGui::ItemAdd(bb, id)) return false;

        bool hovered, held;
        bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);

        if (held) {
            float t = (g.IO.MousePos.x - bb.Min.x) / width;
            *v = min + (ImClamp(t, 0.f, 1.f) * (max - min));
        }

        float frac = (*v - min) / (max - min);

        ImDrawList* dl = window->DrawList;
        dl->AddText(pos, style::text, label);
        dl->AddRectFilled(bb.Min, bb.Max, style::bg_panel, 3.f);
        dl->AddRectFilled(bb.Min,
            ImVec2(bb.Min.x + width * frac, bb.Max.y),
            style::accent, 3.f);

        ImVec2 c(ImVec2(bb.Min.x + width * frac, bb.Min.y + height * .5f));
        dl->AddCircleFilled(c, 5.5f, ImColor(255, 255, 255));
        dl->AddCircle(c, 5.5f, style::border);

        char buf[32];
        sprintf_s(buf, "%.1f", *v);
        dl->AddText(ImVec2(bb.Max.x + 10, bb.Min.y - 3), style::text_dim, buf);

        return pressed || held;
    }

    bool slider_int(const char* label, int* v, int min, int max) {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window->SkipItems) return false;

        ImGuiContext& g = *GImGui;
        ImGuiID id = window->GetID(label);

        ImVec2 pos = window->DC.CursorPos;
        float width = 200.f;
        float height = 6.f;

        ImRect bb(ImVec2(pos.x, pos.y + 20),
            ImVec2(pos.x + width, pos.y + 20 + height));

        ImGui::ItemSize(ImVec2(width, 35));
        if (!ImGui::ItemAdd(bb, id)) return false;

        bool hovered, held;
        bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);

        if (held) {
            float t = (g.IO.MousePos.x - bb.Min.x) / width;
            *v = min + int(ImClamp(t, 0.f, 1.f) * (max - min));
        }

        float frac = float(*v - min) / float(max - min);

        ImDrawList* dl = window->DrawList;
        dl->AddText(pos, style::text, label);
        dl->AddRectFilled(bb.Min, bb.Max, style::bg_panel, 3.f);
        dl->AddRectFilled(bb.Min,
            ImVec2(bb.Min.x + width * frac, bb.Max.y),
            style::accent, 3.f);

        ImVec2 c(ImVec2(bb.Min.x + width * frac, bb.Min.y + height * .5f));
        dl->AddCircleFilled(c, 5.5f, ImColor(255, 255, 255));
        dl->AddCircle(c, 5.5f, style::border);

        char buf[32];
        sprintf_s(buf, "%d", *v);
        dl->AddText(ImVec2(bb.Max.x + 10, bb.Min.y - 3), style::text_dim, buf);

        return pressed || held;
    }

    bool tab(const char* name, bool active, ID3D11ShaderResourceView* icon) {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window->SkipItems) return false;

        ImGuiContext& g = *GImGui;
        ImGuiID id = window->GetID(name);

        ImVec2 pos = window->DC.CursorPos;
        ImVec2 size(120, 28);

        ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));
        ImGui::ItemSize(bb);
        if (!ImGui::ItemAdd(bb, id)) return false;

        bool hovered, held;
        bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);

        auto& anim = anims[id];
        anim.value = ImLerp(anim.value, active ? 1.f : hovered ? 0.7f : 0.4f,
            g.IO.DeltaTime * 10.f);

        ImDrawList* dl = window->DrawList;

        if (active)
            dl->AddRectFilled(bb.Min, bb.Max, style::accent, 6.f);
        else if (hovered)
            dl->AddRectFilled(bb.Min, bb.Max,
                ImColor(style::accent.Value.x,
                    style::accent.Value.y,
                    style::accent.Value.z,
                    0.2f),
                6.f);

        ImVec2 text_sz = ImGui::CalcTextSize(name);
        ImVec2 text_pos(
            bb.Min.x + (size.x - text_sz.x) * 0.5f,
            bb.Min.y + (size.y - text_sz.y) * 0.5f
        );

        dl->AddText(text_pos,
            ImColor(255, 255, 255, int(255 * anim.value)),
            name
        );

        return pressed;
    }
}

/* =========================================================
   MAIN MENU
   ========================================================= */

enum tabs {
    TAB_AIMBOT,
    TAB_TRIGGERBOT,
    TAB_VISUALS,
    TAB_MISC
};

enum aimbot_subtabs {
    AIMBOT_MAIN,
    AIMBOT_WEAPON_CONFIGS
};

static tabs current_tab = TAB_AIMBOT;
static aimbot_subtabs current_aimbot_subtab = AIMBOT_MAIN;

void LoadStyles() {
    ImGuiStyle& s = ImGui::GetStyle();
    ImGui::StyleColorsDark();

    s.WindowRounding = style::rounding;
    s.FrameRounding = 6.f;
    s.ItemSpacing = ImVec2(8, 8);
    s.WindowPadding = ImVec2(0, 0);

    s.Colors[ImGuiCol_WindowBg] = style::bg_main;
    s.Colors[ImGuiCol_Text] = style::text;
}

/* =========================================================
   MENU RENDER
   ========================================================= */

void menu_class() {
    ImGui::SetNextWindowSize({ 800, 600 }, ImGuiCond_Once);
    ImGui::Begin("##main", nullptr,
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetWindowPos();
    ImVec2 size = ImGui::GetWindowSize();

    const char* cHitboxes[] = { "Head", "Neck", "Chest", "Pelvis" };



    // Tab buttons with pink theme
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.0f, 0.4f, 0.6f, 1.0f)); // Pink
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.5f, 0.7f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.9f, 0.3f, 0.5f, 1.0f));
    
    ImGui::SetCursorPos({ 10, 14 });
    if (ImGui::Button("Aimbot", ImVec2(90, 30)))
        current_tab = TAB_AIMBOT;
    ImGui::SameLine();
    if (ImGui::Button("Triggerbot", ImVec2(90, 30)))
        current_tab = TAB_TRIGGERBOT;
    ImGui::SameLine();
    if (ImGui::Button("Visuals", ImVec2(90, 30)))
        current_tab = TAB_VISUALS;
    ImGui::SameLine();
    if (ImGui::Button("Misc", ImVec2(90, 30)))
        current_tab = TAB_MISC;

    ImGui::PopStyleColor(3);

    ImGui::SetCursorPos({ 10, 50 });
    if (current_tab == TAB_AIMBOT) {
        // Master Aimbot Enable Dropdown
        ImGui::SetCursorPosX(28);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
        ImGui::SetNextItemWidth(150);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.15f, 0.15f, 0.18f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.18f, 0.18f, 0.22f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 0.9f));
        
        const char* aimbot_modes[] = { "Disabled", "Enabled" };
        static int aimbot_master_index = 0;
        aimbot_master_index = aimbot.enable ? 1 : 0;  // Sync from settings so menu doesn't show wrong state
        if (ImGui::Combo("Master Aimbot", &aimbot_master_index, aimbot_modes, IM_ARRAYSIZE(aimbot_modes))) {
            aimbot.enable = (aimbot_master_index == 1);
            // Enable all aimbot features when master is enabled
            if (aimbot.enable) {
                aimbot.human_aim = true;
                aimbot.drawfov = true;
                aimbot.fov_arrows = true;
            }
        }
        
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar();
        
        // Aimbot Method Dropdown
        ImGui::SetCursorPosX(28);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
        ImGui::SetNextItemWidth(120);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.15f, 0.15f, 0.18f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.18f, 0.18f, 0.22f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 0.9f));
        
        const char* aimbot_methods[] = { "Kernel (Driver)", "Mouse (SendInput)", "Memory (Detected)" };
        ImGui::Combo("Aimbot Method", &aimbot.method, aimbot_methods, IM_ARRAYSIZE(aimbot_methods));
        
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar();
        
        elements::checkbox("Human Aim", &aimbot.human_aim);
        elements::checkbox("Visibility Check", &aimbot.vischeck);
        elements::checkbox("Prediction", &aimbot.prediction);
        elements::checkbox("Legit Mode", &aimbot.legit_mode);
        elements::checkbox("ESP Preview", &aimbot.esp_preview_enabled);
        
        if (aimbot.enable) {
            // Keybind Combo
            ImGui::SetCursorPosX(28);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
            ImGui::SetNextItemWidth(100);
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.15f, 0.15f, 0.18f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.18f, 0.18f, 0.22f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 0.9f));

            const char* keybind_options[] = { "LM", "RM", "Mouse 4", "Mouse 5" };
            static int keybind_vk[] = {
                VK_LBUTTON,
                VK_RBUTTON,
                VK_XBUTTON1,
                VK_XBUTTON2
            };

            ImGui::Combo("Keybind", &aimbot.aimkey_index, keybind_options, IM_ARRAYSIZE(keybind_options));
            aimbot.aimkey = keybind_vk[aimbot.aimkey_index];

            ImGui::PopStyleColor(3);
            ImGui::PopStyleVar();

            // Hitbox Combo
            ImGui::SetCursorPosX(28);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
            ImGui::SetNextItemWidth(100);
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.15f, 0.15f, 0.18f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.18f, 0.18f, 0.22f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 0.9f));

            ImGui::Combo("Aimbot Hitbox", &aimbot.Hitbox, cHitboxes, IM_ARRAYSIZE(cHitboxes));

            ImGui::PopStyleColor(3);
            ImGui::PopStyleVar();
        }

        ImGui::Columns(2, nullptr, false);
        ImGui::NextColumn();
        
        elements::checkbox("Show FOV", &aimbot.drawfov);
        elements::checkbox("Show FOV Filled", &aimbot.predictiondot);
        elements::checkbox("Show FOV RGB", &aimbot.show_fov_rgb);
        elements::checkbox("Show FOV Arrows", &aimbot.fov_arrows);
        
        elements::slider_int("FOV Size", &aimbot.fovsize, 50, 500);
        elements::slider_int("Max Distance", &aimbot.fov_drawdistance, 50, 1000);
        elements::slider_int("Smooth", &aimbot.smoothsize, 1, 50);
        
        if (aimbot.legit_mode) {
            elements::slider_int("Legit FOV", &aimbot.legit_fov, 30, 200);
            elements::slider_int("Legit Smooth", &aimbot.legit_smooth, 1, 80);
        }
        
        // Weapon Config Selection Dropdown
        ImGui::SetCursorPosX(28);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
        ImGui::SetNextItemWidth(150);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.15f, 0.15f, 0.18f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.18f, 0.18f, 0.22f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 0.9f));
        
        const char* weapon_configs[] = { 
            "None", 
            "Assault Rifle", 
            "SMG", 
            "Shotgun", 
            "Sniper Rifle", 
            "Pistol" 
        };
        static int weapon_config_index = 0;
        // Sync from settings so menu doesn't reset display
        if (aimbot.rifle_config_enabled) weapon_config_index = 1;
        else if (aimbot.smg_config_enabled) weapon_config_index = 2;
        else if (aimbot.shotgun_config_enabled) weapon_config_index = 3;
        else if (aimbot.sniper_config_enabled) weapon_config_index = 4;
        else if (aimbot.pistol_config_enabled) weapon_config_index = 5;
        else weapon_config_index = 0;
        
        if (ImGui::Combo("Weapon Type", &weapon_config_index, weapon_configs, IM_ARRAYSIZE(weapon_configs))) {
            // Reset all configs
            aimbot.rifle_config_enabled = false;
            aimbot.smg_config_enabled = false;
            aimbot.shotgun_config_enabled = false;
            aimbot.sniper_config_enabled = false;
            aimbot.pistol_config_enabled = false;
            aimbot.weapon_configs_enabled = (weapon_config_index > 0);
            
            // Enable selected config
            switch(weapon_config_index) {
                case 1: aimbot.rifle_config_enabled = true; break;
                case 2: aimbot.smg_config_enabled = true; break;
                case 3: aimbot.shotgun_config_enabled = true; break;
                case 4: aimbot.sniper_config_enabled = true; break;
                case 5: aimbot.pistol_config_enabled = true; break;
            }
        }
        
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar();
        
        // Display settings for enabled weapons
        if (aimbot.rifle_config_enabled) {
            ImGui::Text("Rifle Config:");
            elements::slider_int("Rifle FOV", &aimbot.rifle_fov, 50, 500);
            elements::slider_int("Rifle Smooth", &aimbot.rifle_smooth, 1, 20);
        }
        
        if (aimbot.smg_config_enabled) {
            ImGui::Text("SMG Config:");
            elements::slider_int("SMG FOV", &aimbot.smg_fov, 50, 500);
            elements::slider_int("SMG Smooth", &aimbot.smg_smooth, 1, 20);
        }
        
        ImGui::Columns(1);
        
        // Legit Config Section
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        elements::checkbox("Enable Legit Config", &aimbot.legit_config_enabled);
        
        if (aimbot.legit_config_enabled) {
            ImGui::Text("Legit Aim Configuration (Anti-Spectator Mode)");
            ImGui::Spacing();
            
            elements::slider_int("Reaction Time (ms)", &aimbot.legit_reaction_time, 50, 500);
            elements::slider_int("Human Smoothing", &aimbot.legit_human_smoothing, 1, 20);
            elements::slider_int("Max FOV", &aimbot.legit_max_fov, 10, 150);
            
            ImGui::Spacing();
            ImGui::Text("Advanced Settings:");
            
            elements::checkbox("Random Movement", &aimbot.legit_random_movement);
            if (aimbot.legit_random_movement) {
                elements::slider_float("Jitter Strength", &aimbot.legit_jitter_strength, 0.1f, 1.0f);
            }
            
            ImGui::Spacing();
            ImGui::Text("Anti-Detection (Mouse/Kernel):");
            elements::checkbox("Aim Easing (Bezier-like)", &aimbot.legit_aim_easing);
            elements::checkbox("Per-Axis Variance", &aimbot.legit_per_axis_variance);
            elements::slider_float("Step Variance", &aimbot.legit_step_variance, 0.0f, 0.4f);
            elements::slider_float("Hesitation Chance", &aimbot.legit_hesitation_chance, 0.0f, 0.25f);
            elements::slider_float("Overshoot", &aimbot.legit_overshoot, 0.0f, 0.2f);
            
            elements::checkbox("Adaptive Smoothing", &aimbot.legit_adaptive_smoothing);
            elements::slider_int("Target Switch Delay (ms)", &aimbot.legit_target_switch_delay, 100, 1000);
            
            ImGui::Spacing();
            ImGui::Text("Combat Behavior:");
            
            elements::checkbox("Burst Mode", &aimbot.legit_burst_mode);
            if (aimbot.legit_burst_mode) {
                elements::slider_int("Shots Per Burst", &aimbot.legit_burst_count, 2, 10);
            }
            
            elements::checkbox("Random Headshots", &aimbot.legit_headshot_chance);
            if (aimbot.legit_headshot_chance) {
                elements::slider_int("Headshot Chance %", &aimbot.legit_headshot_percentage, 30, 95);
            }
        }
    }

    if (current_tab == TAB_TRIGGERBOT) {
        ImGui::Columns(2, nullptr, false); // 2 columns

        elements::checkbox("Enable Triggerbot", &triggerbot.triggerbot_enable);
        
        if (triggerbot.triggerbot_enable) {
            // Triggerbot Keybind
            ImGui::SetCursorPosX(28);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
            ImGui::SetNextItemWidth(100);
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.15f, 0.15f, 0.18f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.18f, 0.18f, 0.22f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 0.9f));

            const char* trigger_key_options[] = { "Shift", "Ctrl", "Alt", "Right Mouse", "Mouse 4", "Mouse 5" };
            static int trigger_key_vk[] = {
                VK_LSHIFT,
                VK_LCONTROL,
                VK_LMENU,
                VK_RBUTTON,
                VK_XBUTTON1,
                VK_XBUTTON2
            };

            int trigger_key_index = 0;
            // Find current key index
            for (int i = 0; i < IM_ARRAYSIZE(trigger_key_vk); i++) {
                if (triggerbot.triggerbot_key == trigger_key_vk[i]) {
                    trigger_key_index = i;
                    break;
                }
            }

            ImGui::Combo("Trigger Key", &trigger_key_index, trigger_key_options, IM_ARRAYSIZE(trigger_key_options));
            triggerbot.triggerbot_key = trigger_key_vk[trigger_key_index];

            ImGui::PopStyleColor(3);
            ImGui::PopStyleVar();

            elements::slider_int("Distance", &triggerbot.triggerbot_distance, 50, 500);
            elements::slider_int("Delay (ms)", &triggerbot.delay, 0, 100);
        }

        ImGui::NextColumn(); // move to second column
        
        ImGui::Columns(1); // reset to single column
        
        ImGui::SetCursorPos({ size.x - 200, size.y - 25 });
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 0.8f));
        ImGui::Text("Made By Dxrk   2026");
        ImGui::PopStyleColor();
    }

    if (current_tab == TAB_VISUALS) {
        ImGui::Columns(2, nullptr, false); // 2 columns for checkboxes

        // Column 1
        elements::checkbox("Box", &visuals.box);
        elements::checkbox("Filled Box", &visuals.filled);
        elements::checkbox("Skeleton", &visuals.skeleton);
        elements::checkbox("Alternative Skeleton", &visuals.skeleton_alt); // Second skeleton style
        elements::checkbox("Skeleton Head", &visuals.skeletonhead);
        elements::checkbox("Name", &visuals.username);
        elements::checkbox("Render Count", &visuals.render_count); // Players nearby counter
        elements::checkbox("Visible Count", &visuals.visible_count); // Visible players counter
        // Picture ESP Dropdown
        ImGui::SetCursorPosX(28);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
        ImGui::SetNextItemWidth(120);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.15f, 0.15f, 0.18f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.18f, 0.18f, 0.22f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 0.9f));
        
        const char* picture_esp_options[] = { "None", "Charlie ESP", "Box ESP" };
        ImGui::Combo("Picture ESP", &settings::visuals::picture_esp_type, picture_esp_options, IM_ARRAYSIZE(picture_esp_options));
        
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar();

        ImGui::NextColumn(); // move to column 2
        elements::checkbox("Distance", &visuals.distance);
        elements::checkbox("Weapon", &visuals.weapon);
        elements::checkbox("Radar", &visuals.rRadar);
        elements::checkbox("Rank", &visuals.rank);
        elements::checkbox("Platform", &visuals.platform);
        elements::checkbox("Team Check", &visuals.teamcheck);

        ImGui::Columns(1); // reset to single column for sliders

        // ESP Settings
        ImGui::Text("ESP Settings");
        elements::slider_float("ESP Font Size", &vscolors.esp_font_size, 8.0f, 24.0f);
        elements::slider_float("FOV Arrow Size", &vscolors.fov_arrow_thickness, 1.0f, 10.0f);
        
        // Color Pickers
        ImGui::Text("ESP Colors");
        ImGui::ColorEdit4("Visible Box", vscolors.box_visible, picker_flags);
        ImGui::ColorEdit4("Invisible Box", vscolors.box_invisible, picker_flags);
        ImGui::ColorEdit4("Visible Fill", vscolors.box_filled_visible, picker_flags);
        ImGui::ColorEdit4("Invisible Fill", vscolors.box_filled_invisible, picker_flags);
        
        // Sliders
        elements::slider_int("Box Thickness", &visuals.box_thick, 1, 15);
        elements::slider_int("Skeleton Thickness", &visuals.skeleton_thickness, 1, 15);
        elements::slider_int("Render Distance", &visuals.renderDistance, 0, 800);
        ImGui::SetCursorPos({ size.x - 200, size.y - 25 });
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 0.8f));
        ImGui::Text("Made By DXRK Ultimate 2026");
        ImGui::PopStyleColor();
    }

    if (current_tab == TAB_MISC) {
        elements::checkbox("VSync", &visuals.vsync);
        elements::slider_int("Target FPS", &visuals.overlay_fps, 30, 240);
        elements::checkbox("Ignore Knocked", &visuals.ignore_dying);
        elements::checkbox("Visible Check", &aimbot.vischeck);

        /* Driver mapper: type 1–4, then pick directory for mapper exe. Saved in config [Driver] MapperType, MapperDir. */
        {
            const char* mapper_options[] = { "1 = kdmapper", "2 = Aether.Mapper", "3 = rtcore (vuln)", "4 = LegitMemory (untested)" };
            int mapper_idx = (g_mapper_type >= 0 && g_mapper_type <= 3) ? g_mapper_type : 0;
            if (ImGui::Combo("Mapper type (1-4)", &mapper_idx, mapper_options, IM_ARRAYSIZE(mapper_options)))
                g_mapper_type = mapper_idx;
            ImGui::Text("Mapper folder: %s", g_mapper_directory.empty() ? "(exe directory)" : g_mapper_directory.c_str());
            if (ImGui::Button("Pick mapper folder (Enter)")) {
                BROWSEINFOA bi = {};
                bi.lpszTitle = "Select folder containing mapper exe (kdmapper.exe, Aether.Mapper.exe, etc.)";
                bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
                LPITEMIDLIST pidl = SHBrowseForFolderA(&bi);
                if (pidl) {
                    char path[MAX_PATH] = {};
                    if (SHGetPathFromIDListA(pidl, path))
                        g_mapper_directory = path;
                    CoTaskMemFree(pidl);
                }
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Opens folder browser. Choose the folder where your mapper exe is (e.g. x64/Release).");
            if (!g_mapper_directory.empty() && ImGui::Button("Clear mapper folder")) {
                g_mapper_directory.clear();
            }
        }

        // Config buttons
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 10);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.6f, 0.1f, 1.0f));
        if (ImGui::Button("Save Config", ImVec2(120, 30))) {
            save_config("hezux_config.ini");
        }
        ImGui::PopStyleColor(3);
        
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.7f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.8f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.1f, 0.6f, 1.0f));
        if (ImGui::Button("Load Config", ImVec2(120, 30))) {
            load_config("hezux_config.ini");
        }
        ImGui::PopStyleColor(3);
        
        ImGui::SetCursorPos({ size.x - 200, size.y - 25 });
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 0.8f));
        ImGui::Text("Made By DXRK Ultimate 2026");
        ImGui::PopStyleColor();
    }

    ImGui::End();
}
