#include "OVInteriorSubsystem.h"
#include "OVTransportSubsystem.h"
#include "OVCombatSubsystem.h"
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
    R.Advance(1);TestEqual(TEXT("사망 사건은 중복되지 않음"),R.Interior->TotalDeaths,16);
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
    const auto Dev=R.Transport->AddDevPassengers(R.Origin,0,1);R.Transport->LaunchPods(R.Origin,R.Target);R.Advance(.5f);
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
    TestTrue(TEXT("실제 유닛 진입이 세력 보호 활성"),R.Target->Systems->HasInteriorPresence(0));
    TestFalse(TEXT("실제 유닛 보호를 DEV로 표시하지 않음"),R.Target->Systems->IsInteriorDeveloperInput(0));
    TestFalse(TEXT("DEV 복귀 버튼으로 실내 이동 우회 금지"),R.Transport->DevReturnPersonToEntry(Person.PersonId));
    TestFalse(TEXT("진입점 좌표를 전달해도 실제 철수 명령·위치 없이는 거부"),R.Transport->ReturnRuntimePersonToEntry(Person.PersonId,Ids[0],R.Target,Person.EntryLocalPoint,Person.EntryLayoutVersion));
    const FName Shield=R.Slot(EOVFacilityType::ShieldControl);const auto Facility=R.Target->Systems->GetSlotState(Shield);
    R.Interior->IssueOrder(Ids,EOVInteriorOrder::Capture,Shield);R.Advance(.1f);
    TestTrue(TEXT("목표 지시만으로 즉시 점령하지 않음"),R.Target->Systems->GetSlotState(Shield).OccupyingFaction!=0);
    R.Advance(25);
    TestEqual(TEXT("실제 이동·작업 후 시설 점령"),R.Target->Systems->GetSlotState(Shield).OccupyingFaction,0);
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
        TestEqual(TEXT("손실 뒤 무료 인원 생성/삭제 없음"),R.Transport->Persons.Num(),2);
        TestEqual(TEXT("손실 뒤 무료 포드 생성 없음"),R.Transport->Pods.Num(),0);
        TestEqual(TEXT("구조/회수 대기 영웅은 세력 배치 한도 점유"),R.Interior->CountActiveHeroes(0),1);
    }
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
#endif
