#include "OVHeroSubsystem.h"
#include "OVMissionSubsystem.h"
#include "OVCombatSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Engine/Texture2D.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

void UOVHeroSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    FString Text;
    TSharedPtr<FJsonObject> Root;
    const FString Path=FPaths::ProjectContentDir()/TEXT("HeroData/Roster.json");
    if(!FFileHelper::LoadFileToString(Text,*Path) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Root) || !Root.IsValid())
    {UE_LOG(LogTemp,Error,TEXT("영웅 정의 목록을 읽을 수 없습니다: %s"),*Path);return;}
    for(const auto& Row:Root->GetArrayField(TEXT("heroes")))
    {
        const auto& Json=Row->AsObject();
        FOVHeroDefinition D;
        D.Id=FName(Json->GetStringField(TEXT("id"))); D.Name=Json->GetStringField(TEXT("name"));
        D.Role=static_cast<EOVInteriorRole>(Json->GetIntegerField(TEXT("role")));
        D.Story=Json->GetStringField(TEXT("story")); D.Origin=Json->GetStringField(TEXT("origin"));
        D.Passive=Json->GetStringField(TEXT("passive")); D.Signature=Json->GetStringField(TEXT("signature"));
        auto Texture=[&](const TCHAR* Key)->UTexture2D*{
            const FString Ref=Json->GetStringField(Key);
            return Ref.IsEmpty()?nullptr:LoadObject<UTexture2D>(nullptr,*Ref);
        };
        D.Portrait=Texture(TEXT("portrait"));D.Standing=Texture(TEXT("standing"));D.SD=Texture(TEXT("sd"));
        const auto& Foot=Json->GetArrayField(TEXT("foot"));
        if(Foot.Num()==2)D.Foot=FVector2D(Foot[0]->AsNumber(),Foot[1]->AsNumber());
        D.Scale=Json->GetNumberField(TEXT("scale"));
        Definitions.Add(MoveTemp(D));
    }
}

const FOVHeroDefinition* UOVHeroSubsystem::Definition(FName Id) const
{
    return Definitions.FindByPredicate([Id](const auto& D){return D.Id==Id;});
}

FGuid UOVHeroSubsystem::CreateDefinedHero(int32 Side,FName DefinitionId)
{
    const auto* D=Definition(DefinitionId);
    if(!D || !CanChangeRoster())return {};
    if(!D->Passive.IsEmpty() && Heroes.ContainsByPredicate([&](const auto& H){return H.FactionId==Side && H.DefinitionId==D->Id;}))
    {LastMessage=TEXT("이미 명단에 있는 고유 영웅입니다");return {};}
    const FGuid Id=CreateRosterEntry(Side,D->Role,D->Name,false);
    if(auto* H=FindHero(Id))H->DefinitionId=DefinitionId;
    return Id;
}

FOVHeroRecord* UOVHeroSubsystem::FindHero(FGuid Id)
{ return Heroes.FindByPredicate([Id](const FOVHeroRecord& H){return H.HeroId==Id;}); }

FOVHeroRecord UOVHeroSubsystem::GetHero(FGuid Id) const
{
    const FOVHeroRecord* H=Heroes.FindByPredicate([Id](const FOVHeroRecord& Row){return Row.HeroId==Id;});
    return H?*H:FOVHeroRecord();
}

FString UOVHeroSubsystem::GetSkillDescription(FGuid HeroId,int32 SkillIndex) const
{
    const auto H=GetHero(HeroId);
    if(!H.HeroId.IsValid() || SkillIndex<0 || SkillIndex>=4 || H.SkillRanks.Num()!=4)return TEXT("스킬 정보가 없습니다");
    static const TCHAR* const Melee[]={
        TEXT("역장 방어: 자신 피해 경감 +20/25/30%p, 4초 · 재사용 16초 · 총 경감 65% 상한"),
        TEXT("역장 엄호: 3m 이내 다른 아군의 선내 피해 8/10/12% 감소 · 같은 효과 최고값"),
        TEXT("강화 돌입: 6/7/8m 내 적에게 이동속도 1.5배로 최대 2초 접근, 다음 공격 +20/30/40% · 재사용 30초"),
        TEXT("생존 한계 해방: HP 25% 미만에서 경감 +10/15/20%p, 3초 · 재사용 120초 · HP 40% 이상 회복 후 재발동")};
    static const TCHAR* const Ranged[]={
        TEXT("초감각 조준: 다음 기본공격 2/2.25/2.5배 · 재사용 12초"),
        TEXT("약점 감지: 개인 공격이 대상의 선내 피해 경감 8/12/16%p 무시"),
        TEXT("전자기 제압: 기본공격 대신 0.6초 동안 0.5배 피해 3발, 적 이동 20/25/30% 감소 4초 · 재사용 30초; 보직 시 연사 +15/20/25% 8초 · 재사용 60초"),
        TEXT("인지 가속: 같은 적을 4초 추적한 뒤 개인 피해 +10/15/20% · 표적 변경이나 시야 상실 시 초기화")};
    static const TCHAR* const Support[]={
        TEXT("생체 재구성: HP 성장 배율을 적용한 85/100/115 회복 · 대상 HP 50% 이하 · 촉매 1개 · 재사용 6/5.5/5초"),
        TEXT("생존장 전개: 같은 방 아군 재난 틱 피해 15/22.5/30% 감소 · 같은 효과 최고값"),
        TEXT("재생 공명장: 6m 아군 최대 HP의 2/3/4%를 초당 회복, 5초 · 촉매 2개 · 재사용 90초; 보직 수리는 개발용 견적·재료·충전 필요"),
        TEXT("촉매 동조: 개인 촉매 보유상한 +1/2/3 · 상한 증가 시 무료 충전 없음")};
    static const TCHAR* const Commander[]={
        TEXT("신경 공명: 14m 아군 피해 +12/15/18% · 같은 효과 최고값"),
        TEXT("신경 접속: 자신의 점령·공작 기여 +10/15/20% · 작업속도 2배 상한"),
        TEXT("전투 동조: 14m 아군 공격속도 +10/12.5/15%, 8초 · 재사용 60초; 보직 시 선회 +10/15/20% 10초"),
        TEXT("집단 기동 예지: 같은 방의 후퇴·방어 이동 +5/7.5/10% · 이동 버프 20% 상한")};
    const TCHAR* const* Rows=H.Role==EOVInteriorRole::Melee?Melee:H.Role==EOVInteriorRole::Ranged?Ranged:H.Role==EOVInteriorRole::Support?Support:Commander;
    const int32 Unlock=SkillIndex==0?1:SkillIndex==1?5:SkillIndex==2?10:15;
    return FString::Printf(TEXT("%s · %s · 현재 랭크 %d/3 · 후보 수치"),Rows[SkillIndex],H.Level>=Unlock?TEXT("해금됨"):*FString::Printf(TEXT("레벨 %d 해금"),Unlock),H.SkillRanks[SkillIndex]);
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
    if(!CanChangeRoster()){LastMessage=TEXT("시험 영웅 생성은 비전투 상태에서 가능합니다");return {};}
    return CreateRosterEntry(FactionId,Role,Name,false);
}

FGuid UOVHeroSubsystem::CreateRosterEntry(int32 FactionId,EOVInteriorRole Role,const FString& Name,bool bDeploy)
{
    if(FactionId<0 || (bDeploy && CountDeployed(FactionId)>=DeploymentLimit(FactionId)) || Role==EOVInteriorRole::Trooper ||
        static_cast<uint8>(Role)>static_cast<uint8>(EOVInteriorRole::Commander))
    {LastMessage=TEXT("시험 영웅 생성 불가: 비전투 상태와 유효한 세력/유형이 필요합니다");return FGuid();}
    int32 Count=0;for(const auto& H:Heroes)if(H.FactionId==FactionId && H.Status!=EOVHeroStatus::Dead)++Count;
    if(Count>=30){LastMessage=TEXT("후보 로스터 한도 30명");return FGuid();}
    FOVHeroRecord H;H.HeroId=FGuid::NewGuid();H.FactionId=FactionId;H.Role=Role;
    H.DefinitionId=FName(*FString::Printf(TEXT("G%02d"),static_cast<int32>(Role)));
    H.Name=Name.IsEmpty()?TEXT("시험 영웅"):Name;H.SkillRanks={1,0,0,0};H.CooldownRemaining.Init(0,4);
    H.bDeploymentReserved=bDeploy;H.Status=bDeploy?EOVHeroStatus::Deployed:EOVHeroStatus::Reserve;
    H.Catalysts=Role==EOVInteriorRole::Support?4:0;Heroes.Add(H);
    LastMessage=TEXT("시험 영웅 생성 · 후보 수치");return H.HeroId;
}

int32 UOVHeroSubsystem::DeploymentLimit(int32 Faction) const
{
    const int32* Tier=CommandTiers.Find(Faction);
    const int32 Limits[]={3,6,10,15};
    return Limits[Tier?FMath::Clamp(*Tier,1,4)-1:3];
}

bool UOVHeroSubsystem::SetTrialCommandTier(int32 Faction,int32 Tier)
{
    const int32 Limits[]={3,6,10,15};
    if(!CanChangeRoster() || Faction<0 || Tier<1 || Tier>4 || CountDeployed(Faction)>Limits[Tier-1])return false;
    CommandTiers.Add(Faction,Tier);return true;
}

int32 UOVHeroSubsystem::CountDeployed(int32 FactionId) const
{int32 N=0;for(const auto& H:Heroes)if(H.FactionId==FactionId && H.bDeploymentReserved && H.Status!=EOVHeroStatus::Dead)++N;return N;}

bool UOVHeroSubsystem::SetDeployment(FGuid HeroId,bool bDeploy)
{
    FOVHeroRecord* H=FindHero(HeroId);
    if(!H || H->Status==EOVHeroStatus::Dead || H->RuntimeUnitId!=0 || !CanChangeRoster())
    {LastMessage=TEXT("배치 변경 불가: 비전투 상태의 살아 있는 영웅이 필요합니다");return false;}
    if(bDeploy && (H->Status==EOVHeroStatus::Captured || H->Status==EOVHeroStatus::Missing ||
        H->Status==EOVHeroStatus::RescueWaiting || (!H->bDeploymentReserved && CountDeployed(H->FactionId)>=DeploymentLimit(H->FactionId))))
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

void UOVHeroSubsystem::SetArenaLevel(FGuid Id,int32 Level)
{
    auto* H=FindHero(Id);
    if(!H)return;
    while(H->Level<FMath::Clamp(Level,1,20)){
        const int32 X=H->Level-1;
        OVApplyHeroXP(*H,120+40*X+10*X*X-H->XP);
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
    if(!Save)return false;Save->Heroes=Heroes;Save->CommandTiers=CommandTiers;
    const bool bSaved=UGameplayStatics::SaveGameToSlot(Save,SlotName,UserIndex);
    LastMessage=bSaved?TEXT("영웅 명단 저장"):TEXT("영웅 명단 저장 실패");return bSaved;
}

bool UOVHeroSubsystem::LoadRoster(const FString& SlotName,int32 UserIndex)
{
    if(!CanChangeRoster() || SlotName.IsEmpty()){LastMessage=TEXT("비전투 상태에서 불러오세요");return false;}
    for(const auto& H:Heroes)if(H.RuntimeUnitId!=0 || H.RuntimePersonId!=0){LastMessage=TEXT("실행 중인 영웅 연결이 있습니다");return false;}
    const auto* Save=Cast<UOVHeroSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName,UserIndex));
    if(!Save || (Save->Version!=1 && Save->Version!=2)){LastMessage=TEXT("저장 데이터가 없거나 버전이 다릅니다");return false;}
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
    const int32 Limits[]={3,6,10,15};
    for(const auto& Pair:Save->CommandTiers)if(Pair.Key<0 || Pair.Value<1 || Pair.Value>4 || Deployed.FindRef(Pair.Key)>Limits[Pair.Value-1])return false;
    for(const auto& Pair:Deployed)if(Pair.Value>15){LastMessage=TEXT("배치 한도 초과");return false;}
    auto Restored=Save->Heroes;
    for(auto& H:Restored)
    {
        if(H.FocusTier<0 || H.FocusTier>4 || H.ArmorTier<0 || H.ArmorTier>4 || H.TacticalTier<0 || H.TacticalTier>4 ||
           (H.TacticalTier && H.TacticalMode!=TEXT("Move") && H.TacticalMode!=TEXT("Heal") && H.TacticalMode!=TEXT("Capture")))return false;
        if(H.DefinitionId.IsNone())H.DefinitionId=FName(*FString::Printf(TEXT("G%02d"),static_cast<int32>(H.Role)));
        const auto* D=Definition(H.DefinitionId);
        if(!D || D->Role!=H.Role){LastMessage=TEXT("영웅 정의와 저장된 유형이 일치하지 않습니다");return false;}
    }
    CommandTiers=Save->CommandTiers;Heroes=MoveTemp(Restored);LastMessage=TEXT("영웅 명단 불러오기");return true;
}
