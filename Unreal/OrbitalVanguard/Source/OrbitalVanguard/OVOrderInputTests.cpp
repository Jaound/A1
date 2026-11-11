#include "OVOrderInput.h"
#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVOrderInputTest,"OrbitalVanguard.Input.AnchoredAltitudeAndUI",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FOVOrderInputTest::RunTest(const FString&)
{
 FOVOrderInput Input; Input.Reset(); FVector P(0,0,300);
 Input.Sample(P,FVector(100,200,300),500,false,false);
 TestEqual(TEXT("XY projection"),P,FVector(100,200,300));
 Input.Sample(P,FVector(900,800,300),500,true,false);
 Input.Sample(P,FVector(900,800,300),495,true,false);
 TestEqual(TEXT("Under 8px no altitude"),P.Z,300.0);
 Input.Sample(P,FVector(900,800,300),480,true,false);
 TestEqual(TEXT("20px altitude"),P,FVector(100,200,500));
 Input.Sample(P,FVector(900,800,300),480,false,false);
 TestEqual(TEXT("Release no XY jump"),P,FVector(100,200,500));
 Input.Sample(P,FVector(920,820,300),480,false,false);
 TestEqual(TEXT("Continued XY offset"),P,FVector(120,220,500));
 Input.Sample(P,FVector(0,0,0),0,true,true);
 Input.Sample(P,FVector(4000,4000,0),300,true,false);
 TestEqual(TEXT("HUD/focus reentry no jump"),P,FVector(120,220,500));
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVProjectionTest,"OrbitalVanguard.Input.ProjectionGuards",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FOVProjectionTest::RunTest(const FString&)
{
 FVector P; TestFalse(TEXT("Shallow rejected"),FOVOrderInput::Project(FVector(0,0,4000),FVector(1,0,-.01),300,P));
 TestFalse(TEXT("Behind ray rejected"),FOVOrderInput::Project(FVector(0,0,4000),FVector(0,0,1),300,P));
 TestFalse(TEXT("Near camera rejected"),FOVOrderInput::Project(FVector(0,0,400),FVector(0,0,-1),300,P));
 TestTrue(TEXT("Valid selected elevation"),FOVOrderInput::Project(FVector(0,0,4000),FVector(0,0,-1),300,P));
 TestEqual(TEXT("Actual plane matches disc"),P.Z,300.0); return true;
}
#endif
