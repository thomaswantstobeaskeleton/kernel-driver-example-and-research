#ifndef SETTINGS_H
#define SETTINGS_H

#include <mutex>
#include <memory>
#include <Windows.h>
#include "../../../framework/imgui.h"

class menu_t
{
public:
	bool ShowMenu = false;  // Start hidden - press INSERT to open
	int menu_key = VK_INSERT;
	int fontsize = 15;

	ImFont* MenuFont;
	bool menu_cursor = false;
	int menu_index = 0;
	int tab = 1;
}; menu_t menus;

class watermark_t
{
public:
	bool enable = true;
	float watermark_size = 18.0f;
	int watermark_pos_x = 1;
	int watermark_pos_y = 1;
	bool center = false;
}; watermark_t watermark;

class globals_t
{
public:
	int ScreenWidth;
	ImVec2 NiggaWidth;
	int ScreenHeight;

	__int64 va_text = 0;
	HWND window_handle;
	bool use_crosshairx = false; // Use CrosshairX hijacking instead of own overlay (lower CPU usage)
	bool use_discord = false; // Use Discord hijacking instead of own overlay (lower CPU usage)
}; globals_t globals;

class triggerbot_t
{
public:
	bool triggerbot_enable = false;
	int triggerbot_distance = 120;
	int delay = 1;
	int triggerbot_key = VK_RBUTTON;
}; triggerbot_t triggerbot;

class aimbot_t
{
public:
	// Main
	bool enable = false;
	bool freezeplayer = false;
	bool target_line = false;
	int method = 0;
	bool human_aim = false;

	// Misc
	bool prediction = false;
	bool predictiondot = false;
	bool show_fov_rgb = false;
	bool vischeck = true;
	bool legit_mode = false; // Anti-spectator legit mode
	bool esp_preview = false; // ESP preview window
	bool esp_preview_enabled = false; // Enable ESP preview
	bool drawfov = true;
	bool fov_arrows = true;
	int fov_drawdistance;
	int crosshair_type = 1; // 0 = Point, 1 = Normal

	// Settings
	int fovsize = 150;
	int ADS_Fov_Size = 150;
	int legit_smooth = 3; // Smoother aim for legit mode
	int legit_fov = 100; // Reduced FOV for legit mode
	int smoothsize = 9;
	int Hitbox = 0;
	int aimkey = VK_LBUTTON;
	int freezeplayerkey = VK_RBUTTON;
	int aimkey_index = 0; // 0 = Left Click, 1 = Right Click, 2 = Mouse4, etc.
	
	// Weapon Configs
	bool rifle_config_enabled = false;
	bool smg_config_enabled = false;
	bool shotgun_config_enabled = false;
	bool sniper_config_enabled = false;
	bool pistol_config_enabled = false;
	
	int rifle_fov = 150;
	int rifle_smooth = 9;
	int rifle_hitbox = 0;
	int smg_fov = 120;
	int smg_smooth = 7;
	int smg_hitbox = 1;
	int shotgun_fov = 100;
	int shotgun_smooth = 5;
	int shotgun_hitbox = 2;
	int sniper_fov = 200;
	int sniper_smooth = 12;
	int sniper_hitbox = 0;
	int pistol_fov = 80;
	int pistol_smooth = 4;
	int pistol_hitbox = 1;
	bool weapon_configs_enabled = false;
	
	// Legit Config
	bool legit_config_enabled = false;
	int legit_reaction_time = 150; // ms delay before locking on
	int legit_human_smoothing = 8; // More human-like smoothing
	int legit_max_fov = 60; // Tight FOV to appear more legit
	bool legit_random_movement = true; // Adds slight random movement
	float legit_jitter_strength = 0.3f; // Amount of random jitter
	bool legit_adaptive_smoothing = true; // Adjusts smoothing based on distance
	int legit_target_switch_delay = 300; // Delay before switching targets (ms)
	bool legit_burst_mode = false; // Fires in bursts instead of continuous
	int legit_burst_count = 3; // Number of shots per burst
	bool legit_headshot_chance = true; // Random headshots to look legit
	int legit_headshot_percentage = 70; // Percentage chance for headshots
	
	// Anti-detection (mouse movement)
	bool legit_aim_easing = true; // Ease-in-out curve (Bezier-like, evades linear detection)
	float legit_step_variance = 0.15f; // ±15% random step size variation
	float legit_hesitation_chance = 0.08f; // 8% chance to skip frame (human hesitation)
	float legit_overshoot = 0.05f; // 5% overshoot then correction
	bool legit_per_axis_variance = true; // X/Y move at slightly different rates
}; aimbot_t aimbot;

class world_t
{
public:
	bool enable = false;
	bool chests = false;
	bool pickups = false;
	bool uncommon = false;
	bool common = false;
	bool epic = false;
	bool rare = false;
	bool legendary = false;
	bool mythic = false;

	int loot_distace = 50;
	int cache_update_speed = 100;
}; world_t world;

class visuals_t
{
public:
	bool enable = true;
	bool player_count = false;
	bool render_count = false;
	bool visible_count = false;
	bool nigger = true;

	bool box = false;
	bool corner_box = false;
	bool filled = false;
	bool skeleton = true;   // Default on
	bool skeleton_alt = false;
	bool head_esp = false;
	bool corneredbox = false;
	bool radar = false;
	bool distance = false;
	bool username = true;   // Default on (name ESP)
	bool platform = false;
	bool weapon = false;
	bool crosshair = false;
	bool rank = false;
	bool killscore = false;
	bool level_score = false;
	bool chests_opened = false;
	bool minutes_alive = false;
	bool current_health = false;
	bool current_shield = false;
	bool current_spectators = false;
	bool cornered_outline = false;
	bool cornered_filled = false;
	bool corner_box_filled = false;
	bool skeletonhead = false;
	bool ShowRadar = false;
	bool teamcheck = false;
	bool ignore_dying = false;
	int crosshair_color[4] = { 255, 255, 255, 255 };
	int visible_color[4] = { 0, 255, 0, 255 };
	int invisible_color[4] = { 255, 0, 0, 255 };  // Red (RGBA)
	bool vsync = false;
	int overlay_fps = 120;  // Own overlay target FPS when vsync off (higher = faster ESP updates)
	bool rRadar = false;
	bool corner_box_outline = false;
	bool corner_filled = false;



	int box_thick = 1;
	int skeleton_thickness = 1;

	float corner_width = 0.35f;

	bool box_outline = false;
	bool skel_outline = false;

	int renderDistance = 200;
	int offset_x = 0;

	// Picture ESP texture
	ImTextureID picture_texture = nullptr;
}; visuals_t visuals;

class colors_t
{
public:
	// Green color for visible box
	float box_visible[4] = { 0.f, 255.f, 0.f, 255.f }; // RGBA: Green (255 for Green, 0 for Red and Blue, 255 for full opacity)
	// Red color for invisible box
	float box_invisible[4] = { 255.f, 0.f, 0.f, 255.f }; // RGBA: Red (255 for Red, 0 for Green and Blue, 255 for full opacity)
	float box_filled_visible[4] = { 30.f / 255.f, 30.f / 255.f, 30.f / 255.f, 50.f / 255.f };  // Dunkles Grau, leicht durchsichtig
	float box_filled_invisible[4] = { 1.0f, 0.f, 0.f, 110.f / 255.f };  // Rot mit Transparenz
	
	// ESP Font and Arrow Sizes
	float esp_font_size = 14.0f; // ESP text font size
	float fov_arrow_thickness = 3.0f; // FOV arrow thickness
}; colors_t vscolors;
DWORD picker_flags = ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaPreview;

class misc_t
{
public:
	bool debug = true;
}; misc_t misc;

class exploits_t
{
public:
	bool fov_changer = false;
	bool spin_bot = false;
	bool bEditEnemyBuilds = false;
	bool airStuck_enemyPlayers = false;
	bool insta_reboot = false;
	bool AimInAir = false;
	bool airStuck = false;
	bool no_recoil = false;

	float rotationSpeed = 0.9f;
	float currentAngle = 8.0f;
	bool isSpinning = false;
	bool big_players = false;
	bool bFallDamage = false;
	bool magicbullet = false;
	float radarsize;
	ImVec2 radarpos; 
	fvector originalRotation;

	int fov_value;

	int freeze_enemyplayers_key = 'G';
}; inline exploits_t exploits;

#endif // SETTINGS_H