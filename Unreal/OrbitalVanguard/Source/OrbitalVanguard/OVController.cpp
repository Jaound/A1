#include "OVController.h"
#include "OVShip.h"
#include "OVHUD.h"
#include "OVCalibration.h"
#include "OVShipSystemsComponent.h"
#include "OVCombatSubsystem.h"
#include "OVCarrierSubsystem.h"
#include "OVPointDefenseSubsystem.h"
#include "OVTransportSubsystem.h"
#include "OVInteriorSubsystem.h"
#include "OVMissionSubsystem.h"
#include "GameFramework/InputSettings.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameViewportClient.h"
#include "UnrealClient.h"
#include "InputKeyEventArgs.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "DrawDebugHelpers.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"

AOVController::AOVController() { bShowMouseCursor=true; bEnableClickEvents=true; bEnableMouseOverEvents=true; }
void AOVController::BeginPlay()
{
 Super::BeginPlay();
 LegacyTraceDistance=HitResultTraceDistance;
 Combat=GetWorld()->GetSubsystem<UOVCombatSubsystem>();
 Carrier=GetWorld()->GetSubsystem<UOVCarrierSubsystem>();
 PointDefense=GetWorld()->GetSubsystem<UOVPointDefenseSubsystem>();
 Transport=GetWorld()->GetSubsystem<UOVTransportSubsystem>();
 Interior=GetWorld()->GetSubsystem<UOVInteriorSubsystem>();
 Mission=GetWorld()->GetSubsystem<UOVMissionSubsystem>();
 FInputModeGameAndUI Mode; Mode.SetHideCursorDuringCapture(false); Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock); SetInputMode(Mode);
 RangeCamera=GetWorld()->SpawnActor<ACameraActor>();
 RangeCamera->GetCameraComponent()->SetFieldOfView(55);
 // Keep HUD and mouse coordinates in the same full viewport on ultrawide/resized windows.
 RangeCamera->GetCameraComponent()->bConstrainAspectRatio=false;
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
 SelectShip(Ships[0]); ResetCameraProfile();
 UE_LOG(LogTemp,Display,TEXT("OV: ControlRange ready; two friendly ships + one target"));
 // Optional local play/viewport quality harness. Normal Editor/PIE startup is unchanged.
 int32 RequestedScene=-1;
 if(FParse::Value(FCommandLine::Get(),TEXT("OV2CScene="),RequestedScene)&&StartPhase2CRange(RequestedScene)) {
  if(FParse::Param(FCommandLine::Get(),TEXT("OVAutoRun"))) { for(AOVShip* Ship:Ships)Ship->SetAutomation(false,true);bSimulationPaused=false; }
 }
 int32 RequestedMission=-1;
 if(FParse::Value(FCommandLine::Get(),TEXT("OVMission="),RequestedMission) && RequestedMission>=0 && RequestedMission<=2)
  Mission->StartMission(this,static_cast<EOVMissionMode>(RequestedMission));
}
void AOVController::StartCombatRange()
{
 if(!Combat || Ships.Num()<3) return;
 ResetPhase2C();
 Action(TEXT("Cancel")); OrbitGesture={}; Groups.Reset();
 Combat->DisableCombat();
 const bool bRestoreLegacy = CalibrationScenario >= 0;
 CalibrationScenario = -1;
 HitResultTraceDistance = LegacyTraceDistance;
 const FVector Positions[]={FVector(-1400,-700,300),FVector(-1600,800,300),FVector(1600,0,300)};
 for(int32 I=0;I<Ships.Num();++I) {
  AOVShip* Ship=Ships[I];
  if (bRestoreLegacy)
  {
   const TCHAR* Names[] = {TEXT("01 / KESTREL - Escort"), TEXT("02 / ATLAS - Cruiser"), TEXT("03 / RED SIGNAL - Target")};
   const float Speeds[] = {850,420,0}, Turns[] = {90,24,0}, Scales[] = {1.4f,2.5f,1.8f};
   const FLinearColor Colors[] = {{.1f,.8f,1},{.3f,.5f,1},{1,.12f,.08f}};
   Ship->Configure(Names[I],I!=2,Speeds[I],Turns[I],FVector(Scales[I]),Colors[I]);
   Ship->SetCalibrationHull(nullptr);
   Ship->Systems->Tuning=GetDefault<UOVShipSystemsComponent>()->Tuning;
  }
  // Transient per-run fixture. Authored 2A Content assets are never modified.
  UShipLayoutDataAsset* Layout=UShipLayoutDataAsset::MakeTestLayout(this,I+1);
  if(I==0) { Layout->WeaponMounts.RemoveAt(2); Layout->MissileSlots=0; }
  if(I==1) {
   FOVWeaponMountDefinition Guided=Layout->WeaponMounts[2];
   Guided.WeaponId=TEXT("Weapon_3"); Guided.LocalPosition=FVector(0,0,-40);
   Layout->WeaponMounts.Add(Guided); ++Layout->MissileSlots;
  }
  Layout->WeaponTier=I==0?1:2;
  for(auto& Mount:Layout->WeaponMounts) Mount.Tier=Layout->WeaponTier;
  Ship->Systems->FactionId=I==2?1:0;
  Ship->Systems->Initialize(Layout);
  Ship->DebugResetMotion();
  Ship->bFriendly=I!=2;
  Ship->SetActorHiddenInGame(false);
  Ship->SetActorEnableCollision(true);
  Ship->SetActorLocationAndRotation(Positions[I],FRotator(0,I==2?180:0,0));
  Ship->SetAutomation(I==2,I==2);
 }
 TArray<AOVShip*> Combatants; for(AOVShip* Ship:Ships) Combatants.Add(Ship);
 Combat->EnableCombat(Combatants,260112);
 Combat->RegisterShip(Ships[1],2,true);
 Combat->SetWeaponProfile(Ships[1],TEXT("Weapon_3"),TEXT("GuidedMissile_T2"));
 bSimulationPaused=true; SimulationSpeed=1; SimulationSeconds=0;
 SelectShip(Ships[1]); SelectedWeapon=1;
 // Frame the wider combat fixture; profile gains and all subsequent controls stay unchanged.
 CameraFocus=FVector(0,0,300); CameraDistance=6500; UpdateCamera();
 LastAction=TEXT("전투 준비 · T 표적 지정 → B 선택 무장 / L 전체 발사 · F1 진행");
 UE_LOG(LogTemp,Display,TEXT("OV2B: Combat range reset, seed=260112, paused"));
}
bool AOVController::StartCalibrationRange(int32 Scenario)
{
 if (!Combat || Scenario<0 || Scenario>2) return false;
 ResetPhase2C();
 TArray<AOVShip*> Combatants;
 for (AOVShip* Ship : Ships) Combatants.Add(Ship);
 if (!OVCalibration::Start(*Combat, Combatants, Scenario)) return false;

 Action(TEXT("Cancel"));
 OrbitGesture={};
 Groups.Reset();
 CalibrationScenario=Scenario;
 HitResultTraceDistance=OVCalibration::RayLength;
 bSimulationPaused=true;
 SimulationSpeed=1;
 SimulationSeconds=0;
 SelectShip(Ships[0]);
 CameraFocus=FVector(0,0,2000);
 CameraDistance=OVCalibration::StartZoom; CameraYaw=-90; CameraPitch=-55;
 UpdateCamera();
 LastAction=TEXT("교정 전투 · F6 장면 변경 · F5 기본 전투 · T 표적 → B 발사 · F1 진행");
 return true;
}

double AOVController::CommandXY() const { return CalibrationScenario>=0 ? OVCalibration::XYLimit : 6000; }
double AOVController::CommandMinZ() const { return CalibrationScenario>=0 ? OVCalibration::MinZ : -1500; }
double AOVController::CommandMaxZ() const { return CalibrationScenario>=0 ? OVCalibration::MaxZ : 4000; }
double AOVController::CommandRayLength() const { return CalibrationScenario>=0 ? OVCalibration::RayLength : 100000; }
double AOVController::AltitudeScale() const { return CalibrationScenario>=0 ? OVCalibration::AltitudeScale : 1; }

void AOVController::SelectShip(AOVShip* Ship)
{
 CancelRightMove();
 if(Combat && Combat->bEnabled && Combat->IsDeparted(Ship)) Ship=nullptr;
 if(!Ship) { ClearSelection(); return; }
 for(AOVShip* Other:Ships) Other->bSelected=false;
 Selection.Reset(); Selection.Add(Ship);
 Selected=Ship; Selected->bSelected=true; bPlacing=false; InputMode=EOVInputMode::None; bFollow=false;
 ViewedDeck=0; SelectedSlot=0; SelectedArmor=0; SelectedWeapon=0;
 LastAction=TEXT("선택: ")+Ship->ShipName;
 UE_LOG(LogTemp,Display,TEXT("OV: Select %s"),*Ship->ShipName);
}
void AOVController::ClearSelection()
{
 CancelRightMove();
 for(AOVShip* Ship:Ships) Ship->bSelected=false;
 Selection.Reset(); Selected=nullptr; bPlacing=false; InputMode=EOVInputMode::None; bFollow=false;
}
void AOVController::SelectAdditional(AOVShip* Ship)
{
 if(!Ship) return;
 if(Combat && Combat->bEnabled && Combat->IsDeparted(Ship)) return;
 CancelRightMove();
 if(!Selected) { SelectShip(Ship); return; }
 if(Selection.Contains(Ship)) { Selection.Remove(Ship); Ship->bSelected=false; if(Selected==Ship) { Selected=Selection.Num()?Selection[0]:nullptr; ViewedDeck=0; SelectedSlot=0; SelectedArmor=0; SelectedWeapon=0; bPlacing=false; InputMode=EOVInputMode::None; bFollow=false; } }
 else { Selection.Add(Ship); Ship->bSelected=true; }
}
void AOVController::SetTarget(AOVShip* Ship)
{
 if(Combat && Combat->bEnabled && Combat->IsDeparted(Ship)) return;
 if(Selected && Ship && Ship!=Selected && Ship->bFriendly!=Selected->bFriendly && !Ship->Systems->bWrecked) {
  for(AOVShip* Vessel:Selection) if(Vessel->bFriendly && Vessel->Systems->CanCommand()) {
   if(Combat && Combat->bEnabled) Combat->SetTarget(Vessel,Ship); else Vessel->AttackTarget=Ship;
  }
  LastAction=TEXT("표적 지정: ")+Ship->ShipName+(Combat&&Combat->bEnabled?TEXT(" / B 선택 무장, L 전체 무장 발사"):TEXT(" (weapons not active in 2A)"));
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
 if(Mission && Mission->HandleAction(Name)) return;
 if(Phase2CAction(Name)) return;
 if(Name==TEXT("Phase2C")) { StartPhase2CRange((Phase2CScenario+1)%6); return; }
 if(Name==TEXT("RestartScene")) { if(Phase2CScenario>=0) StartPhase2CRange(Phase2CScenario); else if(CalibrationScenario>=0) StartCalibrationRange(CalibrationScenario); else StartCombatRange(); return; }
 if(Name==TEXT("Overview")) { FrameEngagement(); return; }
 if(Name.ToString().StartsWith(TEXT("Pick_")) || Name.ToString().StartsWith(TEXT("WorldPick_"))) {
  const int32 Index=FCString::Atoi(*Name.ToString().Mid(Name.ToString().StartsWith(TEXT("WorldPick_"))?10:5));
  if(Ships.IsValidIndex(Index)) { if(InputMode==EOVInputMode::Target) SetTarget(Ships[Index]); else SelectShip(Ships[Index]); } return;
 }
 if(Name==TEXT("CalibrationRange")) { StartCalibrationRange((CalibrationScenario+1)%3); return; }
 if(Name==TEXT("CombatRange")) { StartCombatRange(); return; }
 if(Name==TEXT("CameraProfile"))
 {
  Action(TEXT("Cancel"));
  OrbitGesture={};
  bPlayGame3=!bPlayGame3;
  ResetCameraProfile();
  return;
 }
 if(Name==TEXT("Move") || Name==TEXT("Stop") || Name==TEXT("Auto") ||
    Name==TEXT("AutoFree") || Name==TEXT("MoveAuto") || Name==TEXT("Target") || Name==TEXT("Heading") || Name==TEXT("Cancel"))
 {
  CancelRightMove();
 }
 if(Name==TEXT("Pause")) { bSimulationPaused=!bSimulationPaused; LastAction=bSimulationPaused?TEXT("일시정지 · 카메라와 명령 입력 가능"):TEXT("전투 진행"); }
 if(Name==TEXT("Speed1")) SimulationSpeed=1;
 if(Name==TEXT("Speed2")) SimulationSpeed=2;
 if(Name==TEXT("Speed3")) SimulationSpeed=3;
 if(Name==TEXT("Ship1")) SelectShip(Ships[0]);
 if(Name==TEXT("Ship2")) SelectShip(Ships[1]);
 if(Name==TEXT("Ship3")) SelectShip(Ships[2]);
 if(Name==TEXT("Deselect")) ClearSelection();
 if(Name==TEXT("Developer")) { bDeveloperMode=!bDeveloperMode; ViewedDeck=0; }
 if(Selected && Selected->bFriendly && Selected->Systems->CanCommand() && !(Combat&&Combat->bEnabled&&Combat->IsDeparted(Selected))) {
  if(Name==TEXT("Manual")) { Selected->SetAutomation(false,false); LastAction=TEXT("이동·사격 수동"); }
  if(Name==TEXT("Auto")) { Selected->SetAutomation(true,true); bPlacing=false; InputMode=EOVInputMode::None; LastAction=TEXT("AUTO / Hold and manual attitude preserved")+(Combat&&Combat->bEnabled?FString(TEXT(" / automatic engagement")):FString(TEXT("; combat deferred"))); }
  if(Name==TEXT("MoveAuto") && Combat && Combat->bEnabled) {
   Selected->SetAutomation(!Selected->IsMovementAutomatic(),Selected->IsAttackAutomatic());
   bPlacing=false; InputMode=EOVInputMode::None;
   LastAction=TEXT("이동 자동화 변경 · 사격과 정지/수동 자세 유지");
  }
  if(Name==TEXT("FireAuto") && Combat && Combat->bEnabled) {
   Selected->SetAutomation(Selected->IsMovementAutomatic(),!Selected->IsAttackAutomatic());
   LastAction=TEXT("사격 자동화 변경 · 이동 명령 유지");
  }
  if(Name==TEXT("AutoFree") && Combat && Combat->bEnabled) {
   Selected->SetAutomation(true,true);
   if(!Selected->bHasDestination && !Selected->bFTLCharging && !Selected->bManualAttitude) Selected->bHold=false;
   bPlacing=false; InputMode=EOVInputMode::None;
   LastAction=TEXT("AUTO FREE / release Hold only; existing destination, manual attitude and FTL remain authoritative");
  }
  if(Combat && Combat->bEnabled) {
   const auto Order=Combat->GetShipState(Selected);
   if(!Order.ForcedMount.IsNone() && (Name==TEXT("Cancel") || Name==TEXT("Move") || Name==TEXT("Stop") || Name==TEXT("Heading") || Name==TEXT("Manual") || Name==TEXT("Auto") || Name==TEXT("AutoFree") || Name==TEXT("FireAuto") || Name==TEXT("MoveAuto")))
    Combat->SetAttackIntent(Selected,Order.Intent);
   if(Name==TEXT("HullIntent")) Combat->SetAttackIntent(Selected,EOVAttackIntent::DestroyHull);
   if(Name==TEXT("GunIntent")) Combat->SetAttackIntent(Selected,EOVAttackIntent::DisableWeapons);
   if(Name==TEXT("CancelAttack")) Combat->CancelAttackOrder(Selected);
   if(Name==TEXT("Protection")) Combat->SetProtectionPolicy(Selected,static_cast<EOVProtectionPolicy>((static_cast<int32>(Order.Protection)+1)%3));
   if(Name==TEXT("ForceGun") && Selected->Systems->WeaponMountStates.IsValidIndex(SelectedWeapon)) {
    const FName M=Selected->Systems->WeaponMountStates[SelectedWeapon].WeaponMountId;
    if(Combat->SetAttackIntent(Selected,EOVAttackIntent::DisableWeapons,M)) Combat->RequestFire(Selected,M);
    LastAction=TEXT("Forced gun attack requested: missed shots can hit hull / interior ally risk");
   }
   if(bDeveloperMode && Selected->AttackTarget) {
    auto* TargetSystems=Selected->AttackTarget->Systems.Get();const int32 F=Selected->Systems->FactionId;
    if(Name==TEXT("DevInterior")) TargetSystems->SetInteriorPresence(F,!TargetSystems->HasInteriorPresence(F),false,true);
    if(Name==TEXT("DevObjective")) TargetSystems->SetInteriorPresence(F,true,!TargetSystems->IsInteriorObjectiveComplete(F),true);
   }
  }
  if(Name==TEXT("NextWeapon") && Selected->Systems->WeaponMountStates.Num()>0) {
   const auto* L=Selected->Systems->Layout.Get();
   for(int32 I=0;I<Selected->Systems->WeaponMountStates.Num();++I) { SelectedWeapon=(SelectedWeapon+1)%Selected->Systems->WeaponMountStates.Num(); if(!L||!L->WeaponMounts.IsValidIndex(SelectedWeapon)||L->WeaponMounts[SelectedWeapon].Type!=EOVWeaponType::Interception)break; }
  }
  if((Name==TEXT("Fire") || Name==TEXT("FireAll")) && Combat && Combat->bEnabled) {
   const auto& Mounts=Selected->Systems->WeaponMountStates;
   const FName Mount=Name==TEXT("FireAll")?NAME_None:(Mounts.IsValidIndex(SelectedWeapon)?Mounts[SelectedWeapon].WeaponMountId:NAME_None);
   const bool Accepted=Combat->RequestFire(Selected,Mount);
   LastAction=Accepted?TEXT("발사 요청 접수 · 조준/사거리/시설 조건 충족 시 한 차례 발사"):TEXT("발사 요청 거부 · 표적/소유/무장 상태 확인");
  }
  if(Name==TEXT("Move")) { bPlacing=true; InputMode=EOVInputMode::Move; Preview=Selected->GetActorLocation(); PlacementPlaneZ=Preview.Z; OrderInput.Reset(); LastAction=(bPlayGame3?TEXT("평면 위치 지정 · Shift 드래그 고도 · 클릭/Enter 확정"):TEXT("평면 위치 지정 · Shift/우클릭 고도 · 클릭/Enter 확정")); }
  if(Name==TEXT("Up") && bPlacing) Preview.Z=FMath::Min(Preview.Z+250*AltitudeScale(),CommandMaxZ());
  if(Name==TEXT("Down") && bPlacing) Preview.Z=FMath::Max(Preview.Z-250*AltitudeScale(),CommandMinZ());
  if(Name==TEXT("Confirm")) ConfirmMove();
  if(Name==TEXT("Stop")) { for(AOVShip* Ship:Selection) { Transport->SuspendShuttle(Ship); Ship->StopMovement(); } bPlacing=false; InputMode=EOVInputMode::None; LastAction=TEXT("감속 정지 · 위치 유지"); }
  if(Name==TEXT("Target")) { bPlacing=false; InputMode=EOVInputMode::Target; LastAction=TEXT("적을 클릭해 표적 지정 · 발사는 별도 B/L 명령"); }
  if(Name==TEXT("ClearTarget")) { if(Combat&&Combat->bEnabled) Combat->SetTarget(Selected,nullptr); else Selected->AttackTarget=nullptr; }
  if(Name==TEXT("Heading")) { Transport->SuspendShuttle(Selected); bPlacing=false; InputMode=EOVInputMode::Heading; PlacementPlaneZ=Selected->GetActorLocation().Z; LastAction=TEXT("전장을 클릭해 방향 지정 · 현재 상하 기울기 유지"); }
  if(Name==TEXT("Port") || Name==TEXT("Starboard")) { FRotator Attitude=Selected->GetActorRotation(); Attitude.Yaw+=Name==TEXT("Port")?-90:90; Selected->SetManualAttitude(Attitude); }
  if(Name==TEXT("Repair")) LastAction=TEXT("보급 수리 미지원 · 시험 수리는 DEV에서만 가능");
 }
 if(Name==TEXT("Cancel")) { bPlacing=false; InputMode=EOVInputMode::None; bRightGesture=false; }
 if(Name==TEXT("Focus") && Selected) { bFollow=!bFollow && !Selected->Systems->bWrecked; if(bFollow || !bPlayGame3) CameraFocus=Selected->GetActorLocation(); }
 if(Name==TEXT("Details")) if(AOVHUD* HUD=Cast<AOVHUD>(GetHUD())) HUD->OpenDetails();
 if(Selected && Selected->Systems->Layout) {
  UOVShipSystemsComponent* S=Selected->Systems;
  const bool Reveal=Selected->bFriendly || bDeveloperMode;
  const FString ActionString=Name.ToString();
  if(Name==TEXT("External")) { ViewedDeck=0; if(auto* HUD=Cast<AOVHUD>(GetHUD())) HUD->OpenDetails(); }
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
 for(AOVShip* Ship:Selection) if(Ship->bFriendly && Ship->Systems->CanCommand()) { Transport->SuspendShuttle(Ship); Ship->SetAutomation(false,Ship->IsAttackAutomatic()); Ship->SetDestination(Preview+(Ship->GetActorLocation()-Selected->GetActorLocation())); }
 bPlacing=false; InputMode=EOVInputMode::None;
 LastAction=FString::Printf(TEXT("이동 명령 수락 · X %.0fm  Y %.0fm  고도 %.0fm"),Preview.X/100,Preview.Y/100,Preview.Z/100);
 UE_LOG(LogTemp,Display,TEXT("OV: %s"),*LastAction);
}
void AOVController::UpdatePreview()
{
 FVector Origin=FVector::ZeroVector,Direction=FVector::ZeroVector; float X=0,Y=0;
 int32 Width=0,Height=0;
 GetViewportSize(Width,Height);
 const bool Inside=GetMousePosition(X,Y)&&X>=0&&Y>=0&&X<Width&&Y<Height;
 const AOVHUD* HUD=Cast<AOVHUD>(GetHUD());
 const auto* Viewport=GetWorld()->GetGameViewport();
 const bool Blocked=!Inside || !Viewport || !Viewport->Viewport || !Viewport->Viewport->HasFocus() || !DeprojectMousePositionToWorld(Origin,Direction) || (HUD && HUD->IsOverPanel(X,Y));
 SampleOrderCursor(Origin,Direction,Y,IsInputKeyDown(EKeys::LeftShift)||IsInputKeyDown(EKeys::RightShift)||(!bPlayGame3&&IsInputKeyDown(EKeys::RightMouseButton)),Blocked);
}
void AOVController::SampleOrderCursor(FVector Origin,FVector Direction,float MouseY,bool Altitude,bool Blocked)
{
 FVector Raw=Preview; const bool Valid=!Blocked&&FOVOrderInput::Project(Origin,Direction,PlacementPlaneZ,Raw,ProjectionNearDistance,CommandRayLength());
 OrderInput.Sample(Preview,Raw,MouseY,Altitude,Blocked||!Valid,static_cast<float>(AltitudeUnitsPerPixel*AltitudeScale()),CommandXY(),CommandMinZ(),CommandMaxZ());
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
void AOVController::ResetCameraProfile()
{
 bFollow=false;
 CameraFocus=FVector::ZeroVector;
 CameraYaw=bPlayGame3?32.f:-45.f;
 CameraPitch=bPlayGame3?-38.f:-48.f;
 CameraDistance=CalibrationScenario>=0 ? OVCalibration::StartZoom : (bPlayGame3?FOVPlayGame3Profile::InitialDistance:6500.f);
 UpdateCamera();
}

void AOVController::ApplyCameraInput(FVector2D OrbitDelta, FVector LocalPan, float YawAxis, int32 Wheel, float RealSeconds)
{
 const float Dt=FMath::Clamp(RealSeconds,0.f,.1f);
 CameraYaw=FRotator::NormalizeAxis(CameraYaw+OrbitDelta.X*(bPlayGame3?.32:.3)+YawAxis*75*Dt);
 CameraPitch=FMath::Clamp(CameraPitch-OrbitDelta.Y*(bPlayGame3?.28:.3),bPlayGame3?-84.:-85.,bPlayGame3?-12.:-10.);
 if(Wheel)
 {
  float MinDistance=1200;
  float MaxDistance=16000;
  double Multiplier=FMath::Pow(.86,Wheel);
  if(bPlayGame3)
  {
   MinDistance=35*FOVPlayGame3Profile::UnitsPerReferenceUnit;
   MaxDistance=FOVPlayGame3Profile::MaxDistance;
   if(Selected && Selected->Systems->Layout && !Selected->Systems->bWrecked)
   {
    const FVector HullExtent=Selected->Systems->Layout->LocalHalfExtent*Selected->GetActorScale3D();
    MinDistance=FMath::Clamp(1.45*HullExtent.Size(),double(MinDistance),double(MaxDistance));
   }
   Multiplier=Wheel>0?FMath::Pow(.88,Wheel):FMath::Pow(1.13,-Wheel);
  }
  if (CalibrationScenario>=0)
  {
   MaxDistance=OVCalibration::MaxZoom;
   MinDistance=3000;
   if (Selected && Selected->Systems->Layout)
    MinDistance=FMath::Max(MinDistance, float(1.45*(Selected->Systems->Layout->LocalHalfExtent*Selected->GetActorScale3D()).Size()));
  }
  CameraDistance=FMath::Clamp(CameraDistance*Multiplier,double(MinDistance),double(MaxDistance));
 }
 if(!LocalPan.IsNearlyZero())
 {
  bFollow=false;
  const double Speed=CalibrationScenario>=0 ? (bPlayGame3?FOVPlayGame3Profile::PanSpeed(CameraDistance/20)*20:CameraDistance*.45)
   : (bPlayGame3?FOVPlayGame3Profile::PanSpeed(CameraDistance):CameraDistance*.45);
  CameraFocus+=FRotator(0,CameraYaw,0).RotateVector(LocalPan.GetClampedToMaxSize(1))*Speed*Dt;
 }
 if(bFollow)
 {
  if(!IsValid(Selected) || Selected->Systems->bWrecked) bFollow=false;
  else CameraFocus=FMath::Lerp(CameraFocus,Selected->GetActorLocation(),bPlayGame3?1.0-FMath::Exp(-10.0*Dt):1.0);
 }
 UpdateCamera();
}

void AOVController::UpdateCamera()
{
 if(!RangeCamera) return;
 auto* Camera=RangeCamera->GetCameraComponent();
 Camera->bOverrideAspectRatioAxisConstraint=bPlayGame3;
 Camera->AspectRatioAxisConstraint=AspectRatio_MaintainXFOV;
 int32 Width=0, Height=0;
 GetViewportSize(Width,Height);
 const double Aspect=Width>0&&Height>0?double(Width)/Height:16.0/9.0;
 Camera->SetFieldOfView(bPlayGame3?FOVPlayGame3Profile::HorizontalFov(Aspect):55.f);
 const FRotator Rotation(CameraPitch,CameraYaw,0);
 RangeCamera->SetActorLocationAndRotation(CameraFocus-Rotation.Vector()*CameraDistance,Rotation);
}

void AOVController::CancelRightMove()
{
 RightMove={};
 RightCommander.Reset();
 RightTarget.Reset();
 bRightHeading=false;
}

void AOVController::FlushPressedKeys()
{
 Super::FlushPressedKeys();
 OrbitGesture={};
 bConsumeAltLeft=false;
 bRightGesture=false;
 CancelRightMove();
 OrderInput.bBlocked=true;
 if(auto* HUD=Cast<AOVHUD>(GetHUD())) HUD->CancelPointerDrag();
}

void AOVController::UpdateOrbit(FVector2D Cursor)
{
 if(OrbitGesture.Surface==EOVOrbitSurface::None) return;
 FVector2D Delta=OrbitGesture.Sample(Cursor);
 if(!bPlayGame3)
 {
  float DX=0,DY=0;
  GetInputMouseDelta(DX,DY);
  Delta=FVector2D(DX,DY);
 }
 if(OrbitGesture.Surface==EOVOrbitSurface::World)
 {
  ApplyCameraInput(Delta,FVector::ZeroVector,0,0,0);
 }
 else if(auto* HUD=Cast<AOVHUD>(GetHUD()))
 {
  if(HUD->HasOrbitPreview()) HUD->OrbitPreview(Delta);
  else OrbitGesture={};
 }
}

void AOVController::UpdateRightMove(FVector2D Cursor)
{
 Preview=RightMove.Sample(Cursor,AltitudeScale(),CommandMinZ(),CommandMaxZ());
 OrderInput.bShowAltitude=!bRightHeading && RightMove.MaxTravelSquared>=FMath::Square(8.0);
 if(bRightHeading) Preview.Z=RightMove.Anchor.Z;
}

void AOVController::HandleRightMove(bool Pressed, FVector2D Cursor, bool InWorld, bool Shift, bool Ctrl)
{
 if(Pressed)
 {
  if(RightMove.bActive || !InWorld || bPlacing || InputMode!=EOVInputMode::None ||
     OrbitGesture.Surface!=EOVOrbitSurface::None || !Selected || !Selected->bFriendly ||
     !Selected->Systems->CanCommand()) return;

  RightCommander=Selected;
  bRightHeading=Ctrl;
  if(!Shift && !Ctrl)
  {
   FHitResult Hit;
   if(GetHitResultAtScreenPosition(Cursor,ECC_Visibility,false,Hit))
   {
    AOVShip* Ship=Cast<AOVShip>(Hit.GetActor());
    if(Ship && !Ship->bFriendly && !Ship->Systems->bWrecked) RightTarget=Ship;
   }
  }

  PlacementPlaneZ=Selected->GetActorLocation().Z;
  FVector Origin,Direction,Point=Selected->GetActorLocation();
  const bool Valid=DeprojectScreenPositionToWorld(Cursor.X,Cursor.Y,Origin,Direction) &&
   FOVOrderInput::Project(Origin,Direction,PlacementPlaneZ,Point,ProjectionNearDistance,CommandRayLength());
  Point.X=FMath::Clamp(Point.X,-CommandXY(),CommandXY());
  Point.Y=FMath::Clamp(Point.Y,-CommandXY(),CommandXY());
  RightMove.Begin(Cursor,Point,Valid);
  UpdateRightMove(Cursor);
  LastAction=TEXT("우클릭 XY 고정 · 드래그로 고도 · 해제로 명령 확정");
  return;
 }

 if(!RightMove.bActive) return;
 UpdateRightMove(Cursor);
 const bool Allowed=InWorld && Selected && RightCommander.Get()==Selected && Selected->Systems->CanCommand();
 if(RightMove.Finish(Allowed))
 {
  if(RightTarget.IsValid() && !RightTarget->Systems->bWrecked && RightMove.MaxTravelSquared<=FMath::Square(25.0))
   SetTarget(RightTarget.Get());
  else if(RightMove.bValidPoint)
  {
   if(bRightHeading) IssueHeading(Preview);
   else { bPlacing=true; ConfirmMove(); }
  }
 }
 CancelRightMove();
}

bool AOVController::InputKey(const FInputKeyEventArgs& Params)
{
 const FKey Key=Params.Key;
 const bool Pressed=Params.Event==IE_Pressed || Params.Event==IE_DoubleClick;
 const bool Released=Params.Event==IE_Released;
 if(Released && (Key==EKeys::LeftAlt || Key==EKeys::RightAlt) && OrbitGesture.bAltLeft)
  OrbitGesture.End(true);

 if(Pressed || Released)
 {
  FIntPoint Pixel(-1,-1),Size(0,0);
  if(Params.Viewport)
  {
   Params.Viewport->GetMousePos(Pixel);
   Size=Params.Viewport->GetSizeXY();
  }
  const FVector2D Cursor(Pixel);
  const bool Inside=Pixel.X>=0 && Pixel.Y>=0 && Pixel.X<Size.X && Pixel.Y<Size.Y;
  auto* HUD=Cast<AOVHUD>(GetHUD());
  const bool OverUI=Inside && HUD && HUD->IsOverPanel(Cursor.X,Cursor.Y);
  const bool Alt=Params.Viewport && (Params.Viewport->KeyState(EKeys::LeftAlt)||Params.Viewport->KeyState(EKeys::RightAlt));
  const bool AltLeft=Key==EKeys::LeftMouseButton && (bConsumeAltLeft || (bPlayGame3 && Pressed && Alt));
  if(Key==EKeys::MiddleMouseButton || AltLeft)
  {
   if(AltLeft) bConsumeAltLeft=Pressed;
   if(Pressed && Inside && !RightMove.bActive)
   {
    if(!OverUI) OrbitGesture.Begin(EOVOrbitSurface::World,AltLeft,Cursor);
    else if(HUD->IsOverOrbitPreview(Cursor)) OrbitGesture.Begin(EOVOrbitSurface::Preview,AltLeft,Cursor);
   }
   if(Released && OrbitGesture.Surface!=EOVOrbitSurface::None && OrbitGesture.bAltLeft==AltLeft)
   {
    if(bPlayGame3) UpdateOrbit(Cursor);
    OrbitGesture.End(AltLeft);
   }
   return true;
  }
  if(bPlayGame3 && Key==EKeys::RightMouseButton)
  {
   const bool Shift=Params.Viewport && (Params.Viewport->KeyState(EKeys::LeftShift)||Params.Viewport->KeyState(EKeys::RightShift));
   const bool Ctrl=Params.Viewport && (Params.Viewport->KeyState(EKeys::LeftControl)||Params.Viewport->KeyState(EKeys::RightControl));
   HandleRightMove(Pressed,Cursor,Inside&&!OverUI&&(Pressed||Params.Viewport->HasFocus()),Shift,Ctrl);
   return true;
  }
  if((RightMove.bActive || OrbitGesture.Surface!=EOVOrbitSurface::None) && Key==EKeys::LeftMouseButton && Pressed) return true;
 }
 return Super::InputKey(Params);
}

void AOVController::StepSimulation(float DeltaTime)
{
 if(bSimulationPaused || DeltaTime<=0 || !FMath::IsFinite(DeltaTime)) return;
 float Remaining=DeltaTime*SimulationSpeed; SimulationSeconds+=Remaining;
 if(Combat && Combat->bEnabled) { Combat->Simulate(Remaining); if(Selected&&Combat->IsDeparted(Selected)) ClearSelection(); return; }
 while(Remaining>UE_SMALL_NUMBER) {
  const float Dt=FMath::Min(Remaining,1.f/60.f); Remaining-=Dt;
  for(AOVShip* Ship:Ships) {
   Ship->Systems->Simulate(Dt);
   if(Ship->Systems->bWrecked || Ship->Systems->bNeutral) { Ship->StopMovement(); Ship->Velocity=FVector::ZeroVector; Ship->AttackTarget=nullptr; continue; }
   if(Ship->AttackTarget&&Ship->AttackTarget->Systems->bWrecked) Ship->AttackTarget=nullptr;
   if(!Ship->Systems->CanCommand()) { Ship->SetAutomation(true,true); Ship->StopMovement(); }
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
  if(Combat && Combat->bEnabled && (Combat->IsDeparted(A)||Combat->IsDeparted(B))) continue;
  const auto ContactRadius=[this](const AOVShip* Ship) {
   if (CalibrationScenario>=0 && Ship->Systems->Layout)
    return (Ship->Systems->Layout->LocalHalfExtent * Ship->GetActorScale3D()).Size();
   return 100*Ship->GetActorScale3D().GetMax();
  };
  const double RadiusA=ContactRadius(A), RadiusB=ContactRadius(B);
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
 const auto* Client=GetWorld()->GetGameViewport();
 FViewport* Viewport=Client?Client->Viewport:nullptr;
 const bool Focused=Viewport&&Viewport->HasFocus();
 FIntPoint Pixel(-1,-1),Size(0,0);
 if(Viewport) { Viewport->GetMousePos(Pixel); Size=Viewport->GetSizeXY(); }
 const FVector2D Cursor(Pixel);
 const float MX=Cursor.X,MY=Cursor.Y;
 const bool Inside=Focused&&Pixel.X>=0&&Pixel.Y>=0&&Pixel.X<Size.X&&Pixel.Y<Size.Y;
 AOVHUD* HUD=Cast<AOVHUD>(GetHUD());
 const bool LeftPressed=Inside&&WasInputKeyJustPressed(EKeys::LeftMouseButton);
 const int Wheel=Focused?int(WasInputKeyJustPressed(EKeys::MouseScrollUp))-int(WasInputKeyJustPressed(EKeys::MouseScrollDown)):0;
 const bool WorldGesture=RightMove.bActive||OrbitGesture.Surface==EOVOrbitSurface::World;
 const bool OverUI=HUD&&Inside&&(HUD->HandlePointer(MX,MY,LeftPressed,IsInputKeyDown(EKeys::LeftMouseButton),WorldGesture?0:Wheel,false,0,0) || (LeftPressed && !WorldGesture && HUD->IsOverWorldMarker(Cursor)));
 const bool InWorld=Inside&&!OverUI;
 if(!Focused)
 {
  OrbitGesture={};
  CancelRightMove();
  if(HUD) HUD->CancelPointerDrag();
 }
 else
 {
  UpdateOrbit(Cursor);
  if(RightMove.bActive) UpdateRightMove(Cursor);
 }

 const bool Shift=IsInputKeyDown(EKeys::LeftShift)||IsInputKeyDown(EKeys::RightShift);
 const bool Ctrl=IsInputKeyDown(EKeys::LeftControl)||IsInputKeyDown(EKeys::RightControl);
 FVector Pan=FVector::ZeroVector;
 float YawAxis=0;
 if(InWorld)
 {
  Pan.X=int(IsInputKeyDown(EKeys::W)||IsInputKeyDown(EKeys::Up))-int(IsInputKeyDown(EKeys::S)||IsInputKeyDown(EKeys::Down));
  Pan.Y=int(IsInputKeyDown(EKeys::D)||IsInputKeyDown(EKeys::Right))-int(IsInputKeyDown(EKeys::A)||IsInputKeyDown(EKeys::Left));
  const int QE=int(IsInputKeyDown(EKeys::E))-int(IsInputKeyDown(EKeys::Q));
  if(bPlayGame3)
  {
   YawAxis=QE;
   Pan.Z=int(IsInputKeyDown(EKeys::PageUp))-int(IsInputKeyDown(EKeys::PageDown));
  }
  else
  {
   Pan.Z=QE;
   if(WasInputKeyJustPressed(EKeys::Z)) CameraYaw-=15;
   if(WasInputKeyJustPressed(EKeys::C)) CameraYaw+=15;
   if(WasInputKeyJustPressed(EKeys::PageUp)) CameraPitch-=10;
   if(WasInputKeyJustPressed(EKeys::G)) CameraPitch+=10;
  }
  if(WasInputKeyJustPressed(EKeys::Home)) FrameEngagement();
 }
 int CameraWheel=InWorld||WorldGesture?Wheel:0;
 if(OrbitGesture.Surface==EOVOrbitSurface::Preview) CameraWheel=0;
 if(!bPlayGame3 && bPlacing && CameraWheel)
 {
  Preview.Z=FMath::Clamp(Preview.Z+CameraWheel*250*AltitudeScale(),CommandMinZ(),CommandMaxZ());
  OrderInput.bShowAltitude=true;
  OrderInput.bReanchor=true;
  CameraWheel=0;
 }
 ApplyCameraInput(FVector2D::ZeroVector,Pan,YawAxis,CameraWheel,FApp::GetDeltaTime());
 if(ActionPressed(TEXT("OVCameraProfile"))) Action(TEXT("CameraProfile"));
 if(Focused && ActionPressed(TEXT("OVPhase2C"))) Action(TEXT("Phase2C"));
 if(Focused && ActionPressed(TEXT("OVDeveloper"))) Action(TEXT("Developer"));
 if(Focused && ActionPressed(TEXT("OVCalibrationRange"))) Action(TEXT("CalibrationRange"));
 if(Focused && ActionPressed(TEXT("OVCombatRange"))) Action(TEXT("CombatRange"));
 if(WasInputKeyJustPressed(EKeys::F9)) UE_LOG(LogTemp,Display,TEXT("OV VERIFY: camera=%s yaw=%.1f pitch=%.1f distance=%.1f ship=%s pos=%s manual=%d moving=%d paused=%d speed=%.0f sim=%.3f"),*CameraFocus.ToString(),CameraYaw,CameraPitch,CameraDistance,Selected?*Selected->ShipName:TEXT("none"),Selected?*Selected->GetActorLocation().ToString():TEXT("none"),Selected?Selected->bManual:0,Selected?Selected->bHasDestination:0,bSimulationPaused,SimulationSpeed,SimulationSeconds);
 if(ActionPressed(TEXT("OVPause"))) Action(TEXT("Pause"));
 if(ActionPressed(TEXT("OVSpeed1"))) Action(TEXT("Speed1"));
 if(ActionPressed(TEXT("OVSpeed2"))) Action(TEXT("Speed2"));
 if(ActionPressed(TEXT("OVSpeed3"))) Action(TEXT("Speed3"));
 if(ActionPressed(TEXT("OVCancel"))) Action(TEXT("Cancel"));
 if(InWorld) {
  if(ActionPressed(TEXT("OVFollow"))) Action(TEXT("Focus"));
  if(ActionPressed(TEXT("OVMove"))) Action(TEXT("Move"));
  if(ActionPressed(TEXT("OVTarget"))) Action(TEXT("Target"));
  if(ActionPressed(TEXT("OVHeading"))) Action(TEXT("Heading"));
  if(ActionPressed(TEXT("OVStop"))) Action(TEXT("Stop"));
  if(ActionPressed(TEXT("OVRepair"))) Action(TEXT("Repair"));
  if(ActionPressed(TEXT("OVFire"))) Action(TEXT("Fire"));
  if(ActionPressed(TEXT("OVFireAll"))) Action(TEXT("FireAll"));
  if(ActionPressed(TEXT("OVNextWeapon"))) Action(TEXT("NextWeapon"));
  for(int32 I=0;I<9;++I) if(ActionPressed(FName(*FString::Printf(TEXT("OVGroup%d"),I+1)))) {
   if(Ctrl) { auto& Group=Groups.FindOrAdd(I); Group.Reset(); for(AOVShip* S:Selection) if(S->bFriendly) Group.Add(S); }
   else if(const auto* Group=Groups.Find(I)) { ClearSelection(); for(const auto& S:*Group) if(S.IsValid()&&!S->Systems->bWrecked) SelectAdditional(S.Get()); }
  }
  if(!bPlayGame3&&WasInputKeyJustPressed(EKeys::RightMouseButton)) {
   bRightGesture=true; RightStart=FVector2D(MX,MY); RightTarget=nullptr;
   FHitResult Hit; if(!Shift&&GetHitResultUnderCursor(ECC_Visibility,false,Hit)) {
    AOVShip* Target=Cast<AOVShip>(Hit.GetActor()); if(Target&&Selected&&Target->bFriendly!=Selected->bFriendly) RightTarget=Target;
   }
   if(!RightTarget.IsValid()&&!bPlacing) { Action(TEXT("Move")); FVector O,D,Raw; if(DeprojectMousePositionToWorld(O,D)&&FOVOrderInput::Project(O,D,PlacementPlaneZ,Raw,ProjectionNearDistance,CommandRayLength())) { Preview.X=FMath::Clamp(Raw.X,-CommandXY(),CommandXY()); Preview.Y=FMath::Clamp(Raw.Y,-CommandXY(),CommandXY()); } }
  }
 }
 if(bRightGesture&&WasInputKeyJustReleased(EKeys::RightMouseButton)) {
  if(!OverUI&&RightTarget.IsValid()&&FVector2D::Distance(RightStart,FVector2D(MX,MY))<=25) SetTarget(RightTarget.Get());
  bRightGesture=false; RightTarget=nullptr;
 }
 UpdateCamera();
 if(bPlacing) UpdatePreview();
 if(InWorld&&!RightMove.bActive&&WasInputKeyJustPressed(EKeys::Enter)) ConfirmMove();
 if(LeftPressed && InWorld && !RightMove.bActive && OrbitGesture.Surface==EOVOrbitSurface::None) {
  if(bPlacing) ConfirmMove();
  else if(InputMode==EOVInputMode::Heading) { FVector O,D,P; if(DeprojectMousePositionToWorld(O,D)&&FOVOrderInput::Project(O,D,PlacementPlaneZ,P,ProjectionNearDistance,CommandRayLength())) IssueHeading(P); }
  else { FHitResult Hit; AOVShip* Ship=nullptr; if(GetHitResultUnderCursor(ECC_Visibility,false,Hit)) Ship=Cast<AOVShip>(Hit.GetActor());
   if(InputMode==EOVInputMode::Target) { if(Ship) SetTarget(Ship); }
   else if(Shift&&Ship) SelectAdditional(Ship); else SelectShip(Ship);
  }
 }
 StepSimulation(DeltaTime);
 CSV_SCOPED_TIMING_STAT_EXCLUSIVE(OV_WorldEffects);
 TRACE_CPUPROFILER_EVENT_SCOPE(OV_WorldEffects);
 if(Combat && Combat->bEnabled) {
  // Presentation reads resolved events. It never requests fire or applies damage.
  const float FX=CalibrationScenario>=0?20.f:1.f;
  for(const auto& Shot:Combat->Attacks) {
   const FColor Color=Shot.Weapon.Kind==EOVAttackKind::Cannon?FColor::Orange:FColor::Yellow;
   DrawDebugLine(GetWorld(),Shot.Position-Shot.Direction*FMath::Max(100.f,Shot.Weapon.Speed*.035f),Shot.Position,Color,false,-1,0,4);
   DrawDebugSphere(GetWorld(),Shot.Position,12*FX,6,Color,false,-1,0,2);
  }
  for(const auto& Event:Combat->RecentEvents) if(Combat->GameTime-Event.ResolvedAt<.20) {
   if(Event.Weapon.Kind==EOVAttackKind::Beam) DrawDebugLine(GetWorld(),Event.Origin,Event.Position,Event.FactionId==0?FColor::Cyan:FColor::Red,false,-1,0,4);
   DrawDebugSphere(GetWorld(),Event.Origin,15*FX,8,Event.FactionId==0?FColor::Cyan:FColor::Red,false,-1,0,3);
   if(Event.Result==EOVAttackResult::Hit) {
    DrawDebugSphere(GetWorld(),Event.Position,35*FX,12,FColor::Orange,false,-1,0,3);
    for(AOVShip* Hit:Ships) if(Hit->GetFName()==Event.HitShipId && Hit->Systems->bShieldActive && Hit->Systems->Layout)
     DrawDebugBox(GetWorld(),Hit->GetActorLocation(),Hit->Systems->Layout->LocalHalfExtent*Hit->GetActorScale3D()*1.06,Hit->GetActorQuat(),FColor(100,160,255),false,-1,0,4);
   }
  }
  for(const auto& State:Combat->Ships) if(IsValid(State.Ship)&&State.Ship->Systems->Layout&&!State.Ship->Systems->bWrecked&&!State.bDeparted) {
   for(const auto& Mount:State.Mounts) {
    const auto* Def=State.Ship->Systems->Layout->WeaponMounts.FindByPredicate([&](const FOVWeaponMountDefinition& M){return M.WeaponId==Mount.MountId;});
    if(!Def) continue;
    const FVector Base=State.Ship->GetActorTransform().TransformPosition(Def->LocalPosition);
    DrawDebugLine(GetWorld(),Base,Base+State.Ship->GetActorQuat().RotateVector(Mount.LocalAim.Vector())*100*FX,FColor::Silver,false,-1,0,3);
   }
  }
 }
 const double GridStep=CalibrationScenario>=0 ? OVCalibration::GridStep : 500;
 const double GridExtent=CommandXY();
 const float GridLine=CalibrationScenario>=0 ? 40.f : 2.f;
 for(int I=-12;I<=12;++I) {
  DrawDebugLine(GetWorld(),FVector(I*GridStep,-GridExtent,0),FVector(I*GridStep,GridExtent,0),I==0?FColor(50,130,180):FColor(18,35,55),false,-1,0,GridLine);
  DrawDebugLine(GetWorld(),FVector(-GridExtent,I*GridStep,0),FVector(GridExtent,I*GridStep,0),I==0?FColor(180,75,60):FColor(18,35,55),false,-1,0,GridLine);
 }
 for(AOVShip* Ship:Ships) {
  if(Combat && Combat->bEnabled && Combat->IsDeparted(Ship)) continue;
  const FVector P=Ship->GetActorLocation();
  DrawDebugLine(GetWorld(),FVector(P.X,P.Y,0),P,Ship->bFriendly?FColor(30,80,110):FColor(140,30,30),false,-1,0,2);
  if(Ship->bSelected) { FVector Center,Extent; Ship->GetActorBounds(false,Center,Extent); DrawDebugBox(GetWorld(),Center,Extent+FVector(25),FColor::Cyan,false,-1,0,3); }
  if(Ship->bHasDestination) { DrawDebugLine(GetWorld(),P,Ship->Destination,FColor(90,200,140),false,-1,0,3); DrawDebugSphere(GetWorld(),Ship->Destination,70,12,FColor(90,200,140),false,-1,0,2); }
  if(Ship->AttackTarget&&!Ship->AttackTarget->Systems->bWrecked&&!(Combat&&Combat->bEnabled&&Combat->IsDeparted(Ship->AttackTarget))) { DrawDebugLine(GetWorld(),P,Ship->AttackTarget->GetActorLocation(),FColor::Red,false,-1,0,2); DrawDebugSphere(GetWorld(),Ship->AttackTarget->GetActorLocation(),150,12,FColor::Red,false,-1,0,2); }
 }
 if(Selected&&Selected->Systems->Layout) {
  auto* S=Selected->Systems.Get(); const auto* L=S->Layout.Get(); const FTransform Transform=Selected->GetActorTransform();
  if(bDeveloperMode && L->ArmorSections.IsValidIndex(SelectedArmor)) { const auto& A=L->ArmorSections[SelectedArmor]; DrawDebugBox(GetWorld(),Transform.TransformPosition(A.LocalCenter),A.HalfExtent*Transform.GetScale3D(),Transform.GetRotation(),FColor::Orange,false,-1,0,3); }
  if(S->HitEventId>0&&bDeveloperMode) { DrawDebugSphere(GetWorld(),S->LastWorldHit,40,10,FColor::Magenta,false,-1,0,4); if(L->Slots.IsValidIndex(S->LastDamageIndex)) DrawDebugLine(GetWorld(),S->LastWorldHit,Transform.TransformPosition(L->Slots[S->LastDamageIndex].LocalCenter),FColor::Magenta,false,-1,0,3); }
 }
 if(bPlacing || (RightMove.bActive && RightMove.bValidPoint)) {
  const FVector Ground(Preview.X,Preview.Y,PlacementPlaneZ);
  // The disc and projection use exactly the same selected-ship elevation.
  DrawDebugCircle(GetWorld(),Ground,600,48,FColor(30,110,140),false,-1,0,2,FVector::ForwardVector,FVector::RightVector,false);
  if(OrderInput.bShowAltitude) DrawDebugLine(GetWorld(),Ground,Preview,FColor::Yellow,false,-1,0,5);
  DrawDebugSphere(GetWorld(),Preview,90*(CalibrationScenario>=0?20:1),16,FColor::Yellow,false,-1,0,3);
  DrawDebugCircle(GetWorld(),Ground,180*(CalibrationScenario>=0?20:1),32,FColor::Yellow,false,-1,0,3,FVector::ForwardVector,FVector::RightVector,false);
  if(Selected) DrawDebugLine(GetWorld(),Selected->GetActorLocation(),Preview,FColor::Yellow,false,-1,0,2);
 }
}
