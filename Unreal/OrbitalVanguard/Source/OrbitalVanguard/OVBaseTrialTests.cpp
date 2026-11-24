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
    BuildState capped;
    for(int x=3;x<15;++x)
    {
        TestTrue(TEXT("동측 통로"),capped.Start({Kind::Corridor,{x,0,0}}));
        TestTrue(TEXT("서측 통로"),capped.Start({Kind::Corridor,{-x,0,0}}));
    }
    TestTrue(TEXT("연결 한도 24"),capped.Check({Kind::Corridor,{15,0,0}})==OVBase::Error::Limit);
    return true;
}
#endif
