#pragma once
#include <cstdint>
#include <unordered_map>
#include <fstream>

// Current Patch: v39.40

// Offsets from discord.gg/enigmacheat
// 

namespace offsets {

    // ===== Globals =====
    uintptr_t UWorld = 0x177E8AA8; // updated: 0x178685D8 -> 0x177E8AA8
    uintptr_t UWorldXorKey = 0xFFFFFFFF3CE5AF17; // updated: 0xFFFFFFFF30B9BBF9 -> 0xFFFFFFFF3CE5AF17
    uintptr_t UWorldXorRotationCount = 0; // (simple exponent : no ror8 this update)
    uintptr_t GNames = 0x17722E80;
    uintptr_t GEngine = 0x177EA398; // updated: 0x17869ED8 -> 0x177EA398
    uintptr_t GObjects = 0x17780278; // updated: 0x177FFDB0 -> 0x17780278
    uintptr_t GObjectsCount = 0x17780268; // updated: 0x177FFDC0 -> 0x17780268
    uintptr_t StaticFindObject = 0x7AABFD; // updated: 0x72B449 -> 0x7AABFD
    uintptr_t StaticLoadObject = 0xB7BEAB; // updated: 0x96DC19 -> 0xB7BEAB

    uintptr_t ProcessEvent = 0x26474; // updated: 0x84B9A -> 0x26474
    uintptr_t ProcessEventIndex = 0x178;
    uintptr_t DrawTransition = 0x54DFCC; // updated: 0x5243F6 -> 0x54DFCC
    uintptr_t DrawTransitionIndex = 111;
    uintptr_t BoneMatrix = 0x504A62; // updated: 0x56464C -> 0x504A62

    // ===== Player / Controller =====
    uintptr_t LocalPlayers = 0x38;
    uintptr_t PlayerController = 0x30;
    uintptr_t PlayerCameraManager = 0x368; // updated: 0x360 -> 0x368
    uintptr_t AcknowledgedPawn = 0x358;
    uintptr_t PlayerState = 0x2D0;
    uintptr_t NetConnection = 0x528;
    uintptr_t TeamIndex = 0x11B1;
    uintptr_t bIsDying = 0x728;
    uintptr_t bIsDBNO = 0x841;
    uintptr_t bIsABot = 0x2BA;

    uintptr_t Platform = 0x440;
    uintptr_t TargetedFortPawn = 0x1830;
    uintptr_t KillScore = 0x11C8;
    uintptr_t RebootCount = 0x1894;
    uintptr_t PlayerName = 0xA08;
    uintptr_t PlayerNamePrivate = 0x348;
    uintptr_t RankedProgress = 0xD8;
    uintptr_t PlayerAimOffset = 0x2BD0;

    // ===== World =====
    uintptr_t OwningGameInstance = 0x240; // updated: 0x248 -> 0x240
    uintptr_t GameState = 0x1C8; // updated: 0x1D0 -> 0x1C8
    uintptr_t PlayerArray = 0x2C8;
    uintptr_t WorldSettings = 0x2B8;
    uintptr_t WorldGravityZ = 0x330;
    uintptr_t WorldToMeters = 0x320;

    uintptr_t PersistentLevel = 0x40;
    uintptr_t Levels = 0x1E0; // updated: 0x1E8 -> 0x1E0

    // ===== Actor / Pawn =====
    uintptr_t AActor = 0x130; // updated: 0x208 -> 0x130
    uintptr_t RootComponent = 0x1B0;
    uintptr_t PawnPrivate = 0x328;
    uintptr_t MoveIgnoreActors = 0x348;

    // ===== Mesh / Components =====
    uintptr_t Mesh = 0x330;
    uintptr_t BoneArray = 0x5F0;
    uintptr_t BoneCache = 0x600;
    uintptr_t MeshDeformerInstances = 0x5C0;

    uintptr_t ComponentToWorld = 0x1E0;
    uintptr_t RelativeLocation = 0x140;
    uintptr_t RelativeRotation = 0x158;
    uintptr_t RelativeScale3D = 0x170;
    uintptr_t ComponentVelocity = 0x188;
    uintptr_t AdditionalAimOffset = 0x2BA0;
    uintptr_t LastRenderTime = 0x328;
    uintptr_t LocationUnderReticle = 0x2A50;

    // ===== Camera =====
    uintptr_t CameraLocation = 0x178; // updated: 0x170 -> 0x178
    uintptr_t CameraRotation = 0x188; // updated: 0x180 -> 0x188
    uintptr_t CameraFOV = 0x3B4;

    // ===== Vehicles =====
    uintptr_t CurrentVehicle = 0x2C68;

    // ===== Weapons =====
    uintptr_t CurrentWeapon = 0x990;
    uintptr_t WeaponData = 0x5F8; // updated: 0x5A0 -> 0x5F8
    uintptr_t WeaponOffsetCorrection = 0x2C00;
    uintptr_t AmmoCount = 0x1184; // updated: 0x14FC -> 0x1184
    uintptr_t bIsReloadingWeapon = 0x3A1; // updated: 0x3D1 -> 0x3A1
    uintptr_t ReloadAnimation = 0x19D0; // updated: 0x19C8 -> 0x19D0
    uintptr_t LWProjectile_ActivateRemovedTimestamp = 0x29C0; // updated: 0x29F8 -> 0x29C0

    uintptr_t ProjectileSpeed = 0x2564; // updated: 0x24AC -> 0x2564
    uintptr_t ProjectileGravity = 0x2568; // updated: 0x24B0 -> 0x2568
    uintptr_t MaxTargetingAimAdjustPerSecond = 0x2330; // updated: 0x2418 -> 0x2330
    uintptr_t ServerWorldTimeSecondsDelta = 0x2E8;

    uintptr_t LastFireTimeVerified = 0x10D8; // updated: 0x1430 -> 0x10D8
    uintptr_t WeaponCoreAnimation = 0x1A50; // updated: 0x1A58 -> 0x1A50

    // ===== Items / Loot =====
    uintptr_t PrimaryPickupItemEntry = 0x3A8;
    uintptr_t ItemName = 0xB0;
    uintptr_t ItemType = 0xA8;
    uintptr_t PrimaryAssetOverride = 0xA9;
    uintptr_t ItemRarity = 0xAA;
    uintptr_t bAlreadySearched = 0xD52;

    // ===== Misc =====
    uintptr_t HabaneroComponent = 0x948;
};