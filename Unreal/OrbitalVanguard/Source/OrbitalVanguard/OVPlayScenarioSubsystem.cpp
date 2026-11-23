#include "OVHeroSubsystem.h"
#include "OVLogisticsSubsystem.h"
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
#include "Engine/StaticMeshActor.h"

namespace
{
constexpr int32 ScenarioSeed = 260112;
constexpr int32 FactionCount = 2;

// D126 equipment is resolved from the hull definition. Trial deployment and
// starting stocks are authored here independently of its combat statistics.
// LimitSeconds defaults to 0 (disabled) to respect D126 18~26.9 min fleet battle duration.
const FOVPlaySceneDefinition Scenes[] = {
    {TEXT("Fleet50"), TEXT("50코스트 함대전"), TEXT("호위 전력과 항공대로 적 함대를 무력화하세요."),
     EOVPlaySceneKind::Fleet, 50, {{TEXT("carrier"),1,20},{TEXT("vector"),1},{TEXT("air_defense"),1},{TEXT("bastion"),2},{TEXT("corvette"),3},{TEXT("frigate"),1},{TEXT("repair"),1}}, true, 120.f, 0.f, EOVSpaceHazardKind::None},
    {TEXT("Fleet100"), TEXT("100코스트 함대전"), TEXT("혼성 함대로 상대의 전투 능력을 무력화하세요."),
     EOVPlaySceneKind::Fleet, 100, {{TEXT("corvette"),3}, {TEXT("frigate"),1}, {TEXT("vector"),1},
     {TEXT("bastion"),1}, {TEXT("battleship"),2}, {TEXT("air_defense"),1}, {TEXT("radar"),1},
     {TEXT("carrier"),1,20}, {TEXT("repair"),1}}, true, 120.f, 0.f, EOVSpaceHazardKind::None},
    {TEXT("Fleet200"), TEXT("200코스트 함대전"), TEXT("초대형 주력함과 호위 함대로 상대를 무력화하세요."),
     EOVPlaySceneKind::Fleet, 200, {{TEXT("corvette"),3}, {TEXT("frigate"),1}, {TEXT("vector"),1},
     {TEXT("bastion"),1}, {TEXT("dreadnought"),1}, {TEXT("air_defense"),1}, {TEXT("radar"),1},
     {TEXT("battleship"),1}, {TEXT("carrier"),1,20,4,100}, {TEXT("repair"),1}}, true, 120.f, 0.f, EOVSpaceHazardKind::None},
    {TEXT("Fleet300"), TEXT("300코스트 함대전"), TEXT("두 초대형함과 혼성 호위 전력을 운용하세요."),
     EOVPlaySceneKind::Fleet, 300, {{TEXT("corvette"),3}, {TEXT("frigate"),1}, {TEXT("vector"),1},
     {TEXT("bastion"),1}, {TEXT("dreadnought"),1}, {TEXT("battlecarrier"),1,20}, {TEXT("air_defense"),1},
     {TEXT("radar"),1}, {TEXT("carrier"),1,20}, {TEXT("repair"),1}}, true, 120.f, 0.f, EOVSpaceHazardKind::None},
    {TEXT("BlackHoleTrial"), TEXT("블랙홀 중력전 (E1)"), TEXT("블랙홀 1,000m 중력권과 45m 즉사 중심부를 피해 상대를 무력화하세요."),
     EOVPlaySceneKind::Fleet, 100, {{TEXT("corvette"),3}, {TEXT("frigate"),1}, {TEXT("vector"),1},
     {TEXT("bastion"),1}, {TEXT("battleship"),2}, {TEXT("air_defense"),1}, {TEXT("radar"),1},
     {TEXT("carrier"),1,20}, {TEXT("repair"),1}}, true, 120.f, 0.f, EOVSpaceHazardKind::BlackHole, FVector(0.f, 0.f, 2000.f)},
    {TEXT("Logistics"), TEXT("채굴·수송 엄호"), TEXT("채굴선과 수송선을 보호하고 자원을 납품하세요."),
     EOVPlaySceneKind::Logistics, 0, {{TEXT("vector"),1},{TEXT("frigate"),2},{TEXT("mining"),2},{TEXT("transport"),2}}, true, 120.f, 0.f, EOVSpaceHazardKind::None},
    {TEXT("Evacuation"), TEXT("공습 생존·퇴각"), TEXT("공습을 버티고 함선을 초광속으로 이탈시키세요."),
     EOVPlaySceneKind::Evacuation, 0, {{TEXT("corvette"),3},{TEXT("air_defense"),2},{TEXT("battleship"),1},
     {TEXT("carrier"),1,20},{TEXT("dreadnought"),1}}, true, 120.f, 0.f, EOVSpaceHazardKind::None},
    {TEXT("Sandbox"), TEXT("자유 전투 시험장"), TEXT("양측 함선을 원하는 위치에 배치하고 교전하세요."),
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
    return Total == Definition.FleetBudget && Capitals <= FMath::Max(0, Definition.FleetBudget / 100 - 1);
}

bool HasAttackPotential(const UOVShipSystemsComponent& Systems)
{
    if (!Systems.HasPhysicalFacility(EOVFacilityType::WeaponControl)) return false;
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
        if (Systems.HasPhysicalFacility(Supply) && Systems.WeaponMountStates.ContainsByPredicate(
                [&Mount](const FOVWeaponMountState& State) { return State.WeaponMountId == Mount.WeaponId && State.HP > 0; }))
            return true;
    }
    return false;
}

// Mission rehearsal timings, in game seconds. The open corridor is common to all survivors.
constexpr int32 CorridorOpens = 40;
constexpr int32 ChargeDuration = 16;
constexpr int32 CorridorCloses = 150;
struct FRaidWave { int32 Time; FVector Heading; const TCHAR* Bearing; const TCHAR* Escort; };
const FRaidWave Raid[] = {
    {12, FVector(1,0,0), TEXT("동쪽"), TEXT("frigate")},
    {32, FVector(0,1,0), TEXT("북쪽"), TEXT("vector")},
    {65, FVector(-1,0,0), TEXT("서쪽"), TEXT("battleship")}
};

void ArrangeFleet(AOVController& Controller, int32 Faction, bool bRetreat)
{
    TArray<AOVShip*> Group;
    for (AOVShip* Ship : Controller.Ships)
        if (Ship->Systems->FactionId == Faction) Group.Add(Ship);
    Group.StableSort([](const AOVShip& A, const AOVShip& B) {
        const auto TierA = A.Systems->Layout->HullSizeClass;
        const auto TierB = B.Systems->Layout->HullSizeClass;
        if (TierA != TierB) return TierA > TierB;
        return A.Systems->Layout->LocalHalfExtent.Size() * A.GetActorScale3D().X >
               B.Systems->Layout->LocalHalfExtent.Size() * B.GetActorScale3D().X;
    });
    if (Group.IsEmpty()) return;
    const double Side = Faction == 0 ? -1.0 : 1.0;
    const FVector Centre(bRetreat ? 0 : Side * 44000, 0, 2000);
    if(!bRetreat)
    {
        // Start the standard engagement facing the opposing line. Convoys and raid entries own their positions.
        const double Spacing=FMath::Max(5500.,2.15*(Group[0]->Systems->Layout->LocalHalfExtent*Group[0]->GetActorScale3D()).Size()+600.);
        int32 Counts[4]={}, Used[4]={};
        for(auto* Ship:Group)++Counts[AOVController::FormationRow(TEXT("FormationScreen"),Ship->HullDefinitionId)+1];
        for(auto* Ship:Group){
            const int32 Row=AOVController::FormationRow(TEXT("FormationScreen"),Ship->HullDefinitionId);
            const double Across=Used[Row+1]++-(Counts[Row+1]-1)*.5;
            const FVector Position=Centre+FVector(-Side*Row*Spacing,Across*Spacing,0);
            Ship->SetActorLocation(Position);
            Ship->SetDestination(Position+FVector(-Side*17000,0,0));
        }
        return;
    }
    // Phyllotactic offsets keep the widest hull at the centre and vary escort bearings.
    TArray<FVector> Placed;
    TArray<double> Radii;
    for (int32 I = 0; I < Group.Num(); ++I)
    {
        AOVShip* Ship = Group[I];
        const double Radius = (Ship->Systems->Layout->LocalHalfExtent * Ship->GetActorScale3D()).Size();
        const double Angle = I * 2.39996323;
        const FVector Axis(Side * FMath::Cos(Angle), FMath::Sin(Angle), 0);
        double Range = I ? 5500 * FMath::Sqrt(double(I)) : 0;
        FVector Position;
        for (;; Range += 1500)
        {
            Position = Centre + Axis * Range + FVector(0,0,(I % 3 - 1) * (I ? 650 : 0));
            bool Clear = true;
            for (int32 J = 0; J < Placed.Num(); ++J)
                if (FVector::Dist(Position, Placed[J]) < Radius + Radii[J] + 1000) { Clear = false; break; }
            if (Clear) break;
        }
        Ship->SetActorLocation(Position);
        Ship->StopMovement();
        Placed.Add(Position);
        Radii.Add(Radius);
    }
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
        const FTransform Placement(Location);
        if (AOVBlackHoleHazardActor* BlackHole = GetWorld()->SpawnActorDeferred<AOVBlackHoleHazardActor>(
                AOVBlackHoleHazardActor::StaticClass(), Placement, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
        {
            BlackHole->ApplyEnvironmentProfile(FOVSpaceEnvironmentProfile::MakeBlackHoleE1Profile());
            BlackHole->bExternallyStepped = true;
            BlackHole->FinishSpawning(Placement);
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
    EndSandboxPlacement();
    bSandboxHullMenu = false;
    SandboxFaction = 0;
    SandboxAltitude = 2000;
    SandboxHull = TEXT("corvette");
    SandboxHint.Reset();
    bSandboxEngaged = false;
    SandboxCount = 1;
    FreshDeployments.Reset();
    if(auto* GI=GetWorld()->GetGameInstance()) if(auto* Roster=GI->GetSubsystem<UOVHeroSubsystem>())
        Roster->Heroes.RemoveAll([this](const auto& H){return ScenarioHeroes.Contains(H.HeroId);});
    ScenarioHeroes.Reset();
    DestroyActiveHazardActors();
    if (auto* Marker = OreMarker.Get()) Marker->Destroy();
    OreMarker.Reset();
    Miners.Reset();
    Haulers.Reset();
    DeliveryLeg.Reset();
    OreNode = 0; InitialOre.Reset(); DischargedStores=HandedMinedOre=0;
    bRaidLaunched = false;
    DeliveredCargo = 0;
    RescueFleet.Reset();
    ChargeSeconds.Reset(); JumpDestinations.Reset(); JumpTrails.Reset();
    EscapedShips = ArrivedWaves = AbandonedAircraft = 0;
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
    DebriefLines.Reset();
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
    if(auto* GI=Controller.GetGameInstance()) if(auto* Roster=GI->GetSubsystem<UOVHeroSubsystem>())
        for(int32 Side=0;Side<FactionCount;++Side)
            for(int32 I=Roster->CountDeployed(Side);I<15;++I){
                static const TCHAR* Roles[]={TEXT("돌격"),TEXT("사수"),TEXT("지원"),TEXT("지휘")};
                const FGuid IdHero=Roster->CreateDevHero(Side,static_cast<EOVInteriorRole>(1+I%4),FString::Printf(TEXT("%s %02d"),Roles[I%4],I+1));
                if(!IdHero.IsValid())break;
                ScenarioHeroes.Add(IdHero);
                if(!Roster->SetDeployment(IdHero,true))break;
            }
    OVCalibration::BeginSession(*Controller.Combat, ScenarioSeed);
    Controller.PointDefense->Reset(ScenarioSeed);
    Controller.Transport->ResetScenario({});
    auto* Interior = GetWorld()->GetSubsystem<UOVInteriorSubsystem>();
    Interior->ResetScenario({}, ScenarioSeed);

    const EOVSpaceHazardKind EffectiveHazard = (EnvironmentOverride != EOVSpaceHazardKind::None)
        ? EnvironmentOverride
        : Definition->EnvironmentHazard;
    SyncEnvironmentHazard(Controller, EffectiveHazard, Definition->HazardLocation);

    if (Definition->Kind == EOVPlaySceneKind::Logistics && !StartEscort(Controller))
    {
        Controller.ResetPlayScenarioSession(false);
        Controller.LastAction = TEXT("엄호 편성을 생성할 수 없습니다.");
        return false;
    }
    const bool bEvacuation = Definition->Kind == EOVPlaySceneKind::Evacuation;
    for (int32 Faction = 0; Definition->Kind != EOVPlaySceneKind::Logistics && Faction < (bEvacuation ? 1 : FactionCount); ++Faction)
    {
        int32 Ordinal = 0;
        for (const FOVPlayFleetEntry& Entry : Definition->Fleet)
        {
            for (int32 I = 0; I < Entry.Count; ++I)
            {
                if (!Spawn(Controller, Entry, Faction, Ordinal++))
                {
                    Controller.ResetPlayScenarioSession(false);
                    Controller.LastAction = TEXT("편성 생성 실패 · 함체 또는 격납고를 확인하세요.");
                    return false;
                }
            }
        }
    }
    if (Definition->Kind == EOVPlaySceneKind::Fleet || bEvacuation)
        for (int32 Side = 0; Side < (bEvacuation ? 1 : 2); ++Side) ArrangeFleet(Controller, Side, bEvacuation);
    for(int32 Side=0;Side<FactionCount;++Side) AssignScenarioHeroes(Controller,Side);
    if (bEvacuation)
        for (AOVShip* Ship : Controller.Ships) RescueFleet.Add(Ship);
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

void UOVPlayScenarioSubsystem::AssignScenarioHeroes(AOVController& PC,int32 Side)
{
    auto* Interior=GetWorld()->GetSubsystem<UOVInteriorSubsystem>();
    if(ScenarioHeroes.IsEmpty() || !PC.GetGameInstance())return;
    auto* Roster=PC.GetGameInstance()->GetSubsystem<UOVHeroSubsystem>();
    TArray<AOVShip*> Ships;
    for(AOVShip* S:PC.Ships) if(IsValid(S) && S->Systems && S->Systems->FactionId==Side && Interior->AddShip(S))Ships.Add(S);
    if(Ships.IsEmpty())return;
    int32 Index=0;
    for(const FGuid& Id:ScenarioHeroes){
        const auto H=Roster->GetHero(Id);
        if(H.FactionId==Side && H.bDeploymentReserved && !H.RuntimeUnitId && H.Status==EOVHeroStatus::Deployed)
            Interior->SpawnRosterHero(Ships[Index++%Ships.Num()],Id);
    }
}
AOVShip* UOVPlayScenarioSubsystem::Spawn(AOVController& Controller, const FOVPlayFleetEntry& Entry,
    int32 Faction, int32 Ordinal, TOptional<FVector> PositionOverride)
{
    FOVHullMetadata Metadata;
    if (!OVCalibration::GetHullMetadata(Entry.Hull, Metadata)) return nullptr;
    const bool bFriendly = Faction == 0;
    const double Side = bFriendly ? -1 : 1;
    const double Scale = OVCalibration::GetHullGeometryScale(Entry.Hull);
    // The same formation is mirrored. Front scouts lead, capital hulls and support are behind them.
    const FVector Position = PositionOverride.Get(FVector(Side * (42000 + (Ordinal / 4) * 30500), (Ordinal % 4 - 1.5) * 29000, 2000));
    AOVShip* Ship = GetWorld()->SpawnActor<AOVShip>();
    if (!Ship) return nullptr;
    Controller.Ships.Add(Ship); // The session owns even partially configured actors.
    if (!OVCalibration::ConfigureHull(*Controller.Combat, *Ship, Entry.Hull, bFriendly, Position, Scale,Entry.AircraftBays)) return nullptr;
    Ship->ShipName = FString::Printf(TEXT("%s %02d"), *OVCalibration::GetHullName(Entry.Hull), Ordinal + 1);
    Ship->SetAutomation(true, true);
    Ship->SetDestination(FVector(Side * (18000 + (Ordinal / 4) * 6000), Position.Y * .48, Position.Z));
    int32 DefenseMounts = 0;
    for (const FOVWeaponMountDefinition& Mount : Ship->Systems->Layout->WeaponMounts)
        if (Mount.Type == EOVWeaponType::Interception) ++DefenseMounts;
    Controller.PointDefense->RegisterShip(Ship, DefenseMounts, Entry.Hull == TEXT("air_defense") ? 1.5f : 1.f);
    if (Ship->Systems->Layout->bAllowAircraftOperations && !Controller.Carrier->RegisterCarrier(Ship, Entry.AircraftPerHangar)) return nullptr;
    Controller.Transport->AddShip(Ship);
    GetWorld()->GetSubsystem<UOVInteriorSubsystem>()->AddShip(Ship);
    if (ActiveId != TEXT("Logistics") || (Entry.Hull != TEXT("mining") && Entry.Hull != TEXT("transport")))
        GetWorld()->GetSubsystem<UOVLogisticsSubsystem>()->StockFieldShip(Ship,Entry.ReplacementStock);
    if(ActiveId==TEXT("Sandbox") || (Faction==1 && (ActiveId==TEXT("Evacuation") || ActiveId==TEXT("Logistics")))) AssignScenarioHeroes(Controller,Faction);
    InitialCosts[Faction] += Metadata.FleetCost;
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
    if(Action==TEXT("AbandonJumpWing") && Result==EOVPlaySceneResult::Running){
        int32 Count=0;
        for(AOVShip* Ship:Controller.Selection)
            if(IsValid(Ship) && Ship->bFriendly && ChargeSeconds.Contains(Ship) && !Controller.Carrier->CanDepart(Ship)){
                Controller.Carrier->AbandonOutstanding(Ship);++Count;
            }
        Controller.LastAction=Count?TEXT("잔여 함재기 포기 승인"):TEXT("도약 회수 대기 중인 항모가 없습니다");
        return true;
    }
    if (ActiveId == TEXT("Sandbox") && SandboxAction(Controller, Action)) return true;
    if (ActiveId == TEXT("Evacuation") && EvacuationOrder(Controller, Action)) return true;
    if (Action == TEXT("ResumeConvoy") && ActiveId == TEXT("Logistics"))
    {
        if (Result == EOVPlaySceneResult::Running)
        {
            for (int32 I = 0; I < Miners.Num(); ++I)
                if (auto* Ship = Miners[I].Get(); Ship && Ship->Systems->FactionId == 0 && !Controller.Combat->IsDeparted(Ship)) OrderConvoyLeg(*Ship, I, true);
            for (int32 I = 0; I < Haulers.Num(); ++I)
                if (auto* Ship = Haulers[I].Get(); Ship && Ship->Systems->FactionId == 0 && !Controller.Combat->IsDeparted(Ship)) OrderConvoyLeg(*Ship, I, false);
        }
        return true;
    }
    if (Action == TEXT("Pause") && Result != EOVPlaySceneResult::Running) return true;
    if (!ActiveId.IsNone() && (Action == TEXT("LaunchAircraft") || Action == TEXT("RecallAircraft")))
    {
        bool HasCarrier = false;
        for (AOVShip* Ship : Controller.Selection)
            if (IsValid(Ship) && Ship->bFriendly && !Controller.Combat->IsDeparted(Ship) && !Ship->Systems->bWrecked &&
                Controller.Carrier->Carriers.ContainsByPredicate([Ship](const auto& C){ return C.Carrier == Ship; }))
            {
                HasCarrier = true;
                Controller.CommandCarrier(Ship, Action == TEXT("LaunchAircraft"));
            }
        if (!HasCarrier) Controller.LastAction = TEXT("운용할 아군 항모를 선택하세요");
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

    ElapsedSeconds += Delta;
    AdvanceJumps(*Controller);
    if (Definition->Kind == EOVPlaySceneKind::Evacuation)
    {
        StepEvacuation(*Controller, Delta);
        return;
    }
    if (Definition->Kind == EOVPlaySceneKind::Logistics)
    {
        // Combat advances at 120 Hz. Cargo orders and depot transactions run four times per game second.
        DecisionRemaining += Delta;
        constexpr float CargoInterval = .25f;
        if (DecisionRemaining + UE_SMALL_NUMBER >= CargoInterval)
        {
            DecisionRemaining = FMath::Max(0.f, DecisionRemaining - CargoInterval);
            StepEscort(*Controller, CargoInterval);
        }
        return;
    }
    if (Definition->Kind != EOVPlaySceneKind::Fleet) return;
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
        if (Faction < 0 || Faction >= FactionCount || !Ship->Systems->HasPhysicalFacility(EOVFacilityType::FighterControl)) continue;
        const bool bCanLaunch = Ship->Systems->HasPhysicalFacility(EOVFacilityType::Hangar);
        const bool bHasAircraft = Controller.Carrier->Aircraft.ContainsByPredicate(
            [Ship, Faction, bCanLaunch](const FOVAircraftRecord& Aircraft) {
                return Aircraft.Carrier == Ship && Aircraft.FactionId == Faction &&
                    (Aircraft.State == EOVAircraftState::Sortie || (bCanLaunch && Aircraft.State != EOVAircraftState::Wrecked));
            });
        Potential[Faction] |= bHasAircraft;
        if(Controller.Carrier->GetAircraftOverview(Ship).ReplacementKits>0 && Ship->Systems->CanUseResources() && bCanLaunch) Potential[Faction]=true;
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
    if (Result != EOVPlaySceneResult::Running) return;
    Result = Outcome;
    Resolution = Reason;
    int32 Present = 0, Lost = 0, Departed = 0;
    SurvivingCost=0;
    for (AOVShip* Ship : Controller.Ships)
    {
        if (!IsValid(Ship) || !Ship->Systems || Ship->Systems->FactionId != 0 || Ship->Systems->bNeutral) continue;
        if (Ship->Systems->bWrecked) ++Lost;
        else if (Controller.Combat->IsDeparted(Ship)) ++Departed;
        else ++Present;
        FOVHullMetadata Meta;
        if(!Ship->Systems->bWrecked && OVCalibration::GetHullMetadata(Ship->HullDefinitionId,Meta))SurvivingCost+=Meta.FleetCost;
    }
    DebriefLines.Add(FString::Printf(TEXT("아군 함선    전장 %d척    ·    이탈 %d척    ·    완파 %d척"), Present, Departed, Lost));
    int32 Aircraft = 0, Wrecks = 0;
    for (const auto& Craft : Controller.Carrier->Aircraft)
        if (Craft.FactionId == 0) { ++Aircraft; Wrecks += Craft.State == EOVAircraftState::Wrecked; }
    DepartedCount=Departed;SurvivingAircraft=Aircraft-Wrecks;
    if (Aircraft || AbandonedAircraft)
        DebriefLines.Add(FString::Printf(TEXT("함재기    완파 %d기    ·    이탈 시 포기 %d기"), Wrecks, AbandonedAircraft));
    if (ActiveId == TEXT("Logistics"))
        DebriefLines.Add(FString::Printf(TEXT("자원 납품    %.0f / %.0f"), DeliveredCargo, EscortQuota()));

    int32 Heroes = 0, Dead = 0, Stranded = 0;
    for (const auto& Person : Controller.Transport->Persons)
        if (Person.FactionId == 0 && Person.bRuntimeHero)
        {
            ++Heroes;
            Dead += Person.State == EOVPassengerState::Dead;
            Stranded += Person.State == EOVPassengerState::RescueWaiting;
        }
    if (Heroes) DebriefLines.Add(FString::Printf(TEXT("탑승 영웅    사망 %d명    ·    구조 대기 %d명"), Dead, Stranded));
    int32 Pods = 0, Destroyed = 0, Missing = 0;
    for (const auto& Pod : Controller.Transport->Pods)
        if (Pod.FactionId == 0)
        {
            ++Pods;
            Destroyed += Pod.State == EOVPodFlightState::Destroyed;
            Missing += Pod.State == EOVPodFlightState::Stranded;
        }
    if (Pods) DebriefLines.Add(FString::Printf(TEXT("강습 포드    파괴 %d기    ·    고립 %d기"), Destroyed, Missing));
    Controller.bSimulationPaused = true;
}

FString UOVPlayScenarioSubsystem::GetOutcomeTitle() const
{
    switch (Result)
    {
    case EOVPlaySceneResult::Victory: return TEXT("승리");
    case EOVPlaySceneResult::Defeat: return TEXT("패배");
    case EOVPlaySceneResult::PartialVictory: return TEXT("부분 성공");
    case EOVPlaySceneResult::MutualDestruction: return TEXT("양측 전멸");
    case EOVPlaySceneResult::Stalemate: return TEXT("교착");
    case EOVPlaySceneResult::TimeLimit: return TEXT("시간 종료");
    case EOVPlaySceneResult::NoCombatCapability: return TEXT("교전 종료");
    default: return FString();
    }
}

FString UOVPlayScenarioSubsystem::GetStatus() const
{
    if (!Resolution.IsEmpty())
    {
        if (Result == EOVPlaySceneResult::PartialVictory) return FString(TEXT("부분 성공 · ")) + Resolution;
        if (Result == EOVPlaySceneResult::Victory) return FString(TEXT("승리 · ")) + Resolution;
        if (Result == EOVPlaySceneResult::Defeat) return FString(TEXT("패배 · ")) + Resolution;
        return Resolution;
    }
    if (ActiveId == TEXT("Evacuation")) return GetEvacuationStatus(nullptr);
    if (ActiveId == TEXT("Logistics"))
        return FString::Printf(TEXT("납품 %d / %.0f   ·   %s"), FMath::FloorToInt(DeliveredCargo), EscortQuota(),
            bRaidLaunched ? TEXT("습격대 출현 · 수송로 엄호") : TEXT("적 접근 중 · 호위함을 배치하세요"));
    if (InitialCosts.Num() != FactionCount || InitialCosts[0] == 0) return FString();
    const TCHAR* HazardTag = ActiveBlackHoleActor.IsValid() ? TEXT(" · 블랙홀(E1)") : TEXT("");
    return FString::Printf(TEXT("양측 %d코스트 · 아군 함재기 %d기 · %d초%s"),
        InitialCosts[0], InitialAircraft[0], FMath::FloorToInt(ElapsedSeconds), HazardTag);
}

FString UOVPlayScenarioSubsystem::GetEvacuationStatus(AOVShip* Ship) const
{
    if (Ship)
    {
        if (const float* Remaining = ChargeSeconds.Find(Ship))
            return *Remaining > ElapsedSeconds ? FString::Printf(TEXT("%s · 충전 %d초"), *Ship->ShipName, FMath::CeilToInt(*Remaining - ElapsedSeconds)) :
                                    FString(TEXT("충전 완료 · 함재기 회수 대기"));
        return ElapsedSeconds < CorridorOpens ? FString::Printf(TEXT("퇴로 개방까지 %d초"), FMath::CeilToInt(CorridorOpens - ElapsedSeconds)) :
            FString::Printf(TEXT("퇴로 폐쇄까지 %d초"), FMath::Max(0,FMath::CeilToInt(CorridorCloses - ElapsedSeconds)));
    }
    const FString Alert = ArrivedWaves < UE_ARRAY_COUNT(Raid) ?
        FString::Printf(TEXT("%s 공습까지 %d초"), Raid[ArrivedWaves].Bearing, FMath::Max(0,FMath::CeilToInt(Raid[ArrivedWaves].Time - ElapsedSeconds))) :
        FString(TEXT("최종 공습 진행 중"));
    return FString::Printf(TEXT("%s  ·  이탈 %d/%d척  ·  목표 %d척"), *Alert, EscapedShips, RescueFleet.Num(), FMath::DivideAndRoundUp(RescueFleet.Num()*3,5));
}

bool UOVPlayScenarioSubsystem::EvacuationOrder(AOVController& Controller, FName Action)
{
    if (Action != TEXT("EvacCharge") && Action != TEXT("EvacAbandon") && Action != TEXT("EvacFinish")) return false;
    if (Result != EOVPlaySceneResult::Running) return true;
    if (Action == TEXT("EvacFinish"))
    {
        if (EscapedShips > 0) ResolveEvacuation(Controller);
        return true;
    }
    if (ElapsedSeconds < CorridorOpens) { Controller.LastAction = TEXT("퇴로 개방을 기다리세요"); return true; }
    int32 Accepted = 0;
    for (AOVShip* Ship : Controller.Selection)
    {
        if (!RescueFleet.Contains(Ship) || !IsValid(Ship) || Ship->Systems->FactionId != 0 ||
            Ship->Systems->bWrecked || Controller.Combat->IsDeparted(Ship)) continue;
        if (Action == TEXT("EvacAbandon"))
        {
            if (ChargeSeconds.Contains(Ship) && !Controller.Carrier->CanDepart(Ship))
            {
                Controller.Carrier->AbandonOutstanding(Ship);
                ++Accepted;
            }
        }
        else if (!ChargeSeconds.Contains(Ship) && Ship->Systems->CanCommand() &&
                 Ship->Systems->GetEfficiency(EOVFacilityType::Engine) > 0)
        {
            Ship->StopMovement();
            Ship->bFTLCharging = true;
            ChargeSeconds.Add(Ship, ElapsedSeconds + ChargeDuration);
            Controller.Carrier->Recall(Ship);
            ++Accepted;
        }
    }
    Controller.LastAction = Accepted ? FString::Printf(TEXT("%d척 · %s"), Accepted,
        Action == TEXT("EvacAbandon") ? TEXT("잔여기 포기 승인") : TEXT("회수 및 도약 충전 시작")) : TEXT("명령 가능한 함선을 선택하세요");
    return true;
}

float UOVPlayScenarioSubsystem::ChargeFraction(AOVShip* Ship) const
{
    const float* Due=ChargeSeconds.Find(Ship);
    return Due ? FMath::Clamp(1.f-(*Due-ElapsedSeconds)/ChargeDuration,0.f,1.f) : 0.f;
}
bool UOVPlayScenarioSubsystem::OrderJump(AOVController& PC,FName Destination)
{
    if(Result!=EOVPlaySceneResult::Running || Destination.IsNone()) return false;
    if(ActiveId==TEXT("Evacuation") && ElapsedSeconds<CorridorOpens){PC.LastAction=TEXT("퇴로가 아직 열리지 않았습니다");return false;}
    bool Accepted=false;
    for(AOVShip* Ship:PC.Selection){
        if(!IsValid(Ship)|| !Ship->bFriendly || Ship->Systems->FactionId!=0 || Ship->Systems->bWrecked || PC.Combat->IsDeparted(Ship) ||
            ChargeSeconds.Contains(Ship) || !Ship->Systems->CanCommand() || Ship->Systems->GetEfficiency(EOVFacilityType::Engine)<=0)continue;
        Ship->StopMovement();Ship->bFTLCharging=true;
        ChargeSeconds.Add(Ship,ElapsedSeconds+ChargeDuration);JumpDestinations.Add(Ship,Destination);PC.Carrier->Recall(Ship);Accepted=true;
    }
    PC.LastAction=Accepted?TEXT("목적지 설정 · 함재기 회수 및 도약 충전"):TEXT("도약 가능한 아군 함선을 선택하세요");
    return Accepted;
}
void UOVPlayScenarioSubsystem::AdvanceJumps(AOVController& PC)
{
    JumpTrails.RemoveAll([this](const FJumpTrail& T){return ElapsedSeconds-T.At>1.2f;});
    for(auto It=ChargeSeconds.CreateIterator();It;++It){
        AOVShip* Ship=It.Key().Get();
        if(!IsValid(Ship)||Ship->Systems->bWrecked||Ship->Systems->FactionId!=0||!Ship->bFTLCharging||PC.Combat->IsDeparted(Ship)){
            JumpDestinations.Remove(It.Key());It.RemoveCurrent();continue;
        }
        if(ElapsedSeconds<It.Value()||!PC.Carrier->CanDepart(Ship))continue;
        for(const auto& Craft:PC.Carrier->Aircraft)
            if(Craft.Carrier==Ship && Craft.State!=EOVAircraftState::Stored && Craft.State!=EOVAircraftState::Wrecked)++AbandonedAircraft;
        const FVector P=Ship->GetActorLocation();
        JumpTrails.Add({P,P+Ship->GetActorForwardVector()*48000,ElapsedSeconds});
        PC.Combat->NotifyFTLDeparture(Ship);Ship->bFTLCharging=false;++EscapedShips;It.RemoveCurrent();
    }
}
void UOVPlayScenarioSubsystem::StepEvacuation(AOVController& Controller, float Delta)
{
    DecisionRemaining += Delta;
    if (DecisionRemaining < .25f) return;
    DecisionRemaining = 0;
    int32 OnField = 0;
    FVector Centre = FVector::ZeroVector;
    AOVShip* First = nullptr;
    for (const auto& Ref : RescueFleet)
    {
        AOVShip* Ship = Ref.Get();
        if (!Ship || Ship->Systems->bWrecked || Ship->Systems->FactionId != 0 || Controller.Combat->IsDeparted(Ship))
        { ChargeSeconds.Remove(Ref); continue; }
        if (!First) First = Ship;
        ++OnField;
        Centre += Ship->GetActorLocation();
    }
    if (!OnField || ElapsedSeconds >= CorridorCloses) { ResolveEvacuation(Controller); return; }
    Centre /= OnField;
    if (ArrivedWaves < UE_ARRAY_COUNT(Raid) && ElapsedSeconds >= Raid[ArrivedWaves].Time)
    {
        const FRaidWave& Wave = Raid[ArrivedWaves];
        double Outer = 0;
        for (auto Ref : RescueFleet)
            if (auto* S = Ref.Get(); S && !Controller.Combat->IsDeparted(S) && !S->Systems->bWrecked)
                Outer = FMath::Max(Outer,FVector::Dist(Centre,S->GetActorLocation()));
        const FVector From = Centre + Wave.Heading * (Outer + 36000);
        const FVector Across(-Wave.Heading.Y,Wave.Heading.X,0);
        for (int32 I = 0; I < 3; ++I)
        {
            const bool bCarrier = I == 1;
            FOVPlayFleetEntry Entry{bCarrier ? FName(TEXT("carrier")) : FName(Wave.Escort),1,bCarrier ? 6 : 0};
            auto* Enemy = Spawn(Controller,Entry,1,ArrivedWaves*3+I, From + Across*(I-1)*12000);
            if (!Enemy) { Finish(Controller,EOVPlaySceneResult::NoCombatCapability,TEXT("공습 편성 생성 실패")); return; }
            Enemy->SetAutomation(false, true);
            Enemy->SetDestination(Centre + Wave.Heading * (Outer + (bCarrier ? 30000 : 22500)) + Across*(I-1)*11500);
            Controller.Combat->SetTarget(Enemy,First);
            // The carrier subsystem handles auto launches when a hostile target is acquired.
        }
        ++ArrivedWaves;
    }
}

void UOVPlayScenarioSubsystem::ResolveEvacuation(AOVController& Controller)
{
    int32 Wrecked = 0, LostCost = 0;
    for (auto Ref : RescueFleet)
        if (auto* Ship = Ref.Get(); Ship && Ship->Systems->bWrecked)
        {
            ++Wrecked;
            const int32 Tier = Controller.Combat->GetShipState(Ship).WeaponTierCap;
            constexpr int32 Cost[] = {1,5,25,125};
            LostCost += Cost[FMath::Clamp(Tier-1,0,3)];
        }
    const FString Summary = FString::Printf(TEXT("이탈 %d척 · 완파 %d척(%d코스트) · 잔류 %d척 · 포기 %d기"),
        EscapedShips,Wrecked,LostCost,RescueFleet.Num()-Wrecked-EscapedShips,AbandonedAircraft);
    Finish(Controller, EscapedShips * 5 >= RescueFleet.Num() * 3 ? EOVPlaySceneResult::Victory :
        (EscapedShips ? EOVPlaySceneResult::PartialVictory : EOVPlaySceneResult::Defeat), *Summary);
}
