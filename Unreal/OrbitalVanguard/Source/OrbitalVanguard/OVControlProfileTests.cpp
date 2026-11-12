#include "OVControlProfile.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVRightMoveTest, "OrbitalVanguard.Controls.RightMoveAnchorAndRelease",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOVRightMoveTest::RunTest(const FString&)
{
    FOVRightMoveGesture Gesture;
    const FVector Anchor(40, 60, 300);
    Gesture.Begin({100, 100}, Anchor, true);
    TestEqual(TEXT("Sub-threshold preview equals click destination"), Gesture.Sample({103, 96}), Anchor);
    TestEqual(TEXT("Drag changes altitude only"), Gesture.Sample({180, 80}), FVector(40, 60, 454));
    TestTrue(TEXT("First valid release commits"), Gesture.Finish(true));
    TestFalse(TEXT("Repeated release is consumed"), Gesture.Finish(true));
    Gesture.Begin({100, 100}, Anchor, true);
    TestFalse(TEXT("UI release cancels"), Gesture.Finish(false));
    TestFalse(TEXT("Late release cannot revive canceled move"), Gesture.Finish(true));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVOrbitOwnerTest, "OrbitalVanguard.Controls.OrbitOwnerAndReset",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOVOrbitOwnerTest::RunTest(const FString&)
{
    FOVOrbitGesture Gesture;
    Gesture.Begin(EOVOrbitSurface::World, false, {20, 30});
    Gesture.Begin(EOVOrbitSurface::Preview, true, {250, 300});
    TestTrue(TEXT("A second button cannot steal ownership"), Gesture.Surface==EOVOrbitSurface::World);
    TestEqual(TEXT("Crossing the panel still samples from the original anchor"), Gesture.Sample({250, 300}), FVector2D(230, 270));
    Gesture.End(true);
    TestTrue(TEXT("Unrelated release keeps orbit"), Gesture.Surface==EOVOrbitSurface::World);
    Gesture.End(false);
    Gesture.Begin(EOVOrbitSurface::Preview, true, {400, 400});
    Gesture.End(true);
    TestTrue(TEXT("Alt release ends Alt-left orbit"), Gesture.Surface==EOVOrbitSurface::None);
    Gesture.Begin(EOVOrbitSurface::World, false, {600, 600});
    TestEqual(TEXT("New gesture does not inherit delta"), Gesture.Sample({600, 600}), FVector2D::ZeroVector);
    return true;
}
#endif
