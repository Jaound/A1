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
        if(bPreparingBoardingLesson && Facility!=EOVFacilityType::Core)continue;
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
    for(const auto& W:ArenaWaves)for(int32 Id:W.Troopers){
        const int32 Squad=PC.Interior->GetUnit(Id).SquadId;
        ArenaMembers.FindOrAdd(Squad).Add(Id);
    }
    PresetArenaFormation();
    return Index==2;
}

bool UOVPlayScenarioSubsystem::SendArenaWave(AOVController& PC,int32 Index)
{
    auto& Wave=ArenaWaves[Index];
    if(Wave.Sent){PC.LastAction=TEXT("이미 출발한 강습팀입니다");return false;}
    auto* Transit=PC.Transport.Get(); AOVShip* Home=PC.Ships[0];AOVShip* Target=PC.Ships[1];
    if(Transit->NextTeamDelay(Target)>.0001f){PC.LastAction=FString::Printf(TEXT("동일 목표 출발까지 %d초"),FMath::CeilToInt(Transit->NextTeamDelay(Target)));return false;}
    TArray<int32> Team;
    for(int32 Group:ArenaGroups[Index]){
        for(int32 Id:ArenaMembers.FindChecked(Group)){
            const auto U=PC.Interior->GetUnit(Id);
            if(U.Ship!=Home || U.HP<=0 || U.bDutyRequested){
                PC.LastAction=TEXT("편성 인원의 위치·보직을 확인하세요");return false;
            }
            Team.Add(Id);
        }
    }
    if(Team.IsEmpty()){PC.LastAction=TEXT("먼저 이 팀에 인원을 편성하세요");return false;}
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
    for(int32 I=0;I<Team.Num();++I){
        if(PC.Interior->GetUnit(Team[I]).Role!=EOVInteriorRole::Trooper && !Transit->GetPerson(People[I]).bBoardingAssigned){
            if(!PC.Interior->SetHeroBoardingAssignment(Team[I],true)){Undo();PC.LastAction=PC.Interior->LastMessage;return false;}
            Assigned.Add(Team[I]);
        }
    }
    if(ByShuttle){
        Ship->SetActorLocation(PC.Carrier->HangarPosition(*Bay));
        if(!Transit->Shuttles.ContainsByPredicate([Ship](const auto& S){return S.Ship==Ship;}))Transit->RegisterShuttle(Ship,Home);
        if(!Transit->DispatchShuttle(Ship,Target,People)){Undo();PC.LastAction=Transit->LastMessage;return false;}
        Bay->StoredSmallShip.Reset();Ship->HangarHost.Reset();Ship->SetActorHiddenInGame(false);Ship->SetActorEnableCollision(true);Ship->SetActorTickEnabled(true);
        PC.Ships.AddUnique(Ship);PC.Combat->RegisterShip(Ship,1,false,1);
        PC.PointDefense->RegisterShip(Ship,1);Transit->AddShip(Ship);PC.Interior->AddShip(Ship);
    }else if(Transit->LaunchPodTeam(Home,Target,People)!=People.Num()){Undo();PC.LastAction=Transit->LastMessage;return false;}
    Wave.Units=MoveTemp(Team);Wave.UsedShuttle=ByShuttle;Wave.ActualShuttle=ByShuttle?Ship:nullptr;Wave.Sent=true;bArenaFormationOpen=false;
    PC.LastAction=ByShuttle?TEXT("격납고에서 강습선 출발"):TEXT("개인 포드 발사 개시");
    return true;
}

void UOVPlayScenarioSubsystem::RecallArenaWave(AOVController& PC,int32 Index)
{
    auto& W=ArenaWaves[Index];if(!W.Sent)return;
    W.Recalled=true;
    PC.Interior->IssueOrder(W.Units,EOVInteriorOrder::Retreat,NAME_None);
    PC.Transport->RecallTeamPods(PC.Interior->GetPassengerIds(W.Units));
    if(W.UsedShuttle)PC.Transport->RecallShuttle(W.ActualShuttle.Get());
}

FString UOVPlayScenarioSubsystem::ArenaWaveStatus(int32 Index) const
{
    const auto& W=ArenaWaves[Index];
    const bool Shuttle=W.Sent?W.UsedShuttle:(ArenaRoute==1 || (ArenaRoute==2 && Index==1));
    const TCHAR* Route=Shuttle?TEXT("강습선"):TEXT("포드");
    if(!W.Sent){
        int32 Heroes=0;
        for(int32 Group:ArenaGroups[Index])Heroes+=ArenaMembers.FindChecked(Group).Num()==1;
        return FString::Printf(TEXT("대기 · 영웅 %d / 병력 %d"),Heroes,ArenaGroupHeadcount(Index)-Heroes);
    }
    int32 Inside=0,Transit=0,Aboard=0,Loss=0;
    for(int32 Id:W.Units){
        const auto U=Owner->Interior->GetUnit(Id);
        const auto P=Owner->Transport->GetPerson(U.PersonId);
        if(P.State==EOVPassengerState::InsideTarget || P.State==EOVPassengerState::AwaitingRecovery)++Inside;
        if(P.State==EOVPassengerState::InShuttle || P.State==EOVPassengerState::InPod)++Transit;
        if(P.State==EOVPassengerState::Aboard)++Aboard;
        if(P.State==EOVPassengerState::Dead || P.State==EOVPassengerState::RescueWaiting)++Loss;
    }
    return FString::Printf(TEXT("%s · 이동 %d / 내부 %d / 탑승 %d / 손실 %d"),Route,Transit,Inside,Aboard,Loss);
}

void UOVPlayScenarioSubsystem::StepArena(AOVController& PC,float Delta)
{
    if(!bArenaStarted || PC.Ships.Num()<2)return;
    ElapsedSeconds+=Delta;
    AOVShip* Home=PC.Ships[0];AOVShip* Target=PC.Ships[1];
    if(PracticeChapter==INDEX_NONE && Target->Systems->FactionId==0){Finish(PC,EOVPlaySceneResult::Victory,TEXT("적 함교 점령"));return;}
    int32 Living=0,Recovered=0;
    for(auto& W:ArenaWaves){
        if(!W.Sent)continue;
        auto* Record=PC.Transport->Shuttles.FindByPredicate([&](const auto& S){return S.Ship==W.ActualShuttle;});
        bool AllOnWayBack=true;
        for(int32 Id:W.Units){
            const auto U=PC.Interior->GetUnit(Id);const auto P=PC.Transport->GetPerson(U.PersonId);
            if(P.State==EOVPassengerState::Dead || P.State==EOVPassengerState::RescueWaiting)continue;
            ++Living;
            const bool OnReturnedShuttle=W.PlayerDirected && W.ActualShuttle.IsValid() && W.ActualShuttle->HangarHost==Home && P.CurrentShip==W.ActualShuttle;
            if(P.State==EOVPassengerState::Aboard && ((P.CurrentShip==Home && U.Ship==Home)||OnReturnedShuttle) && U.State!=EOVInteriorState::InTransit){
                if(W.Recalled || PC.Transport->Pods.ContainsByPredicate([&](const auto& Pod){return Pod.PersonId==P.PersonId && Pod.State==EOVPodFlightState::Recovered;}))++Recovered;
            }
            if(P.State==EOVPassengerState::InsideTarget && U.Order!=EOVInteriorOrder::Retreat)AllOnWayBack=false;
        }
        if(Record && Record->State==EOVShuttleState::Delivered && AllOnWayBack){PC.Transport->RecallShuttle(W.ActualShuttle.Get());W.Recalled=true;}
        if(W.Recalled && Record && Record->State==EOVShuttleState::Ready && PC.Ships.Contains(W.ActualShuttle.Get())){
            auto* Ship=W.ActualShuttle.Get();
            if(W.PlayerDirected){
                if(PracticeChapter!=8 && !Ship->HangarHost.IsValid()){
                    auto* Bay=PC.Carrier->Hangars.FindByPredicate([&](const auto& H){return H.Carrier==Home && H.Mode==EOVHangarMode::SmallShip && !H.StoredSmallShip.IsValid() && !H.InboundShip.IsValid() && PC.Carrier->IsHangarOperational(H);});
                    if(Bay && !PC.Carrier->Hangars.ContainsByPredicate([Ship](const auto& H){return H.InboundShip==Ship;}))PC.RequestSmallShipDock(Bay->HangarId,Ship);
                }
            }else if(PC.Carrier->StoreShuttle(Home,W.Bay,Ship)){
                for(auto& P:PC.Transport->Persons)if(P.CurrentShip==Ship && P.State==EOVPassengerState::Aboard)P.CurrentShip=Home;
                PC.Ships.Remove(Ship);PC.Combat->Ships.RemoveAll([Ship](const auto& S){return S.Ship==Ship;});
                PC.PointDefense->Ships.RemoveAll([Ship](const auto& S){return S.Ship==Ship;});
                if(PC.Selected==Ship)PC.SelectShip(Home);
                Ship->SetActorHiddenInGame(true);Ship->SetActorEnableCollision(false);Ship->SetActorTickEnabled(false);
            }
        }
    }
    if(PracticeChapter!=INDEX_NONE)return;
    if(!Living && ArenaWaves[0].Sent && ArenaWaves[1].Sent)Finish(PC,EOVPlaySceneResult::Defeat,TEXT("출발한 강습대 전원 작전 불능"));
    else if(Living>0 && Recovered==Living)Finish(PC,EOVPlaySceneResult::PartialVictory,TEXT("출발한 생존 영웅·일반 병력 전원 철수"));
}

int32 UOVPlayScenarioSubsystem::ArenaGroupHeadcount(int32 Column) const
{
    int32 Count=0;
    for(int32 Group:ArenaGroups[Column])Count+=ArenaMembers.FindChecked(Group).Num();
    return Count;
}

int32 UOVPlayScenarioSubsystem::ArenaTeamForUnit(int32 Id) const
{
    if(ActiveId==TEXT("HeroArena"))
        for(int32 I=0;I<2;++I)if(ArenaWaves[I].Sent && ArenaWaves[I].Units.Contains(Id))return I;
    return INDEX_NONE;
}

FString UOVPlayScenarioSubsystem::ArenaGroupLabel(int32 Group) const
{
    if(ArenaMembers.FindChecked(Group).Num()==1){
        const auto U=Owner->Interior->GetUnit(Group);
        return FString::Printf(TEXT("%s #%d"),*U.Name,Group);
    }
    for(int32 I=0;I<2;++I){
        const int32 Offset=ArenaWaves[I].Troopers.IndexOfByKey(Group);
        if(Offset!=INDEX_NONE)return FString::Printf(TEXT("보병 %d분대 · 4명"),1+I*3+Offset/4);
    }
    return {};
}

void UOVPlayScenarioSubsystem::PresetArenaFormation()
{
    for(auto& Column:ArenaGroups)Column.Reset();
    TArray<int32> Keys;
    ArenaMembers.GetKeys(Keys);
    Keys.Sort();
    int32 Heroes=0,Squads=0;
    const int32 SquadsPerTeam=(12-ArenaHeroQuota)/4;
    for(int32 Key:Keys){
        const bool Hero=ArenaMembers.FindChecked(Key).Num()==1;
        const int32 Position=Hero?Heroes++:Squads++;
        const int32 PerTeam=Hero?ArenaHeroQuota:SquadsPerTeam;
        const int32 Column=PerTeam>0?FMath::Min(2,Position/PerTeam):2;
        ArenaGroups[Column].Add(Key);
    }
    ArenaSelectedGroup=0;ArenaFormationPage=0;
}

bool UOVPlayScenarioSubsystem::HandleArenaFormation(FName Action,AOVController& PC)
{
    if(Action==TEXT("ArenaFormation")){bArenaFormationOpen=!bArenaFormationOpen;return true;}
    if(!bArenaFormationOpen)return false;
    constexpr int32 Rows=4;
    if(Action==TEXT("ArenaPageNext") || Action==TEXT("ArenaPagePrev")){
        const int32 Largest=FMath::Max3(ArenaGroups[0].Num(),ArenaGroups[1].Num(),ArenaGroups[2].Num());
        ArenaFormationPage=FMath::Clamp(ArenaFormationPage+(Action==TEXT("ArenaPageNext")?1:-1),0,FMath::Max(0,(Largest-1)/Rows));
        return true;
    }
    const FString Name=Action.ToString();
    if(Name.StartsWith(TEXT("ArenaPick_"))){
        const int32 Id=FCString::Atoi(*Name.Mid(10));
        if(ArenaMembers.Contains(Id))ArenaSelectedGroup=Id;
        return true;
    }
    if(!Name.StartsWith(TEXT("ArenaAssign_")))return false;
    const int32 To=FCString::Atoi(*Name.Mid(12));
    const auto* People=ArenaMembers.Find(ArenaSelectedGroup);
    if(!People || To<0 || To>2)return true;
    if(!ArenaGroups[To].Contains(ArenaSelectedGroup)){
        int32 Heroes=0;
        for(int32 Group:ArenaGroups[To])Heroes+=ArenaMembers.FindChecked(Group).Num()==1;
        if(To<2 && (ArenaGroupHeadcount(To)+People->Num()>12 || (People->Num()==1 && Heroes>=4))){
            PC.LastAction=TEXT("자리가 부족합니다 · 먼저 예비로 이동하세요");return true;
        }
        for(auto& Column:ArenaGroups)Column.Remove(ArenaSelectedGroup);
        ArenaGroups[To].Add(ArenaSelectedGroup);
        bArenaCustomFormation=true;
        ArenaFormationPage=(ArenaGroups[To].Num()-1)/Rows;
    }
    ArenaSelectedGroup=0;
    return true;
}
