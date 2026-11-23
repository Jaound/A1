#include "OVPlayScenarioSubsystem.h"
#include "OVCalibration.h"
#include "OVCarrierSubsystem.h"
#include "OVCombatSubsystem.h"
#include "OVController.h"
#include "OVInteriorSubsystem.h"
#include "OVPointDefenseSubsystem.h"
#include "OVShip.h"
#include "OVShipSystemsComponent.h"
#include "OVTransportSubsystem.h"

bool UOVPlayScenarioSubsystem::PrepareArenaForces(AOVController& PC)
{
    AOVShip* Home=PC.Ships[0]; AOVShip* Target=PC.Ships[1];
    for(int32 Wave=0;Wave<2;++Wave){
        ArenaWaves[Wave].Troopers=PC.Interior->RegisterBoardingTeam(Home,0,0,12);
        if(ArenaWaves[Wave].Troopers.Num()!=12)return false;
    }
    for(const auto Facility:{EOVFacilityType::Core,EOVFacilityType::Bridge,EOVFacilityType::LifeSupport}){
        const auto* Room=Target->Systems->Layout->Slots.FindByPredicate([Facility](const auto& D){return D.Facility==Facility;});
        if(!Room)return false;
        int32 Squad=0;
        for(int32 I=0;I<4;++I){
            const int32 Id=PC.Interior->RegisterUnit(Target,1,EOVInteriorRole::Trooper,Room->SlotId);
            if(!Id)return false;
            if(I==0)Squad=Id;
            PC.Interior->Units.Last().SquadId=Squad;
        }
    }
    int32 Index=0;
    for(const auto& Slot:Home->Systems->Layout->Slots){
        if(Slot.Facility!=EOVFacilityType::Hangar)continue;
        auto& Wave=ArenaWaves[Index];Wave.Bay=Slot.SlotId;
        AOVShip* Ship=GetWorld()->SpawnActor<AOVShip>();Wave.Shuttle=Ship;
        if(!Ship || !OVCalibration::ConfigureHull(*PC.Combat,*Ship,TEXT("assault"),true,Home->GetActorLocation(),
            OVCalibration::GetHullGeometryScale(TEXT("assault")),0,false,true) || !PC.Carrier->StoreShuttle(Home,Slot.SlotId,Ship))return false;
        Ship->ShipName=Index==0?TEXT("A팀 강습선"):TEXT("B팀 강습선");
        Ship->SetActorTickEnabled(false);
        if(++Index==2)break;
    }
    return Index==2;
}

bool UOVPlayScenarioSubsystem::SendArenaWave(AOVController& PC,int32 Index)
{
    auto& Wave=ArenaWaves[Index];
    if(Wave.Sent){PC.LastAction=TEXT("이미 출발한 강습팀입니다");return false;}
    auto* Transit=PC.Transport.Get(); AOVShip* Home=PC.Ships[0];AOVShip* Target=PC.Ships[1];
    if(Transit->NextTeamDelay(Target)>.0001f){PC.LastAction=FString::Printf(TEXT("동일 목표 출발까지 %d초"),FMath::CeilToInt(Transit->NextTeamDelay(Target)));return false;}
    TArray<int32> Team;
    for(const auto& U:PC.Interior->Units){
        if(Team.Num()>=ArenaHeroQuota)break;
        if(U.FactionId==0 && U.Ship==Home && U.Role!=EOVInteriorRole::Trooper && !U.bDutyRequested && U.HP>0 &&
            !ArenaWaves[1-Index].Units.Contains(U.UnitId))Team.Add(U.UnitId);
    }
    if(Team.Num()!=ArenaHeroQuota){PC.LastAction=TEXT("보직 없는 영웅을 더 배치하거나 팀 영웅 수를 줄이세요");return false;}
    for(const auto& U:PC.Interior->Units){
        if(Team.Num()==12)break;
        if(U.FactionId==0 && U.Ship==Home && U.Role==EOVInteriorRole::Trooper && U.HP>0 && Wave.Troopers.Contains(U.UnitId))Team.Add(U.UnitId);
    }
    if(Team.Num()!=12){PC.LastAction=TEXT("가용 일반 병력이 부족합니다");return false;}
    const bool ByShuttle=ArenaRoute==1 || (ArenaRoute==2 && Index==1);
    auto* Bay=PC.Carrier->Hangars.FindByPredicate([&](const auto& H){return H.Carrier==Home && H.SlotId==Wave.Bay;});
    AOVShip* Ship=Wave.Shuttle.Get();
    if(ByShuttle && (!Ship || !Bay || Bay->StoredSmallShip!=Ship || !PC.Carrier->IsHangarOperational(*Bay))){
        PC.LastAction=TEXT("가동 중인 격납고에 해당 팀 강습선이 필요합니다");return false;
    }
    const auto People=PC.Interior->GetPassengerIds(Team);
    if(People.Num()!=Team.Num()){PC.LastAction=TEXT("승객 등록이 누락되어 출발할 수 없습니다");return false;}
    TArray<int32> Assigned;
    auto Undo=[&](){for(int32 Unit:Assigned)PC.Interior->SetHeroBoardingAssignment(Unit,false);};
    for(int32 I=0;I<ArenaHeroQuota;++I){
        if(!Transit->GetPerson(People[I]).bBoardingAssigned){
            if(!PC.Interior->SetHeroBoardingAssignment(Team[I],true)){Undo();PC.LastAction=PC.Interior->LastMessage;return false;}
            Assigned.Add(Team[I]);
        }
    }
    if(ByShuttle){
        Ship->SetActorLocation(PC.Carrier->HangarPosition(*Bay));
        if(!Transit->Shuttles.ContainsByPredicate([Ship](const auto& S){return S.Ship==Ship;}))Transit->RegisterShuttle(Ship,Home);
        if(!Transit->DispatchShuttle(Ship,Target,People)){Undo();PC.LastAction=Transit->LastMessage;return false;}
        Bay->StoredSmallShip.Reset();Ship->SetActorHiddenInGame(false);Ship->SetActorEnableCollision(true);Ship->SetActorTickEnabled(true);
        PC.Ships.AddUnique(Ship);PC.Combat->RegisterShip(Ship,1,false,1);
        PC.PointDefense->RegisterShip(Ship,1);Transit->AddShip(Ship);PC.Interior->AddShip(Ship);
    }else if(Transit->LaunchPodTeam(Home,Target,People)!=People.Num()){Undo();PC.LastAction=Transit->LastMessage;return false;}
    Wave.Units=MoveTemp(Team);Wave.Sent=true;
    PC.LastAction=ByShuttle?TEXT("격납고에서 강습선 출발"):TEXT("개인 포드 발사 개시");
    return true;
}

void UOVPlayScenarioSubsystem::RecallArenaWave(AOVController& PC,int32 Index)
{
    auto& W=ArenaWaves[Index];if(!W.Sent)return;
    W.Recalled=true;
    PC.Interior->IssueOrder(W.Units,EOVInteriorOrder::Retreat,NAME_None);
    PC.Transport->RecallTeamPods(PC.Interior->GetPassengerIds(W.Units));
    if(ArenaRoute==1 || (ArenaRoute==2 && Index==1))PC.Transport->RecallShuttle(W.Shuttle.Get());
}

FString UOVPlayScenarioSubsystem::ArenaWaveStatus(int32 Index) const
{
    const auto& W=ArenaWaves[Index];
    const bool Shuttle=ArenaRoute==1 || (ArenaRoute==2 && Index==1);
    const TCHAR* Route=Shuttle?TEXT("강습선"):TEXT("포드");
    if(!W.Sent)return FString::Printf(TEXT("%s · 영웅 %d / 병력 %d"),Route,ArenaHeroQuota,12-ArenaHeroQuota);
    int32 Inside=0,Home=0,Loss=0;
    for(int32 Id:W.Units){
        const auto U=Owner->Interior->GetUnit(Id);
        const auto P=Owner->Transport->GetPerson(U.PersonId);
        if(P.State==EOVPassengerState::InsideTarget || P.State==EOVPassengerState::AwaitingRecovery)++Inside;
        if(P.State==EOVPassengerState::Aboard)++Home;
        if(P.State==EOVPassengerState::Dead || P.State==EOVPassengerState::RescueWaiting)++Loss;
    }
    return FString::Printf(TEXT("%s · 내부 %d / 탑승 %d / 손실 %d"),Route,Inside,Home,Loss);
}

void UOVPlayScenarioSubsystem::StepArena(AOVController& PC,float Delta)
{
    if(!bArenaStarted || PC.Ships.Num()<2)return;
    ElapsedSeconds+=Delta;
    AOVShip* Home=PC.Ships[0];AOVShip* Target=PC.Ships[1];
    if(Target->Systems->FactionId==0){Finish(PC,EOVPlaySceneResult::Victory,TEXT("적 함교 점령"));return;}
    int32 Living=0,Recovered=0;
    for(auto& W:ArenaWaves){
        if(!W.Sent)continue;
        auto* Record=PC.Transport->Shuttles.FindByPredicate([&](const auto& S){return S.Ship==W.Shuttle;});
        bool AllOnWayBack=true;
        for(int32 Id:W.Units){
            const auto U=PC.Interior->GetUnit(Id);const auto P=PC.Transport->GetPerson(U.PersonId);
            if(P.State==EOVPassengerState::Dead || P.State==EOVPassengerState::RescueWaiting)continue;
            ++Living;
            if(P.State==EOVPassengerState::Aboard && P.CurrentShip==Home && U.Ship==Home && U.State!=EOVInteriorState::InTransit){
                if(W.Recalled || PC.Transport->Pods.ContainsByPredicate([&](const auto& Pod){return Pod.PersonId==P.PersonId && Pod.State==EOVPodFlightState::Recovered;}))++Recovered;
            }
            if(P.State==EOVPassengerState::InsideTarget && U.Order!=EOVInteriorOrder::Retreat)AllOnWayBack=false;
        }
        if(Record && Record->State==EOVShuttleState::Delivered && AllOnWayBack){PC.Transport->RecallShuttle(W.Shuttle.Get());W.Recalled=true;}
        if(Record && Record->State==EOVShuttleState::Ready && PC.Ships.Contains(W.Shuttle.Get())){
            auto* Ship=W.Shuttle.Get();
            if(PC.Carrier->StoreShuttle(Home,W.Bay,Ship)){
                for(auto& P:PC.Transport->Persons)if(P.CurrentShip==Ship && P.State==EOVPassengerState::Aboard)P.CurrentShip=Home;
                PC.Ships.Remove(Ship);PC.Combat->Ships.RemoveAll([Ship](const auto& S){return S.Ship==Ship;});
                PC.PointDefense->Ships.RemoveAll([Ship](const auto& S){return S.Ship==Ship;});
                if(PC.Selected==Ship)PC.SelectShip(Home);
                Ship->SetActorHiddenInGame(true);Ship->SetActorEnableCollision(false);Ship->SetActorTickEnabled(false);
            }
        }
    }
    if(!Living && ArenaWaves[0].Sent && ArenaWaves[1].Sent)Finish(PC,EOVPlaySceneResult::Defeat,TEXT("출발한 강습대 전원 작전 불능"));
    else if(Living>0 && Recovered==Living)Finish(PC,EOVPlaySceneResult::PartialVictory,TEXT("출발한 생존 영웅·일반 병력 전원 철수"));
}
