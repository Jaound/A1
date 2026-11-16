#include "OVHeroSubsystem.h"
#include "OVMissionSubsystem.h"
#include "OVCombatSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

FOVHeroRecord* UOVHeroSubsystem::FindHero(FGuid Id)
{ return Heroes.FindByPredicate([Id](const FOVHeroRecord& H){return H.HeroId==Id;}); }

FOVHeroRecord UOVHeroSubsystem::GetHero(FGuid Id) const
{
    const FOVHeroRecord* H=Heroes.FindByPredicate([Id](const FOVHeroRecord& Row){return Row.HeroId==Id;});
    return H?*H:FOVHeroRecord();
}

TArray<FOVHeroRecord> UOVHeroSubsystem::GetHeroes(int32 FactionId) const
{
    TArray<FOVHeroRecord> Result;
    for(const auto& H:Heroes) if(H.FactionId==FactionId) Result.Add(H);
    return Result;
}

bool UOVHeroSubsystem::CanChangeRoster() const
{
    const UWorld* World=GetWorld();
    if(!World) return false;
    const auto* Mission=World->GetSubsystem<UOVMissionSubsystem>();
    const auto* Interior=World->GetSubsystem<UOVInteriorSubsystem>();
    const auto* Combat=World->GetSubsystem<UOVCombatSubsystem>();
    // A running standalone interior simulation is also combat, even without a mission.
    return (!Mission || !Mission->bEnabled || Mission->Result!=EOVMissionResult::Ongoing)
        && (!Interior || !Interior->bEnabled) && (!Combat || !Combat->bEnabled);
}

FGuid UOVHeroSubsystem::CreateDevHero(int32 FactionId,EOVInteriorRole Role,const FString& Name)
{
    if(!CanChangeRoster() || FactionId<0 || Role==EOVInteriorRole::Trooper ||
        static_cast<uint8>(Role)>static_cast<uint8>(EOVInteriorRole::Commander))
    {LastMessage=TEXT("시험 영웅 생성 불가: 비전투 상태와 유효한 세력/유형이 필요합니다");return FGuid();}
    int32 Count=0;for(const auto& H:Heroes)if(H.FactionId==FactionId && H.Status!=EOVHeroStatus::Dead)++Count;
    if(Count>=30){LastMessage=TEXT("후보 로스터 한도 30명");return FGuid();}
    FOVHeroRecord H;H.HeroId=FGuid::NewGuid();H.FactionId=FactionId;H.Role=Role;
    H.Name=Name.IsEmpty()?TEXT("시험 영웅"):Name;H.SkillRanks={1,0,0,0};H.CooldownRemaining.Init(0,4);
    H.Catalysts=Role==EOVInteriorRole::Support?4:0;Heroes.Add(H);
    LastMessage=TEXT("시험 영웅 생성 · 후보 수치");return H.HeroId;
}

int32 UOVHeroSubsystem::CountDeployed(int32 FactionId) const
{int32 N=0;for(const auto& H:Heroes)if(H.FactionId==FactionId && H.bDeploymentReserved && H.Status!=EOVHeroStatus::Dead)++N;return N;}

bool UOVHeroSubsystem::SetDeployment(FGuid HeroId,bool bDeploy)
{
    FOVHeroRecord* H=FindHero(HeroId);
    if(!H || H->Status==EOVHeroStatus::Dead || H->RuntimeUnitId!=0 || !CanChangeRoster())
    {LastMessage=TEXT("배치 변경 불가: 비전투 상태의 살아 있는 영웅이 필요합니다");return false;}
    if(bDeploy && (H->Status==EOVHeroStatus::Captured || H->Status==EOVHeroStatus::Missing ||
        H->Status==EOVHeroStatus::RescueWaiting || (!H->bDeploymentReserved && CountDeployed(H->FactionId)>=15)))
    {LastMessage=TEXT("배치 불가: 구조/포로 상태 또는 세력 전체 15명 한도");return false;}
    H->bDeploymentReserved=bDeploy;
    if(H->Status==EOVHeroStatus::Reserve || H->Status==EOVHeroStatus::Deployed)
        H->Status=bDeploy?EOVHeroStatus::Deployed:EOVHeroStatus::Reserve;
    LastMessage=TEXT("영웅 편제 변경");return true;
}

bool UOVHeroSubsystem::SpendSkillPoint(FGuid HeroId,int32 SkillIndex)
{
    FOVHeroRecord* H=FindHero(HeroId);
    if(!H || H->Status==EOVHeroStatus::Dead || !CanChangeRoster() || SkillIndex<0 || SkillIndex>=4 ||
        H->SkillRanks.Num()!=4 || H->SkillPoints<=0 || H->SkillRanks[SkillIndex]<1 || H->SkillRanks[SkillIndex]>=3)
    {LastMessage=TEXT("스킬 투자 불가: 해금·포인트·랭크를 확인하세요");return false;}
    ++H->SkillRanks[SkillIndex];--H->SkillPoints;LastMessage=TEXT("스킬 랭크 상승");return true;
}

static void OVApplyHeroXP(FOVHeroRecord& Hero,int32 Amount)
{
    FOVHeroRecord* H=&Hero;H->XP+=Amount;
    while(H->Level<20)
    {
        const int32 X=H->Level-1, Required=120+40*X+10*X*X;
        if(H->XP<Required)break;
        H->XP-=Required;++H->Level;
        if(H->Level%3==0 && H->Level<=18)++H->SkillPoints;
        if(H->Level==5)H->SkillRanks[1]=1;
        if(H->Level==10)H->SkillRanks[2]=1;
        if(H->Level==15)H->SkillRanks[3]=1;
    }
}

bool UOVHeroSubsystem::GrantDevXP(FGuid HeroId,int32 Amount)
{
    auto* H=FindHero(HeroId);
    if(!CanChangeRoster() || !H || H->Status==EOVHeroStatus::Dead || H->SkillRanks.Num()!=4 || Amount<=0 || Amount>1000000)
    {LastMessage=TEXT("시험 XP 지급 불가: 비전투 영웅과 1~1000000 XP가 필요합니다");return false;}
    OVApplyHeroXP(*H,Amount);LastMessage=TEXT("개발용 시험 XP 지급 · 후보 성장값");return true;
}

int32 UOVHeroSubsystem::GetCatalystCapacity(FGuid HeroId) const
{
    const auto H=GetHero(HeroId);
    return H.HeroId.IsValid() && H.Role==EOVInteriorRole::Support ? 4+(H.SkillRanks.IsValidIndex(3)?H.SkillRanks[3]:0) : 0;
}

bool UOVHeroSubsystem::ConfigureDevSupportRepair(FGuid HeroId,FVector Cost,int32 Charges)
{
    auto* H=FindHero(HeroId);
    if(!CanChangeRoster() || !H || H->Role!=EOVInteriorRole::Support || H->Status==EOVHeroStatus::Dead ||
       Charges<0 || Charges>2 || Cost.ContainsNaN() || Cost.X<0 || Cost.Y<0 || Cost.Z<0 || Cost.IsNearlyZero())
    {LastMessage=TEXT("개발용 수리 견적과 충전량을 확인하세요");return false;}
    H->DevRepairBaseCost=Cost;H->AmplifierCharges=Charges;
    LastMessage=TEXT("개발용 수리 견적·충전 설정: 실제 경제 아님");return true;
}

bool UOVHeroSubsystem::AwardBattleXP(FGuid HeroId,const FString& BattleId,bool bVictory,float ParticipationSeconds,int32 HighestObjectiveXP)
{
    FOVHeroRecord* H=FindHero(HeroId);
    if(!H || H->Status==EOVHeroStatus::Dead || H->SkillRanks.Num()!=4 || BattleId.IsEmpty() || H->AwardedBattleIds.Contains(BattleId) ||
       !FMath::IsFinite(ParticipationSeconds) || ParticipationSeconds<0 ||
       (HighestObjectiveXP!=0 && HighestObjectiveXP!=200 && HighestObjectiveXP!=300 && HighestObjectiveXP!=400))return false;
    H->AwardedBattleIds.Add(BattleId);
    OVApplyHeroXP(*H,(bVictory?600:300)+FMath::FloorToInt(FMath::Min(ParticipationSeconds,720.f)*(100.f/60.f))+HighestObjectiveXP);
    return true;
}

bool UOVHeroSubsystem::SaveRoster(const FString& SlotName,int32 UserIndex)
{
    if(!CanChangeRoster() || SlotName.IsEmpty()) {LastMessage=TEXT("비전투 상태에서 저장 슬롯을 지정하세요");return false;}
    for(const auto& H:Heroes)if(H.RuntimeUnitId!=0 || H.RuntimePersonId!=0)
    {LastMessage=TEXT("실행 중인 영웅 연결이 있어 저장할 수 없습니다");return false;}
    auto* Save=Cast<UOVHeroSaveGame>(UGameplayStatics::CreateSaveGameObject(UOVHeroSaveGame::StaticClass()));
    if(!Save)return false;Save->Heroes=Heroes;
    const bool bSaved=UGameplayStatics::SaveGameToSlot(Save,SlotName,UserIndex);
    LastMessage=bSaved?TEXT("영웅 명단 저장"):TEXT("영웅 명단 저장 실패");return bSaved;
}

bool UOVHeroSubsystem::LoadRoster(const FString& SlotName,int32 UserIndex)
{
    if(!CanChangeRoster() || SlotName.IsEmpty()){LastMessage=TEXT("비전투 상태에서 불러오세요");return false;}
    for(const auto& H:Heroes)if(H.RuntimeUnitId!=0 || H.RuntimePersonId!=0){LastMessage=TEXT("실행 중인 영웅 연결이 있습니다");return false;}
    const auto* Save=Cast<UOVHeroSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName,UserIndex));
    if(!Save || Save->Version!=1){LastMessage=TEXT("저장 데이터가 없거나 버전이 다릅니다");return false;}
    TSet<FGuid> Ids;TMap<int32,int32> Deployed,Owned;
    for(const auto& H:Save->Heroes)
    {
        if(!H.HeroId.IsValid() || Ids.Contains(H.HeroId) || H.FactionId<0 || H.Role==EOVInteriorRole::Trooper ||
           static_cast<uint8>(H.Role)>static_cast<uint8>(EOVInteriorRole::Commander) || H.Level<1 || H.Level>20 ||
           H.SkillRanks.Num()!=4 || H.CooldownRemaining.Num()!=4 || H.RuntimeUnitId!=0 || H.RuntimePersonId!=0 ||
           static_cast<uint8>(H.Status)>static_cast<uint8>(EOVHeroStatus::Dead) || H.XP<0 || H.SkillPoints<0 ||
           H.Catalysts<0 || H.AmplifierCharges<0 || H.AmplifierCharges>2 || H.DevRepairBaseCost.ContainsNaN() ||
           H.DevRepairBaseCost.X<0 || H.DevRepairBaseCost.Y<0 || H.DevRepairBaseCost.Z<0 || !FMath::IsFinite(H.CurrentHP) || H.CurrentHP< -1.f)
        {LastMessage=TEXT("저장 데이터의 영웅 ID 또는 상태가 잘못되었습니다");return false;}
        for(int32 I=0;I<4;++I)
        {
            const int32 Unlock=I==0?1:I==1?5:I==2?10:15;
            if(H.SkillRanks[I]<(H.Level>=Unlock?1:0) || H.SkillRanks[I]>(H.Level>=Unlock?3:0) ||
                !FMath::IsFinite(H.CooldownRemaining[I]) || H.CooldownRemaining[I]<0)
            {LastMessage=TEXT("저장 데이터의 스킬 상태가 잘못되었습니다");return false;}
        }
        Ids.Add(H.HeroId);if(H.Status!=EOVHeroStatus::Dead){++Owned.FindOrAdd(H.FactionId);if(H.bDeploymentReserved)++Deployed.FindOrAdd(H.FactionId);}
    }
    for(const auto& Pair:Owned)if(Pair.Value>30){LastMessage=TEXT("로스터 한도 초과");return false;}
    for(const auto& Pair:Deployed)if(Pair.Value>15){LastMessage=TEXT("배치 한도 초과");return false;}
    Heroes=Save->Heroes;LastMessage=TEXT("영웅 명단 불러오기");return true;
}
