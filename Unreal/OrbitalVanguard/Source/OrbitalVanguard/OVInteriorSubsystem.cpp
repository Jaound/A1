#include "OVInteriorSubsystem.h"
#include "OVPlayScenarioSubsystem.h"
#include "OVBattleNotices.h"
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
            const auto* Combat=GetWorld()->GetSubsystem<UOVCombatSubsystem>();
            if(H->Status==EOVHeroStatus::Deployed && (bUnfinished || !U.Ship.IsValid() ||
                U.State==EOVInteriorState::InTransit || U.State==EOVInteriorState::AwaitingRecovery ||
                U.Ship->bFTLCharging || (Combat && Combat->IsDeparted(U.Ship.Get())) ||
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
    if(!H || !IsValid(Ship) || !Ship->Systems || Ship->Systems->bWrecked || Ship->Systems->bNeutral ||
       Ship->Systems->FactionId!=H->FactionId || Ship->bFTLCharging ||
       GetWorld()->GetSubsystem<UOVCombatSubsystem>()->IsDeparted(Ship) ||
       !H->bDeploymentReserved || H->Status!=EOVHeroStatus::Deployed || H->RuntimeUnitId!=0 || H->RuntimePersonId!=0)
    {LastMessage=TEXT("영웅 배치 불가: 편제, 상태 또는 기존 실행 연결 확인");return 0;}
    // F6/F5의 기존 함선은 선내·수송 장면 초기화에 포함되지 않을 수 있다.
    // 기존 장면이나 다른 승객을 재설정하지 않고 필요한 함선만 등록한다.
    if(!AddShip(Ship))return 0;
    auto* Transport=GetWorld()->GetSubsystem<UOVTransportSubsystem>();
    if(!Transport || !Transport->AddShip(Ship))
    {LastMessage=TEXT("영웅 배치 불가: 해당 함선을 수송 시스템에 등록할 수 없습니다");return 0;}
    const int32 Id=RegisterUnit(Ship,H->FactionId,H->Role,SlotId);
    auto* U=FindUnit(Id);if(!U)return 0;
    U->DefinitionId=H->DefinitionId; U->HeroId=HeroId;U->Name=H->Name;U->HeroSkillRanks=H->SkillRanks;U->HeroLevel=H->Level;U->AmplifierCharges=H->AmplifierCharges;U->DevRepairBaseCost=H->DevRepairBaseCost;
    if(U->DefinitionId==TEXT("R04"))U->Range=2000;
    U->MaxHP*=1+.06f*(H->Level-1);U->Damage*=1+.08f*(H->Level-1);
    U->Damage*=1+.05f*FMath::Clamp(H->FocusTier,0,4);
    U->MaxHP*=1+.05f*FMath::Clamp(H->ArmorTier,0,4);
    U->Armor+=.02f*FMath::Clamp(H->ArmorTier-1,0,3);
    const float Tactical=H->TacticalTier>0?.02f+.03f*FMath::Clamp(H->TacticalTier,1,4):0;
    if(H->TacticalMode==TEXT("Move"))U->EquipmentMove=Tactical;
    if(H->TacticalMode==TEXT("Heal"))U->EquipmentHeal=Tactical;
    if(H->TacticalMode==TEXT("Capture"))U->EquipmentCapture=Tactical;
    U->bWounded=H->bWounded;U->bSkill4Armed=H->bSkill4Armed;if(U->bWounded)U->MaxHP*=.9f;
    U->HP=H->CurrentHP<0?U->MaxHP:FMath::Clamp(H->CurrentHP,0.f,U->MaxHP);
    U->Medkits=H->Catalysts;
    if(H->CooldownRemaining.Num()==4){U->SkillReadyAt=GameTime+H->CooldownRemaining[0];U->Skill3ReadyAt=GameTime+H->CooldownRemaining[2];U->Skill4ReadyAt=GameTime+H->CooldownRemaining[3];}
    H->RuntimeUnitId=U->UnitId;H->RuntimePersonId=U->PersonId;
    if(auto* Combat=GetWorld()->GetSubsystem<UOVCombatSubsystem>())if(Combat->Settings)Combat->Settings->bHeroD114Aim=true;
    return Id;
}

bool UOVInteriorSubsystem::SetHeroBoardingAssignment(int32 UnitId,bool bAssigned)
{
    auto* U=FindUnit(UnitId);
    auto* Transport=GetWorld()->GetSubsystem<UOVTransportSubsystem>();
    if(!U || !U->HeroId.IsValid() || !OVInterior::Living(*U) || !Transport || !U->Ship.IsValid() || !U->Ship->Systems ||
       U->Ship->Systems->FactionId!=U->FactionId || U->bWasInsideTarget ||
       (bAssigned && (U->bDutyRequested || U->State==EOVInteriorState::Fighting ||
           (U->State==EOVInteriorState::Operating && (U->Order==EOVInteriorOrder::Capture || U->Order==EOVInteriorOrder::Sabotage)))))
    {LastMessage=TEXT("강습 편성 불가: 아군 함선의 생존 영웅을 선택하고 보직·교전을 먼저 해제하세요");return false;}
    const auto Person=Transport->GetPerson(U->PersonId);
    if(Person.RuntimeUnitId!=UnitId || Person.CurrentShip!=U->Ship || Person.State!=EOVPassengerState::Aboard ||
       !Transport->SetRuntimeBoardingAssignment(U->PersonId,UnitId,bAssigned))
    {LastMessage=TEXT("강습 편성 불가: 실제 탑승 인원 상태를 확인하세요");return false;}
    LastMessage=bAssigned?TEXT("영웅을 강습 대상으로 명시 편성했습니다"):TEXT("영웅의 강습 편성을 해제했습니다");
    return true;
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

bool UOVInteriorSubsystem::ActiveDuty(const FOVInteriorUnitRecord& U) const
{
    const auto* Ship=U.Ship.Get();
    if(!U.bDutyActive || !Ship || Ship->Systems->bWrecked || U.FactionId!=Ship->Systems->FactionId || U.NodeId!=U.DutySlot || !IsOnboard(U))return false;
    if(GetWorld()->GetSubsystem<UOVCombatSubsystem>()->IsDeparted(U.Ship.Get()))return false;
    const auto P=GetWorld()->GetSubsystem<UOVTransportSubsystem>()->GetPerson(U.PersonId);
    if(P.CurrentShip!=U.Ship || (P.State!=EOVPassengerState::Aboard && P.State!=EOVPassengerState::InsideTarget))return false;
    const auto S=Ship->Systems->GetSlotState(U.DutySlot);
    return S.HP>0 && S.Crew>0 && (S.OccupyingFaction<0 || S.OccupyingFaction==U.FactionId);
}
float UOVInteriorSubsystem::FormationRadarBonus(const AOVShip* Receiver) const
{
    for(const auto& U:Units)
        if(U.DefinitionId==TEXT("C03") && U.FactionId==Receiver->Systems->FactionId && ActiveDuty(U) &&
            FVector::DistSquared(Receiver->GetActorLocation(),U.Ship->GetActorLocation())<=FMath::Square(40000.))return .2f;
    return 0;
}

FOVHeroDutyBonuses UOVInteriorSubsystem::GetDutyBonuses(AOVShip* Ship) const
{
    FOVHeroDutyBonuses B;
    if(!IsValid(Ship) || !Ship->Systems || Ship->Systems->bWrecked || GetWorld()->GetSubsystem<UOVCombatSubsystem>()->IsDeparted(Ship))return B;
    for(const auto& U:Units)
    {
        if(U.Ship!=Ship)
        {
            if(U.DefinitionId==TEXT("C03") && U.FactionId==Ship->Systems->FactionId && GameTime<U.Skill3ActiveUntil &&
                Ship->Systems->Layout && Ship->Systems->Layout->HullSizeClass>=EOVHullSizeClass::Large && ActiveDuty(U))
            {
                double Best=FMath::Square(40000.); AOVShip* Nearest=nullptr;
                for(const auto& Candidate:GetWorld()->GetSubsystem<UOVCombatSubsystem>()->Ships){
                    AOVShip* Other=Candidate.Ship;
                    if(!IsValid(Other) || Candidate.bDeparted || Other==U.Ship.Get() || !Other->Systems->CanAct() ||
                        Other->Systems->FactionId!=U.FactionId || Other->Systems->Layout->HullSizeClass<EOVHullSizeClass::Large)continue;
                    const double D=FVector::DistSquared(Other->GetActorLocation(),U.Ship->GetActorLocation());
                    if(D<=Best){Best=D;Nearest=Other;}
                }
                if(Nearest==Ship)B.TurnBonus=FMath::Max(B.TurnBonus,.08f);
            }
            continue;
        }
        if(!ActiveDuty(U))continue;
        const int32 Rank=OVInterior::Rank(U,0), SkillRank=OVInterior::Rank(U,2);
        const bool Active=SkillRank>0 && GameTime<U.Skill3ActiveUntil;
        if(U.DefinitionId==TEXT("S02"))B.bEngineering=true;
        if(U.DefinitionId==TEXT("S03")){B.bShieldHarmonizer=true;B.bShipDisasterField|=Active;}
        if(Ship->Systems->Layout->bAllowAircraftOperations){
            if(U.DefinitionId==TEXT("R07")){B.BoltSpeedScale=1.12f;B.WingEvasion=.03f;if(Active)B.WingPeriodScale=.9f;}
            if(U.DefinitionId==TEXT("S04")){B.ProductionScale=.9f;B.DockScale=.65f;B.bHangarEfficiency|=Active;}
            if(U.DefinitionId==TEXT("C04")){B.SectionScale=.75f;B.ReformScale=.88f;if(Active)B.ReturnScale=1.1f;}
        }
        if(U.DefinitionId==TEXT("C05") && Ship->Systems->Layout->HullSizeClass>=EOVHullSizeClass::Large){
            B.PodScale=1.12f;B.ShuttleDockScale=.8f;
            if(Active)B.ShipCombatTempo=FMath::Max(B.ShipCombatTempo,(.10f+.025f*(SkillRank-1))*.5f);
        }
        if(U.DefinitionId==TEXT("R02")) {B.PDInterval=.9f/(Active?1.15f:1.f);B.PDAccuracy=.02f;}
        if(U.DefinitionId==TEXT("R01")) B.bOpeningAim=true;
        if(U.DefinitionId==TEXT("R03")) B.bLongAim=true;
        if(U.DefinitionId==TEXT("R05")) {B.MissileInterval=.92f;if(Active){B.GuidedTurn=1.25f;B.GuidedSpread=.85f;}}
        if(U.DefinitionId==TEXT("R09")) B.bNearTraverse=true;
        if(U.DefinitionId==TEXT("R10")) B.bExposedAim=true;
        if(U.DefinitionId==TEXT("C02")) {B.AccelerationScale=1.12f;B.BrakingScale=Active?1.2f:1.12f;if(Active)B.LateralScale=1.18f;}
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

int32 UOVInteriorSubsystem::CountEmbarkedHeroes(const AOVShip* Ship,int32 Faction) const
{
    int32 Count=0;
    for(const auto& U:Units)
        if(U.Ship==Ship && U.FactionId==Faction && OVInterior::Hero(U) && U.HP>0)++Count;
    return Count;
}

int32 UOVInteriorSubsystem::CountActiveHeroes(int32 Faction) const
{
    int32 Count=0; for (const auto& U:Units) if(U.FactionId==Faction && OVInterior::Hero(U) && U.State!=EOVInteriorState::Dead) ++Count;
    return Count;
}

int32 UOVInteriorSubsystem::RegisterUnit(AOVShip* Ship,int32 Faction,EOVInteriorRole Role,FName SlotId)
{
    const auto* GI=GetWorld()->GetGameInstance();
    const int32 Limit=GI?GI->GetSubsystem<UOVHeroSubsystem>()->DeploymentLimit(Faction):15;
    if(!bEnabled || !FindGraph(Ship) || Faction<0 || Ship->Systems->bWrecked ||
        (Role!=EOVInteriorRole::Trooper && CountActiveHeroes(Faction)>=FMath::Min(Limit,Settings.MaxActiveHeroesPerFaction)))
    { LastMessage=TEXT("인원 등록 불가: 배치·생존·세력 영웅 15명 한도 확인"); return 0; }
    if(Role!=EOVInteriorRole::Trooper && GetWorld()->GetSubsystem<UOVPlayScenarioSubsystem>()->GetActiveDefinition() &&
       CountEmbarkedHeroes(Ship,Faction)>=GetWorld()->GetSubsystem<UOVPlayScenarioSubsystem>()->CurrentHeroBerths()){
        LastMessage=TEXT("이 함선의 시나리오 영웅 정원이 찼습니다");return 0;
    }
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
        {
            Ids.Add(Id);
            if(Role==EOVInteriorRole::Trooper)FindUnit(Id)->SquadId=Ids[0];
            GetWorld()->GetSubsystem<UOVTransportSubsystem>()->SetRuntimeBoardingAssignment(FindUnit(Id)->PersonId,Id,true); }
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

float UOVInteriorSubsystem::MovementBonus(const FOVInteriorUnitRecord& U) const
{
    float Aura=0, AdditionalAura=0;
    if(U.Order==EOVInteriorOrder::Retreat || U.Order==EOVInteriorOrder::Defend)
        for(const auto& Friend:Units)if(!Friend.bDutyRequested && Friend.Role==EOVInteriorRole::Commander && Friend.FactionId==U.FactionId && Friend.Ship==U.Ship && Friend.NodeId==U.NodeId && IsOnboard(Friend) && OVInterior::Rank(Friend,3)>0)
            Aura=FMath::Max(Aura,.05f+.025f*(OVInterior::Rank(Friend,3)-1));
    if(U.Order==EOVInteriorOrder::Retreat || U.Order==EOVInteriorOrder::Defend)
        if(Units.ContainsByPredicate([&](const auto& A){return A.DefinitionId==TEXT("C01") && A.FactionId==U.FactionId &&
           !A.bDutyRequested && GameTime<A.Skill3ActiveUntil && Visible(A,U,1400);}))AdditionalAura=.05f;
    const float Scale=Aura+AdditionalAura>0?ExternalAuraScale(U):1.f;
    float Bonus=FMath::Max(U.EquipmentMove,Aura*Scale)+AdditionalAura*Scale;
    if(GameTime<U.InvasionAlertUntil && (U.TargetSlot==U.InvasionSlot || U.PlannedGoal==U.InvasionSlot))Bonus+=.12f;
    return FMath::Min(.20f,Bonus);
}

bool UOVInteriorSubsystem::MovePoint(FOVInteriorUnitRecord& U,FVector Goal,float Delta)
{
    const FVector Scale=U.Ship->GetActorScale3D();const FVector WorldDelta=(Goal-U.LocalPosition)*Scale;
    const float Distance=WorldDelta.Size();
    if(Distance<=Settings.ArrivalTolerance) {U.LocalPosition=Goal;return true;}
    float Speed=U.Speed*(1+MovementBonus(U));
    if(U.Role==EOVInteriorRole::Melee && GameTime<U.Skill3ActiveUntil)Speed*=1.5f;
    Speed*=1-FMath::Clamp(FMath::Max(GameTime<U.GuardSlowUntil?.2f:0.f,GameTime<U.SlowUntil?U.SlowFraction:0.f),0.f,1.f);
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
    if(auto* Feed=GetWorld()->GetSubsystem<UOVBattleNotices>())Feed->RecordInteriorEvent(Kind,U,GameTime);
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
    if(U.HP>0)if(auto* Feed=GetWorld()->GetSubsystem<UOVBattleNotices>())Feed->RecordInteriorEvent(TEXT("Damage"),U,GameTime);
    if(U.HP>0 && U.Role==EOVInteriorRole::Melee && OVInterior::Rank(U,3)>0 && U.HP<U.MaxHP*.25f && U.bSkill4Armed && GameTime>=U.Skill4ReadyAt)
    {if(U.DefinitionId==TEXT("M08"))U.ApexUntil=GameTime+4;U.Skill4ActiveUntil=GameTime+3;U.Skill4ReadyAt=GameTime+120;U.bSkill4Armed=false;++U.SkillUses;}
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
            AOVShip* Arrived=P.CurrentShip.Get();
            if(!IsValid(Arrived) || !Arrived->Systems || !Arrived->Systems->Layout ||
               Arrived->Systems->Layout->LayoutVersion!=P.EntryLayoutVersion ||
               Arrived->Systems->Layout->GetSlotIndex(P.EntrySlot)==INDEX_NONE || P.EntryLocalPoint.ContainsNaN() || !AddShip(Arrived))
            {U.Ship=Arrived;U.State=EOVInteriorState::Blocked;U.Reason=TEXT("진입함 내부 배치 검증 실패 · 인원 보존");return;}
            U.Ship=P.CurrentShip;U.NodeId=P.EntrySlot;U.LocalPosition=P.EntryLocalPoint;U.LayoutVersion=P.EntryLayoutVersion;
            if(P.PodId>0) {
                U.PodEntryAt=GameTime;
                for(auto& Guard:Units) if(Guard.DefinitionId==TEXT("M06") && IsOnboard(Guard) && Guard.Ship==Arrived &&
                    Guard.FactionId==Arrived->Systems->FactionId && Guard.FactionId!=U.FactionId){
                    Guard.InvasionAlertUntil=GameTime+8;Guard.InvasionSlot=P.EntrySlot;
                }
            }
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
        AOVShip* Returned=P.CurrentShip.Get();
        if(!IsValid(Returned) || !Returned->Systems || !Returned->Systems->Layout || !AddShip(Returned))
        {U.Ship=Returned;U.State=EOVInteriorState::Blocked;U.Reason=TEXT("귀환함 내부 배치 검증 실패 · 인원 보존");return;}
        U.Ship=P.CurrentShip;U.bWasInsideTarget=false;U.EntryShip=nullptr;U.Path.Reset();U.PlannedGoal=NAME_None;
        if(U.Ship.IsValid()) {U.NodeId=U.Ship->Systems->Layout->CoreSlotId;U.LocalPosition=U.Ship->Systems->Layout->Slots[U.Ship->Systems->Layout->GetSlotIndex(U.NodeId)].LocalCenter;U.LayoutVersion=U.Ship->Systems->Layout->LayoutVersion;}
        U.Order=EOVInteriorOrder::Defend;U.TargetSlot=U.NodeId;U.State=EOVInteriorState::Aboard;
        if(U.bWasCriticallyInjured && OVInterior::Hero(U) && !U.bWounded)
        {
            auto* Medic=Units.FindByPredicate([&](const auto& A){return A.DefinitionId==TEXT("S05") && A.bTraumaTreatmentAvailable &&
                A.FactionId==U.FactionId && A.Ship==Returned && A.NodeId==U.NodeId && IsOnboard(A);});
            if(Medic){Medic->bTraumaTreatmentAvailable=false;U.bWasCriticallyInjured=false;}
            else {U.bWounded=true;U.MaxHP*=.9f;U.HP=FMath::Min(U.HP,U.MaxHP);}
        }
        U.Reason=TEXT("생존 귀환 · HP/촉매/쿨타임 보존");Emit(TEXT("Return"),U,U.Reason);
    }
}

float UOVInteriorSubsystem::ExternalAuraScale(const FOVInteriorUnitRecord& Target) const
{
    if(!OVInterior::Hero(Target))return 1.f;
    const bool Isolated=Units.ContainsByPredicate([&](const auto& Duelist){
        return Duelist.DefinitionId==TEXT("M08") && Duelist.State==EOVInteriorState::Fighting &&
            Duelist.EnemyId==Target.UnitId && Duelist.FactionId!=Target.FactionId &&
            GameTime-Duelist.TrackedSince>=5 && Visible(Duelist,Target,Duelist.Range);
    });
    return Isolated?.5f:1.f;
}

float UOVInteriorSubsystem::InteriorReduction(const FOVInteriorUnitRecord& Target,float Ignore,bool bPhaseStrike) const
{
    float Armor=Target.Armor;
    if(Target.Role==EOVInteriorRole::Melee && GameTime<Target.SkillActiveUntil)
        Armor+=Settings.MeleeReduction+.05f*(OVInterior::Rank(Target,0)-1);
    if(Target.Role==EOVInteriorRole::Melee && GameTime<Target.Skill4ActiveUntil)
        Armor+=.1f+.05f*(OVInterior::Rank(Target,3)-1);
    float Cover=0, Guard=0, Detail=0, Bridge=0;
    for(const auto& Ally:Units)
    {
        if(Ally.UnitId==Target.UnitId || Ally.FactionId!=Target.FactionId || !IsOnboard(Ally) || Ally.Ship!=Target.Ship)continue;
        const bool Same=Ally.NodeId==Target.NodeId;
        if(Same && Ally.DefinitionId==TEXT("M09") && Target.bDutyActive) Guard=.10f;
        if(Same && Ally.DefinitionId==TEXT("M03") && GameTime<Ally.SkillActiveUntil && !OVInterior::Hero(Target)) Detail=.06f;
        const float Radius=Ally.DefinitionId==TEXT("M01") && GameTime-Ally.PodEntryAt<6 ?450.f:300.f;
        if(Same && Ally.DefinitionId==TEXT("C01") && Ally.State==EOVInteriorState::Operating &&
           Ally.Order==EOVInteriorOrder::Capture && !OVInterior::Hero(Target)){
            const int32 Room=Ally.Ship->Systems->Layout->GetSlotIndex(Ally.NodeId);
            if(Room!=INDEX_NONE && Ally.Ship->Systems->Layout->Slots[Room].Facility==EOVFacilityType::Bridge)Bridge=.08f;
        }
        if(!Ally.bDutyRequested && Ally.Role==EOVInteriorRole::Melee && OVInterior::Rank(Ally,1)>0 && Visible(Target,Ally,Radius))
        {
            float Rate=.08f+.02f*(OVInterior::Rank(Ally,1)-1);
            if(Same && Ally.DefinitionId==TEXT("M07") && GameTime<Ally.SkillActiveUntil && Target.HP<=Target.MaxHP*.5f)Rate*=1.5f;
            Cover=FMath::Max(Cover,Rate);
        }
    }
    Cover=1-(1-Cover)*(1-Guard)*(1-Detail)*(1-Bridge);
    if(Cover>0)Cover*=ExternalAuraScale(Target);
    const float Combined=FMath::Clamp(1-(1-FMath::Clamp(Armor-Ignore,0.f,.65f))*(1-Cover),0.f,.65f);
    return bPhaseStrike && Combined>=.20f ? Combined-.08f : Combined;
}

void UOVInteriorSubsystem::ReleaseIrisField(FOVInteriorUnitRecord& Unit)
{
    Unit.bPendingIrisArc=false;
    if(Unit.bDutyRequested || Unit.Order==EOVInteriorOrder::Retreat || !IsOnboard(Unit))return;
    const auto* Layout=Unit.Ship->Systems->Layout.Get();
    const int32 From=Layout->GetSlotIndex(Unit.NodeId);
    if(From==INDEX_NONE)return;
    Unit.LastPhaseArcAt=GameTime;
    ++Unit.SignatureUses;
    Emit(TEXT("PhaseArc"),Unit,TEXT("위상 잔영 베기"));
    for(auto& Target:Units)
    {
        if(Target.FactionId==Unit.FactionId || !Visible(Unit,Target,250.f))continue;
        const int32 To=Layout->GetSlotIndex(Target.NodeId);
        if(To==INDEX_NONE || Layout->Slots[From].DeckId!=Layout->Slots[To].DeckId)continue;
        const FVector Offset=(Target.LocalPosition-Unit.LocalPosition)*Unit.Ship->GetActorScale3D();
        if(!Offset.IsNearlyZero() && FVector::DotProduct(Offset.GetSafeNormal(),Unit.FieldFacing)<.5)continue;
        ApplyDamage(Target,Unit.Damage*(Unit.bWounded?.9f:1.f)*1.4f*(1-InteriorReduction(Target,0,true)),TEXT("위상 잔영 베기"));
    }
}

void UOVInteriorSubsystem::AdvanceUnit(FOVInteriorUnitRecord& U,float Delta)
{
    if(!IsOnboard(U)||U.State==EOVInteriorState::AwaitingRecovery){U.bPendingIrisArc=false;return;}
    if(U.bDutyRequested || U.Order==EOVInteriorOrder::Retreat)U.bPendingIrisArc=false;
    if(!FindGraph(U.Ship.Get())){U.State=EOVInteriorState::Blocked;U.Reason=TEXT("내부 배치가 없어 이동할 수 없습니다");return;}
    if(GetWorld()->GetSubsystem<UOVCombatSubsystem>()->IsDeparted(U.Ship.Get()))
    {if(U.HeroId.IsValid())GetWorld()->GetSubsystem<UOVLogisticsSubsystem>()->CancelDevRepair(U.Ship.Get(),U.HeroId);U.bDutyActive=false;U.State=EOVInteriorState::Blocked;U.Reason=TEXT("목표 초광속 이탈 · 인원 보존, 성계 밖 작전/회수 미지원");return;}
    const auto* L=U.Ship->Systems->Layout.Get();
    if(U.LayoutVersion!=L->LayoutVersion){U.State=EOVInteriorState::Blocked;U.Reason=TEXT("배치 버전 불일치: 이동 중지");return;}
    if(U.bPendingIrisArc && GameTime>=U.SkillActiveUntil)ReleaseIrisField(U);
    if(U.HP>=U.MaxHP*.4f)U.bSkill4Armed=true;
    if(U.DefinitionId==TEXT("M03")){
        TSet<int32> Here;
        if(U.Ship->Systems->FactionId==U.FactionId && U.Order==EOVInteriorOrder::Defend)
            for(auto& Other:Units)if(Other.bWasInsideTarget && Other.FactionId!=U.FactionId && IsOnboard(Other) && Other.Ship==U.Ship && Other.NodeId==U.NodeId){
                Here.Add(Other.UnitId);
                if(!U.GuardContacts.Contains(Other.UnitId)) Other.GuardSlowUntil=GameTime+2;
            }
        U.GuardContacts=MoveTemp(Here);
    }
    if(U.bDutyRequested)
    {
        const auto Facility=U.Ship->Systems->GetSlotState(U.DutySlot);
        if(U.Ship->Systems->FactionId!=U.FactionId || Facility.HP<=0 || Facility.Crew<=0 || (Facility.OccupyingFaction>=0 && Facility.OccupyingFaction!=U.FactionId))
        {if(U.HeroId.IsValid())GetWorld()->GetSubsystem<UOVLogisticsSubsystem>()->CancelDevRepair(U.Ship.Get(),U.HeroId);U.bDutyActive=false;U.DutyReadyAt=0;U.State=EOVInteriorState::Blocked;U.Reason=TEXT("보직 시설 가동 조건 미충족");return;}
        if(!MoveTowards(U,U.DutySlot,Delta)){U.bDutyActive=false;U.DutyReadyAt=0;U.State=EOVInteriorState::Moving;return;}
        if(U.DutyReadyAt<=0){
            const bool Escort=U.LastDutySlot==U.DutySlot && Units.ContainsByPredicate([&](const auto& A){
                return A.DefinitionId==TEXT("M09") && A.FactionId==U.FactionId && A.Ship==U.Ship && A.NodeId==U.NodeId && IsOnboard(A);});
            U.DutyReadyAt=GameTime+(Escort?1:3);
        }
        if(GameTime<U.DutyReadyAt){U.State=EOVInteriorState::Operating;U.Reason=TEXT("보직 인계 중");return;}
        U.LastDutySlot=U.DutySlot;U.bDutyActive=true;U.State=EOVInteriorState::Operating;U.Reason=TEXT("보직 수행 중");
        if((U.Role==EOVInteriorRole::Ranged || U.Role==EOVInteriorRole::Commander) && OVInterior::Rank(U,2)>0 && GameTime>=U.Skill3ReadyAt)
        {U.Skill3ReadyAt=GameTime+60;U.Skill3ActiveUntil=GameTime+(U.Role==EOVInteriorRole::Ranged && U.DefinitionId!=TEXT("R09")?8:10);++U.SkillUses;}
        if(U.Role==EOVInteriorRole::Support && U.HeroId.IsValid() && OVInterior::Rank(U,2)>0 && U.AmplifierCharges>0 && GameTime>=U.Skill3ReadyAt && !U.DevRepairBaseCost.IsNearlyZero())
        {
            const float Total=U.Ship->Systems->MaxHullHP*(.0025f+.0025f*OVInterior::Rank(U,2));
            auto* Logistics=GetWorld()->GetSubsystem<UOVLogisticsSubsystem>();
            if(Logistics->StartDevRepair(U.Ship.Get(),U.Ship.Get(),true,U.HeroId,U.DevRepairBaseCost*(U.DefinitionId==TEXT("S02")?1.25/1.5:1.),0,Total/10,10,Total))
            {--U.AmplifierCharges;U.Skill3ActiveUntil=GameTime+10;U.Skill3ReadyAt=GameTime+90;++U.SkillUses;Emit(TEXT("DutyRepair"),U,TEXT("지원 보직 수리 · 유한 자원과 충전 소모"));}
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
        if(Patient){const int32 R=OVInterior::Rank(U,0);const float Heal=(Settings.SupportHeal+15*(R-1))*(1+.06f*(U.HeroLevel-1));Patient->HP=FMath::Min(Patient->MaxHP,Patient->HP+FMath::Max(0.f,Heal)*(1+U.EquipmentHeal)*(U.DefinitionId==TEXT("S01") && Patient->HP<Patient->MaxHP*.3f?1.1f:1.f));--U.Medkits;U.SkillReadyAt=GameTime+FMath::Max(.1f,Settings.SupportCooldown-.5f*(R-1)-(U.DefinitionId==TEXT("S05")?1.f:0.f));++U.SkillUses;Emit(TEXT("Heal"),U,TEXT("생체 재구성 · 촉매 1개 소모"));}
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
            {U.Skill3ReadyAt=GameTime+30;U.Skill3ActiveUntil=GameTime+2;U.bRushAttack=true;U.RushTargets.Reset();U.bRushWeakened=false;
             for(auto& Counter:Units)if(Counter.DefinitionId==TEXT("R08") && Counter.FactionId!=U.FactionId &&
                 !Counter.bDutyRequested && GameTime>=Counter.AttackReadyAt && Visible(Counter,U,1200)){
                 Counter.AttackReadyAt=GameTime+Counter.Period;++Counter.Shots;
                 if(Random.FRand()<.92f*(1-U.Evasion)){++Counter.Hits;U.bRushWeakened=true;
                     ApplyDamage(U,Counter.Damage*(1-InteriorReduction(U,0,false)),TEXT("돌입 견제 사격"));}
                 if(!OVInterior::Living(U))return;
             }
             U.RushRefundUntil=U.DefinitionId==TEXT("M10")?GameTime+5:0;++U.SkillUses;}
            const FVector Before=U.LocalPosition;
            if(U.NodeId==Enemy->NodeId)MovePoint(U,Enemy->LocalPosition,Delta);
            else MoveTowards(U,Enemy->NodeId,Delta);
            if(U.DefinitionId==TEXT("M04") && U.bRushAttack && GameTime<U.Skill3ActiveUntil){
                const auto Scale=U.Ship->GetActorScale3D();
                for(auto& Other:Units)if(U.RushTargets.Num()<2 && Other.FactionId!=U.FactionId && !U.RushTargets.Contains(Other.UnitId) &&
                   Visible(U,Other,U.Range) && FMath::PointDistToSegment(Other.LocalPosition*Scale,Before*Scale,U.LocalPosition*Scale)<=80){
                    U.RushTargets.Add(Other.UnitId);ApplyDamage(Other,U.Damage*.65f*(1-InteriorReduction(Other,0,false)),TEXT("유성 연쇄 돌입"));
                }
            }
            return;
        }
        if(U.Role==EOVInteriorRole::Melee && GameTime>=U.SkillReadyAt)
        {U.SkillReadyAt=GameTime+FMath::Max(.1f,Settings.MeleeCooldown);U.SkillActiveUntil=GameTime+FMath::Max(0.f,Settings.MeleeDuration);++U.SkillUses;
         if(U.DefinitionId==TEXT("M02")){
             U.bPendingIrisArc=true;
             FVector Aim=(Enemy->LocalPosition-U.LocalPosition)*U.Ship->GetActorScale3D();
             if(!Aim.IsNearlyZero())U.FieldFacing=Aim.GetSafeNormal();
         }
         if(U.DefinitionId==TEXT("M06")) for(auto& Other:Units)
             if(Other.Role==EOVInteriorRole::Ranged && Other.FactionId!=U.FactionId && Visible(U,Other,250))Other.JamUntil=GameTime+3;
         Emit(TEXT("ForceField"),U,TEXT("역장 방어 가동"));}
        if(GameTime<U.AttackReadyAt)return;
        if(U.Role==EOVInteriorRole::Ranged && OVInterior::Rank(U,2)>0 && U.BurstRemaining==0 && GameTime>=U.Skill3ReadyAt)
        {U.Skill3ReadyAt=GameTime+30;U.BurstRemaining=3;U.BurstTargetId=Enemy->UnitId;++U.SkillUses;}
        const bool bBurst=U.BurstRemaining>0 && U.BurstTargetId==Enemy->UnitId;
        const float ShipTempo=U.FactionId==U.Ship->Systems->FactionId?GetDutyBonuses(U.Ship.Get()).ShipCombatTempo:0;
        const float ExternalScale=ExternalAuraScale(U);
        float AttackSpeed=FMath::Max(ShipTempo*ExternalScale,GameTime<U.ApexUntil?.15f:0.f);
        for(const auto& Friend:Units)if(!Friend.bDutyRequested && Friend.Role==EOVInteriorRole::Commander && Friend.FactionId==U.FactionId && GameTime<Friend.Skill3ActiveUntil && OVInterior::Rank(Friend,2)>0 && Visible(U,Friend,1400))
            AttackSpeed=FMath::Max(AttackSpeed,(.10f+.025f*(OVInterior::Rank(Friend,2)-1))*(Friend.UnitId==U.UnitId?1.f:ExternalScale));
        if(bBurst){--U.BurstRemaining;U.AttackReadyAt=GameTime+(U.BurstRemaining>0?.3f:FMath::Max(.05f,U.Period/(1+AttackSpeed)));}
        else U.AttackReadyAt=GameTime+FMath::Max(.05f,U.Period/(1+AttackSpeed));
        ++U.Shots;
        float Damage=U.Damage*(U.bWounded?.9f:1.f);
        if(GameTime<U.ApexUntil)Damage*=1.10f;
        if(U.DefinitionId==TEXT("R06") && U.bWasInsideTarget &&
            Units.ContainsByPredicate([&](const auto& A){return IsOnboard(A) && A.Ship==U.Ship && A.NodeId==U.NodeId && A.FactionId==U.FactionId &&
                A.Role==EOVInteriorRole::Melee && A.State==EOVInteriorState::Fighting && A.EnemyId==Enemy->UnitId;})) Damage*=1.1f;
        if(U.DefinitionId==TEXT("M05") && (Enemy->bDutyActive || Enemy->Order==EOVInteriorOrder::Defend) &&
           OVInterior::Hero(*Enemy))Damage*=1.12f;
        if(bBurst)Damage*=.5f;
        if(U.Role==EOVInteriorRole::Melee && U.bRushAttack){Damage*=1+.1f*(OVInterior::Rank(U,2)+1);U.bRushAttack=false;if(U.bRushWeakened)Damage*=.85f;
         if(U.DefinitionId==TEXT("M01")) for(auto& Other:Units)
            if(Other.FactionId!=U.FactionId && Visible(U,Other,200)){
                if(GameTime>=Other.SlowUntil)Other.SlowFraction=0;
                Other.SlowUntil=FMath::Max(Other.SlowUntil,GameTime+(OVInterior::Hero(Other)?1.2:2.5));
                Other.SlowFraction=FMath::Max(Other.SlowFraction,.25f);
            }
        }
        const bool Tracked=OVInterior::Rank(U,3)>0 && GameTime-U.TrackedSince>=
            (U.DefinitionId==TEXT("R10") && Enemy->HP<=Enemy->MaxHP*.4f?2.5:4.0);
        if(U.Role==EOVInteriorRole::Ranged && Tracked)Damage*=1+.05f*(OVInterior::Rank(U,3)+1);
        const bool AimedShot=!bBurst && U.Role==EOVInteriorRole::Ranged && GameTime>=U.SkillReadyAt;
        if(AimedShot)
        {Damage*=FMath::Max(1.f,Settings.RangedMultiplier+.25f*(OVInterior::Rank(U,0)-1)+(U.DefinitionId==TEXT("R06") && OVInterior::Rank(U,1)>0?.2f:0.f));U.SkillReadyAt=GameTime+FMath::Max(.1f,Settings.RangedCooldown);++U.SkillUses;}
        float Aura=0;
        for(const auto& Friend:Units)
            if(!Friend.bDutyRequested && Friend.FactionId==U.FactionId && Friend.Role==EOVInteriorRole::Commander && Visible(U,Friend,Settings.CommanderAuraRange))
            {
                const float Scale=Friend.UnitId==U.UnitId?1.f:ExternalAuraScale(U);
                Aura=FMath::Max(Aura,(Settings.CommanderDamageBonus+.03f*(OVInterior::Rank(Friend,0)-1))*Scale);
            }
        Damage*=1+FMath::Max(0.f,Aura);
        if(AimedShot && Tracked && U.DefinitionId==TEXT("R10") && Enemy->HP<=Enemy->MaxHP*.35f)Damage*=1.15f;
        const float Aim=.92f-(GameTime<U.JamUntil?.08f:0.f)+(U.DefinitionId==TEXT("R04") && Distance>=1200?.03f:0.f);
        const float Evasion=Enemy->Evasion+(Enemy->DefinitionId==TEXT("M04") && (GameTime<Enemy->Skill3ActiveUntil || MovementBonus(*Enemy)>0)?.06f:0.f);
        if(Random.FRand()>=FMath::Clamp(Aim*(1-Evasion),0.f,1.f))return;
        if(AimedShot && U.DefinitionId==TEXT("R08") && OVInterior::Hero(*Enemy)){
            for(double* Ready:{&Enemy->SkillReadyAt,&Enemy->Skill3ReadyAt,&Enemy->Skill4ReadyAt})
                if(*Ready>GameTime)*Ready+=2;
        }
        ++U.Hits;
        const int32 Weakness=U.Role==EOVInteriorRole::Ranged?OVInterior::Rank(U,1):0;
        const float Ignore=Weakness>0?.08f+.04f*(Weakness-1):0;
        const bool Phase=U.DefinitionId==TEXT("M02");
        const float Reduction=InteriorReduction(*Enemy,Ignore,Phase);
        if(Phase && Reduction+UE_SMALL_NUMBER<InteriorReduction(*Enemy,Ignore,false))++U.PassiveHits;
        if(bBurst){if(GameTime>=Enemy->SlowUntil)Enemy->SlowFraction=0;Enemy->SlowUntil=FMath::Max(Enemy->SlowUntil,GameTime+4+(U.DefinitionId==TEXT("R01") && Tracked?1.5:0.));
            if(U.DefinitionId==TEXT("R04") && (Enemy->State==EOVInteriorState::Operating ||
                (Enemy->DefinitionId==TEXT("M05") && Enemy->Order==EOVInteriorOrder::Sabotage && Enemy->NodeId==Enemy->TargetSlot &&
                 Operations.ContainsByPredicate([&](const auto& Op){return !Op.bCompleted && Op.Ship==Enemy->Ship &&
                     Op.SlotId==Enemy->TargetSlot && Op.ParticipantUnitIds.Contains(Enemy->UnitId);}))))
                Enemy->SuppressedUntil=GameTime+2.5;
            Enemy->SlowFraction=FMath::Max(Enemy->SlowFraction,.20f+.05f*(OVInterior::Rank(U,2)-1));}
        ApplyDamage(*Enemy,Damage*(1-Reduction),TEXT("선내 교전 사망"));
        if(U.DefinitionId==TEXT("M10") && Enemy->HP<=0){
            const bool SquadEliminated=Enemy->SquadId>0 && !Units.ContainsByPredicate([&](const auto& Member){
                return Member.SquadId==Enemy->SquadId && OVInterior::Living(Member);
            });
            if(OVInterior::Hero(*Enemy) || SquadEliminated)U.KillRallyUntil=GameTime+10;
            if(GameTime<U.RushRefundUntil){U.Skill3ReadyAt=FMath::Max(GameTime,U.Skill3ReadyAt-6);U.RushRefundUntil=0;}
        }
        return;
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
        auto& U=Units[I];
        const bool Engineer=U.DefinitionId==TEXT("M05") && U.Order==EOVInteriorOrder::Sabotage;
        if(GameTime<U.SuppressedUntil || !IsOnboard(U)||U.State==EOVInteriorState::AwaitingRecovery||U.State==EOVInteriorState::Blocked||(U.State==EOVInteriorState::Fighting && !Engineer)||
            (U.Order!=EOVInteriorOrder::Capture && U.Order!=EOVInteriorOrder::Sabotage)||U.NodeId!=U.TargetSlot)continue;
        const auto* L=U.Ship->Systems->Layout.Get();const int32 Index=L->GetSlotIndex(U.TargetSlot);if(Index==INDEX_NONE)continue;
        if(((U.LocalPosition-L->Slots[Index].LocalCenter)*U.Ship->GetActorScale3D()).Size()>Settings.ArrivalTolerance)continue;
        const auto Facility=U.Ship->Systems->GetSlotState(U.TargetSlot);
        if(L->Slots[Index].Facility==EOVFacilityType::None){U.State=EOVInteriorState::Defending;continue;}
        if(Facility.HP<=0){U.State=EOVInteriorState::Blocked;U.Reason=TEXT("목표 시설 물리 파괴 · 점령 불가");continue;}
        const int32 Owner=Facility.OccupyingFaction<0?U.Ship->Systems->FactionId:Facility.OccupyingFaction;
        bool Contested=false;for(const auto& Other:Units)if(Other.FactionId!=U.FactionId && IsOnboard(Other) && Other.Ship==U.Ship && Other.NodeId==U.NodeId){Contested=true;break;}
        if(Contested && !Engineer){U.Reason=TEXT("방 내부 적 잔류 · 점령 중단");continue;}
        if(U.Order!=EOVInteriorOrder::Sabotage && Owner==U.FactionId)
        {
            if(U.Order==EOVInteriorOrder::Capture)
            {if(auto* G=FindGraph(U.Ship.Get()))G->CompletedBy.Add(U.FactionId);U.Order=EOVInteriorOrder::Defend;U.Progress=1;}
            U.State=EOVInteriorState::Defending;U.Reason=TEXT("현장에서 아군 제어 확인 · 시설 수호");continue;
        }
        const auto Kind=U.Order==EOVInteriorOrder::Sabotage?EOVInteriorOrder::Sabotage:EOVInteriorOrder::Capture;
        int32 Op=Operations.IndexOfByPredicate([&](const auto& O){return O.Ship==U.Ship&&O.SlotId==U.TargetSlot&&O.FactionId==U.FactionId&&O.Kind==Kind;});
        if(Op==INDEX_NONE){FOVInteriorOperation O;O.Ship=U.Ship;O.SlotId=U.TargetSlot;O.FactionId=U.FactionId;O.Kind=Kind;Op=Operations.Add(O);}
        if(Operations[Op].bCompleted){Operations[Op].bCompleted=false;Operations[Op].Progress=Operations[Op].Checkpoint=0;Operations[Op].ParticipantUnitIds.Reset();}
        Workers.FindOrAdd(Op).Add(I);if(!Contested)U.State=EOVInteriorState::Operating;
    }
    for(int32 I=0;I<Operations.Num();++I)
    {
        auto& O=Operations[I];const auto* W=Workers.Find(I);
        if(!W || W->IsEmpty())
        {if(!O.bCompleted)O.Progress=O.Kind==EOVInteriorOrder::Sabotage?O.Checkpoint:0;continue;}
        if(!O.Ship.IsValid()||O.Ship->Systems->bWrecked)continue;
        const auto* L=O.Ship->Systems->Layout.Get();const int32 Slot=L->GetSlotIndex(O.SlotId);
        for(int32 Unit:*W){O.ParticipantUnitIds.AddUnique(Units[Unit].UnitId);Units[Unit].OperationContributedAt=GameTime;}
        const float Duration=O.Kind==EOVInteriorOrder::Sabotage?Settings.SabotageSeconds:L->Slots[Slot].Facility==EOVFacilityType::Bridge?Settings.BridgeSeconds:Settings.CaptureSeconds;
        float Command=0;
        for(int32 Unit:*W)if(Units[Unit].Role==EOVInteriorRole::Commander && OVInterior::Rank(Units[Unit],1)>0)
            Command+=.10f+.05f*(OVInterior::Rank(Units[Unit],1)-1);
        if(O.Kind==EOVInteriorOrder::Capture && Units.ContainsByPredicate([&](const auto& A){
            return IsOnboard(A) && A.FactionId==O.FactionId && A.Ship==O.Ship && A.NodeId==O.SlotId && GameTime<A.KillRallyUntil;}))Command+=.12f;
        if(O.Kind==EOVInteriorOrder::Capture){
            float Equipment=0; for(int32 N:*W)Equipment=FMath::Max(Equipment,Units[N].EquipmentCapture);
            Command+=Equipment;
        }
        const float Speed=FMath::Clamp(1+Settings.AdditionalWorkerBonus*(W->Num()-1)+Command,1.f,2.f);
        const bool Fighting=W->ContainsByPredicate([&](int32 N){return Units[N].State==EOVInteriorState::Fighting;});
        const float Before=O.Checkpoint;
        O.Progress=FMath::Min(1.f,O.Progress+Delta*(Fighting?.5f:1.f)*Speed/FMath::Max(.1f,Duration));
        if(O.Kind==EOVInteriorOrder::Sabotage)O.Checkpoint=FMath::Min(2.f/3,FMath::FloorToFloat(O.Progress*3)/3);
        if(O.Checkpoint>Before && W->ContainsByPredicate([&](int32 N){return Units[N].DefinitionId==TEXT("M05");}))
            for(auto& Other:Units)if(IsOnboard(Other) && Other.Ship==O.Ship && Other.NodeId==O.SlotId && Other.FactionId!=O.FactionId){
                if(GameTime>=Other.SlowUntil)Other.SlowFraction=0;
                Other.SlowUntil=FMath::Max(Other.SlowUntil,GameTime+2.5);
                Other.SlowFraction=FMath::Max(Other.SlowFraction,.25f);
            }
        for(int32 Unit:*W){Units[Unit].Progress=O.Progress;Units[Unit].Reason=FString::Printf(TEXT("%s 진행 %.0f%%"),O.Kind==EOVInteriorOrder::Sabotage?TEXT("공작"):TEXT("점령"),100*O.Progress);}
        if(O.Progress<1)continue;
        if(O.Ship->Systems->CompleteInteriorOperation(O.SlotId,O.FactionId,O.Kind==EOVInteriorOrder::Sabotage))
        {
            O.bCompleted=true;if(O.Kind==EOVInteriorOrder::Sabotage)++TotalSabotages;else ++TotalCaptures;
            if(auto* G=FindGraph(O.Ship.Get()))G->CompletedBy.Add(O.FactionId);
            for(int32 Unit:*W){Units[Unit].Order=EOVInteriorOrder::Defend;Units[Unit].State=EOVInteriorState::Defending;Units[Unit].Reason=TEXT("실제 시설 작전 완료 · 현재 방 수호");}
            Emit(O.Kind==EOVInteriorOrder::Sabotage?TEXT("Sabotage"):TEXT("Capture"),Units[(*W)[0]],TEXT("시설 작전 완료: 외부 기능에 반영"));
            Events.Last().ParticipantUnitIds=O.ParticipantUnitIds;
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
            if(Slot.HP<=0){
                const auto Type=L->Slots[Index].Facility;
                const bool Warden=(Type==EOVFacilityType::Engine || Type==EOVFacilityType::Core) &&
                    Units.ContainsByPredicate([&](const auto& A){return A.DefinitionId==TEXT("M07") && IsOnboard(A) &&
                        A.FactionId==Ship->Systems->FactionId && A.Ship==Ship && A.NodeId==Slot.SlotId;});
                G.Disasters.Add(Slot.SlotId,GameTime+Settings.DisasterSeconds*(Warden?.75f:1.f));
            }
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
            if(Units.ContainsByPredicate([&](const auto& A){return A.DefinitionId==TEXT("S01") && IsOnboard(A) &&
                !A.bDutyRequested && A.Ship==U.Ship && A.NodeId==U.NodeId && A.FactionId==U.FactionId && GameTime<A.Skill3ActiveUntil;}))Protection=FMath::Min(.65f,Protection+.1f);
            if(U.FactionId==Ship->Systems->FactionId && GetDutyBonuses(Ship).bShipDisasterField)Protection=FMath::Min(.65f,Protection+.25f);
            const int32 Room=L->GetSlotIndex(U.NodeId);
            const bool Warden=U.DefinitionId==TEXT("M07") && U.FactionId==Ship->Systems->FactionId && Room!=INDEX_NONE &&
                (L->Slots[Room].Facility==EOVFacilityType::Engine || L->Slots[Room].Facility==EOVFacilityType::Core);
            ApplyDamage(U,Settings.DisasterDamagePerSecond*Delta*(1-Protection)*(Warden?.75f:1.f),TEXT("파괴 구역 재난 사망"));
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
        for(const auto& Healer:Units)if(Healer.Role==EOVInteriorRole::Support && Healer.FactionId==Patient.FactionId && !Healer.bDutyActive && GameTime<Healer.Skill3ActiveUntil && OVInterior::Rank(Healer,2)>0 && Visible(Healer,Patient,Healer.DefinitionId==TEXT("S05")?800:600))
            Rate=FMath::Max(Rate,(.01f+.01f*OVInterior::Rank(Healer,2))*(Healer.UnitId==Patient.UnitId?1.f:ExternalAuraScale(Patient)));
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
