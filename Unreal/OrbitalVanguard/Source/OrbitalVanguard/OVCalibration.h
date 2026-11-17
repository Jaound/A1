#pragma once

#include "CoreMinimal.h"

class AOVShip;
class UOVCombatSubsystem;

// Read-only candidate metadata. Zero size/cost means no hull definition.
struct ORBITALVANGUARD_API FOVHullMetadata
{
    int32 SizeClass = 0;
    int32 FleetCost = 0;
    TArray<FName> RoleTags;
    FString Description;
};

namespace OVCalibration
{
inline constexpr TCHAR Id[] = TEXT("Calibration_D116_v06");
inline constexpr double XYLimit = 180000;
inline constexpr double MinZ = -45000;
inline constexpr double MaxZ = 60000;
inline constexpr double GridStep = 15000;
inline constexpr double StartZoom = 100000;
inline constexpr double MaxZoom = 420000;
inline constexpr double RayLength = 1500000;
inline constexpr double AltitudeScale = 20;

TArray<FName> GetHullIds();
bool GetHullMetadata(FName HullId, FOVHullMetadata& OutMetadata);
FString GetHullName(FName HullId);
double GetHullGeometryScale(FName HullId);
TArray<FName> GetWeaponProfileIds();
FString GetWeaponProfileDescription(FName ProfileId);
FString GetSourceHash();
// Returns false before initializing any runtime profile if the requested roster is invalid.
bool Start(UOVCombatSubsystem& Combat, const TArray<AOVShip*>& Ships, int32 Scenario);
// Candidate data reuse for 2C. Does not alter stored inventory or reload profiles.
bool ConfigureHull(UOVCombatSubsystem& Combat, AOVShip& Ship, FName HullId, bool Friendly, FVector Position, double Scale);
}
