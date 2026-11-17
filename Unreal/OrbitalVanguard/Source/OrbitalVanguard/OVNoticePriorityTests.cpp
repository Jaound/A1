#include "OVBattleNotices.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVNoticePriorityTest,"OrbitalVanguard.Presentation.CriticalToastPriority",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOVNoticePriorityTest::RunTest(const FString& Parameters)
{
    auto* Feed=NewObject<UOVBattleNotices>();
    TestEqual(TEXT("empty feed"),Feed->GetToastIndex(10),INDEX_NONE);
    auto& Critical=Feed->Notices.AddDefaulted_GetRef();Critical.bCritical=true;Critical.DisplayTime=10;Critical.Serial=1;
    auto& Routine=Feed->Notices.AddDefaulted_GetRef();Routine.DisplayTime=11;Routine.Serial=2;
    TestEqual(TEXT("critical stays above newer routine"),Feed->GetToastIndex(11.5),0);
    TestEqual(TEXT("ordinary notice resumes after critical interval"),Feed->GetToastIndex(13),1);
    TestEqual(TEXT("ordinary notice expires"),Feed->GetToastIndex(16),INDEX_NONE);
    TestEqual(TEXT("history remains in original order"),Feed->Notices[0].Serial,int64(1));
    TestEqual(TEXT("history retains ordinary notice"),Feed->Notices[1].Serial,int64(2));
    auto& NewCritical=Feed->Notices.AddDefaulted_GetRef();NewCritical.bCritical=true;NewCritical.DisplayTime=12;NewCritical.Serial=3;
    TestEqual(TEXT("latest critical wins"),Feed->GetToastIndex(12.5),2);
    Feed->Reset();TestEqual(TEXT("reset leaves no toast"),Feed->GetToastIndex(12.5),INDEX_NONE);
    return true;
}
#endif
