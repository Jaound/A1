#include "OVHUD.h"
#include "OVDevelopmentWidget.h"
#include "OVHeroWidget.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "OVController.h"
#include "OVShip.h"
#include "OVShipSystemsComponent.h"
#include "OVCombatSubsystem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "CanvasItem.h"
#include "CanvasTypes.h"
namespace {
const FLinearColor Ink(.75f,.86f,.94f),Accent(.18f,.85f,1),Panel(.008f,.018f,.035f,.96f);
FLinearColor Health(float R) { return R<=0?FLinearColor(.2f,.23f,.26f):R<.3f?FLinearColor(.85f,.15f,.1f):R<.7f?FLinearColor(.9f,.65f,.1f):FLinearColor(.15f,.65f,.4f); }
FString Facility(EOVFacilityType T) {
 const TCHAR* Labels[]={TEXT("빈 공간"),TEXT("코어"),TEXT("함교"),TEXT("생명유지"),TEXT("엔진"),TEXT("무기제어실"),TEXT("탄약고"),TEXT("축전실"),TEXT("미사일 적재실"),TEXT("쉴드 제어실"),TEXT("레이더"),TEXT("요격 제어실"),TEXT("포드 발사실"),TEXT("격납고"),TEXT("함재기 관제실"),TEXT("자원 저장소"),TEXT("보안 시설")};
 return Labels[FMath::Clamp(static_cast<int32>(T),0,16)]; }
}
void AOVHUD::Button(FName N,const FString& L,float X,float Y,float W,bool Active)
{
 ActionCenters.Add(N,FVector2D(X+W*.5f,Y+13.5f));
 DrawRect(Active?FLinearColor(.03f,.4f,.55f,.95f):FLinearColor(.04f,.08f,.13f,.98f),X,Y,W,27);
 DrawText(L,Active?FLinearColor::White:Ink,X+6,Y+5,GEngine->GetSmallFont());
 AddHitBox(FVector2D(X,Y)*UIScale,FVector2D(W,27)*UIScale,N,true,bDrawingInterior?210:bDrawingDetails?110:10);
}
FName AOVHUD::GetHitActionAtCanvasPoint(FVector2D Point) const
{
 for(const FHUDHitBox& Hit:HitBoxMap)
  if(Hit.ConsumesInput() && Hit.Contains(Point*UIScale)) return Hit.GetName();
 return NAME_None;
}
bool AOVHUD::IsOverPanel(float X,float Y) const
{
 if(IsOverAddedUI(X,Y)) return true;
 const auto* PC=Cast<AOVController>(PlayerOwner); if(!PC) return false;
 const FVector2D P(X/UIScale,Y/UIScale);
 if(IsOverInteriorPanel(X,Y)) return true;
 if(IsOverMissionPanel(X,Y)) return true;
 if(bDragging||bResizing) return true;
 if(!PC->bDeveloperMode) {
  if(P.Y<96 || P.Y>ViewportPixels.Y/UIScale-126 || (P.X<204 && P.Y<100+PC->Ships.Num()*36) || (PC->Phase2CScenario>=0 && P.X<258 && P.Y>=300 && P.Y<=600)) return true;
 }
 if(PC->bDeveloperMode && PC->Combat && PC->Combat->bEnabled && PC->Selected && (PC->Selected->bFriendly||PC->bDeveloperMode) && P.X>=10 && P.X<=440 && P.Y>=216 && P.Y<=560) return true;
 if(PC->bDeveloperMode && (P.Y<55 || (PC->Selected&&P.Y>ViewportPixels.Y/UIScale-88) || (P.X<200&&P.Y<210))) return true;
 return PC->Selected&&bDetailsOpen&&P.X>=WindowPosition.X&&P.X<=WindowPosition.X+WindowSize.X&&P.Y>=WindowPosition.Y&&P.Y<=WindowPosition.Y+(bMinimized?32:WindowSize.Y);
}
bool AOVHUD::HasOrbitPreview() const
{
 const auto* PC=Cast<AOVController>(PlayerOwner);
 return PC && PC->Selected && PC->Selected->Systems->Layout && bDetailsOpen && !bMinimized && PC->ViewedDeck==0;
}

bool AOVHUD::IsOverOrbitPreview(FVector2D Cursor) const
{
 const FVector2D P=Cursor/UIScale-WindowPosition;
 if(IsOverInteriorPanel(Cursor.X,Cursor.Y)) return false;
 return HasOrbitPreview() && P.X>=10 && P.X<=WindowSize.X-10 && P.Y>=72 && P.Y<=264;
}

void AOVHUD::OrbitPreview(FVector2D Delta)
{
 if(!HasOrbitPreview()) return;
 PreviewRotation.Yaw+=Delta.X;
 PreviewRotation.Pitch=FMath::Clamp(PreviewRotation.Pitch-Delta.Y,-85.,85.);
}

bool AOVHUD::HandlePointer(float X,float Y,bool Pressed,bool Down,float Wheel,bool Orbit,float DX,float DY)
{
 const auto* PC=Cast<AOVController>(PlayerOwner);
 if(PC) { int32 Width=0,Height=0; PC->GetViewportSize(Width,Height); if(Width>0&&Height>0) ViewportPixels=FVector2D(Width,Height); }
 UIScale=FMath::Max(.25f,FMath::Min(1.f,FMath::Min(ViewportPixels.X/1280.f,ViewportPixels.Y/740.f)));
 // AHUD::Canvas is cleared by the viewport after rendering. Input runs outside DrawHUD.
 const bool Over=IsOverPanel(X,Y);
 // A drag begun in the existing window retains ownership; a new interior gesture consumes all buttons/wheel.
 if(!bDragging&&!bResizing&&IsOverInteriorPanel(X,Y)) return true;
 if(!PC||!PC->Selected||!bDetailsOpen) { bDragging=false;bResizing=false;return Over; }
 const FVector2D P(X/UIScale,Y/UIScale);
 if(Pressed&&Over) {
  if(P.X>=WindowPosition.X&&P.X<WindowPosition.X+WindowSize.X-90&&P.Y>=WindowPosition.Y&&P.Y<WindowPosition.Y+30) { bDragging=true;DragOffset=P-WindowPosition; }
  if(!bMinimized&&P.X>WindowPosition.X+WindowSize.X-18&&P.X<=WindowPosition.X+WindowSize.X&&P.Y>WindowPosition.Y+WindowSize.Y-18&&P.Y<=WindowPosition.Y+WindowSize.Y) bResizing=true;
 }
 if(!Down) { bDragging=false;bResizing=false; }
 if(bDragging) {
  WindowPosition=P-DragOffset;
  WindowPosition.X=FMath::Clamp(WindowPosition.X,0.0,FMath::Max(0.0,ViewportPixels.X/UIScale-WindowSize.X));
  WindowPosition.Y=FMath::Clamp(WindowPosition.Y,56.0,FMath::Max(56.0,ViewportPixels.Y/UIScale-WindowSize.Y-90));
 }
 if(bResizing) { WindowSize.X=FMath::Clamp(P.X-WindowPosition.X,400.0,700.0);WindowSize.Y=FMath::Clamp(P.Y-WindowPosition.Y,550.0,750.0); }
 if(IsOverOrbitPreview(FVector2D(X,Y))) {
  if(Orbit) OrbitPreview(FVector2D(DX,DY));
  if(Wheel) PreviewZoom=FMath::Clamp(PreviewZoom+Wheel*.1f,.5f,1.6f);
 }
 return Over||bDragging||bResizing;
}
void AOVHUD::NotifyHitBoxClick(FName N)
{
 if(HandleInteriorAction(N)) return;
 if(N==TEXT("MissionBackground")) return;
 if(N==TEXT("WindowBackground")) return;
 if(N==TEXT("WindowClose")) { bDetailsOpen=false;return; }
 if(N==TEXT("WindowMin")) { bMinimized=!bMinimized;return; }
 if(N==TEXT("WindowMax")) { WindowPosition=FVector2D(210,60);WindowSize=WindowSize.X>500?FVector2D(424,550):FVector2D(650,620);return; }
 if(auto* PC=Cast<AOVController>(PlayerOwner)) PC->Action(N);
}
void AOVHUD::DrawExternal(float X,float Y,float W)
{
 const auto* PC=Cast<AOVController>(PlayerOwner);const auto* S=PC->Selected->Systems.Get();const auto* L=S->Layout.Get();
 DrawRect(FLinearColor(.005f,.012f,.025f),X,Y,W,192);
 // Read-only 3D geometry projection: no actors, simulation ticks or damage state.
 const FQuat R=PreviewRotation.Quaternion(); double ExtentY=1,ExtentZ=1;
 for(int32 I=0;I<8;++I) { const FVector Corner=R.RotateVector(L->LocalHalfExtent*FVector((I&1)?1:-1,(I&2)?1:-1,(I&4)?1:-1)); ExtentY=FMath::Max(ExtentY,FMath::Abs(Corner.Y));ExtentZ=FMath::Max(ExtentZ,FMath::Abs(Corner.Z)); }
 const double Scale=.9*FMath::Min((W-24)/(2*ExtentY),150/(2*ExtentZ))*PreviewZoom;
 auto Project=[&](FVector P) { P=R.RotateVector(P)*Scale;return FVector2D(X+W*.5f+P.Y,Y+85-P.Z); };
 auto Inside=[&](FVector2D P) { return P.X>=X+10&&P.X<=X+W-10&&P.Y>=Y+10&&P.Y<=Y+162; };
 auto Line=[&](FVector2D A,FVector2D B,FLinearColor Color) {
  // Clip projected edges and hit markers to the same preview rectangle.
  const FVector2D D=B-A; double T0=0,T1=1;
  auto Clip=[&](double P,double Q) { if(FMath::Abs(P)<UE_SMALL_NUMBER)return Q>=0; const double T=Q/P; if(P<0)T0=FMath::Max(T0,T);else T1=FMath::Min(T1,T);return T0<=T1; };
  if(Clip(-D.X,A.X-X)&&Clip(D.X,X+W-A.X)&&Clip(-D.Y,A.Y-Y)&&Clip(D.Y,Y+170-A.Y)) {
   const FVector2D Start=A+D*T0,End=A+D*T1;DrawLine(Start.X,Start.Y,End.X,End.Y,Color,2);
  }
 };
 struct FFace { int32 Index;double Depth; };TArray<FFace> Faces;
 for(int32 I=0;I<L->ArmorSections.Num();++I) Faces.Add({I,R.RotateVector(L->ArmorSections[I].LocalCenter).X});
 Faces.Sort([](const FFace& A,const FFace& B){return A.Depth<B.Depth;});
 for(const auto& Face:Faces) {
  const auto& D=L->ArmorSections[Face.Index];FVector Normal,U,V;
  switch(D.Face) {
   case EOVArmorFace::Front: Normal=FVector::ForwardVector;U=FVector(0,D.HalfExtent.Y,0);V=FVector(0,0,D.HalfExtent.Z);break;
   case EOVArmorFace::Rear: Normal=-FVector::ForwardVector;U=FVector(0,D.HalfExtent.Y,0);V=FVector(0,0,D.HalfExtent.Z);break;
   case EOVArmorFace::Left: Normal=-FVector::RightVector;U=FVector(D.HalfExtent.X,0,0);V=FVector(0,0,D.HalfExtent.Z);break;
   case EOVArmorFace::Right: Normal=FVector::RightVector;U=FVector(D.HalfExtent.X,0,0);V=FVector(0,0,D.HalfExtent.Z);break;
   case EOVArmorFace::Top: Normal=FVector::UpVector;U=FVector(D.HalfExtent.X,0,0);V=FVector(0,D.HalfExtent.Y,0);break;
   default: Normal=-FVector::UpVector;U=FVector(D.HalfExtent.X,0,0);V=FVector(0,D.HalfExtent.Y,0);break;
  }
  if(R.RotateVector(Normal).X<=0) continue;
  const FVector2D P[]={Project(D.LocalCenter-U-V),Project(D.LocalCenter+U-V),Project(D.LocalCenter+U+V),Project(D.LocalCenter-U+V)};
  const FLinearColor Color=Face.Index==PC->SelectedArmor?FLinearColor(1,.7f,.1f):Health(S->ArmorSections[Face.Index].HP/D.MaxArmor);
  for(int32 J=0;J<4;++J) Line(P[J],P[(J+1)%4],Color);
  const FVector2D C=Project(D.LocalCenter);
  if(!Inside(C)) continue;
  AddHitBox((C-FVector2D(10,8))*UIScale,FVector2D(20,16)*UIScale,FName(*FString::Printf(TEXT("Armor_%d"),Face.Index)),true,120);
  DrawRect(Color,C.X-4,C.Y-4,8,8);
 }
 for(int32 I=0;I<L->WeaponMounts.Num();++I) {
  const auto& M=L->WeaponMounts[I];const FVector2D P=Project(M.LocalPosition);
  if(!Inside(P))continue;
  if(PC->Combat&&PC->Combat->bEnabled) {
   const auto State=PC->Combat->GetShipState(PC->Selected);
   if(const auto* Mount=State.Mounts.FindByPredicate([&](const FOVCombatMountState& Item){return Item.MountId==M.WeaponId;}))
    Line(P,Project(M.LocalPosition+Mount->LocalAim.Vector()*60),Accent);
  }
  DrawRect(I==PC->SelectedWeapon?Accent:Health(S->WeaponMountStates[I].HP/M.MaxHP),P.X-6,P.Y-6,12,12);
  AddHitBox((P-FVector2D(7,7))*UIScale,FVector2D(14,14)*UIScale,FName(*FString::Printf(TEXT("Weapon_%d"),I)),true,130);
 }
 DrawText(TEXT("3D 상태 보기 · 중클릭 회전 / 휠 확대"),Ink,X+8,Y+174,GEngine->GetSmallFont());
}
void AOVHUD::DrawDetails()
{
 auto* PC=Cast<AOVController>(PlayerOwner);if(!PC->Selected||!bDetailsOpen)return;
 const auto* S=PC->Selected->Systems.Get();const auto* L=S->Layout.Get();if(!L)return;
 TGuardValue<bool> DetailLayer(bDrawingDetails,true);
 const float X=WindowPosition.X,Y=WindowPosition.Y,W=WindowSize.X;
 // The foreground window consumes blank-area clicks too, including over fire buttons.
 AddHitBox(WindowPosition*UIScale,FVector2D(W,bMinimized?32:WindowSize.Y)*UIScale,TEXT("WindowBackground"),true,100);
 DrawRect(Panel,X,Y,W,bMinimized?32:WindowSize.Y);
 DrawText(PC->Selected->ShipName,Accent,X+10,Y+8,GEngine->GetSmallFont());
 Button(TEXT("WindowMin"),bMinimized?TEXT("+"):TEXT("_"),X+W-90,Y+2,26);Button(TEXT("WindowMax"),TEXT("[]"),X+W-60,Y+2,26);Button(TEXT("WindowClose"),TEXT("X"),X+W-30,Y+2,26);
 if(bMinimized)return;
 Button(TEXT("External"),TEXT("장갑 / 외장 무장"),X+10,Y+38,145,PC->ViewedDeck==0);
 const bool Reveal=PC->Selected->bFriendly||PC->bDeveloperMode;
 for(int32 D=1;D<=L->DeckCount;++D) if(Reveal)Button(FName(*FString::Printf(TEXT("Deck%d"),D)),FString::Printf(TEXT("%dF"),D),X+165+(D-1)*52,Y+38,46,PC->ViewedDeck==D);
 if(PC->ViewedDeck==0)DrawExternal(X+10,Y+72,W-20);
 else if(Reveal) {
  int32 Cell=0;
  for(int32 I=0;I<L->Slots.Num();++I)if(L->Slots[I].DeckId==PC->ViewedDeck) {
   const float CX=X+10+(Cell%5)*(W-20)/5,CY=Y+78+(Cell/5)*54;++Cell;
   const auto& D=L->Slots[I];const auto& State=S->Slots[I];
   Button(FName(*FString::Printf(TEXT("Slot_%d"),I)),PC->bDeveloperMode?D.SlotId.ToString():Facility(D.Facility),CX,CY,(W-30)/5,I==PC->SelectedSlot);
   DrawRect(Health(D.MaxFacilityHP>0?State.HP/D.MaxFacilityHP:0),CX,CY+30,(W-30)/5,4);
   DrawText(Facility(D.Facility).Left(10),Ink,CX,CY+36,GEngine->GetSmallFont());
  }
 }
 DrawText(FString::Printf(TEXT("선체 %.0f/%.0f | 쉴드 %.0f/%.0f %s"),S->HullHP,S->MaxHullHP,S->Shield,S->MaxShield,S->bShieldActive?TEXT("ON"):TEXT("OFF")),Ink,X+10,Y+280,GEngine->GetSmallFont());
 if(S->ArmorSections.IsValidIndex(PC->SelectedArmor)) {
  const auto& A=S->ArmorSections[PC->SelectedArmor];
  DrawText(FString::Printf(TEXT("장갑 %s %.0f/%.0f"),*(PC->bDeveloperMode?A.ArmorSectionId.ToString():FString(TEXT("선택 구역"))),A.HP,L->ArmorSections[PC->SelectedArmor].MaxArmor),Accent,X+10,Y+301,GEngine->GetSmallFont());
 }
 if(PC->ViewedDeck==0&&S->WeaponMountStates.IsValidIndex(PC->SelectedWeapon)) {
  const auto& M=S->WeaponMountStates[PC->SelectedWeapon];const auto& D=L->WeaponMounts[PC->SelectedWeapon];
  const FString Status=!Reveal?TEXT("EXTERNAL ONLY"):(PC->Combat&&PC->Combat->bEnabled?PC->Combat->GetReadiness(PC->Selected,M.WeaponMountId).Reason:(S->IsWeaponAvailable(M.WeaponMountId)?TEXT("SUPPLIED"):TEXT("OFF")));
  DrawText(FString::Printf(TEXT("외장 무장 %s T%.0f HP %.0f/%.0f %s"),*(PC->bDeveloperMode?M.WeaponMountId.ToString():FString::FromInt(PC->SelectedWeapon+1)),D.Tier,M.HP,D.MaxHP,*(PC->bDeveloperMode?Status:FString(M.HP>0?TEXT("상태 정상"):TEXT("파괴")))),Ink,X+10,Y+322,GEngine->GetSmallFont());
 }else if(Reveal&&L->Slots.IsValidIndex(PC->SelectedSlot)) {
  const auto& F=S->Slots[PC->SelectedSlot];const auto& D=L->Slots[PC->SelectedSlot];
  DrawText(FString::Printf(TEXT("%s %s HP %.0f/%.0f Crew %.0f/%.0f"),*(PC->bDeveloperMode?F.SlotId.ToString():FString(TEXT("시설"))),*Facility(D.Facility),F.HP,D.MaxFacilityHP,F.Crew,D.RequiredCrew),Ink,X+10,Y+322,GEngine->GetSmallFont());
  DrawText(S->GetSlotStatus(F.SlotId),Accent,X+10,Y+341,GEngine->GetSmallFont());
 }
 if(Reveal)DrawText(FString::Printf(TEXT("엔진 %.0f%% 쉴드 %.0f%% 레이더 %.0f%%"),100*S->GetEfficiency(EOVFacilityType::Engine),100*S->GetEfficiency(EOVFacilityType::ShieldControl),100*S->GetEfficiency(EOVFacilityType::Radar)),Ink,X+10,Y+362,GEngine->GetSmallFont());
 else DrawText(TEXT("적 내부 정보 숨김 · 관측된 외장만 표시"),Ink,X+10,Y+362,GEngine->GetSmallFont());
 if(PC->bDeveloperMode) {
  DrawText(TEXT("DEV ONLY: direct tools, not supply repair"),FLinearColor::Yellow,X+10,Y+384,GEngine->GetSmallFont());
  const FName Names[]={TEXT("TestHit"),TEXT("ArmorZero"),TEXT("RepairArmor"),TEXT("Reset"),TEXT("DestroyExt"),TEXT("Sabotage"),TEXT("RepairSlot"),TEXT("Occupy"),TEXT("CrewDown"),TEXT("CrewUp"),TEXT("WeaponHit"),TEXT("RepairWeapon")};
  const TCHAR* Labels[]={TEXT("HIT 150"),TEXT("ARMOR 0"),TEXT("FIX ARMOR"),TEXT("RESET"),TEXT("DESTROY"),TEXT("SABOTAGE"),TEXT("FIX ROOM"),TEXT("OCCUPY"),TEXT("CREW -2"),TEXT("CREW +2"),TEXT("HIT GUN"),TEXT("FIX GUN")};
  for(int32 I=0;I<12;++I)Button(Names[I],I==0?FString::Printf(TEXT("HIT %.0f"),PC->TestDamage):FString(Labels[I]),X+10+(I%4)*(W-20)/4,Y+405+(I/4)*33,(W-32)/4);
  DrawText(FString::Printf(TEXT("Event %d: %s -> %s / HP -%.1f"),S->HitEventId,*S->LastArmorSection.ToString(),*S->LastDamageSlot.ToString(),S->LastHullDamage),Accent,X+10,Y+510,GEngine->GetSmallFont());
 }
 DrawLine(X+W-16,Y+WindowSize.Y-2,X+W-2,Y+WindowSize.Y-16,Ink,2);
}
void AOVHUD::DrawHUD()
{
    CSV_SCOPED_TIMING_STAT_EXCLUSIVE(OV_HUD);
    TRACE_CPUPROFILER_EVENT_SCOPE(OV_HUD);
 SyncAddedUI();
 ActionCenters.Reset();
 Super::DrawHUD();auto* PC=Cast<AOVController>(PlayerOwner);if(!PC||!Canvas||PC->Ships.Num()<3)return;
 ViewportPixels=FVector2D(Canvas->SizeX,Canvas->SizeY);
 UIScale=FMath::Max(.25f,FMath::Min(1.f,FMath::Min(Canvas->SizeX/1280.f,Canvas->SizeY/740.f)));
 Canvas->Canvas->PushRelativeTransform(FScaleMatrix(FVector(UIScale,UIScale,1)));
 const float Width=Canvas->SizeX/UIScale,Height=Canvas->SizeY/UIScale;
 if(!PC->bDeveloperMode) { DrawPlayHUD(); DrawWorldIndicators(); DrawPhase2C(); DrawMission(); DrawDetails(); DrawInterior(); Canvas->Canvas->PopTransform(); return; }
 const bool CombatOn=PC->Combat&&PC->Combat->bEnabled;
 DrawRect(Panel,10,10,Width-20,40);if(PC->CalibrationScenario<0)DrawText(CombatOn?TEXT("ORBITAL VANGUARD / 2B COMBAT RANGE"):TEXT("ORBITAL VANGUARD / 2A TEST RANGE"),Accent,20,22,GEngine->GetSmallFont());
 Button(TEXT("Pause"),PC->bSimulationPaused?TEXT("F1 RESUME"):TEXT("F1 PAUSE"),380,16,100,PC->bSimulationPaused);
 Button(TEXT("Speed1"),TEXT("F2 1x"),488,16,68,PC->SimulationSpeed==1);Button(TEXT("Speed2"),TEXT("F3 2x"),564,16,68,PC->SimulationSpeed==2);Button(TEXT("Speed3"),TEXT("F4 3x"),640,16,68,PC->SimulationSpeed==3);
 Button(TEXT("Developer"),PC->bDeveloperMode?TEXT("DEV: ON"):TEXT("DEV: OFF"),725,16,100,PC->bDeveloperMode);
 DrawText(FString::Printf(TEXT("SIM %.1fs"),PC->SimulationSeconds),Ink,840,24,GEngine->GetSmallFont());
 Button(TEXT("CameraProfile"),PC->bPlayGame3?TEXT("P PlayGame3"):TEXT("P Legacy"),960,16,124);
 if(PC->CalibrationScenario>=0)
 {
  const TCHAR* Scenario[]={TEXT("DUEL"),TEXT("WING"),TEXT("MISSILES")};
  DrawText(FString::Printf(TEXT("D116 CANDIDATE / %s"),Scenario[FMath::Clamp(PC->CalibrationScenario,0,2)]),Accent,20,22,GEngine->GetSmallFont());
 }
 Button(TEXT("CombatRange"),PC->CalibrationScenario>=0?TEXT("F5 LEGACY BATTLE"):CombatOn?TEXT("F5 RESET BATTLE"):TEXT("F5 COMBAT RANGE"),1092,16,158);
 for(int32 I=0;I<PC->Ships.Num();++I)if((PC->Ships[I]->bFriendly||PC->bDeveloperMode) && !(CombatOn&&PC->Combat->IsDeparted(PC->Ships[I])))Button(FName(*FString::Printf(TEXT("Ship%d"),I+1)),PC->Ships[I]->ShipName.Left(24),12,72+I*36,182,PC->Selected==PC->Ships[I]);
 Button(TEXT("CalibrationRange"),TEXT("F6 D116 CANDIDATE"),12,182,182);
 DrawText(TEXT("Ctrl+1..9 save / 1..9 select"),Ink,206,190,GEngine->GetSmallFont());
 if(PC->Selected) {
  const float Y=Height-83;DrawRect(Panel,10,Y,Width-20,74);
  const FName Names[]={TEXT("Manual"),TEXT("Auto"),TEXT("Move"),TEXT("Stop"),TEXT("Target"),TEXT("ClearTarget"),TEXT("Heading"),TEXT("Port"),TEXT("Starboard"),TEXT("Focus"),TEXT("Details"),TEXT("Cancel"),TEXT("Deselect")};
  const TCHAR* Labels[]={TEXT("MANUAL"),TEXT("AUTO"),TEXT("V MOVE"),TEXT("X STOP"),TEXT("T TARGET"),TEXT("CLEAR"),TEXT("H FACING"),TEXT("PORT"),TEXT("STBD"),TEXT("F FOLLOW"),TEXT("DETAILS"),TEXT("CANCEL"),TEXT("DESELECT")};
  for(int32 I=0;I<13;++I)Button(Names[I],Labels[I],20+I*86,Y+6,80);
  DrawText(PC->LastAction.Left(130),Accent,20,Y+47,GEngine->GetSmallFont());
 }
 for(AOVShip* Ship:PC->Ships) {
  if(CombatOn&&PC->Combat->IsDeparted(Ship)) continue;
  FVector2D P;if(PC->ProjectWorldLocationToScreen(Ship->GetActorLocation()+FVector(0,0,220),P)&&!IsOverPanel(P.X,P.Y)) {
   P/=UIScale;DrawText(Ship->ShipName,Ship->Systems->bWrecked?FLinearColor::Gray:Ship->bFriendly?Accent:FLinearColor(1,.3f,.2f),P.X,P.Y,GEngine->GetSmallFont());
   DrawRect(Health(Ship->Systems->HullHP/FMath::Max(1.f,Ship->Systems->MaxHullHP)),P.X,P.Y+16,100*Ship->Systems->HullHP/FMath::Max(1.f,Ship->Systems->MaxHullHP),3);
  }
 }
 DrawCombat(); DrawPhase2C(); DrawMission(); DrawDetails(); DrawInterior();
 Canvas->Canvas->PopTransform();
}
void AOVHUD::DrawCombat()
{
 const auto* PC=Cast<AOVController>(PlayerOwner);
 if(!PC || !PC->Selected || !PC->Combat || !PC->Combat->bEnabled) return;
 if(!PC->Selected->bFriendly && !PC->bDeveloperMode) return;
 const auto* C=PC->Combat.Get(); const auto* Ship=PC->Selected.Get();
 const FOVCombatShipState State=C->GetShipState(PC->Selected);
 DrawRect(Panel,10,216,430,344);
 DrawText(TEXT("2B / LIVE FIRE CONTROL"),Accent,20,226,GEngine->GetSmallFont());
 DrawText((TEXT("Target: ")+(Ship->AttackTarget?Ship->AttackTarget->ShipName:TEXT("NONE"))).Left(58),Ink,20,247,GEngine->GetSmallFont());
 const FOVDetectionInfo Detection=C->GetDetection(PC->Selected,Ship->AttackTarget);
 DrawText(FString::Printf(TEXT("Sensor: %s / bonus +%.1f%%"),*Detection.Source,Detection.AccuracyBonus*100),Ink,20,267,GEngine->GetSmallFont());
 const auto& Health=Ship->Systems->WeaponMountStates;
 const FName SelectedId=Health.IsValidIndex(PC->SelectedWeapon)?Health[PC->SelectedWeapon].WeaponMountId:NAME_None;
 if(const auto* Mount=State.Mounts.FindByPredicate([SelectedId](const FOVCombatMountState& M){return M.MountId==SelectedId;})) {
  const auto& M=*Mount;
  const auto R=C->GetReadiness(PC->Selected,M.MountId);
  DrawText(FString::Printf(TEXT("%s / %s"),*M.MountId.ToString(),*M.Profile.ProfileId.ToString()),Accent,20,287,GEngine->GetSmallFont());
  DrawText(FString::Printf(TEXT("%s | CD %.2fs | aim %.1f deg"),*R.Reason,R.Cooldown,R.AimError),R.bReady?FLinearColor::Green:FLinearColor::Yellow,20,307,GEngine->GetSmallFont());
  const FString AimQuality=M.Profile.Kind==EOVAttackKind::Beam?FString::Printf(TEXT("hit %.1f%%"),100*R.Accuracy):FString::Printf(TEXT("spread %.2f deg"),R.AimSpreadDegrees);
  DrawText(FString::Printf(TEXT("%.0f/%.0f cm | %s | queued %s"),R.Distance,M.Profile.Range,*AimQuality,M.bFireRequested?TEXT("YES"):TEXT("NO")),Ink,20,327,GEngine->GetSmallFont());
 }
 Button(TEXT("Fire"),TEXT("B FIRE"),20,351,90);
 Button(TEXT("FireAll"),TEXT("L FIRE ALL"),118,351,105);
 Button(TEXT("NextWeapon"),TEXT("N NEXT GUN"),231,351,110);
 Button(TEXT("AutoFree"),TEXT("AUTO FREE"),349,351,81);
 DrawText(FString::Printf(TEXT("Shots %d / hits %d / flying %d"),C->ShotCount,C->HitCount,C->Attacks.Num()),Ink,20,386,GEngine->GetSmallFont());
 if(C->RecentEvents.Num()) {
  const auto& E=C->RecentEvents.Last();
  DrawText(FString::Printf(TEXT("#%d %s / hull -%.1f / blasts %d"),E.AttackId,*StaticEnum<EOVAttackResult>()->GetNameStringByValue(static_cast<int64>(E.Result)),E.HullDamage,E.Explosions),Accent,20,407,GEngine->GetSmallFont());
 }
 DrawText(C->GetOrderStatus(PC->Selected),FLinearColor::Yellow,20,454,GEngine->GetSmallFont());
 Button(TEXT("HullIntent"),TEXT("HULL"),20,475,62);
 Button(TEXT("GunIntent"),TEXT("GUNS"),88,475,62);
 Button(TEXT("ForceGun"),TEXT("FORCE GUN!"),156,475,108);
 Button(TEXT("CancelAttack"),TEXT("CANCEL FIRE"),270,475,160);
 const TCHAR* Policies[]={TEXT("CONTINUE"),TEXT("UNTIL GOAL"),TEXT("UNTIL EXIT")};
 Button(TEXT("Protection"),FString(TEXT("HOLD: "))+Policies[static_cast<int32>(State.Protection)],20,507,165);
 if(PC->bDeveloperMode) {
  Button(TEXT("DevInterior"),TEXT("DEV INSIDE"),191,507,117);
  Button(TEXT("DevObjective"),TEXT("DEV GOAL"),314,507,116);
 }
 if(Ship->AttackTarget && Ship->AttackTarget->Systems->IsInteriorDeveloperInput(Ship->Systems->FactionId))
  DrawText(TEXT("DEV injected own-faction interior state"),FLinearColor::Yellow,20,539,GEngine->GetSmallFont());
 Button(TEXT("MoveAuto"),Ship->IsMovementAutomatic()?TEXT("MOVE: AUTO"):TEXT("MOVE: MANUAL"),20,422,195,Ship->IsMovementAutomatic());
 Button(TEXT("FireAuto"),Ship->IsAttackAutomatic()?TEXT("FIRE: AUTO"):TEXT("FIRE: MANUAL"),223,422,207,Ship->IsAttackAutomatic());
}

void AOVHUD::BeginPlay()
{
 Super::BeginPlay();
 if(!PlayerOwner || !PlayerOwner->IsLocalController()) return;
 DevelopmentWidget=CreateWidget<UOVDevelopmentWidget>(PlayerOwner,UOVDevelopmentWidget::StaticClass());
 HeroWidget=CreateWidget<UOVHeroWidget>(PlayerOwner,UOVHeroWidget::StaticClass());
 if(DevelopmentWidget){DevelopmentWidget->AddToViewport(20);DevelopmentWidget->SetVisibility(ESlateVisibility::Collapsed);}
 if(HeroWidget)HeroWidget->AddToViewport(10);
 SyncAddedUI();
}
void AOVHUD::SyncAddedUI()
{
 auto* PC=Cast<AOVController>(PlayerOwner);if(!PC)return;int32 W=0,H=0;PC->GetViewportSize(W,H);if(W<=0||H<=0)return;
 const float Dpi=FMath::Max(.01f,UWidgetLayoutLibrary::GetViewportScale(this));
 const FVector2D DevSize(FMath::Min(760.f,W*.8f),FMath::Min(530.f,H*.78f));
 const FVector2D DevPos((W-DevSize.X)*.5f,(H-DevSize.Y)*.5f);DevelopmentRect=FBox2D(DevPos,DevPos+DevSize);
 const FVector2D HeroSize(FMath::Min(310.f,W*.27f),FMath::Min(310.f,H*.48f));
 const FVector2D HeroPos(W-HeroSize.X-12.f,100.f);HeroRect=FBox2D(HeroPos,HeroPos+HeroSize);
 if(DevelopmentWidget){DevelopmentWidget->SetPositionInViewport(DevPos/Dpi,false);DevelopmentWidget->SetDesiredSizeInViewport(DevSize/Dpi);DevelopmentWidget->SetVisibility(PC->bDeveloperMode?ESlateVisibility::Visible:ESlateVisibility::Collapsed);}
 if(HeroWidget){HeroWidget->SetPositionInViewport(HeroPos/Dpi,false);HeroWidget->SetDesiredSizeInViewport(HeroSize/Dpi);HeroWidget->SetVisibility(PC->bDeveloperMode?ESlateVisibility::Collapsed:ESlateVisibility::Visible);}
}
bool AOVHUD::IsOverAddedUI(float X,float Y) const
{
 const auto* PC=Cast<AOVController>(PlayerOwner);if(!PC)return false;const FVector2D P(X,Y);
 return PC->bDeveloperMode?DevelopmentWidget&&DevelopmentRect.IsInsideOrOn(P):HeroWidget&&HeroRect.IsInsideOrOn(P);
}
bool AOVHUD::IsAddedUIEditing() const
{
 const auto* PC=Cast<AOVController>(PlayerOwner);return PC&&PC->bDeveloperMode&&DevelopmentWidget&&DevelopmentWidget->IsEditing();
}
