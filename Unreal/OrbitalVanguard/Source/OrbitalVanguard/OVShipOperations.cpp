#include "OVController.h"
#include "OVCarrierSubsystem.h"
#include "OVCombatSubsystem.h"
#include "OVInteriorSubsystem.h"
#include "OVPlayScenarioSubsystem.h"
#include "OVPointDefenseSubsystem.h"
#include "OVShip.h"
#include "OVShipSystemsComponent.h"
#include "OVTransportSubsystem.h"

namespace
{
bool Available(AOVShip* Ship)
{
    return IsValid(Ship) && Ship->Systems && Ship->Systems->Layout && !Ship->Systems->bWrecked && !Ship->Systems->bNeutral;
}
FVector ExitPoint(const AOVShip& Host,const AOVShip& Small)
{
    const FVector HostSize=Host.Systems->Layout->LocalHalfExtent*Host.GetActorScale3D();
    const FVector GuestSize=Small.Systems->Layout->LocalHalfExtent*Small.GetActorScale3D();
    return Host.GetActorLocation()+Host.GetActorUpVector()*(HostSize.Z+GuestSize.Size()+180.);
}
FOVHangarState* BayById(UOVCarrierSubsystem* Carrier,int32 Id)
{
    return Carrier ? Carrier->Hangars.FindByPredicate([Id](const auto& H){return H.HangarId==Id;}) : nullptr;
}
bool ReadyRoom(AOVShip* Origin,FName Room)
{
    if(!Available(Origin))return false;
    const int32 I=Origin->Systems->Layout->GetSlotIndex(Room);
    if(!Origin->Systems->Slots.IsValidIndex(I))return false;
    const auto& State=Origin->Systems->Slots[I];
    return State.HP>0 && (State.OccupyingFaction<0 || State.OccupyingFaction==Origin->Systems->FactionId);
}
}

bool AOVController::RequestSmallShipDock(int32 Id,AOVShip* Guest)
{
    auto* Bay=BayById(Carrier,Id);
    auto* Home=Bay?Bay->Carrier.Get():nullptr;
    if(!Bay || !Available(Home) || !Available(Guest) || Guest==Home || Guest->HangarHost.IsValid() ||
       !Ships.Contains(Guest) || Guest->Systems->FactionId!=0 || Home->Systems->FactionId!=0 ||
       !Guest->Systems->Layout->bSmallHull || Guest->Systems->Layout->DeckCount!=1 ||
       Guest->bFTLCharging || Home->bFTLCharging || Combat->IsDeparted(Guest) || Combat->IsDeparted(Home) ||
       !Guest->Systems->CanMove() || !Carrier->IsHangarOperational(*Bay) || Bay->StoredSmallShip.IsValid() || Bay->InboundShip.IsValid()){
        LastAction=TEXT("수납 불가 · 아군 소형함과 빈 가동 격납고를 확인하세요");return false;
    }
    if(Carrier->Hangars.ContainsByPredicate([Guest](const auto& H){return H.InboundShip==Guest;}) ||
       Transport->Shuttles.ContainsByPredicate([Guest](const auto& S){return S.Ship==Guest && S.State!=EOVShuttleState::Ready;})){
        LastAction=TEXT("이미 접근 중이거나 운송 임무 중입니다");return false;
    }
    if(!Carrier->SetHangarMode(Id,EOVHangarMode::SmallShip)){LastAction=TEXT("이 격납고에 배속된 함재기가 있습니다");return false;}
    Bay->InboundShip=Guest;
    Bay->ApproachDestination=ExitPoint(*Home,*Guest);
    Guest->SetDestination(Bay->ApproachDestination);
    LastAction=TEXT("격납고 접근 중 · ")+Guest->ShipName;return true;
}

bool AOVController::LaunchSmallShip(int32 Id)
{
    auto* Bay=BayById(Carrier,Id);
    auto* Home=Bay?Bay->Carrier.Get():nullptr;
    auto* Ship=Bay?Bay->StoredSmallShip.Get():nullptr;
    if(!Available(Home)||!Available(Ship)||Home->Systems->FactionId!=0||Ship->Systems->FactionId!=0||
       Home->bFTLCharging||Combat->IsDeparted(Home)||!Carrier->IsHangarOperational(*Bay)){
        LastAction=TEXT("출격 불가 · 격납고 가동과 정박 함선을 확인하세요");return false;
    }
    if(CrewOperations.ContainsByPredicate([&](const auto& Op){return Op.bTransfer && (Op.Target==Ship || Op.Origin==Ship);})){
        LastAction=TEXT("승하선 중 · 완료하거나 승하선 명령을 취소하세요");return false;
    }
    FVector Location=ExitPoint(*Home,*Ship);
    const double Radius=(Ship->Systems->Layout->LocalHalfExtent*Ship->GetActorScale3D()).Size();
    bool Clear=false;
    for(int32 I=0;I<8 && !Clear;++I){
        Clear=!Ships.ContainsByPredicate([&](const auto& Other){return IsValid(Other)&&Other!=Home&&Other!=Ship&&!Other->IsHidden()&&Other->Systems->Layout &&
            FVector::Distance(Other->GetActorLocation(),Location)<Radius+(Other->Systems->Layout->LocalHalfExtent*Other->GetActorScale3D()).Size();});
        if(!Clear)Location+=Home->GetActorUpVector()*(2*Radius+180);
    }
    if(!Clear){LastAction=TEXT("출구 주변 공간이 부족합니다");return false;}
    Ship->HangarHost.Reset();Bay->StoredSmallShip.Reset();
    Ship->SetActorLocationAndRotation(Location,Home->GetActorRotation());Ship->Velocity=Home->Velocity;
    Ship->SetActorHiddenInGame(false);Ship->SetActorEnableCollision(true);Ship->SetActorTickEnabled(true);
    Ship->StopMovement();Ships.AddUnique(Ship);
    if(!Combat->Ships.ContainsByPredicate([Ship](const auto& S){return S.Ship==Ship;})){
        Combat->RegisterShip(Ship);int32 PD=0;
        for(const auto& M:Ship->Systems->Layout->WeaponMounts)PD+=M.Type==EOVWeaponType::Interception;
        if(PD)PointDefense->RegisterShip(Ship,PD);
    }
    Transport->AddShip(Ship);Interior->AddShip(Ship);
    if(Ship->HullDefinitionId==TEXT("assault")){
        if(auto* S=Transport->Shuttles.FindByPredicate([Ship](const auto& V){return V.Ship==Ship;}))S->Home=Home;
        else Transport->RegisterShuttle(Ship,Home);
    }
    SelectShip(Ship);LastAction=TEXT("출격 · ")+Ship->ShipName;return true;
}

bool AOVController::TransferDockedCrew(AOVShip* Origin,AOVShip* Destination,const TArray<int32>& Units)
{
    if(!Available(Origin)||!Available(Destination))return false;
    FCrewOperation Op;Op.Origin=Origin;Op.Target=Destination;Op.Units=Units;Op.bTransfer=true;
    auto* Bay=Carrier->Hangars.FindByPredicate([&](const auto& H){return (H.Carrier==Origin&&H.StoredSmallShip==Destination)||(H.Carrier==Destination&&H.StoredSmallShip==Origin);});
    if(!Bay||!Carrier->IsHangarOperational(*Bay)){LastAction=TEXT("승하선에는 두 함선의 정박 연결이 필요합니다");return false;}
    Op.Room=Bay->Carrier==Origin?Bay->SlotId:Origin->Systems->Layout->CoreSlotId;
    return BeginCrewOperation(MoveTemp(Op));
}

bool AOVController::PrepareBoarding(AOVShip* Origin,AOVShip* Target,AOVShip* Shuttle,const TArray<int32>& Units)
{
    if(!Available(Origin)||!Available(Target)||Target->Systems->FactionId==0||Target->IsHidden()||Combat->IsDeparted(Target)){
        LastAction=TEXT("강습할 적 함선을 선택하세요");return false;
    }
    FCrewOperation Op;Op.Origin=Origin;Op.Target=Target;Op.Shuttle=Shuttle;Op.Units=Units;
    if(Shuttle){
        if(!Available(Shuttle)||Shuttle->HullDefinitionId!=TEXT("assault")||(Shuttle!=Origin && Shuttle->HangarHost!=Origin))return false;
        if(Origin==Shuttle)Op.Room=Origin->Systems->Layout->CoreSlotId;
        else if(auto* Bay=Carrier->Hangars.FindByPredicate([&](const auto& H){return H.StoredSmallShip==Shuttle&&Carrier->IsHangarOperational(H);}))Op.Room=Bay->SlotId;
    }else{
        const auto* Room=Origin->Systems->Layout->Slots.FindByPredicate([&](const auto& R){return R.Facility==EOVFacilityType::Boarding && ReadyRoom(Origin,R.SlotId);});
        if(Room)Op.Room=Room->SlotId;
    }
    if(Op.Room.IsNone()){LastAction=TEXT("가동할 승선 시설이 없습니다");return false;}
    return BeginCrewOperation(MoveTemp(Op));
}

bool AOVController::BeginCrewOperation(FCrewOperation Op)
{
    auto* Ship=Op.Origin.Get();
    if(!Available(Ship)||Ship->Systems->FactionId!=0||Ship->bFTLCharging||Op.Units.IsEmpty()||!ReadyRoom(Ship,Op.Room))return false;
    int32 HeroCount=0;TSet<int32> Seen;
    for(int32 Id:Op.Units){
        const auto U=Interior->GetUnit(Id);
        const auto P=Transport->GetPerson(U.PersonId);
        if(U.UnitId!=Id||U.Ship!=Ship||U.HP<=0||U.FactionId!=0||P.State!=EOVPassengerState::Aboard||P.CurrentShip!=Ship||
           Seen.Contains(Id)||CrewOperations.ContainsByPredicate([Id](const auto& Pending){return Pending.Units.Contains(Id);})){
            LastAction=TEXT("승선 가능한 아군 인원을 선택하세요 · 중복 명령 확인");return false;
        }
        Seen.Add(Id);HeroCount+=U.Role!=EOVInteriorRole::Trooper;
    }
    if(!Op.bTransfer && (HeroCount>4||Op.Units.Num()>12)){LastAction=TEXT("팀당 12명 · 영웅 최대 4명");return false;}
    if(Op.bTransfer){
        auto* To=Op.Target.Get();
        int32 Reserved=0,ReservedHeroes=0;
        for(const auto& Pending:CrewOperations)if(Pending.bTransfer && Pending.Target==To){
            Reserved+=Pending.Units.Num();for(int32 Id:Pending.Units)ReservedHeroes+=Interior->GetUnit(Id).Role!=EOVInteriorRole::Trooper;
        }
        const bool Assault=To->HullDefinitionId==TEXT("assault");
        const int32 Limit=Assault?Transport->Settings.ShuttleHeroCapacity:GetPlayScenario()->CurrentHeroBerths(To);
        if((HeroCount>0 && Interior->CountEmbarkedHeroes(To,0)+ReservedHeroes+HeroCount>Limit)||
           (Assault&&Interior->GetUnitsOnShip(To,0).Num()+Reserved+Op.Units.Num()>Transport->Settings.ShuttleCapacity)){
            LastAction=TEXT("도착 함선의 탑승 정원을 초과합니다");return false;
        }
        if(!Interior->AddShip(To))return false;
    }
    if(!Interior->IssueOrder(Op.Units,EOVInteriorOrder::Defend,Op.Room))return false;
    Op.Serial=NextCrewOperation++;CrewOperations.Add(MoveTemp(Op));
    LastAction=bSimulationPaused?TEXT("승선 지점 이동 대기 · 전투 재개 시 이동"):TEXT("승선 지점으로 이동");return true;
}

int32 AOVController::Mustered(const FCrewOperation& Op) const
{
    auto* Ship=Op.Origin.Get();if(!ReadyRoom(Ship,Op.Room))return 0;
    int32 N=0;
    const auto& Room=Ship->Systems->Layout->Slots[Ship->Systems->Layout->GetSlotIndex(Op.Room)];
    for(int32 Id:Op.Units){
        const auto U=Interior->GetUnit(Id);
        if(U.Ship==Ship && U.HP>0 && U.NodeId==Op.Room && U.Order==EOVInteriorOrder::Defend && U.TargetSlot==Op.Room &&
            FVector::Dist(U.LocalPosition,Room.LocalCenter)<40 && Transport->GetPerson(U.PersonId).State==EOVPassengerState::Aboard)++N;
    }
    return N;
}

void AOVController::CancelCrewOperation(int32 Serial)
{
    const int32 I=CrewOperations.IndexOfByPredicate([Serial](const auto& O){return O.Serial==Serial;});
    if(I==INDEX_NONE)return;
    const auto Op=CrewOperations[I];CrewOperations.RemoveAt(I);
    for(auto& U:Interior->Units)if(Op.Units.Contains(U.UnitId))U.bEmbarkedForDeparture=false;
    for(int32 Id:Op.Units){
        const auto U=Interior->GetUnit(Id);
        if(U.Ship==Op.Origin && U.Order==EOVInteriorOrder::Defend && U.TargetSlot==Op.Room)Interior->IssueOrder({Id},EOVInteriorOrder::Defend,U.NodeId);
    }
    LastAction=TEXT("승선 준비 취소");
}

bool AOVController::ConfirmBoarding(int32 Serial)
{
    const int32 I=CrewOperations.IndexOfByPredicate([Serial](const auto& O){return O.Serial==Serial;});
    if(I==INDEX_NONE)return false;
    const auto Op=CrewOperations[I];auto* Home=Op.Origin.Get();auto* Target=Op.Target.Get();auto* Shuttle=Op.Shuttle.Get();
    if(Op.bTransfer||!Available(Home)||!Available(Target)||Home->Systems->FactionId!=0||Target->Systems->FactionId==0||Target->IsHidden()||
       Combat->IsDeparted(Target)||Home->bFTLCharging||Combat->IsDeparted(Home)||Mustered(Op)!=Op.Units.Num()){
        LastAction=TEXT("대원 집결·표적·발사 시설을 확인하세요");return false;
    }
    if(!Shuttle && Home->HangarHost.IsValid()){LastAction=TEXT("정박 중인 함선에서 외부로 발사할 수 없습니다");return false;}
    auto People=Interior->GetPassengerIds(Op.Units);if(People.Num()!=Op.Units.Num())return false;
    TArray<int32> Assigned;
    auto Undo=[&](){for(int32 Id:Assigned)Interior->SetHeroBoardingAssignment(Id,false);};
    for(int32 Id:Op.Units){
        const auto U=Interior->GetUnit(Id);
        if(U.Role!=EOVInteriorRole::Trooper && !Transport->GetPerson(U.PersonId).bBoardingAssigned){
            if(!Interior->SetHeroBoardingAssignment(Id,true)){Undo();return false;}Assigned.Add(Id);
        }
    }
    if(Shuttle){
        auto* Bay=Carrier->Hangars.FindByPredicate([Shuttle](const auto& H){return H.StoredSmallShip==Shuttle;});
        if((Shuttle!=Home && (!Bay||Bay->Carrier!=Home)) || (Bay&&!Carrier->IsHangarOperational(*Bay))){Undo();return false;}
        if(!Transport->Shuttles.ContainsByPredicate([Shuttle](const auto& S){return S.Ship==Shuttle;}))Transport->RegisterShuttle(Shuttle,Bay?Bay->Carrier.Get():nullptr);
        if(!Transport->CanDispatchCrew(Shuttle,Target,People)){Undo();LastAction=Transport->LastMessage;return false;}
        if(Bay&&!LaunchSmallShip(Bay->HangarId)){Undo();return false;}
        if(!Transport->DispatchShuttle(Shuttle,Target,People)){Undo();LastAction=Transport->LastMessage;return false;}
    }else if(Transport->LaunchPodTeam(Home,Target,People)!=People.Num()){Undo();LastAction=Transport->LastMessage;return false;}
    auto* Play=GetPlayScenario();
    if(Play && Play->ActiveId==TEXT("HeroArena")){
        for(auto& Wave:Play->ArenaWaves)if(!Wave.Sent){Wave.Units=Op.Units;Wave.Sent=true;Wave.ActualShuttle=Shuttle;Wave.UsedShuttle=Shuttle!=nullptr;Wave.PlayerDirected=true;break;}
        Play->bArenaStarted=true;
    }
    for(auto& U:Interior->Units)if(Op.Units.Contains(U.UnitId))U.bEmbarkedForDeparture=false;
    CrewOperations.RemoveAt(I);LastAction=TEXT("강습팀 출발");return true;
}

void AOVController::ResetShipOperations()
{
    CrewOperations.Reset();
}

void AOVController::AdvanceShipOperations()
{
    if(!Carrier||!Interior||!Transport)return;
    for(auto& Bay:Carrier->Hangars){
        auto* Home=Bay.Carrier.Get();auto* Guest=Bay.StoredSmallShip.Get();
        if(Guest && Carrier->IsHangarDestroyed(Bay)){
            Guest->Systems->ForceWreckInstant();Bay.StoredSmallShip.Reset();Guest->HangarHost.Reset();
        }else if(Guest && Home){
            Guest->SetActorLocation(Home->GetActorLocation());
            if(Bay.StoredHostFaction!=Home->Systems->FactionId){
                Bay.StoredHostFaction=Home->Systems->FactionId;
                if(Guest->HullDefinitionId==TEXT("repair") || Guest->HullDefinitionId==TEXT("mining")){
                    Guest->Systems->FactionId=Bay.StoredHostFaction;
                    Guest->bFriendly=Bay.StoredHostFaction==0;
                }else{
                    Guest->Systems->bNeutral=true;
                    Guest->SetAutomation(false,false);
                }
            }
        }
        auto* In=Bay.InboundShip.Get();
        if(!In)continue;
        if(!Available(Home)||!Available(In)||Home->bFTLCharging||In->bFTLCharging||In->bPlayerHold||(In->bHasDestination && !In->Destination.Equals(Bay.ApproachDestination,1.))||!In->Systems->CanMove()||
           Home->Systems->FactionId!=In->Systems->FactionId||Combat->IsDeparted(In)||Combat->IsDeparted(Home)||!Carrier->IsHangarOperational(Bay)){
            Bay.InboundShip.Reset();continue;
        }
        const FVector Gate=ExitPoint(*Home,*In);
        if(FVector::Dist(In->GetActorLocation(),Gate)>FMath::Max(300.,In->Systems->Layout->LocalHalfExtent.Size()*.18)){
            if(!In->bHasDestination||FVector::DistSquared(In->Destination,Gate)>10000){Bay.ApproachDestination=Gate;In->SetDestination(Gate);}
            continue;
        }
        if(Carrier->StoreShuttle(Home,Bay.SlotId,In)){if(Selection.Contains(In))SelectShip(Home);LastAction=TEXT("수납 완료 · ")+In->ShipName;}
    }
    for(int32 I=CrewOperations.Num()-1;I>=0;--I){
        const auto Op=CrewOperations[I];auto* Origin=Op.Origin.Get();auto* To=Op.Target.Get();
        if(!Available(Origin)||!Available(To)||Origin->Systems->FactionId!=0||
           Op.Units.ContainsByPredicate([&](int32 Id){auto U=Interior->GetUnit(Id);return U.HP<=0||U.Ship!=Origin||U.TargetSlot!=Op.Room||U.Order!=EOVInteriorOrder::Defend;})){
            CancelCrewOperation(Op.Serial);continue;
        }
        if(!ReadyRoom(Origin,Op.Room) || (Op.Shuttle.IsStale()) || (!Op.bTransfer && (To->Systems->FactionId==0 || Combat->IsDeparted(To)))){
            CancelCrewOperation(Op.Serial);continue;
        }
        if(!Op.bTransfer){
            const auto& Room=Origin->Systems->Layout->Slots[Origin->Systems->Layout->GetSlotIndex(Op.Room)];
            for(auto& U:Interior->Units)if(Op.Units.Contains(U.UnitId) && U.NodeId==Op.Room && FVector::Dist(U.LocalPosition,Room.LocalCenter)<40)
                U.bEmbarkedForDeparture=true;
            continue;
        }
        const auto* Bay=Carrier->Hangars.FindByPredicate([&](const auto& H){return (H.Carrier==To&&H.StoredSmallShip==Origin)||(H.Carrier==Origin&&H.StoredSmallShip==To);});
        if(!Bay||!Carrier->IsHangarOperational(*Bay)){CancelCrewOperation(Op.Serial);continue;}
        if(Mustered(Op)==Op.Units.Num() && Transport->MoveDockedPassengers(Origin,To,Interior->GetPassengerIds(Op.Units))){
            for(auto& U:Interior->Units)if(Op.Units.Contains(U.UnitId)){
                U.Ship=To;U.NodeId=Bay->Carrier==To?Bay->SlotId:To->Systems->Layout->CoreSlotId;
                const auto& R=To->Systems->Layout->Slots[To->Systems->Layout->GetSlotIndex(U.NodeId)];
                U.LocalPosition=R.LocalCenter;U.TargetSlot=U.NodeId;U.LayoutVersion=To->Systems->Layout->LayoutVersion;
                U.Path.Reset();U.PlannedGoal=NAME_None;U.State=EOVInteriorState::Aboard;U.bWasInsideTarget=false;U.EntryShip.Reset();
            }
            CrewOperations.RemoveAt(I);LastAction=TEXT("승하선 완료 · ")+To->ShipName;
        }
    }
}
