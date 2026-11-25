#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Tests/AutomationEditorCommon.h"
#include "Framework/Application/SlateApplication.h"
#include "Layout/WidgetPath.h"
#include "UObject/UObjectIterator.h"
#include "OVCalibration.h"
#include "OVController.h"
#include "OVHUD.h"
#include "OVPlayScenarioSubsystem.h"
#include "OVShip.h"
#include "OVShipPreview.h"
#include "OVShipSystemsComponent.h"
#include "OVSortieDesignWidget.h"
#include "OVTitleScreenWidget.h"

namespace
{
class FCheckSortiePointer : public IAutomationLatentCommand
{
public:
    explicit FCheckSortiePointer(FAutomationTestBase& InTest):Test(InTest),Deadline(FPlatformTime::Seconds()+45){}
    bool Update() override
    {
        if(FPlatformTime::Seconds()>Deadline){
            Test.AddError(FString::Printf(TEXT("Timed out awaiting PIE design view or rendered input response, phase %d"),Phase));
            return true;
        }
        UWorld* World=GEditor?GEditor->PlayWorld.Get():nullptr;
        AOVController* PC=World?Cast<AOVController>(World->GetFirstPlayerController()):nullptr;
        AOVHUD* HUD=PC?Cast<AOVHUD>(PC->GetHUD()):nullptr;
        if(!HUD)return false;
        if(Phase==0){
            if(!HUD->BeginFleetPreparation(TEXT("Fleet50"),false)){Test.AddError(TEXT("Could not open preparation"));return true;}
            Menu=HUD->GetTitleScreen();
            Menu->SortieClick(TEXT("Next"));
            Menu->SortieClick(TEXT("Add"),OVCalibration::GetHullIds().IndexOfByKey(FName(TEXT("battleship"))));
            Menu->SortieClick(TEXT("Next"));
            Menu->SortieClick(TEXT("ShowWeapons"));
            if(!PC->GetPlayScenario()->bSortieExternal){Test.AddError(TEXT("External design did not open"));return true;}
            Phase=1;
            return false;
        }
        auto& App=FSlateApplication::Get();
        if(!Board.IsValid()){
            for(TObjectIterator<UOVSortieDesignBoard> It;It;++It)if(It->GetWorld()==World && It->GetCachedWidget().IsValid()){
                FWidgetPath Candidate;
                if(App.GeneratePathToWidgetUnchecked(It->GetCachedWidget().ToSharedRef(),Candidate)){Board=*It;break;}
            }
        }
        if(!Board.IsValid())return false;
        const auto Window=App.FindWidgetWindow(Board->TakeWidget());
        if(!Window.IsValid())return false;
        for(TObjectIterator<UOVShipPreview> It;!Preview.IsValid() && It;++It)
            if(It->GetOuter()==Menu.Get() && It->GetTexture())Preview=*It;
        if(!Preview.IsValid())return false;
        const auto& G=Board->GetCachedGeometry();
        if(G.GetLocalSize().X<25 || G.GetLocalSize().Y<25)return false;
        const FVector2D Center=G.LocalToAbsolute(G.GetLocalSize()*.5f);
        // Pointer routing needs the hit-test grid's per-widget virtual positions.
        // A visibility-only path from GeneratePathToWidgetUnchecked lacks that pointer data.
        const FWidgetPath Path=App.LocateWindowUnderMouse(Center,{Window.ToSharedRef()});
        if(!Path.IsValid() || !Path.ContainsWidget(&Board->TakeWidget().Get()))return false;
        const auto PositionAfterDrag=Center+FVector2D(85,37);
        const TSet<FKey> Held{EKeys::MiddleMouseButton};
        const TSet<FKey> Released;
        auto Pointer=[](FVector2D Now,FVector2D Previous,const TSet<FKey>& Buttons,FKey Cause,float Wheel=0.f){
            return FPointerEvent(FSlateApplication::CursorPointerIndex,Now,Previous,Buttons,Cause,Wheel,FModifierKeysState());
        };
        if(Phase==1){
            auto* Ship=PC->GetPlayScenario()->InspectSortie(0);
            if(!Ship || !Ship->Systems->Layout || Ship->Systems->Layout->WeaponMounts.IsEmpty()){Test.AddError(TEXT("Expected authored battleship mount"));return true;}
            Mount=Ship->Systems->Layout->WeaponMounts[0].LocalPosition;
            BeforeDrag=Preview->Project(Mount);
            const FRotator Original=Menu->SortieViewRotation;
            // A physical mouse enters the viewport before pressing. An offscreen editor
            // starts with no previous widget path; Slate uses that path when claiming capture.
            App.RoutePointerMoveEvent(Path,Pointer(Center,Center-FVector2D(2,1),Released,EKeys::Invalid),false);
            const auto Down=App.RoutePointerDownEvent(Path,Pointer(Center,Center,Held,EKeys::MiddleMouseButton));
            Test.TestTrue(TEXT("Middle press routed and captured by real design board"),Down.IsEventHandled() && Board->HasMouseCapture());
            App.RoutePointerMoveEvent(Path,Pointer(PositionAfterDrag,Center,Held,EKeys::Invalid),false);
            Test.TestFalse(TEXT("Held pointer movement changes hull rotation"),Original.Equals(Menu->SortieViewRotation,.01));
            App.RoutePointerUpEvent(Path,Pointer(PositionAfterDrag,PositionAfterDrag,Released,EKeys::MiddleMouseButton));
            Test.TestFalse(TEXT("Middle release clears board capture"),Board->HasMouseCapture());
            AfterRotation=Menu->SortieViewRotation;
            App.RoutePointerMoveEvent(Path,Pointer(PositionAfterDrag+FVector2D(24,12),PositionAfterDrag,Released,EKeys::Invalid),false);
            Test.TestTrue(TEXT("Unheld motion leaves orientation stable"),AfterRotation.Equals(Menu->SortieViewRotation,.01));
            if(Test.HasAnyErrors())return true;
            Phase=2;
            return false;
        }
        const FVector2D Projected=Preview->Project(Mount);
        if(Phase==2){
            if(Projected.Equals(BeforeDrag,.001))return false;
            AfterDrag=Projected;
            Test.AddInfo(FString::Printf(TEXT("Actual mount projection before drag %s, after drag %s"),*BeforeDrag.ToString(),*AfterDrag.ToString()));
            const float OldZoom=Menu->SortieViewZoom;
            const auto Wheel=App.RouteMouseWheelOrGestureEvent(Path,Pointer(Center,Center,Released,EKeys::MouseWheelAxis,1.f));
            Test.TestTrue(TEXT("Routed mouse wheel increases external view zoom"),Wheel.IsEventHandled() && Menu->SortieViewZoom>OldZoom);
            if(Test.HasAnyErrors())return true;
            Phase=3;
            return false;
        }
        if(Projected.Equals(AfterDrag,.0005))return false;
        const FVector2D FrameCenter(.5,.5);
        Test.TestTrue(TEXT("Zoom moves the physical socket projection away from center"),FVector2D::Distance(Projected,FrameCenter)>FVector2D::Distance(AfterDrag,FrameCenter));
        Test.AddInfo(FString::Printf(TEXT("Actual mount projection after wheel %s; rotation %s; zoom %.2f"),*Projected.ToString(),*AfterRotation.ToString(),Menu->SortieViewZoom));
        PC->LeavePlayScenario();
        return true;
    }
private:
    FAutomationTestBase& Test;
    double Deadline;
    int32 Phase=0;
    TWeakObjectPtr<UOVTitleScreenWidget> Menu;
    TWeakObjectPtr<UOVSortieDesignBoard> Board;
    TWeakObjectPtr<UOVShipPreview> Preview;
    FRotator AfterRotation=FRotator::ZeroRotator;
    FVector Mount=FVector::ZeroVector;
    FVector2D BeforeDrag=FVector2D::ZeroVector,AfterDrag=FVector2D::ZeroVector;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVSortiePointerTest,"OrbitalVanguard.Sortie.DesignBoard.SlatePointer",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::NonNullRHI|EAutomationTestFlags::EngineFilter)
bool FOVSortiePointerTest::RunTest(const FString&)
{
    if(GEditor->PlayWorld){AddError(TEXT("Run in a separate editor without an existing PIE session"));return false;}
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Maps/ControlRange")));
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    ADD_LATENT_AUTOMATION_COMMAND(FCheckSortiePointer(*this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}
#endif
