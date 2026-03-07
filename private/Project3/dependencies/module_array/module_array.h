//#include <string>
//#include <vector>
//#include "../../utilities/sdk/utils/settings.h"
//#include "../../framework/imgui.h"
//
//enum MenuTab : int
//{
//	Combat = 0,
//	Visuals = 1,
//	Radarington = 2,
//	Misc = 3,
//	Settings = 4
//};
//MenuTab eMenuTab;
//
//void ModuleArrayList()
//{
//	float fSpacing = 0.f;
//
//	struct Module {
//		std::string name;
//		bool status;
//	};
//
//	std::vector<Module> modules;
//
//	auto AddModule = [&](std::string sModuleName, bool bStatus) -> void
//		{
//			if (bStatus)
//			{
//				modules.push_back({ sModuleName, bStatus });
//			}
//		};
//
//	// Combat tab
//	AddModule("Aimbot", aimbot.enable);
//	AddModule("Draw Fov", aimbot.drawfov);
//	AddModule("Prediction", aimbot.prediction);
//	AddModule("Prediction Dot", aimbot.predictiondot);
//	// Visuals
//	AddModule("Visuals", visuals.enable);
//	AddModule("Box", visuals.box);
//	AddModule("Filled Box", visuals.filled);
//	AddModule("Skeleton", visuals.skeleton);
//	AddModule("Distance", visuals.distance);
//	AddModule("Username", visuals.username);
//	AddModule("Platform", visuals.platform);
//	AddModule("Weapon", visuals.weapon);
//	AddModule("Rank", visuals.rank);
//	AddModule("Kills", visuals.killscore);
//	AddModule("Level", visuals.level_score);
//	AddModule("Chests Opened", visuals.chests_opened);
//	AddModule("Minutes Alive", visuals.minutes_alive);
//	AddModule("Current Health", visuals.current_health);
//	AddModule("Current Shield", visuals.current_shield);
//	AddModule("Spectators", visuals.current_spectators);
//	AddModule("Box Outline", visuals.box_outline);
//	AddModule("Skeleton Outline", visuals.skel_outline);
//	// Misc
//	AddModule("Menu Crosshair", menus.menu_cursor);
//
//	// Sort the modules by name length in descending order
//	//std::sort(modules.begin(), modules.end(), [](const Module& a, const Module& b) {
//	//	return a.name.length() > b.name.length(); // Descending order
//	//	});
//	
//	for (const auto& module : modules)
//	{
//		auto Size = ImGui::CalcTextSize(module.name.c_str());
//		ImGui::GetForegroundDrawList()->AddRectFilled(ImVec2(1920 - (Size.x + 10), fSpacing), ImVec2(1920, fSpacing + 20), ImColor(0, 0, 0, 160));
//		ImGui::GetForegroundDrawList()->AddRectFilled(ImVec2(1920 - (Size.x + 10), fSpacing), ImVec2((1920 - (Size.x + 10)) + 3, fSpacing + 20), ImColor(255, 0, 0));
//		ImGui::GetForegroundDrawList()->AddText(ImVec2(1920 - (Size.x + 5), fSpacing + 1), ImColor(255, 255, 255), module.name.c_str());
//		fSpacing += 20.f;
//	}
//}