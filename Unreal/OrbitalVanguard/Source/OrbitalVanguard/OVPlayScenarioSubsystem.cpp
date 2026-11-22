#include "OVPlayScenarioSubsystem.h"
#include "OVBlackHoleHazardActor.h"
#include "OVCalibration.h"
#include "OVCarrierSubsystem.h"
#include "OVCombatSubsystem.h"
#include "OVController.h"
#include "OVInteriorSubsystem.h"
#include "OVPointDefenseSubsystem.h"
#include "OVShip.h"
#include "OVShipSystemsComponent.h"
#include "OVTransportSubsystem.h"
#include "Engine/World.h"

namespace
{
constexpr int32 ScenarioSeed = 260112;
constexpr int32 FactionCount = 2;

// D126 equipment is resolved from the hull definition. Trial deployment and
// starting stocks are authored here independently of its combat statistics.
// LimitSeconds defaults to 0 (disabled) to respect D126 18~26.9 min fleet battle duration.
const FOVPlaySceneDefinition Scenes[] = {
    {TEXT("Fleet100"), TEXT("100코스트 함대전"), TEXT("혼성 함대로 상대의 전투 능력을 무력화하세요."),
     EOVPlaySceneKind::Fleet, 100, {{TEXT("corvette"),3}, {TEXT("frigate"),1}, {TEXT("vector"),1},
     {TEXT("bastion"),1}, {TEXT("battleship"),2}, {TEXT("air_defense"),1}, {TEXT("radar"),1},
     {TEXT("carrier"),1,10}, {TEXT("repair"),1}}, true, 120.f, 0.f, EOVSpaceHazardKind::None},
    {TEXT("Fleet200"), TEXT("200코스트 함대전"), TEXT("초대형 주력함과 호위 함대로 상대를 무력화하세요."),
     EOVPlaySceneKind::Fleet, 200, {{TEXT("corvette"),3}, {TEXT("frigate"),1}, {TEXT("vector"),1},
     {TEXT("bastion"),1}, {TEXT("dreadnought"),1}, {TEXT("air_defense"),1}, {TEXT("radar"),1},
     {TEXT("battleship"),1}, {TEXT("carrier"),1,10}, {TEXT("repair"),1}}, true, 120.f, 0.f, EOVSpaceHazardKind::None},
    {TEXT("Fleet300"), TEXT("300코스트 함대전"), TEXT("두 초대형함과 혼성 호위 전력을 운용하세요."),
     EOVPlaySceneKind::Fleet, 300, {{TEXT("corvette"),3}, {TEXT("frigate"),1}, {TEXT("vector"),1},
     {TEXT("bastion"),1}, {TEXT("dreadnought"),1}, {TEXT("battlecarrier"),1,10}, {TEXT("air_defense"),1},
     {TEXT("radar"),1}, {TEXT("carrier"),1,10}, {TEXT("repair"),1}}, true, 120.f, 0.f, EOVSpaceHazardKind::None},
    {TEXT("BlackHoleTrial"), TEXT("블랙홀 중력전 (E1)"), TEXT("블랙홀 1,000m 중력권과 45m 즉사 중심부를 피해 상대를 무력화하세요."),
     EOVPlaySceneKind::Fleet, 100, {{TEXT("corvette"),3}, {TEXT("frigate"),1}, {TEXT("vector"),1},
     {TEXT("bastion"),1}, {TEXT("battleship"),2}, {TEXT("air_defense"),1}, {TEXT("radar"),1},
     {TEXT("carrier"),1,10}, {TEXT("repair"),1}}, true, 120.f, 0.f, EOVSpaceHazardKind::BlackHole, FVector(0.f, 0.f, 2000.f)},
    {TEXT("Logistics"), TEXT("채굴·수송 엄호"), TEXT("채굴선과 수송선을 보호하고 자원을 납품하세요."),
     EOVPlaySceneKind::Logistics, 0, {}, false, 120.f, 0.f, EOVSpaceHazardKind::None},
    {TEXT("Evacuation"), TEXT("공습 생존·퇴각"), TEXT("공습을 버티고 함선을 초광속으로 이탈시키세요."),
     EOVPlaySceneKind::Evacuation, 0, {}, false, 120.f, 0.f, EOVSpaceHazardKind::None},
    {TEXT("Sandbox"), TEXT("자유 전투 시험장"), TEXT("개발 도구에서 함선을 배치하고 시험하세요."),
     EOVPlaySceneKind::Sandbox, 0, {}, true, 120.f, 0.f, EOVSpaceHazardKind::None}
};

const FOVPlaySceneDefinition* FindDefinition(FName Id)
{
    return MakeArrayView(Scenes).FindByPredicate([Id](const FOVPlaySceneDefinition& Entry) { return Entry.Id == Id; });
}

bool FitsBudget(const FOVPlaySceneDefinition& Definition)
{
    if (Definition.Kind != EOVPlaySceneKind::Fleet) return true;
    int32 Total = 0, Capitals = 0;
    for (const FOVPlayFleetEntry& Entry : Definition.Fleet)
    {
        FOVHullMetadata Hull;
        if (!OVCalibration::GetHullMetadata(Entry.Hull, Hull) || Entry.Count <= 0) return false;
        Total += Hull.FleetCost * Entry.Count;
        if (Hull.SizeClass == 4) Capitals += Entry.Count;
    }
    return Total == Definition.FleetBudget && Capitals <= Definition.FleetBudget / 100 - 1;
}

bool HasPhysicalFacility(const UOVShipSystemsComponent& Systems, EOVFacilityType Facility)
{
    for (int32 Index = 0; Index < Systems.Layout->Slots.Num(); ++Index)
        if (Systems.Layout->Slots[Index].Facility == Facility && Systems.Slots.IsValidIndex(Index) &&
            Systems.Slots[Index].HP > 0)
            return true;
    return false;
}

bool HasAttackPotential(const UOVShipSystemsComponent& Systems)
{
    if (!HasPhysicalFacility(Systems, EOVFacilityType::WeaponControl)) return false;
    for (const FOVWeaponMountDefinition& Mount : Systems.Layout->WeaponMounts)
    {
        EOVFacilityType Supply;
        switch (Mount.Type)
        {
        case EOVWeaponType::Beam: Supply = EOVFacilityType::Capacitor; break;
        case EOVWeaponType::Cannon: Supply = EOVFacilityType::Ammo; break;
        case EOVWeaponType::Missile: Supply = EOVFacilityType::MissileStorage; break;
        default: continue;
        }
        if (HasPhysicalFacility(Systems, Supply) && Systems.WeaponMountStates.ContainsByPredicate(
                [&Mount](const FOVWeaponMountState& State) { return State.WeaponMountId == Mount.WeaponId && State.HP > 0; }))
            return true;
    }
    return false;
}

EOVSpaceEnvironmentType ToControllerEnvironment(EOVSpaceHazardKind Kind)
{
    switch (Kind)
    {
    case EOVSpaceHazardKind::BlackHole:     return EOVSpaceEnvironmentType::BlackHole;
    case EOVSpaceHazardKind::SolarFlare:    return EOVSpaceEnvironmentType::SolarFlare;
    case EOVSpaceHazardKind::RadiationZone: return EOVSpaceEnvironmentType::RadiationZone;
    case EOVSpaceHazardKind::AsteroidField: return EOVSpaceEnvironmentType::AsteroidField;
    default:                                return EOVSpaceEnvironmentType::None;
    }
}
}

TConstArrayView<FOVPlaySceneDefinition> UOVPlayScenarioSubsystem::Definitions() { return MakeArrayView(Scenes); }
const FOVPlaySceneDefinition* UOVPlayScenarioSubsystem::GetActiveDefinition() const { return FindDefinition(ActiveId); }

void UOVPlayScenarioSubsystem::DestroyActiveHazardActors()
{
    if (AOVBlackHoleHazardActor* BlackHole = ActiveBlackHoleActor.Get())
    {
        BlackHole->Destroy();
    }
    ActiveBlackHoleActor.Reset();
}

void UOVPlayScenarioSubsystem::SyncEnvironmentHazard(AOVController& Controller, EOVSpaceHazardKind HazardKind, const FVector& Location)
{
    DestroyActiveHazardActors();
    Controller.SetSpaceEnvironment(ToControllerEnvironment(HazardKind));
    if (HazardKind == EOVSpaceHazardKind::BlackHole && GetWorld())
    {
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        if (AOVBlackHoleHazardActor* BlackHole = GetWorld()->SpawnActor<AOVBlackHoleHazardActor>(
                AOVBlackHoleHazardActor::StaticClass(), Location, FRotator::ZeroRotator, Params))
        {
            BlackHole->ApplyEnvironmentProfile(FOVSpaceEnvironmentProfile::MakeBlackHoleE1Profile());
            BlackHole->bExternallyStepped = true;
            ActiveBlackHoleActor = BlackHole;
            Controller.ActiveBlackHoleCenter = Location;
            Controller.ActiveBlackHoleRadius = BlackHole->GravityPullRadius;
        }
    }
}

void UOVPlayScenarioSubsystem::SetEnvironmentOverride(EOVSpaceHazardKind Kind)
{
    EnvironmentOverride = Kind;
    if (AOVController* Controller = Owner.Get())
    {
        const FOVPlaySceneDefinition* Def = GetActiveDefinition();
        const EOVSpaceHazardKind Effective = (EnvironmentOverride != EOVSpaceHazardKind::None)
            ? EnvironmentOverride
            : (Def ? Def->EnvironmentHazard : EOVSpaceHazardKind::None);
        const FVector Loc = Def ? Def->HazardLocation : FVector(0.f, 0.f, 2000.f);
        SyncEnvironmentHazard(*Controller, Effective, Loc);
    }
}

void UOVPlayScenarioSubsystem::Clear()
{
    DestroyActiveHazardActors();
    Owner.Reset();
    ActiveId = NAME_None;
    Result = EOVPlaySceneResult::Running;
    ElapsedSeconds = 0;
    InitialCosts.Reset();
    InitialAircraft.Reset();
    DecisionRemaining = 0;
    PreviousIntegrity = -1;
    LastProgressAt = 0;
    Resolution.Reset();
    InitialSorties.Reset();
}

bool UOVPlayScenarioSubsystem::Start(AOVController& Controller, FName Id)
{
    const FOVPlaySceneDefinition* Definition = FindDefinition(Id);
    if (!Definition || !Definition->bAvailable || !Controller.Combat || !FitsBudget(*Definition)) return false;
    Controller.ResetPlayScenarioSession(false);
    Owner = &Controller;
    ActiveId = Id;
    InitialCosts.Init(0, FactionCount);
    InitialAircraft.Init(0, FactionCount);
    OVCalibration::BeginSession(*Controller.Combat, ScenarioSeed);
    Controller.PointDefense->Reset(ScenarioSeed);
    Controller.Transport->ResetScenario({});
    auto* Interior = GetWorld()->GetSubsystem<UOVInteriorSubsystem>();
    Interior->ResetScenario({}, ScenarioSeed);

    const EOVSpaceHazardKind EffectiveHazard = (EnvironmentOverride != EOVSpaceHazardKind::None)
        ? EnvironmentOverride
        : Definition->EnvironmentHazard;
    SyncEnvironmentHazard(Controller, EffectiveHazard, Definition->HazardLocation);

    for (int32 Faction = 0; Faction < FactionCount; ++Faction)
    {
        int32 Ordinal = 0;
        for (const FOVPlayFleetEntry& Entry : Definition->Fleet)
        {
            FOVHullMetadata Metadata;
            if (!OVCalibration::GetHullMetadata(Entry.Hull, Metadata)) continue; // Definition preflight checks each hull.
            for (int32 I = 0; I < Entry.Count; ++I)
            {
                if (!Spawn(Controller, Entry, Faction, Ordinal++))
                {
                    Controller.ResetPlayScenarioSession(false);
                    Controller.LastAction = TEXT("편성 생성 실패 · 함체 또는 격납고를 확인하세요.");
                    return false;
                }
                InitialCosts[Faction] += Metadata.FleetCost;
            }
        }
    }
    for (const FOVAircraftRecord& Aircraft : Controller.Carrier->Aircraft)
        if (InitialAircraft.IsValidIndex(Aircraft.FactionId)) ++InitialAircraft[Aircraft.FactionId];
    if (!Controller.Ships.IsEmpty())
    {
        Controller.SelectShip(Controller.Ships[0]);
        Controller.FrameEngagement();
    }
    Controller.LastAction = FString::Printf(TEXT("%s · 준비 완료"), Definition->Title);
    return true;
}

AOVShip* UOVPlayScenarioSubsystem::Spawn(AOVController& Controller, const FOVPlayFleetEntry& Entry,
    int32 Faction, int32 Ordinal)
{
    const bool bFriendly = Faction == 0;
    const double Side = bFriendly ? -1 : 1;
    const double Scale = OVCalibration::GetHullGeometryScale(Entry.Hull);
    // The same formation is mirrored. Front scouts lead, capital hulls and support are behind them.
    const FVector Position(Side * (42000 + (Ordinal / 4) * 30500), (Ordinal % 4 - 1.5) * 29000, 2000);
    AOVShip* Ship = GetWorld()->SpawnActor<AOVShip>();
    if (!Ship) return nullptr;
    Controller.Ships.Add(Ship); // The session owns even partially configured actors.
    if (!OVCalibration::ConfigureHull(*Controller.Combat, *Ship, Entry.Hull, bFriendly, Position, Scale)) return nullptr;
    Ship->ShipName = FString::Printf(TEXT("%s %02d"), *OVCalibration::GetHullName(Entry.Hull), Ordinal + 1);
    Ship->SetAutomation(true, true);
    Ship->SetDestination(FVector(Side * (18000 + (Ordinal / 4) * 6000), Position.Y * .48, Position.Z));
    int32 DefenseMounts = 0;
    for (const FOVWeaponMountDefinition& Mount : Ship->Systems->Layout->WeaponMounts)
        if (Mount.Type == EOVWeaponType::Interception) ++DefenseMounts;
    Controller.PointDefense->RegisterShip(Ship, DefenseMounts, Entry.Hull == TEXT("air_defense") ? 1.5f : 1.f);
    if (Entry.AircraftPerHangar > 0 && !Controller.Carrier->RegisterCarrier(Ship, Entry.AircraftPerHangar)) return nullptr;
    Controller.Transport->AddShip(Ship);
    GetWorld()->GetSubsystem<UOVInteriorSubsystem>()->AddShip(Ship);
    return Ship;
}

bool UOVPlayScenarioSubsystem::HandleAction(AOVController& Controller, FName Action)
{
    if (Action == TEXT("PlaySceneExit"))
    {
        Controller.LeavePlayScenario();
        return true;
    }
    if (Action == TEXT("RestartScene") && !ActiveId.IsNone())
    {
        Start(Controller, ActiveId);
        return true;
    }
    if (Action == TEXT("ToggleBlackHoleEnv"))
    {
        const EOVSpaceHazardKind Next = (EnvironmentOverride == EOVSpaceHazardKind::BlackHole)
            ? EOVSpaceHazardKind::None
            : EOVSpaceHazardKind::BlackHole;
        SetEnvironmentOverride(Next);
        return true;
    }
    if (Action == TEXT("Pause") && Result != EOVPlaySceneResult::Running) return true;
    if (!ActiveId.IsNone() && (Action == TEXT("LaunchAircraft") || Action == TEXT("RecallAircraft")))
    {
        if (Controller.CommandCarrier(Controller.Selected, Action == TEXT("LaunchAircraft")))
            InitialSorties.Add(Controller.Selected.Get());
        return true;
    }
    for (const FOVPlaySceneDefinition& Definition : Scenes)
        if (Action == Definition.Id)
        {
            Start(Controller, Definition.Id);
            return true;
        }
    return false;
}

void UOVPlayScenarioSubsystem::Step(float Delta)
{
    const FOVPlaySceneDefinition* Definition = GetActiveDefinition();
    AOVController* Controller = Owner.Get();
    if (!Controller || !Definition || Result != EOVPlaySceneResult::Running) return;

    if (AOVBlackHoleHazardActor* BlackHole = ActiveBlackHoleActor.Get())
    {
        BlackHole->SimulateHazard(Delta);
    }

    if (Definition->Kind != EOVPlaySceneKind::Fleet) return;
    ElapsedSeconds += Delta;
    DecisionRemaining -= Delta;
    if (DecisionRemaining > 0) return;
    DecisionRemaining += .25f;
    CheckFleetResult(*Controller, *Definition);
}

void UOVPlayScenarioSubsystem::CheckFleetResult(AOVController& Controller, const FOVPlaySceneDefinition& Definition)
{
    int32 Active[FactionCount] = {};
    int32 Wrecked[FactionCount] = {};
    bool Potential[FactionCount] = {};
    double Integrity = 0;
    for (AOVShip* Ship : Controller.Ships)
    {
        if (!IsValid(Ship) || !Ship->Systems || !Ship->Systems->Layout) continue;
        const auto& Systems = *Ship->Systems;
        const int32 Faction = Systems.FactionId;
        if (Faction < 0 || Faction >= FactionCount || Systems.bNeutral) continue;
        if (Systems.bWrecked) { ++Wrecked[Faction]; continue; }
        if (Controller.Combat->IsDeparted(Ship)) continue;
        ++Active[Faction];
        Potential[Faction] |= HasAttackPotential(Systems);
        Integrity += Systems.HullHP + Systems.Shield;
        for (const auto& Armor : Systems.ArmorSections) Integrity += Armor.HP;
        for (const auto& Mount : Systems.WeaponMountStates) Integrity += Mount.HP;
        for (const auto& Facility : Systems.Slots) Integrity += Facility.HP;
    }
    for (const FOVCarrierState& Carrier : Controller.Carrier->Carriers)
    {
        AOVShip* Ship = Carrier.Carrier.Get();
        if (!IsValid(Ship) || Ship->Systems->bWrecked || Ship->Systems->bNeutral || Controller.Combat->IsDeparted(Ship)) continue;
        const int32 Faction = Ship->Systems->FactionId;
        if (Faction < 0 || Faction >= FactionCount || !HasPhysicalFacility(*Ship->Systems, EOVFacilityType::FighterControl)) continue;
        const bool bCanLaunch = HasPhysicalFacility(*Ship->Systems, EOVFacilityType::Hangar);
        const bool bHasAircraft = Controller.Carrier->Aircraft.ContainsByPredicate(
            [Ship, Faction, bCanLaunch](const FOVAircraftRecord& Aircraft) {
                return Aircraft.Carrier == Ship && Aircraft.FactionId == Faction &&
                    (Aircraft.State == EOVAircraftState::Sortie || (bCanLaunch && Aircraft.State != EOVAircraftState::Wrecked));
            });
        Potential[Faction] |= bHasAircraft;
        if (bCanLaunch && !Carrier.bRecallOrdered && Ship->IsAttackAutomatic() && IsValid(Ship->AttackTarget) && bHasAircraft &&
            !InitialSorties.Contains(Ship) && Controller.Carrier->Launch(Ship) > 0)
            InitialSorties.Add(Ship);
    }
    for (const auto& Aircraft : Controller.Carrier->Aircraft)
        if (Aircraft.State != EOVAircraftState::Wrecked) Integrity += Aircraft.HP;
    if (PreviousIntegrity < 0 || FMath::Abs(Integrity - PreviousIntegrity) > .1)
    {
        PreviousIntegrity = Integrity;
        LastProgressAt = ElapsedSeconds;
    }

    const float EffectiveLimit = CustomLimitSeconds > 0.f ? CustomLimitSeconds : Definition.LimitSeconds;
    if (!Potential[0] && !Potential[1])
    {
        if (Active[0] == 0 && Active[1] == 0 && Wrecked[0] && Wrecked[1])
            Finish(Controller, EOVPlaySceneResult::MutualDestruction, TEXT("양측 전력 소실"));
        else Finish(Controller, EOVPlaySceneResult::NoCombatCapability, TEXT("양측 전투 지속 불가"));
    }
    else if (!Potential[1])
        Finish(Controller, EOVPlaySceneResult::Victory, Active[1] ? TEXT("적 전투 전력 무력화") : TEXT("적 주력 이탈 또는 완파"));
    else if (!Potential[0])
        Finish(Controller, EOVPlaySceneResult::Defeat, Active[0] ? TEXT("아군 전투 전력 무력화") : TEXT("아군 주력 이탈 또는 완파"));
    else if (EffectiveLimit > 0.f && ElapsedSeconds >= EffectiveLimit)
        Finish(Controller, EOVPlaySceneResult::TimeLimit, TEXT("시험 시간 초과"));
    else if (ElapsedSeconds - LastProgressAt >= Definition.StalemateSeconds)
        Finish(Controller, EOVPlaySceneResult::Stalemate, TEXT("교착 · 전력 변화 없음"));
}

void UOVPlayScenarioSubsystem::Finish(AOVController& Controller, EOVPlaySceneResult Outcome, const TCHAR* Reason)
{
    Result = Outcome;
    Resolution = Reason;
    Controller.bSimulationPaused = true;
}

FString UOVPlayScenarioSubsystem::GetStatus() const
{
    if (!Resolution.IsEmpty())
    {
        if (Result == EOVPlaySceneResult::Victory) return FString(TEXT("승리 · ")) + Resolution;
        if (Result == EOVPlaySceneResult::Defeat) return FString(TEXT("패배 · ")) + Resolution;
        return Resolution;
    }
    if (InitialCosts.Num() != FactionCount || InitialCosts[0] == 0) return FString();
    const TCHAR* HazardTag = ActiveBlackHoleActor.IsValid() ? TEXT(" · 블랙홀(E1)") : TEXT("");
    return FString::Printf(TEXT("양측 %d코스트 · 아군 함재기 %d기 · %d초%s"),
        InitialCosts[0], InitialAircraft[0], FMath::FloorToInt(ElapsedSeconds), HazardTag);
}
