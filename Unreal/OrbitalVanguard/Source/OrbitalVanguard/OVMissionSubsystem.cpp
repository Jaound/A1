#include "OVMissionSubsystem.h"
#include "OVHeroSubsystem.h"
#include "Engine/GameInstance.h"
#include "OVController.h"
#include "OVHUD.h"
#include "OVShip.h"
#include "OVShipSystemsComponent.h"
#include "OVCombatSubsystem.h"
#include "OVCarrierSubsystem.h"
#include "OVPointDefenseSubsystem.h"
#include "OVTransportSubsystem.h"
#include "OVCalibration.h"
#include "Engine/World.h"

namespace
{
FName Facility(AOVShip* Ship,EOVFacilityType Type)
{
    if(Ship&&Ship->Systems&&Ship->Systems->Layout)
        for(const auto& Slot:Ship->Systems->Layout->Slots) if(Slot.Facility==Type)return Slot.SlotId;
    return NAME_None;
}
bool Living(AOVShip* Ship) { return IsValid(Ship)&&Ship->Systems&&!Ship->Systems->bWrecked; }
bool Living(const FOVInteriorUnitRecord& Unit) { return Unit.UnitId>0&&Unit.HP>0&&Unit.State!=EOVInteriorState::Dead; }
}

void UOVMissionSubsystem::Disable()
{
    bUsesOwnedRoster=false;BattleId.Empty();HeroParticipation.Reset();HeroObjectiveXP.Reset();LastHeroEvent=0;
    bEnabled=false; bAutoPlayer=false; Owner.Reset(); Roster.Reset();
    Origin=Objective=Shuttle=nullptr; AttackUnitIds.Reset();DefenderUnitIds.Reset();
    LaunchedIds.Reset();EnteredIds.Reset();ReturnedIds.Reset();AutoOrderedIds.Reset();
    bDispatched=bRetreatOrdered=bAutoBridgeOrdered=bDefenderRetakeOrdered=false;
    DecisionRemaining=0;Score={};GameTime=0;Result=EOVMissionResult::Ongoing;Phase=EOVMissionPhase::Battle;
}

bool UOVMissionSubsystem::StartMission(AOVController* Controller,EOVMissionMode InMode,int32 InSeed,int32 AttackHeroes,int32 DefenseHeroes,bool bShuttle)
{
    if(bStarting||!IsValid(Controller)||AttackHeroes<0||AttackHeroes>4||DefenseHeroes<0||DefenseHeroes>4)return false;
    bStarting=true;
    // 기존 세 장면의 임시 런타임 상태만 명시적으로 초기화한다. Content/자산은 변경하지 않는다.
    if(!Controller->StartCalibrationRange(1)){bStarting=false;return false;}
    Owner=Controller;Mode=InMode;Seed=InSeed;AttackHeroCount=AttackHeroes;DefenseHeroCount=DefenseHeroes;bUseShuttle=bShuttle;
    AttackerFaction=Mode==EOVMissionMode::Defense?1:0;
    auto* Combat=Controller->Combat.Get();
    // 교정 프로필을 유지하면서 실제 전투 난수도 요청 시드로 초기화한다.
    auto* Candidate=DuplicateObject<UOVCombatSettings>(Combat->Settings,this);
    Combat->EnableCombat({},Seed);Combat->Settings=Candidate;
    auto* Wing=GetWorld()->GetSubsystem<UOVCarrierSubsystem>();Wing->Reset(Seed);
    auto* PD=GetWorld()->GetSubsystem<UOVPointDefenseSubsystem>();PD->Reset(Seed);
    while(Controller->Ships.Num()<6)Controller->Ships.Add(GetWorld()->SpawnActor<AOVShip>());
    const FName Hulls[]={TEXT("carrier"),TEXT("bastion"),TEXT("assault")};
    const double Scales[]={22.,13.,6.};
    const TCHAR* Names[]={TEXT("항공모함"),TEXT("바스티온 호위"),TEXT("강습선")};
    const double Half=FMath::Clamp(double(Settings.CarrierSeparation)*.5,10000.,12000.);
    for(int32 Faction=0;Faction<2;++Faction)for(int32 Index=0;Index<3;++Index)
    {
        AOVShip* Ship=Controller->Ships[Faction*3+Index];
        const double Side=Faction==0?-1.:1.;
        const FVector Position(Side*(Half+(Index==0?0:Index==1?7000:12000)),Index==1?14000:Index==2?-12000:0,2000);
        if(!OVCalibration::ConfigureHull(*Combat,*Ship,Hulls[Index],Faction==0,Position,Scales[Index]))
        { bStarting=false;Disable();Status=TEXT("통합 임무 생성 실패: 후보 함체/배치 확인");return false; }
        Ship->Systems->Layout->LayoutVersion=FName(*FString::Printf(TEXT("Interior_%s_D116_v1"),*Hulls[Index].ToString()));
        Ship->ShipName=(Faction==0?FString(TEXT("아군 ")):FString(TEXT("적 ")))+Names[Index];
        Ship->SetAutomation(false,Index!=2);
        Roster.Add(Ship);
        int32 Mounts=0;for(const auto& M:Ship->Systems->Layout->WeaponMounts)Mounts+=M.Type==EOVWeaponType::Interception?1:0;
        PD->RegisterShip(Ship,Mounts);
        if(Index==0)Wing->RegisterCarrier(Ship,20);
    }
    Origin=Roster[AttackerFaction*3];Objective=Roster[(1-AttackerFaction)*3];Shuttle=Roster[AttackerFaction*3+2];
    ShieldSlot=Facility(Objective,EOVFacilityType::ShieldControl);BridgeSlot=Facility(Objective,EOVFacilityType::Bridge);
    TArray<AOVShip*> All;for(AOVShip* Ship:Roster)All.Add(Ship);
    auto* Transport=GetWorld()->GetSubsystem<UOVTransportSubsystem>();Transport->ResetScenario(All);
    Transport->RegisterShuttle(Roster[2],Roster[0]);Transport->RegisterShuttle(Roster[5],Roster[3]);
    auto* Interior=GetWorld()->GetSubsystem<UOVInteriorSubsystem>();Interior->ResetScenario(All,Seed);
    // 양측 동일 초기 경비. 시설 운영 승무원과 다른 개별 전투 인원이다.
    for(int32 Index=0;Index<Roster.Num();++Index)
    {
        AOVShip* Ship=Roster[Index];
        // 전용 강습선의 12인 운송 좌석에 수비병을 숨겨 추가 탑승시키지 않는다.
        if(Index%3==2)continue;
        const bool Target=Ship==Objective;
        RegisterGuards(Ship,Target&&Mode!=EOVMissionMode::FleetOnly?DefenseHeroCount:0,Target?&DefenderUnitIds:nullptr);
    }
    AttackUnitIds=Interior->RegisterBoardingTeam(Origin,AttackerFaction,Mode==EOVMissionMode::FleetOnly?0:AttackHeroCount,Settings.BoardingTroops);
    for(int32 Faction=0;Faction<2;++Faction)for(int32 I=0;I<2;++I)
        Combat->SetTarget(Roster[Faction*3+I],Roster[(1-Faction)*3]);
    for(const auto& C:Wing->Carriers)Wing->Launch(C.Carrier.Get());
    Controller->Phase2CScenario=-1;Controller->CalibrationScenario=1;
    Controller->bDeveloperMode=false;Controller->bSimulationPaused=true;Controller->SimulationSpeed=1;Controller->SimulationSeconds=0;
    Controller->SelectShip(Roster[0]);Controller->FrameEngagement();
    if(auto* HUD=Cast<AOVHUD>(Controller->GetHUD()))HUD->SetDetailsOpen(false);
    bEnabled=true;bStarting=false;bWasShieldActive=Objective->Systems->bShieldActive;
    Status=TEXT("실제 교전 준비 · 쉴드 제거 후 강습 · F1 시작");Controller->LastAction=Status;
    RefreshMetrics();
    return true;
}

bool UOVMissionSubsystem::StartRosterMission(AOVController* PC,bool bDefense,int32 InSeed,bool bShuttle)
{
    if(!PC || !GetWorld()->GetGameInstance())return false;
    auto* Heroes=GetWorld()->GetGameInstance()->GetSubsystem<UOVHeroSubsystem>();
    if(!Heroes->CanChangeRoster()){Status=TEXT("먼저 비전투 편성실에서 보유 영웅을 편성하세요.");return false;}
    TArray<FGuid> Chosen;
    for(const auto& H:Heroes->Heroes)if(H.FactionId==0 && H.bDeploymentReserved && H.Status==EOVHeroStatus::Deployed && H.RuntimeUnitId==0)Chosen.Add(H.HeroId);
    if(Chosen.Num()<1 || Chosen.Num()>4){Status=TEXT("이 소규모 임무는 편성 예약 영웅 1~4명이 필요합니다. 세력 전체 한도 15명은 별도로 유지됩니다.");return false;}
    if(!StartMission(PC,bDefense?EOVMissionMode::Defense:EOVMissionMode::Attack,InSeed,bDefense?4:0,bDefense?0:2,bShuttle))return false;
    bUsesOwnedRoster=true;BattleId=FGuid::NewGuid().ToString();
    auto* Interior=GetWorld()->GetSubsystem<UOVInteriorSubsystem>();
    auto* Transit=GetWorld()->GetSubsystem<UOVTransportSubsystem>();
    AOVShip* Home=bDefense?Objective.Get():Origin.Get();
    for(const FGuid Id:Chosen)
    {
        const int32 UnitId=Interior->SpawnRosterHero(Home,Id,bDefense?BridgeSlot:NAME_None);
        if(UnitId<=0){Status=Interior->LastMessage;return false;}
        (bDefense?DefenderUnitIds:AttackUnitIds).Add(UnitId);
        if(!bDefense)Transit->SetRuntimeBoardingAssignment(Interior->GetUnit(UnitId).PersonId,UnitId,true);
        HeroParticipation.Add(Id,0);HeroObjectiveXP.Add(Id,0);
    }
    Status=TEXT("보유 영웅 임무 준비 · 동일 ID/HP/스킬/소모품으로 실제 운송과 작전 수행");
    return true;
}

float UOVMissionSubsystem::GetHeroParticipationSeconds(FGuid HeroId) const
{return HeroParticipation.FindRef(HeroId);}
int32 UOVMissionSubsystem::GetHeroObjectiveXP(FGuid HeroId) const
{return HeroObjectiveXP.FindRef(HeroId);}

void UOVMissionSubsystem::TrackHeroes(float Delta)
{
    if(!bUsesOwnedRoster || Result!=EOVMissionResult::Ongoing)return;
    auto* Interior=GetWorld()->GetSubsystem<UOVInteriorSubsystem>();
    auto* Combat=GetWorld()->GetSubsystem<UOVCombatSubsystem>();
    auto* Transport=GetWorld()->GetSubsystem<UOVTransportSubsystem>();
    auto Engaged=[Combat](AOVShip* Ship)
    {
        if(!Ship || !Combat)return false;
        for(const auto& A:Combat->Attacks)if(A.Result==EOVAttackResult::Flying && (A.Attacker.Get()==Ship || A.Target.Get()==Ship))return true;
        for(const auto& S:Combat->Ships)if(S.Ship==Ship)
            for(const auto& Mount:S.Mounts)if(Mount.ShotsFired>0 && Mount.Cooldown>0)return true;
        return false;
    };
    for(const auto& U:Interior->Units)if(U.HeroId.IsValid() && HeroParticipation.Contains(U.HeroId) && Living(U))
    {
        const auto Passenger=Transport->GetPerson(U.PersonId);
        const bool bBoarding=Passenger.State==EOVPassengerState::InPod || Passenger.State==EOVPassengerState::InShuttle ||
            Passenger.State==EOVPassengerState::InsideTarget || Passenger.State==EOVPassengerState::AwaitingRecovery;
        const bool bOperation=U.OperationContributedAt>=Interior->GameTime-1.e-4;
        const bool bFighting=U.State==EOVInteriorState::Fighting;
        const bool bDuty=U.bDutyActive && Engaged(U.Ship.Get());
        if(bBoarding || bOperation || bFighting || bDuty)HeroParticipation[U.HeroId]+=Delta;
    }
    for(const auto& E:Interior->Events)if(E.Serial>LastHeroEvent)
    {
        if(E.Kind==TEXT("Capture")||E.Kind==TEXT("Sabotage"))
        {
            const int32 Bonus=E.SlotId==BridgeSlot?400:E.Kind==TEXT("Sabotage")?300:200;
            for(int32 UnitId:E.ParticipantUnitIds)
            {
                const auto U=Interior->GetUnit(UnitId);
                if(U.HeroId.IsValid() && HeroParticipation.Contains(U.HeroId))
                    HeroObjectiveXP.FindOrAdd(U.HeroId)=FMath::Max(HeroObjectiveXP.FindRef(U.HeroId),Bonus);
            }
        }
        LastHeroEvent=FMath::Max(LastHeroEvent,E.Serial);
    }
}

void UOVMissionSubsystem::RegisterGuards(AOVShip* Ship,int32 Heroes,TArray<int32>* OutIds)
{
    auto* Interior=GetWorld()->GetSubsystem<UOVInteriorSubsystem>();
    const EOVFacilityType Types[]={EOVFacilityType::Bridge,EOVFacilityType::Core,EOVFacilityType::LifeSupport};
    for(EOVFacilityType Type:Types)
    {
        const FName Slot=Facility(Ship,Type);if(Slot.IsNone())continue;
        for(int32 I=0;I<Settings.GuardsPerFacility;++I)
        {
            const int32 Id=Interior->RegisterUnit(Ship,Ship->Systems->FactionId,EOVInteriorRole::Trooper,Slot);
            if(Id>0&&OutIds)OutIds->Add(Id);
        }
    }
    for(int32 I=0;I<Heroes;++I)
    {
        const FName Slot=Facility(Ship,I%2?EOVFacilityType::Bridge:EOVFacilityType::ShieldControl);
        const int32 Id=Interior->RegisterUnit(Ship,Ship->Systems->FactionId,static_cast<EOVInteriorRole>(1+I%4),Slot);
        if(Id>0&&OutIds)OutIds->Add(Id);
    }
}

bool UOVMissionSubsystem::IsObserved(AOVShip* Source,AOVShip* Target) const
{
    return Living(Source)&&Living(Target)&&GetWorld()->GetSubsystem<UOVCombatSubsystem>()->GetDetection(Source,Target).bDetected;
}

bool UOVMissionSubsystem::Dispatch(bool bShuttle)
{
    if(!bEnabled||Mode==EOVMissionMode::FleetOnly||bDispatched||!Living(Origin)||!Living(Objective)||
        !IsObserved(Origin,Objective)||Objective->Systems->bShieldActive)
    {Status=TEXT("강습 대기 · 탐지된 표적의 활성 쉴드를 실제 공격으로 제거하세요");return false;}
    auto* Transport=GetWorld()->GetSubsystem<UOVTransportSubsystem>();
    auto* Interior=GetWorld()->GetSubsystem<UOVInteriorSubsystem>();
    bool Accepted=bShuttle?Transport->DispatchShuttle(Shuttle,Objective,Interior->GetPassengerIds(AttackUnitIds)):Transport->LaunchPods(Origin,Objective)>0;
    Status=Transport->LastMessage;
    if(Accepted){bDispatched=true;bUseShuttle=bShuttle;Phase=EOVMissionPhase::Transit;}
    return Accepted;
}

TArray<int32> UOVMissionSubsystem::GetPlayerUnitIds() const
{
    return Mode==EOVMissionMode::Defense?DefenderUnitIds:AttackUnitIds;
}

bool UOVMissionSubsystem::OrderTeam(EOVInteriorOrder Order,FName Slot)
{
    if(!bEnabled||Mode==EOVMissionMode::FleetOnly)return false;
    auto* Interior=GetWorld()->GetSubsystem<UOVInteriorSubsystem>();
    TArray<int32> Ids=GetPlayerUnitIds();
    if(Order!=EOVInteriorOrder::Retreat&&Order!=EOVInteriorOrder::None&&!Interior->IsStructureKnown(0,Objective))
    {Status=TEXT("진입 전에는 적 내부 시설 목표를 지정할 수 없습니다");return false;}
    const bool Accepted=Interior->IssueOrder(Ids,Order,Slot);Status=Interior->LastMessage;
    if(Accepted&&Order==EOVInteriorOrder::Retreat&&Mode==EOVMissionMode::Attack)
    {
        if(!bRetreatOrdered)
        {
            Score.RetreatAt=GameTime;Score.AliveWhenRetreatOrdered=0;
            for(int32 Id:AttackUnitIds){const auto U=Interior->GetUnit(Id);if(Living(U)&&U.EntryShip==Objective)++Score.AliveWhenRetreatOrdered;}
        }
        bRetreatOrdered=true;Phase=EOVMissionPhase::Extracting;
    }
    return Accepted;
}

void UOVMissionSubsystem::SetAutoPlayer(bool bAuto)
{
    bAutoPlayer=bAuto;
    if(bAuto&&bEnabled)for(AOVShip* Ship:Roster)if(Living(Ship)&&Ship->bFriendly)
        Ship->SetAutomation(Ship->IsMovementAutomatic(),Ship!=Roster[2]);
}

void UOVMissionSubsystem::RefreshMetrics()
{
    auto* Interior=GetWorld()->GetSubsystem<UOVInteriorSubsystem>();auto* Transport=GetWorld()->GetSubsystem<UOVTransportSubsystem>();
    if(!IsValid(Objective))return;
    const bool Active=Objective->Systems->bShieldActive;
    if(bWasShieldActive&&!Active&&Score.ShieldBreakAt<0)Score.ShieldBreakAt=GameTime;
    if(!bWasShieldActive&&Active)++Score.ShieldReactivations;
    bWasShieldActive=Active;
    for(int32 Id:AttackUnitIds)
    {
        const auto Unit=Interior->GetUnit(Id);const auto Person=Transport->GetPerson(Unit.PersonId);
        bool Left=Person.State==EOVPassengerState::InPod||Person.State==EOVPassengerState::InShuttle||Person.State==EOVPassengerState::InsideTarget||Person.State==EOVPassengerState::AwaitingRecovery;
        if(Left)LaunchedIds.Add(Id);
        if(Unit.EntryShip==Objective||(Person.CurrentShip==Objective&&Person.State==EOVPassengerState::InsideTarget))
        {EnteredIds.Add(Id);if(Score.FirstEntryAt<0)Score.FirstEntryAt=GameTime;}
        if(LaunchedIds.Contains(Id)&&Person.State==EOVPassengerState::Aboard&&Person.CurrentShip.IsValid()&&Person.CurrentShip!=Objective&&Person.CurrentShip->Systems->FactionId==AttackerFaction)
        {if(!ReturnedIds.Contains(Id))Score.LastReturnAt=GameTime;ReturnedIds.Add(Id);}
    }
    Score.Launched=LaunchedIds.Num();Score.Entered=EnteredIds.Num();Score.Returned=ReturnedIds.Num();
    Score.ReturnedAfterEntry=0;for(int32 Id:ReturnedIds)Score.ReturnedAfterEntry+=EnteredIds.Contains(Id)?1:0;
    const auto Shield=Objective->Systems->GetSlotState(ShieldSlot);
    if(Score.ShieldOperationAt<0&&(Shield.OccupyingFaction==AttackerFaction||Shield.HP<=0)&&Interior->IsStructureKnown(AttackerFaction,Objective))
    {
        // 3D 포격에 의한 파괴를 선내 성공으로 오인하지 않는다.
        for(const auto& Op:Interior->Operations)if(Op.Ship==Objective&&Op.SlotId==ShieldSlot&&Op.FactionId==AttackerFaction&&Op.bCompleted)
            {Score.ShieldOperationAt=GameTime;break;}
    }
    if(Score.BridgeCaptureAt<0&&Objective->Systems->FactionId==AttackerFaction&&Objective->Systems->CaptureCount>0)Score.BridgeCaptureAt=GameTime;
}

void UOVMissionSubsystem::AdvanceOrders()
{
    auto* Interior=GetWorld()->GetSubsystem<UOVInteriorSubsystem>();
    const bool AutoAttacker=Mode==EOVMissionMode::Defense||bAutoPlayer;
    if(Mode!=EOVMissionMode::FleetOnly&&AutoAttacker)
    {
        if(!bDispatched)Dispatch(bUseShuttle);
        if(bDispatched&&!bRetreatOrdered&&Interior->IsStructureKnown(AttackerFaction,Objective))
        {
            const bool ShieldDestroyed=Objective->Systems->GetSlotState(ShieldSlot).HP<=0;
            TArray<int32> Arrivals;
            for(int32 Id:AttackUnitIds){const auto U=Interior->GetUnit(Id);if(Living(U)&&U.Ship==Objective&&!AutoOrderedIds.Contains(Id))Arrivals.Add(Id);}
            if(!Arrivals.IsEmpty())
            {
                Interior->IssueOrder(Arrivals,EOVInteriorOrder::Capture,Score.ShieldOperationAt<0&&!ShieldDestroyed?ShieldSlot:BridgeSlot);
                for(int32 Id:Arrivals)AutoOrderedIds.Add(Id);
            }
            if((Score.ShieldOperationAt>=0||ShieldDestroyed)&&!bAutoBridgeOrdered)
            {Interior->IssueOrder(AttackUnitIds,EOVInteriorOrder::Capture,BridgeSlot);bAutoBridgeOrdered=true;}
            const bool Danger=IsObserved(Origin,Objective)&&Objective->Systems->HullHP/Objective->Systems->MaxHullHP<Settings.AutoWithdrawHullFraction;
            if(Score.BridgeCaptureAt>=0||Danger||Objective->Systems->GetSlotState(BridgeSlot).HP<=0)
            {
                Interior->IssueOrder(AttackUnitIds,EOVInteriorOrder::Retreat);
                bRetreatOrdered=true;Score.RetreatAt=GameTime;Score.AliveWhenRetreatOrdered=0;
                for(int32 Id:AttackUnitIds){const auto U=Interior->GetUnit(Id);if(Living(U)&&U.EntryShip==Objective)++Score.AliveWhenRetreatOrdered;}
            }
        }
    }
    // 원 소속 수비병은 소유권이 바뀌어도 실제 함교로 이동하여 재점령한다.
    // 숨은 공격 인원의 위치/수/레벨을 조회하지 않으며 자기 함교 통제권 경보만 사용한다.
    if((Mode!=EOVMissionMode::Defense||bAutoPlayer)&&Living(Objective))
    {
        if(Objective->Systems->FactionId==AttackerFaction&&!bDefenderRetakeOrdered)
        {Interior->IssueOrder(DefenderUnitIds,EOVInteriorOrder::Capture,BridgeSlot);bDefenderRetakeOrdered=true;}
        if(Objective->Systems->FactionId!=AttackerFaction)bDefenderRetakeOrdered=false;
    }
}

FOVMissionScore UOVMissionSubsystem::GetScore() const
{
    FOVMissionScore Out=Score;Out.GameTime=GameTime;
    auto* Interior=GetWorld()->GetSubsystem<UOVInteriorSubsystem>();auto* Wing=GetWorld()->GetSubsystem<UOVCarrierSubsystem>();
    const int32 Costs[]={25,5,1};
    for(int32 Faction=0;Faction<2;++Faction)
    {
        FOVMissionFactionScore Team;Team.FactionId=Faction;
        for(int32 I=0;I<3;++I)if(Roster.IsValidIndex(Faction*3+I))
        {
            const auto* Ship=Roster[Faction*3+I].Get();if(!Living(const_cast<AOVShip*>(Ship)))++Team.ShipsLost;
            else if(Ship->Systems->FactionId==Faction)Team.LivingCost+=Costs[I];
        }
        for(const auto& Unit:Interior->Units)if(Unit.FactionId==Faction)
        {
            if(Unit.Role==EOVInteriorRole::Trooper){++Team.Troops;Team.TroopsDead+=Unit.State==EOVInteriorState::Dead?1:0;}
            else{++Team.Heroes;Team.HeroesDead+=Unit.State==EOVInteriorState::Dead?1:0;Team.HeroesWounded+=Unit.bWounded?1:0;Team.HeroesRescueWaiting+=Unit.State==EOVInteriorState::RescueWaiting?1:0;}
        }
        for(const auto& Craft:Wing->Aircraft)if(Craft.FactionId==Faction)
        {if(Craft.State==EOVAircraftState::Wrecked)++Team.AircraftLost;else++Team.AircraftSurviving;}
        Out.Factions.Add(Team);
    }
    return Out;
}

void UOVMissionSubsystem::Finish(EOVMissionResult Outcome,const FString& Message)
{
    if(Result!=EOVMissionResult::Ongoing)return;
    Result=Outcome;Phase=EOVMissionPhase::Completed;Score.FinishedAt=GameTime;Status=Message;
    if(bUsesOwnedRoster && (Outcome==EOVMissionResult::Victory || Outcome==EOVMissionResult::Defeat) && GetWorld()->GetGameInstance())
    {
        auto* Heroes=GetWorld()->GetGameInstance()->GetSubsystem<UOVHeroSubsystem>();
        for(const auto& Pair:HeroParticipation)if(Pair.Value>0)
            Heroes->AwardBattleXP(Pair.Key,BattleId,Outcome==EOVMissionResult::Victory,Pair.Value,HeroObjectiveXP.FindRef(Pair.Key));
    }
    // 결과 표시가 외부전·기발사 공격·잔존 병력을 지우거나 무료 귀환시키지 않는다.
}

void UOVMissionSubsystem::Simulate(float Delta)
{
    if(!bEnabled||Delta<=0||!FMath::IsFinite(Delta))return;
    GameTime+=Delta;TrackHeroes(Delta);DecisionRemaining-=Delta;if(DecisionRemaining>0)return;
    DecisionRemaining=FMath::Max(.01f,Settings.DecisionInterval);
    RefreshMetrics();
    if(Result!=EOVMissionResult::Ongoing)return;
    if(Mode==EOVMissionMode::FleetOnly)
    {
        if(!Living(Roster[0]))Finish(EOVMissionResult::Defeat,TEXT("아군 주력함 상실 · 함대전 종료"));
        else if(!Living(Roster[3]))Finish(EOVMissionResult::Victory,TEXT("적 주력함 완파 · 함대전 목표 달성"));
        return;
    }
    AdvanceOrders();
    auto* Interior=GetWorld()->GetSubsystem<UOVInteriorSubsystem>();auto* Transport=GetWorld()->GetSubsystem<UOVTransportSubsystem>();
    int32 Inside=0,Outstanding=0,DeadOrRescue=0,Queued=0;
    for(int32 Id:AttackUnitIds)
    {
        const auto U=Interior->GetUnit(Id);const auto P=Transport->GetPerson(U.PersonId);
        const bool Lost=U.State==EOVInteriorState::Dead||U.State==EOVInteriorState::RescueWaiting;
        DeadOrRescue+=Lost?1:0;
        Inside+=Living(U)&&U.Ship==Objective?1:0;
        Outstanding+=LaunchedIds.Contains(Id)&&!ReturnedIds.Contains(Id)&&!Lost?1:0;
        Queued+=Transport->Pods.ContainsByPredicate([&](const auto& Pod){return Pod.PersonId==U.PersonId&&Pod.State==EOVPodFlightState::Queued;})?1:0;
    }
    if(bRetreatOrdered)Phase=EOVMissionPhase::Extracting;
    else if(Inside>0)Phase=EOVMissionPhase::Interior;
    else if(bDispatched)Phase=EOVMissionPhase::Transit;
    else if(Living(Objective)&&IsObserved(Origin,Objective)&&!Objective->Systems->bShieldActive)Phase=EOVMissionPhase::ShieldWindow;
    else Phase=EOVMissionPhase::Battle;
    if(!Living(Objective))
    {
        Finish(EOVMissionResult::Defeat,TEXT("강습 목표 완파 · 내부 인원 손실과 외부 귀환 결과 확인"));return;
    }
    if(bDispatched&&Score.Launched>0&&Outstanding==0&&Queued==0)
    {
        if(Mode==EOVMissionMode::Defense)
        {
            Finish(Objective->Systems->FactionId==0?EOVMissionResult::Victory:EOVMissionResult::Defeat,
                Objective->Systems->FactionId==0?TEXT("적 강습 종료 · 함교 통제 유지"):TEXT("적 강습 종료 · 함교 통제 상실"));
        }
        else if(Score.ReturnedAfterEntry>0&&(Score.ShieldOperationAt>=0||Score.BridgeCaptureAt>=0))Finish(EOVMissionResult::Victory,TEXT("실제 내부 목표 달성 · 생존 인원 원 수단 회수"));
        else if(DeadOrRescue==AttackUnitIds.Num())Finish(EOVMissionResult::Defeat,TEXT("강습조 작전 불능 · 사망/구조 대기 분리 기록"));
        else if(bRetreatOrdered)Finish(EOVMissionResult::Withdrawn,TEXT("목표 미완료 철수 · 회수 인원과 손실 확인"));
    }
}

FString UOVMissionSubsystem::GetPublicStatus() const
{
    if(!bEnabled)return TEXT("");
    if(Result!=EOVMissionResult::Ongoing)return Status;
    if(Mode==EOVMissionMode::FleetOnly)return TEXT("함대전 비교 · 강습 없음 · 실제 무장/피격 진행");
    if(Phase==EOVMissionPhase::Extracting)return TEXT("실제 철수 중 · 원 진입점까지 이동 후 원 포드/생존 강습선으로 회수");
    if(Phase==EOVMissionPhase::Interior)
    {
        if(Living(Objective)&&Objective->Systems->bShieldActive)return TEXT("선내 작전 · 쉴드 재가동 경보 · 추가 강습 차단");
        return Mode==EOVMissionMode::Defense?TEXT("침입 경보 · 쉴드실/함교 수호 또는 실제 재점령"):TEXT("선내 작전 · 내부 아군 보호로 사격 보류 · 쉴드실/함교/철수 선택");
    }
    if(Phase==EOVMissionPhase::Transit)return TEXT("강습 이동 중 · 요격/쉴드 재가동/수송선 상실 위험");
    if(Phase==EOVMissionPhase::ShieldWindow)return TEXT("관측된 쉴드 비활성 · 포드 또는 강습선 투입 가능");
    return TEXT("실제 함대 교전 · 적 쉴드 제거와 탐지 유지 필요");
}

void UOVMissionSubsystem::MarkComparisonTimeout()
{
    Finish(EOVMissionResult::Unresolved,TEXT("측정 시간 종료 · 승패/사망으로 치환하지 않은 미결정 상태"));
}

bool UOVMissionSubsystem::HandleAction(FName Action)
{
    auto* PC=Cast<AOVController>(GetWorld()->GetFirstPlayerController());
    if(Action==TEXT("MissionAttack"))return StartMission(PC,EOVMissionMode::Attack);
    if(Action==TEXT("MissionDefense"))return StartMission(PC,EOVMissionMode::Defense);
    if(Action==TEXT("MissionFleetOnly"))return StartMission(PC,EOVMissionMode::FleetOnly);
    if(!bEnabled||!PC)return false;
    if(Action==TEXT("MissionRestart")||Action==TEXT("RestartScene"))
    {
        // Restarting the example fixture would detach the owned roster and silently replace its team.
        // Keep the actual operation and transport state until recovery and explicit roster-room departure.
        if(bUsesOwnedRoster)
        {
            Status=TEXT("재시작 보류 · 회수 후 편성실에서 재출전하세요");
            PC->LastAction=Status;
            return true;
        }
        return StartMission(PC,Mode,Seed,AttackHeroCount,DefenseHeroCount,bUseShuttle);
    }
    bool Handled=true;
    if(Action==TEXT("MissionPods")){if(Mode==EOVMissionMode::Attack)Dispatch(false);}
    else if(Action==TEXT("MissionShuttle")){if(Mode==EOVMissionMode::Attack)Dispatch(true);}
    else if(Action==TEXT("MissionShield"))OrderTeam(EOVInteriorOrder::Capture,ShieldSlot);
    else if(Action==TEXT("MissionBridge"))OrderTeam(EOVInteriorOrder::Capture,BridgeSlot);
    else if(Action==TEXT("MissionSabotage"))OrderTeam(EOVInteriorOrder::Sabotage,ShieldSlot);
    else if(Action==TEXT("MissionDefendShield"))OrderTeam(EOVInteriorOrder::Defend,ShieldSlot);
    else if(Action==TEXT("MissionDefendBridge")||Action==TEXT("MissionDefend"))OrderTeam(EOVInteriorOrder::Defend,BridgeSlot);
    else if(Action==TEXT("MissionRetreat"))OrderTeam(EOVInteriorOrder::Retreat);
    else if(Action==TEXT("MissionAircraftLaunch")){if(PC->Selected&&PC->Selected->bFriendly)GetWorld()->GetSubsystem<UOVCarrierSubsystem>()->Launch(PC->Selected);}
    else if(Action==TEXT("MissionAircraftRecall")){if(PC->Selected&&PC->Selected->bFriendly)GetWorld()->GetSubsystem<UOVCarrierSubsystem>()->Recall(PC->Selected);}
    else Handled=false;
    if(Handled)PC->LastAction=Status;
    return Handled;
}

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
namespace OVMissionTest
{
struct FRange
{
    UWorld* World;AOVController* PC;UOVMissionSubsystem* Mission;
    FRange()
    {
        World=UWorld::CreateWorld(EWorldType::Game,false);PC=World->SpawnActor<AOVController>();
        PC->Combat=World->GetSubsystem<UOVCombatSubsystem>();PC->Carrier=World->GetSubsystem<UOVCarrierSubsystem>();
        PC->Transport=World->GetSubsystem<UOVTransportSubsystem>();PC->PointDefense=World->GetSubsystem<UOVPointDefenseSubsystem>();
        PC->Interior=World->GetSubsystem<UOVInteriorSubsystem>();PC->Mission=Mission=World->GetSubsystem<UOVMissionSubsystem>();
        for(int32 I=0;I<3;++I)PC->Ships.Add(World->SpawnActor<AOVShip>());
    }
    ~FRange(){World->DestroyWorld(false);}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVMissionSetup,"OrbitalVanguard.Mission.RealRosterShieldAndTransportGate",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FOVMissionSetup::RunTest(const FString&)
{
    OVMissionTest::FRange R;TestTrue(TEXT("정상 후보 편성 생성"),R.Mission->StartMission(R.PC,EOVMissionMode::Attack,260112,4,2));
    TestEqual(TEXT("6척 편성"),R.PC->Ships.Num(),6);TestEqual(TEXT("양측 기체 80대의 실제 재고"),R.PC->Carrier->Aircraft.Num(),80);
    TestEqual(TEXT("대표 4유형과 일반병6명"),R.Mission->AttackUnitIds.Num(),10);
    TestTrue(TEXT("적 쉴드가 활성·정상 최대량으로 시작"),R.Mission->Objective->Systems->bShieldActive&&R.Mission->Objective->Systems->Shield==R.Mission->Objective->Systems->MaxShield);
    TestFalse(TEXT("활성 쉴드에 임무 포드 명령 거절"),R.Mission->Dispatch(false));
    TestEqual(TEXT("거절이 포드를 만들지 않음"),R.PC->Transport->Pods.Num(),0);
    TestFalse(TEXT("진입 전 적 시설 원격 점령 명령 거절"),R.Mission->OrderTeam(EOVInteriorOrder::Capture,R.Mission->ShieldSlot));
    bool Runtime=true;for(const auto& P:R.PC->Transport->Persons)Runtime&=P.RuntimeUnitId>0&&!P.bHeroTypeDEV;
    TestTrue(TEXT("DEV 승객 대신 실제 유닛 연결"),Runtime);
    const auto Score=R.Mission->GetScore();TestEqual(TEXT("아군 등록 코스트31"),Score.Factions[0].LivingCost,31);TestEqual(TEXT("적 등록 코스트31"),Score.Factions[1].LivingCost,31);
    TestEqual(TEXT("대형 출격 상주 한도 내 아군4"),R.PC->Interior->CountActiveHeroes(0),4);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVMissionReset,"OrbitalVanguard.Mission.RestartDefenseAndLegacyPreservation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FOVMissionReset::RunTest(const FString&)
{
    OVMissionTest::FRange R;R.Mission->StartMission(R.PC,EOVMissionMode::Attack,260113,2,4);
    const int32 OldId=R.Mission->AttackUnitIds[0];const int32 FirstCount=R.PC->Transport->Persons.Num();
    R.Mission->StartMission(R.PC,EOVMissionMode::Attack,260113,2,4);
    TestEqual(TEXT("명시 재시작의 인원수는 보존"),R.PC->Transport->Persons.Num(),FirstCount);
    TestTrue(TEXT("이전 임무 ID가 새 인물을 지칭하지 않음"),R.Mission->AttackUnitIds[0]>OldId);
    R.Mission->StartMission(R.PC,EOVMissionMode::Defense,260113,2,4);
    TestEqual(TEXT("방어 장면 목표는 플레이어 소유"),R.Mission->Objective->Systems->FactionId,0);
    TestEqual(TEXT("공격자는 적 소속"),R.Mission->Origin->Systems->FactionId,1);
    bool Own=true;for(int32 Id:R.Mission->GetPlayerUnitIds())Own&=R.PC->Interior->GetUnit(Id).FactionId==0;
    TestTrue(TEXT("플레이어 명령 목록에 적이 섞이지 않음"),Own);
    R.PC->StartPhase2CRange(2);TestFalse(TEXT("F7 전환은 통합 임무 해제"),R.Mission->bEnabled);TestEqual(TEXT("F7 기존3척 복구"),R.PC->Ships.Num(),3);
    R.PC->StartCalibrationRange(1);TestEqual(TEXT("F6 기존 교정 장면 복구"),R.PC->Ships.Num(),3);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVMissionPause,"OrbitalVanguard.Mission.PauseManualPriorityAndTimeout",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FOVMissionPause::RunTest(const FString&)
{
    OVMissionTest::FRange R;R.Mission->StartMission(R.PC,EOVMissionMode::Attack);
    R.PC->StepSimulation(10);TestEqual(TEXT("정지 중 임무 게임시간 불변"),R.Mission->GameTime,0.);
    R.Mission->Origin->SetDestination(FVector(-10000,2000,8000));R.Mission->Origin->SetAutomation(false,true);
    R.Mission->SetAutoPlayer(true);
    TestFalse(TEXT("임무 시험 AI가 수동 이동을 켜지 않음"),R.Mission->Origin->IsMovementAutomatic());
    TestTrue(TEXT("기존 XYZ 목적지 유지"),R.Mission->Origin->bHasDestination);
    R.PC->bSimulationPaused=false;R.PC->StepSimulation(1);
    TestTrue(TEXT("공통 전투 시계에서 임무 전진"),R.Mission->GameTime>.9);
    const int32 Deaths=R.PC->Interior->TotalDeaths;R.Mission->MarkComparisonTimeout();
    TestEqual(TEXT("시간 초과는 미결정"),R.Mission->Result,EOVMissionResult::Unresolved);
    TestEqual(TEXT("시간 초과가 사망을 만들지 않음"),R.PC->Interior->TotalDeaths,Deaths);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVMissionRecallMetric,"OrbitalVanguard.Mission.PreEntryRecallIsNotInteriorExtraction",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FOVMissionRecallMetric::RunTest(const FString&)
{
    OVMissionTest::FRange R;R.Mission->StartMission(R.PC,EOVMissionMode::Attack);
    // 이 단위 검사는 회항 집계 경계를 격리한다. 실제 무장 쉴드 제거는 PIE 통합 임무에서 별도로 검증한다.
    auto* Target=R.Mission->Objective.Get();
    Target->Systems->ApplyWorldHit(Target->GetActorLocation(),FVector::ForwardVector,Target->Systems->Shield,EOVDamageCause::External,true);
    TestTrue(TEXT("쉴드가 제거된 뒤 정상 포드 명령 접수"),R.Mission->Dispatch(false));
    R.PC->Transport->Simulate(.1f);R.PC->Interior->Simulate(.1f);R.Mission->Simulate(.1f);
    TestTrue(TEXT("실제로 발사한 개인 포드 존재"),R.Mission->GetScore().Launched>0);
    R.PC->Transport->RecallPods(R.Mission->Origin);
    for(int32 I=0;I<300;++I){R.PC->Transport->Simulate(1.f/60);R.PC->Interior->Simulate(1.f/60);R.Mission->Simulate(1.f/60);}
    const auto Score=R.Mission->GetScore();
    TestTrue(TEXT("발사 후 비행 회항도 실제 귀환에 포함"),Score.Returned>0);
    TestEqual(TEXT("진입하지 않은 사람은 내부 진입0"),Score.Entered,0);
    TestEqual(TEXT("비행 회항은 선내 철수 성공이 아님"),Score.ReturnedAfterEntry,0);
    TestTrue(TEXT("미진입 회항만으로 임무 승리 불가"),R.Mission->Result!=EOVMissionResult::Victory);
    return true;
}
#endif
