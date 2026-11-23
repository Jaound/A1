#include "OVCalibration.h"

#include "OVCombatSubsystem.h"
#include "OVMarkedSlots.h"
#include "OVWeaponVisualComponent.h"
#include "OVPointDefenseSubsystem.h"
#include "OVShip.h"
#include "OVShipSystemsComponent.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "HAL/IConsoleManager.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "StaticMeshResources.h"
#endif

namespace
{
// Source-surface boxes are provisional; zero restores the legacy test layout.
TAutoConsoleVariable<int32> CVarOVMarkedMounts(TEXT("ov.Calibration.MarkedMounts"),1,
    TEXT("Drawing-based mounts and provisional source-surface collision boxes."), ECVF_Default);
struct FWeapon
{
    const TCHAR* Key;
    EOVAttackKind Kind;
    int32 MinTier, Salvo;
    float Damage, Period, Range, Speed, Turn, HP, Spread, GuideTurn, Lifetime;
};

struct FRadar { int32 Tier; float Range, Bonus; };
struct FHullCollisionCandidate { const TCHAR* Hull; FVector Center, HalfExtent; };
const FHullCollisionCandidate HullCollisionCandidates[] = {
#include "OVHullCollisionCandidateData.inl"
};
struct FMount { int32 Weapon, Tier; bool bTop; };
struct FHull
{
    const TCHAR* Key;
    int32 Size, FleetCost, Decks, PD, RadarTier;
    float HP, Shield, Armor, FrontArmor, Speed, Turn;
    int32 MountCount;
    FMount Mounts[16];
};

#include "OVCalibrationData.inl"

struct FD126HullRule
{
    bool bTopHeavy = false;
    int32 MaxHangars = 0;
    bool bAircraftCapable = false;
    float ArmorMaxReduction = 0.60f;
};

FD126HullRule GetD126HullRule(FName Key)
{
    if (Key == TEXT("battlecarrier")) return { true,  6, true,  0.60f }; // 9-12-15 (36), aircraft capable
    if (Key == TEXT("carrier"))       return { true,  4, true,  0.60f }; // 6-9-12 (27), aircraft capable
    if (Key == TEXT("dreadnought"))   return { false, 4, false, 0.60f }; // 15-12-9 (36), small-craft bay only
    if (Key == TEXT("battleship"))    return { false, 2, false, 0.60f }; // 12-9-6 (27), small-craft bay only
    if (Key == TEXT("missile_ship"))  return { true,  1, false, 0.60f }; // 9-12 (21), small-craft bay only
    if (Key == TEXT("vector"))        return { false, 0, false, 0.26f }; // 9 (1F), D126 26% armor cap
    if (Key == TEXT("bastion"))       return { false, 0, false, 0.42f }; // 9-6 (15), D126 42% armor cap
    return { false, 0, false, 0.60f };
}

EOVHullSizeClass ToHullSizeClass(int32 Size)
{
    switch (Size)
    {
    case 1: return EOVHullSizeClass::Small;
    case 2: return EOVHullSizeClass::Medium;
    case 3: return EOVHullSizeClass::Large;
    case 4: return EOVHullSizeClass::ExtraLarge;
    default: return EOVHullSizeClass::Unspecified;
    }
}

const FHull* FindHull(FName Key)
{
    for (const FHull& Hull : Hulls)
        if (FName(Hull.Key) == Key)
        {
            if(Key==TEXT("battleship")){
                // 플레이 편제 후보. 일반 모듈 두 좌를 교체한 뒤 시설·장착 검증은 같은 경로를 사용한다.
                static const FHull Modular=[&Hull](){FHull Result=Hull;Result.Mounts[0].Weapon=4;Result.Mounts[1].Weapon=5;return Result;}();
                return &Modular;
            }
            if (Key == TEXT("air_defense") && CVarOVMarkedMounts.GetValueOnGameThread() == 1)
            {
                // The user-authored drawing has one general slot. Keep the
                // first existing profile equipped; the second source candidate
                // remains in D116 data rather than firing from an unmarked slot.
                static const FHull Revised = [&Hull]() { FHull Result = Hull; Result.MountCount = 1; return Result; }();
                return &Revised;
            }
            return &Hull;
        }
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

// Keep the equipped weapon IDs/profiles and map them onto the user's authored slots.
// When D126 expands mount counts beyond legacy D116 drawing marks (e.g. frigate/vector/bastion),
// bind every available authored slot deterministically without duplicating slots and keep extra D126 mounts on hull surface positions.
bool ApplyMarkedAttackMounts(const FHull& Hull, UShipLayoutDataAsset& Layout)
{
    const FName HullId(Hull.Key);
    const auto Candidates = OVMarkedSlots::GetCandidates(HullId);
    if (Candidates.IsEmpty()) return false;
    TArray<const FOVMarkedSlotCandidate*> General, Dedicated;
    for (const auto& Slot : Candidates)
    {
        if (Slot.Kind == EOVMarkedSlotKind::General) General.Add(&Slot);
        else if (Slot.Kind == EOVMarkedSlotKind::Dedicated) Dedicated.Add(&Slot);
    }
    if (General.IsEmpty() && Dedicated.IsEmpty()) return false;

    TArray<int32> SpecialWeapons, RegularWeapons;
    for (int32 I = 0; I < Hull.MountCount; ++I)
    {
        const bool bSpecial = Hull.Mounts[I].Weapon == 6 ||
            (HullId == TEXT("missile_ship") && Hull.Mounts[I].Weapon >= 4 && SpecialWeapons.Num() < Dedicated.Num());
        (bSpecial ? SpecialWeapons : RegularWeapons).Add(I);
    }

    const auto Place = [&Layout, &Hull, HullId](const TArray<int32>& WeaponsToPlace, const TArray<const FOVMarkedSlotCandidate*>& Slots)
    {
        const int32 Count = FMath::Min(WeaponsToPlace.Num(), Slots.Num());
        for (int32 I = 0; I < Count; ++I)
        {
            auto& Mount = Layout.WeaponMounts[WeaponsToPlace[I]];
            Mount.AuthoredSlotId = Slots[I]->SlotId;
            Mount.AuthoredSlotPurpose = Slots[I]->Kind == EOVMarkedSlotKind::Dedicated ? FName(TEXT("Dedicated")) : FName(TEXT("General"));
            if (Slots[I]->Kind == EOVMarkedSlotKind::Dedicated)
            {
                const FMount& Source = Hull.Mounts[WeaponsToPlace[I]];
                Mount.RequiredProfileId = ProfileId(Weapons[Source.Weapon], Source.Tier);
                if(HullId==TEXT("missile_ship") && Mount.Type==EOVWeaponType::Missile)Mount.PresentationProfile=TEXT("AntiShipMissileModule");
            }
            Mount.LocalPosition = Slots[I]->LocalPosition;
            Mount.LocalSurfaceNormal = Slots[I]->LocalNormal;
        }
        return Count;
    };

    const int32 PlacedSpecial = Place(SpecialWeapons, Dedicated);
    const int32 PlacedRegular = Place(RegularWeapons, General);
    return (PlacedSpecial + PlacedRegular) > 0;
}

// Display length relative to the fitted hull. One coefficient feeds art, muzzle and mount hits.
float WeaponGeometryScale(int32 Size)
{
    return Size == 4 ? .32f : Size == 3 ? .45f : 1.f;
}

struct FSurfaceSeat { const TCHAR* Hull; FVector Position, Normal; };
struct FShieldFit { const TCHAR* Hull; FVector Radii; };
#define OV_HULL_SHIELD(Key, Radii)
#define OV_HULL_SEAT(Key, P, N) {Key, P, N},
const FSurfaceSeat SurfaceSeats[] = {
#include "OVHullPresentationSurface.inl"
};
#undef OV_HULL_SHIELD
#undef OV_HULL_SEAT
#define OV_HULL_SHIELD(Key, Radii) {Key, Radii},
#define OV_HULL_SEAT(Key, P, N)
const FShieldFit ShieldFits[] = {
#include "OVHullPresentationSurface.inl"
};
#undef OV_HULL_SHIELD
#undef OV_HULL_SEAT

void FitHullEquipment(const FHull& Hull, UShipLayoutDataAsset& Layout)
{
    const FName Key(Hull.Key);
    TArray<FVector> Occupied;
    for (const auto& M : Layout.WeaponMounts)
        if (!M.AuthoredSlotId.IsNone()) Occupied.Add(M.LocalPosition);
    for (auto& M : Layout.WeaponMounts)
    {
        if (!M.AuthoredSlotId.IsNone()) continue;
        const FSurfaceSeat* Best = nullptr;
        double BestScore = -DBL_MAX;
        for (const auto& Seat : SurfaceSeats)
        {
            if (Key != Seat.Hull || (Seat.Normal.Z > 0) != (M.LocalPosition.Z >= 0)) continue;
            double Clearance = 30.0;
            for (const FVector& Other : Occupied)
                Clearance = FMath::Min(Clearance, FVector::Distance(Other,Seat.Position));
            const double Score = 5.0 * Clearance - FVector::DistXY(M.LocalPosition,Seat.Position);
            if (Clearance > 1.0 && Score > BestScore) { Best = &Seat; BestScore = Score; }
        }
        if (ensureMsgf(Best,TEXT("No hull seat for %s/%s"),Hull.Key,*M.WeaponId.ToString()))
        {
            M.LocalPosition = Best->Position;
            M.LocalSurfaceNormal = Best->Normal;
            Occupied.Add(M.LocalPosition);
        }
    }

    for (const auto& Fit : ShieldFits)
    {
        if (Key != Fit.Hull) continue;
        // Keep the bow and stern close to the hull. Expand the transverse ellipse
        // only as far as needed to enclose each weapon in every traverse direction.
        FVector Axes = Fit.Radii;
        const auto Reach = [&Layout](const FOVWeaponMountDefinition& M) {
            return 1.20 * M.GeometryScale * OVWeaponGeometry::BarrelLength(
                Layout.LocalHalfExtent,M.Tier,M.Type==EOVWeaponType::Interception);
        };
        for (const auto& M : Layout.WeaponMounts)
            Axes.X = FMath::Max(Axes.X,(FMath::Abs(M.LocalPosition.X)+Reach(M))*1.05);
        double MaxQ = 1.0;
        for (const auto& M : Layout.WeaponMounts)
        {
            const double R = Reach(M);
            const FVector2D Q(M.LocalPosition.Y/Axes.Y,M.LocalPosition.Z/Axes.Z);
            const double Gradient = FVector2D(Q.X/Axes.Y,Q.Y/Axes.Z).Size();
            // Bound a complete sphere by axial slices. Each slice uses its largest
            // transverse disc and its farthest longitudinal endpoint.
            for (int32 I=0; I<8; ++I)
            {
                const double Lo = R*(I/4.0-1), Hi = Lo+R*.25;
                const double Near = FMath::Clamp(0.0,Lo,Hi);
                const double Disc = FMath::Sqrt(FMath::Max(0.0,R*R-Near*Near));
                const double X = FMath::Max(FMath::Abs(M.LocalPosition.X+Lo),FMath::Abs(M.LocalPosition.X+Hi))/Axes.X;
                MaxQ = FMath::Max(MaxQ,(Q.SizeSquared()+2*Disc*Gradient+FMath::Square(Disc/FMath::Min(Axes.Y,Axes.Z)))/(1-X*X));
            }
        }
        const double Transverse = FMath::Sqrt(MaxQ);
        Layout.FittedShieldRadii = FVector(Axes.X,Axes.Y*Transverse,Axes.Z*Transverse);
        break;
    }
}

void AppendDefenseMounts(const FHull& Hull, UShipLayoutDataAsset& Layout)
{
    const int32 First = Layout.WeaponMounts.Num();
    for (int32 I = 0; I < Hull.PD; ++I)
    {
        auto& M = Layout.WeaponMounts.AddDefaulted_GetRef();
        M.WeaponId = FName(*FString::Printf(TEXT("PD_%d"), I));
        M.Type = EOVWeaponType::Interception;
        M.GeometryScale = WeaponGeometryScale(Hull.Size);
        M.Tier = Layout.WeaponTier;
        M.MaxHP = 100;
        M.LocalPosition = FVector(FMath::Lerp(-.7, .7, (I + .5) / Hull.PD) * Layout.LocalHalfExtent.X,
            (I % 2 ? 1 : -1) * Layout.LocalHalfExtent.Y * .8, Layout.LocalHalfExtent.Z + 8);
    }
    const auto Marked = OVMarkedSlots::GetCandidates(FName(Hull.Key));
    TArray<const FOVMarkedSlotCandidate*> Slots;
    for (const auto& Slot : Marked)
        if (Slot.Kind == EOVMarkedSlotKind::Interception) Slots.Add(&Slot);
    if (CVarOVMarkedMounts.GetValueOnGameThread() == 1 && !Slots.IsEmpty())
    {
        const int32 Count = FMath::Min(Slots.Num(), Hull.PD);
        for (int32 I = 0; I < Count; ++I)
        {
            auto& Mount = Layout.WeaponMounts[First + I];
            Mount.AuthoredSlotId = Slots[I]->SlotId;
            Mount.AuthoredSlotPurpose = TEXT("Interception");
            Mount.LocalPosition = Slots[I]->LocalPosition;
            Mount.LocalSurfaceNormal = Slots[I]->LocalNormal;
        }
    }
    if (CVarOVMarkedMounts.GetValueOnGameThread() == 1) FitHullEquipment(Hull,Layout);
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
            Profile.MountMaxHP = W.HP * TierDamage[Tier - 1];
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

FVector FittedHullExtent(FName MeshId)
{
    static TMap<FName, FVector> Cache;
    if (const FVector* Found = Cache.Find(MeshId)) return *Found;
    const UStaticMesh* Mesh = OVCalibration::LoadHullMesh(MeshId);
    const FVector E = Mesh ? Mesh->GetBounds().BoxExtent : FVector::ZeroVector;
    if (E.ContainsNaN() || E.GetMin() <= UE_SMALL_NUMBER)
    {
        UE_LOG(LogTemp, Error, TEXT("Cannot size %s: missing or degenerate source bounds"), *MeshId.ToString());
        return FVector::ZeroVector;
    }
    const FVector Fitted = E * (FVector(175,75,30) / E.ComponentMax(FVector(1))).GetMin();
    Cache.Add(MeshId, Fitted);
    return Fitted;
}

UShipLayoutDataAsset* MakeLayout(UObject* Outer, const FHull& Hull)
{
    const FName HullId(Hull.Key);
    const FD126HullRule Rule = GetD126HullRule(HullId);
    const EOVHullSizeClass SizeClass = ToHullSizeClass(Hull.Size);
    auto* Layout = UShipLayoutDataAsset::MakeD126Layout(
        Outer, SizeClass, Hull.Decks, Rule.bTopHeavy, Rule.MaxHangars, Rule.bAircraftCapable);
    if (!Layout)
    {
        UE_LOG(LogTemp, Error, TEXT("D126 failed to construct layout for %s (size=%d decks=%d)"), Hull.Key, Hull.Size, Hull.Decks);
        return nullptr;
    }

    FVector Extent = FittedHullExtent(HullId);
    // HalfHeight is serialized as a float; use its representable extent throughout.
    Extent.Z = static_cast<float>(Extent.Z);
    if (Extent.GetMin() <= 0.) return nullptr;
    const FVector Fit = Extent / Layout->LocalHalfExtent;
    for (auto& Slot : Layout->Slots) { Slot.LocalCenter *= Fit; Slot.HalfExtent *= Fit; }
    for (auto& Section : Layout->ArmorSections) { Section.LocalCenter *= Fit; Section.HalfExtent *= Fit; }
    Layout->LocalHalfExtent = Extent;
    Layout->HalfHeight = Extent.Z;
    if (HullId == TEXT("transport")) Layout->LayoutVersion = TEXT("D126_R37_Medium_v1");
    Layout->MaxHullHP = Hull.HP;
    Layout->WeaponMounts.Reset();
    Layout->CannonSlots = Layout->BeamSlots = Layout->MissileSlots = 0;
    Layout->WeaponTier = Hull.MountCount > 0 ? Hull.Mounts[0].Tier : 1;
    for (auto& Armor : Layout->ArmorSections)
        Armor.MaxArmor = Armor.Face == EOVArmorFace::Front ? Hull.FrontArmor : Hull.Armor;

    for (int32 I = 0; I < Hull.MountCount; ++I)
    {
        const auto& Mount = Hull.Mounts[I];
        const auto& Weapon = Weapons[Mount.Weapon];
        auto& Def = Layout->WeaponMounts.AddDefaulted_GetRef();
        Def.WeaponId = FName(*FString::Printf(TEXT("Weapon_%d"), I));
        Def.Type = Family(Weapon.Kind);
        Def.GeometryScale = WeaponGeometryScale(Hull.Size);
        Def.Tier = Mount.Tier;
        Def.ExplosionTierCoefficient = TierDamage[Mount.Tier - 1];
        Def.MaxHP = Weapon.HP * TierDamage[Mount.Tier - 1];
        if(Def.Type==EOVWeaponType::Missile)
            Def.PresentationProfile=Weapon.Kind==EOVAttackKind::GuidedMissile ? TEXT("GuidedMissilePod") : TEXT("StandardMissilePod");
        const double X = FMath::Lerp(-.8, .8, (I + 1.0) / (Hull.MountCount + 1.0));
        Def.LocalPosition = FVector(X * Layout->LocalHalfExtent.X, 0,
            (Mount.bTop ? 1 : -1) * (Layout->HalfHeight + 12));
        if (Def.Type == EOVWeaponType::Beam) ++Layout->BeamSlots;
        else if (Def.Type == EOVWeaponType::Cannon) ++Layout->CannonSlots;
        else ++Layout->MissileSlots;
    }

    // Build ordered desired facility list: mandatory weapon supply rooms first (never merged/deleted),
    // followed by role-essential and tactical support rooms.
    TArray<EOVFacilityType> DesiredFacilities;
    if (Hull.MountCount > 0) DesiredFacilities.Add(EOVFacilityType::WeaponControl);
    if (Layout->CannonSlots > 0) DesiredFacilities.Add(EOVFacilityType::Ammo);
    if (Layout->BeamSlots > 0) DesiredFacilities.Add(EOVFacilityType::Capacitor);
    if (Layout->MissileSlots > 0) DesiredFacilities.Add(EOVFacilityType::MissileStorage);
    if (HullId == TEXT("repair") || HullId == TEXT("mining") || HullId == TEXT("transport"))
        DesiredFacilities.Add(EOVFacilityType::ResourceStorage);
    if (HullId == TEXT("assault"))
        DesiredFacilities.Add(EOVFacilityType::Boarding);
    TArray<EOVFacilityType> RequiredFacilities = DesiredFacilities;
    if (Rule.bAircraftCapable)
    {
        RequiredFacilities.Add(EOVFacilityType::Hangar);
        RequiredFacilities.Add(EOVFacilityType::FighterControl);
    }
    if (HullId == TEXT("air_defense")) RequiredFacilities.Add(EOVFacilityType::Interception);
    if (HullId == TEXT("radar")) RequiredFacilities.Add(EOVFacilityType::Radar);
    if (Hull.Shield > 0.f) DesiredFacilities.Add(EOVFacilityType::ShieldControl);
    if (Hull.PD > 0) DesiredFacilities.AddUnique(EOVFacilityType::Interception);
    DesiredFacilities.AddUnique(EOVFacilityType::Radar);

    auto Install = [&](EOVFacilityType Type, int32 Count = 1) -> bool
    {
        int32 Existing=0;
        for (const auto& Slot : Layout->Slots)
            if (Slot.Facility == Type) ++Existing;
        if (Existing >= Count) return true;
        for (auto& Slot : Layout->Slots)
        {
            if (Slot.Facility != EOVFacilityType::None) continue;
            if (!Slot.AllowedFacilities.Contains(Type)) continue;
            Slot.Facility = Type;
            Slot.MaxFacilityHP = 100.f;
            Slot.RequiredCrew = (Type == EOVFacilityType::None || Type == EOVFacilityType::Core ||
                                 Type == EOVFacilityType::Security) ? 0.f : 10.f;
            if (++Existing >= Count) return true;
        }
        return false;
    };

    TArray<EOVFacilityType> AuditList = {
        EOVFacilityType::Core, EOVFacilityType::Bridge, EOVFacilityType::LifeSupport, EOVFacilityType::Engine
    };
    AuditList.Append(RequiredFacilities);
    for (auto Fac : DesiredFacilities) Install(Fac);
    if (Rule.bAircraftCapable)
    {
        Install(EOVFacilityType::Boarding);
        Install(EOVFacilityType::Hangar, 2);
        Install(EOVFacilityType::FighterControl);
    }

    // Explicitly audit and log Small-7 / Medium-9 capacity conflicts without merging or dropping mandatory facilities.
    TArray<EOVFacilityType> Unplaced;
    FString AuditReport;
    const bool bCapacity = UShipLayoutDataAsset::AuditFacilityCapacity(Layout, AuditList, Unplaced, AuditReport);
    for (const auto Facility : AuditList)
        if (!Layout->Slots.ContainsByPredicate([Facility](const FOVSlotDefinition& Slot){return Slot.Facility==Facility;}))
            Unplaced.AddUnique(Facility);
    if (!bCapacity || !Unplaced.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("D126 launch rejected [%s]: %s"), Hull.Key, *AuditReport);
        return nullptr;
    }

    if (CVarOVMarkedMounts.GetValueOnGameThread() == 1)
    {
        ApplyMarkedAttackMounts(Hull, *Layout);
        for (const auto& Candidate : HullCollisionCandidates)
            if (FName(Candidate.Hull) == HullId)
            {
                const FVector Low = (Candidate.Center - Candidate.HalfExtent).ComponentMax(-Extent);
                const FVector High = (Candidate.Center + Candidate.HalfExtent).ComponentMin(Extent);
                if ((High - Low).GetMin() <= UE_SMALL_NUMBER) continue;
                auto& Box = Layout->CombatHullBoxes.AddDefaulted_GetRef();
                Box.LocalCenter = (High + Low) * .5;
                Box.HalfExtent = (High - Low) * .5;
            }
    }

    TArray<FString> Errors;
    if (Layout->ValidateLayout(Errors)) return Layout;
    UE_LOG(LogTemp, Error, TEXT("D126 invalid %s layout: %s"), Hull.Key, *FString::Join(Errors, TEXT("; ")));
    return nullptr;
}
}

bool OVCalibration::GetHullMetadata(FName HullId, FOVHullMetadata& OutMetadata)
{
    OutMetadata = {};
    const FHull* Hull = FindHull(HullId);
    if (!Hull) return false;
    const FD126HullRule Rule = GetD126HullRule(HullId);
    const EOVHullSizeClass SizeClass = ToHullSizeClass(Hull->Size);

    OutMetadata.SizeClass = Hull->Size;
    OutMetadata.FleetCost = Hull->FleetCost;
    OutMetadata.DeckCount = Hull->Decks;
    UShipLayoutDataAsset::ComputeExpectedD126DeckSlots(SizeClass, Hull->Decks, Rule.bTopHeavy, OutMetadata.DeckSlotCounts);
    OutMetadata.TotalInteriorSlots = 0;
    for (int32 Count : OutMetadata.DeckSlotCounts) OutMetadata.TotalInteriorSlots += Count;
    OutMetadata.MaxHangarInstallations = Rule.MaxHangars;
    OutMetadata.bAllowAircraftOperations = Rule.bAircraftCapable;
    OutMetadata.MoveSpeedCmPerSec = Hull->Speed;
    OutMetadata.AttackMountCount = Hull->MountCount;
    OutMetadata.PointDefenseCount = Hull->PD;
    OutMetadata.ArmorMaxReduction = Rule.ArmorMaxReduction;

    if (Hull->MountCount > 0) OutMetadata.RoleTags.Add(TEXT("공격"));
    // Explicit documented specializations; neither PD count nor weapon count infers a role.
    if (HullId == TEXT("corvette")) OutMetadata.RoleTags.Add(TEXT("정찰"));
    if (HullId == TEXT("frigate")) OutMetadata.RoleTags.Add(TEXT("호위"));
    if (HullId == TEXT("air_defense")) OutMetadata.RoleTags.Add(TEXT("방공"));
    if (HullId == TEXT("radar")) OutMetadata.RoleTags.Add(TEXT("탐지 지원"));
    if (Rule.bAircraftCapable) OutMetadata.RoleTags.Add(TEXT("함재기 운용"));
    if (HullId == TEXT("missile_ship")) OutMetadata.RoleTags.Add(TEXT("미사일"));
    if (HullId == TEXT("repair")) OutMetadata.RoleTags.Add(TEXT("수리"));
    if (HullId == TEXT("mining")) OutMetadata.RoleTags.Add(TEXT("채굴"));
    if (HullId == TEXT("transport")) OutMetadata.RoleTags.Add(TEXT("수송"));
    if (HullId == TEXT("assault")) OutMetadata.RoleTags.Add(TEXT("강습"));

    const TCHAR* HangarDesc = Rule.MaxHangars <= 0 ? TEXT("격납 불가")
        : (Rule.bAircraftCapable ? TEXT("함재기 운용 가능") : TEXT("소형함 격납 전용(함재기 불가)"));
    OutMetadata.Description = FString::Printf(
        TEXT("D126 기준 · 체급 %d · 편성 코스트 %d · %d층(%d슬롯) · 격납 %d(%s) · 선체 %.0f · 쉴드 %.0f · 장갑감쇠 %.0f%% · 속도 %.0fm/s(%.0fcm/s) · 선회 %.1f도/s · 레이더 T%d · 공격 %d문 / PD %d"),
        Hull->Size, Hull->FleetCost, Hull->Decks, OutMetadata.TotalInteriorSlots,
        Rule.MaxHangars, HangarDesc, Hull->HP, Hull->Shield, Rule.ArmorMaxReduction * 100.f,
        Hull->Speed / 100.f, Hull->Speed, Hull->Turn, Hull->RadarTier, Hull->MountCount, Hull->PD);
    if (HullId == TEXT("corvette")) OutMetadata.Description += TEXT(" · 7칸: 쉴드·PD·자체 레이더 미가동");
    if (HullId == TEXT("frigate")) OutMetadata.Description += TEXT(" · 7칸: PD·자체 레이더 미가동");
    if (HullId == TEXT("vector") || HullId == TEXT("assault") || HullId == TEXT("repair") || HullId == TEXT("mining"))
        OutMetadata.Description += TEXT(" · 자체 레이더 미설치");
    if (HullId == TEXT("transport")) OutMetadata.Description += TEXT(" · R37 중형 1층 시험 배치 · 적재량·내구·속도 임시");
    return true;
}

FString OVCalibration::GetHullName(FName HullId)
{
    static const TMap<FName, FString> Names = {
        {TEXT("corvette"),TEXT("초계함")}, {TEXT("frigate"),TEXT("호위함")}, {TEXT("vector"),TEXT("백터급 구축함")},
        {TEXT("bastion"),TEXT("바스티온급 구축함")}, {TEXT("cruiser"),TEXT("순양함")}, {TEXT("air_defense"),TEXT("방공함")},
        {TEXT("radar"),TEXT("레이더함")}, {TEXT("battleship"),TEXT("전함")}, {TEXT("carrier"),TEXT("항공모함")},
        {TEXT("missile_ship"),TEXT("전략 미사일함")}, {TEXT("dreadnought"),TEXT("구축전함")}, {TEXT("battlecarrier"),TEXT("항공순양함")},
        {TEXT("repair"),TEXT("보급수리함")}, {TEXT("mining"),TEXT("채굴선")}, {TEXT("transport"),TEXT("수송선")}, {TEXT("assault"),TEXT("강습선")}
    };
    if (const FString* Name = Names.Find(HullId)) return *Name;
    return FString();
}

double OVCalibration::GetHullGeometryScale(FName HullId)
{
    const FHull* Hull = FindHull(HullId);
    if (!Hull) return 0;
    // Use the fitted source bounding envelope for every hull. It remains defined for
    // open or disconnected art and uses the same fit as ApplyPresentationHull.
    constexpr double VolumeRatio[] = {1., 5., 25., 125.};
    const auto BoxVolume = [](const FVector& E){return 8.*E.X*E.Y*E.Z;};
    const double Reference = BoxVolume(FittedHullExtent(TEXT("corvette")));
    const double Source = BoxVolume(FittedHullExtent(HullId));
    if (Reference <= 0 || Source <= 0 || Hull->Size < 1 || Hull->Size > 4) return 0.;
    return 6. * FMath::Pow(VolumeRatio[Hull->Size-1] * Reference / Source, 1./3.);
}

UStaticMesh* OVCalibration::LoadHullMesh(FName MeshId)
{
    if (!FindHull(MeshId)) return nullptr;
    FString Name = MeshId.ToString();
    Name[0] = FChar::ToUpper(Name[0]);
    for (int32 I = 1; I < Name.Len(); ++I)
        if (Name[I-1] == TEXT('_')) Name[I] = FChar::ToUpper(Name[I]);
    const FString Path = MeshId == TEXT("corvette") ? TEXT("/Game/Presentation/Ships/SM_PC14_FullMesh.SM_PC14_FullMesh") :
        FString::Printf(TEXT("/Game/Presentation/Ships/%sFull/SM_Hull.SM_Hull"), *Name);
    return LoadObject<UStaticMesh>(nullptr, *Path, nullptr, LOAD_NoWarn);
}

FVector OVCalibration::GetHullWorldHalfExtent(FName HullId)
{
    return FindHull(HullId) ? FittedHullExtent(HullId) * GetHullGeometryScale(HullId) : FVector::ZeroVector;
}

TArray<FName> OVCalibration::GetHullIds()
{
    TArray<FName> Result;
    for (const FHull& Hull : Hulls) Result.Add(FName(Hull.Key));
    return Result;
}

TArray<FName> OVCalibration::GetWeaponProfileIds()
{
    TArray<FName> Result;
    for (const FWeapon& Weapon : Weapons)
        for (int32 Tier = Weapon.MinTier; Tier <= 4; ++Tier)
            Result.Add(ProfileId(Weapon, Tier));
    return Result;
}

FString OVCalibration::GetWeaponProfileDescription(FName RequestedId)
{
    for (const FWeapon& Weapon : Weapons)
        for (int32 Tier = Weapon.MinTier; Tier <= 4; ++Tier)
            if (ProfileId(Weapon, Tier) == RequestedId)
                return FString::Printf(TEXT("D116 후보 T%d · 피해 %.1f · 주기 %.2f초 · 사거리 %.0fcm · 속도 %.0fcm/s · 일제사 %d발"),
                    Tier, Weapon.Damage * TierDamage[Tier - 1], Weapon.Period, Weapon.Range, Weapon.Speed, Weapon.Salvo);
    return FString();
}

FString OVCalibration::GetSourceHash() { return SourceHash; }

void OVCalibration::BeginSession(UOVCombatSubsystem& Combat, int32 Seed)
{
    Combat.EnableCombat({}, Seed);
    ApplyProfiles(*Combat.Settings);
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
        if (!Roster[I] || GetHullGeometryScale(FName(Keys[I])) <= 0.) return false;
        Layouts[I] = MakeLayout(Ships[I], *Roster[I]);
        if (!Layouts[I]) return false;
        if(CVarOVMarkedMounts.GetValueOnGameThread()==1)
        {
            bool HasControl=false;for(const auto& Slot:Layouts[I]->Slots)HasControl|=Slot.Facility==EOVFacilityType::Interception;
            if(!HasControl)for(auto& Slot:Layouts[I]->Slots)if(Slot.Facility==EOVFacilityType::None) {
                Slot.Facility=EOVFacilityType::Interception;Slot.AllowedFacilities.AddUnique(Slot.Facility);
                Slot.MaxFacilityHP=100;Slot.RequiredCrew=10;break;
            }
            AppendDefenseMounts(*Roster[I],*Layouts[I]);
        }
    }

    BeginSession(Combat, 260112);
    auto* Defense=Combat.GetWorld()->GetSubsystem<UOVPointDefenseSubsystem>();
    if(Defense&&CVarOVMarkedMounts.GetValueOnGameThread()==1)Defense->Reset(260112);
    const FVector Positions[] = {FVector(-33000,-6200,2000), FVector(-37000,10500,2000), FVector(33000,0,2000)};
    for (int32 I = 0; I < 3; ++I)
    {
        auto* Ship = Ships[I];
        const auto& Hull = *Roster[I];
        const bool bFriendly = I != 2;
        const bool bInactive = bDuel && I == 1;
        const double Scale = GetHullGeometryScale(FName(Keys[I]));
        Ship->Configure(I==0?TEXT("백터"):I==1?TEXT("초계함"):Scenario==2?TEXT("미사일함"):TEXT("바스티온"), bFriendly, Hull.Speed, Hull.Turn, FVector(Scale),
            bFriendly ? FLinearColor(.12f,.65f,1) : FLinearColor(1,.12f,.08f));
        Ship->ConfigureClassMotion(Hull.Size);
        Ship->SetCalibrationHull(&Layouts[I]->LocalHalfExtent);
        const FD126HullRule Rule = GetD126HullRule(FName(Keys[I]));
        auto& Systems = *Ship->Systems;
        Systems.Tuning = GetDefault<UOVShipSystemsComponent>()->Tuning;
        Systems.Tuning.ShieldBase = Hull.Shield;
        Systems.Tuning.ExplosionBase = 40; // D116 provisional balance
        Systems.Tuning.ArmorMaxReduction = Rule.ArmorMaxReduction;
        Systems.Tuning.ShieldRegenFraction = ShieldCombat;
        Systems.Tuning.ShieldNonCombatRegenFraction = ShieldNonCombat;
        Systems.Tuning.NoHitRecoveryInterval = ShieldTransition;
        Systems.Tuning.ShieldRecoveryDelay = ShieldDelay;
        Systems.Tuning.CrewDamageCoefficient = CrewInjury;
        Systems.FactionId = bFriendly ? 0 : 1;
        Systems.Initialize(Layouts[I]);
        Ship->ApplyPresentationHull(FName(Keys[I]));
        Ship->SetActorLocationAndRotation(Positions[I], FRotator(0,bFriendly ? 0 : 180,0));
        Ship->DebugResetMotion();
        Ship->SetAutomation(!bFriendly,!bFriendly);
        Ship->SetActorHiddenInGame(bInactive);
        Ship->SetActorEnableCollision(!bInactive);
        Combat.RegisterShip(Ship, Hull.RadarTier, false, Hull.Size);
        if (bInactive) { Combat.NotifyFTLDeparture(Ship); continue; }
        if(Defense&&CVarOVMarkedMounts.GetValueOnGameThread()==1)Defense->RegisterShip(Ship,Hull.PD);

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

bool OVCalibration::ConfigureHull(UOVCombatSubsystem& Combat, AOVShip& Ship, FName HullId, bool Friendly, FVector Position, double Scale)
{
    const FHull* Hull=FindHull(HullId);
    if(!Hull || !Combat.Settings || !FMath::IsFinite(Scale) || Scale <= 0.) return false;
    const FD126HullRule Rule = GetD126HullRule(HullId);
    UShipLayoutDataAsset* Layout=MakeLayout(&Ship,*Hull);
    if(!Layout) return false;
    auto Install=[&](EOVFacilityType Type, int32 Count) {
        if (Type == EOVFacilityType::Hangar && Layout->MaxHangarInstallations <= 0) return;
        if (Type == EOVFacilityType::FighterControl && !Layout->bAllowAircraftOperations) return;
        int32 Existing=0;
        for(const auto& S:Layout->Slots) if(S.Facility==Type)++Existing;
        const int32 TargetCount = (Type == EOVFacilityType::Hangar && Layout->MaxHangarInstallations >= 0)
            ? FMath::Min(Count, Layout->MaxHangarInstallations) : Count;
        for(auto& S:Layout->Slots) if(Existing<TargetCount && S.Facility==EOVFacilityType::None && S.AllowedFacilities.Contains(Type)) {
            S.Facility=Type; S.MaxFacilityHP=100; S.RequiredCrew=10; ++Existing;
        }
    };
    if (HullId == TEXT("repair") || HullId == TEXT("mining") || HullId == TEXT("transport"))
        Install(EOVFacilityType::ResourceStorage, 1);
    Install(EOVFacilityType::Interception,1);
    Install(EOVFacilityType::Boarding,1);
    if(Rule.bAircraftCapable && Rule.MaxHangars > 0) { Install(EOVFacilityType::Hangar,2); Install(EOVFacilityType::FighterControl,1); }
    AppendDefenseMounts(*Hull,*Layout);
    Ship.Configure(HullId.ToString().ToUpper(),Friendly,Hull->Speed,Hull->Turn,FVector(Scale),Friendly?FLinearColor(.12f,.65f,1):FLinearColor(1,.15f,.08f));
    Ship.ConfigureClassMotion(Hull->Size);
    Ship.SetCalibrationHull(&Layout->LocalHalfExtent);
    auto& S=*Ship.Systems;
    S.Tuning=GetDefault<UOVShipSystemsComponent>()->Tuning; S.Tuning.ShieldBase=Hull->Shield; S.Tuning.ExplosionBase=40; // D116 provisional balance
    S.Tuning.ArmorMaxReduction = Rule.ArmorMaxReduction;
    S.Tuning.ShieldRegenFraction=ShieldCombat; S.Tuning.ShieldNonCombatRegenFraction=ShieldNonCombat;
    S.Tuning.NoHitRecoveryInterval=ShieldTransition; S.Tuning.ShieldRecoveryDelay=ShieldDelay; S.Tuning.CrewDamageCoefficient=CrewInjury;
    S.FactionId=Friendly?0:1; S.Initialize(Layout);
    Ship.ApplyPresentationHull(HullId);
    Ship.SetActorHiddenInGame(false); Ship.SetActorEnableCollision(true);
    Ship.SetActorLocationAndRotation(Position,FRotator(0,Friendly?0:180,0)); Ship.DebugResetMotion(); Ship.SetAutomation(false,false);
    Combat.RegisterShip(&Ship,Hull->RadarTier,HullId==TEXT("radar"),Hull->Size);
    for(int32 I=0;I<Hull->MountCount;++I) Combat.SetWeaponProfile(&Ship,Layout->WeaponMounts[I].WeaponId,ProfileId(Weapons[Hull->Mounts[I].Weapon],Hull->Mounts[I].Tier));
    return true;
}

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVMarkedEquippedMountsTest,
    "OrbitalVanguard.Revision.MarkedEquippedMounts",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOVMarkedEquippedMountsTest::RunTest(const FString&)
{
    const int32 Previous=CVarOVMarkedMounts.GetValueOnGameThread();
    CVarOVMarkedMounts->Set(1,ECVF_SetByConsole);
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
    TArray<AOVShip*> Ships={World->SpawnActor<AOVShip>(),World->SpawnActor<AOVShip>(),World->SpawnActor<AOVShip>()};
    UOVCombatSubsystem* Combat=World->GetSubsystem<UOVCombatSubsystem>();
    const bool Started=OVCalibration::Start(*Combat,Ships,1);
    TestTrue(TEXT("Calibration combat starts"),Started);
    if(Started)
    {
        auto* Defense=World->GetSubsystem<UOVPointDefenseSubsystem>();
        for(AOVShip* Ship:Ships)
        {
            const auto PD=Defense->GetShipState(Ship);
            TestTrue(TEXT("F6 registers real PD mounts"),!PD.Mounts.IsEmpty());
            const bool bHasInterceptionRoom = Ship->Systems->Layout->Slots.ContainsByPredicate(
                [](const FOVSlotDefinition& Slot) { return Slot.Facility == EOVFacilityType::Interception; });
            for(const auto& Mount:PD.Mounts)
            {
                TestTrue(TEXT("F6 PD maps to the authored layout"), Mount.bLayoutMount);
                if (bHasInterceptionRoom)
                {
                    TestTrue(TEXT("F6 PD available when Interception facility fits"), Ship->Systems->IsWeaponAvailable(Mount.MountId));
                }
            }
        }
    }
    for (const FHull& RawHull : Hulls)
    {
        const FHull& Hull = *FindHull(FName(RawHull.Key));
        const auto Candidates = OVMarkedSlots::GetCandidates(FName(Hull.Key));
        if (Candidates.IsEmpty()) continue;
        auto* Layout = MakeLayout(GetTransientPackage(), Hull);
        if (!TestNotNull(TEXT("Marked layout validates"), Layout)) continue;
        TestTrue(TEXT("Marked ship receives nonempty candidate hull boxes"),Layout->CombatHullBoxes.Num()>0 && Layout->CombatHullBoxes.Num()<=32);
        int32 MarkedAttackCount = 0;
        int32 MarkedPDCount = 0;
        for (const auto& C : Candidates)
        {
            if (C.Kind == EOVMarkedSlotKind::Interception) ++MarkedPDCount;
            else ++MarkedAttackCount;
        }
        TSet<FName> Used;
        for (int32 I = 0; I < Hull.MountCount; ++I)
        {
            const auto& Mount = Layout->WeaponMounts[I];
            if (Mount.AuthoredSlotId.IsNone()) continue;
            const auto* Found = Candidates.FindByPredicate([&](const FOVMarkedSlotCandidate& C) {
                return C.Kind != EOVMarkedSlotKind::Interception && C.LocalPosition.Equals(Mount.LocalPosition, .001);
            });
            if (!TestNotNull(TEXT("Equipped weapon uses a marked attack slot"), Found)) continue;
            TestFalse(TEXT("Two equipped weapons cannot share a marked slot"), Used.Contains(Found->SlotId));
            TestEqual(TEXT("Authored slot ID stays separate from weapon ID"),Mount.AuthoredSlotId,Found->SlotId);
            Used.Add(Found->SlotId);
            TestTrue(TEXT("Mount orientation follows source surface"), Mount.LocalSurfaceNormal.Equals(Found->LocalNormal, .001));
            if (Hull.Mounts[I].Weapon == 6) TestTrue(TEXT("Spinal weapon occupies dedicated slot"), Found->Kind == EOVMarkedSlotKind::Dedicated);
            if (Found->Kind == EOVMarkedSlotKind::Dedicated)
                TestEqual(TEXT("Dedicated slot has an explicit profile constraint"),Mount.RequiredProfileId,ProfileId(Weapons[Hull.Mounts[I].Weapon],Hull.Mounts[I].Tier));
        }
        TestEqual(TEXT("All available marked attack slots bound without overlap"), Used.Num(), FMath::Min(MarkedAttackCount, Hull.MountCount));
        // Each hull gets a fresh combat registration; RegisterShip intentionally
        // does not replace an existing ship's equipped runtime state.
        Combat->Ships.RemoveAll([&Ships](const FOVCombatShipState& S){return S.Ship==Ships[0];});
        if (Started && TestTrue(TEXT("Marked ship configures"), OVCalibration::ConfigureHull(*Combat,*Ships[0],FName(Hull.Key),true,FVector::ZeroVector,OVCalibration::GetHullGeometryScale(FName(Hull.Key)))))
        {
            TSet<FName> UsedPD;
            for (const auto& Mount : Ships[0]->Systems->Layout->WeaponMounts)
            {
                if (!Mount.RequiredProfileId.IsNone())
                {
                    TestTrue(TEXT("Dedicated slot retains its designated weapon"),Combat->SetWeaponProfile(Ships[0],Mount.WeaponId,Mount.RequiredProfileId));
                    const auto* Other=Combat->Settings->Weapons.FindByPredicate([&](const FOVWeaponProfile& P){
                        return P.Family==Mount.Type && P.Tier==FMath::RoundToInt(Mount.Tier) && P.ProfileId!=Mount.RequiredProfileId;
                    });
                    if (Other) TestFalse(TEXT("Dedicated slot rejects another same-tier family profile"),Combat->SetWeaponProfile(Ships[0],Mount.WeaponId,Other->ProfileId));
                }
                if (Mount.Type != EOVWeaponType::Interception || Mount.AuthoredSlotId.IsNone()) continue;
                const auto* Found=Candidates.FindByPredicate([&](const FOVMarkedSlotCandidate& C) {
                    return C.Kind == EOVMarkedSlotKind::Interception && C.LocalPosition.Equals(Mount.LocalPosition,.001);
                });
                if (!TestNotNull(TEXT("PD uses a marked interception slot"),Found)) continue;
                TestEqual(TEXT("PD records marked slot identity"),Mount.AuthoredSlotId,Found->SlotId);
                TestFalse(TEXT("PD slots are independently equipped"),UsedPD.Contains(Found->SlotId));
                UsedPD.Add(Found->SlotId);
            }
            TestEqual(TEXT("All marked PD slots have one actual mount"), UsedPD.Num(), FMath::Min(MarkedPDCount, Hull.PD));
        }
    }
    World->DestroyWorld(false);
    CVarOVMarkedMounts->Set(Previous,ECVF_SetByConsole);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVVolumeRatioTrial,
    "OrbitalVanguard.Revision.VolumeRatioTrial",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOVVolumeRatioTrial::RunTest(const FString&)
{
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
    TArray<AOVShip*> Ships={World->SpawnActor<AOVShip>(),World->SpawnActor<AOVShip>(),World->SpawnActor<AOVShip>()};
    auto* Combat=World->GetSubsystem<UOVCombatSubsystem>();
    if(TestTrue(TEXT("F6 scene starts"),OVCalibration::Start(*Combat,Ships,1)))
    {
        const auto Volume=[](const AOVShip* Ship) {
            const FVector E=Ship->Hull->GetStaticMesh()->GetBounds().BoxExtent * Ship->Hull->GetComponentScale();
            return 8.*E.X*E.Y*E.Z;
        };
        const double Base=Volume(Ships[1]);
        const double ClassRatios[]={1.,5.,25.,125.};
        for (FName HullId : OVCalibration::GetHullIds())
        {
            FOVHullMetadata Meta;
            if (!TestTrue(TEXT("Hull metadata exists"),OVCalibration::GetHullMetadata(HullId,Meta))) continue;
            if (TestTrue(TEXT("Actual source hull configures"),OVCalibration::ConfigureHull(*Combat,*Ships[0],HullId,true,FVector::ZeroVector,OVCalibration::GetHullGeometryScale(HullId))))
                TestTrue(FString::Printf(TEXT("%s actual fitted volume ratio"),*HullId.ToString()),FMath::IsNearlyEqual(Volume(Ships[0])/Base,ClassRatios[Meta.SizeClass-1],.001));
        }
    }
    World->DestroyWorld(false);return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVHullPresentationEnvelope, "OrbitalVanguard.Presentation.FittedHullAndWeapons",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOVHullPresentationEnvelope::RunTest(const FString&)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game,false);
    auto* Combat = World->GetSubsystem<UOVCombatSubsystem>();
    OVCalibration::BeginSession(*Combat,126);
    auto* Defense = World->GetSubsystem<UOVPointDefenseSubsystem>();
    Defense->Reset(126);
    for (FName Key : OVCalibration::GetHullIds())
    {
        AOVShip* Ship = World->SpawnActor<AOVShip>();
        if (!TestTrue(Key.ToString(), OVCalibration::ConfigureHull(*Combat,*Ship,Key,true,FVector::ZeroVector,OVCalibration::GetHullGeometryScale(Key)))) continue;
        TestTrue(Key.ToString()+TEXT(" point defense installed"),Defense->RegisterShip(Ship,FindHull(Key)->PD));
        Ship->RefreshWeaponVisuals();
        const FVector Axes = Ship->GetLocalShieldRadii();
        const auto ToLocal = [Ship](USceneComponent* Component, FVector Point) {
            return Ship->GetActorTransform().InverseTransformPosition(Component->GetComponentTransform().TransformPosition(Point));
        };
        TArray<UStaticMeshComponent*> Parts;
        Ship->GetComponents(Parts);
        double WorstQ = 0;
        int32 PartsChecked = 0;
        for (UStaticMeshComponent* Part : Parts)
        {
            if (!Part->IsVisible() || !Part->GetStaticMesh()) continue;
            const USceneComponent* Parent = Part;
            while (Parent && !Parent->IsA<UOVWeaponVisualComponent>()) Parent = Parent->GetAttachParent();
            if (!Parent && Part != Ship->Hull) continue;
            const auto& LOD = Part->GetStaticMesh()->GetRenderData()->LODResources[0];
            const auto& Vertices = LOD.VertexBuffers.PositionVertexBuffer;
            ++PartsChecked;
            for (uint32 I=0; I<Vertices.GetNumVertices(); ++I)
                WorstQ = FMath::Max(WorstQ,(ToLocal(Part,FVector(Vertices.VertexPosition(I)))/Axes).SizeSquared());
        }
        TestTrue(Key.ToString()+TEXT(" visible hull and weapon meshes"),PartsChecked>1);
        TestTrue(FString::Printf(TEXT("%s LOD0 inside shield (q²=%.4f)"),*Key.ToString(),WorstQ),WorstQ>0 && WorstQ<1);
        const auto& LOD = Ship->Hull->GetStaticMesh()->GetRenderData()->LODResources[0];
        for (const auto& M : Ship->Systems->Layout->WeaponMounts)
        {
            if (!M.AuthoredSlotId.IsNone()) continue;
            double Gap = DBL_MAX;
            for (int32 I=0; I+2<LOD.IndexBuffer.GetNumIndices(); I+=3)
            {
                const auto P = [&](int32 J) { return ToLocal(Ship->Hull,FVector(LOD.VertexBuffers.PositionVertexBuffer.VertexPosition(LOD.IndexBuffer.GetIndex(J)))); };
                const FVector A=P(I), B=P(I+1), C=P(I+2);
                if (FVector::CrossProduct(B-A,C-A).IsNearlyZero()) continue;
                Gap=FMath::Min(Gap,FVector::Distance(M.LocalPosition,FMath::ClosestPointOnTriangleToPoint(M.LocalPosition,A,B,C)));
                if (Gap<.05) break;
            }
            TestTrue(FString::Printf(TEXT("%s/%s surface gap %.3f cm"),*Key.ToString(),*M.WeaponId.ToString(),Gap),Gap<.1);
        }
        AddInfo(FString::Printf(TEXT("%s parts=%d max_q_squared=%.4f"),*Key.ToString(),PartsChecked,WorstQ));
    }
    World->DestroyWorld(false);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVD126LaunchAudit, "OrbitalVanguard.D126.LaunchAuditAndMountGeometry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOVD126LaunchAudit::RunTest(const FString&)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game,false);
    auto* Combat = World->GetSubsystem<UOVCombatSubsystem>();
    OVCalibration::BeginSession(*Combat,126);
    for (FName Key : OVCalibration::GetHullIds())
    {
        AOVShip* Ship=World->SpawnActor<AOVShip>();
        if (!TestTrue(Key.ToString(),OVCalibration::ConfigureHull(*Combat,*Ship,Key,true,FVector::ZeroVector,OVCalibration::GetHullGeometryScale(Key)))) continue;
        const auto& L=*Ship->Systems->Layout;
        const auto* H=FindHull(Key);
        for (const auto& Mount:L.WeaponMounts)
        {
            const float Scale=H->Size==4?.32f:H->Size==3?.45f:1.f;
            TestEqual(TEXT("Weapon geometry class coefficient"),Mount.GeometryScale,Scale);
            if (Mount.Type==EOVWeaponType::Cannon) TestTrue(TEXT("Cannon supplied"),Ship->Systems->CanFireCannons());
            if (Mount.Type==EOVWeaponType::Beam) TestTrue(TEXT("Beam supplied"),Ship->Systems->CanFireBeams());
            if (Mount.Type==EOVWeaponType::Missile) TestTrue(TEXT("Missile supplied"),Ship->Systems->CanFireMissiles());
        }
        TestEqual(TEXT("All external mounts retained"),L.WeaponMounts.Num(),H->MountCount+H->PD);
        const int32 Installed=L.Slots.FilterByPredicate([](const FOVSlotDefinition& Slot){return Slot.Facility!=EOVFacilityType::None;}).Num();
        UE_LOG(LogTemp,Display,TEXT("D126_DESIGN %s class=%d cost=%d rooms=%d/%d armor=%d attack=%d pd=%d shield=%d radar=%d pd_online=%d"),
            *Key.ToString(),H->Size,H->FleetCost,Installed,L.Slots.Num(),L.ArmorSections.Num(),H->MountCount,H->PD,
            Ship->Systems->bShieldActive,Ship->Systems->HasRadar(),Ship->Systems->CanIntercept());
        if (Key==TEXT("transport"))
        {
            TestEqual(TEXT("R37 medium"),H->Size,2); TestEqual(TEXT("R37 fleet cost"),H->FleetCost,5);
            TestTrue(TEXT("Medium cargo design installed"),L.Slots.Num()==9 && Ship->Systems->GetEfficiency(EOVFacilityType::ResourceStorage)>0);
        }
    }
    // Four base rooms + weapon control + three distinct supply rooms need eight slots.
    FHull Impossible=*FindHull(TEXT("corvette"));
    Impossible.MountCount=3; Impossible.Mounts[2]={4,1,true};
    AddExpectedError(TEXT("D126 launch rejected"),EAutomationExpectedErrorFlags::Contains,1);
    TestNull(TEXT("Mixed battery lacking a required room cannot launch"),MakeLayout(GetTransientPackage(),Impossible));
    World->DestroyWorld(false);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVD116CatalogTest, "OrbitalVanguard.Revision.D116CandidateCatalog",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOVD116CatalogTest::RunTest(const FString&)
{
    TestEqual(TEXT("All candidate hull IDs retained"), int32(UE_ARRAY_COUNT(Hulls)), 16);
    const FHull* Vector = FindHull(TEXT("vector"));
    const FHull* Bastion = FindHull(TEXT("bastion"));
    const FHull* Dreadnought = FindHull(TEXT("dreadnought"));
    if (!Vector || !Bastion || !Dreadnought) return false;
    TestTrue(TEXT("D126 distinct destroyer deck, attack, PD, and speed metadata"),
        Vector->Decks == 1 && Vector->MountCount == 4 && Vector->PD == 3 && FMath::IsNearlyEqual(Vector->Speed, 14800.f)
        && Bastion->Decks == 2 && Bastion->MountCount == 7 && Bastion->PD == 4 && FMath::IsNearlyEqual(Bastion->Speed, 8800.f));
    TestEqual(TEXT("Extra-large hull HP remains absolute"), Dreadnought->HP, 825000.f);
    const FHull* Battlecarrier = FindHull(TEXT("battlecarrier"));
    const FHull* Carrier = FindHull(TEXT("carrier"));
    for (const FHull* Hull : {Carrier, Battlecarrier})
    {
        if (!Hull) { AddError(TEXT("Missing carrier hull")); continue; }
        UShipLayoutDataAsset* Layout = MakeLayout(GetTransientPackage(), *Hull);
        if (!TestNotNull(TEXT("Carrier layout exists"), Layout)) continue;
        const int32 Limit = Hull == Carrier ? 4 : 6;
        TestEqual(TEXT("D126 hangar installation limit"), Layout->MaxHangarInstallations, Limit);
        TestTrue(TEXT("D126 carrier/battlecarrier allow aircraft operations"), Layout->bAllowAircraftOperations);
        int32 Installed = Layout->Slots.FilterByPredicate([](const FOVSlotDefinition& Slot) {
            return Slot.Facility == EOVFacilityType::Hangar;
        }).Num();
        for (auto& Slot : Layout->Slots)
        {
            if (Slot.Facility != EOVFacilityType::None || !Slot.AllowedFacilities.Contains(EOVFacilityType::Hangar)) continue;
            Slot.Facility = EOVFacilityType::Hangar;
            Slot.MaxFacilityHP = 100;
            Slot.RequiredCrew = 10;
            ++Installed;
            TArray<FString> Errors;
            if (Installed == Limit)
            {
                const bool bValid = Layout->ValidateLayout(Errors);
                TestTrue(FString::Printf(TEXT("At hangar limit accepted: %s"), *FString::Join(Errors, TEXT("; "))), bValid);
            }
            if (Installed == Limit + 1)
            {
                TestFalse(TEXT("Above hangar limit rejected"), Layout->ValidateLayout(Errors));
                TestTrue(TEXT("Limit failure identified"), Errors.Contains(TEXT("Hangar installation limit exceeded")));
                break;
            }
        }
        TestEqual(TEXT("Enough actual slots for boundary test"), Installed, Limit + 1);
    }
    for (const FHull* Hull : {Dreadnought, Battlecarrier})
    {
        if (!Hull) { AddError(TEXT("Missing extra-large hull")); continue; }
        const UShipLayoutDataAsset* Layout = MakeLayout(GetTransientPackage(), *Hull);
        TestNotNull(TEXT("Mixed-tier layout accepted"), Layout);
        if (!Layout) continue;
        float Sum = 0;
        for (const FOVWeaponMountDefinition& Mount : Layout->WeaponMounts)
            if (Mount.Type == EOVWeaponType::Cannon) Sum += Mount.ExplosionTierCoefficient;
        const float Expected = Hull == Dreadnought ? 12 * 6.5f + 4 * 17.f : 8 * 6.5f + 2 * 17.f;
        TestEqual(TEXT("Mixed cannon explosion coefficient sum"), Sum, Expected);
        auto* Systems = NewObject<UOVShipSystemsComponent>();
        Systems->Initialize(const_cast<UShipLayoutDataAsset*>(Layout));
        Systems->Tuning.ExplosionBase = 40;
        for (int32 Index = 0; Index < Layout->Slots.Num(); ++Index)
            if (Layout->Slots[Index].Facility == EOVFacilityType::Ammo)
            {
                TestEqual(TEXT("Ammo explosion uses mixed tier coefficients"), Systems->GetExplosionDamage(Index), Expected * 40);
                break;
            }
    }
    FOVHullMetadata Meta;
    TestTrue(TEXT("Vector metadata exists"), OVCalibration::GetHullMetadata(TEXT("vector"), Meta));
    TestTrue(TEXT("Vector class, fleet cost, D126 armor cap, and slots"),
        Meta.SizeClass == 2 && Meta.FleetCost == 5 && Meta.TotalInteriorSlots == 9
        && FMath::IsNearlyEqual(Meta.ArmorMaxReduction, 0.26f) && Meta.RoleTags.Contains(TEXT("공격")));
    TestTrue(TEXT("Air defense documented role"), OVCalibration::GetHullMetadata(TEXT("air_defense"), Meta) && Meta.RoleTags.Contains(TEXT("방공")));
    TestTrue(TEXT("Radar is not inferred to be air defense"), OVCalibration::GetHullMetadata(TEXT("radar"), Meta) && Meta.RoleTags.Contains(TEXT("탐지 지원")) && !Meta.RoleTags.Contains(TEXT("방공")));
    TestTrue(TEXT("Support source fleet cost and role"), OVCalibration::GetHullMetadata(TEXT("repair"), Meta) && Meta.FleetCost == 1 && Meta.RoleTags.Contains(TEXT("수리")));
    TestTrue(TEXT("Extra-large source fleet cost and D126 non-aircraft hangar limit"),
        OVCalibration::GetHullMetadata(TEXT("dreadnought"), Meta) && Meta.SizeClass == 4 && Meta.FleetCost == 125
        && Meta.TotalInteriorSlots == 36 && Meta.MaxHangarInstallations == 4 && !Meta.bAllowAircraftOperations);
    TestFalse(TEXT("Unknown metadata rejected"), OVCalibration::GetHullMetadata(TEXT("not_a_hull"), Meta));
    TestTrue(TEXT("Unknown metadata has no synthetic values"), Meta.SizeClass == 0 && Meta.FleetCost == 0 && Meta.RoleTags.IsEmpty());
    TestEqual(TEXT("Public hull catalog matches source"), OVCalibration::GetHullIds().Num(), 16);
    TestEqual(TEXT("Public profile catalog matches source"), OVCalibration::GetWeaponProfileIds().Num(), 25);
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

