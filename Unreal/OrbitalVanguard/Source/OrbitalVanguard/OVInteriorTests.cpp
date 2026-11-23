#include "OVInteriorSubsystem.h"
#include "OVBattleNotices.h"
#include "OVTransportSubsystem.h"
#include "OVCombatSubsystem.h"
#include "OVLogisticsSubsystem.h"
#include "OVShip.h"
#include "OVShipSystemsComponent.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace OVInteriorTest
{
struct FRange
{
    UWorld* World;
    UOVInteriorSubsystem* Interior;
    UOVTransportSubsystem* Transport;
    UOVCombatSubsystem* Combat;
    AOVShip* Origin;
    AOVShip* Target;
    FRange()
    {
        World=UWorld::CreateWorld(EWorldType::Game,false);
        Interior=World->GetSubsystem<UOVInteriorSubsystem>();Transport=World->GetSubsystem<UOVTransportSubsystem>();Combat=World->GetSubsystem<UOVCombatSubsystem>();
        Origin=Make(0,FVector(-5000,0,0),3);Target=Make(1,FVector::ZeroVector,3);
        Combat->EnableCombat({Origin,Target},230117);Transport->ResetScenario({Origin,Target});Interior->ResetScenario({Origin,Target},230117);
        Target->Systems->Shield=0;Target->Systems->bShieldActive=false;
    }
    ~FRange(){World->DestroyWorld(false);}
    AOVShip* Make(int32 Faction,FVector Position,int32 Decks)
    {auto* S=World->SpawnActor<AOVShip>();S->Systems->FactionId=Faction;S->Systems->Initialize(UShipLayoutDataAsset::MakeTestLayout(S,Decks));S->SetActorLocation(Position);S->DebugResetMotion();return S;}
    AOVShip* Shuttle()
    {
        // Existing transport fixture: actual unarmed small hull, one PD, no free pods.
        auto* S=Make(0,FVector(-5000,2000,0),1);
        auto* L=UShipLayoutDataAsset::MakeTestLayout(S,1);
        L->WeaponMounts.Reset();L->CannonSlots=L->BeamSlots=L->MissileSlots=0;
        FOVWeaponMountDefinition PD;PD.WeaponId=TEXT("PD1");PD.Type=EOVWeaponType::Interception;L->WeaponMounts.Add(PD);
        S->Systems->Initialize(L);Combat->RegisterShip(S);
        Transport->ResetScenario({Origin,Target,S});Interior->ResetScenario({Origin,Target,S},230117);
        Transport->RegisterShuttle(S,Origin);return S;
    }
    FName Slot(EOVFacilityType Type)const
    {for(const auto& S:Target->Systems->Layout->Slots)if(S.Facility==Type)return S.SlotId;return NAME_None;}
    void Advance(float Seconds,float Chunk=1.f/60)
    {float Left=Seconds;while(Left>1.e-5f){const float D=FMath::Min(Left,Chunk);Transport->Simulate(D);Interior->Simulate(D);Left-=D;}}
    TArray<int32> Enter(int32 Heroes=1,int32 Troops=0)
    {auto Ids=Interior->RegisterBoardingTeam(Origin,0,Heroes,Troops);Transport->LaunchPods(Origin,Target);Advance(3);return Ids;}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVInteriorHostileBoardingNotice,"OrbitalVanguard.Interior.HostileBoardingNotice",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FOVInteriorHostileBoardingNotice::RunTest(const FString&)
{
    OVInteriorTest::FRange R;
    R.Origin->Systems->Shield=0;R.Origin->Systems->bShieldActive=false;
    const auto Enemy=R.Interior->RegisterBoardingTeam(R.Target,1,1,0);
    auto* Feed=R.World->GetSubsystem<UOVBattleNotices>();
    TestFalse(TEXT("출발 전 침입 경보 없음"),Feed->Notices.ContainsByPredicate([](const FOVBattleNotice& N){return N.Kind==EOVBattleNoticeKind::HostileBoarding;}));
    TestTrue(TEXT("실제 적 운송 인원 등록"),Enemy.Num()>0);
    TestTrue(TEXT("실제 적 포드 발사"),R.Transport->LaunchPods(R.Target,R.Origin)>0);
    R.Advance(3);
    const auto* Notice=Feed->Notices.FindByPredicate([](const FOVBattleNotice& N){return N.Kind==EOVBattleNoticeKind::HostileBoarding;});
    TestTrue(TEXT("실제 아군 함선 진입에만 위급 경보"),Notice&&Notice->bCritical&&Notice->ShipId==R.Origin->GetFName());
    const int32 Count=Feed->Notices.Num();R.Advance(1);
    TestEqual(TEXT("머무르는 동안 침입 경보 반복 없음"),Feed->Notices.Num(),Count);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVInteriorHeroHitNotice,"OrbitalVanguard.Interior.HeroHitNotice",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FOVInteriorHeroHitNotice::RunTest(const FString&)
{
    OVInteriorTest::FRange R;const FName Room=R.Origin->Systems->Layout->CoreSlotId;
    const int32 Hero=R.Interior->RegisterUnit(R.Origin,0,EOVInteriorRole::Ranged,Room);
    const int32 Enemy=R.Interior->RegisterUnit(R.Origin,1,EOVInteriorRole::Trooper,Room);
    for(auto& U:R.Interior->Units) {
        if(U.UnitId==Hero){U.DefinitionId=TEXT("R04");U.HP=U.MaxHP=10000;U.Damage=0;U.Evasion=0;}
        if(U.UnitId==Enemy){U.Damage=10;U.Evasion=0;}
    }
    R.Advance(5);
    auto* Feed=R.World->GetSubsystem<UOVBattleNotices>();
    const auto* Hit=Feed->Notices.FindByPredicate([Hero](const FOVBattleNotice& N){return N.Kind==EOVBattleNoticeKind::HeroHit&&N.UnitId==Hero;});
    TestTrue(TEXT("실제 선내 공격 피해로 아군 영웅 알림"),R.Interior->GetUnit(Hero).HP<10000&&Hit!=nullptr);
    TestTrue(TEXT("알림은 실제 아군 영웅 정의를 기억"),Hit && Hit->FriendlyHeroDefinition==TEXT("R04"));
    TestTrue(TEXT("같은 영웅의 반복 피격은 묶임"),Hit&&Hit->Count>1);
    TestFalse(TEXT("적 일반병의 피격은 아군 영웅으로 표시하지 않음"),Feed->Notices.ContainsByPredicate([Enemy](const FOVBattleNotice& N){return N.Kind==EOVBattleNoticeKind::HeroHit&&N.UnitId==Enemy;}));
    const int32 Other=R.Interior->RegisterUnit(R.Origin,0,EOVInteriorRole::Melee,Room);
    const int32 PreviousCount=Feed->Notices.Num();
    Feed->RecordInteriorEvent(TEXT("Damage"),R.Interior->GetUnit(Other),R.Interior->GameTime);
    TestTrue(TEXT("합성 피격 사건에서 서로 다른 영웅은 묶이지 않음"),Other>0&&Feed->Notices.Num()==PreviousCount+1&&Feed->Notices.Last().UnitId==Other&&Feed->Notices.Last().Count==1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVInteriorOperationContributors,"OrbitalVanguard.Interior.ActualOperationContributors",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FOVInteriorOperationContributors::RunTest(const FString&)
{
    OVInteriorTest::FRange R;
    const FName Slot=R.Slot(EOVFacilityType::ShieldControl);
    const int32 Worker=R.Interior->RegisterUnit(R.Target,0,EOVInteriorRole::Melee,Slot);
    const int32 Bystander=R.Interior->RegisterUnit(R.Target,0,EOVInteriorRole::Support,Slot);
    TestTrue(TEXT("작업자와 비참가자 생성"),Worker>0 && Bystander>0);
    R.Interior->IssueOrder({Bystander},EOVInteriorOrder::None);
    R.Advance(2);
    TestTrue(TEXT("적 시설 수호 대기는 점령 작전을 만들지 않음"),R.Interior->Operations.IsEmpty());
    TestTrue(TEXT("작업 명령"),R.Interior->IssueOrder({Worker},EOVInteriorOrder::Capture,Slot));
    R.Advance(2);
    const auto* Started=R.Interior->Operations.FindByPredicate([Slot](const FOVInteriorOperation& O){return O.SlotId==Slot && O.FactionId==0;});
    TestTrue(TEXT("명시적 점령은 진행됨"),Started && Started->Progress>0);
    R.Interior->IssueOrder({Worker},EOVInteriorOrder::Defend,Slot);R.Advance(.2f);
    const auto* Stopped=R.Interior->Operations.FindByPredicate([Slot](const FOVInteriorOperation& O){return O.SlotId==Slot && O.FactionId==0;});
    TestTrue(TEXT("수호 전환은 점령 진행을 중단"),Stopped && FMath::IsNearlyZero(Stopped->Progress));
    TestTrue(TEXT("점령 명령 재개"),R.Interior->IssueOrder({Worker},EOVInteriorOrder::Capture,Slot));
    R.Advance(25);
    const auto* Event=R.Interior->Events.FindByPredicate([Slot](const FOVInteriorEvent& E){return E.Kind==TEXT("Capture") && E.SlotId==Slot;});
    TestNotNull(TEXT("실제 점령 완료 사건"),Event);
    if(Event)
    {
        TestTrue(TEXT("실제 작업자 기록"),Event->ParticipantUnitIds.Contains(Worker));
        TestFalse(TEXT("같은 방 대기자는 기여하지 않음"),Event->ParticipantUnitIds.Contains(Bystander));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVInteriorSupportFiniteSkills,"OrbitalVanguard.Interior.SupportFiniteSkills",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FOVInteriorSupportFiniteSkills::RunTest(const FString&)
{
    OVInteriorTest::FRange R;
    const int32 Healer=R.Interior->RegisterUnit(R.Origin,0,EOVInteriorRole::Support);
    const int32 Patient=R.Interior->RegisterUnit(R.Origin,0,EOVInteriorRole::Melee);
    auto* H=R.Interior->Units.FindByPredicate([Healer](const auto& U){return U.UnitId==Healer;});
    auto* P=R.Interior->Units.FindByPredicate([Patient](const auto& U){return U.UnitId==Patient;});
    TestTrue(TEXT("실제 선내 영웅 생성"),H && P);
    if(!H || !P)return false;
    H->HeroSkillRanks={1,1,1,1};P->HP=P->MaxHP*.2f;
    R.Advance(.2f);
    const auto After=R.Interior->GetUnit(Healer);
    TestEqual(TEXT("단일 회복과 공명장에 촉매 3개 사용"),After.Medkits,1);
    TestTrue(TEXT("두 재사용 대기시간 각각 설정"),After.SkillReadyAt>R.Interior->GameTime && After.Skill3ReadyAt>R.Interior->GameTime);
    TestTrue(TEXT("실제 생존 대상 HP 회복"),R.Interior->GetUnit(Patient).HP>P->MaxHP*.2f);
    R.Advance(1);
    TestEqual(TEXT("공명장 유지 중 자동 무료 충전 없음"),R.Interior->GetUnit(Healer).Medkits,1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVInteriorMeleeAdvancedSkills,"OrbitalVanguard.Interior.MeleeAdvancedSkills",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FOVInteriorMeleeAdvancedSkills::RunTest(const FString&)
{
    OVInteriorTest::FRange R;
    const int32 Hero=R.Interior->RegisterUnit(R.Origin,0,EOVInteriorRole::Melee);
    const int32 Enemy=R.Interior->RegisterUnit(R.Origin,1,EOVInteriorRole::Trooper);
    auto* H=R.Interior->Units.FindByPredicate([Hero](const auto& U){return U.UnitId==Hero;});
    auto* E=R.Interior->Units.FindByPredicate([Enemy](const auto& U){return U.UnitId==Enemy;});
    if(!TestNotNull(TEXT("근거리 영웅"),H) || !TestNotNull(TEXT("상대"),E))return false;
    H->HeroSkillRanks={1,1,1,1};H->HP=H->MaxHP*.26f;H->LocalPosition+=FVector(400,0,0);
    E->HP=E->MaxHP=10000;
    R.Advance(3);
    const auto After=R.Interior->GetUnit(Hero);
    TestTrue(TEXT("강화 돌입 이동과 재사용 대기"),After.DistanceTravelled>0 && After.Skill3ReadyAt>R.Interior->GameTime);
    TestTrue(TEXT("25% 미만 생존 한계 발동"),After.Skill4ReadyAt>R.Interior->GameTime && !After.bSkill4Armed);
    TestTrue(TEXT("실제 적에게 공격 피해"),R.Interior->GetUnit(Enemy).HP<10000);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVInteriorRangedAdvancedSkills,"OrbitalVanguard.Interior.RangedAdvancedSkills",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FOVInteriorRangedAdvancedSkills::RunTest(const FString&)
{
    OVInteriorTest::FRange R;
    const int32 Hero=R.Interior->RegisterUnit(R.Origin,0,EOVInteriorRole::Ranged);
    const int32 Enemy=R.Interior->RegisterUnit(R.Origin,1,EOVInteriorRole::Trooper);
    auto* H=R.Interior->Units.FindByPredicate([Hero](const auto& U){return U.UnitId==Hero;});
    auto* E=R.Interior->Units.FindByPredicate([Enemy](const auto& U){return U.UnitId==Enemy;});
    if(!TestNotNull(TEXT("원거리 영웅"),H) || !TestNotNull(TEXT("상대"),E))return false;
    H->HeroSkillRanks={1,1,1,1};E->HP=E->MaxHP=10000;E->Damage=0;E->Evasion=0;
    R.Advance(5);
    const auto After=R.Interior->GetUnit(Hero),Target=R.Interior->GetUnit(Enemy);
    TestTrue(TEXT("제압 3발과 재사용 대기"),After.Shots>=3 && After.Skill3ReadyAt>R.Interior->GameTime);
    TestTrue(TEXT("제압 명중 시 대상 감속 이력"),Target.SlowUntil>0 && Target.SlowFraction>=.2f);
    TestTrue(TEXT("동일 적 4초 추적"),After.TrackedEnemy==Enemy && R.Interior->GameTime-After.TrackedSince>=4);
    TestTrue(TEXT("실제 사격 피해"),Target.HP<10000);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVInteriorCommanderAdvancedSkills,"OrbitalVanguard.Interior.CommanderAdvancedSkills",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FOVInteriorCommanderAdvancedSkills::RunTest(const FString&)
{
    OVInteriorTest::FRange R;
    const FName Slot=R.Slot(EOVFacilityType::ShieldControl);
    const int32 Hero=R.Interior->RegisterUnit(R.Target,0,EOVInteriorRole::Commander,Slot);
    auto* H=R.Interior->Units.FindByPredicate([Hero](const auto& U){return U.UnitId==Hero;});
    if(!TestNotNull(TEXT("지휘 영웅"),H))return false;
    H->HeroSkillRanks={1,1,1,1};
    R.Interior->IssueOrder({Hero},EOVInteriorOrder::Capture,Slot);R.Advance(6);
    const auto* Op=R.Interior->Operations.FindByPredicate([Slot](const auto& O){return O.SlotId==Slot;});
    TestNotNull(TEXT("실제 점령 진행"),Op);
    if(Op)TestTrue(TEXT("신경 접속 10% 작업 기여"),Op->Progress>6.f/18.f+.02f);
    const int32 Enemy=R.Interior->RegisterUnit(R.Target,1,EOVInteriorRole::Trooper,Slot);
    auto* E=R.Interior->Units.FindByPredicate([Enemy](const auto& U){return U.UnitId==Enemy;});
    if(E){E->HP=E->MaxHP=10000;E->Damage=0;}
    R.Advance(.5f);
    const auto After=R.Interior->GetUnit(Hero);
    TestTrue(TEXT("전투 동조 활성 및 재사용 대기"),After.Skill3ActiveUntil>R.Interior->GameTime && After.Skill3ReadyAt>R.Interior->GameTime);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVInteriorPassiveAuraSkills,"OrbitalVanguard.Interior.PassiveAuraSkills",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FOVInteriorPassiveAuraSkills::RunTest(const FString&)
{
    auto TargetHP=[](bool bCover)
    {
        OVInteriorTest::FRange R;
        const int32 Target=R.Interior->RegisterUnit(R.Origin,0,EOVInteriorRole::Trooper);
        const int32 Cover=R.Interior->RegisterUnit(R.Origin,0,EOVInteriorRole::Melee);
        const int32 Enemy=R.Interior->RegisterUnit(R.Origin,1,EOVInteriorRole::Trooper);
        for(auto& U:R.Interior->Units)
        {if(U.UnitId==Cover)U.HeroSkillRanks={1,bCover?1:0,0,0};if(U.UnitId==Enemy)U.HP=U.MaxHP=10000;}
        R.Advance(3);return R.Interior->GetUnit(Target).HP;
    };
    const float Uncovered=TargetHP(false),Covered=TargetHP(true);
    TestTrue(TEXT("근거리 엄호는 다른 아군의 실제 교전 피해를 감소"),Covered>Uncovered);
    auto Travel=[](bool bAura)
    {
        OVInteriorTest::FRange R;
        const int32 Commander=R.Interior->RegisterUnit(R.Origin,0,EOVInteriorRole::Commander);
        const int32 Ally=R.Interior->RegisterUnit(R.Origin,0,EOVInteriorRole::Trooper);
        for(auto& U:R.Interior->Units)if(U.UnitId==Commander)U.HeroSkillRanks={1,0,0,bAura?1:0};
        FName Destination=NAME_None;
        for(const auto& Slot:R.Origin->Systems->Layout->Slots)if(Slot.Facility==EOVFacilityType::LifeSupport){Destination=Slot.SlotId;break;}
        R.Interior->IssueOrder({Ally},EOVInteriorOrder::Defend,Destination);R.Advance(.5f);
        return R.Interior->GetUnit(Ally).DistanceTravelled;
    };
    const float Plain=Travel(false),Boosted=Travel(true);
    TestTrue(TEXT("지휘 기동 예지는 방어 이동의 실제 거리를 증가"),Plain>0 && Boosted>Plain);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVInteriorSupportDutyResources,"OrbitalVanguard.Interior.SupportDutyFiniteRepair",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FOVInteriorSupportDutyResources::RunTest(const FString&)
{
    OVInteriorTest::FRange R;
    auto* Logistics=R.World->GetSubsystem<UOVLogisticsSubsystem>();
    const int32 Id=R.Interior->RegisterUnit(R.Origin,0,EOVInteriorRole::Support);
    auto* H=R.Interior->Units.FindByPredicate([Id](const auto& U){return U.UnitId==Id;});
    if(!TestNotNull(TEXT("지원 영웅"),H))return false;
    H->HeroId=FGuid::NewGuid();H->HeroSkillRanks={1,1,1,1};H->AmplifierCharges=2;H->DevRepairBaseCost=FVector(1,0,0);
    R.Origin->Systems->HullHP=R.Origin->Systems->MaxHullHP*.5f;
    TestTrue(TEXT("유한 선내 자재 등록"),Logistics->RegisterDevShip(R.Origin,EOVDevLogisticsRole::Transport,200,100));
    const FName Slot=R.Slot(EOVFacilityType::LifeSupport);
    TestTrue(TEXT("지원 보직 지시"),R.Interior->SetHeroDuty(Id,true,Slot));
    for(int32 I=0;I<1200 && R.Interior->GetUnit(Id).Skill3ReadyAt<=0;++I)
    {R.Interior->Simulate(1.f/60);Logistics->Simulate(1.f/60);}
    auto After=R.Interior->GetUnit(Id);
    TestEqual(TEXT("수리 시작 성공 시에만 충전 1회 차감"),After.AmplifierCharges,1);
    TestTrue(TEXT("실제 유한 수리 링크"),Logistics->RepairLinks.ContainsByPredicate([Id,&R](const auto& L){return L.bHero && L.HeroId==R.Interior->GetUnit(Id).HeroId;}));
    const float BeforeHP=R.Origin->Systems->HullHP;
    for(int32 I=0;I<60;++I){R.Interior->Simulate(1.f/60);Logistics->Simulate(1.f/60);}
    TestTrue(TEXT("선체만 실제 회복"),R.Origin->Systems->HullHP>BeforeHP);
    const auto* Cargo=Logistics->Cargo.FindByPredicate([&R](const auto& C){return C.Ship==R.Origin;});
    TestTrue(TEXT("회복분의 실제 자재 소모"),Cargo && Cargo->Amount<100);
    R.Origin->Systems->DebugDestroySlot(Slot,EOVDamageCause::External);
    R.Interior->Simulate(1.f/60);Logistics->Simulate(1.f/60);
    TestTrue(TEXT("보직 시설 파괴 시 수리 링크 해제"),Logistics->RepairLinks.IsEmpty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVInteriorGraphAndRoster,"OrbitalVanguard.Interior.GraphVersionAndFactionRoster",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FOVInteriorGraphAndRoster::RunTest(const FString&)
{
    OVInteriorTest::FRange R;
    for(int32 Decks=1;Decks<=3;++Decks){auto* S=R.Make(0,FVector(0,Decks*1000,0),Decks);TArray<FString> Errors;TestTrue(TEXT("임시 1/2/3층 내부는 연결됨"),R.Interior->ValidateInterior(S,Errors));}
    for(int32 I=0;I<15;++I)TestTrue(TEXT("세력 전체 15명까지 안정적인 ID 배치"),R.Interior->RegisterUnit(R.Origin,0,EOVInteriorRole::Melee)>0);
    R.Interior->Settings.MaxActiveHeroesPerFaction=100;
    TestEqual(TEXT("16번째 영웅은 등록 거부"),R.Interior->RegisterUnit(R.Origin,0,EOVInteriorRole::Ranged),0);
    TestEqual(TEXT("일반 병력은 영웅 한도와 별도"),R.Interior->CountActiveHeroes(0),15);
    R.Interior->RegisterUnit(R.Origin,0,EOVInteriorRole::Trooper);
    TestEqual(TEXT("수비병 자동 차출 금지"),R.Transport->LaunchPods(R.Origin,R.Target),0);
    const int32 Id=R.Interior->Units[0].UnitId;
    R.Origin->Systems->Layout->LayoutVersion=TEXT("ReplacedLayout");R.Interior->Simulate(.1f);
    TestEqual(TEXT("배치 버전 교체시 임의 슬롯 이동 금지"),R.Interior->GetUnit(Id).State,EOVInteriorState::Blocked);
    R.Origin->Systems->HullHP=0;R.Origin->Systems->RefreshDerivedState();R.Advance(.1f);
    TestEqual(TEXT("내부 완파는 전원 영구 사망"),R.Interior->TotalDeaths,16);
    auto* Feed=R.World->GetSubsystem<UOVBattleNotices>();
    const int32 HeroDeaths=Feed->Notices.FilterByPredicate([](const FOVBattleNotice& N){return N.Kind==EOVBattleNoticeKind::HeroDeath;}).Num();
    TestEqual(TEXT("영웅 전사 알림만 기록하고 일반병은 영웅으로 표시하지 않음"),HeroDeaths,15);
    R.Advance(1);TestEqual(TEXT("사망 사건은 중복되지 않음"),R.Interior->TotalDeaths,16);
    TestEqual(TEXT("반복 처리시 영웅 전사 알림 중복 없음"),Feed->Notices.FilterByPredicate([](const FOVBattleNotice& N){return N.Kind==EOVBattleNoticeKind::HeroDeath;}).Num(),HeroDeaths);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVInteriorPresenceSources,"OrbitalVanguard.Interior.ImmediateProtectionAndMixedPresenceSources",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FOVInteriorPresenceSources::RunTest(const FString&)
{
    OVInteriorTest::FRange R;
    const int32 Hidden=R.Interior->RegisterUnit(R.Target,1,EOVInteriorRole::Commander,R.Target->Systems->Layout->CoreSlotId);
    const auto Ids=R.Interior->RegisterBoardingTeam(R.Origin,0,1,0);
    TestFalse(TEXT("진입 전 적 내부 구조 숨김"),R.Interior->IsStructureKnown(0,R.Target));
    TestFalse(TEXT("진입 전 적 영웅을 전지적으로 공개하지 않음"),R.Interior->CanObserve(0,Hidden));
    R.Transport->LaunchPods(R.Origin,R.Target);R.Transport->Simulate(.32f);
    TestTrue(TEXT("선내 다음 스텝 전 실제 도착 순간 보호 활성"),R.Target->Systems->HasInteriorPresence(0));
    R.Interior->Simulate(.32f);
    TestTrue(TEXT("진입 후 구조 공개"),R.Interior->IsStructureKnown(0,R.Target));
    TestFalse(TEXT("구조 공개가 시야 밖 적 영웅 공개를 뜻하지 않음"),R.Interior->CanObserve(0,Hidden));
    const auto Dev=R.Transport->AddDevPassengers(R.Origin,0,1);
    TestEqual(TEXT("두 번째 팀의 조기 출발 차단"),R.Transport->LaunchPods(R.Origin,R.Target),0);
    R.Transport->Simulate(30);
    TestEqual(TEXT("간격 경과 후 DEV 팀 출발"),R.Transport->LaunchPods(R.Origin,R.Target),1);R.Advance(.5f);
    R.Transport->MarkRuntimePersonDead(R.Interior->GetUnit(Ids[0]).PersonId,Ids[0]);R.Advance(.1f);
    TestTrue(TEXT("실제 인원 사망 뒤 명시 DEV 잔류만 보호 유지"),R.Target->Systems->HasInteriorPresence(0));
    TestTrue(TEXT("실제 인원이 없으면 DEV 출처 표시 복귀"),R.Target->Systems->IsInteriorDeveloperInput(0));
    R.Transport->DevReturnPersonToEntry(Dev[0]);R.Advance(2);
    TestFalse(TEXT("마지막 DEV 이탈 뒤 runtime baseline 유령 보호 없음"),R.Target->Systems->HasInteriorPresence(0));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVInteriorEntryOperationReturn,"OrbitalVanguard.Interior.ActualEntryCaptureAndOriginalPodReturn",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FOVInteriorEntryOperationReturn::RunTest(const FString&)
{
    OVInteriorTest::FRange R;const auto Ids=R.Enter();const auto Before=R.Interior->GetUnit(Ids[0]);const auto Person=R.Transport->GetPerson(Before.PersonId);
    auto* Feed=R.World->GetSubsystem<UOVBattleNotices>();
    TestTrue(TEXT("실제 강습 진입 알림"),Feed->Notices.ContainsByPredicate([](const FOVBattleNotice& N){return N.Kind==EOVBattleNoticeKind::BoardingEntry;}));
    const int32 BeforeEnemy=Feed->Notices.Num();auto EnemyEvent=Before;EnemyEvent.FactionId=1;
    Feed->RecordInteriorEvent(TEXT("Entry"),EnemyEvent,R.Interior->GameTime);
    TestEqual(TEXT("비공개 적 선내 사건을 알림으로 누설하지 않음"),Feed->Notices.Num(),BeforeEnemy);
    TestTrue(TEXT("실제 유닛 진입이 세력 보호 활성"),R.Target->Systems->HasInteriorPresence(0));
    TestFalse(TEXT("실제 유닛 보호를 DEV로 표시하지 않음"),R.Target->Systems->IsInteriorDeveloperInput(0));
    TestFalse(TEXT("DEV 복귀 버튼으로 실내 이동 우회 금지"),R.Transport->DevReturnPersonToEntry(Person.PersonId));
    TestFalse(TEXT("진입점 좌표를 전달해도 실제 철수 명령·위치 없이는 거부"),R.Transport->ReturnRuntimePersonToEntry(Person.PersonId,Ids[0],R.Target,Person.EntryLocalPoint,Person.EntryLayoutVersion));
    const FName Shield=R.Slot(EOVFacilityType::ShieldControl);const auto Facility=R.Target->Systems->GetSlotState(Shield);
    R.Interior->IssueOrder(Ids,EOVInteriorOrder::Capture,Shield);R.Advance(.1f);
    TestTrue(TEXT("목표 지시만으로 즉시 점령하지 않음"),R.Target->Systems->GetSlotState(Shield).OccupyingFaction!=0);
    R.Advance(25);
    TestEqual(TEXT("실제 이동·작업 후 시설 점령"),R.Target->Systems->GetSlotState(Shield).OccupyingFaction,0);
    TestTrue(TEXT("실제 시설 장악 알림"),Feed->Notices.ContainsByPredicate([](const FOVBattleNotice& N){return N.Kind==EOVBattleNoticeKind::FacilityCaptured;}));
    TestEqual(TEXT("점령은 물리 HP를 복원하거나 파괴하지 않음"),R.Target->Systems->GetSlotState(Shield).HP,Facility.HP);
    TestEqual(TEXT("운영 승무원과 강습 병력 분리"),R.Target->Systems->GetSlotState(Shield).Crew,Facility.Crew);
    TestTrue(TEXT("통로를 실제 이동함"),R.Interior->GetUnit(Ids[0]).DistanceTravelled>0);
    TestTrue(TEXT("실제 점령 완료만 목적 완료 표시"),R.Target->Systems->IsInteriorObjectiveComplete(0));
    const int32 Captures=R.Interior->TotalCaptures;
    R.Interior->IssueOrder(Ids,EOVInteriorOrder::Capture,Shield);R.Advance(.1f);
    TestTrue(TEXT("이미 아군 시설도 현장 확인 뒤 목표 완료 복구"),R.Target->Systems->IsInteriorObjectiveComplete(0));
    TestEqual(TEXT("같은 아군 시설 재지정은 점령 사건을 복제하지 않음"),R.Interior->TotalCaptures,Captures);
    R.Interior->IssueOrder(Ids,EOVInteriorOrder::Capture,R.Slot(EOVFacilityType::Bridge));
    TestFalse(TEXT("새 목표 변경시 이전 목적 완료 상태 제거"),R.Target->Systems->IsInteriorObjectiveComplete(0));
    R.Interior->IssueOrder(Ids,EOVInteriorOrder::Retreat);R.Advance(10);
    const auto After=R.Transport->GetPerson(Person.PersonId);
    TestEqual(TEXT("같은 인물이 원 포드로 귀환"),After.State,EOVPassengerState::Aboard);
    TestEqual(TEXT("귀환에 포드를 새로 만들지 않음"),After.PodId,Person.PodId);
    TestFalse(TEXT("마지막 인원 이탈시 보호 해제"),R.Target->Systems->HasInteriorPresence(0));
    TestTrue(TEXT("실제 원 운송수단 귀환 알림"),Feed->Notices.ContainsByPredicate([](const FOVBattleNotice& N){return N.Kind==EOVBattleNoticeKind::BoardingReturn;}));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVInteriorAbilities,"OrbitalVanguard.Interior.FourRolesCombatAndFiniteHealing",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FOVInteriorAbilities::RunTest(const FString&)
{
    OVInteriorTest::FRange R;const FName Room=R.Origin->Systems->Layout->CoreSlotId;
    const int32 Melee=R.Interior->RegisterUnit(R.Origin,0,EOVInteriorRole::Melee,Room);
    const int32 Ranged=R.Interior->RegisterUnit(R.Origin,0,EOVInteriorRole::Ranged,Room);
    const int32 Support=R.Interior->RegisterUnit(R.Origin,0,EOVInteriorRole::Support,Room);
    R.Interior->RegisterUnit(R.Origin,0,EOVInteriorRole::Commander,Room);
    const int32 Enemy=R.Interior->RegisterUnit(R.Origin,1,EOVInteriorRole::Trooper,Room);
    for(auto& U:R.Interior->Units)if(U.UnitId==Enemy){U.HP=U.MaxHP=100000;U.Damage=0;}
    R.Advance(18);
    TestTrue(TEXT("근거리 역장 능력 실제 발동"),R.Interior->GetUnit(Melee).SkillUses>=2);
    TestTrue(TEXT("원거리 강화 공격 실제 발동"),R.Interior->GetUnit(Ranged).SkillUses>=2);
    TestTrue(TEXT("공격이 실제 적 HP를 감소시킴"),R.Interior->GetUnit(Enemy).HP<100000);
    for(int32 I=0;I<4;++I){for(auto& U:R.Interior->Units)if(U.UnitId==Melee)U.HP=40;R.Advance(7);}
    TestEqual(TEXT("지원 촉매는 유한한 4개"),R.Interior->GetUnit(Support).Medkits,0);
    for(auto& U:R.Interior->Units)if(U.UnitId==Melee)U.HP=20;
    R.Advance(7);TestEqual(TEXT("소진 뒤 무료 회복 없음"),R.Interior->GetUnit(Melee).HP,20.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVInteriorCaptureRetake,"OrbitalVanguard.Interior.BridgeCapturePhysicalRetakeAndDestruction",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FOVInteriorCaptureRetake::RunTest(const FString&)
{
    OVInteriorTest::FRange R;R.Interior->Settings.SightRange=0;const FName Bridge=R.Slot(EOVFacilityType::Bridge);
    const int32 Defender=R.Interior->RegisterUnit(R.Target,1,EOVInteriorRole::Melee,R.Target->Systems->Layout->CoreSlotId);
    R.Interior->IssueOrder({Defender},EOVInteriorOrder::None);
    const auto Attackers=R.Enter();R.Interior->IssueOrder(Attackers,EOVInteriorOrder::Capture,Bridge);R.Advance(43);
    TestEqual(TEXT("실제 함교 점령은 소유권 이전"),R.Target->Systems->FactionId,0);
    TestEqual(TEXT("잔존 수비 영웅 소속은 원래 세력"),R.Interior->GetUnit(Defender).FactionId,1);
    R.Interior->IssueOrder(Attackers,EOVInteriorOrder::Retreat);R.Advance(90);
    TestEqual(TEXT("함교를 비워도 타이머로 자동 반환 없음"),R.Target->Systems->FactionId,0);
    R.Interior->IssueOrder({Defender},EOVInteriorOrder::Capture,Bridge);R.Advance(43);
    TestEqual(TEXT("잔존 적의 실제 이동·재점령으로 탈환"),R.Target->Systems->FactionId,1);
    TestEqual(TEXT("두 번의 점령만 집계"),R.Target->Systems->CaptureCount,2);
    R.Target->Systems->DebugDestroySlot(Bridge,EOVDamageCause::External);R.Advance(.1f);
    TestTrue(TEXT("나포 이력의 함교 파괴는 중립 정지"),R.Target->Systems->bNeutral);
    TestFalse(TEXT("물리 파괴 함교는 다시 점령 불가"),R.Target->Systems->CompleteInteriorOperation(Bridge,0,false));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVInteriorSabotageCheckpoint,"OrbitalVanguard.Interior.SabotageCheckpointAndDamagePath",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FOVInteriorSabotageCheckpoint::RunTest(const FString&)
{
    OVInteriorTest::FRange R;const auto Ids=R.Enter();const FName Radar=R.Slot(EOVFacilityType::Radar);
    const auto Armor=R.Target->Systems->ArmorSections;
    R.Interior->IssueOrder(Ids,EOVInteriorOrder::Sabotage,Radar);R.Advance(13);
    R.Interior->IssueOrder(Ids,EOVInteriorOrder::None);R.Advance(.1f);
    const auto* Work=R.Interior->Operations.FindByPredicate([&](const auto& O){return O.SlotId==Radar;});
    TestNotNull(TEXT("진행중 실제 공작 기록 존재"),Work);
    if(Work)TestTrue(TEXT("중단 공작은 1/3 체크포인트 보존"),FMath::IsNearlyEqual(Work->Progress,1.f/3,.001f));
    R.Interior->IssueOrder(Ids,EOVInteriorOrder::Sabotage,Radar);R.Advance(22);
    TestEqual(TEXT("공작 완료시 물리 시설 파괴"),R.Target->Systems->GetSlotState(Radar).HP,0.f);
    for(int32 I=0;I<Armor.Num();++I)TestEqual(TEXT("정밀 공작은 외장 장갑 보존"),R.Target->Systems->ArmorSections[I].HP,Armor[I].HP);
    const float Hull=R.Target->Systems->HullHP;R.Advance(5);TestEqual(TEXT("표시·대기로 피해 재실행하지 않음"),R.Target->Systems->HullHP,Hull);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVInteriorClockBlocked,"OrbitalVanguard.Interior.FixedClockBlockedRouteAndOrders",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FOVInteriorClockBlocked::RunTest(const FString&)
{
    TArray<float> FinalProgress;
    for(int32 Speed=1;Speed<=3;++Speed)
    {
        OVInteriorTest::FRange R;const auto Ids=R.Enter();const FName Shield=R.Slot(EOVFacilityType::ShieldControl);
        R.Interior->IssueOrder(Ids,EOVInteriorOrder::Capture,Shield);
        const double Before=R.Interior->GameTime;R.Interior->Simulate(0);TestEqual(TEXT("정지시 내부 시계 불변"),R.Interior->GameTime,Before);
        R.Advance(12,Speed/60.f);FinalProgress.Add(R.Interior->GetUnit(Ids[0]).Progress);
    }
    TestTrue(TEXT("동일 게임시간 1/2/3배속 작전 일치"),FMath::IsNearlyEqual(FinalProgress[0],FinalProgress[1],.002f)&&FMath::IsNearlyEqual(FinalProgress[0],FinalProgress[2],.002f));
    OVInteriorTest::FRange R;const auto Ids=R.Enter();const auto U=R.Interior->GetUnit(Ids[0]);
    for(const auto& C:R.Target->Systems->Layout->Connections)if(C.FromSlot==U.NodeId||C.ToSlot==U.NodeId)R.Interior->SetConnectionBlocked(R.Target,C.ConnectionId,true);
    R.Interior->IssueOrder(Ids,EOVInteriorOrder::Capture,R.Slot(EOVFacilityType::Bridge));R.Advance(1);
    TestEqual(TEXT("경로 상실시 무단 순간이동 대신 차단"),R.Interior->GetUnit(Ids[0]).State,EOVInteriorState::Blocked);
    for(const auto& C:R.Target->Systems->Layout->Connections)R.Interior->SetConnectionBlocked(R.Target,C.ConnectionId,false);
    R.Interior->IssueOrder(Ids,EOVInteriorOrder::Retreat);R.Advance(8);
    TestEqual(TEXT("차단 해제·명령 변경 후 실제 철수 복구"),R.Transport->GetPerson(U.PersonId).State,EOVPassengerState::Aboard);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVInteriorRuntimeShuttleLoss,"OrbitalVanguard.Interior.RuntimeShuttleCaptureWreckAndDeparture",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FOVInteriorRuntimeShuttleLoss::RunTest(const FString&)
{
    for(int32 Case=0;Case<3;++Case)
    {
        OVInteriorTest::FRange R;auto* Shuttle=R.Shuttle();
        const auto Units=R.Interior->RegisterBoardingTeam(R.Origin,0,1,1);
        const auto People=R.Interior->GetPassengerIds(Units);
        TestEqual(TEXT("실제 영웅과 일반 병력 각각 1명 등록"),Units.Num(),2);
        if(Units.Num()!=2)return false;
        TestTrue(TEXT("실제 인원만 원 강습선에 탑승"),R.Transport->DispatchShuttle(Shuttle,R.Target,People));
        R.Advance(.1f);
        for(int32 Id:Units)TestEqual(TEXT("출발 중 실제 유닛은 운송 상태"),R.Interior->GetUnit(Id).State,EOVInteriorState::InTransit);
        TestTrue(TEXT("수동 운송 중지에서도 손실 판정 유지"),R.Transport->SuspendShuttle(Shuttle));
        if(Case==0)Shuttle->Systems->FactionId=2;
        else if(Case==1){Shuttle->Systems->HullHP=0;Shuttle->Systems->RefreshDerivedState();}
        else R.Combat->NotifyFTLDeparture(Shuttle);
        R.Advance(.1f);
        TestEqual(TEXT("나포/완파/이탈한 원 강습선 회수 경로는 상실"),R.Transport->Shuttles[0].State,EOVShuttleState::Lost);
        auto* Feed=R.World->GetSubsystem<UOVBattleNotices>();
        const int32 RescueNotices=Feed->Notices.FilterByPredicate([](const FOVBattleNotice& N){return N.Kind==EOVBattleNoticeKind::HeroRescue;}).Num();
        TestEqual(TEXT("실제 강습선 완파만 영웅 구조 대기 알림"),RescueNotices,Case==1?1:0);
        TestFalse(TEXT("상실한 강습선의 회수 성공을 꾸미지 않음"),R.Transport->RecallShuttle(Shuttle));
        for(int32 I=0;I<Units.Num();++I)
        {
            const auto U=R.Interior->GetUnit(Units[I]);const auto P=R.Transport->GetPerson(People[I]);
            TestEqual(TEXT("손실 전후 실제 유닛 ID 보존"),U.UnitId,Units[I]);
            TestEqual(TEXT("손실 전후 실제 인원 ID 보존"),U.PersonId,People[I]);
            TestEqual(TEXT("운송 인원과 실제 유닛 연결 보존"),P.RuntimeUnitId,Units[I]);
            TestEqual(TEXT("함선 소유권이 승객 소속을 바꾸지 않음"),U.FactionId,0);
            TestEqual(TEXT("운송 승객 소속도 원 세력 보존"),P.FactionId,0);
            TestTrue(TEXT("원 강습선 기록 보존"),P.Shuttle==Shuttle);
            TestEqual(TEXT("강습선 손실에 포드를 무료 생성하지 않음"),P.PodId,0);
            if(Case==1)
            {
                TestEqual(TEXT("강습선 완파: 실제 영웅 구조 대기/일반병 사망"),P.State,I==0?EOVPassengerState::RescueWaiting:EOVPassengerState::Dead);
                TestEqual(TEXT("실제 유닛도 운송 손실 결과와 일치"),U.State,I==0?EOVInteriorState::RescueWaiting:EOVInteriorState::Dead);
            }
            else
            {
                TestEqual(TEXT("나포/FTL 미정 후속은 회수 불가 대기로 보존"),P.State,EOVPassengerState::AwaitingRecovery);
                TestEqual(TEXT("미정 포로/성계 여행을 죽음이나 귀환으로 성공 처리하지 않음"),U.State,EOVInteriorState::AwaitingRecovery);
                TestEqual(TEXT("나포/이탈은 인물 HP를 소급 변경하지 않음"),U.HP,U.MaxHP);
            }
        }
        const int32 Deaths=Case==1?1:0;
        TestEqual(TEXT("영웅 구조 대기는 사망 집계와 분리"),R.Interior->TotalDeaths,Deaths);
        R.Advance(2);
        TestEqual(TEXT("반복 처리에서도 사망 1회만 집계"),R.Interior->TotalDeaths,Deaths);
        TestEqual(TEXT("강습선 손실 처리 반복에도 구조 알림 중복 없음"),Feed->Notices.FilterByPredicate([](const FOVBattleNotice& N){return N.Kind==EOVBattleNoticeKind::HeroRescue;}).Num(),RescueNotices);
        TestEqual(TEXT("손실 뒤 무료 인원 생성/삭제 없음"),R.Transport->Persons.Num(),2);
        TestEqual(TEXT("손실 뒤 무료 포드 생성 없음"),R.Transport->Pods.Num(),0);
        TestEqual(TEXT("구조/회수 대기 영웅은 세력 배치 한도 점유"),R.Interior->CountActiveHeroes(0),1);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVInteriorPodRescueNotice,"OrbitalVanguard.Interior.RealPodRescueNotice",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FOVInteriorPodRescueNotice::RunTest(const FString&)
{
    OVInteriorTest::FRange R;const auto Units=R.Interior->RegisterBoardingTeam(R.Origin,0,1,1);
    TestEqual(TEXT("영웅과 일반 병력 각각 등록"),Units.Num(),2);
    if(Units.Num()!=2)return false;
    TestEqual(TEXT("실제 승객별 포드 발사"),R.Transport->LaunchPods(R.Origin,R.Target),2);
    R.Advance(.02f); // Test interception while the first pod is in flight, before its arrival.
    const auto Hero=R.Interior->GetUnit(Units[0]);
    const auto Person=R.Transport->GetPerson(Hero.PersonId);
    const auto* Pod=R.Transport->Pods.FindByPredicate([&](const FOVPodState& P){return P.PersonId==Person.PersonId;});
    if(!TestNotNull(TEXT("실제 영웅 원 포드"),Pod))return false;
    const int32 PodId=Pod->PodId;const FVector LastPosition=Pod->Position;
    TestTrue(TEXT("비행 중 포드 실제 격추 처리"),R.Transport->InterceptPod(PodId,10000));
    auto* Feed=R.World->GetSubsystem<UOVBattleNotices>();
    const auto* Notice=Feed->Notices.FindByPredicate([&](const FOVBattleNotice& N){return N.Kind==EOVBattleNoticeKind::HeroRescue&&N.UnitId==Hero.UnitId;});
    TestTrue(TEXT("마지막 포드 위치에 영웅 구조 대기 경보"),Notice&&Notice->bCritical&&Notice->LastPosition.Equals(LastPosition,.01));
    TestFalse(TEXT("파괴 포드 중복 처리 금지"),R.Transport->InterceptPod(PodId,10000));
    R.Advance(.2f);
    TestEqual(TEXT("유닛은 실제 구조 대기"),R.Interior->GetUnit(Hero.UnitId).State,EOVInteriorState::RescueWaiting);
    TestEqual(TEXT("포드 및 유닛 처리에서도 구조 알림은 한 번"),Feed->Notices.FilterByPredicate([](const FOVBattleNotice& N){return N.Kind==EOVBattleNoticeKind::HeroRescue;}).Num(),1);
    OVInteriorTest::FRange Stranded;
    const auto Survivor=Stranded.Interior->RegisterBoardingTeam(Stranded.Origin,0,1,0);
    Stranded.Transport->LaunchPods(Stranded.Origin,Stranded.Target);Stranded.Advance(.02f);
    Stranded.Combat->NotifyFTLDeparture(Stranded.Target);Stranded.Origin->Systems->bWrecked=true;
    Stranded.Advance(.1f);
    auto* StrandedFeed=Stranded.World->GetSubsystem<UOVBattleNotices>();
    TestTrue(TEXT("귀환 목적지 상실은 실제 영웅 구조 대기 알림"),Survivor.Num()==1&&Stranded.Interior->GetUnit(Survivor[0]).State==EOVInteriorState::RescueWaiting&&StrandedFeed->Notices.ContainsByPredicate([](const FOVBattleNotice& N){return N.Kind==EOVBattleNoticeKind::HeroRescue;}));
    const int32 StrandedCount=StrandedFeed->Notices.Num();Stranded.Advance(.5f);
    TestEqual(TEXT("표류 상태 반복 갱신으로 알림을 복제하지 않음"),StrandedFeed->Notices.Num(),StrandedCount);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVInteriorResidentDeparture,"OrbitalVanguard.Interior.RuntimeResidentDepartureSuspendsAndPreserves",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FOVInteriorResidentDeparture::RunTest(const FString&)
{
    OVInteriorTest::FRange R;const auto Ids=R.Enter(1,1);const FName Shield=R.Slot(EOVFacilityType::ShieldControl);
    R.Interior->IssueOrder(Ids,EOVInteriorOrder::Capture,Shield);R.Advance(5);
    const int32 OccupyingFactionBefore=R.Target->Systems->GetSlotState(Shield).OccupyingFaction;
    TArray<FOVInteriorUnitRecord> Before;TArray<int32> PodIds;
    for(int32 Id:Ids){Before.Add(R.Interior->GetUnit(Id));PodIds.Add(R.Transport->GetPerson(Before.Last().PersonId).PodId);}
    TestTrue(TEXT("이탈 전에 실제 내부 점령이 진행 중"),R.Interior->GetUnit(Ids[0]).Progress>0);
    R.Combat->NotifyFTLDeparture(R.Target);R.Advance(40);
    TestEqual(TEXT("성계 밖 타이머만으로 점령 성공시키지 않음"),R.Interior->TotalCaptures,0);
    TestEqual(TEXT("이탈한 적 시설 소유권 보존"),R.Target->Systems->GetSlotState(Shield).OccupyingFaction,OccupyingFactionBefore);
    for(int32 I=0;I<Ids.Num();++I)
    {
        const auto U=R.Interior->GetUnit(Ids[I]);const auto P=R.Transport->GetPerson(U.PersonId);
        TestEqual(TEXT("내부 잔류 목표 FTL은 실제 인물 기록 보존"),P.State,EOVPassengerState::InsideTarget);
        TestEqual(TEXT("후속 성계 작전 미지원 상태를 명시"),U.State,EOVInteriorState::Blocked);
        TestEqual(TEXT("FTL 이탈로 인물 피해를 만들지 않음"),U.HP,Before[I].HP);
        TestEqual(TEXT("이탈 뒤 선내 이동/전투를 몰래 진행하지 않음"),U.DistanceTravelled,Before[I].DistanceTravelled);
        TestEqual(TEXT("이탈 뒤 원 포드 ID 보존"),P.PodId,PodIds[I]);
        TestEqual(TEXT("이탈 뒤 실제 인원 ID 보존"),U.PersonId,Before[I].PersonId);
        TestEqual(TEXT("이탈 뒤 영웅/병력 소속 보존"),U.FactionId,0);
    }
    TestTrue(TEXT("아직 내부 잔류 인원이며 가짜 이탈 이벤트 없음"),R.Target->Systems->HasInteriorPresence(0));
    R.Interior->IssueOrder(Ids,EOVInteriorOrder::Retreat);R.Advance(5);
    TestEqual(TEXT("이탈 목표에서 무료 포드 귀환 성공 처리 금지"),R.Transport->CountState(EOVPassengerState::Aboard),0);
    TestEqual(TEXT("기존 포드 외 새 운송수단 없음"),R.Transport->Pods.Num(),2);
    R.Target->Systems->HullHP=0;R.Target->Systems->RefreshDerivedState();R.Advance(.1f);
    for(int32 Id:Ids)TestEqual(TEXT("이탈 뒤에도 실제 내부 완파는 영웅/병력 모두 사망"),R.Interior->GetUnit(Id).State,EOVInteriorState::Dead);
    TestEqual(TEXT("내부 완파는 우주 구조 예외 없이 두 사망"),R.Interior->TotalDeaths,2);
    R.Advance(1);TestEqual(TEXT("FTL/완파 반복 처리 사망 중복 없음"),R.Interior->TotalDeaths,2);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVIrisFieldTest,"OrbitalVanguard.Hero.IrisPhaseDuel",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FOVIrisFieldTest::RunTest(const FString&)
{
    OVInteriorTest::FRange R;
    const auto Ids=R.Enter();
    if(!TestEqual(TEXT("실제 강습 인원"),Ids.Num(),1))return false;
    const auto Arrival=R.Interior->GetUnit(Ids[0]);
    const int32 Guard=R.Interior->RegisterUnit(R.Target,1,EOVInteriorRole::Melee,Arrival.NodeId);
    if(!TestTrue(TEXT("도착 격실 수호대"),Guard>0))return false;
    for(auto& U:R.Interior->Units)
    {
        if(U.UnitId==Ids[0])U.DefinitionId=TEXT("M02");
        if(U.UnitId==Guard){U.LocalPosition=Arrival.LocalPosition;U.Armor=.3f;}
    }
    const float Hull=R.Target->Systems->HullHP;
    R.Advance(.25f);
    TestEqual(TEXT("역장 종료 전 방출 금지"),R.Interior->GetUnit(Ids[0]).SignatureUses,0);
    R.Advance(R.Interior->Settings.MeleeDuration+.25f);
    const auto After=R.Interior->GetUnit(Ids[0]);
    TestEqual(TEXT("역장 종료 한 번 방출"),After.SignatureUses,1);
    TestTrue(TEXT("고경감 실제 표적 기본 공격"),After.PassiveHits>0);
    TestTrue(TEXT("수호대 실제 피해"),R.Interior->GetUnit(Guard).HP<R.Interior->GetUnit(Guard).MaxHP);
    R.Advance(.25f);
    TestEqual(TEXT("방출 중복 없음"),R.Interior->GetUnit(Ids[0]).SignatureUses,1);
    TestEqual(TEXT("개인 검 선체 피해 없음"),R.Target->Systems->HullHP,Hull);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVOfficerIdentityTest,"OrbitalVanguard.Hero.NamedDutyLifecycle",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FOVOfficerIdentityTest::RunTest(const FString&)
{
    OVInteriorTest::FRange R;
    const FName Room=R.Slot(EOVFacilityType::WeaponControl);
    const int32 Id=R.Interior->RegisterUnit(R.Target,1,EOVInteriorRole::Ranged,Room);
    if(!TestTrue(TEXT("실제 포술 보직자 등록"),Id>0))return false;
    auto& Officer=R.Interior->Units.Last();
    Officer.DefinitionId=TEXT("R02");Officer.HeroSkillRanks={1,1,1,0};
    if(!TestTrue(TEXT("보직 인계 지시"),R.Interior->SetHeroDuty(Id,true,Room)))return false;
    R.Advance(3.4f);
    const auto Pd=R.Interior->GetDutyBonuses(R.Target);
    TestTrue(TEXT("숙련 보직 발동 시 요격 주기와 명중"),Pd.PDInterval<.9f && Pd.PDAccuracy>0);
    TestEqual(TEXT("반대 세력 요격 보정 없음"),R.Interior->GetDutyBonuses(R.Origin).PDAccuracy,0.f);
    TestTrue(TEXT("보직 해제"),R.Interior->SetHeroDuty(Id,false,NAME_None));
    TestEqual(TEXT("해제 즉시 기본 주기로 복원"),R.Interior->GetDutyBonuses(R.Target).PDInterval,1.f);
    Officer.DefinitionId=TEXT("R05");
    R.Interior->SetHeroDuty(Id,true,Room); R.Advance(3.4f);
    TestTrue(TEXT("다른 장교의 미사일 전용 효과"),R.Interior->GetDutyBonuses(R.Target).MissileInterval<1);
    TestEqual(TEXT("옛 요격 보정 미잔류"),R.Interior->GetDutyBonuses(R.Target).PDAccuracy,0.f);
    R.Target->Systems->DebugDestroySlot(Room,EOVDamageCause::Sabotage);
    TestEqual(TEXT("보직 시설 파괴 즉시 효과 소멸"),R.Interior->GetDutyBonuses(R.Target).MissileInterval,1.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVNamedOfficersTest,"OrbitalVanguard.Hero.OfficerBonusesFollowRoomAndFaction",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FOVNamedOfficersTest::RunTest(const FString&)
{
    using Check=bool(*)(const FOVHeroDutyBonuses&);
    struct FCase { const TCHAR* Id; EOVInteriorRole Role; Check Applies; };
    const FCase Cases[]={
        {TEXT("R01"),EOVInteriorRole::Ranged,[](const auto& B){return B.bOpeningAim;}},
        {TEXT("R03"),EOVInteriorRole::Ranged,[](const auto& B){return B.bLongAim;}},
        {TEXT("R07"),EOVInteriorRole::Ranged,[](const auto& B){return B.BoltSpeedScale>1 && B.WingPeriodScale<1;}},
        {TEXT("R09"),EOVInteriorRole::Ranged,[](const auto& B){return B.bNearTraverse;}},
        {TEXT("R10"),EOVInteriorRole::Ranged,[](const auto& B){return B.bExposedAim;}},
        {TEXT("S02"),EOVInteriorRole::Support,[](const auto& B){return B.bEngineering;}},
        {TEXT("S03"),EOVInteriorRole::Support,[](const auto& B){return B.bShieldHarmonizer;}},
        {TEXT("S04"),EOVInteriorRole::Support,[](const auto& B){return B.ProductionScale<1 && B.DockScale<1;}},
        {TEXT("C02"),EOVInteriorRole::Commander,[](const auto& B){return B.LateralScale>1 && B.AccelerationScale>1;}},
        {TEXT("C04"),EOVInteriorRole::Commander,[](const auto& B){return B.ReformScale<1 && B.ReturnScale>1;}},
        {TEXT("C05"),EOVInteriorRole::Commander,[](const auto& B){return B.PodScale>1 && B.ShuttleDockScale<1;}}
    };
    for(const auto& Candidate:Cases)
    {
        OVInteriorTest::FRange Range;
        Range.Target->Systems->Layout->bAllowAircraftOperations=true;
        Range.Target->Systems->Layout->HullSizeClass=EOVHullSizeClass::Large;
        const auto Facility=Candidate.Role==EOVInteriorRole::Commander?EOVFacilityType::Bridge:
            Candidate.Role==EOVInteriorRole::Ranged?EOVFacilityType::WeaponControl:EOVFacilityType::Engine;
        const FName Room=Range.Slot(Facility);
        const int32 Id=Range.Interior->RegisterUnit(Range.Target,1,Candidate.Role,Room);
        if(!TestTrue(Candidate.Id,Id>0))return false;
        auto& Unit=Range.Interior->Units.Last();
        Unit.DefinitionId=Candidate.Id; Unit.HeroSkillRanks={1,1,1,0};
        TestFalse(FString(Candidate.Id)+TEXT(" 보직 전"),Candidate.Applies(Range.Interior->GetDutyBonuses(Range.Target)));
        TestTrue(Candidate.Id,Range.Interior->SetHeroDuty(Id,true,Room));
        Range.Advance(3.5f);
        TestTrue(FString(Candidate.Id)+TEXT(" 인계 후"),Candidate.Applies(Range.Interior->GetDutyBonuses(Range.Target)));
        TestFalse(FString(Candidate.Id)+TEXT(" 상대편"),Candidate.Applies(Range.Interior->GetDutyBonuses(Range.Origin)));
        Range.Target->Systems->DebugDestroySlot(Room,EOVDamageCause::Sabotage);
        TestFalse(FString(Candidate.Id)+TEXT(" 파괴 후"),Candidate.Applies(Range.Interior->GetDutyBonuses(Range.Target)));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVNamedWardAndHealingTest,"OrbitalVanguard.Hero.NamedBoardingGuardsAndFiniteHealing",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FOVNamedWardAndHealingTest::RunTest(const FString&)
{
    // Each run uses the same patient, position, clock and seed.
    auto Heal=[this](FName Id){
        OVInteriorTest::FRange R;
        const FName Room=R.Slot(EOVFacilityType::Engine);
        const int32 Medic=R.Interior->RegisterUnit(R.Target,1,EOVInteriorRole::Support,Room);
        R.Interior->Units.Last().DefinitionId=Id;
        R.Interior->Units.Last().Medkits=1;
        const int32 Patient=R.Interior->RegisterUnit(R.Target,1,EOVInteriorRole::Melee,Room);
        R.Interior->Units.Last().HP=10;
        R.Advance(.15f);
        const auto State=R.Interior->GetUnit(Medic);
        TestEqual(Id.ToString()+TEXT(" 촉매 1개 차감"),State.Medkits,0);
        return FVector2D(R.Interior->GetUnit(Patient).HP,State.SkillReadyAt);
    };
    const auto Common=Heal(TEXT("G03")), Lifeline=Heal(TEXT("S01")), Surgeon=Heal(TEXT("S05"));
    TestTrue(TEXT("저체력 환자에게만 추가 회복"),Lifeline.X>Common.X && FMath::IsNearlyEqual(Common.X,Surgeon.X));
    TestTrue(TEXT("의무관 재사용 1초 단축"),FMath::IsNearlyEqual(Common.Y-Surgeon.Y,1.0));
    OVInteriorTest::FRange R;
    const auto Raiders=R.Enter();
    if(Raiders.IsEmpty())return false;
    const auto Arrival=R.Interior->GetUnit(Raiders[0]);
    const int32 Sentinel=R.Interior->RegisterUnit(R.Target,1,EOVInteriorRole::Melee,Arrival.NodeId);
    R.Interior->Units.Last().DefinitionId=TEXT("M03");
    R.Interior->IssueOrder({Sentinel},EOVInteriorOrder::Defend,Arrival.NodeId);
    R.Advance(.1);
    const double First=R.Interior->GetUnit(Raiders[0]).GuardSlowUntil;
    TestTrue(TEXT("수호자가 실제 진입한 적에게 감속"),First>R.Interior->GameTime+1.5);
    R.Advance(.5);
    TestEqual(TEXT("격실 안에 있는 동안 감속 무한 연장 방지"),R.Interior->GetUnit(Raiders[0]).GuardSlowUntil,First);
    const int32 Jammer=R.Interior->RegisterUnit(R.Target,1,EOVInteriorRole::Melee,Arrival.NodeId);
    R.Interior->Units.Last().DefinitionId=TEXT("M06");
    const int32 Gunner=R.Interior->RegisterUnit(R.Target,0,EOVInteriorRole::Ranged,Arrival.NodeId);
    R.Advance(.2);
    TestTrue(TEXT("근접 역장이 실제 적 사수를 교란"),R.Interior->GetUnit(Gunner).JamUntil>R.Interior->GameTime);
    TestTrue(TEXT("역장 사용자는 아군 교란 없음"),R.Interior->GetUnit(Jammer).JamUntil<=R.Interior->GameTime);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVNamedOperationsTest,"OrbitalVanguard.Hero.NamedFacilityOperationsAndRelay",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FOVNamedOperationsTest::RunTest(const FString&)
{
    auto Sabotage=[](FName Definition,bool Defended){
        OVInteriorTest::FRange R;
        const FName Engine=R.Slot(EOVFacilityType::Engine);
        const int32 Raider=R.Interior->RegisterUnit(R.Target,0,EOVInteriorRole::Melee,Engine);
        auto& U=R.Interior->Units.Last();U.DefinitionId=Definition;U.Damage=0;
        if(Defended){
            R.Interior->RegisterUnit(R.Target,1,EOVInteriorRole::Melee,Engine);
            R.Interior->Units.Last().Damage=0;
        }
        R.Interior->IssueOrder({Raider},EOVInteriorOrder::Sabotage,Engine);
        R.Advance(5);
        return R.Interior->GetUnit(Raider).Progress;
    };
    const float Full=Sabotage(TEXT("M05"),false),Half=Sabotage(TEXT("M05"),true);
    TestTrue(TEXT("교전 중 공작 절반 유지"),Full>0 && Half>Full*.46f && Half<Full*.54f);
    TestEqual(TEXT("일반 강습원 교전 중 공작 정지"),Sabotage(TEXT("G01"),true),0.f);
    auto Disaster=[](FName Definition){
        OVInteriorTest::FRange R;
        const FName Engine=R.Slot(EOVFacilityType::Engine);
        const int32 Id=R.Interior->RegisterUnit(R.Target,1,EOVInteriorRole::Melee,Engine);
        auto& U=R.Interior->Units.Last();U.DefinitionId=Definition;U.HP=U.MaxHP=10000;
        R.Target->Systems->DebugDestroySlot(Engine,EOVDamageCause::Sabotage);
        R.Advance(16);
        const float Before=R.Interior->GetUnit(Id).HP;
        R.Advance(2);
        return FVector2D(10000-Before,Before-R.Interior->GetUnit(Id).HP);
    };
    const auto Normal=Disaster(TEXT("G01")),Warden=Disaster(TEXT("M07"));
    TestTrue(TEXT("재난 피해 완화 및 15초 종료"),Warden.X>0 && Warden.X<Normal.X && Warden.Y==0 && Normal.Y>0);
    OVInteriorTest::FRange R;
    auto* Major=R.Make(1,FVector(9000,0,0),3);
    Major->Systems->Layout->HullSizeClass=EOVHullSizeClass::Large;R.Combat->RegisterShip(Major);
    const auto Bridge=R.Slot(EOVFacilityType::Bridge);
    const int32 Id=R.Interior->RegisterUnit(R.Target,1,EOVInteriorRole::Commander,Bridge);
    auto& Officer=R.Interior->Units.Last();Officer.DefinitionId=TEXT("C03");Officer.HeroSkillRanks={1,1,1,0};
    R.Interior->SetHeroDuty(Id,true,Bridge);R.Advance(3.5f);
    TestTrue(TEXT("아군 공유 보정과 근접 주력함 선회"),R.Interior->FormationRadarBonus(Major)>0 && R.Interior->GetDutyBonuses(Major).TurnBonus>0);
    TestEqual(TEXT("적 세력 격리"),R.Interior->FormationRadarBonus(R.Origin),0.f);
    Major->SetActorLocation(FVector(41000,0,0));
    TestEqual(TEXT("400m 이탈 해제"),R.Interior->FormationRadarBonus(Major),0.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVNamedStrikeTest,"OrbitalVanguard.Hero.NamedRushAndSquadActions",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FOVNamedStrikeTest::RunTest(const FString&)
{
    for(FName Id:{FName(TEXT("M01")),FName(TEXT("M04"))})
    {
        OVInteriorTest::FRange R;
        const auto Room=R.Slot(EOVFacilityType::Engine);
        const int32 Raider=R.Interior->RegisterUnit(R.Target,0,EOVInteriorRole::Melee,Room);
        auto& Attacker=R.Interior->Units.Last();
        Attacker.HeroSkillRanks={1,1,1,0};Attacker.DefinitionId=Id;Attacker.Evasion=0;
        const FVector Start=Attacker.LocalPosition,Scale=R.Target->GetActorScale3D();
        TArray<int32> Foes;
        for(int32 I=0;I<3;++I){
            Foes.Add(R.Interior->RegisterUnit(R.Target,1,EOVInteriorRole::Ranged,Room));
            auto& Defender=R.Interior->Units.Last();
            const float Along=Id==TEXT("M04") && I>0?90.f+I*25.f:360.f+I*8.f;
            Defender.LocalPosition=Start+FVector(Along,I*4,0)/Scale;
            Defender.HP=Defender.MaxHP=10000;Defender.Damage=0;Defender.Evasion=0;
            if(I==0)Defender.DefinitionId=TEXT("R08");
        }
        R.Interior->Units[0].EnemyId=Foes[0];
        R.Advance(.1f);
        TestTrue(TEXT("돌입에 반응한 원거리 사수"),R.Interior->GetUnit(Foes[0]).AttackReadyAt>R.Interior->GameTime && R.Interior->GetUnit(Raider).bRushWeakened);
        R.Advance(1.2f);
        const auto Fighter=R.Interior->GetUnit(Raider);
        if(Id==TEXT("M01")){
            const auto Victim=R.Interior->GetUnit(Foes[0]);
            TestTrue(TEXT("강화 돌입 도달 때 주변 감속"),Victim.SlowUntil>0 && Victim.SlowFraction>=.25f);
        }else{
            TestTrue(TEXT("돌입 경로상의 별도 피해, 최대 두 명"),Fighter.RushTargets.Num()>0 && Fighter.RushTargets.Num()<=2);
        }
    }
    OVInteriorTest::FRange R;
    const auto Squad=R.Interior->RegisterBoardingTeam(R.Target,1,0,2);
    if(!TestEqual(TEXT("실제 일반 병력 편제"),Squad.Num(),2))return false;
    const auto Start=R.Interior->GetUnit(Squad[0]);
    const int32 Id=R.Interior->RegisterUnit(R.Target,0,EOVInteriorRole::Melee,Start.NodeId);
    for(auto& U:R.Interior->Units){
        U.LocalPosition=Start.LocalPosition;
        if(U.UnitId==Id){U.DefinitionId=TEXT("M10");U.Damage=10000;}
        else{U.Damage=0;U.Evasion=0;}
    }
    R.Advance(8);
    TestTrue(TEXT("같은 분대 전멸"),R.Interior->GetUnit(Squad[0]).HP<=0 && R.Interior->GetUnit(Squad[1]).HP<=0);
    TestTrue(TEXT("마지막 병사 사망 후 점령 효과"),R.Interior->GetUnit(Id).KillRallyUntil>R.Interior->GameTime);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVIrisArcGeometryTest,"OrbitalVanguard.Hero.IrisArcRespectsFacingDeckAndRange",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FOVIrisArcGeometryTest::RunTest(const FString&)
{
    OVInteriorTest::FRange R;
    const auto Room=R.Slot(EOVFacilityType::Engine);
    const auto Id=R.Interior->RegisterUnit(R.Target,0,EOVInteriorRole::Melee,Room);
    auto& Iris=R.Interior->Units.Last();Iris.DefinitionId=TEXT("M02");Iris.AttackReadyAt=1000;
    const FVector Anchor=Iris.LocalPosition;
    const auto* L=R.Target->Systems->Layout.Get();
    const int32 Deck=L->Slots[L->GetSlotIndex(Room)].DeckId;
    const auto* OtherDeck=L->Slots.FindByPredicate([&](const auto& S){return S.DeckId!=Deck;});
    if(!OtherDeck)return false;
    auto Target=[&](float X,float Y,FName Slot){
        const int32 UnitId=R.Interior->RegisterUnit(R.Target,1,EOVInteriorRole::Ranged,Slot);
        auto& V=R.Interior->Units.Last();V.AttackReadyAt=1000;V.HP=V.MaxHP=2000;
        V.LocalPosition=Anchor+FVector(X,Y,0)/R.Target->GetActorScale3D();
        return UnitId;
    };
    const int32 Front=Target(80,0,Room), Flank=Target(130,60,Room), Back=Target(-80,0,Room);
    const int32 Far=Target(270,0,Room), Upstairs=Target(80,0,OtherDeck->SlotId);
    R.Interior->Units[0].EnemyId=Front;
    R.Advance(4.25);
    TestEqual(TEXT("정면 바라본 역장 1회 종료"),R.Interior->GetUnit(Id).SignatureUses,1);
    TestTrue(TEXT("호형 안 두 대상 실제 피해"),R.Interior->GetUnit(Front).HP<2000 && R.Interior->GetUnit(Flank).HP<2000);
    for(const int32 Excluded:{Back,Far,Upstairs})TestEqual(TEXT("후방·거리 밖·다른 층 피해 없음"),R.Interior->GetUnit(Excluded).HP,2000.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVHeroTacticalEffectsTest,"OrbitalVanguard.Hero.NamedTacticalEffects",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FOVHeroTacticalEffectsTest::RunTest(const FString&)
{
    OVInteriorTest::FRange R;
    const FName Bridge=R.Slot(EOVFacilityType::Bridge);
    auto Spawn=[&](int32 Side,EOVInteriorRole Role,FName Definition){
        int32 Id=R.Interior->RegisterUnit(R.Target,Side,Role,Bridge);
        R.Interior->Units.Last().DefinitionId=Definition;
        return Id;
    };
    const int32 Captain=Spawn(0,EOVInteriorRole::Commander,TEXT("C01"));
    const int32 Troop=Spawn(0,EOVInteriorRole::Trooper,NAME_None);
    const int32 Guard=Spawn(1,EOVInteriorRole::Melee,TEXT("M09"));
    const int32 Officer=Spawn(1,EOVInteriorRole::Ranged,TEXT("R04"));
    const int32 Rival=Spawn(0,EOVInteriorRole::Melee,TEXT("M08"));
    if(!Captain || !Troop || !Guard || !Officer || !Rival)return false;
    auto* C=R.Interior->FindUnit(Captain);auto* T=R.Interior->FindUnit(Troop);
    auto* G=R.Interior->FindUnit(Guard);auto* O=R.Interior->FindUnit(Officer);auto* M=R.Interior->FindUnit(Rival);
    C->State=EOVInteriorState::Operating; C->Order=EOVInteriorOrder::Capture;
    const float BridgeGuard=R.Interior->InteriorReduction(*T,0,false);
    C->State=EOVInteriorState::Defending;
    TestTrue(TEXT("함교 점령 중에만 일반 병력 엄호"),BridgeGuard>R.Interior->InteriorReduction(*T,0,false));
    T->Order=EOVInteriorOrder::Retreat;
    const float NoRally=R.Interior->MovementBonus(*T);
    C->Skill3ActiveUntil=8;
    TestTrue(TEXT("전투 동조 중 철수 이동 가속"),R.Interior->MovementBonus(*T)>NoRally);
    O->bDutyActive=false;
    const float NoGuard=R.Interior->InteriorReduction(*O,0,false);
    O->bDutyActive=true;
    const float Protected=R.Interior->InteriorReduction(*O,0,false);
    TestTrue(TEXT("기관 인계 완료한 보직자 엄호"),Protected>NoGuard);
    M->State=EOVInteriorState::Fighting;M->EnemyId=Officer;M->TrackedSince=0;
    R.Interior->GameTime=5.1;
    TestTrue(TEXT("결투 상대만 외부 엄호 약화"),R.Interior->InteriorReduction(*O,0,false)<Protected &&
        R.Interior->ExternalAuraScale(*G)==1.f);
    M->HeroSkillRanks={1,1,1,1};M->HP=M->MaxHP=100;M->bSkill4Armed=true;
    R.Interior->ApplyDamage(*M,80,TEXT("시험 타격"));
    TestTrue(TEXT("치명상 생존 스킬과 한계 해방 연동"),M->HP==20 && M->ApexUntil>R.Interior->GameTime+3.9);
    auto Shot=[](FName Definition){
        OVInteriorTest::FRange Arena;
        const FName Engine=Arena.Slot(EOVFacilityType::Engine);
        const int32 Id=Arena.Interior->RegisterUnit(Arena.Target,0,EOVInteriorRole::Ranged,Engine);
        auto& Gunner=Arena.Interior->Units.Last();Gunner.DefinitionId=Definition;Gunner.HeroSkillRanks={1,1,0,0};
        const int32 Victim=Arena.Interior->RegisterUnit(Arena.Target,1,EOVInteriorRole::Trooper,Engine);
        auto& Target=Arena.Interior->Units.Last();Target.HP=Target.MaxHP=2000;Target.Evasion=0;Target.AttackReadyAt=10000;
        Arena.Advance(.3f);
        return FVector2D(Arena.Interior->GetUnit(Id).Hits,2000-Arena.Interior->GetUnit(Victim).HP);
    };
    OVInteriorTest::FRange Breach;
    const FName Engine=Breach.Slot(EOVFacilityType::Engine);
    const int32 Worker=Breach.Interior->RegisterUnit(Breach.Target,0,EOVInteriorRole::Melee,Engine);
    Breach.Interior->Units.Last().DefinitionId=TEXT("M05");
    Breach.Interior->Units.Last().HP=Breach.Interior->Units.Last().MaxHP=5000;
    Breach.Interior->Units.Last().Evasion=0;
    Breach.Interior->IssueOrder({Worker},EOVInteriorOrder::Sabotage,Engine);
    Breach.Advance(3);
    const int32 Suppressor=Breach.Interior->RegisterUnit(Breach.Target,1,EOVInteriorRole::Ranged,Engine);
    auto& Sniper=Breach.Interior->Units.Last();Sniper.DefinitionId=TEXT("R04");Sniper.HeroSkillRanks={1,1,1,0};
    Breach.Advance(.7f);
    TestTrue(TEXT("실제 공작자에게 제압 사격 후 2.5초 작업 중단"),Breach.Interior->GetUnit(Suppressor).Hits>0 &&
        Breach.Interior->GetUnit(Worker).SuppressedUntil>Breach.Interior->GameTime+1.7);
    const auto Regular=Shot(TEXT("G02")), Marksman=Shot(TEXT("R06"));
    TestTrue(TEXT("약점 감지 사용 중 초감각 조준 실제 피해 상승"),Regular.X>0 && Regular.X==Marksman.X && Marksman.Y>Regular.Y);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVHeroGarrisonLimits,"OrbitalVanguard.Interior.HeroGarrisonSeparateFromTroopCapacity",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FOVHeroGarrisonLimits::RunTest(const FString&)
{
    struct FCase { EOVHullSizeClass Size; const TCHAR* Id; int32 Heroes; };
    const FCase Cases[]={{EOVHullSizeClass::Small,TEXT("corvette"),1},{EOVHullSizeClass::Medium,TEXT("vector"),2},
        {EOVHullSizeClass::Large,TEXT("carrier"),4},{EOVHullSizeClass::ExtraLarge,TEXT("dreadnought"),6},
        {EOVHullSizeClass::Small,TEXT("assault"),4}};
    for(const FCase& Case:Cases){
        OVInteriorTest::FRange R;
        R.Origin->HullDefinitionId=Case.Id;
        R.Origin->Systems->Layout->HullSizeClass=Case.Size;
        for(int32 I=0;I<Case.Heroes;++I)
            TestTrue(Case.Id,R.Interior->RegisterUnit(R.Origin,0,EOVInteriorRole::Melee,NAME_None)>0);
        TestEqual(TEXT("상주 정원 다음 영웅은 배치할 수 없음"),R.Interior->RegisterUnit(R.Origin,0,EOVInteriorRole::Support,NAME_None),0);
        TestTrue(TEXT("일반병은 상주 영웅 자리를 쓰지 않음"),R.Interior->RegisterUnit(R.Origin,0,EOVInteriorRole::Trooper,NAME_None)>0);
    }
    return true;
}
#endif
