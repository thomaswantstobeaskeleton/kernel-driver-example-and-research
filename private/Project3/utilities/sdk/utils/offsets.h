#pragma once
#include "../../impl/driver.hpp"
#include <unordered_map>
#include <bit>

#define CURRENT_CLASS reinterpret_cast<std::uintptr_t>( this )

/* Offsets - get your own from a dumper per game update. Single user + unique values = less sig risk. */

namespace offsets
{
    // UWorld decrypt: ~rotl(encrypted ^ 0x913F0DAF, 51)
    inline uintptr_t decrypt_UWorld(uintptr_t encrypted_address) {
        return ~(uintptr_t)std::rotl((uint64_t)(encrypted_address ^ 0x913F0DAFULL), 51);
    }

    // ===== Globals =====
    inline static uintptr_t UWorld = 0x1780BA78;
    inline static uintptr_t GNames = 0x17722E80;
    inline static uintptr_t StaticFindObject = 0x7AABFD;

    // ===== Game Framework =====
    inline static uintptr_t OwningGameInstance = 0x250;
    inline static uintptr_t GameState = 0x1D8;
    inline static uintptr_t WorldSettings = 0x2B8;
    inline static uintptr_t Levels = 0x1E0;
    inline static uintptr_t PersistentLevel = 0x40;
    inline static uintptr_t Actors = 0x208;
    inline static uintptr_t PlayerArray = 0x2C8;

    // ===== Local Player =====
    inline static uintptr_t LocalPlayers = 0x38;
    inline static uintptr_t PlayerController = 0x30;
    inline static uintptr_t PlayerCameraManager = 0x368;
    inline static uintptr_t AcknowledgedPawn = 0x358;
    inline static uintptr_t PawnPrivate = 0x328;
    inline static uintptr_t PlayerState = 0x2D0;
    inline static uintptr_t ViewTarget = 0x340;

    // ===== Team / State =====
    inline static uintptr_t TeamIndex = 0x11B1;
    inline static uintptr_t Team_Index = 0x11B1; // alias for compatibility
    inline static uintptr_t KillScore = 0x11C8;
    inline static uintptr_t RankedProgress = 0xD8;
    inline static uintptr_t CurrentHealth = 0xDA4;
    inline static uintptr_t MaxHealth = 0xDA8;
    inline static uintptr_t SeasonLevelUIDisplay = 0x11CC;

    inline static uintptr_t bIsDying = 0x728;
    inline static uintptr_t bIsDBNO = 0x841;
    inline static uintptr_t bIsABot = 0x2BA;
    inline static uintptr_t bIsAnAthenaGameParticipant = 0x18BF;

    // ===== Components =====
    inline static uintptr_t RootComponent = 0x1B0;
    inline static uintptr_t Mesh = 0x330;
    inline static uintptr_t ComponentToWorld = 0x1E0;
    inline static uintptr_t ComponentVelocity = 0x188;
    inline static uintptr_t RelativeLocation = 0x140;
    inline static uintptr_t RelativeScale3D = 0x170;

    // ===== Bones =====
    inline static uintptr_t BoneArray = 0x5F0;
    inline static uintptr_t BoneCache = 0x600;   // BoneArray + 0x10
    inline static uintptr_t BoneArrayCache = 0x600; // alias
    inline static uintptr_t BonesTide = 0x60;

    // ===== Rendering / Visibility =====
    inline static uintptr_t BoundScale = 0x310;
    inline static uintptr_t LastRenderTime = 0x328;
    inline static uintptr_t LastSubmitTime = 0x328;
    inline static uintptr_t fLastRenderTimeOnScreen = 0x328;  // LastRenderTimeOnScreen (BoundScale + 0x1C = 0x32C)

    // ===== Camera =====
    inline static uintptr_t CameraLocation = 0x180;
    inline static uintptr_t CameraRotation = 0x190;  // CameraLocation + 0x10
    inline static uintptr_t CameraFOV = 0x3B4;
    inline static uintptr_t FOV = 0x3B4;
    inline static uintptr_t DefaultFOV = 0x3B4;
    inline static uintptr_t BaseFOV = 0x370;

    // ===== Weapons =====
    inline static uintptr_t CurrentWeapon = 0x990;
    inline static uintptr_t WeaponData = 0x5C0;
    inline static uintptr_t AmmoCount = 0x1184;
    inline static uintptr_t ReloadAnimation = 0x19D0;

    inline static uintptr_t ProjectileSpeed = 0x20C0;
    inline static uintptr_t ProjectileGravity = 0x20C4;  // ProjectileSpeed + 4
    inline static uintptr_t MaxTargetingAimAdjustPerSecond = 0x2330;

    // ===== Aim =====
    inline static uintptr_t PlayerAim = 0x2BD0;
    inline static uintptr_t PlayerAimOffset = 0x2BD0;
    inline static uintptr_t WeaponOffsetCorrection = 0x2C00;  // PlayerAimOffset + 0x30
    inline static uintptr_t AdditionalAimOffset = 0x2BA0;
    inline static uintptr_t TargetedFortPawn = 0x1830;
    inline static uintptr_t LocationUnderReticle = 0x2A50;

    // ===== World Timing =====
    inline static uintptr_t WorldTick = 0x198;
    inline static uintptr_t Seconds = 0x198;
    inline static uintptr_t ServerWorldTimeSecondsDelta = 0x2E8;  // ServerTime

    // ===== Items / Loot =====
    inline static uintptr_t PrimaryPickupItemEntry = 0x3A8;
    inline static uintptr_t ItemDefinition = 0x10;
    inline static uintptr_t ItemName = 0x40;
    inline static uintptr_t FtextData = 0x20;
    inline static uintptr_t FtextLength = 0x28;
    // Length of wide-char name buffer (used in sdk.h and engine loop.h)
    inline static uintptr_t ItemData = 0x28; // TODO: verify for your current Fortnite version
    inline static uintptr_t ItemType = 0xA8;
    inline static uintptr_t ItemRarity = 0xAA;
    inline static uintptr_t Tier = 0xA2;
    inline static uintptr_t Rarity = 0xAA;
    inline static uintptr_t bAlreadySearched = 0xD52;

    // ===== Misc =====
    inline static uintptr_t Platform = 0x440;
    inline static uintptr_t platform = 0x440;  // alias
    inline static uintptr_t HabaneroComponent = 0x948;
    inline static uintptr_t CurrentVehicle = 0x2C68;
    inline static uintptr_t CurrentMovementStyle = 0x843;  // dump FortPawn.hpp: 0x0843
    inline static uintptr_t CustomTimeDilation = 0x68;

    // ===== Names =====
    inline static uintptr_t NamePrivate = 0x348;
    inline static uintptr_t ComparisonIndex = 0x8;
    inline static uintptr_t DisplayName = 0x10D8;      // LastFireTimeVerified

    // Legacy/compatibility aliases
    inline static uintptr_t OwningWorld = 0xc0;
    inline static uintptr_t NetConnection = 0x528;
    inline static uintptr_t Velocity = 0xb8; // alias
    inline static uintptr_t worldgravityz = 0x330;
    inline static uintptr_t WorldToMeters = 0x320;
    inline static uintptr_t PlayerName = 0xA08;
    inline static uintptr_t AActor = 0x130;
    inline static uintptr_t SuperField = 0x40;
    inline static uintptr_t OverlappingBuildings = 0x1D78;
    inline static uintptr_t fLastSubmitTime = 0x328;
}

namespace weapon
{
    inline static uintptr_t CurrentWeapon = 0x990;
    inline static uintptr_t WeaponData = 0x5C0;
    inline static uintptr_t AmmoCount = 0x1184;
    inline static uintptr_t ItemName = 0x40;
    inline static uintptr_t Tier = 0xA2;
    inline static uintptr_t Color = 0x50;
    inline static uintptr_t bIsReloadingWeapon = 0x3A1;
    inline static uintptr_t ReloadAnimation = 0x19D0;
}

namespace world_offsets
{
    inline static uintptr_t levels = 0x1E0;
    inline static uintptr_t Levels = 0x1E0; // alias
    inline static uintptr_t aactor = 0x1D0; // Actors offset
    inline static uintptr_t Actors = 0x1D0; // alias
}

template<class T>
class TArray
{
public:
    int getLength() const
    {
        return count;
    }

    int getIdentifier()
    {
        return data * count * max;
    }

    bool isValid() const
    {
        if (count > max)
            return false;
        if (!data)
            return false;
        return true;
    }

    uint64_t getAddress() const
    {
        return data;
    }

    T operator [](size_t idx) const
    {
        return read<T>(data + sizeof(T) * idx);
    }

protected:
    uint64_t data;
    uint64_t count;
    uint64_t max;
};

struct Fortnite
{
    static uint64_t getVAStart(uintptr_t gworldOffset);

    enum ActorType
    {
        NOT_IN_USE = 0,
        CHEST = 1,
        AMMO_BOX = 2
    };

    struct ActorDefinitions
    {
        int actorID;
        std::string fname;
        ActorType actorType;
    };

    static const std::unordered_map<std::string, ActorType>& getActorByName()
    {
        static const std::unordered_map<std::string, ActorType> actorTypeByName =
        {
            {"Tiered_Chest", ActorType::CHEST},
            {"Tiered_Ammo", ActorType::AMMO_BOX}
        };
        return actorTypeByName;
    }

    static ActorDefinitions getActorDefinitions(int actorID);
};

// Bone IDs for Aimbot
enum bone : int {
    Head = 110,
    Neck = 67,
    Chest = 66,
    Pelvis = 2,
    LShoulder = 9,
    LElbow = 10,
    LHand = 11,
    RShoulder = 38,
    RElbow = 39,
    RHand = 40,
    LHip = 71,
    LKnee = 72,
    LFoot = 73,
    RHip = 78,
    RKnee = 79,
    RFoot = 82,
    Root = 0
};

namespace bone_ids
{
    inline static uintptr_t FN_PELVIS = 2;
    inline static uintptr_t FN_NECK = 67;
    inline static uintptr_t FN_HEAD = 110;
}