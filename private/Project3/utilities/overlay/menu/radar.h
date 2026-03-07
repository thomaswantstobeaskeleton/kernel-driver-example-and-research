#include "../../sdk/cache/actorloop.h"
void RadarRange(float* x, float* y, float range)
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

camera_position_s get_camera()
{
	camera_position_s camera;

	auto location_pointer = read <uintptr_t>(CachePointers.UWorld + 0x178);
	auto rotation_pointer = read <uintptr_t>(CachePointers.UWorld + 0x188);

	struct FNRot
	{
		double a;
		char pad_0008[24];
		double b;
		char pad_0028[424];
		double c;
	} fnRot;

	fnRot.a = read <double>(rotation_pointer);
	fnRot.b = read <double>(rotation_pointer + 0x20);
	fnRot.c = read <double>(rotation_pointer + 0x1d0);

	camera.location = read <fvector>(location_pointer);
	camera.rotation.x = asin(fnRot.c) * (180.0 / M_PI);
	camera.rotation.y = ((atan2(fnRot.a * -1, fnRot.b) * (180.0 / M_PI)) * -1) * -1;
	camera.fov = read<float>((uintptr_t)CachePointers.PlayerController + 0x394) * 90.f;

	return camera;
}
bool ShowRadar = true;
bool rect_radar = true;
float radar_position_x{ 10.0f };
float radar_position_y{ 350.0f };
float radar_size{ 250.0f };
float RadarDistance = { 200.f };

void CalcRadarPoint(fvector vOrigin, int& screenx, int& screeny)
{
	get_camera();
	fvector vAngle = camera_postion.rotation;
	auto fYaw = vAngle.y * M_PI / 180.0f;
	float dx = vOrigin.x - camera_postion.location.x;
	float dy = vOrigin.y - camera_postion.location.y;

	float fsin_yaw = sinf(fYaw);
	float fminus_cos_yaw = -cosf(fYaw);

	float x = dy * fminus_cos_yaw + dx * fsin_yaw;
	x = -x;
	float y = dx * fminus_cos_yaw - dy * fsin_yaw;

	float range = (float)RadarDistance * 1000.f;

	RadarRange(&x, &y, range);

	ImVec2 DrawPos = ImVec2(radar_position_x, radar_position_y);
	ImVec2 DrawSize = ImVec2(radar_size, radar_size);


	int rad_x = (int)DrawPos.x;
	int rad_y = (int)DrawPos.y;

	float r_siz_x = DrawSize.x;
	float r_siz_y = DrawSize.y;

	int x_max = (int)r_siz_x + rad_x - 5;
	int y_max = (int)r_siz_y + rad_y - 5;

	screenx = rad_x + ((int)r_siz_x / 2 + int(x / range * r_siz_x));
	screeny = rad_y + ((int)r_siz_y / 2 + int(y / range * r_siz_y));

	if (screenx > x_max)
		screenx = x_max;

	if (screenx < rad_x)
		screenx = rad_x;

	if (screeny > y_max)
		screeny = y_max;

	if (screeny < rad_y)
		screeny = rad_y;
}

void fortnite_radar(float x, float y, float size, bool rect = false)
{
	if (ShowRadar)
	{
		ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Once);
		ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize, ImGuiCond_Once);
		static const auto flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground;
		ImGui::Begin(("##radar"), nullptr, flags);

		ImDrawList* drawList = ImGui::GetWindowDrawList();

		// Draw transparent rectangle
		ImVec4 rectColor = ImVec4(0.0f, 0.0f, 0.0f, 0.0f); // Fully transparent
		drawList->AddRectFilled(ImVec2(x, y), ImVec2(x + size, y + size), ImGui::GetColorU32(rectColor), 0.0f, 0);

		// Line thickness
		float thickness = 1.5f;

		// Draw white lines with black outline
		ImU32 white = ImGui::GetColorU32({ 1.0f, 1.0f, 1.0f, 1.0f });
		ImU32 black = ImGui::GetColorU32({ 0.5f, 0.5f, 0.5f, 1.0f });
		float outline_thickness = 0.5f;

		// Top line
		drawList->AddLine(ImVec2(x, y), ImVec2(x + size, y), black, thickness + outline_thickness);
		drawList->AddLine(ImVec2(x, y), ImVec2(x + size, y), white, thickness);

		// Right line
		drawList->AddLine(ImVec2(x + size, y), ImVec2(x + size, y + size), black, thickness + outline_thickness);
		drawList->AddLine(ImVec2(x + size, y), ImVec2(x + size, y + size), white, thickness);

		// Bottom line
		drawList->AddLine(ImVec2(x + size, y + size), ImVec2(x, y + size), black, thickness + outline_thickness);
		drawList->AddLine(ImVec2(x + size, y + size), ImVec2(x, y + size), white, thickness);

		// Left line
		drawList->AddLine(ImVec2(x, y + size), ImVec2(x, y), black, thickness + outline_thickness);
		drawList->AddLine(ImVec2(x, y + size), ImVec2(x, y), white, thickness);

		// Middle lines
		// Vertical middle line
		drawList->AddLine(ImVec2(x + size / 2, y), ImVec2(x + size / 2, y + size), black, thickness + outline_thickness);
		drawList->AddLine(ImVec2(x + size / 2, y), ImVec2(x + size / 2, y + size), white, thickness);

		// Horizontal middle line
		drawList->AddLine(ImVec2(x, y + size / 2), ImVec2(x + size, y + size / 2), black, thickness + outline_thickness);
		drawList->AddLine(ImVec2(x, y + size / 2), ImVec2(x + size, y + size / 2), white, thickness);

		// Optionally, draw a smaller circle at the center
		float render_size = 5;
		drawList->AddCircleFilled(ImVec2(x + size / 2, y + size / 2), render_size / 2, white, 100);

		ImGui::End();
	}
}




void add_players_radar(fvector WorldLocation)
{
	if (ShowRadar)
	{
		static const auto flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground;
		ImGui::Begin(("##radar"), nullptr, flags);

		// Taille du radar et calcul du centre
		const int RadarSize = 200;  // Taille totale du radar (peut être ajustée)
		ImVec2 RadarPos = ImGui::GetWindowPos();  // Position de la fenêtre ImGui du radar
		ImVec2 RadarCenter = ImVec2(RadarPos.x + RadarSize / 2, RadarPos.y + RadarSize / 2);  // Centre du radar

		ImDrawList* drawList = ImGui::GetWindowDrawList();

		// Dessiner les barres (lignes)
		// Ligne horizontale
		//drawList->AddLine(ImVec2(RadarCenter.x - 50, RadarCenter.y), ImVec2(RadarCenter.x + 50, RadarCenter.y), ImGui::GetColorU32(ImVec4(1, 1, 1, 1)), 2.0f);
		//// Ligne verticale
		//drawList->AddLine(ImVec2(RadarCenter.x, RadarCenter.y - 50), ImVec2(RadarCenter.x, RadarCenter.y + 50), ImGui::GetColorU32(ImVec4(1, 1, 1, 1)), 2.0f);

		// Calcul du point du joueur sur le radar
		int ScreenX, ScreenY = 0;
		CalcRadarPoint(WorldLocation, ScreenX, ScreenY);

		// Position du joueur relative au centre du radar
		ImVec2 PlayerPos = ImVec2(RadarCenter.x + ScreenX, RadarCenter.y + ScreenY);

		// Dessiner le marqueur du joueur (visible ou invisible)
		if (aimbot.vischeck) {
			drawList->AddTriangleFilled(
				ImVec2(PlayerPos.x, PlayerPos.y - 5),     // Point 1
				ImVec2(PlayerPos.x - 5, PlayerPos.y + 5), // Point 2
				ImVec2(PlayerPos.x + 5, PlayerPos.y + 5), // Point 3
				ImGui::GetColorU32({ vscolors.box_visible[0], vscolors.box_visible[1], vscolors.box_visible[2], 1.0f })
			);
		}
		else {
			drawList->AddTriangleFilled(
				ImVec2(PlayerPos.x, PlayerPos.y - 5),     // Point 1
				ImVec2(PlayerPos.x - 5, PlayerPos.y + 5), // Point 2
				ImVec2(PlayerPos.x + 5, PlayerPos.y + 5), // Point 3
				ImGui::GetColorU32({ vscolors.box_invisible[0], vscolors.box_invisible[1], vscolors.box_invisible[2], 1.0f })
			);
		}

		ImGui::End();
	}
}