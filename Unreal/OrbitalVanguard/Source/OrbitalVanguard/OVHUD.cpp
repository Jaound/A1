#include "OVHUD.h"
#include "OVController.h"
#include "OVShip.h"
#include "OVShipSystemsComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "CanvasItem.h"
#include "CanvasTypes.h"
namespace {
const FLinearColor Ink(.75f,.86f,.94f),Accent(.18f,.85f,1),Panel(.008f,.018f,.035f,.96f);
FLinearColor Health(float R) { return R<=0?FLinearColor(.2f,.23f,.26f):R<.3f?FLinearColor(.85f,.15f,.1f):R<.7f?FLinearColor(.9f,.65f,.1f):FLinearColor(.15f,.65f,.4f); }
FString Facility(EOVFacilityType T) { return StaticEnum<EOVFacilityType>()->GetNameStringByValue(static_cast<int64>(T)); }
}
void AOVHUD::Button(FName N,const FString& L,float X,float Y,float W,bool Active)
{
 DrawRect(Active?FLinearColor(.03f,.4f,.55f,.95f):FLinearColor(.04f,.08f,.13f,.98f),X,Y,W,27);
 DrawText(L,Active?FLinearColor::White:Ink,X+6,Y+5,GEngine->GetSmallFont());
 AddHitBox(FVector2D(X,Y)*UIScale,FVector2D(W,27)*UIScale,N,true,10);
}
bool AOVHUD::IsOverPanel(float X,float Y) const
{
 const auto* PC=Cast<AOVController>(PlayerOwner); if(!Canvas||!PC) return false;
 const FVector2D P(X/UIScale,Y/UIScale);
 if(bDragging||bResizing) return true;
 if(P.Y<55 || (PC->Selected&&P.Y>Canvas->SizeY/UIScale-88) || (P.X<200&&P.Y<210)) return true;
 return PC->Selected&&bDetailsOpen&&P.X>=WindowPosition.X&&P.X<=WindowPosition.X+WindowSize.X&&P.Y>=WindowPosition.Y&&P.Y<=WindowPosition.Y+(bMinimized?32:WindowSize.Y);
}
bool AOVHUD::HandlePointer(float X,float Y,bool Pressed,bool Down,float Wheel,bool Orbit,float DX,float DY)
{
 const bool Over=IsOverPanel(X,Y); const auto* PC=Cast<AOVController>(PlayerOwner);
 if(!PC||!PC->Selected||!bDetailsOpen) { bDragging=false;bResizing=false;return Over; }
 const FVector2D P(X/UIScale,Y/UIScale);
 if(Pressed&&Over) {
  if(P.X>=WindowPosition.X&&P.X<WindowPosition.X+WindowSize.X-90&&P.Y>=WindowPosition.Y&&P.Y<WindowPosition.Y+30) { bDragging=true;DragOffset=P-WindowPosition; }
  if(!bMinimized&&P.X>WindowPosition.X+WindowSize.X-18&&P.Y>WindowPosition.Y+WindowSize.Y-18) bResizing=true;
 }
 if(!Down) { bDragging=false;bResizing=false; }
 if(Canvas&&bDragging) {
  WindowPosition=P-DragOffset;
  WindowPosition.X=FMath::Clamp(WindowPosition.X,0.0,FMath::Max(0.0,Canvas->SizeX/UIScale-WindowSize.X));
  WindowPosition.Y=FMath::Clamp(WindowPosition.Y,56.0,FMath::Max(56.0,Canvas->SizeY/UIScale-WindowSize.Y-90));
 }
 if(Canvas&&bResizing) { WindowSize.X=FMath::Clamp(P.X-WindowPosition.X,400.0,700.0);WindowSize.Y=FMath::Clamp(P.Y-WindowPosition.Y,550.0,750.0); }
 if(Over&&!bMinimized&&P.X>WindowPosition.X&&P.Y>WindowPosition.Y+65&&P.Y<WindowPosition.Y+275) {
  if(Orbit) { PreviewRotation.Yaw+=DX;PreviewRotation.Pitch=FMath::Clamp(PreviewRotation.Pitch-DY,-85.,85.); }
  if(Wheel) PreviewZoom=FMath::Clamp(PreviewZoom+Wheel*.1f,.5f,1.6f);
 }
 return Over||bDragging||bResizing;
}
void AOVHUD::NotifyHitBoxClick(FName N)
{
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
 const FQuat R=PreviewRotation.Quaternion();const float Scale=W/440.f*PreviewZoom;
 auto Project=[&](FVector P) { P=R.RotateVector(P)*Scale;return FVector2D(X+W*.5f+P.Y,Y+100-P.Z); };
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
  for(int32 J=0;J<4;++J) DrawLine(P[J].X,P[J].Y,P[(J+1)%4].X,P[(J+1)%4].Y,Color,2);
  const FVector2D C=Project(D.LocalCenter);
  AddHitBox((C-FVector2D(10,8))*UIScale,FVector2D(20,16)*UIScale,FName(*FString::Printf(TEXT("Armor_%d"),Face.Index)),true,20);
  DrawRect(Color,C.X-4,C.Y-4,8,8);
 }
 for(int32 I=0;I<L->WeaponMounts.Num();++I) {
  const auto& M=L->WeaponMounts[I];const FVector2D P=Project(M.LocalPosition);
  DrawRect(I==PC->SelectedWeapon?Accent:Health(S->WeaponMountStates[I].HP/M.MaxHP),P.X-6,P.Y-6,12,12);
  AddHitBox((P-FVector2D(7,7))*UIScale,FVector2D(14,14)*UIScale,FName(*FString::Printf(TEXT("Weapon_%d"),I)),true,30);
 }
 DrawText(TEXT("READ ONLY 3D | MMB orbit / wheel zoom"),Ink,X+8,Y+174,GEngine->GetSmallFont());
}
void AOVHUD::DrawDetails()
{
 auto* PC=Cast<AOVController>(PlayerOwner);if(!PC->Selected||!bDetailsOpen)return;
 const auto* S=PC->Selected->Systems.Get();const auto* L=S->Layout.Get();if(!L)return;
 const float X=WindowPosition.X,Y=WindowPosition.Y,W=WindowSize.X;
 DrawRect(Panel,X,Y,W,bMinimized?32:WindowSize.Y);
 DrawText(PC->Selected->ShipName,Accent,X+10,Y+8,GEngine->GetSmallFont());
 Button(TEXT("WindowMin"),bMinimized?TEXT("+"):TEXT("_"),X+W-90,Y+2,26);Button(TEXT("WindowMax"),TEXT("[]"),X+W-60,Y+2,26);Button(TEXT("WindowClose"),TEXT("X"),X+W-30,Y+2,26);
 if(bMinimized)return;
 Button(TEXT("External"),TEXT("ARMOR / WEAPONS"),X+10,Y+38,145,PC->ViewedDeck==0);
 const bool Reveal=PC->Selected->bFriendly||PC->bDeveloperMode;
 for(int32 D=1;D<=L->DeckCount;++D) if(Reveal)Button(FName(*FString::Printf(TEXT("Deck%d"),D)),FString::Printf(TEXT("%dF"),D),X+165+(D-1)*52,Y+38,46,PC->ViewedDeck==D);
 if(PC->ViewedDeck==0)DrawExternal(X+10,Y+72,W-20);
 else if(Reveal) {
  int32 Cell=0;
  for(int32 I=0;I<L->Slots.Num();++I)if(L->Slots[I].DeckId==PC->ViewedDeck) {
   const float CX=X+10+(Cell%5)*(W-20)/5,CY=Y+78+(Cell/5)*54;++Cell;
   const auto& D=L->Slots[I];const auto& State=S->Slots[I];
   Button(FName(*FString::Printf(TEXT("Slot_%d"),I)),D.SlotId.ToString(),CX,CY,(W-30)/5,I==PC->SelectedSlot);
   DrawRect(Health(D.MaxFacilityHP>0?State.HP/D.MaxFacilityHP:0),CX,CY+30,(W-30)/5,4);
   DrawText(Facility(D.Facility).Left(10),Ink,CX,CY+36,GEngine->GetSmallFont());
  }
 }
 DrawText(FString::Printf(TEXT("HP %.0f/%.0f | Shield %.0f/%.0f %s"),S->HullHP,S->MaxHullHP,S->Shield,S->MaxShield,S->bShieldActive?TEXT("ON"):TEXT("OFF")),Ink,X+10,Y+280,GEngine->GetSmallFont());
 if(S->ArmorSections.IsValidIndex(PC->SelectedArmor)) {
  const auto& A=S->ArmorSections[PC->SelectedArmor];
  DrawText(FString::Printf(TEXT("Armor %s %.0f/%.0f"),*A.ArmorSectionId.ToString(),A.HP,L->ArmorSections[PC->SelectedArmor].MaxArmor),Accent,X+10,Y+301,GEngine->GetSmallFont());
 }
 if(PC->ViewedDeck==0&&S->WeaponMountStates.IsValidIndex(PC->SelectedWeapon)) {
  const auto& M=S->WeaponMountStates[PC->SelectedWeapon];const auto& D=L->WeaponMounts[PC->SelectedWeapon];
  DrawText(FString::Printf(TEXT("Weapon %s T%.0f HP %.0f/%.0f %s"),*M.WeaponMountId.ToString(),D.Tier,M.HP,D.MaxHP,S->IsWeaponAvailable(M.WeaponMountId)?TEXT("READY"):TEXT("OFF")),Ink,X+10,Y+322,GEngine->GetSmallFont());
 }else if(Reveal&&L->Slots.IsValidIndex(PC->SelectedSlot)) {
  const auto& F=S->Slots[PC->SelectedSlot];const auto& D=L->Slots[PC->SelectedSlot];
  DrawText(FString::Printf(TEXT("%s %s HP %.0f/%.0f Crew %.0f/%.0f"),*F.SlotId.ToString(),*Facility(D.Facility),F.HP,D.MaxFacilityHP,F.Crew,D.RequiredCrew),Ink,X+10,Y+322,GEngine->GetSmallFont());
  DrawText(S->GetSlotStatus(F.SlotId),Accent,X+10,Y+341,GEngine->GetSmallFont());
 }
 if(Reveal)DrawText(FString::Printf(TEXT("Engine %.0f%% Shield %.0f%% Radar %.0f%%"),100*S->GetEfficiency(EOVFacilityType::Engine),100*S->GetEfficiency(EOVFacilityType::ShieldControl),100*S->GetEfficiency(EOVFacilityType::Radar)),Ink,X+10,Y+362,GEngine->GetSmallFont());
 else DrawText(TEXT("Enemy interior hidden / no boarding reveal"),Ink,X+10,Y+362,GEngine->GetSmallFont());
 if(PC->bDeveloperMode) {
  DrawText(TEXT("DEV ONLY: direct tools, not supply repair"),FLinearColor::Yellow,X+10,Y+384,GEngine->GetSmallFont());
  const FName Names[]={TEXT("TestHit"),TEXT("ArmorZero"),TEXT("RepairArmor"),TEXT("Reset"),TEXT("DestroyExt"),TEXT("Sabotage"),TEXT("RepairSlot"),TEXT("Occupy"),TEXT("CrewDown"),TEXT("CrewUp"),TEXT("WeaponHit"),TEXT("RepairWeapon")};
  const TCHAR* Labels[]={TEXT("HIT 150"),TEXT("ARMOR 0"),TEXT("FIX ARMOR"),TEXT("RESET"),TEXT("DESTROY"),TEXT("SABOTAGE"),TEXT("FIX ROOM"),TEXT("OCCUPY"),TEXT("CREW -2"),TEXT("CREW +2"),TEXT("HIT GUN"),TEXT("FIX GUN")};
  for(int32 I=0;I<12;++I)Button(Names[I],Labels[I],X+10+(I%4)*(W-20)/4,Y+405+(I/4)*33,(W-32)/4);
  DrawText(FString::Printf(TEXT("Event %d: %s -> %s / HP -%.1f"),S->HitEventId,*S->LastArmorSection.ToString(),*S->LastDamageSlot.ToString(),S->LastHullDamage),Accent,X+10,Y+510,GEngine->GetSmallFont());
 }
 DrawLine(X+W-16,Y+WindowSize.Y-2,X+W-2,Y+WindowSize.Y-16,Ink,2);
}
void AOVHUD::DrawHUD()
{
 Super::DrawHUD();auto* PC=Cast<AOVController>(PlayerOwner);if(!PC||!Canvas||PC->Ships.Num()<3)return;
 UIScale=FMath::Max(.25f,FMath::Min(1.f,FMath::Min(Canvas->SizeX/1280.f,Canvas->SizeY/740.f)));
 Canvas->Canvas->PushRelativeTransform(FScaleMatrix(FVector(UIScale,UIScale,1)));
 const float Width=Canvas->SizeX/UIScale,Height=Canvas->SizeY/UIScale;
 DrawRect(Panel,10,10,Width-20,40);DrawText(TEXT("ORBITAL VANGUARD / 2A TEST RANGE"),Accent,20,22,GEngine->GetSmallFont());
 Button(TEXT("Pause"),PC->bSimulationPaused?TEXT("F1 RESUME"):TEXT("F1 PAUSE"),380,16,100,PC->bSimulationPaused);
 Button(TEXT("Speed1"),TEXT("F2 1x"),488,16,68,PC->SimulationSpeed==1);Button(TEXT("Speed2"),TEXT("F3 2x"),564,16,68,PC->SimulationSpeed==2);Button(TEXT("Speed3"),TEXT("F4 3x"),640,16,68,PC->SimulationSpeed==3);
 Button(TEXT("Developer"),PC->bDeveloperMode?TEXT("DEV: ON"):TEXT("DEV: OFF"),725,16,100,PC->bDeveloperMode);
 DrawText(FString::Printf(TEXT("SIM %.1fs"),PC->SimulationSeconds),Ink,840,24,GEngine->GetSmallFont());
 for(int32 I=0;I<PC->Ships.Num();++I)if(PC->Ships[I]->bFriendly||PC->bDeveloperMode)Button(FName(*FString::Printf(TEXT("Ship%d"),I+1)),PC->Ships[I]->ShipName.Left(24),12,72+I*36,182,PC->Selected==PC->Ships[I]);
 DrawText(TEXT("Ctrl+1..9 save / 1..9 select"),Ink,14,190,GEngine->GetSmallFont());
 if(PC->Selected) {
  const float Y=Height-83;DrawRect(Panel,10,Y,Width-20,74);
  const FName Names[]={TEXT("Manual"),TEXT("Auto"),TEXT("Move"),TEXT("Stop"),TEXT("Target"),TEXT("ClearTarget"),TEXT("Heading"),TEXT("Port"),TEXT("Starboard"),TEXT("Focus"),TEXT("Details"),TEXT("Cancel"),TEXT("Deselect")};
  const TCHAR* Labels[]={TEXT("MANUAL"),TEXT("AUTO"),TEXT("V MOVE"),TEXT("X STOP"),TEXT("T TARGET"),TEXT("CLEAR"),TEXT("H FACING"),TEXT("PORT"),TEXT("STBD"),TEXT("F FOLLOW"),TEXT("DETAILS"),TEXT("CANCEL"),TEXT("DESELECT")};
  for(int32 I=0;I<13;++I)Button(Names[I],Labels[I],20+I*86,Y+6,80);
  DrawText(PC->LastAction.Left(130),Accent,20,Y+47,GEngine->GetSmallFont());DrawDetails();
 }
 DrawText(TEXT("WASD pan | MMB/Z/C orbit | Q/E height | Wheel zoom | Backspace cancel"),Ink,210,58,GEngine->GetSmallFont());
 for(AOVShip* Ship:PC->Ships) {
  FVector2D P;if(PC->ProjectWorldLocationToScreen(Ship->GetActorLocation()+FVector(0,0,220),P)) {
   P/=UIScale;DrawText(Ship->ShipName,Ship->Systems->bWrecked?FLinearColor::Gray:Ship->bFriendly?Accent:FLinearColor(1,.3f,.2f),P.X,P.Y,GEngine->GetSmallFont());
   DrawRect(Health(Ship->Systems->HullHP/FMath::Max(1.f,Ship->Systems->MaxHullHP)),P.X,P.Y+16,100*Ship->Systems->HullHP/FMath::Max(1.f,Ship->Systems->MaxHullHP),3);
  }
 }
 Canvas->Canvas->PopTransform();
}

