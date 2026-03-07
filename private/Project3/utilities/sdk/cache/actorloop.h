#include <list>
#include <string>
#include <ctime>
#include <chrono>
#include <cmath>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include "../utils/offsets.h"
#include <string.h>
#include "../render/functions.h"
#include <iostream>
#include "../aimbot/aimbot.h"
#include <vector>
#include <immintrin.h>
#include "../utils/utils.h"
#include "../../../dependencies/loader/console.h"
#include "drawing.h"
#include <memory>
#include <Windows.h>
#include "../../../framework/settings.h"
#include "../utils/settings.h"  // menus.ShowMenu for CacheLevels pause
#include <mutex>
#include "../../../framework/logo.hpp"
#include "../../../charliekurk.h"
#include "../../../ice.h"
#include <cmath>
IDXGISwapChain* d3d_swap_chain;
DWORD g_read_pause_until = 0;  // When GetTickCount() < this, skip game reads (see includes.h)
DWORD g_menu_closed_at = 0;    // When menu closes, set here - CacheLevels backs off for grace period
DWORD g_read_pause_expired_at = 0;  // When read pause expires, set here - used for startup ESP ramp-up

// Picture ESP Rendering Function (Supports Charlie and Box ESP)
// Forward declaration for ice texture - defined in render.h
extern ID3D11ShaderResourceView* ice_texture;

inline void RenderPictureESP(ImDrawList* draw_list, const ImVec2& topLeft, const ImVec2& bottomRight, bool isPlayerVisible, const ImVec4& colorss) {
    // Return early if no picture ESP is selected
    if (settings::visuals::picture_esp_type == 0) return;
    
    // Select appropriate texture based on dropdown selection
    ImTextureID current_texture = nullptr;
    
    switch(settings::visuals::picture_esp_type) {
        case 1: // Charlie ESP
            current_texture = visuals.picture_texture;
            break;
        case 2: // Box/Ice ESP
            current_texture = (ImTextureID)ice_texture;
            break;
        default:
            return; // No valid selection
    }
    
    if (current_texture == nullptr) return;
    
    ImVec2 tl = topLeft;
    ImVec2 br = bottomRight;

    auto finite = [](const ImVec2& p) {
        return std::isfinite(p.x) && std::isfinite(p.y);
    };

    if (!finite(tl) || !finite(br))
        return;

    if (tl.x > br.x) std::swap(tl.x, br.x);
    if (tl.y > br.y) std::swap(tl.y, br.y);

    const ImVec2 screen = ImGui::GetIO().DisplaySize;
    tl.x = std::clamp(tl.x, 0.0f, screen.x - 1.0f);
    tl.y = std::clamp(tl.y, 0.0f, screen.y - 1.0f);
    br.x = std::clamp(br.x, 0.0f, screen.x - 1.0f);
    br.y = std::clamp(br.y, 0.0f, screen.y - 1.0f);

    float bw = br.x - tl.x;
    float bh = br.y - tl.y;
    if (bw < 2.0f || bh < 2.0f ||
        bw > screen.x * 1.5f || bh > screen.y * 1.5f)
        return;

    // fix the snapping
    tl.x = floorf(tl.x) + 0.5f;
    tl.y = floorf(tl.y) + 0.5f;
    br.x = floorf(br.x) + 0.5f;
    br.y = floorf(br.y) + 0.5f;

    float border_thickness = visuals.box_thick;
    border_thickness = std::clamp(border_thickness, 1.0f, 6.0f);

    ImU32 border_color = isPlayerVisible
        ? IM_COL32((int)(colorss.x * 255), (int)(colorss.y * 255), (int)(colorss.z * 255), (int)(colorss.w * 255))
        : IM_COL32(255, 0, 0, 255);

    ImU32 tint = isPlayerVisible
        ? IM_COL32(255, 255, 255, 255)
        : IM_COL32(255, 255, 255, 120);

    // Render the selected image
    draw_list->AddImage(
        current_texture,
        tl,
        br,
        ImVec2(0.0f, 0.0f),
        ImVec2(1.0f, 1.0f),
        tint
    );

    // optional border so it behaves exactly like a box
    draw_list->AddRect(
        tl,
        br,
        border_color,
        0.0f,
        0,
        border_thickness
    );
}


#include "../../../utilities/str_obfuscate.hpp"
#include <unordered_map>
// Optimized string drawing with reduced allocations
inline void DrawString3(float fontSize, int x, int y, ImVec4 color, bool bCenter, bool stroke, const char* pText, ...)
{
	static char buf[512];  // Static buffer to avoid repeated allocations
	va_list va_alist;
	va_start(va_alist, pText);
	_vsnprintf_s(buf, sizeof(buf), _TRUNCATE, pText, va_alist);
	va_end(va_alist);
	
	ImU32 converted_color = ImGui::ColorConvertFloat4ToU32(color);
	ImFont* font = ImGui::GetIO().FontDefault ? ImGui::GetIO().FontDefault : ImGui::GetFont();
	
	if (bCenter)
	{
		ImVec2 textSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, buf);
		x = x - textSize.x / 4;
		y = y - textSize.y;
	}
	
	// Optimized stroke drawing - only draw if needed
	if (stroke)
	{
		const ImU32 stroke_color = IM_COL32(0, 0, 0, 255);
		ImGui::GetBackgroundDrawList()->AddText(font, fontSize, ImVec2(x - 1, y - 1), stroke_color, buf);
		ImGui::GetBackgroundDrawList()->AddText(font, fontSize, ImVec2(x + 1, y - 1), stroke_color, buf);
		ImGui::GetBackgroundDrawList()->AddText(font, fontSize, ImVec2(x - 1, y + 1), stroke_color, buf);
		ImGui::GetBackgroundDrawList()->AddText(font, fontSize, ImVec2(x + 1, y + 1), stroke_color, buf);
	}
	
	ImGui::GetBackgroundDrawList()->AddText(font, fontSize, ImVec2(x, y), converted_color, buf);
}

inline void DrawString2(float fontSize, int x, int y, ImColor color, bool bCenter, bool stroke, const char* pText, ...)
{
	ImFont* font = ImGui::GetIO().FontDefault ? ImGui::GetIO().FontDefault : ImGui::GetFont();
	
	va_list va_alist;
	char buf[1024] = { 0 };
	va_start(va_alist, pText);
	_vsnprintf_s(buf, sizeof(buf), pText, va_alist);
	va_end(va_alist);
	
	if (bCenter)
	{
		ImVec2 textSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, buf);
		x = x - textSize.x / 4;
		y = y - textSize.y;
	}
	
	if (stroke)
	{
		ImGui::GetBackgroundDrawList()->AddText(font, fontSize, ImVec2(x, y - 1), ImGui::ColorConvertFloat4ToU32(ImVec4(1 / 255.0, 1 / 255.0, 1 / 255.0, 255 / 255.0)), buf);
		ImGui::GetBackgroundDrawList()->AddText(font, fontSize, ImVec2(x, y + 1), ImGui::ColorConvertFloat4ToU32(ImVec4(1 / 255.0, 1 / 255.0, 1 / 255.0, 255 / 255.0)), buf);
		ImGui::GetBackgroundDrawList()->AddText(font, fontSize, ImVec2(x - 1, y), ImGui::ColorConvertFloat4ToU32(ImVec4(1 / 255.0, 1 / 255.0, 1 / 255.0, 255 / 255.0)), buf);
		ImGui::GetBackgroundDrawList()->AddText(font, fontSize, ImVec2(x + 1, y), ImGui::ColorConvertFloat4ToU32(ImVec4(1 / 255.0, 1 / 255.0, 1 / 255.0, 255 / 255.0)), buf);
	}
	
	ImGui::GetBackgroundDrawList()->AddText(font, fontSize, ImVec2(x, y), color, buf);
}



ImVec2 ScreenCenter(globals.ScreenWidth / 2, globals.ScreenHeight / 2);

enum MenuTab : int
{
	Combat = 0,
	Visuals = 1,
	Radarington = 2,
	Misc = 3,
	Settings = 4
};
MenuTab eMenuTab;



std::mutex base;

float TargetDistance = FLT_MAX;
uintptr_t TargetEntity = NULL;
uintptr_t DesyncTargetEntity = NULL;

void render_elements()
{
	// Left side watermark
	{
		static ImVec2 content_size, pos, current_pos;
		static float alpha = 1.0f;
		
		// Animation for watermark
		alpha = alpha + (1.0f - alpha) * 0.1f;
		
		// Get current time
		time_t now = time(0);
		struct tm timeinfo;
		localtime_s(&timeinfo, &now);
		char time_str[64];
		strftime(time_str, sizeof(time_str), "%H:%M:%S", &timeinfo);
		
		// Get FPS
		float fps = ImGui::GetIO().Framerate;
		char fps_str[32];
		sprintf_s(fps_str, "%.0f FPS", fps);
		
		// Position on left side
		pos = ImVec2(10, 10);
		current_pos.x = current_pos.x + (pos.x - current_pos.x) * 0.1f;
		current_pos.y = current_pos.y + (pos.y - current_pos.y) * 0.1f;
		
		// Set watermark style
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(24.0f/255.0f, 24.0f/255.0f, 24.0f/255.0f, 200.0f/255.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(15, 10));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(15, 5));
		
		ImGui::SetNextWindowPos(current_pos);
		ImGui::Begin("left_watermark", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing);
		{
			ImDrawList* draw_list = ImGui::GetWindowDrawList();
			

			
			// Time text
			//ImGui::SameLine();
		//	ImGui::TextColored(ImVec4(170.0f / 255.0f, 100.0f / 255.0f, 255.0f / 255.0f, 1.0f), "%s", time_str);
			
			// Separator dot
			ImGui::SameLine();
			ImVec2 cursor_pos = ImGui::GetCursorScreenPos();
			draw_list->AddCircleFilled(ImVec2(cursor_pos.x - 8, cursor_pos.y + 8), 2.0f, ImColor(192, 203, 229, 150));
			
			// FPS text
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "%s", fps_str);
			
			content_size = ImVec2(ImGui::GetContentRegionMax().x + 20, ImGui::GetContentRegionMax().y + 20);
		}
		ImGui::End();
		
		// Pop styles
		ImGui::PopStyleVar(4);
		ImGui::PopStyleColor(1);
		ImGui::PopStyleVar();
	}
	



	if (aimbot.drawfov)
	{
		// Skip game read during load pause or when menu open - driver reads block and cause low FPS/flicker
		if ((g_read_pause_until && GetTickCount() < g_read_pause_until) || menus.ShowMenu)
			aimbot.fov_drawdistance = aimbot.fovsize;
		else {
			bool isTargeting = read<bool>((uintptr_t)CachePointers.AcknownledgedPawn + 0x1e34);
			aimbot.fov_drawdistance = isTargeting ? aimbot.ADS_Fov_Size : aimbot.fovsize;
		}

		ImVec2 center = ImVec2(globals.ScreenWidth / 2, globals.ScreenHeight / 2);
		float radius = aimbot.fovsize;
		
		// FOV Filled - dark transparent black fill inside circle
		if (aimbot.predictiondot) {
			ImGui::GetBackgroundDrawList()->AddCircleFilled(center, radius, ImColor(0, 0, 0, (unsigned char)120), 64); // Dark black, semi-transparent
		}
		
		// FOV RGB - rainbow effect (completely replace outline)
		if (aimbot.show_fov_rgb) {
			// Animated rainbow circle - draw full circle
			static float rainbow_time = 0.0f;
			rainbow_time += 0.005f;
			if (rainbow_time > 1.0f) rainbow_time = 0.0f;
			
			// Draw rainbow circle with gradient - complete circle
			int segments = 256; // More segments for smoother gradient
			for (int i = 0; i < segments; i++) {
				float angle1 = (i / (float)segments) * 2.0f * 3.14159f;
				float angle2 = ((i + 1) / (float)segments) * 2.0f * 3.14159f;
				float hue = (i / (float)segments + rainbow_time);
				if (hue > 1.0f) hue -= 1.0f;
				
				ImVec2 p1 = ImVec2(center.x + cosf(angle1) * radius, center.y + sinf(angle1) * radius);
				ImVec2 p2 = ImVec2(center.x + cosf(angle2) * radius, center.y + sinf(angle2) * radius);
				
				// HSV to RGB conversion
				float c = 1.0f;
				float x = c * (1.0f - fabsf(fmodf(hue * 6.0f, 2.0f) - 1.0f));
				
				float r, g, b;
				if (hue < 1.0f / 6.0f) { r = c; g = x; b = 0; }
				else if (hue < 2.0f / 6.0f) { r = x; g = c; b = 0; }
				else if (hue < 3.0f / 6.0f) { r = 0; g = c; b = x; }
				else if (hue < 4.0f / 6.0f) { r = 0; g = x; b = c; }
				else if (hue < 5.0f / 6.0f) { r = x; g = 0; b = c; }
				else { r = c; g = 0; b = x; }
				
				ImGui::GetBackgroundDrawList()->AddLine(p1, p2, ImColor((unsigned char)(r * 255), (unsigned char)(g * 255), (unsigned char)(b * 255), (unsigned char)255), 2.5f);
			}
		}
		else {
			// Normal FOV circle outline (only when RGB is disabled)
			ImGui::GetBackgroundDrawList()->AddCircle(center, radius, ImColor(0, 0, 0, 255), 64, 2.5);
			ImGui::GetBackgroundDrawList()->AddCircle(center, radius, ImColor(255, 255, 255, 255), 64, 1);
		}
	}
}

static auto RootC(uintptr_t actor) {
	return read<uintptr_t>(actor + offsets::RootComponent);
}

static auto GetLocation(uintptr_t actor) {
	return read<fvector>(RootC(actor) + offsets::RelativeLocation);
}


	void CacheLevels()
{
	// Run well below render thread - CRITICAL: prevents monopolizing driver and 1-6 FPS / ESP flicker
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_LOWEST);

	// Conservative limits to prevent PC freeze - UE memory is unstable during level load
	static constexpr DWORD MAX_LEVELS = 32;
	static constexpr DWORD MAX_ACTORS_PER_LEVEL = 4000;
	static constexpr size_t MAX_CACHED_ITEMS = 2000;
	static constexpr DWORD ITERATION_MS_BUDGET = 10;   // Small batches - yield often so actorloop gets driver (fixes sustained 1-6 FPS)
	static constexpr DWORD LOAD_COOLDOWN_MS = 8000;    // When invalid data seen, pause 8s (map transition)
	static constexpr DWORD UWORLD_CHANGE_COOLDOWN = 10000;  // When UWorld changes, pause 10s
	static constexpr DWORD READ_PAUSE_MS = 60000;     // Global read pause on transition - no driver reads (loading freeze fix)
	static uintptr_t last_uworld = 0;
	static DWORD cooldown_until = 0;
	for (;;)
	{
		// Global read pause - no driver reads while game is loading (prevents page-fault freeze)
		if (g_read_pause_until && GetTickCount() < g_read_pause_until) {
			Sleep(1000);
			continue;
		}
		// Menu open = prioritize menu responsiveness - skip CacheLevels entirely (fixes menu/ESP priority)
		if (menus.ShowMenu) {
			Sleep(450 + (GetTickCount64() % 80));  /* 450-529ms - no driver contention when menu visible */
			continue;
		}
		// Menu just closed: back off so actorloop gets driver first - prevents 1-6 FPS spike after close
		if (g_menu_closed_at && (GetTickCount() - g_menu_closed_at) < 600) {
			Sleep(350 + (GetTickCount64() % 100));  /* 350-449ms - let actorloop ramp up */
			continue;
		}
		if (!world.pickups && !world.chests) {
			Sleep(130 + (GetTickCount64() % 45));  /* Jitter: 130-174ms */
			continue;
		}
		if (!CachePointers.UWorld) {
			Sleep(180 + (GetTickCount64() % 55));  /* Jitter: 180-234ms */
			continue;
		}
		// UWorld change = map transition - pause CacheLevels + global reads to avoid freeze
		if (CachePointers.UWorld != last_uworld) {
			last_uworld = CachePointers.UWorld;
			cooldown_until = GetTickCount() + UWORLD_CHANGE_COOLDOWN;
			g_read_pause_until = GetTickCount() + READ_PAUSE_MS;
		}
		if (GetTickCount() < cooldown_until) {
			Sleep(400 + (GetTickCount64() % 80));  /* Jitter: 400-479ms */
			continue;
		}
		// Loading-state: player not spawned or game state invalid
		if (!CachePointers.AcknownledgedPawn || !CachePointers.GameState) {
			Sleep(480 + (GetTickCount64() % 180));  /* Jitter: 480-659ms */
			continue;
		}
		
		std::vector<item> cached_items;
		cached_items.reserve(256);
		uintptr_t levels_ptr = read<uintptr_t>(CachePointers.UWorld + world_offsets::levels);
		if (!levels_ptr) continue;

		DWORD level_count = read<DWORD>(CachePointers.UWorld + (world_offsets::levels + sizeof(PVOID)));
		if (level_count == 0 || level_count > MAX_LEVELS) {
			cooldown_until = GetTickCount() + LOAD_COOLDOWN_MS;
			g_read_pause_until = GetTickCount() + READ_PAUSE_MS;
			item_pawns.clear();  // Clear stale data during load
			continue;
		}
		
		DWORD iteration_start = GetTickCount();
		for (DWORD level_idx = 0; level_idx < level_count; ++level_idx)
		{
			if ((GetTickCount() - iteration_start) > ITERATION_MS_BUDGET) break;
			if (cached_items.size() >= MAX_CACHED_ITEMS) break;
			// CRITICAL: Longer yield to release mutex and allow rendering thread to access driver
			if ((level_idx & 1) == 0) Sleep(5);  // Yield every 2 levels - allows rendering thread to get mutex

			uintptr_t level_ptr = read<uintptr_t>(levels_ptr + (level_idx * sizeof(uintptr_t)));
			if (!level_ptr) continue;

			uintptr_t actors_ptr = read<uintptr_t>(level_ptr + world_offsets::aactor);
			if (!actors_ptr) continue;

			DWORD actor_count = read<DWORD>(level_ptr + (world_offsets::aactor + sizeof(PVOID)));
			if (actor_count == 0 || actor_count > MAX_ACTORS_PER_LEVEL) {
				cooldown_until = GetTickCount() + LOAD_COOLDOWN_MS;
				g_read_pause_until = GetTickCount() + READ_PAUSE_MS;
				item_pawns.clear();
				break;
			}

			for (DWORD actor_idx = 0; actor_idx < actor_count && cached_items.size() < MAX_CACHED_ITEMS; ++actor_idx)
			{
				if ((GetTickCount() - iteration_start) > ITERATION_MS_BUDGET) break;
				// CRITICAL: Yield between reads to release mutex - allows rendering thread priority
				if ((actor_idx & 2) == 0) Sleep(5);  // Yield every 2 actors - releases mutex for rendering thread
				uintptr_t actor_ptr = read<uintptr_t>(actors_ptr + (actor_idx * sizeof(uintptr_t)));
				if (!actor_ptr) continue;

				fvector position = GetLocation(actor_ptr);
				float distance = camera_postion.location.distance(position) / 100.0f;

				if (distance <= 500.0f)
				{
					item new_item{};
					new_item.Actor = actor_ptr;
					new_item.isPickup = (world.pickups || world.chests);
					new_item.distance = distance;
					cached_items.push_back(new_item);
				}
			}
		}

		item_pawns.clear();
		item_pawns = std::move(cached_items);
		// Increased sleep to reduce driver contention - ESP needs driver priority
		Sleep(800 + (GetTickCount64() % 200));  /* 800-999ms - longer pause so actorloop gets driver (fixes FPS drop) */
	}
}

enum class EFortRarity : uint8_t
{
	EFortRarity__Common = 0,
	EFortRarity__Uncommon = 1,
	EFortRarity__Rare = 2,
	EFortRarity__Epic = 3,
	EFortRarity__Legendary = 4,
	EFortRarity__Mythic = 5,
	EFortRarity__Transcendent = 6,
	EFortRarity__Unattainable = 7,
	EFortRarity__NumRarityValues = 8,
};


void draw_cornered_box(int x, int y, int w, int h, const ImColor color, ImColor fill_color, int thickness)
{
	int cornerBoxLength = w / 3;
	int cornerBoxHeight = h / 5;
	ImGui::GetForegroundDrawList()->AddLine(ImVec2(x + cornerBoxLength, y), ImVec2(x, y), (color), thickness);
	ImGui::GetForegroundDrawList()->AddLine(ImVec2(x, y + cornerBoxHeight), ImVec2(x, y), (color), thickness);
	ImGui::GetForegroundDrawList()->AddLine(ImVec2((x + w) - cornerBoxLength, y), ImVec2(x + w, y), (color), thickness);
	ImGui::GetForegroundDrawList()->AddLine(ImVec2((x + w), y + cornerBoxHeight), ImVec2(x + w, y), (color), thickness);
	ImGui::GetForegroundDrawList()->AddLine(ImVec2(x + cornerBoxLength, (y + h)), ImVec2(x, y + h), (color), thickness);
	ImGui::GetForegroundDrawList()->AddLine(ImVec2(x, (y + h) - cornerBoxHeight), ImVec2(x, y + h), (color), thickness);
	ImGui::GetForegroundDrawList()->AddLine(ImVec2((x + w) - cornerBoxLength, (y + h)), ImVec2(x + w, y + h), (color), thickness);
	ImGui::GetForegroundDrawList()->AddLine(ImVec2((x + w), (y + h) - cornerBoxHeight), ImVec2(x + w, y + h), (color), thickness);
}

inline ImVec4 get_loot_color(EFortRarity tier) {
	if (tier == EFortRarity::EFortRarity__Common)
		return ImVec4(123 / 255.0f, 123 / 255.0f, 123 / 255.0f, 1.f);
	else if (tier == EFortRarity::EFortRarity__Uncommon)
		return ImVec4(58 / 255.0f, 121 / 255.0f, 19 / 255.0f, 1.f);
	else if (tier == EFortRarity::EFortRarity__Rare)
		return ImVec4(18 / 255.0f, 88 / 255.0f, 162 / 255.0f, 1.f);
	else if (tier == EFortRarity::EFortRarity__Epic)
		return ImVec4(189 / 255.0f, 63 / 255.0f, 250 / 255.0f, 1.f);
	else if (tier == EFortRarity::EFortRarity__Legendary)
		return ImVec4(255 / 255.0f, 118 / 255.0f, 5 / 255.0f, 1.f);
	else if (tier == EFortRarity::EFortRarity__Mythic)
		return ImVec4(220 / 255.0f, 160 / 255.0f, 30 / 255.0f, 1.f);
	else if (tier == EFortRarity::EFortRarity__Transcendent)
		return ImVec4(0 / 255.0f, 225 / 255.0f, 252 / 255.0f, 1.f);

	return ImVec4(123 / 255.0f, 123 / 255.0f, 123 / 255.0f, 1.f);
}


void radar_range(float* x, float* y, float range)
{
	if (fabs((*x)) > range || fabs((*y)) > range)
	{
		if ((*y) > (*x))
		{
			if ((*y) > -(*x))
			{
				(*x) = range * (*x) / (*y);
				(*y) = range;
			}
			else
			{
				(*y) = -range * (*y) / (*x);
				(*x) = -range;
			}
		}
		else
		{
			if ((*y) > -(*x))
			{
				(*y) = range * (*y) / (*x);
				(*x) = range;
			}
			else
			{
				(*x) = -range * (*x) / (*y);
				(*y) = -range;
			}
		}
	}
}

void project_world_to_radar(fvector vOrigin, int& screenx, int& screeny)
{
	static const ImVec2 kDrawPos(10.0f, 60.0f);
	static const ImVec2 kDrawSize(250.0f, 250.0f);
	static const float kRange = 50000.0f;

	fvector vAngle = camera_postion.rotation;
	float fYaw = vAngle.y * M_PI / 180.0f;
	
	float dx = vOrigin.x - camera_postion.location.x;
	float dy = vOrigin.y - camera_postion.location.y;
	
	float sin_yaw = sinf(fYaw);
	float cos_yaw = cosf(fYaw);
	
	float x = -(dy * (-cos_yaw) + dx * sin_yaw);
	float y = dx * (-cos_yaw) - dy * sin_yaw;

	radar_range(&x, &y, kRange);

	screenx = static_cast<int>(kDrawPos.x + (kDrawSize.x / 2.0f) + (x / kRange * kDrawSize.x));
	screeny = static_cast<int>(kDrawPos.y + (kDrawSize.y / 2.0f) + (y / kRange * kDrawSize.y));

	// Clamp to radar bounds
	int minX = static_cast<int>(kDrawPos.x);
	int maxX = static_cast<int>(kDrawPos.x + kDrawSize.x);
	int minY = static_cast<int>(kDrawPos.y);
	int maxY = static_cast<int>(kDrawPos.y + kDrawSize.y);
	
	screenx = std::clamp(screenx, minX, maxX);
	screeny = std::clamp(screeny, minY, maxY);
}

void leveldrawing()
{
	auto levelListCopy = item_pawns;

	for (auto entity : levelListCopy)
	{
		if (entity.Actor)
		{
			if (world.pickups && strstr(entity.Name.c_str(), ("FortPickupAthena")) or strstr(entity.Name.c_str(), ("Fort_Pickup_Creative_C")))
			{
				if (entity.distance <= world.loot_distace)
				{
					auto definition = read<uint64_t>(entity.Actor + 0x348 + 0x18);

					std::cout << "Def - " << definition << std::endl;

					if ((definition))
					{
						EFortRarity tier = read<EFortRarity>(definition + weapon::Tier);

						ImColor Color, RGBAColor;
						fvector Location = GetLocation(entity.Actor);

						fvector2d ChestPosition = game_helper.ProjectWorldToScreen(Location);

						if (game_helper.IsInScreen(ChestPosition))
						{
							uint64_t ftext_ptr = read<uint64_t>(definition + weapon::ItemName);

							std::cout << "FText Pointer -> " << ftext_ptr << std::endl;

							if ((ftext_ptr))
							{
								uint64_t ftext_data = read<uint64_t>(ftext_ptr + 0x28);
								int ftext_length = read<int>(ftext_ptr + 0x30);
								if (ftext_length > 0 && ftext_length < 50) {
									wchar_t* ftext_buf = new wchar_t[ftext_length];
									(ftext_data, ftext_buf, ftext_length * sizeof(wchar_t));
									std::wstring wstr_buf(ftext_buf);
									std::string WeaponName = std::string(wstr_buf.begin(), wstr_buf.end());
									delete[] ftext_buf;

									std::string wtf2 = WeaponName + " [" + std::to_string((int)entity.distance) + ("m]");

									ImVec2 text_size = ImGui::CalcTextSize(wtf2.c_str());

									if (tier == EFortRarity::EFortRarity__Uncommon and world.uncommon)
									{
										DrawString3(13, ChestPosition.x - (text_size.x / 2), ChestPosition.y - 18, get_loot_color(tier), false, false, wtf2.c_str());
									}
									else if (tier == EFortRarity::EFortRarity__Common and world.common)
									{
										DrawString3(13, ChestPosition.x - (text_size.x / 2), ChestPosition.y - 18, get_loot_color(tier), false, false, wtf2.c_str());
									}
									else if (tier == EFortRarity::EFortRarity__Rare and world.rare)
									{
										DrawString3(13, ChestPosition.x - (text_size.x / 2), ChestPosition.y - 18, get_loot_color(tier), false, false, wtf2.c_str());
									}
									else if (tier == EFortRarity::EFortRarity__Epic and world.epic)
									{
										DrawString3(13, ChestPosition.x - (text_size.x / 2), ChestPosition.y - 18, get_loot_color(tier), false, false, wtf2.c_str());
									}
									else if (tier == EFortRarity::EFortRarity__Legendary and world.legendary)
									{
										DrawString3(13, ChestPosition.x - (text_size.x / 2), ChestPosition.y - 18, get_loot_color(tier), false, false, wtf2.c_str());
									}
									else if (tier == EFortRarity::EFortRarity__Mythic and world.mythic)
									{
										DrawString3(13, ChestPosition.x - (text_size.x / 2), ChestPosition.y - 18, get_loot_color(tier), false, false, wtf2.c_str());
									}
								}
							}
						}
					}
				}
			}

			if (world.chests && strstr(entity.Name.c_str(), ("Tiered_Chest")))
			{
				if (entity.distance <= world.loot_distace)
				{
					fvector Location = GetLocation(entity.Actor);
					fvector2d Screen = game_helper.ProjectWorldToScreen(Location);

					if (game_helper.IsInScreen(Screen))
					{
						std::string BuiltString = "Chest [" + std::to_string((int)entity.distance) + "]";
						ImVec2 text_size = ImGui::CalcTextSize(BuiltString.c_str());

						DrawString2(13, Screen.x - (text_size.x / 2), Screen.y - 18, ImColor(255, 255, 0), false, false, BuiltString.c_str());
					}
				}
			}
		}
	}
}
inline uint64_t rol64(uint64_t value, unsigned int count)
{
	return (value << count) | (value >> (64 - count));
}

// UWorld decrypt: ~rotl(encrypted ^ 0x913F0DAF, 51)
inline uint64_t decrypt_gworld(uint64_t world)
{
	return offsets::decrypt_UWorld((uintptr_t)world);
}
// Optimized actor loop with caching and reduced overhead
void actorloop()
{
	// When true: skip ALL game reads - driver can block on page faults during load
	bool read_paused = (g_read_pause_until != 0 && GetTickCount() < g_read_pause_until);

	static bool last_menu_open = false;
	const DWORD MENU_CLOSE_GRACE_MS = 300;  // Light path after close - instant close + ramp-up
	const DWORD MENU_CLOSE_RAMP_MS = 800;   // Gradual ramp-up period after grace
	
	// Startup ramp-up: gradually enable ESP after read pause expires (prevents sudden FPS drop)
	const DWORD STARTUP_RAMP_MS = 2000;  // 2 second ramp-up after read pause expires
	if (read_paused) {
		// Still in read pause - no ESP, reset ramp timer
		g_read_pause_expired_at = 0;
	} else if (g_read_pause_expired_at == 0 && g_read_pause_until == 0) {
		// Read pause just expired - start ramp-up timer
		g_read_pause_expired_at = GetTickCount();
	}

	// Menu open: skip heavy work (cache refresh, player loop) - maximizes menu FPS and fixes checkbox flicker
	if (menus.ShowMenu) {
		last_menu_open = true;
		temporary_entity_list.clear();
		{
			std::lock_guard<std::mutex> lock(base);
			entity_list.clear();
		}
		render_elements();
		d3d_swap_chain->Present(visuals.vsync ? 1 : 0, 0);
		return;
	}

	// Menu just closed: grace period - light path so close is instant and we ramp up gradually
	if (last_menu_open) {
		g_menu_closed_at = GetTickCount();
		last_menu_open = false;
	}
	
	// Calculate ramp-up factor: 0.0 (no ESP) -> 1.0 (full ESP) over ramp period
	// Apply both startup ramp (after read pause) and menu close ramp
	float esp_ramp_factor = 1.0f;
	
	// Startup ramp: gradually enable ESP after read pause expires
	if (g_read_pause_expired_at) {
		DWORD time_since_startup = GetTickCount() - g_read_pause_expired_at;
		if (time_since_startup < STARTUP_RAMP_MS) {
			// Ramp-up period: gradually increase ESP load from 0 to 1
			float startup_progress = (float)time_since_startup / (float)STARTUP_RAMP_MS;
			esp_ramp_factor = startup_progress * startup_progress;  // Quadratic easing
		} else {
			// Startup ramp complete - clear timer
			g_read_pause_expired_at = 0;
		}
	}
	
	// Menu close ramp: override startup ramp if menu just closed
	if (g_menu_closed_at) {
		DWORD time_since_close = GetTickCount() - g_menu_closed_at;
		if (time_since_close < MENU_CLOSE_GRACE_MS) {
			// Grace period: no ESP rendering
			temporary_entity_list.clear();
			{
				std::lock_guard<std::mutex> lock(base);
				entity_list.clear();
			}
			render_elements();
			d3d_swap_chain->Present(visuals.vsync ? 1 : 0, 0);
			return;
		} else if (time_since_close < MENU_CLOSE_RAMP_MS) {
			// Ramp-up period: gradually increase ESP load
			float ramp_progress = (float)(time_since_close - MENU_CLOSE_GRACE_MS) / (float)(MENU_CLOSE_RAMP_MS - MENU_CLOSE_GRACE_MS);
			esp_ramp_factor = ramp_progress * ramp_progress;  // Quadratic easing for smoother ramp
		} else {
			// Full ESP after ramp period
			g_menu_closed_at = 0;  // Clear after ramp - no need to keep
		}
	}

	// During read pause: skip ALL game reads, only render UI elements
	if (read_paused) {
		render_elements();
		d3d_swap_chain->Present(visuals.vsync ? 1 : 0, 0);
		return;
	}

	// Cache frequently accessed pointers to reduce redundant reads
	static uintptr_t last_uworld = 0;
		static int cache_frame_counter = 0;
		// Reduce cache refresh rate during ramp-up to reduce driver load
		int cache_refresh_rate = (esp_ramp_factor < 1.0f) ? 3 : 1;  // Every 3 frames during ramp, every frame normally
	
	{
		temporary_entity_list.clear();
		std::lock_guard<std::mutex> lock(base);

		// Optimized pointer caching - only refresh every few frames (skip when read_paused)
		if (!read_paused) {
		cache_frame_counter++;
		if (cache_frame_counter >= cache_refresh_rate || CachePointers.UWorld == 0)
		{
			uintptr_t encrypted_uworld = read<__int64>(virtualaddy + offsets::UWorld);
			uintptr_t uworld = decrypt_gworld(encrypted_uworld);
			uintptr_t gi = (uworld != 0) ? read<__int64>(uworld + offsets::OwningGameInstance) : 0;
			// Only update cache if we got valid pointers (prevents ESP flicker from bad reads)
			bool valid = (uworld != 0 && gi != 0);
			if (valid) {
				CachePointers.UWorld = uworld;
				CachePointers.GameInstance = gi;
				CachePointers.LocalPlayer = read<__int64>(read<__int64>(CachePointers.GameInstance + offsets::LocalPlayers));
				CachePointers.PlayerController = read<__int64>(CachePointers.LocalPlayer + offsets::PlayerController);
				CachePointers.AcknownledgedPawn = read<__int64>(CachePointers.PlayerController + offsets::AcknowledgedPawn);
				CachePointers.Mesh = read<__int64>(CachePointers.AcknownledgedPawn + offsets::Mesh);
				CachePointers.PlayerState = read<__int64>(CachePointers.AcknownledgedPawn + offsets::PlayerState);
				CachePointers.RootComponent = read<__int64>(CachePointers.AcknownledgedPawn + offsets::RootComponent);
				CachePointers.GameState = read<__int64>(CachePointers.UWorld + offsets::GameState);
				CachePointers.PlayerArray = read<__int64>(CachePointers.GameState + offsets::PlayerArray);
				CachePointers.PlayerArraySize = read<int>(CachePointers.GameState + (offsets::PlayerArray + sizeof(uintptr_t)));
				CachePointers.LocalWeapon = read<__int64>(CachePointers.AcknownledgedPawn + weapon::CurrentWeapon);
				cache_frame_counter = 0;
			}
		}
		}
	}

    if (!read_paused && settings::exploits::FOVChanger)
    {
        static bool initialized = false;
        static float originalFOV = 0.0f;
        static float originalBaseFOV = 0.0f;

        uintptr_t PlayerCameraManager = read<uintptr_t>(CachePointers.PlayerController + offsets::PlayerCameraManager);
        if (PlayerCameraManager) {
            if (!initialized) {
                originalFOV = read<float>(PlayerCameraManager + offsets::DefaultFOV + 0x4);
                originalBaseFOV = read<float>(PlayerCameraManager + offsets::BaseFOV);
                initialized = true;
            }
            write<float>(PlayerCameraManager + offsets::DefaultFOV + 0x4, settings::exploits::FOVVALUE);
            write<float>(PlayerCameraManager + offsets::BaseFOV, settings::exploits::FOVVALUE);
        }
    }
    else if (!read_paused)
    {
        static bool initialized = false;
        if (initialized) {
            static float originalFOV = 0.0f;
            static float originalBaseFOV = 0.0f;
            uintptr_t PlayerCameraManager = read<uintptr_t>(CachePointers.PlayerController + offsets::PlayerCameraManager);
            if (PlayerCameraManager) {
			    write<float>(PlayerCameraManager + offsets::DefaultFOV + 0x4, originalFOV);
			    write<float>(PlayerCameraManager + offsets::BaseFOV, originalBaseFOV);
			    initialized = false;
            }
        }
    }

    // Spinbot (skip when read_paused - no game reads)
    static bool spinbot_initialized = false;
    static fvector original_rotation;
    static float current_spin_angle = 0.0f;
    
    if (!read_paused && settings::exploits::Spinbot)
    {
        auto mesh = read<uint64_t>(CachePointers.AcknownledgedPawn + offsets::Mesh);
        if (mesh) {
            if (!spinbot_initialized) {
                original_rotation = read<fvector>(mesh + 0x150);
                current_spin_angle = original_rotation.y;
                spinbot_initialized = true;
            }

            current_spin_angle += settings::exploits::SpinbotSpeed;
            if (current_spin_angle >= 360.0f)
                current_spin_angle -= 360.0f;

            write<fvector>(mesh + 0x150, fvector(original_rotation.x, current_spin_angle, original_rotation.z));
        }
    }
    else if (!read_paused)
    {
        if (spinbot_initialized)
        {
            auto mesh = read<uint64_t>(CachePointers.AcknownledgedPawn + offsets::Mesh);
            if (mesh) {
                write<fvector>(mesh + 0x150, original_rotation);
            }
            spinbot_initialized = false;
        }
    }

    // Carfly (skip when read_paused)
    if (!read_paused && settings::exploits::Carfly)
    {
        uintptr_t CurrentVehicle = read<DWORD_PTR>(CachePointers.AcknownledgedPawn + offsets::CurrentVehicle); 

        if (CurrentVehicle && GetAsyncKeyState(VK_SPACE))
        {
            write<bool>(CurrentVehicle + 0x8A2, false); 
        }
        else {
            write<bool>(CurrentVehicle + 0x8A2, true); 
        }
    }

    // SpeedHack (sets CurrentMovementStyle = 5 => FLY) - skip when read_paused
    if (!read_paused && settings::exploits::SpeedHack) {
        if ((GetAsyncKeyState(0x57) & 0x8000) && CachePointers.AcknownledgedPawn) { // 0x57 = 'W'
            write<uint8_t>(uintptr_t(CachePointers.AcknownledgedPawn) + 0x843, 5);
        }
    }


	if (!read_paused && CachePointers.AcknownledgedPawn != 0)
	{
		CachePointers.RootComponent = read<uintptr_t>(CachePointers.AcknownledgedPawn + offsets::RootComponent);
		RelativeLocation = read<fvector>(CachePointers.RootComponent + offsets::RelativeLocation);
		CachePointers.PlayerState = read<uintptr_t>(CachePointers.AcknownledgedPawn + offsets::PlayerState);
		CachePointers.TeamIndex = read<int>(CachePointers.PlayerState + offsets::Team_Index);
	}

	TargetDistance = FLT_MAX;
	TargetEntity = NULL;
	DesyncTargetEntity = NULL;
	
	// Counter variables for player tracking
	static int render_count_total = 0;
	static int visible_count_total = 0;
	render_count_total = 0;
	visible_count_total = 0;

	// Cache camera once per frame - used by ProjectWorldToScreen (avoids redundant driver reads)
	if (!read_paused && CachePointers.UWorld && CachePointers.PlayerController) {
		camera_postion = game_helper.get_camera();
	}

	render_elements();

	// Clear stale bone caches periodically to prevent memory growth
	// This prevents memory leaks from stale mesh pointers
	static DWORD last_bone_cache_clear = 0;
	DWORD now = GetTickCount();
	if (now - last_bone_cache_clear > 5000) {  // Every 5 seconds
		gamehelper_t::cached_bone_arrays.clear();
		gamehelper_t::cached_component_to_world.clear();
		gamehelper_t::cache_frame.clear();
		last_bone_cache_clear = now;
	}

	// Bounds check: during game load PlayerArraySize/PlayerArray can be garbage - prevent PC freeze
	// Skip entire player loop when read_paused
	int safe_player_count = read_paused ? 0 : CachePointers.PlayerArraySize;
	if (safe_player_count <= 0 || safe_player_count > 150 || !CachePointers.PlayerArray || !CachePointers.GameState)
		safe_player_count = 0;
	// Per-frame cap: fewer players = less driver load = better FPS (fixes sustained 1-6 FPS)
	// Apply ramp-up factor when menu just closed to gradually increase ESP load
	const int MAX_PLAYERS_PER_FRAME = 35;
	int max_players_this_frame = (int)(MAX_PLAYERS_PER_FRAME * esp_ramp_factor);
	if (max_players_this_frame < 1) max_players_this_frame = 1;  // Always process at least 1 player if ESP enabled
	if (safe_player_count > max_players_this_frame) safe_player_count = max_players_this_frame;
	for (int i = 0; i < safe_player_count; ++i)
	{
		if ((i & 7) == 0 && i > 0) Sleep(0);  // Yield every 8 players - prevents freeze in Reload (100 players)
		uintptr_t PlayerState = read<uintptr_t>(CachePointers.PlayerArray + (i * sizeof(uintptr_t)));
		uintptr_t CurrentActor = read<uintptr_t>(PlayerState + offsets::PawnPrivate);
		//fvector bottom3d = game_helper.GetBoneLocation(offsets::Mesh, 0);

		if (CurrentActor == CachePointers.AcknownledgedPawn) continue;

		uintptr_t skeletalmesh = read<uintptr_t>(CurrentActor + offsets::Mesh);
		if (!skeletalmesh) continue;

		// Early exit checks BEFORE expensive bone reads
		if (read<char>(CurrentActor + 0x758) >> 3) continue;

		if (visuals.ignore_dying)
		{
			auto is_despawning = (read<char>(CurrentActor + 0x93a) >> 3); //0x93a
			if (is_despawning) continue;
		}
		if (visuals.teamcheck) {
			int player_team_id = read<int>(PlayerState + offsets::Team_Index);
			if (player_team_id == CachePointers.TeamIndex) continue;
		}

		// Increment render count (players being processed/rendered)
		render_count_total++;

		// Read base bone once - reuse for root_bone (same bone index 0)
		fvector base_bone = game_helper.GetBoneLocation(skeletalmesh, 0);
		if (base_bone.x == 0 && base_bone.y == 0 && base_bone.z == 0) continue;

		uintptr_t SkeletalMesh = skeletalmesh;

		// Reuse base_bone for root_bone (same bone index 0) - saves 3 reads
		fvector root_bone = base_bone;
		fvector2d root_box1 = game_helper.ProjectWorldToScreen(fvector(root_bone.x, root_bone.y, root_bone.z - 15));

		fvector head_bone = game_helper.GetBoneLocation(SkeletalMesh, 110);
		fvector2d head_box = game_helper.ProjectWorldToScreen(fvector(head_bone.x, head_bone.y, head_bone.z + 15));
		fvector2d head = game_helper.ProjectWorldToScreen(head_bone);
		fvector2d root = game_helper.ProjectWorldToScreen(root_bone);

		float box_height = abs(head.y - root_box1.y);
		float box_width = box_height * 0.30f;
		float distance = camera_postion.location.distance(root_bone) / 100;

		float CornerHeight = abs(head_box.y - root.y);
		float CornerWidth = CornerHeight * visuals.corner_width;

		int bottom_offset = 0;
		int top_offset = 0;

		uintptr_t pawn_private1 = read<uintptr_t>(offsets::PlayerState + 0x320);

		if (distance > visuals.renderDistance && CachePointers.AcknownledgedPawn) continue;

		static ImColor box_color;
		static ImColor box_filled;
		static ImColor cornerFIlled;

		if (game_helper.IsEnemyVisible(SkeletalMesh))
		{
			box_color = ImGui::GetColorU32({ vscolors.box_visible[0], vscolors.box_visible[1], vscolors.box_visible[2],  1.0f });
			box_filled = ImGui::GetColorU32({ vscolors.box_filled_visible[0], vscolors.box_filled_visible[1], vscolors.box_filled_visible[2], vscolors.box_filled_visible[3] });
			// Increment visible counter
			visible_count_total++;
		}
		else
		{
			box_color = ImGui::GetColorU32({ vscolors.box_invisible[0], vscolors.box_invisible[1], vscolors.box_invisible[2],  1.0f });
			box_filled = ImGui::GetColorU32({ vscolors.box_filled_invisible[0], vscolors.box_filled_invisible[1], vscolors.box_filled_invisible[2],  vscolors.box_filled_invisible[3] });
		}

		// Optimized aimbot targeting - fast and simple
		if (aimbot.enable)
		{
			const float screen_center_x = globals.ScreenWidth / 2.0f;
			const float screen_center_y = globals.ScreenHeight / 2.0f;
			const float dx = head.x - screen_center_x;
			const float dy = head.y - screen_center_y;
			const float dist_sq = dx * dx + dy * dy;
			const float fov_sq = static_cast<float>(aimbot.fovsize * aimbot.fovsize);
			
			if (dist_sq < fov_sq && dist_sq < (TargetDistance * TargetDistance))
			{
				if (!aimbot.vischeck || game_helper.IsEnemyVisible(SkeletalMesh))
				{
					TargetDistance = sqrtf(dist_sq);
					TargetEntity = CurrentActor;
				}
			}
		}
		// Simplified radar drawing for better performance
		if (visuals.rRadar)
		{
			const ImVec2 radar_pos = { 10.0f, 60.0f };
			const ImVec2 radar_size = { 250.0f, 250.0f };
			const ImVec2 center = { radar_pos.x + radar_size.x * 0.5f, radar_pos.y + radar_size.y * 0.5f };
			const float radius = (radar_size.x < radar_size.y ? radar_size.x : radar_size.y) * 0.5f - 4.0f;
			
			ImGui::SetNextWindowPos(radar_pos);
			ImGui::SetNextWindowSize(radar_size);

			ImGui::Begin(OBF_STR("##radar").c_str(), nullptr,
				ImGuiWindowFlags_NoTitleBar |
				ImGuiWindowFlags_NoScrollbar |
				ImGuiWindowFlags_NoScrollWithMouse |
				ImGuiWindowFlags_NoResize |
				ImGuiWindowFlags_NoBackground);

			ImDrawList* draw_list = ImGui::GetWindowDrawList();

			// Simple radar drawing
			draw_list->AddCircleFilled(center, radius, IM_COL32(0, 0, 0, 200));
			draw_list->AddCircle(center, radius, IM_COL32(255, 255, 255, 255), 0, 1.0f);

			// Crosshairs
			const ImU32 crosshair_color = IM_COL32(24, 24, 24, 255);
			
			// Vertical line
			draw_list->AddLine(ImVec2(center.x, radar_pos.y), ImVec2(center.x, radar_pos.y + radar_size.y), crosshair_color, 1.0f);
			
			// Horizontal line
			draw_list->AddLine(ImVec2(radar_pos.x, center.y), ImVec2(radar_pos.x + radar_size.x, center.y), crosshair_color, 1.0f);

			// Player position
			int ScreenX = 0, ScreenY = 0;
			project_world_to_radar(root_bone, ScreenX, ScreenY);

			float dx = static_cast<float>(ScreenX) - center.x;
			float dy = static_cast<float>(ScreenY) - center.y;
			float dist_sq = dx * dx + dy * dy;
			
			if (dist_sq > (radius - 2.0f) * (radius - 2.0f))
			{
				float scale = (radius - 2.0f) / sqrtf(dist_sq);
				dx *= scale;
				dy *= scale;
			}

			// Player dot
			draw_list->AddCircleFilled(ImVec2(center.x + dx, center.y + dy), 2.0f, IM_COL32(255, 50, 50, 255));

			ImGui::End();
		}


// Optimized Box Drawing System - Reduced calculations and allocations
        if (visuals.box && visuals.enable)
		{
			// Pre-calculate box dimensions once
			const float halfWidth = box_width / 1.5f;
			const ImVec2 topLeft(head_box.x - halfWidth, head_box.y);
			const ImVec2 bottomRight(root.x + halfWidth, root.y);
			const int boxThickness = visuals.box_thick;
			const ImU32 box_col = box_color;
			const ImU32 fill_col = box_filled;
			const ImU32 outline_col = IM_COL32(0, 0, 0, 255);
			const ImU32 dark_fill = IM_COL32(0, 0, 0, 120);

            switch (settings::visuals::box_type)
            {
            case 0: // Normal Box - Optimized
                // Batch drawing operations to reduce state changes
                if (visuals.filled)
                    ImGui::GetBackgroundDrawList()->AddRectFilled(topLeft, bottomRight, dark_fill);
                
                if (visuals.box_outline)
                    ImGui::GetBackgroundDrawList()->AddRect(topLeft, bottomRight, outline_col, 0.0f, ImDrawCornerFlags_All, boxThickness + 2);
                
                ImGui::GetBackgroundDrawList()->AddRect(topLeft, bottomRight, box_col, 0.0f, ImDrawCornerFlags_All, boxThickness);
                break;

            case 1: // Corner Box - Optimized with fewer calculations
                {
                    const float cornerSize = 20.0f;
                    const ImVec2 topRight(bottomRight.x, topLeft.y);
                    const ImVec2 bottomLeft(topLeft.x, bottomRight.y);
                    
                    // Batch filled drawing
                    if (visuals.corner_box_filled)
                        ImGui::GetBackgroundDrawList()->AddRectFilled(topLeft, bottomRight, dark_fill);
                    
                    // Optimized outline drawing - batch similar operations
                    if (visuals.cornered_outline) {
                        const float thick_outline = boxThickness + 2;
                        // Top corners
                        ImGui::GetBackgroundDrawList()->AddLine(topLeft, ImVec2(topLeft.x + cornerSize, topLeft.y), outline_col, thick_outline);
                        ImGui::GetBackgroundDrawList()->AddLine(topLeft, ImVec2(topLeft.x, topLeft.y + cornerSize), outline_col, thick_outline);
                        ImGui::GetBackgroundDrawList()->AddLine(topRight, ImVec2(topRight.x - cornerSize, topRight.y), outline_col, thick_outline);
                        ImGui::GetBackgroundDrawList()->AddLine(topRight, ImVec2(topRight.x, topRight.y + cornerSize), outline_col, thick_outline);
                        // Bottom corners
                        ImGui::GetBackgroundDrawList()->AddLine(bottomLeft, ImVec2(bottomLeft.x + cornerSize, bottomLeft.y), outline_col, thick_outline);
                        ImGui::GetBackgroundDrawList()->AddLine(bottomLeft, ImVec2(bottomLeft.x, bottomLeft.y - cornerSize), outline_col, thick_outline);
                        ImGui::GetBackgroundDrawList()->AddLine(bottomRight, ImVec2(bottomRight.x - cornerSize, bottomRight.y), outline_col, thick_outline);
                        ImGui::GetBackgroundDrawList()->AddLine(bottomRight, ImVec2(bottomRight.x, bottomRight.y - cornerSize), outline_col, thick_outline);
                    }
                    
                    // Main corner lines
                    ImGui::GetBackgroundDrawList()->AddLine(topLeft, ImVec2(topLeft.x + cornerSize, topLeft.y), box_col, boxThickness);
                    ImGui::GetBackgroundDrawList()->AddLine(topLeft, ImVec2(topLeft.x, topLeft.y + cornerSize), box_col, boxThickness);
                    ImGui::GetBackgroundDrawList()->AddLine(topRight, ImVec2(topRight.x - cornerSize, topRight.y), box_col, boxThickness);
                    ImGui::GetBackgroundDrawList()->AddLine(topRight, ImVec2(topRight.x, topRight.y + cornerSize), box_col, boxThickness);
                    ImGui::GetBackgroundDrawList()->AddLine(bottomLeft, ImVec2(bottomLeft.x + cornerSize, bottomLeft.y), box_col, boxThickness);
                    ImGui::GetBackgroundDrawList()->AddLine(bottomLeft, ImVec2(bottomLeft.x, bottomLeft.y - cornerSize), box_col, boxThickness);
                    ImGui::GetBackgroundDrawList()->AddLine(bottomRight, ImVec2(bottomRight.x - cornerSize, bottomRight.y), box_col, boxThickness);
                    ImGui::GetBackgroundDrawList()->AddLine(bottomRight, ImVec2(bottomRight.x, bottomRight.y - cornerSize), box_col, boxThickness);
                }
                break;

            case 2: // Round Box - Simplified
                if (visuals.filled)
                    ImGui::GetBackgroundDrawList()->AddRectFilled(topLeft, bottomRight, fill_col, 5.0f);
                if (visuals.box_outline)
                    ImGui::GetBackgroundDrawList()->AddRect(topLeft, bottomRight, outline_col, 5.0f, ImDrawCornerFlags_All, boxThickness + 2);
                ImGui::GetBackgroundDrawList()->AddRect(topLeft, bottomRight, box_col, 5.0f, ImDrawCornerFlags_All, boxThickness);
                break;

            case 3: // 3D Box - Optimized
                {
                    const float depth = 20.0f;
                    const ImVec2 topRight(bottomRight.x, topLeft.y);
                    const ImVec2 bottomLeft(topLeft.x, bottomRight.y);
                    const ImVec2 backTopLeft(topLeft.x + depth, topLeft.y - depth);
                    const ImVec2 backBottomRight(bottomRight.x + depth, bottomRight.y - depth);
                    const ImVec2 backTopRight(backBottomRight.x, backTopLeft.y);
                    const ImVec2 backBottomLeft(backTopLeft.x, backBottomRight.y);
                    
                    // Front face
                    ImGui::GetBackgroundDrawList()->AddRect(topLeft, bottomRight, box_col, 0.0f, ImDrawCornerFlags_All, boxThickness);
                    // Back face
                    ImGui::GetBackgroundDrawList()->AddRect(backTopLeft, backBottomRight, box_col, 0.0f, ImDrawCornerFlags_All, boxThickness);
                    // Connecting lines - batched
                    ImGui::GetBackgroundDrawList()->AddLine(topLeft, backTopLeft, box_col, boxThickness);
                    ImGui::GetBackgroundDrawList()->AddLine(topRight, backTopRight, box_col, boxThickness);
                    ImGui::GetBackgroundDrawList()->AddLine(bottomLeft, backBottomLeft, box_col, boxThickness);
                    ImGui::GetBackgroundDrawList()->AddLine(bottomRight, backBottomRight, box_col, boxThickness);
                }
                break;
            }
            
            // Picture ESP Rendering - Render selected image over player instead of box
            ImVec4 colorss = game_helper.IsEnemyVisible(SkeletalMesh) 
                ? ImVec4(vscolors.box_visible[0], vscolors.box_visible[1], vscolors.box_visible[2], vscolors.box_visible[3])
                : ImVec4(vscolors.box_invisible[0], vscolors.box_invisible[1], vscolors.box_invisible[2], vscolors.box_invisible[3]);
            
            RenderPictureESP(ImGui::GetBackgroundDrawList(), topLeft, bottomRight, game_helper.IsEnemyVisible(SkeletalMesh), colorss);
		}

		if (visuals.corneredbox && visuals.enable)
		{
			const float halfWidth = box_width / 1.5f;
			const ImVec2 topLeft(head_box.x - halfWidth, head_box.y);
			const ImVec2 bottomRight(root.x + halfWidth, root.y);

			const float cornerBoxThickness = 2.0f; // Corner box thickness starts at 1
			const ImColor outlineColor(0, 255, 0, 255); // Outline color (green)
			const ImColor cornerBoxFillColor(cornerFIlled); // Fill color for the cornered box (defined in visuals)

			const int x = static_cast<int>(topLeft.x);
			const int y = static_cast<int>(topLeft.y);
			const int w = static_cast<int>(bottomRight.x - topLeft.x);
			const int h = static_cast<int>(bottomRight.y - topLeft.y);

			// If filled cornered box is enabled, fill the box
			if (visuals.cornered_filled)
			{
				ImGui::GetBackgroundDrawList()->AddRectFilled(topLeft, bottomRight, cornerBoxFillColor, 0.0f, ImDrawCornerFlags_All);
			}

			// If the outline is enabled, draw the outline
			if (visuals.cornered_outline)
			{
				ImGui::GetBackgroundDrawList()->AddRect(topLeft, bottomRight, outlineColor, 0.0f, ImDrawCornerFlags_All, cornerBoxThickness + 2.5); // Outer outline
			}

			// Always draw the cornered box outline regardless of the outline checkbox
			// Call the draw_cornered_box function
			draw_cornered_box(x, y, w, h, outlineColor, cornerBoxFillColor, cornerBoxThickness);
		}

		if (visuals.distance && visuals.enable)
		{
			bottom_offset += 10;

			ImVec2 textPosition(root.x, root.y + bottom_offset);
			std::string distance_str = std::to_string(static_cast<int>(distance)) + "M";
			char buffer[128];

			sprintf_s(buffer, ("%s"), distance_str.c_str());
			drawing.Draw_Text(buffer, textPosition, 15.f, ImColor(255, 255, 255, 255), true);
		}

		if (visuals.username && visuals.enable)
		{
			ImVec2 textPosition(head_box.x, head_box.y - 30);
			std::string playerName = decryption.GetPlayerName(PlayerState);
			char buffer[128];

			sprintf_s(buffer, ("%s"), playerName.c_str());
			drawing.Draw_Text(buffer, textPosition, 15.f, ImColor(255, 255, 255, 255), true);
		}
		else if (visuals.username && visuals.distance)
		{
			ImVec2 textPosition(head_box.x, head_box.y - 30);

			std::string playerName = decryption.GetPlayerName(PlayerState);
			std::string distance_str = std::to_string(static_cast<int>(distance)) + "M";

			// Combine the username and distance into a single string
			std::string full_text = playerName + " " + "(" + distance_str + ")";

			// Draw the combined text
			drawing.Draw_Text(full_text.c_str(), textPosition, 15.f, ImColor(255, 255, 255, 255), true);
		}

		if (visuals.weapon && visuals.enable)
		{
			ImVec2 textPosition(head_box.x, head_box.y - 15);
			std::string weaponname = decryption.GetWeaponName(CurrentActor);
			if (weaponname.empty()) weaponname = "Pickaxe";
			char buffer[128];

			sprintf_s(buffer, ("%s"), weaponname.c_str());

			drawing.Draw_Text(buffer, textPosition, 15.f, ImColor(255, 255, 255, 255), true);
		}

		if (visuals.rank && visuals.enable)
		{
			bottom_offset += 15;

			uintptr_t habenero = read<uintptr_t>(PlayerState + 0x948);
			int32_t RankProgress = read<int32_t>(habenero + 0xD8 + 0x10);

			std::string rankText = decryption.getRank(RankProgress);
			uint32_t rankColor = ImGui::ColorConvertFloat4ToU32(decryption.getRankColor(RankProgress));

			drawing.Draw_Text(rankText, ImVec2(root.x, root.y + bottom_offset), ImGui::GetFontSize(), rankColor, true);
		}
		if (visuals.skeletonhead)
		{
			fvector2d head_2d = game_helper.ProjectWorldToScreen(fvector(head_bone.x, head_bone.y, head_bone.z + 20));

			fvector delta = head_bone - camera_postion.location;
			float distance = delta.length();

			const float constant_circle_size = 10;

			float circle_radius = constant_circle_size * (globals.ScreenHeight / (2.0f * distance * tanf(camera_postion.fov * (float)M_PI / 360.f))) - 1.5;

			float y_offset = +70.0f;
			head_2d.y += y_offset;

			int segments = 50;
			float thickness = 2.0f;

			if (visuals.skel_outline)
			{
				ImGui::GetBackgroundDrawList()->AddCircle(ImVec2(head_2d.x, head_2d.y), circle_radius, ImColor(0, 0, 0, 255), segments, visuals.skeleton_thickness + 2);
			}
			ImGui::GetBackgroundDrawList()->AddCircle(ImVec2(head_2d.x, head_2d.y), circle_radius, box_color, segments, visuals.skeleton_thickness);
		}

		int side_offset = 0;

		if (visuals.platform && visuals.enable)
		{
			bottom_offset += 20;

			ImVec2 textPosition(root.x, root.y + bottom_offset);

			WCHAR buffer[0x30];

			DotMem::read_physical(read<uintptr_t*>(PlayerState + offsets::Platform), buffer, sizeof(buffer));

			std::wstring ws = buffer;

			std::string PlatformName = std::string(ws.begin(), ws.end());

			ImColor PlatformColor;

			if (strstr(PlatformName.c_str(), OBF_STR("WIN").c_str()))
			{
				PlatformColor = ImColor(255, 255, 255, 255);

				drawing.Draw_Text("PC", textPosition, 17.f, PlatformColor, true);
			}
			else if (strstr(PlatformName.c_str(), OBF_STR("PSN").c_str()) || strstr(PlatformName.c_str(), OBF_STR("PS5").c_str()))
			{
				PlatformColor = ImColor(0, 108, 199, 255);

				drawing.Draw_Text("PSN", textPosition, 17.f, PlatformColor, true);

			}
			else if (strstr(PlatformName.c_str(), OBF_STR("XBL").c_str()) || strstr(PlatformName.c_str(), OBF_STR("XSX").c_str()))
			{
				PlatformColor = ImColor(18, 120, 16, 255);

				drawing.Draw_Text("XBOX", textPosition, 17.f, PlatformColor, true);

			}
			else if (strstr(PlatformName.c_str(), OBF_STR("SWT").c_str()))
			{
				PlatformColor = ImColor(223, 0, 17);

				drawing.Draw_Text("SWITCH", textPosition, 17.f, PlatformColor, true);
			}
			else if (strstr(PlatformName.c_str(), OBF_STR("AND").c_str()) || strstr(PlatformName.c_str(), OBF_STR("IOS").c_str()))
			{
				PlatformColor = ImColor(255, 255, 255, 255);

				drawing.Draw_Text("MOBILE", textPosition, 17.f, PlatformColor, true);
			}
			else
			{
				PlatformColor = ImColor(255, 255, 255, 255);

				drawing.Draw_Text(PlatformName.c_str(), textPosition, 17.f, PlatformColor, true);
			}
		}

	// Optimized crosshair drawing - reduced calculations and state changes
	if (visuals.crosshair)
	{
		static const ImVec2 center = ImVec2(GetSystemMetrics(0) / 2, GetSystemMetrics(1) / 2);
		static const ImVec2 horiz_start = ImVec2(center.x - 6, center.y);
		static const ImVec2 horiz_end = ImVec2(center.x + 6, center.y);
		static const ImVec2 vert_start = ImVec2(center.x, center.y - 6);
		static const ImVec2 vert_end = ImVec2(center.x, center.y + 6);
		static const float circle_radius = 6.0f;
		
		const ImU32 crosshair_col = IM_COL32(visuals.crosshair_color[0], visuals.crosshair_color[1], visuals.crosshair_color[2], 255);
		const ImU32 outline_col = IM_COL32(0, 0, 0, 255);
		
		if (aimbot.crosshair_type == 0) {
			// Point - Outlined circle (optimized)
			ImGui::GetBackgroundDrawList()->AddCircle(center, circle_radius, outline_col, 16, 2.0f); // Black outline (fewer segments)
			ImGui::GetBackgroundDrawList()->AddCircle(center, circle_radius, crosshair_col, 16, 1.0f); // Colored circle
		}
		else {
			// Normal - Plus sign (batched drawing)
			ImGui::GetBackgroundDrawList()->AddLine(horiz_start, horiz_end, outline_col, 2.0f);
			ImGui::GetBackgroundDrawList()->AddLine(vert_start, vert_end, outline_col, 2.0f);
			ImGui::GetBackgroundDrawList()->AddLine(horiz_start, horiz_end, crosshair_col, 1.0f);
			ImGui::GetBackgroundDrawList()->AddLine(vert_start, vert_end, crosshair_col, 1.0f);
		}
	}

		if (visuals.radar) {

		}

		// Optimized skeleton drawing - reduced bone calculations and batched operations
		if (visuals.skeleton || visuals.skeleton_alt)
		{
			// Pre-cache bone locations to avoid repeated calculations
			static thread_local std::unordered_map<uintptr_t, std::vector<fvector>> bone_cache;
			static thread_local std::unordered_map<uintptr_t, int> cache_frame;
			const int cache_duration = 2; // Cache for 2 frames
			
			std::vector<fvector>* cached_bones = nullptr;
			bool use_cache = false;
			
			// Check cache
			auto cache_it = bone_cache.find(SkeletalMesh);
			auto frame_it = cache_frame.find(SkeletalMesh);
			
			if (cache_it != bone_cache.end() && frame_it != cache_frame.end())
			{
				if (frame_it->second > 0)
				{
					cached_bones = &cache_it->second;
					use_cache = true;
					frame_it->second--;
				}
			}
			
			// Define bone indices (centralized for easy maintenance)
			constexpr int HEAD_BONE = 110;
			constexpr int NECK_BONE = 67;
			constexpr int CHEST_BONE = 7;
			constexpr int PELVIS_BONE = 2;
			constexpr int RIGHT_SHOULDER = 9;
			constexpr int RIGHT_ELBOW = 10;
			constexpr int RIGHT_WRIST = 11;
			constexpr int LEFT_SHOULDER = 38;
			constexpr int LEFT_ELBOW = 39;
			constexpr int LEFT_WRIST = 40;
			constexpr int RIGHT_HIP = 71;
			constexpr int RIGHT_KNEE = 72;
			constexpr int RIGHT_ANKLE = 73;
			constexpr int RIGHT_FOOT_UPPER = 86;
			constexpr int RIGHT_FOOT = 76;
			constexpr int LEFT_HIP = 78;
			constexpr int LEFT_KNEE = 79;
			constexpr int LEFT_ANKLE = 80;
			constexpr int LEFT_FOOT_UPPER = 87;
			constexpr int LEFT_FOOT = 83;
			
			// Bone positions (either cached or calculated)
			std::vector<fvector> bone_positions;
			
			if (use_cache && cached_bones)
			{
				bone_positions = *cached_bones;
			}
			else
			{
				// Calculate all bones at once
				bone_positions.reserve(20);
				bone_positions.push_back(game_helper.GetBoneLocation(SkeletalMesh, HEAD_BONE));
				bone_positions.push_back(game_helper.GetBoneLocation(SkeletalMesh, NECK_BONE));
				bone_positions.push_back(game_helper.GetBoneLocation(SkeletalMesh, CHEST_BONE));
				bone_positions.push_back(game_helper.GetBoneLocation(SkeletalMesh, PELVIS_BONE));
				bone_positions.push_back(game_helper.GetBoneLocation(SkeletalMesh, RIGHT_SHOULDER));
				bone_positions.push_back(game_helper.GetBoneLocation(SkeletalMesh, RIGHT_ELBOW));
				bone_positions.push_back(game_helper.GetBoneLocation(SkeletalMesh, RIGHT_WRIST));
				bone_positions.push_back(game_helper.GetBoneLocation(SkeletalMesh, LEFT_SHOULDER));
				bone_positions.push_back(game_helper.GetBoneLocation(SkeletalMesh, LEFT_ELBOW));
				bone_positions.push_back(game_helper.GetBoneLocation(SkeletalMesh, LEFT_WRIST));
				bone_positions.push_back(game_helper.GetBoneLocation(SkeletalMesh, RIGHT_HIP));
				bone_positions.push_back(game_helper.GetBoneLocation(SkeletalMesh, RIGHT_KNEE));
				bone_positions.push_back(game_helper.GetBoneLocation(SkeletalMesh, RIGHT_ANKLE));
				bone_positions.push_back(game_helper.GetBoneLocation(SkeletalMesh, RIGHT_FOOT_UPPER));
				bone_positions.push_back(game_helper.GetBoneLocation(SkeletalMesh, RIGHT_FOOT));
				bone_positions.push_back(game_helper.GetBoneLocation(SkeletalMesh, LEFT_HIP));
				bone_positions.push_back(game_helper.GetBoneLocation(SkeletalMesh, LEFT_KNEE));
				bone_positions.push_back(game_helper.GetBoneLocation(SkeletalMesh, LEFT_ANKLE));
				bone_positions.push_back(game_helper.GetBoneLocation(SkeletalMesh, LEFT_FOOT_UPPER));
				bone_positions.push_back(game_helper.GetBoneLocation(SkeletalMesh, LEFT_FOOT));
				
				// Update cache
				bone_cache[SkeletalMesh] = bone_positions;
				cache_frame[SkeletalMesh] = cache_duration;
			}
			
			// Project all bones to screen space at once
			std::vector<ImVec2> screen_positions;
			screen_positions.reserve(bone_positions.size());
			
			for (const auto& bone_pos : bone_positions)
			{
				fvector2d screen_pos = game_helper.ProjectWorldToScreen(bone_pos);
				screen_positions.emplace_back(screen_pos.x, screen_pos.y);
			}
			
			// Batch drawing operations to reduce state changes
			const ImU32 skeleton_col = box_color;
			const ImU32 outline_col = IM_COL32(0, 0, 0, 255);
			const float thickness = visuals.skeleton_thickness;
			const float outline_thickness = thickness + 2.0f;
			
			ImDrawList* draw_list = ImGui::GetBackgroundDrawList();
			
			// Draw skeleton based on selected style
			if (visuals.skeleton_alt)
			{
				// Alternative skeleton style - simplified and cleaner look
				// Only draw main body parts for a cleaner appearance
				
				// Draw outlines first (batched)
				if (visuals.skel_outline)
				{
					draw_list->AddLine(screen_positions[0], screen_positions[1], outline_col, outline_thickness); // Head-Neck
					draw_list->AddLine(screen_positions[1], screen_positions[2], outline_col, outline_thickness); // Neck-Chest
					draw_list->AddLine(screen_positions[2], screen_positions[3], outline_col, outline_thickness); // Chest-Pelvis
					
					// Arms
					draw_list->AddLine(screen_positions[2], screen_positions[4], outline_col, outline_thickness); // Chest-Right Shoulder
					draw_list->AddLine(screen_positions[4], screen_positions[5], outline_col, outline_thickness); // Right Shoulder-Elbow
					draw_list->AddLine(screen_positions[5], screen_positions[6], outline_col, outline_thickness); // Right Elbow-Wrist
					draw_list->AddLine(screen_positions[2], screen_positions[7], outline_col, outline_thickness); // Chest-Left Shoulder
					draw_list->AddLine(screen_positions[7], screen_positions[8], outline_col, outline_thickness); // Left Shoulder-Elbow
					draw_list->AddLine(screen_positions[8], screen_positions[9], outline_col, outline_thickness); // Left Elbow-Wrist
					
					// Legs
					draw_list->AddLine(screen_positions[3], screen_positions[10], outline_col, outline_thickness); // Pelvis-Right Hip
					draw_list->AddLine(screen_positions[10], screen_positions[11], outline_col, outline_thickness); // Right Hip-Knee
					draw_list->AddLine(screen_positions[11], screen_positions[12], outline_col, outline_thickness); // Right Knee-Ankle
					draw_list->AddLine(screen_positions[12], screen_positions[14], outline_col, outline_thickness); // Right Ankle-Foot
					draw_list->AddLine(screen_positions[3], screen_positions[15], outline_col, outline_thickness); // Pelvis-Left Hip
					draw_list->AddLine(screen_positions[15], screen_positions[16], outline_col, outline_thickness); // Left Hip-Knee
					draw_list->AddLine(screen_positions[16], screen_positions[17], outline_col, outline_thickness); // Left Knee-Ankle
					draw_list->AddLine(screen_positions[17], screen_positions[19], outline_col, outline_thickness); // Left Ankle-Foot
				}
				
				// Draw main skeleton lines (batched)
				draw_list->AddLine(screen_positions[0], screen_positions[1], skeleton_col, thickness); // Head-Neck
				draw_list->AddLine(screen_positions[1], screen_positions[2], skeleton_col, thickness); // Neck-Chest
				draw_list->AddLine(screen_positions[2], screen_positions[3], skeleton_col, thickness); // Chest-Pelvis
				
				// Arms
				draw_list->AddLine(screen_positions[2], screen_positions[4], skeleton_col, thickness); // Chest-Right Shoulder
				draw_list->AddLine(screen_positions[4], screen_positions[5], skeleton_col, thickness); // Right Shoulder-Elbow
				draw_list->AddLine(screen_positions[5], screen_positions[6], skeleton_col, thickness); // Right Elbow-Wrist
				draw_list->AddLine(screen_positions[2], screen_positions[7], skeleton_col, thickness); // Chest-Left Shoulder
				draw_list->AddLine(screen_positions[7], screen_positions[8], skeleton_col, thickness); // Left Shoulder-Elbow
				draw_list->AddLine(screen_positions[8], screen_positions[9], skeleton_col, thickness); // Left Elbow-Wrist
				
				// Legs
				draw_list->AddLine(screen_positions[3], screen_positions[10], skeleton_col, thickness); // Pelvis-Right Hip
				draw_list->AddLine(screen_positions[10], screen_positions[11], skeleton_col, thickness); // Right Hip-Knee
				draw_list->AddLine(screen_positions[11], screen_positions[12], skeleton_col, thickness); // Right Knee-Ankle
				draw_list->AddLine(screen_positions[12], screen_positions[14], skeleton_col, thickness); // Right Ankle-Foot
				draw_list->AddLine(screen_positions[3], screen_positions[15], skeleton_col, thickness); // Pelvis-Left Hip
				draw_list->AddLine(screen_positions[15], screen_positions[16], skeleton_col, thickness); // Left Hip-Knee
				draw_list->AddLine(screen_positions[16], screen_positions[17], skeleton_col, thickness); // Left Knee-Ankle
				draw_list->AddLine(screen_positions[17], screen_positions[19], skeleton_col, thickness); // Left Ankle-Foot
			}
			else if (visuals.skeleton)
			{
				// Original skeleton style - full body with all connections
				// Draw outlines first (batched)
				if (visuals.skel_outline)
				{
					draw_list->AddLine(screen_positions[0], screen_positions[1], outline_col, outline_thickness); // Head-Neck
					draw_list->AddLine(screen_positions[1], screen_positions[2], outline_col, outline_thickness); // Neck-Chest
					draw_list->AddLine(screen_positions[2], screen_positions[3], outline_col, outline_thickness); // Chest-Pelvis
					draw_list->AddLine(screen_positions[2], screen_positions[4], outline_col, outline_thickness); // Chest-Right Shoulder
					draw_list->AddLine(screen_positions[4], screen_positions[5], outline_col, outline_thickness); // Right Shoulder-Elbow
					draw_list->AddLine(screen_positions[5], screen_positions[6], outline_col, outline_thickness); // Right Elbow-Wrist
					draw_list->AddLine(screen_positions[2], screen_positions[7], outline_col, outline_thickness); // Chest-Left Shoulder
					draw_list->AddLine(screen_positions[7], screen_positions[8], outline_col, outline_thickness); // Left Shoulder-Elbow
					draw_list->AddLine(screen_positions[8], screen_positions[9], outline_col, outline_thickness); // Left Elbow-Wrist
					draw_list->AddLine(screen_positions[3], screen_positions[10], outline_col, outline_thickness); // Pelvis-Right Hip
					draw_list->AddLine(screen_positions[10], screen_positions[11], outline_col, outline_thickness); // Right Hip-Knee
					draw_list->AddLine(screen_positions[11], screen_positions[12], outline_col, outline_thickness); // Right Knee-Ankle
					draw_list->AddLine(screen_positions[12], screen_positions[13], outline_col, outline_thickness); // Right Ankle-Foot Upper
					draw_list->AddLine(screen_positions[13], screen_positions[14], outline_col, outline_thickness); // Right Foot Upper-Foot
					draw_list->AddLine(screen_positions[3], screen_positions[15], outline_col, outline_thickness); // Pelvis-Left Hip
					draw_list->AddLine(screen_positions[15], screen_positions[16], outline_col, outline_thickness); // Left Hip-Knee
					draw_list->AddLine(screen_positions[16], screen_positions[17], outline_col, outline_thickness); // Left Knee-Ankle
					draw_list->AddLine(screen_positions[17], screen_positions[18], outline_col, outline_thickness); // Left Ankle-Foot Upper
					draw_list->AddLine(screen_positions[18], screen_positions[19], outline_col, outline_thickness); // Left Foot Upper-Foot
				}
				
				// Draw main skeleton lines (batched)
				draw_list->AddLine(screen_positions[0], screen_positions[1], skeleton_col, thickness); // Head-Neck
				draw_list->AddLine(screen_positions[1], screen_positions[2], skeleton_col, thickness); // Neck-Chest
				draw_list->AddLine(screen_positions[2], screen_positions[3], skeleton_col, thickness); // Chest-Pelvis
				draw_list->AddLine(screen_positions[2], screen_positions[4], skeleton_col, thickness); // Chest-Right Shoulder
				draw_list->AddLine(screen_positions[4], screen_positions[5], skeleton_col, thickness); // Right Shoulder-Elbow
				draw_list->AddLine(screen_positions[5], screen_positions[6], skeleton_col, thickness); // Right Elbow-Wrist
				draw_list->AddLine(screen_positions[2], screen_positions[7], skeleton_col, thickness); // Chest-Left Shoulder
				draw_list->AddLine(screen_positions[7], screen_positions[8], skeleton_col, thickness); // Left Shoulder-Elbow
				draw_list->AddLine(screen_positions[8], screen_positions[9], skeleton_col, thickness); // Left Elbow-Wrist
				draw_list->AddLine(screen_positions[3], screen_positions[10], skeleton_col, thickness); // Pelvis-Right Hip
				draw_list->AddLine(screen_positions[10], screen_positions[11], skeleton_col, thickness); // Right Hip-Knee
				draw_list->AddLine(screen_positions[11], screen_positions[12], skeleton_col, thickness); // Right Knee-Ankle
				draw_list->AddLine(screen_positions[12], screen_positions[13], skeleton_col, thickness); // Right Ankle-Foot Upper
				draw_list->AddLine(screen_positions[13], screen_positions[14], skeleton_col, thickness); // Right Foot Upper-Foot
				draw_list->AddLine(screen_positions[3], screen_positions[15], skeleton_col, thickness); // Pelvis-Left Hip
				draw_list->AddLine(screen_positions[15], screen_positions[16], skeleton_col, thickness); // Left Hip-Knee
				draw_list->AddLine(screen_positions[16], screen_positions[17], skeleton_col, thickness); // Left Knee-Ankle
				draw_list->AddLine(screen_positions[17], screen_positions[18], skeleton_col, thickness); // Left Ankle-Foot Upper
				draw_list->AddLine(screen_positions[18], screen_positions[19], skeleton_col, thickness); // Left Foot Upper-Foot
			}
		}
		// Existing player count display
		if (visuals.player_count) {
			char rendercount[256];
			sprintf(rendercount, "Players On Screen: %d", safe_player_count);
			ImVec2 text_size = ImGui::CalcTextSize(rendercount);
			float x = (globals.ScreenWidth - text_size.x) / 2.0f;
			float y = 80.0f;
			ImGui::GetBackgroundDrawList()->AddText(ImGui::GetFont(), 15.0f, ImVec2(x, y), ImColor(255, 0, 0, 255), rendercount);
		}

		// Render count display (players being processed/rendered)
		if (visuals.render_count) {
			char rendercount_text[256];
			sprintf(rendercount_text, "Nearby Players: %d", render_count_total);
			ImVec2 text_size = ImGui::CalcTextSize(rendercount_text);
			float x = (globals.ScreenWidth - text_size.x) / 2.0f;
			float y = 100.0f; // Below the player count
			ImGui::GetBackgroundDrawList()->AddText(ImGui::GetFont(), 15.0f, ImVec2(x, y), ImColor(0, 255, 255, 255), rendercount_text); // Cyan color
		}

		// Visible count display (players that are visible)
		if (visuals.visible_count) {
			char visiblecount_text[256];
			sprintf(visiblecount_text, "Visible Players: %d", visible_count_total);
			ImVec2 text_size = ImGui::CalcTextSize(visiblecount_text);
			float x = (globals.ScreenWidth - text_size.x) / 2.0f;
			float y = 120.0f; // Below the render count
			ImGui::GetBackgroundDrawList()->AddText(ImGui::GetFont(), 15.0f, ImVec2(x, y), ImColor(0, 255, 0, 255), visiblecount_text); // Green color
		}

		// ESP Preview Window
		if (aimbot.esp_preview_enabled) {
			ImGui::SetNextWindowPos(ImVec2(50, 50), ImGuiCond_FirstUseEver);
			ImGui::SetNextWindowSize(ImVec2(300, 400), ImGuiCond_FirstUseEver);
			ImGui::Begin("ESP Preview", &aimbot.esp_preview_enabled, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);
			
			ImGui::Text("ESP Configuration Preview");
			ImGui::Separator();
			
			ImGui::Text("Current Settings:");
			ImGui::Text("FOV Size: %d", aimbot.fovsize);
			ImGui::Text("Smooth: %d", aimbot.smoothsize);
			ImGui::Text("Render Distance: %d", visuals.renderDistance);
			ImGui::Text("Box Thickness: %d", visuals.box_thick);
			ImGui::Text("Skeleton Thickness: %d", visuals.skeleton_thickness);
			
			ImGui::Spacing();
			ImGui::Text("Active Features:");
			if (visuals.box) ImGui::Text("- Box ESP");
			if (visuals.skeleton) ImGui::Text("- Skeleton ESP");
			if (visuals.username) ImGui::Text("- Name ESP");
			if (visuals.distance) ImGui::Text("- Distance ESP");
			if (visuals.weapon) ImGui::Text("- Weapon ESP");
			
			ImGui::Spacing();
			ImGui::Text("Player Info:");
			ImGui::Text("Total Players: %d", safe_player_count);
			ImGui::Text("Rendered: %d", render_count_total);
			ImGui::Text("Visible: %d", visible_count_total);
			
			ImGui::End();
		}

		if (triggerbot.triggerbot_enable && (GetAsyncKeyState(triggerbot.triggerbot_key) < 0))
		{
			if ((read<uintptr_t>(CachePointers.PlayerController + offsets::TargetedFortPawn)))
			{
				if (distance <= triggerbot.triggerbot_distance)
				{
					// No Sleep - instant trigger for maximum FPS
					utils.left_click();
				}
			}
		}

		if (aimbot.fov_arrows)
		{
			decryption.AddPlayerToFovCircle(root_bone, 187, game_helper.IsEnemyVisible(SkeletalMesh), ImColor(255, 0, 0));
		}



		entity cached_actors{ };
		cached_actors.entity = CurrentActor;
		cached_actors.skeletal_mesh = skeletalmesh;
		cached_actors.player_state = PlayerState;

		temporary_entity_list.push_back(cached_actors);
	}

	if (TargetEntity && aimbot.enable)
	{
		auto ClosestMesh = read<uint64_t>(TargetEntity + offsets::Mesh);

		fvector Hitbox;

		if (aimbot.Hitbox == 0)
			Hitbox = game_helper.GetBoneLocation(ClosestMesh, 110);
		else if (aimbot.Hitbox == 1)
			Hitbox = game_helper.GetBoneLocation(ClosestMesh, 66);
		else if (aimbot.Hitbox == 2)
			Hitbox = game_helper.GetBoneLocation(ClosestMesh, 7);
		else if (aimbot.Hitbox == 3)
			Hitbox = game_helper.GetBoneLocation(ClosestMesh, 2);

		fvector2d HitboxScreen = game_helper.ProjectWorldToScreen(Hitbox);

		if (aimbot.predictiondot)
			ImGui::GetBackgroundDrawList()->AddCircleFilled(ImVec2(HitboxScreen.x, HitboxScreen.y), 8.0f, ImColor(102, 0, 255), 200);

		if (aimbot.target_line)
			ImGui::GetBackgroundDrawList()->AddLine(ImVec2(globals.ScreenWidth / 2, globals.ScreenHeight / 2), ImVec2(HitboxScreen.x, HitboxScreen.y), ImColor(255, 255, 255), 1.f);

		if (HitboxScreen.x != 0 || HitboxScreen.y != 0 && (get_cross_distance(HitboxScreen.x, HitboxScreen.y, globals.ScreenWidth / 2, globals.ScreenHeight / 2) <= aimbot.fovsize))
		{
			if (aimbot.vischeck ? game_helper.IsEnemyVisible(ClosestMesh) : true) {

				if (GetAsyncKeyState(aimbot.aimkey))
				{

					if (aimbot.prediction)
					{
						float projectileSpeed = 0;
						float projectileGravityScale = 0;

					//	projectileSpeed = read<float>(CachePointers.LocalWeapon + 0x2634);
					//	projectileGravityScale = read<float>(CachePointers.LocalWeapon + 0x2638);

					//	auto root = read<__int64>(TargetEntity + offsets::RootComponent);
						auto distance = camera_postion.location.distance(Hitbox);
					//	fvector velocity = read<fvector>(root + offsets::Velocity);

						if (projectileSpeed > 1000)
						{
					//		Hitbox = game_helper.PredictLocation(Hitbox, velocity, projectileSpeed, projectileGravityScale, distance);
						}

						HitboxScreen = game_helper.ProjectWorldToScreen(Hitbox);

						move(HitboxScreen);
					}
					else {
						move(HitboxScreen);
					}
				}
			}
		}
	}

	else
	{
		TargetDistance = FLT_MAX;
		TargetEntity = NULL;
	}

	// Bullet TP Logic
	if (settings::exploits::BulletTP && TargetEntity) {
		// Auto TP nach Schuss
		if (settings::bullet_tp::auto_tp && GetAsyncKeyState(VK_LBUTTON) & 0x8000) {
			ExecuteBulletTP(TargetEntity);
		}
		
		// Manual TP mit Tastendruck
		if (GetAsyncKeyState(VK_F1) & 0x8000) {
			ExecuteBulletTP(TargetEntity);
		}
		
		// Process Bullet TP
		ProcessBulletTP();
	}

	// Spectator Mode Logic
	if (settings::exploits::Spectate) {
		// Start Spectate mit F2
		if (GetAsyncKeyState(VK_F2) & 0x8000 && TargetEntity) {
			StartSpectate(TargetEntity);
		}
		
		// End Spectate mit F3
		if (GetAsyncKeyState(VK_F3) & 0x8000) {
			EndSpectate();
		}
		
		// Tick Spectate (läuft kontinuierlich)
		TickSpectate();
	}

	entity_list.clear();
	entity_list = temporary_entity_list;

	// Present once per frame (was incorrectly inside player loop - caused massive CPU/GPU load)
	// Respect VSync setting: 1 = wait for vsync (60Hz), 0 = immediate (uncapped)
	d3d_swap_chain->Present(visuals.vsync ? 1 : 0, 0);
}