#include "OVInteriorSubsystem.h"
#include "OVTransportSubsystem.h"
#include "OVShip.h"
#include "OVShipSystemsComponent.h"
#include "OVCombatSubsystem.h"
#include "OVHeroSubsystem.h"
#include "OVMissionSubsystem.h"
#include "OVLogisticsSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

FOVInteriorProfile::FOVInteriorProfile()
{
    auto Add=[this](EOVInteriorRole Role,float HP,float Damage,float Period,float Range,float Speed,float Armor,float Evasion)
    { FOVInteriorRoleProfile P;P.Role=Role;P.HP=HP;P.Damage=Damage;P.Period=Period;P.Range=Range;P.Speed=Speed;P.Armor=Armor;P.Evasion=Evasion;HeroRoles.Add(P); };
    // v0.5 candidate classes, metres converted once here to UE centimetres.
    Add(EOVInteriorRole::Melee,300,25,1,180,360,.18f,.08f);
    Add(EOVInteriorRole::Ranged,240,28,1.2f,1800,300,.10f,.12f);
    Add(EOVInteriorRole::Support,240,12,1.4f,1100,300,.08f,.10f);
    Add(EOVInteriorRole::Commander,280,22,1.25f,1000,310,.15f,.10f);
}

namespace OVInterior
{
constexpr float Step = 1.f / 60.f;
bool Living(const FOVInteriorUnitRecord& U) { return U.HP > 0 && U.State != EOVInteriorState::Dead; }
bool Hero(const FOVInteriorUnitRecord& U) { return U.Role != EOVInteriorRole::Trooper; }
int32 Rank(const FOVInteriorUnitRecord& U,int32 Index)
{ return U.HeroSkillRanks.IsValidIndex(Index)?U.HeroSkillRanks[Index]:(Index==0?1:0); }
float Distance(const FOVInteriorUnitRecord& A, const FOVInteriorUnitRecord& B)
{ return A.Ship.IsValid() ? ((A.LocalPosition-B.LocalPosition)*A.Ship->GetActorScale3D()).Size() : BIG_NUMBER; }
}

FOVInteriorUnitRecord* UOVInteriorSubsystem::FindUnit(int32 Id)
{ return Units.FindByPredicate([Id](const auto& U) { return U.UnitId == Id; }); }
UOVInteriorSubsystem::FGraphState* UOVInteriorSubsystem::FindGraph(AOVShip* Ship)
{ return Graphs.FindByPredicate([Ship](const auto& G) { return G.Ship == Ship; }); }
const UOVInteriorSubsystem::FGraphState* UOVInteriorSubsystem::FindGraph(AOVShip* Ship) const
{ return Graphs.FindByPredicate([Ship](const auto& G) { return G.Ship == Ship; }); }

void UOVInteriorSubsystem::Disable()
{
    PersistRoster(true);
    for (auto& G : Graphs) if (G.Ship.IsValid())
        for (int32 Faction : G.PresenceFactions) G.Ship->Systems->SetRuntimeInteriorPresence(Faction, false, false);
    Graphs.Reset(); bEnabled = false; Accumulator = 0;
}

void UOVInteriorSubsystem::Deinitialize()
{
    Disable();
    Super::Deinitialize();
}

void UOVInteriorSubsystem::PersistRoster(bool bDetach)
{
    auto* GI=GetWorld()?GetWorld()->GetGameInstance():nullptr;
    auto* Roster=GI?GI->GetSubsystem<UOVHeroSubsystem>():nullptr;
    if(!Roster)return;
    for(auto& U:Units)
    {
        if(!U.HeroId.IsValid())continue;
        auto* H=Roster->FindHero(U.HeroId);
        if(!H || H->RuntimeUnitId!=U.UnitId || H->RuntimePersonId!=U.PersonId)continue;
        H->CurrentHP=U.HP;H->bWounded=U.bWounded;H->bSkill4Armed=U.bSkill4Armed;H->Catalysts=U.Medkits;H->AmplifierCharges=U.AmplifierCharges;
        if(H->CooldownRemaining.Num()==4){H->CooldownRemaining[0]=FMath::Max(0.0,U.SkillReadyAt-GameTime);H->CooldownRemaining[2]=FMath::Max(0.0,U.Skill3ReadyAt-GameTime);H->CooldownRemaining[3]=FMath::Max(0.0,U.Skill4ReadyAt-GameTime);}
        if(U.State==EOVInteriorState::Dead){H->Status=EOVHeroStatus::Dead;H->bDeploymentReserved=false;}
        else if(U.State==EOVInteriorState::RescueWaiting)H->Status=EOVHeroStatus::RescueWaiting;
        if(bDetach)
        {
            if(U.Ship.IsValid())if(auto* Logistics=GetWorld()->GetSubsystem<UOVLogisticsSubsystem>())Logistics->CancelDevRepair(U.Ship.Get(),U.HeroId);
            const auto* Mission=GetWorld()->GetSubsystem<UOVMissionSubsystem>();
            const bool bUnfinished=Mission && Mission->bEnabled && Mission->GameTime>0 && Mission->Result==EOVMissionResult::Ongoing;
            if(H->Status==EOVHeroStatus::Deployed && (bUnfinished || !U.Ship.IsValid() ||
                U.State==EOVInteriorState::InTransit || U.State==EOVInteriorState::AwaitingRecovery ||
                U.Ship->Systems->FactionId!=U.FactionId)) H->Status=EOVHeroStatus::Missing;
            H->RuntimeUnitId=0;H->RuntimePersonId=0;
        }
    }
}

int32 UOVInteriorSubsystem::SpawnRosterHero(AOVShip* Ship,FGuid HeroId,FName SlotId)
{
    auto* GI=GetWorld()?GetWorld()->GetGameInstance():nullptr;
    auto* Roster=GI?GI->GetSubsystem<UOVHeroSubsystem>():nullptr;
    auto* H=Roster?Roster->FindHero(HeroId):nullptr;
    if(!H || !H->bDeploymentReserved || H->Status!=EOVHeroStatus::Deployed || H->RuntimeUnitId!=0 || H->RuntimePersonId!=0)
    {LastMessage=TEXT("영웅 배치 불가: 편제, 상태 또는 기존 실행 연결 확인");return 0;}
    const int32 Id=RegisterUnit(Ship,H->FactionId,H->Role,SlotId);
    auto* U=FindUnit(Id);if(!U)return 0;
    U->HeroId=HeroId;U->Name=H->Name;U->HeroSkillRanks=H->SkillRanks;U->HeroLevel=H->Level;U->AmplifierCharges=H->AmplifierCharges;U->DevRepairBaseCost=H->DevRepairBaseCost;
    U->MaxHP*=1+.06f*(H->Level-1);U->Damage*=1+.08f*(H->Level-1);
    U->bWounded=H->bWounded;U->bSkill4Armed=H->bSkill4Armed;if(U->bWounded)U->MaxHP*=.9f;
    U->HP=H->CurrentHP<0?U->MaxHP:FMath::Clamp(H->CurrentHP,0.f,U->MaxHP);
    U->Medkits=H->Catalysts;
    if(H->CooldownRemaining.Num()==4){U->SkillReadyAt=GameTime+H->CooldownRemaining[0];U->Skill3ReadyAt=GameTime+H->CooldownRemaining[2];U->Skill4ReadyAt=GameTime+H->CooldownRemaining[3];}
    H->RuntimeUnitId=U->UnitId;H->RuntimePersonId=U->PersonId;
    return Id;
}

bool UOVInteriorSubsystem::SetHeroDuty(int32 UnitId,bool bEnable,FName SlotId)
{
    auto* U=FindUnit(UnitId);
    if(!U || !OVInterior::Hero(*U) || !IsOnboard(*U) || U->State==EOVInteriorState::AwaitingRecovery)
    {LastMessage=TEXT("보직 변경 불가: 탑승한 생존 영웅이 필요합니다");return false;}
    if(!bEnable){if(U->HeroId.IsValid())GetWorld()->GetSubsystem<UOVLogisticsSubsystem>()->CancelDevRepair(U->Ship.Get(),U->HeroId);U->Skill3ActiveUntil=0;U->bDutyRequested=U->bDutyActive=false;U->DutySlot=NAME_None;U->Order=EOVInteriorOrder::Defend;U->TargetSlot=U->NodeId;return true;}
    const auto* Layout=U->Ship->Systems->Layout.Get();const int32 Index=Layout?Layout->GetSlotIndex(SlotId):INDEX_NONE;
    if(Index==INDEX_NONE || U->Ship->Systems->FactionId!=U->FactionId)
    {LastMessage=TEXT("아군 함선의 유효한 보직 시설이 필요합니다");return false;}
    const EOVFacilityType Type=Layout->Slots[Index].Facility;
    if((U->Role==EOVInteriorRole::Ranged && Type!=EOVFacilityType::WeaponControl) ||
       (U->Role==EOVInteriorRole::Commander && Type!=EOVFacilityType::Bridge) || Type==EOVFacilityType::None)
    {LastMessage=TEXT("보직에 맞는 시설을 선택하세요");return false;}
    const auto* Combat=GetWorld()->GetSubsystem<UOVCombatSubsystem>();
    const auto* ShipState=Combat?Combat->Ships.FindByPredicate([&](const FOVCombatShipState& S){return S.Ship.Get()==U->Ship.Get();}):nullptr;
    const int32 Limit=ShipState?FMath::Clamp(ShipState->WeaponTierCap,1,4):(Layout->bSmallHull?1:Layout->DeckCount<3?2:3);
    int32 Count=0;for(const auto& Other:Units)if(Other.UnitId!=UnitId && Other.Ship==U->Ship && Other.bDutyRequested && OVInterior::Living(Other))++Count;
    if(Count>=Limit){LastMessage=TEXT("함선 체급별 동시 보직 한도 초과");return false;}
    U->Skill3ActiveUntil=0;U->bRushAttack=false;U->BurstRemaining=0;
    U->bDutyRequested=true;U->bDutyActive=false;U->DutySlot=SlotId;U->DutyReadyAt=0;
    U->Order=EOVInteriorOrder::None;U->Path.Reset();U->PlannedGoal=NAME_None;U->EnemyId=0;
    LastMessage=TEXT("보직 시설 이동 및 인계 준비");return true;
}

FOVHeroDutyBonuses UOVInteriorSubsystem::GetDutyBonuses(AOVShip* Ship) const
{
    FOVHeroDutyBonuses B;
    if(!IsValid(Ship) || !Ship->Systems || Ship->Systems->bWrecked || GetWorld()->GetSubsystem<UOVCombatSubsystem>()->IsDeparted(Ship))return B;
    for(const auto& U:Units)
    {
        if(!U.bDutyActive || U.Ship!=Ship || U.FactionId!=Ship->Systems->FactionId || !IsOnboard(U))continue;
        const auto State=Ship->Systems->GetSlotState(U.DutySlot);
        if(State.HP<=0 || State.Crew<=0 || (State.OccupyingFaction>=0 && State.OccupyingFaction!=U.FactionId))continue;
        const int32 Rank=OVInterior::Rank(U,0), SkillRank=OVInterior::Rank(U,2);
        if(U.Role==EOVInteriorRole::Ranged)
        {
            B.AccuracyBonus=FMath::Max(B.AccuracyBonus,.02f+.01f*(Rank-1));
            if(GameTime<U.Skill3ActiveUntil && SkillRank>0)B.FireRateBonus=FMath::Max(B.FireRateBonus,.15f+.05f*(SkillRank-1));
        }
        if(U.Role==EOVInteriorRole::Commander)
        {
            const float Base=.05f+.02f*(Rank-1);B.SpeedBonus=FMath::Max(B.SpeedBonus,Base);
            const float Turn=(GameTime<U.Skill3ActiveUntil && SkillRank>0)?(.10f+.05f*(SkillRank-1)):Base;
            B.TurnBonus=FMath::Max(B.TurnBonus,Turn);
        }
    }
    B.AccuracyBonus=FMath::Min(.08f,B.AccuracyBonus);B.FireRateBonus=FMath::Min(.30f,B.FireRateBonus);
    B.SpeedBonus=FMath::Min(.20f,B.SpeedBonus);B.TurnBonus=FMath::Min(.20f,B.TurnBonus);return B;
}

bool UOVInteriorSubsystem::ValidateInterior(AOVShip* Ship, TArray<FString>& Errors) const
{
    Errors.Reset();
    if (!IsValid(Ship) || !Ship->Systems || !Ship->Systems->Layout) { Errors.Add(TEXT("내부 배치가 없습니다")); return false; }
    Ship->Systems->Layout->ValidateLayout(Errors);
    if (Ship->Systems->Layout->LayoutVersion.IsNone()) Errors.Add(TEXT("내부 배치 버전이 없습니다"));
    for (const auto& C : Ship->Systems->Layout->Connections)
    {
        const auto* L = Ship->Systems->Layout.Get();
        const int32 A=L->GetSlotIndex(C.FromSlot), B=L->GetSlotIndex(C.ToSlot);
        if (A!=INDEX_NONE && B!=INDEX_NONE && ((L->Slots[A].LocalCenter-L->Slots[B].LocalCenter)*Ship->GetActorScale3D()).Size() < 1)
            Errors.Add(TEXT("길이가 없는 통로입니다"));
    }
    return Errors.IsEmpty();
}

void UOVInteriorSubsystem::ResetScenario(const TArray<AOVShip*>& Ships, int32 Seed)
{
    Disable(); Units.Reset(); Operations.Reset(); Events.Reset();
    GameTime = 0; TotalDeaths = TotalCaptures = TotalSabotages = 0; Random.Initialize(Seed);
    // ID counters intentionally survive resets, preventing old selection/event IDs from addressing a new person.
    for (AOVShip* Ship : Ships) AddShip(Ship);
    bEnabled = !Graphs.IsEmpty(); LastMessage=bEnabled?TEXT("실제 선내 인원·경로 시험 준비"):TEXT("유효한 내부 배치 없음");
}

bool UOVInteriorSubsystem::AddShip(AOVShip* Ship)
{
    if(FindGraph(Ship))return true;
    TArray<FString> Errors;
    if(!ValidateInterior(Ship,Errors)){LastMessage=FString::Join(Errors,TEXT(" / "));return false;}
    FGraphState G;G.Ship=Ship;G.Version=Ship->Systems->Layout->LayoutVersion;G.KnownBy.Add(Ship->Systems->FactionId);
    for(const auto& S:Ship->Systems->Slots)G.PreviousHP.Add(S.SlotId,S.HP);
    for(const auto& C:Ship->Systems->Layout->Connections)
    {G.Neighbors.FindOrAdd(C.FromSlot).Add(C.ToSlot);G.Neighbors.FindOrAdd(C.ToSlot).Add(C.FromSlot);}
    Graphs.Add(MoveTemp(G));bEnabled=true;return true;
}

bool UOVInteriorSubsystem::RemoveShip(AOVShip* Ship)
{
    auto* G=FindGraph(Ship);if(!G)return false;
    for(const auto& U:Units)if(U.Ship==Ship && U.State!=EOVInteriorState::Dead)
    {LastMessage=TEXT("생존 인원이 있는 함선은 선내 그래프를 제거할 수 없습니다");return false;}
    for(int32 F:G->PresenceFactions)if(IsValid(Ship))Ship->Systems->SetRuntimeInteriorPresence(F,false,false);
    Graphs.RemoveAll([Ship](const FGraphState& Row){return Row.Ship==Ship;});
    Operations.RemoveAll([Ship](const FOVInteriorOperation& O){return O.Ship==Ship;});
    bEnabled=!Graphs.IsEmpty();return true;
}

int32 UOVInteriorSubsystem::CountActiveHeroes(int32 Faction) const
{
    int32 Count=0; for (const auto& U:Units) if(U.FactionId==Faction && OVInterior::Hero(U) && U.State!=EOVInteriorState::Dead) ++Count;
    return Count;
}

int32 UOVInteriorSubsystem::RegisterUnit(AOVShip* Ship,int32 Faction,EOVInteriorRole Role,FName SlotId)
{
    if(!bEnabled || !FindGraph(Ship) || Faction<0 || Ship->Systems->bWrecked ||
        (Role!=EOVInteriorRole::Trooper && CountActiveHeroes(Faction)>=FMath::Min(15,Settings.MaxActiveHeroesPerFaction)))
    { LastMessage=TEXT("인원 등록 불가: 배치·생존·세력 영웅 15명 한도 확인"); return 0; }
    auto* L=Ship->Systems->Layout.Get();
    if(SlotId.IsNone()) SlotId=L->CoreSlotId;
    const int32 Index=L->GetSlotIndex(SlotId); if(Index==INDEX_NONE) return 0;
    FOVInteriorUnitRecord U; U.UnitId=NextUnitId++; U.FactionId=Faction; U.Role=Role;
    U.Ship=Ship; U.NodeId=SlotId; U.LayoutVersion=L->LayoutVersion; U.LocalPosition=L->Slots[Index].LocalCenter;
    U.Order=EOVInteriorOrder::Defend; U.TargetSlot=SlotId; U.State=EOVInteriorState::Defending;
    if(Role!=EOVInteriorRole::Trooper)
    {
        const auto* P=Settings.HeroRoles.FindByPredicate([Role](const auto& Row){return Row.Role==Role;});
        if(!P||P->HP<=0||P->Speed<=0||P->Period<=0||!FMath::IsFinite(P->HP)||!FMath::IsFinite(P->Speed))return 0;
        U.MaxHP=P->HP;U.Damage=P->Damage;U.Period=P->Period;U.Range=P->Range;U.Speed=P->Speed;U.Armor=P->Armor;U.Evasion=P->Evasion;
        U.Name=Role==EOVInteriorRole::Melee?TEXT("역장 돌격 영웅"):Role==EOVInteriorRole::Ranged?TEXT("초감각 사격 영웅"):Role==EOVInteriorRole::Support?TEXT("생체 재구성 영웅"):TEXT("신경 공명 영웅");
        if(Role==EOVInteriorRole::Support)U.Medkits=FMath::Max(0,Settings.InitialSupportCatalysts);
    }
    else {U.Name=TEXT("일반 강습·방어 병력");U.MaxHP=Settings.TrooperHP;U.Damage=Settings.TrooperDamage;U.Period=Settings.TrooperPeriod;U.Range=Settings.TrooperRange;U.Speed=Settings.TrooperSpeed;U.Armor=.05f;U.Evasion=.05f;}
    U.HP=U.MaxHP;
    U.PersonId=GetWorld()->GetSubsystem<UOVTransportSubsystem>()->RegisterRuntimePassenger(Ship,U.UnitId,Faction,OVInterior::Hero(U));
    if(U.PersonId==0) { LastMessage=TEXT("운송 시스템을 먼저 준비해야 합니다");return 0; }
    Units.Add(U); RefreshPresence(); Emit(TEXT("Registered"),U,TEXT("실제 인원 배치")); return U.UnitId;
}

TArray<int32> UOVInteriorSubsystem::RegisterBoardingTeam(AOVShip* Origin,int32 Faction,int32 HeroCount,int32 Troops)
{
    TArray<int32> Ids;
    if(HeroCount<0 || HeroCount>4 || Troops<0 || HeroCount+Troops>12 || CountActiveHeroes(Faction)+HeroCount>15)
    { LastMessage=TEXT("강습 편제 초과: 총 12명·영웅 4명·세력 전체 15명"); return Ids; }
    for(int32 I=0;I<HeroCount+Troops;++I)
    {
        const auto Role=I<HeroCount?static_cast<EOVInteriorRole>(1+I%4):EOVInteriorRole::Trooper;
        const int32 Id=RegisterUnit(Origin,Faction,Role); if(Id>0)
        { Ids.Add(Id); GetWorld()->GetSubsystem<UOVTransportSubsystem>()->SetRuntimeBoardingAssignment(FindUnit(Id)->PersonId,Id,true); }
    }
    return Ids;
}

bool UOVInteriorSubsystem::IsOnboard(const FOVInteriorUnitRecord& U) const
{
    return OVInterior::Living(U) && U.Ship.IsValid() && U.State!=EOVInteriorState::InTransit &&
        U.State!=EOVInteriorState::RescueWaiting && !U.Ship->Systems->bWrecked;
}

bool UOVInteriorSubsystem::PassageOpen(const FGraphState& G,FName A,FName B) const
{
    if(!G.Ship.IsValid())return false;
    for(const auto& C:G.Ship->Systems->Layout->Connections)
        if((C.FromSlot==A&&C.ToSlot==B)||(C.FromSlot==B&&C.ToSlot==A)) return !G.BlockedConnections.Contains(C.ConnectionId);
    return false;
}

bool UOVInteriorSubsystem::SetConnectionBlocked(AOVShip* Ship,FName ConnectionId,bool bBlocked)
{
    FGraphState* G=FindGraph(Ship); if(!G || !Ship->Systems->Layout->Connections.ContainsByPredicate([ConnectionId](const auto& C){return C.ConnectionId==ConnectionId;}))return false;
    if(bBlocked)G->BlockedConnections.Add(ConnectionId);else G->BlockedConnections.Remove(ConnectionId);
    for(auto& U:Units)if(U.Ship==Ship){U.Path.Reset();U.PlannedGoal=NAME_None;}
    return true;
}

bool UOVInteriorSubsystem::PlanPath(FOVInteriorUnitRecord& U,FName Goal)
{
    FGraphState* G=FindGraph(U.Ship.Get()); if(!G)return false;
    const auto* L=U.Ship->Systems->Layout.Get();
    if(U.LayoutVersion!=G->Version || L->LayoutVersion!=G->Version) { U.Reason=TEXT("내부 배치 버전 변경: 재배치 검증 필요"); return false; }
    if(L->GetSlotIndex(Goal)==INDEX_NONE || L->GetSlotIndex(U.NodeId)==INDEX_NONE) {U.Reason=TEXT("목표 또는 현재 방이 사라졌습니다");return false;}
    TMap<FName,float> Dist;TMap<FName,FName> Previous;TSet<FName> Visited;
    Dist.Add(U.NodeId,0);
    for(int32 Iter=0;Iter<L->Slots.Num();++Iter)
    {
        FName Current;float Best=BIG_NUMBER;
        for(const auto& S:L->Slots)if(!Visited.Contains(S.SlotId))if(const float* D=Dist.Find(S.SlotId))if(*D<Best){Best=*D;Current=S.SlotId;}
        if(Current.IsNone())break;
        if(Current==Goal)break;
        Visited.Add(Current);const int32 A=L->GetSlotIndex(Current);
        if(const auto* Next=G->Neighbors.Find(Current))for(FName N:*Next)
        {
            if(!PassageOpen(*G,Current,N))continue;const int32 B=L->GetSlotIndex(N);
            const float Cost=Best+((L->Slots[A].LocalCenter-L->Slots[B].LocalCenter)*U.Ship->GetActorScale3D()).Size();
            if(!Dist.Contains(N)||Cost<Dist[N]){Dist.Add(N,Cost);Previous.Add(N,Current);}
        }
    }
    if(!Dist.Contains(Goal)){U.Reason=TEXT("통로 차단: 목표까지 연결된 경로 없음");return false;}
    U.Path.Reset();FName Cursor=Goal;U.Path.Add(Cursor);
    while(Cursor!=U.NodeId){const FName* P=Previous.Find(Cursor);if(!P)return false;Cursor=*P;U.Path.Insert(Cursor,0);}
    U.PathIndex=0; U.PlannedGoal=Goal; return true;
}

bool UOVInteriorSubsystem::MovePoint(FOVInteriorUnitRecord& U,FVector Goal,float Delta)
{
    const FVector Scale=U.Ship->GetActorScale3D();const FVector WorldDelta=(Goal-U.LocalPosition)*Scale;
    const float Distance=WorldDelta.Size();
    if(Distance<=Settings.ArrivalTolerance) {U.LocalPosition=Goal;return true;}
    float Bonus=0;
    if(U.Order==EOVInteriorOrder::Retreat || U.Order==EOVInteriorOrder::Defend)
        for(const auto& Friend:Units)if(!Friend.bDutyRequested && Friend.Role==EOVInteriorRole::Commander && Friend.FactionId==U.FactionId && Friend.Ship==U.Ship && Friend.NodeId==U.NodeId && IsOnboard(Friend) && OVInterior::Rank(Friend,3)>0)
            Bonus=FMath::Max(Bonus,.05f+.025f*(OVInterior::Rank(Friend,3)-1));
    float Speed=U.Speed*(1+FMath::Min(.20f,Bonus));
    if(U.Role==EOVInteriorRole::Melee && GameTime<U.Skill3ActiveUntil)Speed*=1.5f;
    if(GameTime<U.SlowUntil)Speed*=1-FMath::Clamp(U.SlowFraction,0.f,1.f);
    const float Travel=FMath::Min(Distance,Speed*Delta);
    U.LocalPosition+=(Goal-U.LocalPosition)*(Travel/Distance);U.DistanceTravelled+=Travel;
    return Travel>=Distance;
}

bool UOVInteriorSubsystem::MoveTowards(FOVInteriorUnitRecord& U,FName Goal,float Delta)
{
    if(U.PlannedGoal!=Goal||U.Path.IsEmpty())if(!PlanPath(U,Goal)){U.State=EOVInteriorState::Blocked;return false;}
    const auto* L=U.Ship->Systems->Layout.Get();
    if(U.PathIndex>=U.Path.Num())return true;
    const FName Next=U.Path[U.PathIndex];const int32 Index=L->GetSlotIndex(Next);
    if(Index==INDEX_NONE){U.Path.Reset();U.Reason=TEXT("이동 중 목표 방 소멸");return false;}
    if(Next!=U.NodeId && !PassageOpen(*FindGraph(U.Ship.Get()),U.NodeId,Next)){U.Path.Reset();U.Reason=TEXT("이동 중 통로 차단");return false;}
    if(MovePoint(U,L->Slots[Index].LocalCenter,Delta)){U.NodeId=Next;++U.PathIndex;}
    return U.PathIndex>=U.Path.Num();
}

bool UOVInteriorSubsystem::IssueOrder(const TArray<int32>& Ids,EOVInteriorOrder Order,FName Target)
{
    bool Accepted=false;
    for(int32 Id:Ids)if(auto* U=FindUnit(Id))
    {
        if(!OVInterior::Living(*U)||U->State==EOVInteriorState::RescueWaiting)continue;
        if(Order!=EOVInteriorOrder::Retreat && Order!=EOVInteriorOrder::None && IsOnboard(*U) && U->Ship->Systems->Layout->GetSlotIndex(Target)==INDEX_NONE)
        {U->Reason=TEXT("지정한 시설·방이 없습니다");continue;}
        if(U->bDutyRequested){U->Skill3ActiveUntil=0;if(U->HeroId.IsValid() && U->Ship.IsValid())GetWorld()->GetSubsystem<UOVLogisticsSubsystem>()->CancelDevRepair(U->Ship.Get(),U->HeroId);}
        U->bDutyRequested=U->bDutyActive=false;U->DutySlot=NAME_None;
        U->Order=Order;U->TargetSlot=Target;U->Progress=0;U->Path.Reset();U->PlannedGoal=NAME_None;U->EnemyId=0;
        if(Order==EOVInteriorOrder::Capture || Order==EOVInteriorOrder::Sabotage)
            if(auto* G=FindGraph(U->Ship.Get()))G->CompletedBy.Remove(U->FactionId);
        if(Order!=EOVInteriorOrder::Retreat && U->State==EOVInteriorState::AwaitingRecovery && U->bWasInsideTarget && U->EntryShip==U->Ship)
            if(GetWorld()->GetSubsystem<UOVTransportSubsystem>()->CancelRuntimeRecovery(U->PersonId,U->UnitId,U->Ship.Get()))U->State=EOVInteriorState::Defending;
        U->Reason=Order==EOVInteriorOrder::Retreat?TEXT("원 진입점으로 실제 철수"):TEXT("선내 명령 접수");Accepted=true;
        if(Order==EOVInteriorOrder::Retreat)
        {
            auto* T=GetWorld()->GetSubsystem<UOVTransportSubsystem>();const auto P=T->GetPerson(U->PersonId);
            if(P.State==EOVPassengerState::InPod)if(const auto* Pod=T->Pods.FindByPredicate([&](const auto& Flight){return Flight.PersonId==P.PersonId;}))T->RecallPods(Pod->Origin.Get());
            if(P.Shuttle.IsValid())T->RecallShuttle(P.Shuttle.Get());
        }
    }
    RefreshPresence();LastMessage=Accepted?TEXT("선내 목표 명령 접수"):TEXT("선내 명령 불가: 생존·목표·경로 확인");return Accepted;
}

bool UOVInteriorSubsystem::Visible(const FOVInteriorUnitRecord& A,const FOVInteriorUnitRecord& B,float Range) const
{
    if(!IsOnboard(A)||!IsOnboard(B)||A.Ship!=B.Ship||OVInterior::Distance(A,B)>Range)return false;
    if(A.NodeId==B.NodeId)return true;
    const FGraphState* G=FindGraph(A.Ship.Get());return G && PassageOpen(*G,A.NodeId,B.NodeId);
}

bool UOVInteriorSubsystem::CanObserve(int32 Faction,int32 Id) const
{
    const auto* Target=Units.FindByPredicate([Id](const auto& U){return U.UnitId==Id;});if(!Target)return false;
    if(Target->FactionId==Faction)return true;
    for(const auto& U:Units)if(U.FactionId==Faction && Visible(U,*Target,Settings.SightRange))return true;
    return false;
}

bool UOVInteriorSubsystem::IsStructureKnown(int32 Faction,AOVShip* Ship) const
{const auto* G=FindGraph(Ship);return G && G->KnownBy.Contains(Faction);}

void UOVInteriorSubsystem::Emit(FName Kind,const FOVInteriorUnitRecord& U,const FString& Message)
{
    FOVInteriorEvent E;E.Serial=NextEventSerial++;E.Time=GameTime;E.UnitId=U.UnitId;E.FactionId=U.FactionId;E.Kind=Kind;E.Ship=U.Ship;E.SlotId=U.NodeId;E.Message=Message;
    Events.Add(E);if(Events.Num()>256)Events.RemoveAt(0,Events.Num()-256,EAllowShrinking::No);LastMessage=Message;
}

void UOVInteriorSubsystem::Kill(FOVInteriorUnitRecord& U,const FString& Reason)
{
    if(U.State==EOVInteriorState::Dead)return;
    if(U.HeroId.IsValid() && U.Ship.IsValid())GetWorld()->GetSubsystem<UOVLogisticsSubsystem>()->CancelDevRepair(U.Ship.Get(),U.HeroId);
    U.HP=0;U.State=EOVInteriorState::Dead;U.Order=EOVInteriorOrder::None;U.Path.Reset();U.Reason=Reason;
    ++U.DeathEvents;++TotalDeaths;
    GetWorld()->GetSubsystem<UOVTransportSubsystem>()->MarkRuntimePersonDead(U.PersonId,U.UnitId);
    PersistRoster(false);
    Emit(TEXT("Death"),U,Reason);
}

void UOVInteriorSubsystem::ApplyDamage(FOVInteriorUnitRecord& U,float Amount,const FString& Reason)
{
    if(!OVInterior::Living(U)||Amount<=0)return;
    U.HP=FMath::Max(0.f,U.HP-Amount);U.LastDamagedAt=GameTime;
    if(U.HP>0 && U.Role==EOVInteriorRole::Melee && OVInterior::Rank(U,3)>0 && U.HP<U.MaxHP*.25f && U.bSkill4Armed && GameTime>=U.Skill4ReadyAt)
    {U.Skill4ActiveUntil=GameTime+3;U.Skill4ReadyAt=GameTime+120;U.bSkill4Armed=false;++U.SkillUses;}
    if(U.HP<U.MaxHP*.25f)U.bWasCriticallyInjured=true;
    if(U.HP<=0)Kill(U,Reason);
}

void UOVInteriorSubsystem::SyncTransport(FOVInteriorUnitRecord& U)
{
    if(U.State==EOVInteriorState::Dead)return;
    const auto P=GetWorld()->GetSubsystem<UOVTransportSubsystem>()->GetPerson(U.PersonId);
    if(P.PersonId==0 || P.RuntimeUnitId!=U.UnitId){U.State=EOVInteriorState::Blocked;U.Reason=TEXT("운송 인원 ID 연결 상실");return;}
    if(P.State==EOVPassengerState::Dead){Kill(U,TEXT("운송 또는 내부 함선 완파로 사망"));return;}
    if(P.State==EOVPassengerState::RescueWaiting){U.State=EOVInteriorState::RescueWaiting;U.Ship=nullptr;U.Reason=TEXT("우주 구조 대기 · 구조 후속 기능 미구현");PersistRoster(false);return;}
    if(P.State==EOVPassengerState::InPod || P.State==EOVPassengerState::InShuttle)
    {U.State=EOVInteriorState::InTransit;U.Ship=P.CurrentShip;U.Reason=TEXT("실제 운송 중");return;}
    if(P.State==EOVPassengerState::InsideTarget)
    {
        if(!U.bWasInsideTarget||U.EntryShip!=P.CurrentShip)
        {
            U.Ship=P.CurrentShip;U.NodeId=P.EntrySlot;U.LocalPosition=P.EntryLocalPoint;U.LayoutVersion=P.EntryLayoutVersion;
            U.EntryShip=P.CurrentShip;U.EntrySlot=P.EntrySlot;U.EntryLocalPoint=P.EntryLocalPoint;U.bWasInsideTarget=true;
            U.Path.Reset();U.PlannedGoal=NAME_None;
            if(U.Order==EOVInteriorOrder::Defend||U.Order==EOVInteriorOrder::None)U.TargetSlot=U.NodeId;
            if(auto* G=FindGraph(U.Ship.Get()))G->KnownBy.Add(U.FactionId);
            Emit(TEXT("Entry"),U,TEXT("실제 진입: 원 운송수단과 진입점 기록"));
        }
        if(U.State==EOVInteriorState::InTransit || U.State==EOVInteriorState::Aboard)U.State=EOVInteriorState::Defending;
    }
    else if(P.State==EOVPassengerState::AwaitingRecovery)
    {U.Ship=P.CurrentShip;U.State=EOVInteriorState::AwaitingRecovery;U.Reason=TEXT("원 강습선 회수 대기");}
    else if(P.State==EOVPassengerState::Aboard && (U.Ship!=P.CurrentShip||U.State==EOVInteriorState::InTransit||U.State==EOVInteriorState::AwaitingRecovery))
    {
        U.Ship=P.CurrentShip;U.bWasInsideTarget=false;U.EntryShip=nullptr;U.Path.Reset();U.PlannedGoal=NAME_None;
        if(U.Ship.IsValid()) {U.NodeId=U.Ship->Systems->Layout->CoreSlotId;U.LocalPosition=U.Ship->Systems->Layout->Slots[U.Ship->Systems->Layout->GetSlotIndex(U.NodeId)].LocalCenter;U.LayoutVersion=U.Ship->Systems->Layout->LayoutVersion;}
        U.Order=EOVInteriorOrder::Defend;U.TargetSlot=U.NodeId;U.State=EOVInteriorState::Aboard;
        if(U.bWasCriticallyInjured && OVInterior::Hero(U) && !U.bWounded)
        {U.bWounded=true;U.MaxHP*=.9f;U.HP=FMath::Min(U.HP,U.MaxHP);}
        U.Reason=TEXT("생존 귀환 · HP/촉매/쿨타임 보존");Emit(TEXT("Return"),U,U.Reason);
    }
}

void UOVInteriorSubsystem::AdvanceUnit(FOVInteriorUnitRecord& U,float Delta)
{
    if(!IsOnboard(U)||U.State==EOVInteriorState::AwaitingRecovery)return;
    if(GetWorld()->GetSubsystem<UOVCombatSubsystem>()->IsDeparted(U.Ship.Get()))
    {if(U.HeroId.IsValid())GetWorld()->GetSubsystem<UOVLogisticsSubsystem>()->CancelDevRepair(U.Ship.Get(),U.HeroId);U.bDutyActive=false;U.State=EOVInteriorState::Blocked;U.Reason=TEXT("목표 초광속 이탈 · 인원 보존, 성계 밖 작전/회수 미지원");return;}
    const auto* L=U.Ship->Systems->Layout.Get();
    if(U.LayoutVersion!=L->LayoutVersion){U.State=EOVInteriorState::Blocked;U.Reason=TEXT("배치 버전 불일치: 이동 중지");return;}
    if(U.HP>=U.MaxHP*.4f)U.bSkill4Armed=true;
    if(U.bDutyRequested)
    {
        const auto Facility=U.Ship->Systems->GetSlotState(U.DutySlot);
        if(U.Ship->Systems->FactionId!=U.FactionId || Facility.HP<=0 || Facility.Crew<=0 || (Facility.OccupyingFaction>=0 && Facility.OccupyingFaction!=U.FactionId))
        {if(U.HeroId.IsValid())GetWorld()->GetSubsystem<UOVLogisticsSubsystem>()->CancelDevRepair(U.Ship.Get(),U.HeroId);U.bDutyActive=false;U.DutyReadyAt=0;U.State=EOVInteriorState::Blocked;U.Reason=TEXT("보직 시설 가동 조건 미충족");return;}
        if(!MoveTowards(U,U.DutySlot,Delta)){U.bDutyActive=false;U.DutyReadyAt=0;U.State=EOVInteriorState::Moving;return;}
        if(U.DutyReadyAt<=0)U.DutyReadyAt=GameTime+3;
        if(GameTime<U.DutyReadyAt){U.State=EOVInteriorState::Operating;U.Reason=TEXT("보직 인계 중");return;}
        U.bDutyActive=true;U.State=EOVInteriorState::Operating;U.Reason=TEXT("보직 수행 중");
        if((U.Role==EOVInteriorRole::Ranged || U.Role==EOVInteriorRole::Commander) && OVInterior::Rank(U,2)>0 && GameTime>=U.Skill3ReadyAt)
        {U.Skill3ReadyAt=GameTime+60;U.Skill3ActiveUntil=GameTime+(U.Role==EOVInteriorRole::Ranged?8:10);++U.SkillUses;}
        if(U.Role==EOVInteriorRole::Support && U.HeroId.IsValid() && OVInterior::Rank(U,2)>0 && U.AmplifierCharges>0 && GameTime>=U.Skill3ReadyAt && !U.DevRepairBaseCost.IsNearlyZero())
        {
            const float Total=U.Ship->Systems->MaxHullHP*(.0025f+.0025f*OVInterior::Rank(U,2));
            auto* Logistics=GetWorld()->GetSubsystem<UOVLogisticsSubsystem>();
            if(Logistics->StartDevRepair(U.Ship.Get(),U.Ship.Get(),true,U.HeroId,U.DevRepairBaseCost,0,Total/10,10,Total))
            {--U.AmplifierCharges;U.Skill3ReadyAt=GameTime+90;++U.SkillUses;Emit(TEXT("DutyRepair"),U,TEXT("지원 보직 수리 · 유한 자원과 충전 소모"));}
        }
        return;
    }
    if(U.Order==EOVInteriorOrder::Retreat)
    {
        U.EnemyId=0;U.State=EOVInteriorState::Retreating;
        if(!U.bWasInsideTarget || U.EntryShip!=U.Ship) {U.State=EOVInteriorState::Defending;U.Reason=TEXT("외부 진입 기록 없음 · 현재 함선에서 대기");return;}
        if(!MoveTowards(U,U.EntrySlot,Delta))return;
        if(!MovePoint(U,U.EntryLocalPoint,Delta))return;
        if(GetWorld()->GetSubsystem<UOVTransportSubsystem>()->ReturnRuntimePersonToEntry(U.PersonId,U.UnitId,U.Ship.Get(),U.LocalPosition,U.LayoutVersion))
        {U.Reason=TEXT("실제 원 진입점 도착 · 원 운송수단으로 철수");Emit(TEXT("Extraction"),U,U.Reason);SyncTransport(U);}
        else {U.State=EOVInteriorState::Blocked;U.Reason=TEXT("원 운송수단 회수 불가 · 무료 탈출 없음");}
        return;
    }
    // Healing is finite and uses the same observed range/LOS as combat. Moving or returning never refills it.
    if(U.Role==EOVInteriorRole::Support && OVInterior::Rank(U,2)>0 && U.Medkits>=2 && GameTime>=U.Skill3ReadyAt)
    {
        bool bNeed=false;for(const auto& Friend:Units)if(Friend.FactionId==U.FactionId && Friend.HP<Friend.MaxHP*.5f && Visible(U,Friend,600)){bNeed=true;break;}
        if(bNeed){U.Medkits-=2;U.Skill3ReadyAt=GameTime+90;U.Skill3ActiveUntil=GameTime+5;++U.SkillUses;}
    }
    if(U.Role==EOVInteriorRole::Support && U.Medkits>0 && GameTime>=U.SkillReadyAt)
    {
        FOVInteriorUnitRecord* Patient=nullptr;float Lowest=.50001f;
        for(auto& Friend:Units)if(Friend.FactionId==U.FactionId && Visible(U,Friend,U.Range))
            if(Friend.HP/Friend.MaxHP<Lowest){Lowest=Friend.HP/Friend.MaxHP;Patient=&Friend;}
        if(Patient){const int32 R=OVInterior::Rank(U,0);const float Heal=(Settings.SupportHeal+15*(R-1))*(1+.06f*(U.HeroLevel-1));Patient->HP=FMath::Min(Patient->MaxHP,Patient->HP+FMath::Max(0.f,Heal));--U.Medkits;U.SkillReadyAt=GameTime+FMath::Max(.1f,Settings.SupportCooldown-.5f*(R-1));++U.SkillUses;Emit(TEXT("Heal"),U,TEXT("생체 재구성 · 촉매 1개 소모"));}
    }
    FOVInteriorUnitRecord* Enemy=nullptr;
    if(U.EnemyId>0) {auto* Existing=FindUnit(U.EnemyId);if(Existing && Existing->FactionId!=U.FactionId && Visible(U,*Existing,Settings.SightRange))Enemy=Existing;}
    if(!Enemy)
    {
        float Nearest=BIG_NUMBER;
        for(auto& Candidate:Units)if(Candidate.FactionId!=U.FactionId && Visible(U,Candidate,Settings.SightRange))
        {const float D=OVInterior::Distance(U,Candidate);if(D<Nearest){Nearest=D;Enemy=&Candidate;}}
    }
    U.EnemyId=Enemy?Enemy->UnitId:0;
    if(U.TrackedEnemy!=U.EnemyId){U.TrackedEnemy=U.EnemyId;U.TrackedSince=GameTime;U.BurstRemaining=0;}
    if(Enemy)
    {
        U.State=EOVInteriorState::Fighting;U.Reason=TEXT("관측된 적과 자동 교전");
        const float Distance=OVInterior::Distance(U,*Enemy);
        if(U.Role==EOVInteriorRole::Commander && OVInterior::Rank(U,2)>0 && GameTime>=U.Skill3ReadyAt)
        {U.Skill3ReadyAt=GameTime+60;U.Skill3ActiveUntil=GameTime+8;++U.SkillUses;}
        if(Distance>U.Range)
        {
            if(U.Role==EOVInteriorRole::Melee && OVInterior::Rank(U,2)>0 && Distance<=(500+100*OVInterior::Rank(U,2)) && GameTime>=U.Skill3ReadyAt)
            {U.Skill3ReadyAt=GameTime+30;U.Skill3ActiveUntil=GameTime+2;U.bRushAttack=true;++U.SkillUses;}
            if(U.NodeId==Enemy->NodeId)MovePoint(U,Enemy->LocalPosition,Delta);
            else MoveTowards(U,Enemy->NodeId,Delta);
            return;
        }
        if(U.Role==EOVInteriorRole::Melee && GameTime>=U.SkillReadyAt)
        {U.SkillReadyAt=GameTime+FMath::Max(.1f,Settings.MeleeCooldown);U.SkillActiveUntil=GameTime+FMath::Max(0.f,Settings.MeleeDuration);++U.SkillUses;Emit(TEXT("ForceField"),U,TEXT("역장 방어 가동"));}
        if(GameTime<U.AttackReadyAt)return;
        if(U.Role==EOVInteriorRole::Ranged && OVInterior::Rank(U,2)>0 && U.BurstRemaining==0 && GameTime>=U.Skill3ReadyAt)
        {U.Skill3ReadyAt=GameTime+30;U.BurstRemaining=3;U.BurstTargetId=Enemy->UnitId;++U.SkillUses;}
        const bool bBurst=U.BurstRemaining>0 && U.BurstTargetId==Enemy->UnitId;
        float AttackSpeed=0;
        for(const auto& Friend:Units)if(!Friend.bDutyRequested && Friend.Role==EOVInteriorRole::Commander && Friend.FactionId==U.FactionId && GameTime<Friend.Skill3ActiveUntil && OVInterior::Rank(Friend,2)>0 && Visible(U,Friend,1400))
            AttackSpeed=FMath::Max(AttackSpeed,.10f+.025f*(OVInterior::Rank(Friend,2)-1));
        if(bBurst){--U.BurstRemaining;U.AttackReadyAt=GameTime+(U.BurstRemaining>0?.3f:FMath::Max(.05f,U.Period/(1+AttackSpeed)));}
        else U.AttackReadyAt=GameTime+FMath::Max(.05f,U.Period/(1+AttackSpeed));
        ++U.Shots;
        float Damage=U.Damage*(U.bWounded?.9f:1.f);
        if(bBurst)Damage*=.5f;
        if(U.Role==EOVInteriorRole::Melee && U.bRushAttack){Damage*=1+.1f*(OVInterior::Rank(U,2)+1);U.bRushAttack=false;}
        if(U.Role==EOVInteriorRole::Ranged && OVInterior::Rank(U,3)>0 && GameTime-U.TrackedSince>=4)Damage*=1+.05f*(OVInterior::Rank(U,3)+1);
        if(!bBurst && U.Role==EOVInteriorRole::Ranged && GameTime>=U.SkillReadyAt)
        {Damage*=FMath::Max(1.f,Settings.RangedMultiplier+.25f*(OVInterior::Rank(U,0)-1));U.SkillReadyAt=GameTime+FMath::Max(.1f,Settings.RangedCooldown);++U.SkillUses;}
        float Aura=0;for(const auto& Friend:Units)if(!Friend.bDutyRequested && Friend.FactionId==U.FactionId && Friend.Role==EOVInteriorRole::Commander && Visible(U,Friend,Settings.CommanderAuraRange))
        Aura=FMath::Max(Aura,Settings.CommanderDamageBonus+.03f*(OVInterior::Rank(Friend,0)-1));
        Damage*=1+FMath::Max(0.f,Aura);
        if(Random.FRand()>=FMath::Clamp(.92f*(1-Enemy->Evasion),0.f,1.f))return;
        ++U.Hits;
        const int32 Weakness=U.Role==EOVInteriorRole::Ranged?OVInterior::Rank(U,1):0;
        const float Ignore=Weakness>0?.08f+.04f*(Weakness-1):0;
        const float Emergency=(Enemy->Role==EOVInteriorRole::Melee && GameTime<Enemy->Skill4ActiveUntil)?(.10f+.05f*(OVInterior::Rank(*Enemy,3)-1)):0;
        const float Reduction=FMath::Clamp(Enemy->Armor+((Enemy->Role==EOVInteriorRole::Melee && GameTime<Enemy->SkillActiveUntil)?Settings.MeleeReduction+.05f*(OVInterior::Rank(*Enemy,0)-1):0.f)+Emergency-Ignore,0.f,.65f);
        float Cover=0;
        for(const auto& Friend:Units)if(!Friend.bDutyRequested && Friend.UnitId!=Enemy->UnitId && Friend.FactionId==Enemy->FactionId && Friend.Role==EOVInteriorRole::Melee && OVInterior::Rank(Friend,1)>0 && Visible(*Enemy,Friend,300))
            Cover=FMath::Max(Cover,.08f+.02f*(OVInterior::Rank(Friend,1)-1));
        if(bBurst){if(GameTime>=Enemy->SlowUntil)Enemy->SlowFraction=0;Enemy->SlowUntil=GameTime+4;Enemy->SlowFraction=FMath::Max(Enemy->SlowFraction,.20f+.05f*(OVInterior::Rank(U,2)-1));}
        ApplyDamage(*Enemy,Damage*(1-Reduction)*(1-Cover),TEXT("선내 교전 사망"));return;
    }
    if(U.Order==EOVInteriorOrder::None){U.State=EOVInteriorState::Defending;U.Reason=TEXT("명령 대기");return;}
    if(U.TargetSlot.IsNone()){U.State=EOVInteriorState::Blocked;U.Reason=TEXT("시설 목표를 지정하세요");return;}
    U.State=EOVInteriorState::Moving;
    if(MoveTowards(U,U.TargetSlot,Delta))
    {U.State=U.Order==EOVInteriorOrder::Defend?EOVInteriorState::Defending:EOVInteriorState::Operating;U.Reason=TEXT("목표 방 도착 · 제어점 확보 확인");}
}

void UOVInteriorSubsystem::AdvanceOperations(float Delta)
{
    TMap<int32,TArray<int32>> Workers;
    for(int32 I=0;I<Units.Num();++I)
    {
        auto& U=Units[I];if(!IsOnboard(U)||U.State==EOVInteriorState::AwaitingRecovery||U.State==EOVInteriorState::Blocked||U.State==EOVInteriorState::Fighting||U.Order==EOVInteriorOrder::Retreat||U.Order==EOVInteriorOrder::None||U.NodeId!=U.TargetSlot)continue;
        const auto* L=U.Ship->Systems->Layout.Get();const int32 Index=L->GetSlotIndex(U.TargetSlot);if(Index==INDEX_NONE)continue;
        if(((U.LocalPosition-L->Slots[Index].LocalCenter)*U.Ship->GetActorScale3D()).Size()>Settings.ArrivalTolerance)continue;
        const auto Facility=U.Ship->Systems->GetSlotState(U.TargetSlot);
        if(L->Slots[Index].Facility==EOVFacilityType::None){U.State=EOVInteriorState::Defending;continue;}
        if(Facility.HP<=0){U.State=EOVInteriorState::Blocked;U.Reason=TEXT("목표 시설 물리 파괴 · 점령 불가");continue;}
        const int32 Owner=Facility.OccupyingFaction<0?U.Ship->Systems->FactionId:Facility.OccupyingFaction;
        bool Contested=false;for(const auto& Other:Units)if(Other.FactionId!=U.FactionId && IsOnboard(Other) && Other.Ship==U.Ship && Other.NodeId==U.NodeId){Contested=true;break;}
        if(Contested){U.Reason=TEXT("방 내부 적 잔류 · 점령 중단");continue;}
        if(U.Order!=EOVInteriorOrder::Sabotage && Owner==U.FactionId)
        {
            if(U.Order==EOVInteriorOrder::Capture)
            {if(auto* G=FindGraph(U.Ship.Get()))G->CompletedBy.Add(U.FactionId);U.Order=EOVInteriorOrder::Defend;U.Progress=1;}
            U.State=EOVInteriorState::Defending;U.Reason=TEXT("현장에서 아군 제어 확인 · 시설 수호");continue;
        }
        const auto Kind=U.Order==EOVInteriorOrder::Sabotage?EOVInteriorOrder::Sabotage:EOVInteriorOrder::Capture;
        int32 Op=Operations.IndexOfByPredicate([&](const auto& O){return O.Ship==U.Ship&&O.SlotId==U.TargetSlot&&O.FactionId==U.FactionId&&O.Kind==Kind;});
        if(Op==INDEX_NONE){FOVInteriorOperation O;O.Ship=U.Ship;O.SlotId=U.TargetSlot;O.FactionId=U.FactionId;O.Kind=Kind;Op=Operations.Add(O);}
        if(Operations[Op].bCompleted){Operations[Op].bCompleted=false;Operations[Op].Progress=Operations[Op].Checkpoint=0;}
        Workers.FindOrAdd(Op).Add(I);U.State=EOVInteriorState::Operating;
    }
    for(int32 I=0;I<Operations.Num();++I)
    {
        auto& O=Operations[I];const auto* W=Workers.Find(I);
        if(!W || W->IsEmpty())
        {if(!O.bCompleted)O.Progress=O.Kind==EOVInteriorOrder::Sabotage?O.Checkpoint:0;continue;}
        if(!O.Ship.IsValid()||O.Ship->Systems->bWrecked)continue;
        const auto* L=O.Ship->Systems->Layout.Get();const int32 Slot=L->GetSlotIndex(O.SlotId);
        const float Duration=O.Kind==EOVInteriorOrder::Sabotage?Settings.SabotageSeconds:L->Slots[Slot].Facility==EOVFacilityType::Bridge?Settings.BridgeSeconds:Settings.CaptureSeconds;
        float Command=0;
        for(int32 Unit:*W)if(Units[Unit].Role==EOVInteriorRole::Commander && OVInterior::Rank(Units[Unit],1)>0)
            Command+=.10f+.05f*(OVInterior::Rank(Units[Unit],1)-1);
        const float Speed=FMath::Clamp(1+Settings.AdditionalWorkerBonus*(W->Num()-1)+Command,1.f,2.f);
        O.Progress=FMath::Min(1.f,O.Progress+Delta*Speed/FMath::Max(.1f,Duration));
        if(O.Kind==EOVInteriorOrder::Sabotage)O.Checkpoint=FMath::Min(2.f/3,FMath::FloorToFloat(O.Progress*3)/3);
        for(int32 Unit:*W){Units[Unit].Progress=O.Progress;Units[Unit].Reason=FString::Printf(TEXT("%s 진행 %.0f%%"),O.Kind==EOVInteriorOrder::Sabotage?TEXT("공작"):TEXT("점령"),100*O.Progress);}
        if(O.Progress<1)continue;
        if(O.Ship->Systems->CompleteInteriorOperation(O.SlotId,O.FactionId,O.Kind==EOVInteriorOrder::Sabotage))
        {
            O.bCompleted=true;if(O.Kind==EOVInteriorOrder::Sabotage)++TotalSabotages;else ++TotalCaptures;
            if(auto* G=FindGraph(O.Ship.Get()))G->CompletedBy.Add(O.FactionId);
            for(int32 Unit:*W){Units[Unit].Order=EOVInteriorOrder::Defend;Units[Unit].State=EOVInteriorState::Defending;Units[Unit].Reason=TEXT("실제 시설 작전 완료 · 현재 방 수호");}
            Emit(O.Kind==EOVInteriorOrder::Sabotage?TEXT("Sabotage"):TEXT("Capture"),Units[(*W)[0]],TEXT("시설 작전 완료: 외부 기능에 반영"));
        }
    }
}

void UOVInteriorSubsystem::ProcessFacilityDamage(float Delta)
{
    for(auto& G:Graphs)
    {
        AOVShip* Ship=G.Ship.Get();if(!Ship||Ship->Systems->bWrecked||GetWorld()->GetSubsystem<UOVCombatSubsystem>()->IsDeparted(Ship))continue;
        const auto* L=Ship->Systems->Layout.Get();
        for(const auto& Slot:Ship->Systems->Slots)
        {
            const float Previous=G.PreviousHP.FindRef(Slot.SlotId);G.PreviousHP.Add(Slot.SlotId,Slot.HP);
            if(Slot.HP>=Previous || Previous<=0)continue;
            const int32 Index=L->GetSlotIndex(Slot.SlotId);
            if(Index==INDEX_NONE)continue;
            if(Slot.HP<=0)G.Disasters.Add(Slot.SlotId,GameTime+Settings.DisasterSeconds);
            for(auto& U:Units)if(IsOnboard(U)&&U.Ship==Ship&&U.NodeId==Slot.SlotId)
            {
                if(Slot.HP<=0 && L->Slots[Index].Facility==EOVFacilityType::Bridge && Ship->Systems->bCapturedBridgeDestroyed)
                {
                    if(!OVInterior::Hero(U)){Kill(U,TEXT("나포 함교 파괴: 일반 병력 사망"));continue;}
                    ApplyDamage(U,U.MaxHP*Settings.CapturedBridgeHeroDamageFraction,TEXT("나포 함교 파괴 충격 사망"));
                    if(OVInterior::Living(U))if(const auto* Neighbors=G.Neighbors.Find(U.NodeId))for(FName N:*Neighbors)if(PassageOpen(G,U.NodeId,N))
                    {U.NodeId=N;U.LocalPosition=L->Slots[L->GetSlotIndex(N)].LocalCenter;U.Path.Reset();U.PlannedGoal=NAME_None;U.Reason=TEXT("함교 파괴 충격으로 인접 방 이탈");break;}
                }
                else ApplyDamage(U,U.MaxHP*Settings.FacilityShockFraction*FMath::Clamp((Previous-Slot.HP)/FMath::Max(1.f,L->Slots[Index].MaxFacilityHP),0.f,1.f),TEXT("시설 피격 충격 사망"));
            }
        }
        for(auto& U:Units)if(IsOnboard(U)&&U.Ship==Ship)if(const double* Until=G.Disasters.Find(U.NodeId))if(GameTime<*Until)
        {
            float Protection=0;
            for(const auto& Friend:Units)if(IsOnboard(Friend)&&Friend.Ship==Ship&&Friend.NodeId==U.NodeId&&Friend.FactionId==U.FactionId&&Friend.Role==EOVInteriorRole::Support&&OVInterior::Rank(Friend,1)>0)
                Protection=FMath::Max(Protection,.15f+.075f*(OVInterior::Rank(Friend,1)-1));
            ApplyDamage(U,Settings.DisasterDamagePerSecond*Delta*(1-Protection),TEXT("파괴 구역 재난 사망"));
        }
        for(auto It=G.Disasters.CreateIterator();It;++It)if(GameTime>=It.Value())It.RemoveCurrent();
    }
}

void UOVInteriorSubsystem::RefreshPresence()
{
    for(auto& G:Graphs)if(G.Ship.IsValid())
    {
        TSet<int32> Present;
        for(const auto& U:Units)if(IsOnboard(U)&&U.Ship==G.Ship)Present.Add(U.FactionId);
        for(int32 F:G.PresenceFactions)if(!Present.Contains(F))G.Ship->Systems->SetRuntimeInteriorPresence(F,false,false);
        for(int32 F:Present)G.Ship->Systems->SetRuntimeInteriorPresence(F,true,G.CompletedBy.Contains(F));
        G.PresenceFactions=MoveTemp(Present);
    }
}

void UOVInteriorSubsystem::Step(float Delta)
{
    GameTime+=Delta;
    for(auto& U:Units)
    {
        AOVShip* PreviousShip=U.Ship.Get();
        SyncTransport(U);
        if(U.HeroId.IsValid() && PreviousShip && (!IsOnboard(U) || U.Ship!=PreviousShip))
            GetWorld()->GetSubsystem<UOVLogisticsSubsystem>()->CancelDevRepair(PreviousShip,U.HeroId);
        if(U.State!=EOVInteriorState::Dead && U.Ship.IsValid() && U.Ship->Systems->bWrecked)Kill(U,TEXT("내부 잔류 함선 완파: 전원 사망"));
    }
    ProcessFacilityDamage(Delta);
    for(auto& U:Units)AdvanceUnit(U,Delta);
    for(auto& Patient:Units)if(IsOnboard(Patient))
    {
        float Rate=0;
        for(const auto& Healer:Units)if(Healer.Role==EOVInteriorRole::Support && Healer.FactionId==Patient.FactionId && !Healer.bDutyActive && GameTime<Healer.Skill3ActiveUntil && OVInterior::Rank(Healer,2)>0 && Visible(Healer,Patient,600))
            Rate=FMath::Max(Rate,.01f+.01f*OVInterior::Rank(Healer,2));
        if(Rate>0)Patient.HP=FMath::Min(Patient.MaxHP,Patient.HP+Patient.MaxHP*Rate*Delta);
    }
    AdvanceOperations(Delta);RefreshPresence();
}

void UOVInteriorSubsystem::Simulate(float Delta)
{
    if(!bEnabled||!FMath::IsFinite(Delta)||Delta<=0)return;
    Accumulator+=Delta;
    while(Accumulator+1.e-7>=OVInterior::Step){Step(OVInterior::Step);Accumulator-=OVInterior::Step;if(Accumulator<0)Accumulator=0;}
}

FOVInteriorUnitRecord UOVInteriorSubsystem::GetUnit(int32 Id) const
{const auto* U=Units.FindByPredicate([Id](const auto& Item){return Item.UnitId==Id;});return U?*U:FOVInteriorUnitRecord();}
TArray<int32> UOVInteriorSubsystem::GetPassengerIds(const TArray<int32>& Ids) const
{TArray<int32> Out;for(int32 Id:Ids){const auto U=GetUnit(Id);if(U.PersonId>0)Out.Add(U.PersonId);}return Out;}
TArray<int32> UOVInteriorSubsystem::GetUnitsOnShip(AOVShip* Ship,int32 Faction) const
{TArray<int32> Out;for(const auto& U:Units)if(U.Ship==Ship&&U.FactionId==Faction&&IsOnboard(U))Out.Add(U.UnitId);return Out;}
FString UOVInteriorSubsystem::GetSummary(int32 Faction) const
{
    int32 Alive=0,Dead=0,Rescue=0,Inside=0;
    for(const auto& U:Units)if(U.FactionId==Faction){if(U.State==EOVInteriorState::Dead)++Dead;else ++Alive;if(U.State==EOVInteriorState::RescueWaiting)++Rescue;if(U.bWasInsideTarget&&IsOnboard(U))++Inside;}
    return FString::Printf(TEXT("아군 생존 %d · 내부 %d · 사망 %d · 구조 대기 %d"),Alive,Inside,Dead,Rescue);
}
