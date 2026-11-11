#include "OVController.h"
#include "OVShip.h"
#include "OVHUD.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"

AOVController::AOVController() { bShowMouseCursor=true; bEnableClickEvents=true; bEnableMouseOverEvents=true; }
void AOVController::BeginPlay()
{
 Super::BeginPlay();
 FInputModeGameAndUI Mode; Mode.SetHideCursorDuringCapture(false); Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock); SetInputMode(Mode);
 RangeCamera=GetWorld()->SpawnActor<ACameraActor>();
 RangeCamera->GetCameraComponent()->SetFieldOfView(55);
 RangeCamera->GetCameraComponent()->PostProcessSettings.bOverride_AutoExposureMethod=true;
 RangeCamera->GetCameraComponent()->PostProcessSettings.AutoExposureMethod=EAutoExposureMethod::AEM_Manual;
 RangeCamera->GetCameraComponent()->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure=true;
 RangeCamera->GetCameraComponent()->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure=false;
 SetViewTarget(RangeCamera);
 auto Spawn=[this](const FString& Name,FVector Pos,bool Friendly,float Speed,float Turn,FVector Scale,FLinearColor Color) {
  AOVShip* Ship=GetWorld()->SpawnActor<AOVShip>(Pos,FRotator::ZeroRotator);
  Ship->Configure(Name,Friendly,Speed,Turn,Scale,Color); Ships.Add(Ship); return Ship;
 };
 Spawn(TEXT("01 / KESTREL - Escort"),FVector(-1000,-650,300),true,850,90,FVector(1.4,1.4,1.4),FLinearColor(0.1f,0.8f,1));
 Spawn(TEXT("02 / ATLAS - Cruiser"),FVector(-800,650,300),true,420,24,FVector(2.5,2.5,2.5),FLinearColor(0.3f,0.5f,1));
 Spawn(TEXT("03 / RED SIGNAL - Target"),FVector(1900,900,1100),false,0,0,FVector(1.8,1.8,1.8),FLinearColor(1,0.12f,0.08f));
 SelectShip(Ships[0]); UpdateCamera();
 UE_LOG(LogTemp,Display,TEXT("OV: ControlRange ready; two friendly ships + one target"));
}
void AOVController::SelectShip(AOVShip* Ship)
{
 if(!Ship) return;
 for(AOVShip* Other:Ships) Other->bSelected=false;
 Selected=Ship; Selected->bSelected=true; bPlacing=false;
 LastAction=TEXT("Selected: ")+Ship->ShipName;
 UE_LOG(LogTemp,Display,TEXT("OV: Select %s"),*Ship->ShipName);
}
void AOVController::Action(FName Name)
{
 if(Name==TEXT("Pause")) { bSimulationPaused=!bSimulationPaused; LastAction=bSimulationPaused?TEXT("Simulation paused - camera and orders remain active"):TEXT("Simulation resumed"); }
 if(Name==TEXT("Speed1")) SimulationSpeed=1;
 if(Name==TEXT("Speed2")) SimulationSpeed=2;
 if(Name==TEXT("Speed3")) SimulationSpeed=3;
 if(Name==TEXT("Ship1")) SelectShip(Ships[0]);
 if(Name==TEXT("Ship2")) SelectShip(Ships[1]);
 if(Selected && Selected->bFriendly) {
  if(Name==TEXT("Manual")) { Selected->bManual=true; LastAction=TEXT("MANUAL - persists until AUTO is clicked"); }
  if(Name==TEXT("Auto")) { Selected->bManual=false; Selected->bHasDestination=false; bPlacing=false; LastAction=TEXT("AUTO - track target heading (no weapons in phase 1)"); }
  if(Name==TEXT("Move")) { bPlacing=true; Preview=Selected->GetActorLocation(); PlacementPlaneZ=Preview.Z; LastAction=TEXT("Move cursor for XY; wheel for Z; Enter / left click confirms"); }
  if(Name==TEXT("Up") && bPlacing) Preview.Z=FMath::Min(Preview.Z+250,4000.0);
  if(Name==TEXT("Down") && bPlacing) Preview.Z=FMath::Max(Preview.Z-250,-1500.0);
  if(Name==TEXT("Confirm")) ConfirmMove();
 }
 if(Name==TEXT("Cancel")) bPlacing=false;
 if(Name==TEXT("Focus") && Selected) CameraFocus=Selected->GetActorLocation();
 UE_LOG(LogTemp,Display,TEXT("OV: Action=%s paused=%d speed=%.0f"),*Name.ToString(),bSimulationPaused,SimulationSpeed);
}
void AOVController::ConfirmMove()
{
 if(!bPlacing || !Selected || !Selected->bFriendly) return;
 Selected->bManual=true; Selected->SetDestination(Preview); bPlacing=false;
 LastAction=FString::Printf(TEXT("MANUAL order: X %.0f  Y %.0f  Z %.0f"),Preview.X,Preview.Y,Preview.Z);
 UE_LOG(LogTemp,Display,TEXT("OV: %s"),*LastAction);
}
void AOVController::UpdatePreview()
{
 FVector Origin,Direction;
 if(DeprojectMousePositionToWorld(Origin,Direction) && FMath::Abs(Direction.Z)>0.001) {
  const double T=(PlacementPlaneZ-Origin.Z)/Direction.Z;
  if(T>0) { const FVector Point=Origin+Direction*T; Preview.X=FMath::Clamp(Point.X,-6000.0,6000.0); Preview.Y=FMath::Clamp(Point.Y,-6000.0,6000.0); }
 }
}
void AOVController::UpdateCamera()
{
 const FRotator Rotation(CameraPitch,CameraYaw,0);
 // Frame the focus in the usable area to the right of the HUD.
 int32 Width=1280,Height=720; GetViewportSize(Width,Height);
 const float UIScale=FMath::Min(1.f,FMath::Min(Width/1280.f,Height/720.f));
 const float Offset=185.f*UIScale/FMath::Max(Width,1)*2.f*CameraDistance*FMath::Tan(FMath::DegreesToRadians(27.5f));
 RangeCamera->SetActorLocationAndRotation(CameraFocus-Rotation.Vector()*CameraDistance-FRotationMatrix(Rotation).GetUnitAxis(EAxis::Y)*Offset,Rotation);
}
void AOVController::StepSimulation(float DeltaTime)
{
 if(bSimulationPaused) return;
 const float SimDt=DeltaTime*SimulationSpeed; SimulationSeconds+=SimDt;
 for(AOVShip* Ship:Ships) {
  if(!Ship->bFriendly) continue;
  if(!Ship->bManual) {
   const FQuat Current=Ship->GetActorQuat(); const FQuat Desired=(Ships[2]->GetActorLocation()-Ship->GetActorLocation()).Rotation().Quaternion();
   const double Angle=Current.AngularDistance(Desired);
   Ship->SetActorRotation(FQuat::Slerp(Current,Desired,Angle>SMALL_NUMBER?FMath::Min(1.0,FMath::DegreesToRadians(Ship->TurnRate*SimDt)/Angle):1.0));
  } else Ship->Simulate(SimDt);
 }
}
void AOVController::PlayerTick(float DeltaTime)
{
 Super::PlayerTick(DeltaTime);
 if(!RangeCamera) return;
 float MX=0,MY=0; GetMousePosition(MX,MY);
 const AOVHUD* HUD=Cast<AOVHUD>(GetHUD()); const bool OverUI=HUD && HUD->IsOverPanel(MX,MY);
 const float Dt=FMath::Min(DeltaTime,0.1f);
 const FRotator Flat(0,CameraYaw,0);
 FVector Pan=FVector::ZeroVector;
 if(IsInputKeyDown(EKeys::W)||IsInputKeyDown(EKeys::Up)) Pan+=Flat.Vector();
 if(IsInputKeyDown(EKeys::S)||IsInputKeyDown(EKeys::Down)) Pan-=Flat.Vector();
 if(IsInputKeyDown(EKeys::D)||IsInputKeyDown(EKeys::Right)) Pan+=FRotationMatrix(Flat).GetUnitAxis(EAxis::Y);
 if(IsInputKeyDown(EKeys::A)||IsInputKeyDown(EKeys::Left)) Pan-=FRotationMatrix(Flat).GetUnitAxis(EAxis::Y);
 if(IsInputKeyDown(EKeys::E)) Pan.Z+=1;
 if(IsInputKeyDown(EKeys::Q)) Pan.Z-=1;
 CameraFocus+=Pan.GetClampedToMaxSize(1)*CameraDistance*0.45*Dt;
 if(IsInputKeyDown(EKeys::MiddleMouseButton)) { float DX,DY; GetInputMouseDelta(DX,DY); CameraYaw+=DX*0.3f; CameraPitch=FMath::Clamp(CameraPitch-DY*0.3f,-85.f,-10.f); }
 const int Wheel=(WasInputKeyJustPressed(EKeys::MouseScrollUp)?1:0)-(WasInputKeyJustPressed(EKeys::MouseScrollDown)?1:0);
 if(Wheel) { if(bPlacing) Preview.Z=FMath::Clamp(Preview.Z+Wheel*250,-1500.0,4000.0); else CameraDistance=FMath::Clamp(CameraDistance*FMath::Pow(0.86f,Wheel),1200.f,16000.f); }
 if(WasInputKeyJustPressed(EKeys::Z)) CameraYaw-=15;
 if(WasInputKeyJustPressed(EKeys::C)) CameraYaw+=15;
 if(WasInputKeyJustPressed(EKeys::T)) CameraPitch=FMath::Clamp(CameraPitch-10,-85.f,-10.f);
 if(WasInputKeyJustPressed(EKeys::G)) CameraPitch=FMath::Clamp(CameraPitch+10,-85.f,-10.f);
 if(WasInputKeyJustPressed(EKeys::Home)) { CameraFocus=FVector::ZeroVector; CameraYaw=-45; CameraPitch=-48; CameraDistance=6500; }
 if(WasInputKeyJustPressed(EKeys::F9)) UE_LOG(LogTemp,Display,TEXT("OV VERIFY: camera=%s yaw=%.1f pitch=%.1f distance=%.1f ship=%s pos=%s manual=%d moving=%d paused=%d speed=%.0f sim=%.3f"),*CameraFocus.ToString(),CameraYaw,CameraPitch,CameraDistance,Selected?*Selected->ShipName:TEXT("none"),Selected?*Selected->GetActorLocation().ToString():TEXT("none"),Selected?Selected->bManual:0,Selected?Selected->bHasDestination:0,bSimulationPaused,SimulationSpeed,SimulationSeconds);
 if(WasInputKeyJustPressed(EKeys::F)) Action(TEXT("Focus"));
 if(WasInputKeyJustPressed(EKeys::F1)) Action(TEXT("Ship1"));
 if(WasInputKeyJustPressed(EKeys::F2)) Action(TEXT("Ship2"));
 if(WasInputKeyJustPressed(EKeys::SpaceBar)) Action(TEXT("Pause"));
 if(WasInputKeyJustPressed(EKeys::One)) Action(TEXT("Speed1"));
 if(WasInputKeyJustPressed(EKeys::Two)) Action(TEXT("Speed2"));
 if(WasInputKeyJustPressed(EKeys::Three)) Action(TEXT("Speed3"));
 if(WasInputKeyJustPressed(EKeys::R)) Action(TEXT("Move"));
 if(WasInputKeyJustPressed(EKeys::BackSpace)) Action(TEXT("Cancel"));
 if(WasInputKeyJustPressed(EKeys::RightMouseButton) && !OverUI) Action(bPlacing?TEXT("Cancel"):TEXT("Move"));
 UpdateCamera();
 if(bPlacing && !OverUI) UpdatePreview();
 if(WasInputKeyJustPressed(EKeys::Enter)) ConfirmMove();
 if(WasInputKeyJustPressed(EKeys::LeftMouseButton) && !OverUI) {
  if(bPlacing) ConfirmMove();
  else { FHitResult Hit; if(GetHitResultUnderCursor(ECC_Visibility,false,Hit)) if(AOVShip* Ship=Cast<AOVShip>(Hit.GetActor())) SelectShip(Ship); }
 }
 StepSimulation(DeltaTime);
 for(int I=-12;I<=12;++I) {
  DrawDebugLine(GetWorld(),FVector(I*500,-6000,0),FVector(I*500,6000,0),I==0?FColor(50,130,180):FColor(18,35,55),false,-1,0,2);
  DrawDebugLine(GetWorld(),FVector(-6000,I*500,0),FVector(6000,I*500,0),I==0?FColor(180,75,60):FColor(18,35,55),false,-1,0,2);
 }
 for(AOVShip* Ship:Ships) {
  const FVector P=Ship->GetActorLocation();
  DrawDebugLine(GetWorld(),FVector(P.X,P.Y,0),P,Ship->bFriendly?FColor(30,80,110):FColor(140,30,30),false,-1,0,2);
  if(Ship->bSelected) { FVector Center,Extent; Ship->GetActorBounds(false,Center,Extent); DrawDebugBox(GetWorld(),Center,Extent+FVector(25),FColor::Cyan,false,-1,0,3); }
  if(Ship->bHasDestination) { DrawDebugLine(GetWorld(),P,Ship->Destination,FColor(90,200,140),false,-1,0,3); DrawDebugSphere(GetWorld(),Ship->Destination,70,12,FColor(90,200,140),false,-1,0,2); }
 }
 if(bPlacing) {
  const FVector Ground(Preview.X,Preview.Y,0);
  DrawDebugLine(GetWorld(),Ground,Preview,FColor::Yellow,false,-1,0,5);
  DrawDebugSphere(GetWorld(),Preview,90,16,FColor::Yellow,false,-1,0,3);
  DrawDebugCircle(GetWorld(),Ground,180,32,FColor::Yellow,false,-1,0,3,FVector::ForwardVector,FVector::RightVector,false);
  if(Selected) DrawDebugLine(GetWorld(),Selected->GetActorLocation(),Preview,FColor::Yellow,false,-1,0,2);
 }
}
