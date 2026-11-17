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
#endif
