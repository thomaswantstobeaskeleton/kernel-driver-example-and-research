#include <WinBase.h>
#include <string>

#include "../../framework/settings.h"
#include "../../utilities/sdk/utils/settings.h"
#include "driver_mapper_selection.h"

static BOOL write_private_profile_int(LPCSTR lpAppName, LPCSTR lpKeyName, int nInteger, LPCSTR lpFileName) {
    char lpString[1024];
    sprintf(lpString, ("%d"), nInteger);
    return WritePrivateProfileStringA(lpAppName, lpKeyName, lpString, lpFileName);
}
static BOOL write_private_profile_float(LPCSTR lpAppName, LPCSTR lpKeyName, float nInteger, LPCSTR lpFileName) {
    char lpString[1024];
    sprintf(lpString, ("%f"), nInteger);
    return WritePrivateProfileStringA(lpAppName, lpKeyName, lpString, lpFileName);
}
static float get_private_profile_float(LPCSTR lpAppName, LPCSTR lpKeyName, FLOAT flDefault, LPCSTR lpFileName) {
    char szData[32];

    GetPrivateProfileStringA(lpAppName, lpKeyName, std::to_string(flDefault).c_str(), szData, 32, lpFileName);

    return (float)atof(szData);
}

static BOOL read_bool(LPCSTR section, LPCSTR key, bool& target, LPCSTR path) {
    int value = GetPrivateProfileIntA(section, key, -1, path);
    if (value != -1) {
        target = value != 0;

        return true;
    }       

    return false;
}

static BOOL read_int(LPCSTR section, LPCSTR key, int& target, LPCSTR path) {
    int value = GetPrivateProfileIntA(section, key, -1, path);
    if (value != -1) {
        target = value;

        return true;
    }

    return false;
}

static BOOL read_string(LPCSTR section, LPCSTR key, std::string& target, LPCSTR path, size_t maxLen = 512) {
    std::string buf(maxLen, '\0');
    if (GetPrivateProfileStringA(section, key, "", &buf[0], (DWORD)maxLen, path) > 0) {
        buf.resize(strlen(buf.c_str()));
        target = buf;
        return true;
    }
    return false;
}

static BOOL read_float(LPCSTR section, LPCSTR key, float& target, LPCSTR path) {
    char buffer[32];
    if (GetPrivateProfileStringA(section, key, "", buffer, sizeof(buffer), path) > 0) {
        float value = atof(buffer);
        target = value;

        return true;
    }

    return false;
}

static BOOL read_color(LPCSTR section, LPCSTR key, float target[4], LPCSTR path) {
    char buffer[64];
    if (GetPrivateProfileStringA(section, key, "", buffer, sizeof(buffer), path) > 0) {
        char* token;
        char* context;
        int i = 0;
        token = strtok_s(buffer, (","), &context);
        while (token != NULL && i < 4) {
            float value = atof(token);
            target[i] = value;
            token = strtok_s(NULL, (","), &context);
            i++;
        }

        return true;
    }

    return false;
}

static void write_color(LPCSTR section, LPCSTR key, const float value[4], LPCSTR path) {
    char buffer[64];
    snprintf(buffer, sizeof(buffer), ("%.6f,%.6f,%.6f,%.6f"), value[0], value[1], value[2], value[3]);
    WritePrivateProfileStringA(section, key, buffer, path);
}

void save_config(LPCSTR path)
{
    {
        {
            // Combat
            write_private_profile_int(("Combat"), ("enable"), aimbot.enable, path);
            write_private_profile_int(("Combat"), ("method"), aimbot.method, path);
            write_private_profile_int(("Combat"), ("prediction"), aimbot.prediction, path);
            write_private_profile_int(("Combat"), ("prediction_dot"), aimbot.predictiondot, path);
            write_private_profile_int(("Combat"), ("draw_fov"), aimbot.drawfov, path);
            write_private_profile_int(("Combat"), ("fov_arrows"), aimbot.fov_arrows, path);
            write_private_profile_int(("Combat"), ("fov_size"), aimbot.fovsize, path);
            write_private_profile_int(("Combat"), ("smothness"), aimbot.smoothsize, path);
            write_private_profile_int(("Combat"), ("hitbox"), aimbot.Hitbox, path);
            write_private_profile_int(("Combat"), ("legit_mode"), aimbot.legit_mode, path);
            write_private_profile_int(("Combat"), ("esp_preview"), aimbot.esp_preview, path);
            write_private_profile_int(("Combat"), ("esp_preview_enabled"), aimbot.esp_preview_enabled, path);
            write_private_profile_int(("Combat"), ("method"), aimbot.method, path);
            write_private_profile_float(("Visuals"), ("esp_font_size"), vscolors.esp_font_size, path);
            write_private_profile_float(("Visuals"), ("fov_arrow_thickness"), vscolors.fov_arrow_thickness, path);
            write_private_profile_int(("Combat"), ("legit_fov"), aimbot.legit_fov, path);
            write_private_profile_int(("Combat"), ("legit_smooth"), aimbot.legit_smooth, path);
            // Weapon Configs
            write_private_profile_int(("WeaponConfigs"), ("enabled"), aimbot.weapon_configs_enabled, path);
            write_private_profile_int(("WeaponConfigs"), ("rifle_enabled"), aimbot.rifle_config_enabled, path);
            write_private_profile_int(("WeaponConfigs"), ("smg_enabled"), aimbot.smg_config_enabled, path);
            write_private_profile_int(("WeaponConfigs"), ("shotgun_enabled"), aimbot.shotgun_config_enabled, path);
            write_private_profile_int(("WeaponConfigs"), ("sniper_enabled"), aimbot.sniper_config_enabled, path);
            write_private_profile_int(("WeaponConfigs"), ("pistol_enabled"), aimbot.pistol_config_enabled, path);
            write_private_profile_int(("WeaponConfigs"), ("rifle_fov"), aimbot.rifle_fov, path);
            write_private_profile_int(("WeaponConfigs"), ("rifle_smooth"), aimbot.rifle_smooth, path);
            write_private_profile_int(("WeaponConfigs"), ("rifle_hitbox"), aimbot.rifle_hitbox, path);
            write_private_profile_int(("WeaponConfigs"), ("smg_fov"), aimbot.smg_fov, path);
            write_private_profile_int(("WeaponConfigs"), ("smg_smooth"), aimbot.smg_smooth, path);
            write_private_profile_int(("WeaponConfigs"), ("smg_hitbox"), aimbot.smg_hitbox, path);
            write_private_profile_int(("WeaponConfigs"), ("shotgun_fov"), aimbot.shotgun_fov, path);
            write_private_profile_int(("WeaponConfigs"), ("shotgun_smooth"), aimbot.shotgun_smooth, path);
            write_private_profile_int(("WeaponConfigs"), ("shotgun_hitbox"), aimbot.shotgun_hitbox, path);
            write_private_profile_int(("WeaponConfigs"), ("sniper_fov"), aimbot.sniper_fov, path);
            write_private_profile_int(("WeaponConfigs"), ("sniper_smooth"), aimbot.sniper_smooth, path);
            write_private_profile_int(("WeaponConfigs"), ("sniper_hitbox"), aimbot.sniper_hitbox, path);
            write_private_profile_int(("WeaponConfigs"), ("pistol_fov"), aimbot.pistol_fov, path);
            write_private_profile_int(("WeaponConfigs"), ("pistol_smooth"), aimbot.pistol_smooth, path);
            write_private_profile_int(("WeaponConfigs"), ("pistol_hitbox"), aimbot.pistol_hitbox, path);
            
            // Legit Config
            write_private_profile_int(("LegitConfig"), ("enabled"), aimbot.legit_config_enabled, path);
            write_private_profile_int(("LegitConfig"), ("reaction_time"), aimbot.legit_reaction_time, path);
            write_private_profile_int(("LegitConfig"), ("human_smoothing"), aimbot.legit_human_smoothing, path);
            write_private_profile_int(("LegitConfig"), ("max_fov"), aimbot.legit_max_fov, path);
            write_private_profile_int(("LegitConfig"), ("random_movement"), aimbot.legit_random_movement, path);
            write_private_profile_float(("LegitConfig"), ("jitter_strength"), aimbot.legit_jitter_strength, path);
            write_private_profile_int(("LegitConfig"), ("adaptive_smoothing"), aimbot.legit_adaptive_smoothing, path);
            write_private_profile_int(("LegitConfig"), ("target_switch_delay"), aimbot.legit_target_switch_delay, path);
            write_private_profile_int(("LegitConfig"), ("burst_mode"), aimbot.legit_burst_mode, path);
            write_private_profile_int(("LegitConfig"), ("burst_count"), aimbot.legit_burst_count, path);
            write_private_profile_int(("LegitConfig"), ("headshot_chance"), aimbot.legit_headshot_chance, path);
            write_private_profile_int(("LegitConfig"), ("headshot_percentage"), aimbot.legit_headshot_percentage, path);
            write_private_profile_int(("Hotkeys"), ("aimbot_key"), aimbot.aimkey, path);
            // Triggerbot
            write_private_profile_int(("Triggerbot"), ("enable"), triggerbot.triggerbot_enable, path);
            write_private_profile_int(("Triggerbot"), ("distance"), triggerbot.triggerbot_distance, path);
            write_private_profile_int(("Triggerbot"), ("delay"), triggerbot.delay, path);
            write_private_profile_int(("Triggerbot"), ("key"), triggerbot.triggerbot_key, path);
            // Visuals
            write_private_profile_int(("Visuals"), ("enable"), visuals.enable, path);
            write_private_profile_int(("Visuals"), ("box"), visuals.box, path);
            write_private_profile_int(("Visuals"), ("filled"), visuals.filled, path);
            write_private_profile_int(("Visuals"), ("skeleton"), visuals.skeleton, path);
            write_private_profile_int(("Visuals"), ("render_count"), visuals.render_count, path);
            write_private_profile_int(("Visuals"), ("visible_count"), visuals.visible_count, path);
            write_private_profile_int(("Visuals"), ("distance"), visuals.distance, path);
            write_private_profile_int(("Visuals"), ("username"), visuals.username, path);
            write_private_profile_int(("Visuals"), ("platform"), visuals.platform, path);
            write_private_profile_int(("Visuals"), ("weapon"), visuals.weapon, path);
            write_private_profile_int(("Visuals"), ("rank"), visuals.rank, path);
            write_private_profile_int(("Visuals"), ("box_thickness"), visuals.box_thick, path);
            write_private_profile_int(("Visuals"), ("skel_thickness"), visuals.skeleton_thickness, path);
            write_private_profile_int(("Visuals"), ("box_outline"), visuals.box_thick, path);
            write_private_profile_int(("Visuals"), ("skel_outline"), visuals.skel_outline, path);
            write_private_profile_int(("Visuals"), ("render_distance"), visuals.renderDistance, path);
            write_private_profile_int(("Visuals"), ("overlay_fps"), visuals.overlay_fps, path);

            write_private_profile_int(("Exploits"), ("Spinbot"), settings::exploits::Spinbot, path);
            write_private_profile_int(("Exploits"), ("FOVChanger"), settings::exploits::FOVChanger, path);
            write_private_profile_float(("Exploits"), ("FOVValue"), settings::exploits::FOVVALUE, path);

            write_private_profile_int(("Driver"), ("MapperType"), g_mapper_type, path);
            WritePrivateProfileStringA(("Driver"), ("MapperDir"), g_mapper_directory.c_str(), path);

            write_color(("Colors"), ("box_visible"), vscolors.box_visible, path);
            write_color(("Colors"), ("box_invisible"), vscolors.box_invisible, path);
            write_color(("Colors"), ("box_filled_visible"), vscolors.box_filled_visible, path);
            write_color(("Colors"), ("box_filled_invisible"), vscolors.box_filled_invisible, path);
            
            // ESP Settings
            write_private_profile_float(("ESP"), ("text_size"), vscolors.esp_font_size, path);
            write_private_profile_float(("ESP"), ("fov_arrow_size"), vscolors.fov_arrow_thickness, path);
        }
    }
}

void load_config(LPCSTR path)
{
    {
        {
            // Combat
            read_bool(("Combat"), ("enable"), aimbot.enable, path);
            read_int(("Combat"), ("method"), aimbot.method, path);
            read_bool(("Combat"), ("prediction"), aimbot.prediction, path);
            read_bool(("Combat"), ("prediction_dot"), aimbot.predictiondot, path);
            read_bool(("Combat"), ("draw_fov"), aimbot.drawfov, path);
            read_bool(("Combat"), ("fov_arrows"), aimbot.fov_arrows, path);
            read_bool(("Combat"), ("legit_mode"), aimbot.legit_mode, path);
            read_bool(("Combat"), ("esp_preview"), aimbot.esp_preview, path);
            read_bool(("Combat"), ("esp_preview_enabled"), aimbot.esp_preview_enabled, path);
            read_int(("Combat"), ("method"), aimbot.method, path);
            read_float(("Visuals"), ("esp_font_size"), vscolors.esp_font_size, path);
            read_float(("Visuals"), ("fov_arrow_thickness"), vscolors.fov_arrow_thickness, path);
            read_int(("Combat"), ("fov_size"), aimbot.fovsize, path);
            read_int(("Combat"), ("legit_fov"), aimbot.legit_fov, path);
            read_int(("Combat"), ("smoothsize"), aimbot.smoothsize, path);
            read_int(("Combat"), ("legit_smooth"), aimbot.legit_smooth, path);
            read_int(("Combat"), ("hitbox"), aimbot.Hitbox, path);
            // Weapon Configs
            read_bool(("WeaponConfigs"), ("enabled"), aimbot.weapon_configs_enabled, path);
            read_bool(("WeaponConfigs"), ("rifle_enabled"), aimbot.rifle_config_enabled, path);
            read_bool(("WeaponConfigs"), ("smg_enabled"), aimbot.smg_config_enabled, path);
            read_bool(("WeaponConfigs"), ("shotgun_enabled"), aimbot.shotgun_config_enabled, path);
            read_bool(("WeaponConfigs"), ("sniper_enabled"), aimbot.sniper_config_enabled, path);
            read_bool(("WeaponConfigs"), ("pistol_enabled"), aimbot.pistol_config_enabled, path);
            read_int(("WeaponConfigs"), ("rifle_fov"), aimbot.rifle_fov, path);
            read_int(("WeaponConfigs"), ("rifle_smooth"), aimbot.rifle_smooth, path);
            read_int(("WeaponConfigs"), ("rifle_hitbox"), aimbot.rifle_hitbox, path);
            read_int(("WeaponConfigs"), ("smg_fov"), aimbot.smg_fov, path);
            read_int(("WeaponConfigs"), ("smg_smooth"), aimbot.smg_smooth, path);
            read_int(("WeaponConfigs"), ("smg_hitbox"), aimbot.smg_hitbox, path);
            read_int(("WeaponConfigs"), ("shotgun_fov"), aimbot.shotgun_fov, path);
            read_int(("WeaponConfigs"), ("shotgun_smooth"), aimbot.shotgun_smooth, path);
            read_int(("WeaponConfigs"), ("shotgun_hitbox"), aimbot.shotgun_hitbox, path);
            read_int(("WeaponConfigs"), ("sniper_fov"), aimbot.sniper_fov, path);
            read_int(("WeaponConfigs"), ("sniper_smooth"), aimbot.sniper_smooth, path);
            read_int(("WeaponConfigs"), ("sniper_hitbox"), aimbot.sniper_hitbox, path);
            read_int(("WeaponConfigs"), ("pistol_fov"), aimbot.pistol_fov, path);
            read_int(("WeaponConfigs"), ("pistol_smooth"), aimbot.pistol_smooth, path);
            read_int(("WeaponConfigs"), ("pistol_hitbox"), aimbot.pistol_hitbox, path);
            
            // Legit Config
            read_bool(("LegitConfig"), ("enabled"), aimbot.legit_config_enabled, path);
            read_int(("LegitConfig"), ("reaction_time"), aimbot.legit_reaction_time, path);
            read_int(("LegitConfig"), ("human_smoothing"), aimbot.legit_human_smoothing, path);
            read_int(("LegitConfig"), ("max_fov"), aimbot.legit_max_fov, path);
            read_bool(("LegitConfig"), ("random_movement"), aimbot.legit_random_movement, path);
            read_float(("LegitConfig"), ("jitter_strength"), aimbot.legit_jitter_strength, path);
            read_bool(("LegitConfig"), ("adaptive_smoothing"), aimbot.legit_adaptive_smoothing, path);
            read_int(("LegitConfig"), ("target_switch_delay"), aimbot.legit_target_switch_delay, path);
            read_bool(("LegitConfig"), ("burst_mode"), aimbot.legit_burst_mode, path);
            read_int(("LegitConfig"), ("burst_count"), aimbot.legit_burst_count, path);
            read_bool(("LegitConfig"), ("headshot_chance"), aimbot.legit_headshot_chance, path);
            read_int(("LegitConfig"), ("headshot_percentage"), aimbot.legit_headshot_percentage, path);
            
            read_int(("Hotkeys"), ("aimbot_key"), aimbot.aimkey, path);
            // Triggerbot
            read_bool(("Triggerbot"), ("enable"), triggerbot.triggerbot_enable, path);
            read_int(("Triggerbot"), ("distance"), triggerbot.triggerbot_distance, path);
            read_int(("Triggerbot"), ("delay"), triggerbot.delay, path);
            read_int(("Triggerbot"), ("key"), triggerbot.triggerbot_key, path);
            // Visuals
            read_bool(("Visuals"), ("enable"), visuals.enable, path);
            read_bool(("Visuals"), ("box"), visuals.box, path);
            read_bool(("Visuals"), ("filled"), visuals.filled, path);
            read_bool(("Visuals"), ("skeleton"), visuals.skeleton, path);
            read_bool(("Visuals"), ("skeleton_alt"), visuals.skeleton_alt, path);
            read_bool(("Visuals"), ("render_count"), visuals.render_count, path);
            read_bool(("Visuals"), ("visible_count"), visuals.visible_count, path);
            read_bool(("Visuals"), ("distance"), visuals.distance, path);
            read_bool(("Visuals"), ("username"), visuals.username, path);
            read_bool(("Visuals"), ("platform"), visuals.platform, path);
            read_bool(("Visuals"), ("weapon"), visuals.weapon, path);
            read_bool(("Visuals"), ("rank"), visuals.rank, path);
            read_int(("Visuals"), ("box_thickness"), visuals.box_thick, path);
            read_int(("Visuals"), ("skel_thickness"), visuals.skeleton_thickness, path);
            read_int(("Visuals"), ("box_outline"), visuals.box_thick, path);
            read_bool(("Visuals"), ("skel_outline"), visuals.skel_outline, path);
            read_int(("Visuals"), ("render_distance"), visuals.renderDistance, path);
            read_int(("Visuals"), ("overlay_fps"), visuals.overlay_fps, path);

            read_bool(("Exploits"), ("Spinbot"), settings::exploits::Spinbot, path);
            read_bool(("Exploits"), ("FOVChanger"), settings::exploits::FOVChanger, path);
            read_float(("Exploits"), ("FOVValue"), settings::exploits::FOVVALUE, path);

            read_int(("Driver"), ("MapperType"), g_mapper_type, path);
            read_string(("Driver"), ("MapperDir"), g_mapper_directory, path);

            read_color(("Colors"), ("box_visible"), vscolors.box_visible, path);
            read_color(("Colors"), ("box_invisible"), vscolors.box_invisible, path);
            read_color(("Colors"), ("box_filled_visible"), vscolors.box_filled_visible, path);
            read_color(("Colors"), ("box_filled_invisible"), vscolors.box_filled_invisible, path);
            
            // ESP Settings
            read_float(("ESP"), ("text_size"), vscolors.esp_font_size, path);
            read_float(("ESP"), ("fov_arrow_size"), vscolors.fov_arrow_thickness, path);
        }
    }
}