#include "OVController.h"
#include "OVShip.h"
#include "OVHUD.h"
#include "OVShipSystemsComponent.h"
#include "GameFramework/InputSettings.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameViewportClient.h"
#include "UnrealClient.h"
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
 for(int32 Index=0;Index<Ships.Num();++Index) {
  const FString Path=FString::Printf(TEXT("/Game/Layouts/DA_Test%dDeck.DA_Test%dDeck"),Index+1,Index+1);
  UShipLayoutDataAsset* Layout=LoadObject<UShipLayoutDataAsset>(nullptr,*Path);
  if(!Layout) Layout=UShipLayoutDataAsset::MakeTestLayout(this,Index+1);
  Ships[Index]->Systems->FactionId=Ships[Index]->bFriendly?0:1;
  Ships[Index]->Systems->Initialize(Layout);
 }
 SelectShip(Ships[0]); UpdateCamera();
 UE_LOG(LogTemp,Display,TEXT("OV: ControlRange ready; two friendly ships + one target"));
}
void AOVController::SelectShip(AOVShip* Ship)
{
 if(!Ship) { ClearSelection(); return; }
 for(AOVShip* Other:Ships) Other->bSelected=false;
 Selection.Reset(); Selection.Add(Ship);
 Selected=Ship; Selected->bSelected=true; bPlacing=false; InputMode=EOVInputMode::None; bFollow=false;
 ViewedDeck=0; SelectedSlot=0; SelectedArmor=0; SelectedWeapon=0;
 if(AOVHUD* HUD=Cast<AOVHUD>(GetHUD())) HUD->OpenDetails();
 LastAction=TEXT("Selected: ")+Ship->ShipName;
 UE_LOG(LogTemp,Display,TEXT("OV: Select %s"),*Ship->ShipName);
}
void AOVController::ClearSelection()
{
 for(AOVShip* Ship:Ships) Ship->bSelected=false;
 Selection.Reset(); Selected=nullptr; bPlacing=false; InputMode=EOVInputMode::None; bFollow=false;
}
void AOVController::SelectAdditional(AOVShip* Ship)
{
 if(!Ship) return;
 if(!Selected) { SelectShip(Ship); return; }
 if(Selection.Contains(Ship)) { Selection.Remove(Ship); Ship->bSelected=false; if(Selected==Ship) { Selected=Selection.Num()?Selection[0]:nullptr; ViewedDeck=0; SelectedSlot=0; SelectedArmor=0; SelectedWeapon=0; bPlacing=false; InputMode=EOVInputMode::None; bFollow=false; } }
 else { Selection.Add(Ship); Ship->bSelected=true; }
}
void AOVController::SetTarget(AOVShip* Ship)
{
 if(Selected && Ship && Ship!=Selected && Ship->bFriendly!=Selected->bFriendly && !Ship->Systems->bWrecked) {
  for(AOVShip* Vessel:Selection) if(Vessel->bFriendly && Vessel->Systems->CanCommand()) Vessel->AttackTarget=Ship;
  LastAction=TEXT("Target only: ")+Ship->ShipName+TEXT(" (weapons not active in 2A)");
 }
 InputMode=EOVInputMode::None;
}
bool AOVController::ActionPressed(FName Name) const
{
 TArray<FInputActionKeyMapping> Mappings; GetDefault<UInputSettings>()->GetActionMappingByName(Name,Mappings);
 for(const auto& Mapping:Mappings) if(WasInputKeyJustPressed(Mapping.Key)
  && (!Mapping.bShift || IsInputKeyDown(EKeys::LeftShift)||IsInputKeyDown(EKeys::RightShift))
  && (!Mapping.bCtrl || IsInputKeyDown(EKeys::LeftControl)||IsInputKeyDown(EKeys::RightControl))
  && (!Mapping.bAlt || IsInputKeyDown(EKeys::LeftAlt)||IsInputKeyDown(EKeys::RightAlt))) return true;
 return false;
}
void AOVController::Action(FName Name)
{
 if(Name==TEXT("Pause")) { bSimulationPaused=!bSimulationPaused; LastAction=bSimulationPaused?TEXT("Simulation paused - camera and orders remain active"):TEXT("Simulation resumed"); }
 if(Name==TEXT("Speed1")) SimulationSpeed=1;
 if(Name==TEXT("Speed2")) SimulationSpeed=2;
 if(Name==TEXT("Speed3")) SimulationSpeed=3;
 if(Name==TEXT("Ship1")) SelectShip(Ships[0]);
 if(Name==TEXT("Ship2")) SelectShip(Ships[1]);
 if(Name==TEXT("Ship3")) SelectShip(Ships[2]);
 if(Name==TEXT("Deselect")) ClearSelection();
 if(Name==TEXT("Developer")) { bDeveloperMode=!bDeveloperMode; ViewedDeck=0; }
 if(Selected && Selected->bFriendly && Selected->Systems->CanCommand()) {
  if(Name==TEXT("Manual")) { Selected->bManual=true; LastAction=TEXT("MANUAL - persists until AUTO is clicked"); }
  if(Name==TEXT("Auto")) { Selected->bManual=false; bPlacing=false; InputMode=EOVInputMode::None; LastAction=TEXT("AUTO / Hold and manual attitude preserved; combat deferred"); }
  if(Name==TEXT("Move")) { bPlacing=true; InputMode=EOVInputMode::Move; Preview=Selected->GetActorLocation(); PlacementPlaneZ=Preview.Z; OrderInput.Reset(); LastAction=TEXT("Cursor XY; hold Shift/RMB + drag Z; LMB/Enter confirms"); }
  if(Name==TEXT("Up") && bPlacing) Preview.Z=FMath::Min(Preview.Z+250,4000.0);
  if(Name==TEXT("Down") && bPlacing) Preview.Z=FMath::Max(Preview.Z-250,-1500.0);
  if(Name==TEXT("Confirm")) ConfirmMove();
  if(Name==TEXT("Stop")) { for(AOVShip* Ship:Selection) Ship->StopMovement(); bPlacing=false; InputMode=EOVInputMode::None; LastAction=TEXT("Braking to persistent HOLD"); }
  if(Name==TEXT("Target")) { bPlacing=false; InputMode=EOVInputMode::Target; LastAction=TEXT("Click enemy: target selection does not fire or change mode"); }
  if(Name==TEXT("ClearTarget")) Selected->AttackTarget=nullptr;
  if(Name==TEXT("Heading")) { bPlacing=false; InputMode=EOVInputMode::Heading; PlacementPlaneZ=Selected->GetActorLocation().Z; LastAction=TEXT("Click world to set heading; hold current pitch and roll"); }
  if(Name==TEXT("Port") || Name==TEXT("Starboard")) { FRotator Attitude=Selected->GetActorRotation(); Attitude.Yaw+=Name==TEXT("Port")?-90:90; Selected->SetManualAttitude(Attitude); }
  if(Name==TEXT("Repair")) LastAction=TEXT("R reserved: supply repair not implemented; use explicit DEV tools");
 }
 if(Name==TEXT("Cancel")) { bPlacing=false; InputMode=EOVInputMode::None; bRightGesture=false; }
 if(Name==TEXT("Focus") && Selected) { bFollow=!bFollow; CameraFocus=Selected->GetActorLocation(); }
 if(Name==TEXT("Details")) if(AOVHUD* HUD=Cast<AOVHUD>(GetHUD())) HUD->OpenDetails();
 if(Selected && Selected->Systems->Layout) {
  UOVShipSystemsComponent* S=Selected->Systems;
  const bool Reveal=Selected->bFriendly || bDeveloperMode;
  const FString ActionString=Name.ToString();
  if(Name==TEXT("External")) ViewedDeck=0;
  if(Reveal && ActionString.StartsWith(TEXT("Deck"))) ViewedDeck=FMath::Clamp(FCString::Atoi(*ActionString.Mid(4)),1,S->Layout->DeckCount);
  if(Reveal && ActionString.StartsWith(TEXT("Slot_"))) SelectedSlot=FMath::Clamp(FCString::Atoi(*ActionString.Mid(5)),0,S->Slots.Num()-1);
  if(ActionString.StartsWith(TEXT("Armor_"))) SelectedArmor=FMath::Clamp(FCString::Atoi(*ActionString.Mid(6)),0,S->ArmorSections.Num()-1);
  if(ActionString.StartsWith(TEXT("Weapon_"))) SelectedWeapon=FMath::Clamp(FCString::Atoi(*ActionString.Mid(7)),0,S->WeaponMountStates.Num()-1);
  if(bDeveloperMode && S->Slots.IsValidIndex(SelectedSlot)) {
   const FName Slot=S->Slots[SelectedSlot].SlotId;
   if(Name==TEXT("TestHit")) TestArmorHit();
   if(Name==TEXT("DestroyExt")) S->DebugDestroySlot(Slot,EOVDamageCause::External);
   if(Name==TEXT("Sabotage")) S->DebugDestroySlot(Slot,EOVDamageCause::Sabotage);
   if(Name==TEXT("RepairSlot")) S->DebugRepairSlot(Slot);
   if(Name==TEXT("CrewDown")) S->DebugSetCrew(Slot,S->Slots[SelectedSlot].Crew-2);
   if(Name==TEXT("CrewUp")) S->DebugSetCrew(Slot,S->Slots[SelectedSlot].Crew+2);
   if(Name==TEXT("Occupy")) S->DebugSetOccupation(Slot,S->Slots[SelectedSlot].OccupyingFaction<0?1-S->FactionId:-1);
   if(Name==TEXT("Reset")) { S->ResetState(); Selected->DebugResetMotion(); }
   if(Name==TEXT("ArmorZero") && S->ArmorSections.IsValidIndex(SelectedArmor)) S->DebugSetArmor(S->ArmorSections[SelectedArmor].ArmorSectionId,0);
   if(Name==TEXT("RepairArmor") && S->ArmorSections.IsValidIndex(SelectedArmor)) S->DebugSetArmor(S->ArmorSections[SelectedArmor].ArmorSectionId,S->Layout->ArmorSections[SelectedArmor].MaxArmor);
   if(Name==TEXT("WeaponHit") && S->WeaponMountStates.IsValidIndex(SelectedWeapon)) S->DebugHitWeapon(S->WeaponMountStates[SelectedWeapon].WeaponMountId,TestDamage);
   if(Name==TEXT("RepairWeapon") && S->WeaponMountStates.IsValidIndex(SelectedWeapon)) S->DebugRepairWeapon(S->WeaponMountStates[SelectedWeapon].WeaponMountId);
  }
 }
 UE_LOG(LogTemp,Display,TEXT("OV: Action=%s paused=%d speed=%.0f"),*Name.ToString(),bSimulationPaused,SimulationSpeed);
}
void AOVController::ConfirmMove()
{
 if(!bPlacing || !Selected || !Selected->bFriendly || !Selected->Systems->CanCommand()) return;
 for(AOVShip* Ship:Selection) if(Ship->bFriendly && Ship->Systems->CanCommand()) { Ship->bManual=true; Ship->SetDestination(Preview+(Ship->GetActorLocation()-Selected->GetActorLocation())); }
 bPlacing=false; InputMode=EOVInputMode::None;
 LastAction=FString::Printf(TEXT("MANUAL order: X %.0f  Y %.0f  Z %.0f"),Preview.X,Preview.Y,Preview.Z);
 UE_LOG(LogTemp,Display,TEXT("OV: %s"),*LastAction);
}
void AOVController::UpdatePreview()
{
 FVector Origin,Direction; float X=0,Y=0; GetMousePosition(X,Y);
 const AOVHUD* HUD=Cast<AOVHUD>(GetHUD());
 const auto* Viewport=GetWorld()->GetGameViewport();
 const bool Blocked=!Viewport || !Viewport->Viewport || !Viewport->Viewport->HasFocus() || !DeprojectMousePositionToWorld(Origin,Direction) || (HUD && HUD->IsOverPanel(X,Y));
 SampleOrderCursor(Origin,Direction,Y,IsInputKeyDown(EKeys::LeftShift)||IsInputKeyDown(EKeys::RightShift)||IsInputKeyDown(EKeys::RightMouseButton),Blocked);
}
void AOVController::SampleOrderCursor(FVector Origin,FVector Direction,float MouseY,bool Altitude,bool Blocked)
{
 FVector Raw; const bool Valid=FOVOrderInput::Project(Origin,Direction,PlacementPlaneZ,Raw,ProjectionNearDistance);
 OrderInput.Sample(Preview,Raw,MouseY,Altitude,Blocked||!Valid,AltitudeUnitsPerPixel);
}
void AOVController::IssueHeading(FVector Point)
{
 if(!Selected || !Selected->Systems->CanCommand()) return;
 FRotator Heading=Selected->GetActorRotation(); const FVector To=Point-Selected->GetActorLocation();
 if(To.SizeSquared2D()>1) { Heading.Yaw=To.Rotation().Yaw; Selected->SetManualAttitude(Heading); }
 InputMode=EOVInputMode::None;
}
void AOVController::TestArmorHit()
{
 if(!bDeveloperMode || !Selected || !Selected->Systems->Layout || !Selected->Systems->Layout->ArmorSections.IsValidIndex(SelectedArmor)) return;
 const auto& Armor=Selected->Systems->Layout->ArmorSections[SelectedArmor];
 FVector Normal=Armor.LocalCenter.GetSafeNormal();
 Selected->Systems->ApplyWorldHit(Selected->GetActorTransform().TransformPosition(Armor.LocalCenter),Selected->GetActorTransform().TransformVectorNoScale(Normal),TestDamage,EOVDamageCause::External);
}
void AOVController::UpdateCamera()
{
 const FRotator Rotation(CameraPitch,CameraYaw,0);
 // Frame the focus in the usable area to the right of the HUD.
 int32 Width=1280,Height=720; GetViewportSize(Width,Height);
 const float UIScale=FMath::Min(1.f,FMath::Min(Width/1280.f,Height/720.f));
 const float Offset=0;
 RangeCamera->SetActorLocationAndRotation(CameraFocus-Rotation.Vector()*CameraDistance-FRotationMatrix(Rotation).GetUnitAxis(EAxis::Y)*Offset,Rotation);
}
void AOVController::StepSimulation(float DeltaTime)
{
 if(bSimulationPaused || DeltaTime<=0 || !FMath::IsFinite(DeltaTime)) return;
 float Remaining=DeltaTime*SimulationSpeed; SimulationSeconds+=Remaining;
 while(Remaining>UE_SMALL_NUMBER) {
  const float Dt=FMath::Min(Remaining,1.f/60.f); Remaining-=Dt;
  for(AOVShip* Ship:Ships) {
   Ship->Systems->Simulate(Dt);
   if(Ship->Systems->bWrecked || Ship->Systems->bNeutral) { Ship->StopMovement(); Ship->Velocity=FVector::ZeroVector; Ship->AttackTarget=nullptr; continue; }
   if(Ship->AttackTarget&&Ship->AttackTarget->Systems->bWrecked) Ship->AttackTarget=nullptr;
   if(!Ship->Systems->CanCommand()) { Ship->bManual=false; Ship->StopMovement(); }
   if(Ship->bFriendly) Ship->Simulate(Dt);
  }
  ResolveContacts(Dt);
 }
}
void AOVController::ResolveContacts(float SimDelta)
{
 // Each unordered pair is resolved exactly once per substep. No ramming damage in 2A.
 for(int32 I=0;I<Ships.Num();++I) for(int32 J=I+1;J<Ships.Num();++J) {
  AOVShip* A=Ships[I]; AOVShip* B=Ships[J];
  if(A->Systems->bWrecked||B->Systems->bWrecked) continue;
  const float RadiusA=100*A->GetActorScale3D().GetMax(); const float RadiusB=100*B->GetActorScale3D().GetMax();
  FVector Delta=B->GetActorLocation()-A->GetActorLocation(); const double Distance=Delta.Size();
  const FVector Normal=Distance>0.001?Delta/Distance:FVector::RightVector;
  const bool MoveA=A->Systems->CanMove()&&!A->bFTLCharging;
  const bool MoveB=B->Systems->CanMove()&&!B->bFTLCharging;
  if(!MoveA&&!MoveB) continue;
  if(Distance<RadiusA+RadiusB+80) {
   // Soft avoidance steers velocity before hard overlap correction.
   const float Push=40*SimDelta;
   if(MoveA&&!A->bHold) A->Velocity-=Normal*Push;
   if(MoveB&&!B->bHold) B->Velocity+=Normal*Push;
  }
  if(Distance<RadiusA+RadiusB) {
   const FVector Correction=Normal*(RadiusA+RadiusB-Distance+0.1);
   if(MoveA) A->AddActorWorldOffset(-Correction*(MoveB?0.5:1));
   if(MoveB) B->AddActorWorldOffset(Correction*(MoveA?0.5:1));
   ++PairResolutionCount;
  }
 }
}
void AOVController::PlayerTick(float DeltaTime)
{
 Super::PlayerTick(DeltaTime);
 if(!RangeCamera) return;
 float MX=0,MY=0; GetMousePosition(MX,MY);
 AOVHUD* HUD=Cast<AOVHUD>(GetHUD());
 float DX=0,DY=0; GetInputMouseDelta(DX,DY);
 const int Wheel=(WasInputKeyJustPressed(EKeys::MouseScrollUp)?1:0)-(WasInputKeyJustPressed(EKeys::MouseScrollDown)?1:0);
 const bool OverUI=HUD && HUD->HandlePointer(MX,MY,WasInputKeyJustPressed(EKeys::LeftMouseButton),IsInputKeyDown(EKeys::LeftMouseButton),Wheel,IsInputKeyDown(EKeys::MiddleMouseButton),DX,DY);
 const bool Shift=IsInputKeyDown(EKeys::LeftShift)||IsInputKeyDown(EKeys::RightShift);
 const bool Ctrl=IsInputKeyDown(EKeys::LeftControl)||IsInputKeyDown(EKeys::RightControl);
 const float Dt=FMath::Min(DeltaTime,0.1f);
 const FRotator Flat(0,CameraYaw,0);
 FVector Pan=FVector::ZeroVector;
 if(!OverUI) {
 if(IsInputKeyDown(EKeys::W)||IsInputKeyDown(EKeys::Up)) Pan+=Flat.Vector();
 if(IsInputKeyDown(EKeys::S)||IsInputKeyDown(EKeys::Down)) Pan-=Flat.Vector();
 if(IsInputKeyDown(EKeys::D)||IsInputKeyDown(EKeys::Right)) Pan+=FRotationMatrix(Flat).GetUnitAxis(EAxis::Y);
 if(IsInputKeyDown(EKeys::A)||IsInputKeyDown(EKeys::Left)) Pan-=FRotationMatrix(Flat).GetUnitAxis(EAxis::Y);
 if(IsInputKeyDown(EKeys::E)) Pan.Z+=1;
 if(IsInputKeyDown(EKeys::Q)) Pan.Z-=1;
 }
 if(!Pan.IsNearlyZero()) bFollow=false;
 if(bFollow&&Selected) CameraFocus=Selected->GetActorLocation();
 CameraFocus+=Pan.GetClampedToMaxSize(1)*CameraDistance*0.45*Dt;
 if(!OverUI&&IsInputKeyDown(EKeys::MiddleMouseButton)) { CameraYaw+=DX*0.3f; CameraPitch=FMath::Clamp(CameraPitch-DY*0.3f,-85.f,-10.f); }
 if(Wheel&&!OverUI) { if(bPlacing) { Preview.Z=FMath::Clamp(Preview.Z+Wheel*250,-1500.0,4000.0); OrderInput.bShowAltitude=true; OrderInput.bReanchor=true; } else CameraDistance=FMath::Clamp(CameraDistance*FMath::Pow(0.86f,Wheel),1200.f,16000.f); }
 if(!OverUI&&WasInputKeyJustPressed(EKeys::Z)) CameraYaw-=15;
 if(!OverUI&&WasInputKeyJustPressed(EKeys::C)) CameraYaw+=15;
 if(WasInputKeyJustPressed(EKeys::PageUp)&&!OverUI) CameraPitch=FMath::Clamp(CameraPitch-10,-85.f,-10.f);
 if(!OverUI&&WasInputKeyJustPressed(EKeys::G)) CameraPitch=FMath::Clamp(CameraPitch+10,-85.f,-10.f);
 if(!OverUI&&WasInputKeyJustPressed(EKeys::Home)) { CameraFocus=FVector::ZeroVector; CameraYaw=-45; CameraPitch=-48; CameraDistance=6500; }
 if(WasInputKeyJustPressed(EKeys::F9)) UE_LOG(LogTemp,Display,TEXT("OV VERIFY: camera=%s yaw=%.1f pitch=%.1f distance=%.1f ship=%s pos=%s manual=%d moving=%d paused=%d speed=%.0f sim=%.3f"),*CameraFocus.ToString(),CameraYaw,CameraPitch,CameraDistance,Selected?*Selected->ShipName:TEXT("none"),Selected?*Selected->GetActorLocation().ToString():TEXT("none"),Selected?Selected->bManual:0,Selected?Selected->bHasDestination:0,bSimulationPaused,SimulationSpeed,SimulationSeconds);
 if(ActionPressed(TEXT("OVPause"))) Action(TEXT("Pause"));
 if(ActionPressed(TEXT("OVSpeed1"))) Action(TEXT("Speed1"));
 if(ActionPressed(TEXT("OVSpeed2"))) Action(TEXT("Speed2"));
 if(ActionPressed(TEXT("OVSpeed3"))) Action(TEXT("Speed3"));
 if(ActionPressed(TEXT("OVCancel"))) Action(TEXT("Cancel"));
 if(!OverUI) {
  if(ActionPressed(TEXT("OVFollow"))) Action(TEXT("Focus"));
  if(ActionPressed(TEXT("OVMove"))) Action(TEXT("Move"));
  if(ActionPressed(TEXT("OVTarget"))) Action(TEXT("Target"));
  if(ActionPressed(TEXT("OVHeading"))) Action(TEXT("Heading"));
  if(ActionPressed(TEXT("OVStop"))) Action(TEXT("Stop"));
  if(ActionPressed(TEXT("OVRepair"))) Action(TEXT("Repair"));
  for(int32 I=0;I<9;++I) if(ActionPressed(FName(*FString::Printf(TEXT("OVGroup%d"),I+1)))) {
   if(Ctrl) { auto& Group=Groups.FindOrAdd(I); Group.Reset(); for(AOVShip* S:Selection) if(S->bFriendly) Group.Add(S); }
   else if(const auto* Group=Groups.Find(I)) { ClearSelection(); for(const auto& S:*Group) if(S.IsValid()&&!S->Systems->bWrecked) SelectAdditional(S.Get()); }
  }
  if(WasInputKeyJustPressed(EKeys::RightMouseButton)) {
   bRightGesture=true; RightStart=FVector2D(MX,MY); RightTarget=nullptr;
   FHitResult Hit; if(!Shift&&GetHitResultUnderCursor(ECC_Visibility,false,Hit)) {
    AOVShip* Target=Cast<AOVShip>(Hit.GetActor()); if(Target&&Selected&&Target->bFriendly!=Selected->bFriendly) RightTarget=Target;
   }
   if(!RightTarget.IsValid()&&!bPlacing) { Action(TEXT("Move")); FVector O,D,Raw; if(DeprojectMousePositionToWorld(O,D)&&FOVOrderInput::Project(O,D,PlacementPlaneZ,Raw,ProjectionNearDistance)) { Preview.X=Raw.X; Preview.Y=Raw.Y; } }
  }
 }
 if(bRightGesture&&WasInputKeyJustReleased(EKeys::RightMouseButton)) {
  if(!OverUI&&RightTarget.IsValid()&&FVector2D::Distance(RightStart,FVector2D(MX,MY))<=25) SetTarget(RightTarget.Get());
  bRightGesture=false; RightTarget=nullptr;
 }
 UpdateCamera();
 if(bPlacing) UpdatePreview();
 if(!OverUI&&WasInputKeyJustPressed(EKeys::Enter)) ConfirmMove();
 if(WasInputKeyJustPressed(EKeys::LeftMouseButton) && !OverUI) {
  if(bPlacing) ConfirmMove();
  else if(InputMode==EOVInputMode::Heading) { FVector O,D,P; if(DeprojectMousePositionToWorld(O,D)&&FOVOrderInput::Project(O,D,PlacementPlaneZ,P,ProjectionNearDistance)) IssueHeading(P); }
  else { FHitResult Hit; AOVShip* Ship=nullptr; if(GetHitResultUnderCursor(ECC_Visibility,false,Hit)) Ship=Cast<AOVShip>(Hit.GetActor());
   if(InputMode==EOVInputMode::Target) { if(Ship) SetTarget(Ship); }
   else if(Shift&&Ship) SelectAdditional(Ship); else SelectShip(Ship);
  }
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
  if(Ship->AttackTarget&&!Ship->AttackTarget->Systems->bWrecked) { DrawDebugLine(GetWorld(),P,Ship->AttackTarget->GetActorLocation(),FColor::Red,false,-1,0,2); DrawDebugSphere(GetWorld(),Ship->AttackTarget->GetActorLocation(),150,12,FColor::Red,false,-1,0,2); }
 }
 if(Selected&&Selected->Systems->Layout) {
  auto* S=Selected->Systems.Get(); const auto* L=S->Layout.Get(); const FTransform Transform=Selected->GetActorTransform();
  if(L->ArmorSections.IsValidIndex(SelectedArmor)) { const auto& A=L->ArmorSections[SelectedArmor]; DrawDebugBox(GetWorld(),Transform.TransformPosition(A.LocalCenter),A.HalfExtent*Transform.GetScale3D(),Transform.GetRotation(),FColor::Orange,false,-1,0,3); }
  if(S->HitEventId>0&&bDeveloperMode) { DrawDebugSphere(GetWorld(),S->LastWorldHit,40,10,FColor::Magenta,false,-1,0,4); if(L->Slots.IsValidIndex(S->LastDamageIndex)) DrawDebugLine(GetWorld(),S->LastWorldHit,Transform.TransformPosition(L->Slots[S->LastDamageIndex].LocalCenter),FColor::Magenta,false,-1,0,3); }
 }
 if(bPlacing) {
  const FVector Ground(Preview.X,Preview.Y,PlacementPlaneZ);
  // The disc and projection use exactly the same selected-ship elevation.
  DrawDebugCircle(GetWorld(),Ground,600,48,FColor(30,110,140),false,-1,0,2,FVector::ForwardVector,FVector::RightVector,false);
  if(OrderInput.bShowAltitude) DrawDebugLine(GetWorld(),Ground,Preview,FColor::Yellow,false,-1,0,5);
  DrawDebugSphere(GetWorld(),Preview,90,16,FColor::Yellow,false,-1,0,3);
  DrawDebugCircle(GetWorld(),Ground,180,32,FColor::Yellow,false,-1,0,3,FVector::ForwardVector,FVector::RightVector,false);
  if(Selected) DrawDebugLine(GetWorld(),Selected->GetActorLocation(),Preview,FColor::Yellow,false,-1,0,2);
 }
}

