#include <string.h>
#include <list>
#include <iostream>
#include <chrono>
#include <unordered_map>
#include "../../impl/driver.hpp"
#include "../../sdk/render/camera.h"
#include "../../../framework/settings.h"

#define FNAMEPOOL_OFFSET 0x117284C0

// Forward declarations
struct FTransform;

class decryption_t
{
public:

	//static std::string GetNameFromIndex(int key)
	//{
	//	uint32_t ChunkOffset = (uint32_t)((int)(key) >> 16);
	//	uint16_t NameOffset = (uint16_t)key;
	//	uint64_t NamePoolChunk = read<uint64_t>(globals.va_text + FNAMEPOOL_OFFSET + (8 * ChunkOffset) + 16) + (unsigned int)(2 * NameOffset);
	//	uint16_t nameEntry = read<uint16_t>(NamePoolChunk);
	//	int nameLength = nameEntry >> 6;
	//	char buff[1024] = {};

	//	char* v3 = buff; // rdi
	//	int v5; // r8d
	//	__int64 result = nameLength; // rax
	//	__int64 v7 = 0; // rdx
	//	unsigned int v8; // r8d
	//	v5 = 26;
	//	if ((int)result)
	//	{
	//		mem::read_physical(reinterpret_cast<void*>(NamePoolChunk + 2), static_cast<uint8_t*>(static_cast<void*>(buff)), 2 * nameLength);
	//		do
	//		{
	//			v8 = v5 + 45297;
	//			*v3 = v8 + ~*v3;
	//			result = v8 << 8;
	//			v5 = result | (v8 >> 8);
	//			++v3;
	//			--v7;
	//		} while (v7);
	//		buff[nameLength] = '\0';
	//		return std::string(buff);
	//	}
	//	return std::string("");
	//}
	void AddPlayerToFovCircle(fvector WorldLocation, float fDistance, bool visible, ImColor color)
	{
		fvector vAngle = camera_postion.rotation;
		float fYaw = vAngle.y * PI / 180.0f;
		float dx = WorldLocation.x - camera_postion.location.x;
		float dy = WorldLocation.y - camera_postion.location.y;
		float fsin_yaw = sinf(fYaw);
		float fminus_cos_yaw = -cosf(fYaw);

		auto Center = ImVec2(globals.ScreenWidth / 2, globals.ScreenHeight / 2);

		float x = -(dy * fminus_cos_yaw + dx * fsin_yaw);
		float y = dx * fminus_cos_yaw - dy * fsin_yaw;

		float fovRadius = aimbot.fovsize;
		float angle = atan2f(y, x);
		float triangleSize = 12.0f;
		float widthFactor = 8.0f;
		float outlineThickness = 2.0f;

		ImVec2 triangleCenter = ImVec2(Center.x + cosf(angle) * (fovRadius + triangleSize),
			Center.y + sinf(angle) * (fovRadius + triangleSize));

		ImVec2 point1 = ImVec2(triangleCenter.x + cosf(angle) * triangleSize,
			triangleCenter.y + sinf(angle) * triangleSize);
		ImVec2 point2 = ImVec2(triangleCenter.x + cosf(angle + widthFactor) * triangleSize,
			triangleCenter.y + sinf(angle + widthFactor) * triangleSize);
		ImVec2 point3 = ImVec2(triangleCenter.x + cosf(angle - widthFactor) * triangleSize,
			triangleCenter.y + sinf(angle - widthFactor) * triangleSize);

		ImU32 fillColor = ImGui::ColorConvertFloat4ToU32(visible ? color : ImColor(color.Value.x, color.Value.y, color.Value.z, 0.5f));
		ImU32 outlineColor = ImGui::ColorConvertFloat4ToU32(ImColor(0, 0, 0, 1.0));

		ImDrawList* drawList = ImGui::GetForegroundDrawList();
		drawList->AddTriangleFilled(point1, point2, point3, fillColor);
		drawList->AddTriangle(point1, point2, point3, outlineColor, outlineThickness);
	}

	std::string getRank(int tier) {
		switch (tier) {
		case 0:  return "Unranked";
		case 1:  return "Bronze 2";
		case 2:  return "Bronze 3";
		case 3:  return "Silver 1";
		case 4:  return "Silver 2";
		case 5:  return "Silver 3";
		case 6:  return "Gold 1";
		case 7:  return "Gold 2";
		case 8:  return "Gold 3";
		case 9:  return "Platinum 1";
		case 10: return "Platinum 2";
		case 11: return "Platinum 3";
		case 12: return "Diamond 1";
		case 13: return "Diamond 2";
		case 14: return "Diamond 3";
		case 15: return "Elite";
		case 16: return "Champion";
		case 17: return "Unreal";
		default: return "Unranked";
		}
	}

	ImVec4 getRankColor(int tier) {
		switch (tier) {
		case 0:  return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);    // Unranked
		case 1:
		case 2:  return ImVec4(0.902f, 0.580f, 0.227f, 1.0f); // Bronze
		case 3:
		case 4:
		case 5:  return ImVec4(0.843f, 0.843f, 0.843f, 1.0f); // Silver
		case 6:
		case 7:
		case 8:  return ImVec4(1.0f, 0.871f, 0.0f, 1.0f); // Gold
		case 9:
		case 10:
		case 11: return ImVec4(0.0f, 0.7f, 0.7f, 1.0f);  // Platinum
		case 12:
		case 13:
		case 14: return ImVec4(0.1686f, 0.3294f, 0.8235f, 1.0f); // Diamond
		case 15: return ImVec4(0.7f, 0.7f, 0.7f, 1.0f);   // Elite
		case 16: return ImVec4(1.0f, 0.6f, 0.0f, 1.0f);   // Champion
		case 17: return ImVec4(0.6f, 0.0f, 0.6f, 1.0f);   // Unreal
		default: return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);    // Unranked
		}
	}

	std::string ReadWideString(uint64_t address, int length) {
		if (length <= 0 || length > 50) return "";

		std::wstring buffer(length, L'\0');
		(address, buffer.data(), length * sizeof(wchar_t));
		return std::string(buffer.begin(), buffer.end());
	}

#define PLAYERNAME 0xA08
	static std::string GetPlayerName(uintptr_t playerState) {
		auto Name = read<uintptr_t>(playerState + PLAYERNAME);
		auto length = read<int>(Name + 0x10);
		auto v6 = (__int64)length;

		if (length <= 0 || length > 255) return std::string("BOT");

		auto FText = (uintptr_t)read<__int64>(Name + 0x8);

		wchar_t* Buffer = new wchar_t[length];
		DotMem::read_physical(PVOID(static_cast<ULONGLONG>(FText)), Buffer, length * sizeof(wchar_t));

		char v21;
		int v22;
		int i;

		int v25;
		UINT16* v23;

		v21 = v6 - 1;
		if (!(UINT32)v6)
			v21 = 0;
		v22 = 0;
		v23 = (UINT16*)Buffer;
		for (i = (v21) & 3; ; *v23++ += i & 7)
		{
			v25 = v6 - 1;
			if (!(UINT32)v6)
				v25 = 0;
			if (v22 >= v25)
				break;
			i += 3;
			++v22;
		}

		std::wstring PlayerName{ Buffer };
		delete[] Buffer;
		return std::string(PlayerName.begin(), PlayerName.end());
	}

	std::string GetWeaponName(uintptr_t Player) {
		std::string PlayersWeaponName = "";
		uint64_t CurrentWeapon = read<uint64_t>((uintptr_t)Player + weapon::CurrentWeapon);
		uint64_t weapondata = read<uint64_t>(CurrentWeapon + weapon::WeaponData);
		uint64_t AmmoCount = read<uint64_t>(CurrentWeapon + weapon::AmmoCount);
		uint64_t ItemName = read<uint64_t>(weapondata + weapon::ItemName);

		if (!ItemName) return "";

		uint64_t FData = read<uint64_t>(ItemName + offsets::FtextData);
		int FLength = read<int>(ItemName + offsets::FtextLength);

		if (FLength > 0 && FLength < 50) {

			wchar_t* WeaponBuffer = new wchar_t[FLength];
			DotMem::read_physical((void*)FData, (PVOID)WeaponBuffer, FLength * sizeof(wchar_t));
			std::wstring wstr_buf(WeaponBuffer);
			if (AmmoCount != 0) PlayersWeaponName.append(std::string(wstr_buf.begin(), wstr_buf.end()) + " [" + "Ammo: " + std::to_string(AmmoCount) + "]");
			else PlayersWeaponName.append(std::string(wstr_buf.begin(), wstr_buf.end()));
			PlayersWeaponName.append(std::string(wstr_buf.begin(), wstr_buf.end()));

			delete[] WeaponBuffer;
		}
		return PlayersWeaponName;
	}
}; decryption_t decryption;

class gamehelper_t
{
public:
	// Cache bone_array and ComponentToWorld per mesh to reduce redundant reads
	// These rarely change, so caching saves 2 reads per GetBoneLocation call
	// Made public so actorloop can clear stale entries
	static std::unordered_map<uintptr_t, uintptr_t> cached_bone_arrays;
	static std::unordered_map<uintptr_t, FTransform> cached_component_to_world;
	static std::unordered_map<uintptr_t, DWORD> cache_frame;  // Frame when cached
	static DWORD last_cache_clear;
	
	// Clear stale cache entries every 5 seconds to prevent memory growth
	static void ClearStaleCache() {
		DWORD now = GetTickCount();
		if (now - last_cache_clear < 5000) return;  // Clear every 5 seconds
		last_cache_clear = now;
		
		// Clear caches if they get too large (>100 entries)
		if (cached_bone_arrays.size() > 100) {
			cached_bone_arrays.clear();
			cached_component_to_world.clear();
			cache_frame.clear();
		}
	}
	
	static auto GetBoneLocation(uintptr_t skeletal_mesh, int bone_index) -> fvector {
		ClearStaleCache();
		
		// Get or cache bone_array (rarely changes)
		uintptr_t bone_array = 0;
		auto bone_array_it = cached_bone_arrays.find(skeletal_mesh);
		if (bone_array_it != cached_bone_arrays.end()) {
			bone_array = bone_array_it->second;
		} else {
			bone_array = read<uintptr_t>(skeletal_mesh + offsets::BoneArray);
			if (bone_array == NULL) bone_array = read<uintptr_t>(skeletal_mesh + offsets::BoneArray + 0x10);
			if (bone_array) cached_bone_arrays[skeletal_mesh] = bone_array;
		}
		if (!bone_array) return fvector(0, 0, 0);
		
		// Get or cache ComponentToWorld (rarely changes per mesh)
		FTransform component_to_world;
		auto ctw_it = cached_component_to_world.find(skeletal_mesh);
		if (ctw_it != cached_component_to_world.end()) {
			component_to_world = ctw_it->second;
		} else {
			component_to_world = read<FTransform>(skeletal_mesh + offsets::ComponentToWorld);
			cached_component_to_world[skeletal_mesh] = component_to_world;
		}
		
		// Only read bone transform (this changes per bone)
		FTransform bone = read<FTransform>(bone_array + (bone_index * 0x60));
		D3DMATRIX matrix = MatrixMultiplication(bone.ToMatrixWithScale(), component_to_world.ToMatrixWithScale());
		return fvector(matrix._41, matrix._42, matrix._43);
	}

	FTransform GetBoneIndex(uint64_t mesh, int index)
	{
		uint64_t bonearray = read<uint64_t>(mesh + offsets::BoneArray);
		if (!bonearray) bonearray = read<uint64_t>(mesh + offsets::BoneArray + 0x10);
		return read<FTransform>(bonearray + (index * 0x60));
	}

	fvector GetBoneWithRotation(DWORD_PTR mesh, int id)
	{
		FTransform bone = GetBoneIndex(mesh, id);
		FTransform ComponentToWorld = read<FTransform>(mesh + offsets::ComponentToWorld);

		D3DMATRIX Matrix;
		Matrix = MatrixMultiplication(bone.ToMatrixWithScale(), ComponentToWorld.ToMatrixWithScale());

		return fvector(Matrix._41, Matrix._42, Matrix._43);
	}

	inline camera_position_s get_camera()
	{
		camera_position_s camera{};
		if (!CachePointers.UWorld || !CachePointers.PlayerController) return camera;
		uintptr_t LocationPointer = read<uintptr_t>(CachePointers.UWorld + offsets::CameraLocation);
		uintptr_t RotationPointer = read<uintptr_t>(CachePointers.UWorld + offsets::CameraRotation);

		struct FNRot
		{
			double a; //0x0000
			char pad_0008[24]; //0x0008
			double b; //0x0020
			char pad_0028[424]; //0x0028
			double c; //0x01D0
		} fnRot;

		fnRot = read<FNRot>(RotationPointer);
		camera.location = read<fvector>(LocationPointer);
		camera.rotation.x = asin(fnRot.c) * (180.0 / M_PI);
		camera.rotation.y = ((atan2(fnRot.a * -1, fnRot.b) * (180.0 / M_PI)) * -1) * -1;
		camera.rotation.z = 0;
		camera.fov = read<float>(CachePointers.PlayerController + offsets::DefaultFOV) * 90.f;
		return camera;
	}

	inline fvector2d ProjectWorldToScreen(fvector WorldLocation)
	{
		// Use cached camera_postion (updated once per frame in actorloop) - avoids redundant driver reads
		// camera_postion is updated once per frame, so no need to call get_camera() here

		if (WorldLocation.x == 0)
			return fvector2d(0, 0);

		_MATRIX tempMatrix = Matrix(camera_postion.rotation);
		fvector vAxisX = fvector(tempMatrix.m[0][0], tempMatrix.m[0][1], tempMatrix.m[0][2]);
		fvector vAxisY = fvector(tempMatrix.m[1][0], tempMatrix.m[1][1], tempMatrix.m[1][2]);
		fvector vAxisZ = fvector(tempMatrix.m[2][0], tempMatrix.m[2][1], tempMatrix.m[2][2]);

		fvector vDelta = WorldLocation - camera_postion.location;
		fvector vTransformed = fvector(vDelta.dot(vAxisY), vDelta.dot(vAxisZ), vDelta.dot(vAxisX));

		if (vTransformed.z < 1.f)
			vTransformed.z = 1.f;

		return fvector2d((globals.ScreenWidth / 2.0f) + vTransformed.x * (((globals.ScreenWidth / 2.0f) / tanf(camera_postion.fov * (float)M_PI / 360.f))) / vTransformed.z, (globals.ScreenHeight / 2.0f) - vTransformed.y * (((globals.ScreenWidth / 2.0f) / tanf(camera_postion.fov * (float)M_PI / 360.f))) / vTransformed.z);
	}

	fvector PredictLocation(fvector target, fvector targetVelocity, float projectileSpeed, float projectileGravityScale, float distance)
	{
		float horizontalTime = distance / projectileSpeed;
		float verticalTime = distance / projectileSpeed;

		target.x += targetVelocity.x * horizontalTime;
		target.y += targetVelocity.y * horizontalTime;
		target.z += targetVelocity.z * verticalTime +
			abs(-980 * projectileGravityScale) * 0.5f * (verticalTime * verticalTime);

		return target;
	}

	static auto IsEnemyVisible(uintptr_t mesh) 
	{
		// Cache UWorld Seconds value - it's the same for all players in a frame
		static double cached_seconds = 0.0;
		static DWORD last_cache_frame = 0;
		DWORD current_frame = GetTickCount();
		// Refresh cache every ~16ms (roughly once per frame at 60fps)
		if (current_frame != last_cache_frame) {
			if (CachePointers.UWorld) {
				cached_seconds = read<double>(CachePointers.UWorld + 0x198);
				last_cache_frame = current_frame;
			}
		}
		auto LastRenderTime = read<float>(mesh + 0x328);
		return cached_seconds - LastRenderTime <= 0.06f;
	}

	auto IsInScreen(fvector2d screen_location) -> bool
	{
		if (screen_location.x > 0 && screen_location.x < globals.ScreenWidth && screen_location.y > 0 && screen_location.y < globals.ScreenHeight) return true;
		else return false;
	}
}; 

// Define static members
std::unordered_map<uintptr_t, uintptr_t> gamehelper_t::cached_bone_arrays;
std::unordered_map<uintptr_t, FTransform> gamehelper_t::cached_component_to_world;
std::unordered_map<uintptr_t, DWORD> gamehelper_t::cache_frame;
DWORD gamehelper_t::last_cache_clear = 0;

gamehelper_t game_helper;

char* wchar_to_char(const wchar_t* pwchar)
{
	int currentCharIndex = 0;
	char currentChar = pwchar[currentCharIndex];

	while (currentChar != '\0')
	{
		currentCharIndex++;
		currentChar = pwchar[currentCharIndex];
	}

	const int charCount = currentCharIndex + 1;

	char* filePathC = (char*)malloc(sizeof(char) * charCount);

	for (int i = 0; i < charCount; i++)
	{
		char character = pwchar[i];

		*filePathC = character;

		filePathC += sizeof(char);

	}
	filePathC += '\0';

	filePathC -= (sizeof(char) * charCount);

	return filePathC;
}

// Helper Functions - GetBoneWithRotation ist jetzt in gamehelper_t Klasse

// Spectator Functions
inline void Teleport(fvector pos) {
    auto LocalPawn = CachePointers.AcknownledgedPawn;
    if (!LocalPawn) return;
    uintptr_t root = read<uintptr_t>(LocalPawn + offsets::RootComponent);
    write<fvector>(root + offsets::ComponentToWorld + 0x20, pos);
}

inline void StartSpectate(uintptr_t targetPawn)
{
    if (!CachePointers.PlayerController || !targetPawn)
        return;
    uintptr_t original_pawn = read<uintptr_t>(CachePointers.PlayerController + offsets::AcknowledgedPawn);
    if (!settings::spectate::active)
    {
        settings::spectate::original_pawn = original_pawn;
        auto character_movement = read<uintptr_t>(CachePointers.AcknownledgedPawn + 0x330);
        if (character_movement)
        {
            settings::spectate::original_movement_mode = read<uint8_t>(character_movement + 0x231);
            settings::spectate::original_pawn_motion = read<uint8_t>(CachePointers.AcknownledgedPawn + 0x164);
            settings::spectate::original_controller_motion = read<uint8_t>(CachePointers.PlayerController + 0x164);
        }
        auto Mesh = read<uint64_t>(CachePointers.AcknownledgedPawn + offsets::Mesh);
        if (Mesh)
        {
            settings::spectate::original_mesh_values = read<fvector>(Mesh + 0x139);
            settings::spectate::mesh_values_stored = true;
        }
    }
    settings::spectate::has_teleported = false;
    settings::spectate::last_target_position = fvector(0, 0, 0);
    settings::spectate::target_pawn = targetPawn;
    settings::spectate::active = true;
}

inline void EndSpectate()
{
    if (!CachePointers.PlayerController)
        return;
    auto character_movement = read<uintptr_t>(CachePointers.AcknownledgedPawn + 0x330);
    if (character_movement)
    {
        write<uint8_t>(character_movement + 0x231, settings::spectate::original_movement_mode);
        write<uint8_t>(CachePointers.AcknownledgedPawn + 0x164, settings::spectate::original_pawn_motion);
        write<uint8_t>(CachePointers.PlayerController + 0x164, settings::spectate::original_controller_motion);
    }
    if (settings::spectate::mesh_values_stored)
    {
        auto Mesh = read<uint64_t>(CachePointers.AcknownledgedPawn + offsets::Mesh);
        if (Mesh)
        {
            write<fvector>(Mesh + 0x139, settings::spectate::original_mesh_values);
        }
    }
    settings::spectate::active = false;
    settings::spectate::target_pawn = 0;
    settings::spectate::has_teleported = false;
    settings::spectate::last_target_position = fvector(0, 0, 0);
    settings::spectate::original_movement_mode = 0;
    settings::spectate::original_pawn_motion = 0;
    settings::spectate::original_controller_motion = 0;
    settings::spectate::original_mesh_values = fvector(0, 0, 0);
    settings::spectate::mesh_values_stored = false;
}

inline void TickSpectate()
{
    if (!settings::spectate::active || !settings::spectate::target_pawn)
        return;
    uintptr_t Isdying = read<char>(std::uintptr_t(settings::spectate::target_pawn) + offsets::bIsDying) >> 5 & 1;
    if (Isdying)
    {
        EndSpectate();
        return;
    }
    uintptr_t targetPawn = settings::spectate::target_pawn;
    uintptr_t targetMesh = read<uintptr_t>(targetPawn + offsets::Mesh);
    if (!targetMesh)
    {
        return;
    }
    gamehelper_t helper;
    fvector targetHead = helper.GetBoneWithRotation(targetMesh, 110); // Echte Kopf-Position
    fvector targetBottom = helper.GetBoneWithRotation(targetMesh, 0); // Echte Körper-Position
    fvector targetPos = fvector(targetBottom.x, targetBottom.y, targetBottom.z);
    if (!settings::spectate::has_teleported)
    {           
        settings::spectate::has_teleported = true;
        settings::spectate::last_target_position = targetPos;
        return;
    }
    static float camera_distance = 100.0f;
    static float camera_height = 50.0f;
    int wheel_delta = 0;
    if (GetAsyncKeyState(VK_UP) & 0x8000) {
        wheel_delta = 1;
    }
    if (GetAsyncKeyState(VK_DOWN) & 0x8000) {
        wheel_delta = -1;
    }   
    if (wheel_delta != 0) {
        if (wheel_delta > 0) {
            camera_distance = max(20.0f, camera_distance - 10.0f);
            camera_height = max(20.0f, camera_height - 5.0f);
        } else {
            camera_distance = min(300.0f, camera_distance + 10.0f);
            camera_height = min(150.0f, camera_height + 5.0f);
        }
    }
    fvector camPos = fvector(
        targetPos.x - camera_distance,
        targetPos.y,
        targetPos.z + camera_height
    );  
    auto character_movement = read<uintptr_t>(CachePointers.AcknownledgedPawn + 0x330);
    if (character_movement)
    {
        write<uint8_t>(character_movement + 0x231, 5);
        write<uint8_t>(CachePointers.AcknownledgedPawn + 0x164, 3);
        write<uint8_t>(CachePointers.PlayerController + 0x164, 3);
        
        Teleport(camPos);
        
        write<fvector>(character_movement + 0x5c0, camPos);
        write<fvector>(character_movement + 0xd08 + 0x30, camPos);
        
        fvector camRot = fvector(-45.0f, 0.0f, 0.0f);
        write<fvector>(character_movement + 0x5c0 + 0xC, camRot);
        write<fvector>(character_movement + 0xd08 + 0x30 + 0xC, camRot);
    }   
    auto Mesh = read<uint64_t>(CachePointers.AcknownledgedPawn + offsets::Mesh);
    if (Mesh)
    {
        write<fvector>(Mesh + 0x139, fvector(1, rand() % 361, 1));
    }
    settings::spectate::last_target_position = targetPos;
}

// Player List für Spectator Mode
inline void ShowPlayerList() {
    if (!settings::exploits::Spectate) return;
    
    // Diese Funktion würde alle verfügbaren Spieler anzeigen
    // Für jetzt zeigen wir nur den Status
    if (settings::spectate::active) {
        // Zeige aktuell gespectaten Spieler
        ImGui::Text("Currently spectating player");
    } else {
        // Zeige verfügbare Spieler
        ImGui::Text("Available players:");
        ImGui::Text("1. Player 1");
        ImGui::Text("2. Player 2");
        ImGui::Text("3. Player 3");
        ImGui::Text("...");
        ImGui::Text("Aim at player and press F2 to spectate");
    }
}

// Bullet TP Functions
inline void BulletTP(fvector target_pos) {
    if (!CachePointers.AcknownledgedPawn) return;
    
    // Nutze die Teleport Funktion aus Spectator
    Teleport(target_pos);
    
    // Optional: Ghost Mode (unsichtbar machen)
    if (settings::bullet_tp::ghost_mode) {
        auto Mesh = read<uint64_t>(CachePointers.AcknownledgedPawn + offsets::Mesh);
        if (Mesh) {
            write<fvector>(Mesh + 0x139, fvector(0.1f, 0.1f, 0.1f)); // Sehr klein machen
        }
    }
}

inline void ExecuteBulletTP(uintptr_t target_pawn) {
    if (!target_pawn) return;
    
    uintptr_t targetMesh = read<uintptr_t>(target_pawn + offsets::Mesh);
    if (!targetMesh) return;
    
    fvector target_pos;
    if (settings::bullet_tp::tp_to_head) {
        // TP zu Kopf - echte Bone-Position
        gamehelper_t helper;
        target_pos = helper.GetBoneWithRotation(targetMesh, 110);
    } else {
        // TP zu Körper - echte Bone-Position
        gamehelper_t helper;
        target_pos = helper.GetBoneWithRotation(targetMesh, 0);
    }
    
    // Speichere Ziel für späteren TP
    settings::bullet_tp::last_target = target_pawn;
    settings::bullet_tp::last_target_pos = target_pos;
    settings::bullet_tp::has_shot = true;
}

inline void ProcessBulletTP() {
    if (!settings::exploits::BulletTP || !settings::bullet_tp::has_shot) return;
    
    static auto last_tp_time = std::chrono::steady_clock::now();
    auto current_time = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - last_tp_time).count();
    
    // Warte auf TP-Delay
    if (elapsed >= (settings::bullet_tp::tp_delay * 1000)) {
        BulletTP(settings::bullet_tp::last_target_pos);
        settings::bullet_tp::has_shot = false;
        last_tp_time = current_time;
    }
}