#include "OVCalibration.h"

#include "OVCombatSubsystem.h"
#include "OVMarkedSlots.h"
#include "OVPointDefenseSubsystem.h"
#include "OVShip.h"
#include "OVShipSystemsComponent.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "HAL/IConsoleManager.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#endif

namespace
{
// Provisional source-surface boxes remain opt-in while mission regression persists.
TAutoConsoleVariable<int32> CVarOVMarkedMounts(TEXT("ov.Calibration.MarkedMounts"),0,
    TEXT("Experimental drawing-based mounts and provisional source-surface collision boxes."), ECVF_Default);
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

const FHull* FindHull(FName Key)
{
    for (const FHull& Hull : Hulls)
        if (FName(Hull.Key) == Key)
        {
            // D121 overrides the historical D116 candidate without changing its source data.
            if (Key == TEXT("battlecarrier"))
            {
                static const FHull Revised = [&Hull]() {
                    FHull Result = Hull;
                    Result.MountCount = 10; // eight T3 cannons and two T4 spinal guns
                    return Result;
                }();
                return &Revised;
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

// Keep the equipped weapon IDs/profiles and map them onto the user's slots.
// A count mismatch must not silently create, delete, or overlap equipment.
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
    if (General.Num() + Dedicated.Num() != Hull.MountCount) return false;
    TArray<int32> SpecialWeapons, RegularWeapons;
    for (int32 I = 0; I < Hull.MountCount; ++I)
    {
        const bool bSpecial = Hull.Mounts[I].Weapon == 6 ||
            (HullId == TEXT("missile_ship") && Hull.Mounts[I].Weapon >= 4 && SpecialWeapons.Num() < Dedicated.Num());
        (bSpecial ? SpecialWeapons : RegularWeapons).Add(I);
    }
    if (SpecialWeapons.Num() != Dedicated.Num() || RegularWeapons.Num() != General.Num()) return false;
    const auto Place = [&Layout,&Hull](const TArray<int32>& WeaponsToPlace, const TArray<const FOVMarkedSlotCandidate*>& Slots)
    {
        for (int32 I = 0; I < WeaponsToPlace.Num(); ++I)
        {
            auto& Mount = Layout.WeaponMounts[WeaponsToPlace[I]];
            Mount.AuthoredSlotId = Slots[I]->SlotId;
            Mount.AuthoredSlotPurpose = Slots[I]->Kind == EOVMarkedSlotKind::Dedicated ? FName(TEXT("Dedicated")) : FName(TEXT("General"));
            if (Slots[I]->Kind == EOVMarkedSlotKind::Dedicated) {
                const FMount& Source = Hull.Mounts[WeaponsToPlace[I]];
                Mount.RequiredProfileId = ProfileId(Weapons[Source.Weapon],Source.Tier);
            }
            Mount.LocalPosition = Slots[I]->LocalPosition;
            Mount.LocalSurfaceNormal = Slots[I]->LocalNormal;
        }
    };
    Place(RegularWeapons, General);
    Place(SpecialWeapons, Dedicated);
    return true;
}

void AppendDefenseMounts(const FHull& Hull,UShipLayoutDataAsset& Layout)
{
    const int32 First=Layout.WeaponMounts.Num();
    for(int32 I=0;I<Hull.PD;++I) {
        auto& M=Layout.WeaponMounts.AddDefaulted_GetRef();M.WeaponId=FName(*FString::Printf(TEXT("PD_%d"),I));
        M.Type=EOVWeaponType::Interception;M.Tier=Layout.WeaponTier;M.MaxHP=100;
        M.LocalPosition=FVector(FMath::Lerp(-.7,.7,(I+.5)/Hull.PD)*Layout.LocalHalfExtent.X,
            (I%2?1:-1)*Layout.LocalHalfExtent.Y*.8,Layout.LocalHalfExtent.Z+8);
    }
    const auto Marked=OVMarkedSlots::GetCandidates(FName(Hull.Key));
    TArray<const FOVMarkedSlotCandidate*> Slots;
    for(const auto& Slot:Marked)if(Slot.Kind==EOVMarkedSlotKind::Interception)Slots.Add(&Slot);
    if(CVarOVMarkedMounts.GetValueOnGameThread()==1&&Slots.Num()==Hull.PD)
        for(int32 I=0;I<Hull.PD;++I) {
            auto& Mount=Layout.WeaponMounts[First+I];Mount.AuthoredSlotId=Slots[I]->SlotId;
            Mount.AuthoredSlotPurpose=TEXT("Interception");Mount.LocalPosition=Slots[I]->LocalPosition;
            Mount.LocalSurfaceNormal=Slots[I]->LocalNormal;
        }
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
    if (FName(Hull.Key) == TEXT("carrier")) Layout->MaxHangarInstallations = 4;
    else if (FName(Hull.Key) == TEXT("battlecarrier")) Layout->MaxHangarInstallations = 8;
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
        const auto& Weapon = Weapons[Mount.Weapon];
        auto& Def = Layout->WeaponMounts.AddDefaulted_GetRef();
        Def.WeaponId = FName(*FString::Printf(TEXT("Weapon_%d"), I));
        Def.Type = Family(Weapon.Kind);
        Def.Tier = Mount.Tier;
        Def.ExplosionTierCoefficient = TierDamage[Mount.Tier - 1];
        Def.MaxHP = Weapon.HP * TierDamage[Mount.Tier - 1];
        const double X = FMath::Lerp(-.8, .8, (I + 1.0) / (Hull.MountCount + 1.0));
        Def.LocalPosition = FVector(X * Layout->LocalHalfExtent.X, 0,
            (Mount.bTop ? 1 : -1) * (Layout->HalfHeight + 12));
        if (Def.Type == EOVWeaponType::Beam) ++Layout->BeamSlots;
        else if (Def.Type == EOVWeaponType::Cannon) ++Layout->CannonSlots;
        else ++Layout->MissileSlots;
    }

    if (CVarOVMarkedMounts.GetValueOnGameThread() == 1)
    {
        ApplyMarkedAttackMounts(Hull, *Layout);
        for (const auto& Candidate:HullCollisionCandidates)
            if (FName(Candidate.Hull)==FName(Hull.Key)) {
                auto& Box=Layout->CombatHullBoxes.AddDefaulted_GetRef();
                Box.LocalCenter=Candidate.Center;Box.HalfExtent=Candidate.HalfExtent;
            }
    }
    TArray<FString> Errors;
    if (Layout->ValidateLayout(Errors)) return Layout;
    UE_LOG(LogTemp, Error, TEXT("D116 invalid %s layout: %s"), Hull.Key, *FString::Join(Errors, TEXT("; ")));
    return nullptr;
}
}

bool OVCalibration::GetHullMetadata(FName HullId, FOVHullMetadata& OutMetadata)
{
    OutMetadata = {};
    const FHull* Hull = FindHull(HullId);
    if (!Hull) return false;
    OutMetadata.SizeClass = Hull->Size;
    OutMetadata.FleetCost = Hull->FleetCost;
    if (Hull->MountCount > 0) OutMetadata.RoleTags.Add(TEXT("공격"));
    // Explicit documented specializations; neither PD count nor weapon count infers a role.
    if (HullId == TEXT("corvette")) OutMetadata.RoleTags.Add(TEXT("정찰"));
    if (HullId == TEXT("frigate")) OutMetadata.RoleTags.Add(TEXT("호위"));
    if (HullId == TEXT("air_defense")) OutMetadata.RoleTags.Add(TEXT("방공"));
    if (HullId == TEXT("radar")) OutMetadata.RoleTags.Add(TEXT("탐지 지원"));
    if (HullId == TEXT("carrier") || HullId == TEXT("battlecarrier")) OutMetadata.RoleTags.Add(TEXT("함재기 운용"));
    if (HullId == TEXT("missile_ship")) OutMetadata.RoleTags.Add(TEXT("미사일"));
    if (HullId == TEXT("repair")) OutMetadata.RoleTags.Add(TEXT("수리"));
    if (HullId == TEXT("mining")) OutMetadata.RoleTags.Add(TEXT("채굴"));
    if (HullId == TEXT("transport")) OutMetadata.RoleTags.Add(TEXT("수송"));
    if (HullId == TEXT("assault")) OutMetadata.RoleTags.Add(TEXT("강습"));
    OutMetadata.Description = FString::Printf(TEXT("D116 후보 · 체급 %d · 시험 편성 코스트 %d · %d층 · 선체 %.0f · 쉴드 %.0f · 장갑/칸 %.0f · 전면 장갑/칸 %.0f · 속도 %.0fcm/s · 선회 %.1f도/s · 레이더 T%d · 공격 장착점 %d / PD %d · 함체별 총 승무원 미정"),
        Hull->Size, Hull->FleetCost, Hull->Decks, Hull->HP, Hull->Shield, Hull->Armor, Hull->FrontArmor,
        Hull->Speed, Hull->Turn, Hull->RadarTier, Hull->MountCount, Hull->PD);
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
    // Assault ship was explicitly excluded from the current model revision.
    if (HullId == TEXT("assault")) return 6.0;
    // Imported hulls are uniformly fitted into the common local selection box.
    // Correct for each mesh's fitted X length before applying the requested
    // class ratio, without touching the combat speed or weapon profiles.
    const auto FittedLength = [](FName MeshId) {
        static TMap<FName, double> Lengths;
        if (const double* Cached = Lengths.Find(MeshId)) return *Cached;
        FString Name = MeshId.ToString();
        Name[0] = FChar::ToUpper(Name[0]);
        for (int32 I = 1; I < Name.Len(); ++I) if (Name[I-1] == TEXT('_')) Name[I] = FChar::ToUpper(Name[I]);
        const FString Path = MeshId == TEXT("corvette") ? TEXT("/Game/Presentation/Ships/SM_PC14_FullMesh.SM_PC14_FullMesh") :
            FString::Printf(TEXT("/Game/Presentation/Ships/%sFull/SM_Hull.SM_Hull"), *Name);
        const UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *Path, nullptr, LOAD_NoWarn);
        if (!Mesh) return 0.0;
        const FVector E = Mesh->GetBounds().BoxExtent;
        const double Length = 2 * E.X * (FVector(175,75,30) / E.ComponentMax(FVector(1))).GetMin();
        Lengths.Add(MeshId, Length);
        return Length;
    };
    const int32 Size=HullId==TEXT("transport")?2:Hull->Size;
    constexpr double LengthRatios[]={1.,4.,16.,32.};
    const double Base = FittedLength(TEXT("corvette"));
    const double Length = FittedLength(HullId);
    return Size>=1&&Size<=4&&Base>0&&Length>0 ? 6.0*Base*LengthRatios[Size-1]/Length : 0.0;
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

    Combat.EnableCombat({}, 260112);
    ApplyProfiles(*Combat.Settings);
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
        Ship->SetCalibrationHull(&Layouts[I]->LocalHalfExtent);
        auto& Systems = *Ship->Systems;
        Systems.Tuning = GetDefault<UOVShipSystemsComponent>()->Tuning;
        Systems.Tuning.ShieldBase = Hull.Shield;
        Systems.Tuning.ExplosionBase = 40; // D116 provisional balance
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
    if(!Hull || !Combat.Settings) return false;
    UShipLayoutDataAsset* Layout=MakeLayout(&Ship,*Hull);
    if(!Layout) return false;
    auto Install=[&](EOVFacilityType Type, int32 Count) {
        int32 Existing=0;
        for(const auto& S:Layout->Slots) if(S.Facility==Type)++Existing;
        for(auto& S:Layout->Slots) if(Existing<Count && S.Facility==EOVFacilityType::None) {
            S.Facility=Type; S.AllowedFacilities.AddUnique(Type); S.MaxFacilityHP=100; S.RequiredCrew=10; ++Existing;
        }
    };
    if (HullId == TEXT("repair") || HullId == TEXT("mining") || HullId == TEXT("transport"))
        Install(EOVFacilityType::ResourceStorage, 1);
    Install(EOVFacilityType::Interception,1);
    Install(EOVFacilityType::Boarding,1);
    if(HullId==TEXT("carrier") || HullId==TEXT("battlecarrier")) { Install(EOVFacilityType::Hangar,2); Install(EOVFacilityType::FighterControl,1); }
    AppendDefenseMounts(*Hull,*Layout);
    Ship.Configure(HullId.ToString().ToUpper(),Friendly,Hull->Speed,Hull->Turn,FVector(Scale),Friendly?FLinearColor(.12f,.65f,1):FLinearColor(1,.15f,.08f));
    Ship.SetCalibrationHull(&Layout->LocalHalfExtent);
    auto& S=*Ship.Systems;
    S.Tuning=GetDefault<UOVShipSystemsComponent>()->Tuning; S.Tuning.ShieldBase=Hull->Shield; S.Tuning.ExplosionBase=40; // D116 provisional balance
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
            for(const auto& Mount:PD.Mounts)
                TestTrue(TEXT("F6 PD maps to the authored layout"),Mount.bLayoutMount && Ship->Systems->IsWeaponAvailable(Mount.MountId));
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
        TSet<FName> Used;
        for (int32 I = 0; I < Hull.MountCount; ++I)
        {
            const auto& Mount = Layout->WeaponMounts[I];
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
                if (Mount.Type != EOVWeaponType::Interception) continue;
                const auto* Found=Candidates.FindByPredicate([&](const FOVMarkedSlotCandidate& C) {
                    return C.Kind == EOVMarkedSlotKind::Interception && C.LocalPosition.Equals(Mount.LocalPosition,.001);
                });
                if (!TestNotNull(TEXT("PD uses a marked interception slot"),Found)) continue;
                TestEqual(TEXT("PD records marked slot identity"),Mount.AuthoredSlotId,Found->SlotId);
                TestFalse(TEXT("PD slots are independently equipped"),UsedPD.Contains(Found->SlotId));
                UsedPD.Add(Found->SlotId);
            }
            TestEqual(TEXT("All marked PD slots have one actual mount"),UsedPD.Num(),Hull.PD);
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
        auto Length=[](const AOVShip* Ship) {
            const UStaticMesh* Mesh=Ship->Hull->GetStaticMesh();
            return Mesh ? 2.*Mesh->GetBounds().BoxExtent.X*Ship->Hull->GetRelativeScale3D().X*Ship->GetActorScale3D().X : 0.;
        };
        const double Base=Length(Ships[1]);
        TestTrue(TEXT("Small reference mesh length is positive"),Base>0);
        TestTrue(TEXT("F6 medium hulls are four times reference mesh length"),
            FMath::IsNearlyEqual(Length(Ships[0])/Base,4.,.001)&&FMath::IsNearlyEqual(Length(Ships[2])/Base,4.,.001));
        const FName Ids[]={TEXT("frigate"),TEXT("carrier"),TEXT("dreadnought"),TEXT("battlecarrier"),TEXT("transport")};
        const double Ratios[]={1.,16.,32.,32.,4.};
        for(int32 I=0;I<UE_ARRAY_COUNT(Ids);++I)
            if(TestTrue(TEXT("Hull config succeeds"),OVCalibration::ConfigureHull(*Combat,*Ships[0],Ids[I],true,FVector::ZeroVector,OVCalibration::GetHullGeometryScale(Ids[I]))))
                TestTrue(FString::Printf(TEXT("%s requested source mesh length ratio"),*Ids[I].ToString()),FMath::IsNearlyEqual(Length(Ships[0])/Base,Ratios[I],.001));
        constexpr double ClassRatios[]={1.,4.,16.,32.};
        for (FName HullId : OVCalibration::GetHullIds())
        {
            if (HullId == TEXT("assault")) continue;
            FOVHullMetadata Meta;
            if (!TestTrue(TEXT("Hull metadata exists"),OVCalibration::GetHullMetadata(HullId,Meta))) continue;
            const int32 Class=HullId == TEXT("transport") ? 2 : Meta.SizeClass;
            if (TestTrue(TEXT("Actual source hull configures"),OVCalibration::ConfigureHull(*Combat,*Ships[0],HullId,true,FVector::ZeroVector,OVCalibration::GetHullGeometryScale(HullId))))
                TestTrue(FString::Printf(TEXT("%s actual length ratio"),*HullId.ToString()),Class>=1&&Class<=4&&FMath::IsNearlyEqual(Length(Ships[0])/Base,ClassRatios[Class-1],.001));
        }
    }
    World->DestroyWorld(false);return true;
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
    TestTrue(TEXT("Distinct destroyer deck, attack and PD metadata"),
        Vector->Decks == 1 && Vector->MountCount == 3 && Vector->PD == 2
        && Bastion->Decks == 2 && Bastion->MountCount == 5 && Bastion->PD == 4);
    TestEqual(TEXT("Extra-large hull HP remains absolute"), Dreadnought->HP, 825000.f);
    const FHull* Battlecarrier = FindHull(TEXT("battlecarrier"));
    const FHull* Carrier = FindHull(TEXT("carrier"));
    for (const FHull* Hull : {Carrier, Battlecarrier})
    {
        if (!Hull) { AddError(TEXT("Missing carrier hull")); continue; }
        UShipLayoutDataAsset* Layout = MakeLayout(GetTransientPackage(), *Hull);
        if (!TestNotNull(TEXT("Carrier layout exists"), Layout)) continue;
        const int32 Limit = Hull == Carrier ? 4 : 8;
        TestEqual(TEXT("D121 installation limit"), Layout->MaxHangarInstallations, Limit);
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
    TestTrue(TEXT("Vector class and source fleet cost"), Meta.SizeClass == 2 && Meta.FleetCost == 5 && Meta.RoleTags.Contains(TEXT("공격")));
    TestTrue(TEXT("Air defense documented role"), OVCalibration::GetHullMetadata(TEXT("air_defense"), Meta) && Meta.RoleTags.Contains(TEXT("방공")));
    TestTrue(TEXT("Radar is not inferred to be air defense"), OVCalibration::GetHullMetadata(TEXT("radar"), Meta) && Meta.RoleTags.Contains(TEXT("탐지 지원")) && !Meta.RoleTags.Contains(TEXT("방공")));
    TestTrue(TEXT("Support source fleet cost and role"), OVCalibration::GetHullMetadata(TEXT("repair"), Meta) && Meta.FleetCost == 1 && Meta.RoleTags.Contains(TEXT("수리")));
    TestTrue(TEXT("Extra-large source fleet cost"), OVCalibration::GetHullMetadata(TEXT("dreadnought"), Meta) && Meta.SizeClass == 4 && Meta.FleetCost == 125);
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

