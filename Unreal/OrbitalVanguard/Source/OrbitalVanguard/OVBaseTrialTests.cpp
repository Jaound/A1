#include "OVBaseBuildState.h"
#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVBaseTrialNetwork,"OrbitalVanguard.Base.TrialNetwork",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FOVBaseTrialNetwork::RunTest(const FString&)
{
    using namespace OVBase;
    BuildState base;
    TestTrue(TEXT("겹침"),base.Check({Kind::Corridor,{2,0,0}})==OVBase::Error::Occupied);
    TestTrue(TEXT("잘못된 방향"),base.Check({Kind::Corridor,{3,0,0},1})==OVBase::Error::Direction);
    TestTrue(TEXT("분리된 높이"),base.Check({Kind::Corridor,{3,0,1}})==OVBase::Error::NoConnection);
    TestTrue(TEXT("통로 예약"),base.Start({Kind::Corridor,{3,0,0}}));
    TestTrue(TEXT("창고 예약"),base.Start({Kind::Storage,{4,0,0}}));
    TestFalse(TEXT("사용 중인 연결 취소"),base.Cancel(1));
    TestTrue(TEXT("별도 슬롯의 허브"),base.Start({Kind::Junction,{-3,0,0}}));
    TestTrue(TEXT("허브 수직 분기"),base.Start({Kind::Junction,{-3,0,1}}));
    TestTrue(TEXT("상부 취소"),base.Cancel(4));
    TestTrue(TEXT("하부 취소"),base.Cancel(3));
    TestEqual(TEXT("전액 환불"),base.Funds(),860);
    base.Advance(0);
    TestEqual(TEXT("정지"),base.Modules()[1].Remaining,15.);
    base.Advance(18);
    TestEqual(TEXT("통로 완공"),base.Modules()[1].Remaining,0.);
    TestEqual(TEXT("남는 게임시간만 창고에 적용"),base.Modules()[2].Remaining,57.);
    base.Advance(57);
    TestFalse(TEXT("완공 후 환불 차단"),base.Cancel(2));
    TestTrue(TEXT("창고 가동"),base.Network(false)[2]);
    BuildState funds;
    for(int side=0;side<3;++side)
    {
        const auto cell=Rotate({3,0,0},side);
        TestTrue(TEXT("세 방향 독립 통로"),funds.Start({Kind::Corridor,cell,side}));
        TestTrue(TEXT("세 방향 창고"),funds.Start({Kind::Storage,Rotate({4,0,0},side),side}));
        funds.Advance(75);
    }
    TestTrue(TEXT("마지막 통로"),funds.Start({Kind::Corridor,{0,-3,0},3}));
    TestTrue(TEXT("잔고 부족"),funds.Check({Kind::Storage,{0,-4,0},3})==OVBase::Error::Funds);
    BuildState trial(false);
    for(int side=0;side<4;++side)
    {
        TestTrue(TEXT("시험장 통로"),trial.Start({Kind::Corridor,Rotate({3,0,0},side),side}));
        TestTrue(TEXT("시험장 창고"),trial.Start({Kind::Storage,Rotate({4,0,0},side),side}));
        trial.Advance(75);
    }
    TestEqual(TEXT("누적 기준 비용이 재고를 넘어도 무료"),trial.Funds(),1200);
    BuildState capped;
    for(int x=3;x<15;++x)
    {
        TestTrue(TEXT("동측 통로"),capped.Start({Kind::Corridor,{x,0,0}}));
        TestTrue(TEXT("서측 통로"),capped.Start({Kind::Corridor,{-x,0,0}}));
    }
    TestTrue(TEXT("연결 한도 24"),capped.Check({Kind::Corridor,{15,0,0}})==OVBase::Error::Limit);
    for(int turn=0;turn<4;++turn)
    {
        BuildState staging;
        const auto placement=[turn](Kind kind,Cell origin,int yaw) { return Module{kind,Rotate(origin,turn),(yaw+turn)%4}; };
        TestTrue(TEXT("측면 통로"),staging.Start(placement(Kind::Corridor,{0,3,0},1)));
        TestTrue(TEXT("조선소 예약"),staging.Start(placement(Kind::Yard,{-3,4,0},0)));
        TestEqual(TEXT("조선소 비용"),staging.Funds(),510);
        TestTrue(TEXT("출입구 두 칸 전체 차단"),staging.Check(placement(Kind::Junction,{-5,6,2},0))==OVBase::Error::Clearance);
        TestTrue(TEXT("외곽 개방선 차단"),staging.Check(placement(Kind::Junction,{-18,5,0},0))==OVBase::Error::Clearance);
        staging.Advance(135);
        TestTrue(TEXT("동쪽 도크 통로"),staging.Start(placement(Kind::Corridor,{3,0,0},0)));
        TestTrue(TEXT("표준 도크 예약"),staging.Start(placement(Kind::Dock,{4,-1,0},0)));
        TestEqual(TEXT("시설 둘과 통로 둘의 잔액"),staging.Funds(),120);
        TestTrue(TEXT("도크 측면 개방선"),staging.Check(placement(Kind::Corridor,{5,12,0},0))==OVBase::Error::Clearance);
        staging.Advance(90);
        TestTrue(TEXT("두 시설 가동"),staging.Network(false).back());
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVBaseStage3,"OrbitalVanguard.Base.Stage3GrowthAndNetwork",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FOVBaseStage3::RunTest(const FString&)
{
    using namespace OVBase;
    // 실패한 전제 뒤의 인덱스 접근을 피한다.
#define VERIFY_RULE(...) if (!TestTrue(TEXT(#__VA_ARGS__), (__VA_ARGS__))) return false
    VERIFY_RULE(std::size(Catalog)==19);
    BuildState trial(false);
    VERIFY_RULE(trial.Check({Kind::YardMedium,{1,8,0}})==OVBase::Error::Tier);
    VERIFY_RULE(trial.Start({Kind::Corridor,{3,0,0}}));
    VERIFY_RULE(trial.Start({Kind::Storage,{4,0,0}}));
    VERIFY_RULE(!trial.Cancel(1));
    trial.Advance(18);
    VERIFY_RULE(trial.Modules()[2].Remaining==57);
    trial.Advance(57);
    VERIFY_RULE(trial.Capacity().Advanced==2500);
    VERIFY_RULE(trial.UnlockTestTechnology(Technology::EngineeringII) && trial.StartUpgrade(0));
    trial.Advance(299);
    VERIFY_RULE(trial.CoreTier()==1);
    trial.Advance(1);
    VERIFY_RULE(trial.CoreTier()==2 && trial.Limits().Concurrent==2);
    VERIFY_RULE(!trial.HasTechnology(Technology::HullMedium));
    VERIFY_RULE(trial.UnlockTestTechnology(Technology::EngineeringIII) && trial.StartUpgrade(0));
    VERIFY_RULE(trial.UnlockTestTechnology(Technology::StorageII) && trial.StartUpgrade(2));
    trial.Advance(600);
    VERIFY_RULE(trial.CoreTier()==3 && trial.Modules()[2].Level==2);
    VERIFY_RULE(trial.UnlockTestTechnology(Technology::StorageIII) && trial.StartUpgrade(2));
    trial.Advance(120);
    VERIFY_RULE(trial.Capacity().Advanced==5500);
    VERIFY_RULE(trial.UnlockTestTechnology(Technology::EngineeringIV));
    VERIFY_RULE(trial.CheckUpgrade(0)==OVBase::Error::Capacity);
    VERIFY_RULE(trial.Start({Kind::Corridor,{-3,0,0},2}));
    VERIFY_RULE(trial.Start({Kind::Storage,{-4,0,0},2}));
    trial.Advance(75);
    VERIFY_RULE(trial.Modules()[4].Level==1 && trial.StartUpgrade(0));
    trial.Advance(960);
    VERIFY_RULE(trial.CoreTier()==4 && trial.Limits().Links==132);

    BuildState paid;
    VERIFY_RULE(paid.Start({Kind::Junction,{3,0,0}}));
    VERIFY_RULE(paid.Start({Kind::Junction,{4,0,0}}));
    VERIFY_RULE(paid.Start({Kind::Junction,{5,0,0}}));
    VERIFY_RULE(paid.Start({Kind::Junction,{3,1,0}}));
    VERIFY_RULE(paid.Start({Kind::Junction,{4,1,0}}));
    VERIFY_RULE(paid.Start({Kind::Junction,{5,1,0}}));
    VERIFY_RULE(paid.Start({Kind::Storage,{6,0,0}}));
    paid.Advance(120);
    VERIFY_RULE(paid.Network(false).back());
    auto preview=paid.InspectRemoval(2,false);
    VERIFY_RULE(preview.Result==OVBase::Error::None && preview.Disconnected.empty() && preview.Refund.Common==30);
    VERIFY_RULE(paid.Demolish(2) && paid.Network(false).back());
    VERIFY_RULE(paid.Demolish(3) && !paid.Network(false).back());
    VERIFY_RULE(paid.Start({Kind::Junction,{4,0,0}}));
    paid.Advance(4);
    BuildState restored;
    VERIFY_RULE(restored.Restore(paid.Modules(),paid.Available(),paid.ResearchMask(),paid.Charges(),paid.NextModuleId()));
    VERIFY_RULE(restored.Funds()==paid.Funds() && restored.Modules().back().Remaining==11);
    const int last=static_cast<int>(restored.Modules().size()-1);
    const int money=restored.Funds();
    VERIFY_RULE(restored.Cancel(last) && restored.Funds()==money+40);
    VERIFY_RULE(restored.Start({Kind::Junction,{4,0,0}}));
    restored.Advance(15);
    VERIFY_RULE(restored.Network(false)[4]);
    auto corrupt=paid.Modules();
    corrupt.back().Id=1;
    VERIFY_RULE(!restored.Restore(corrupt,paid.Available(),0,true,paid.NextModuleId()));
    VERIFY_RULE(restored.Funds()==money);
    for (int turn=0;turn<4;++turn)
    {
        const Module yard{Kind::YardLarge,{3,2,1},turn};
        const Module dock{Kind::DockSuperheavy,{-2,-3,0},turn};
        // Each opening straddles two lateral cells, beyond the full-width apron.
        VERIFY_RULE(InFlightLane(yard,yard.Origin+Rotate({-6,2,1},turn)));
        VERIFY_RULE(InFlightLane(yard,yard.Origin+Rotate({-6,3,1},turn)));
        VERIFY_RULE(!InFlightLane(yard,yard.Origin+Rotate({-6,1,0},turn)));
        VERIFY_RULE(InFlightLane(dock,dock.Origin+Rotate({4,14,1},turn)));
        VERIFY_RULE(InFlightLane(dock,dock.Origin+Rotate({5,14,1},turn)));
        VERIFY_RULE(!InFlightLane(dock,dock.Origin+Rotate({2,14,0},turn)));
    }
#undef VERIFY_RULE
    return true;
}

#endif
