#include "OVHeroSubsystem.h"
#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVHeroGrowthTest,"OrbitalVanguard.Hero.CandidateGrowthAndDuplicateAward",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FOVHeroGrowthTest::RunTest(const FString&)
{
    auto* GI=NewObject<UGameInstance>();
    auto* Roster=NewObject<UOVHeroSubsystem>(GI);
    FOVHeroRecord H;H.HeroId=FGuid::NewGuid();H.FactionId=0;H.SkillRanks={1,0,0,0};H.CooldownRemaining.Init(0,4);
    Roster->Heroes.Add(H);
    TestTrue(TEXT("참가자 승리 경험치 지급"),Roster->AwardBattleXP(H.HeroId,TEXT("battle-a"),true,0,0));
    auto Result=Roster->GetHero(H.HeroId);
    TestEqual(TEXT("후보 XP 단계 계산"),Result.Level,4);
    TestEqual(TEXT("다음 레벨 잔여 XP"),Result.XP,70);
    TestEqual(TEXT("3레벨 포인트"),Result.SkillPoints,1);
    TestFalse(TEXT("같은 작전 재지급 금지"),Roster->AwardBattleXP(H.HeroId,TEXT("battle-a"),true,720,400));
    TestEqual(TEXT("중복 지급 후 XP 불변"),Roster->GetHero(H.HeroId).XP,70);
    TestFalse(TEXT("정의되지 않은 목표 보너스 금지"),Roster->AwardBattleXP(H.HeroId,TEXT("battle-b"),true,0,250));
    for(int32 I=0;I<100 && Roster->GetHero(H.HeroId).Level<20;++I)
        Roster->AwardBattleXP(H.HeroId,FString::Printf(TEXT("candidate-%d"),I),true,720,400);
    Result=Roster->GetHero(H.HeroId);
    TestEqual(TEXT("레벨 상한"),Result.Level,20);
    TestEqual(TEXT("6개의 성장 포인트"),Result.SkillPoints,6);
    TestEqual(TEXT("15레벨 네 번째 기술 해금"),Result.SkillRanks[3],1);
    Roster->Heroes[0].Role=EOVInteriorRole::Support;Roster->Heroes[0].SkillRanks[3]=3;Roster->Heroes[0].Catalysts=1;
    TestEqual(TEXT("지원 촉매 최대 7개"),Roster->GetCatalystCapacity(H.HeroId),7);
    TestEqual(TEXT("상한 상승 시 무료 촉매 없음"),Roster->GetHero(H.HeroId).Catalysts,1);
    Roster->Heroes[0].Status=EOVHeroStatus::Dead;
    TestFalse(TEXT("사망한 영웅에게 성장 지급 금지"),Roster->AwardBattleXP(H.HeroId,TEXT("battle-c"),true,0,0));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVHeroSaveMigrationTest,"OrbitalVanguard.Hero.LegacyRosterIdentityAndLoss",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FOVHeroSaveMigrationTest::RunTest(const FString&)
{
    auto* Roster=NewObject<UOVHeroSubsystem>(NewObject<UGameInstance>());
    FOVHeroDefinition Generic;Generic.Id=TEXT("G01");Generic.Role=EOVInteriorRole::Melee;
    Roster->Definitions.Add(Generic);
    FOVHeroDefinition Named=Generic;Named.Id=TEXT("M02");Named.Passive=TEXT("고경감 경감 무시");
    Roster->Definitions.Add(Named);
    auto* Save=NewObject<UOVHeroSaveGame>();
    Save->Version=1;
    FOVHeroRecord Person;Person.HeroId=FGuid::NewGuid();Person.FactionId=0;
    Person.Role=EOVInteriorRole::Melee;Person.Name=TEXT("이리스 베인");
    Person.SkillRanks={1,0,0,0};Person.CooldownRemaining={11,0,0,0};
    Person.CurrentHP=43;Person.Status=EOVHeroStatus::RescueWaiting;Person.XP=91;
    Save->Heroes.Add(Person);
    if(!TestTrue(TEXT("기존 형식 복원"),Roster->RestoreRoster(Save)))return false;
    const auto Restored=Roster->GetHero(Person.HeroId);
    TestEqual(TEXT("옛 동명이인을 고유 영웅으로 승격하지 않음"),Restored.DefinitionId,Generic.Id);
    TestEqual(TEXT("구조 대기 유지"),Restored.Status,EOVHeroStatus::RescueWaiting);
    TestEqual(TEXT("피해 유지"),Restored.CurrentHP,43.f);
    TestEqual(TEXT("재사용 유지"),Restored.CooldownRemaining[0],11.f);
    TestEqual(TEXT("경험치 유지"),Restored.XP,91);
    Save->Version=2;Save->Heroes[0].DefinitionId=Named.Id;
    Person.HeroId=FGuid::NewGuid();Person.DefinitionId=Named.Id;Save->Heroes.Add(Person);
    TestFalse(TEXT("다른 개인 ID로 같은 고유 영웅 중복 불가"),Roster->RestoreRoster(Save));
    TestEqual(TEXT("실패 시 기존 명단 보존"),Roster->Heroes.Num(),1);
    Save->Heroes[1].FactionId=1;
    TestTrue(TEXT("다른 세력의 독립 시험 인원"),Roster->RestoreRoster(Save));
    return true;
}

#endif
