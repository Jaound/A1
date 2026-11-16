#include "OVCalibration.h"

#include "OVCombatSubsystem.h"
#include "OVShip.h"
#include "OVShipSystemsComponent.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#endif

namespace
{
struct FWeapon
{
    const TCHAR* Key;
    EOVAttackKind Kind;
    int32 MinTier, Salvo;
    float Damage, Period, Range, Speed, Turn, HP, Spread, GuideTurn, Lifetime;
};

struct FRadar { int32 Tier; float Range, Bonus; };
struct FMount { int32 Weapon, Tier; bool bTop; };
struct FHull
{
    const TCHAR* Key;
    int32 Size, Decks, PD, RadarTier;
    float HP, Shield, Armor, FrontArmor, Speed, Turn;
    int32 MountCount;
    FMount Mounts[16];
};

#include "OVCalibrationData.inl"

const FHull* FindHull(FName Key)
{
    for (const FHull& Hull : Hulls)
        if (FName(Hull.Key) == Key) return &Hull;
    return nullptr;
}

EOVWeaponType Family(EOVAttackKind Kind)
{
    if (Kind == EOVAttackKind::Beam) return EOVWeaponType::Beam;
    if (Kind == EOVAttackKind::Cannon) return EOVWeaponType::Cannon;
    return EOVWeaponType::Missile;
}

FName ProfileId(const FWeapon& Weapon, int32 Tier)
{
    return FName(*FString::Printf(TEXT("D116_%s_T%d"), Weapon.Key, Tier));
}

void ApplyProfiles(UOVCombatSettings& Settings)
{
    Settings.GuaranteedRange = 25000;
    Settings.AutoClearanceHeight = 8500;
    Settings.GuidedTrackingSeconds = 22;
    Settings.Weapons.Reset();
    Settings.Radars.Reset();
    for (const auto& R : Radars)
    {
        FOVRadarProfile& Radar = Settings.Radars.AddDefaulted_GetRef();
        Radar.Tier = R.Tier;
        Radar.Range = R.Range;
        Radar.AccuracyBonus = R.Bonus;
    }
    for (const auto& W : Weapons)
    {
        for (int32 Tier = W.MinTier; Tier <= 4; ++Tier)
        {
            FOVWeaponProfile& Profile = Settings.Weapons.AddDefaulted_GetRef();
            Profile.ProfileId = ProfileId(W, Tier);
            Profile.Family = Family(W.Kind);
            Profile.Kind = W.Kind;
            Profile.Tier = Tier;
            Profile.Range = W.Range;
            Profile.Interval = W.Period;
            Profile.Damage = W.Damage * TierDamage[Tier - 1];
            // Beams are instant; existing profile validation requires a positive speed field.
            Profile.Speed = W.Speed > 0 ? W.Speed : 1;
            Profile.TraverseSpeed = W.Turn;
            Profile.TraverseAcceleration = W.Turn * 4;
            Profile.AimSpreadDegrees = W.Spread;
            Profile.Salvo = W.Salvo;
            Profile.SalvoInterval = SalvoInterval;
            Profile.GuidanceTurnRate = W.GuideTurn;
            Profile.Lifetime = W.Lifetime > 0 ? W.Lifetime : (W.Speed > 0 ? W.Range / W.Speed + 5 : 1);
            Profile.VLSHeight = 4000;
        }
    }
}

UShipLayoutDataAsset* MakeLayout(UObject* Outer, const FHull& Hull)
{
    auto* Layout = UShipLayoutDataAsset::MakeTestLayout(Outer, Hull.Decks);
    Layout->bSmallHull = Hull.Size == 1;
    Layout->MaxHullHP = Hull.HP;
    Layout->WeaponMounts.Reset();
    Layout->CannonSlots = Layout->BeamSlots = Layout->MissileSlots = 0;
    Layout->WeaponTier = Hull.MountCount > 0 ? Hull.Mounts[0].Tier : 1;
    for (auto& Armor : Layout->ArmorSections)
        Armor.MaxArmor = Armor.Face == EOVArmorFace::Front ? Hull.FrontArmor : Hull.Armor;
    TSet<EOVFacilityType> SingletonFacilities;
    for (auto& Slot : Layout->Slots)
    {
        if (Slot.Facility != EOVFacilityType::ShieldControl && Slot.Facility != EOVFacilityType::Engine
            && Slot.Facility != EOVFacilityType::Radar) continue;
        if (SingletonFacilities.Contains(Slot.Facility))
        {
            Slot.Facility = EOVFacilityType::None;
            Slot.RequiredCrew = Slot.MaxFacilityHP = 0;
        }
        else SingletonFacilities.Add(Slot.Facility);
    }

    for (int32 I = 0; I < Hull.MountCount; ++I)
    {
        const auto& Mount = Hull.Mounts[I];
        // The existing explosion model has one tier per layout. Refuse unsupported loadouts.
        if (Mount.Tier != Layout->WeaponTier) return nullptr;
        const auto& Weapon = Weapons[Mount.Weapon];
        auto& Def = Layout->WeaponMounts.AddDefaulted_GetRef();
        Def.WeaponId = FName(*FString::Printf(TEXT("Weapon_%d"), I));
        Def.Type = Family(Weapon.Kind);
        Def.Tier = Mount.Tier;
        Def.MaxHP = Weapon.HP * TierDamage[Mount.Tier - 1];
        const double X = FMath::Lerp(-.8, .8, (I + 1.0) / (Hull.MountCount + 1.0));
        Def.LocalPosition = FVector(X * Layout->LocalHalfExtent.X, 0,
            (Mount.bTop ? 1 : -1) * (Layout->HalfHeight + 12));
        if (Def.Type == EOVWeaponType::Beam) ++Layout->BeamSlots;
        else if (Def.Type == EOVWeaponType::Cannon) ++Layout->CannonSlots;
        else ++Layout->MissileSlots;
    }

    TArray<FString> Errors;
    if (Layout->ValidateLayout(Errors)) return Layout;
    UE_LOG(LogTemp, Error, TEXT("D116 invalid %s layout: %s"), Hull.Key, *FString::Join(Errors, TEXT("; ")));
    return nullptr;
}
}

bool OVCalibration::Start(UOVCombatSubsystem& Combat, const TArray<AOVShip*>& Ships, int32 Scenario)
{
    if (Ships.Num() != 3 || Scenario < 0 || Scenario > 2) return false;
    const TCHAR* Keys[] = {TEXT("vector"), TEXT("corvette"), Scenario == 2 ? TEXT("missile_ship") : TEXT("bastion")};
    const bool bDuel = Scenario == 0;
    const FHull* Roster[3] = {};
    UShipLayoutDataAsset* Layouts[3] = {};
    for (int32 I = 0; I < 3; ++I)
    {
        Roster[I] = FindHull(FName(Keys[I]));
        if (!Roster[I]) return false;
        Layouts[I] = MakeLayout(Ships[I], *Roster[I]);
        if (!Layouts[I]) return false;
    }

    Combat.EnableCombat({}, 260112);
    ApplyProfiles(*Combat.Settings);
    const FVector Positions[] = {FVector(-33000,-6200,2000), FVector(-37000,10500,2000), FVector(33000,0,2000)};
    for (int32 I = 0; I < 3; ++I)
    {
        auto* Ship = Ships[I];
        const auto& Hull = *Roster[I];
        const bool bFriendly = I != 2;
        const bool bInactive = bDuel && I == 1;
        const double Scale = I == 1 ? 6 : I == 0 ? 10 : Scenario == 2 ? 18 : 13;
        Ship->Configure(FString(Hull.Key).ToUpper(), bFriendly, Hull.Speed, Hull.Turn, FVector(Scale),
            bFriendly ? FLinearColor(.12f,.65f,1) : FLinearColor(1,.12f,.08f));
        Ship->SetCalibrationHull(&Layouts[I]->LocalHalfExtent);
        auto& Systems = *Ship->Systems;
        Systems.Tuning = GetDefault<UOVShipSystemsComponent>()->Tuning;
        Systems.Tuning.ShieldBase = Hull.Shield;
        Systems.Tuning.ShieldRegenFraction = ShieldCombat;
        Systems.Tuning.ShieldNonCombatRegenFraction = ShieldNonCombat;
        Systems.Tuning.NoHitRecoveryInterval = ShieldTransition;
        Systems.Tuning.ShieldRecoveryDelay = ShieldDelay;
        Systems.Tuning.CrewDamageCoefficient = CrewInjury;
        Systems.FactionId = bFriendly ? 0 : 1;
        Systems.Initialize(Layouts[I]);
        Ship->SetActorLocationAndRotation(Positions[I], FRotator(0,bFriendly ? 0 : 180,0));
        Ship->DebugResetMotion();
        Ship->SetAutomation(!bFriendly,!bFriendly);
        Ship->SetActorHiddenInGame(bInactive);
        Ship->SetActorEnableCollision(!bInactive);
        Combat.RegisterShip(Ship, Hull.RadarTier, false, Hull.Size);
        if (bInactive) { Combat.NotifyFTLDeparture(Ship); continue; }

        for (int32 M = 0; M < Hull.MountCount; ++M)
        {
            const auto& Mount = Hull.Mounts[M];
            Combat.SetWeaponProfile(Ship, Layouts[I]->WeaponMounts[M].WeaponId,
                ProfileId(Weapons[Mount.Weapon], Mount.Tier));
        }
    }
    UE_LOG(LogTemp, Display, TEXT("%s scenario=%d source_sha256=%s"), Id, Scenario, SourceHash);
    return true;
}

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVD116CatalogTest, "OrbitalVanguard.Revision.D116CandidateCatalog",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOVD116CatalogTest::RunTest(const FString&)
{
    TestEqual(TEXT("All candidate hull IDs retained"), int32(UE_ARRAY_COUNT(Hulls)), 16);
    const FHull* Vector = FindHull(TEXT("vector"));
    const FHull* Bastion = FindHull(TEXT("bastion"));
    const FHull* Dreadnought = FindHull(TEXT("dreadnought"));
    if (!Vector || !Bastion || !Dreadnought) return false;
    TestTrue(TEXT("Distinct destroyer deck, attack and PD metadata"),
        Vector->Decks == 1 && Vector->MountCount == 3 && Vector->PD == 2
        && Bastion->Decks == 2 && Bastion->MountCount == 5 && Bastion->PD == 4);
    TestEqual(TEXT("Extra-large hull HP remains absolute"), Dreadnought->HP, 825000.f);
    TestNull(TEXT("Mixed-tier explosion layout is rejected"), MakeLayout(GetTransientPackage(), *Dreadnought));
    auto* Settings = NewObject<UOVCombatSettings>();
    ApplyProfiles(*Settings);
    ApplyProfiles(*Settings);
    const auto* Beam = Settings->Weapons.FindByPredicate([](const auto& W) { return W.ProfileId == TEXT("D116_beam_T1"); });
    const auto* Spinal = Settings->Weapons.FindByPredicate([](const auto& W) { return W.ProfileId == TEXT("D116_spinal_T4"); });
    TestTrue(TEXT("Repeated load preserves exactly 25 calibrated profiles and cm range"),
        Settings->Weapons.Num() == 25 && Beam && Beam->Range == 70000 && Settings->GuaranteedRange == 25000);
    TestTrue(TEXT("Spinal damage uses full T4 multiplier once"), Spinal && Spinal->Damage == 5100 && Spinal->Speed == 42000);
    return true;
}
#endif

