#include "OVHUD.h"
#include "OVDevelopmentWidget.h"
#include "OVHeroWidget.h"
#include "OVShipPreview.h"
#include "OVCarrierSubsystem.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "OVController.h"
#include "OVShip.h"
#include "OVShipSystemsComponent.h"
#include "OVCombatSubsystem.h"
#include "OVBattleNotices.h"
#include "OVPointDefenseSubsystem.h"
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
UOVBattleNotices* AOVHUD::GetBattleNoticeFeed() const
{
 return GetWorld()?GetWorld()->GetSubsystem<UOVBattleNotices>():nullptr;
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
 if(bDragging||bResizing||DraggedPin!=INDEX_NONE) return true;
 if(!PC->bDeveloperMode&&!bSceneMenuOpen&&PC->Phase2CScenario>=0&&P.X>=212&&P.X<=484&&P.Y>=76&&P.Y<=224)return true;
 if(!PC->bDeveloperMode&&!bHeroPanelOpen)for(int32 I=0;I<PinnedShips.Num();++I) {
  const FVector2D Pos=GetPinnedPosition(I);
  const FVector2D Size=GetPinnedSize(I);
  if(P.X>=Pos.X&&P.X<=Pos.X+Size.X&&P.Y>=Pos.Y&&P.Y<=Pos.Y+Size.Y)return true;
 }
 const int32 Notices=BattleNoticeRows();const float NoticeX=ViewportPixels.X/UIScale*.5f-120;
 const auto* Feed=GetBattleNoticeFeed();const int32 Footer=bBattleFeedOpen?66+(Feed&&Feed->DroppedCount>0?20:0):0;
 if(Notices>0&&P.X>=NoticeX&&P.X<=NoticeX+240&&P.Y>=66&&P.Y<=66+Notices*32+Footer)return true;
 // The basic HUD remains visible under the separate developer window.
 if(P.Y<62 || (P.X<620 && P.Y>ViewportPixels.Y/UIScale-220) || (P.X<204 && P.Y<ViewportPixels.Y/UIScale-230) || (bSceneMenuOpen && P.X>=210 && P.X<=650 && P.Y<=205)) return true;
 return !PC->bDeveloperMode&&PC->Selected&&bDetailsOpen&&P.X>=WindowPosition.X&&P.X<=WindowPosition.X+WindowSize.X&&P.Y>=WindowPosition.Y&&P.Y<=WindowPosition.Y+(bMinimized?32:WindowSize.Y);
}
bool AOVHUD::HasOrbitPreview() const
{
 const auto* PC=Cast<AOVController>(PlayerOwner);
 return PC && !PC->bDeveloperMode && PC->Selected && PC->Selected->Systems->Layout && bDetailsOpen && !bMinimized && !bHangarView && PC->ViewedDeck==0;
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
 if(PC&&PC->bDeveloperMode){
  bDragging=false;bResizing=false;bInteriorDragging=false;bInteriorResizing=false;DraggedPin=INDEX_NONE;bPinResizing=false;
  const FVector2D Cursor(X,Y);
  if(Pressed&&DevelopmentRect.IsInsideOrOn(Cursor)) {
   const float Dpi=FMath::Max(.01f,UWidgetLayoutLibrary::GetViewportScale(this));
   if(Cursor.Y<DevelopmentRect.Min.Y+36*Dpi&&Cursor.X<DevelopmentRect.Max.X-200*Dpi) {bDevDragging=true;DevelopmentDragOffset=Cursor-DevelopmentRect.Min;}
   else if(!bDevelopmentMinimized&&Cursor.X>DevelopmentRect.Max.X-20*Dpi&&Cursor.Y>DevelopmentRect.Max.Y-20*Dpi)bDevResizing=true;
  }
  if(!Down){bDevDragging=false;bDevResizing=false;}
  if(bDevDragging){bDevelopmentMaximized=false;DevelopmentPosition=Cursor-DevelopmentDragOffset;SyncAddedUI();}
  if(bDevResizing){bDevelopmentMaximized=false;DevelopmentPosition=DevelopmentRect.Min;DevelopmentSize=Cursor-DevelopmentRect.Min;SyncAddedUI();}
  return Over||bDevDragging||bDevResizing;
 }
 bDevDragging=false;bDevResizing=false;
 // A drag begun in the existing window retains ownership; a new interior gesture consumes all buttons/wheel.
 const FVector2D P(X/UIScale,Y/UIScale);
 if(DraggedPin!=INDEX_NONE) {
  if(!Down||!PinnedPositions.IsValidIndex(DraggedPin)||!PinnedSizes.IsValidIndex(DraggedPin)) {DraggedPin=INDEX_NONE;bPinResizing=false;return true;}
  const float Width=ViewportPixels.X/UIScale,Height=ViewportPixels.Y/UIScale;
  const FVector2D Size=GetPinnedSize(DraggedPin);
  if(bPinResizing) {
   const FVector2D Pos=GetPinnedPosition(DraggedPin);
   PinnedSizes[DraggedPin]=FVector2D(FMath::Clamp(P.X-Pos.X,266.,FMath::Max(266.,FMath::Min(550.,double(Width-Pos.X)))),
       FMath::Clamp(P.Y-Pos.Y,158.,FMath::Max(158.,FMath::Min(400.,double(Height-Pos.Y)))));
  } else PinnedPositions[DraggedPin]=FVector2D(FMath::Clamp(P.X-PinDragOffset.X,0.,FMath::Max(0.,double(Width-Size.X))),
      FMath::Clamp(P.Y-PinDragOffset.Y,56.,FMath::Max(56.,double(Height-Size.Y))));
  return true;
 }
 if(!bDragging&&!bResizing&&(bInteriorDragging||bInteriorResizing||IsOverInteriorPanel(X,Y))) {
  const FBox2D Bounds=InteriorBounds();
  if(Pressed) {
   if(P.X>=Bounds.Min.X&&P.X<Bounds.Max.X-164&&P.Y>=Bounds.Min.Y&&P.Y<Bounds.Min.Y+30) {bInteriorDragging=true;InteriorDragOffset=P-Bounds.Min;}
   else if(!bInteriorMinimized&&P.X>=Bounds.Max.X-18&&P.Y>=Bounds.Max.Y-18) bInteriorResizing=true;
  }
  if(!Down){bInteriorDragging=false;bInteriorResizing=false;}
  if(bInteriorDragging){bInteriorMaximized=false;InteriorWindowPosition=P-InteriorDragOffset;}
  if(bInteriorResizing){bInteriorMaximized=false;InteriorWindowPosition=Bounds.Min;InteriorWindowSize=FVector2D(FMath::Max(620.,P.X-Bounds.Min.X),FMath::Max(500.,P.Y-Bounds.Min.Y));}
  return true;
 }
 if(PC&&!PC->bDeveloperMode&&!bHeroPanelOpen&&!bDragging&&!bResizing&&Pressed) {
  const bool DetailsCover=PC->Selected&&bDetailsOpen&&P.X>=WindowPosition.X&&P.X<=WindowPosition.X+WindowSize.X
      &&P.Y>=WindowPosition.Y&&P.Y<=WindowPosition.Y+(bMinimized?32:WindowSize.Y);
  if(!DetailsCover)for(int32 I=PinnedShips.Num()-1;I>=0;--I) {
   const FVector2D Pos=GetPinnedPosition(I);
   const FVector2D Size=GetPinnedSize(I);
   if(P.X>=Pos.X+Size.X-18&&P.X<=Pos.X+Size.X&&P.Y>=Pos.Y+Size.Y-18&&P.Y<=Pos.Y+Size.Y) {
    DraggedPin=I;bPinResizing=true;PinnedPositions[I]=Pos;return true;
   }
   if(P.X>=Pos.X+4&&P.X<=Pos.X+42&&P.Y>=Pos.Y+4&&P.Y<=Pos.Y+31) {
    DraggedPin=I;bPinResizing=false;PinDragOffset=P-Pos;PinnedPositions[I]=Pos;return true;
   }
   // Later cards draw on top; their body must block gestures on a card underneath.
   if(P.X>=Pos.X&&P.X<=Pos.X+Size.X&&P.Y>=Pos.Y&&P.Y<=Pos.Y+Size.Y)return true;
  }
 }
 if(!PC||!PC->Selected||!bDetailsOpen) { bDragging=false;bResizing=false;return Over; }
 if(Pressed&&Over) {
  if(P.X>=WindowPosition.X&&P.X<WindowPosition.X+WindowSize.X-134&&P.Y>=WindowPosition.Y&&P.Y<WindowPosition.Y+30) { bDragging=true;DragOffset=P-WindowPosition; }
  if(!bMinimized&&P.X>WindowPosition.X+WindowSize.X-18&&P.X<=WindowPosition.X+WindowSize.X&&P.Y>WindowPosition.Y+WindowSize.Y-18&&P.Y<=WindowPosition.Y+WindowSize.Y) bResizing=true;
 }
 if(!Down) { bDragging=false;bResizing=false; }
 if(bDragging) {
  WindowPosition=P-DragOffset;
  WindowPosition.X=FMath::Clamp(WindowPosition.X,0.0,FMath::Max(0.0,ViewportPixels.X/UIScale-WindowSize.X));
  WindowPosition.Y=FMath::Clamp(WindowPosition.Y,56.0,FMath::Max(56.0,ViewportPixels.Y/UIScale-WindowSize.Y-90));
 }
 if(bResizing) { WindowSize.X=FMath::Clamp(P.X-WindowPosition.X,400.0,700.0);WindowSize.Y=FMath::Clamp(P.Y-WindowPosition.Y,420.0,750.0); }
 if(IsOverOrbitPreview(FVector2D(X,Y))) {
  if(Orbit) OrbitPreview(FVector2D(DX,DY));
  if(Wheel) PreviewZoom=FMath::Clamp(PreviewZoom+Wheel*.1f,.5f,1.6f);
 }
 return Over||bDragging||bResizing;
}
void AOVHUD::NotifyHitBoxClick(FName N)
{
 // Different rendered buttons need unique hitboxes even when they share a game command.
 if(N==TEXT("MissionHeroPanel"))N=TEXT("HeroPanel");
 if(N==TEXT("CommandHeroPanel")||N==TEXT("InteriorHeroPanel"))N=TEXT("HeroPanel");
 if(N==TEXT("HangarLaunchAircraft"))N=TEXT("LaunchAircraft");
 if(N==TEXT("HangarRecallAircraft"))N=TEXT("RecallAircraft");
 if(N==TEXT("DetailsInteriorToggle"))N=TEXT("InteriorToggle");
 if(N==TEXT("SceneMenuClose")){bSceneMenuOpen=false;return;}
 if(HandleInteriorAction(N)) return;
 if(N==TEXT("HeroPanel")) { SetHeroPanelOpen(!bHeroPanelOpen);return; }
 if(N==TEXT("BattleFeedToggle")) { bBattleFeedOpen=!bBattleFeedOpen;BattleFeedPage=0;BattleFeedFilter=0;return; }
 if(N==TEXT("BattleWarningLevel")) {if(bBattleFeedOpen)WarningLevel=(WarningLevel+2)%3;return;}
 if(N==TEXT("BattleFeedFilter")) {if(bBattleFeedOpen){BattleFeedFilter=(BattleFeedFilter+1)%6;BattleFeedPage=0;}return;}
 if(N==TEXT("BattleFeedOlder")||N==TEXT("BattleFeedNewer")) {
  const auto Indices=BattleNoticeIndices();
  if(bBattleFeedOpen&&!Indices.IsEmpty())BattleFeedPage=FMath::Clamp(BattleFeedPage+(N==TEXT("BattleFeedOlder")?1:-1),0,(Indices.Num()-1)/3);
  return;
 }
 if(N.ToString().StartsWith(TEXT("BattleNotice_"))) {
  const int64 Serial=FCString::Atoi64(*N.ToString().Mid(13));
  auto* PC=Cast<AOVController>(PlayerOwner);auto* Feed=GetWorld()->GetSubsystem<UOVBattleNotices>();
  if(PC&&Feed&&!Feed->Notices.IsEmpty()) {
   const int32 Index=Feed->Notices.IndexOfByPredicate([Serial](const FOVBattleNotice& Item){return Item.Serial==Serial;});
   const auto Indices=BattleNoticeIndices();const int32 Pages=FMath::Max(1,(Indices.Num()+2)/3);
   const int32 Page=bBattleFeedOpen?FMath::Clamp(BattleFeedPage,0,Pages-1):0;
   const int32 Position=Indices.IndexOfByKey(Index);
   if(Position!=INDEX_NONE&&Position>=Page*3&&Position<Page*3+VisibleBattleNoticeCount()) {PC->bFollow=false;PC->CameraFocus=Feed->Notices[Index].LastPosition;PC->UpdateCamera();}
  }
  return;
 }
 if(N==TEXT("MissionBackground")) return;
 if(N==TEXT("WindowBackground")) return;
 if(N.ToString().StartsWith(TEXT("PinnedBackground_"))) return;
 if(N==TEXT("SceneMenu")) { bSceneMenuOpen=!bSceneMenuOpen;return; }
 if(N==TEXT("FleetPrevious")) { FleetPage=FMath::Max(0,FleetPage-1);return; }
 if(N==TEXT("FleetNext")) { ++FleetPage;return; }
 if(N==TEXT("AircraftPrevious")) { AircraftPage=FMath::Max(0,AircraftPage-1);return; }
 if(N==TEXT("AircraftNext")) { ++AircraftPage;return; }
 if(N==TEXT("AircraftFilter")) { AircraftFilter=(AircraftFilter+1)%4;AircraftPage=0;return; }
 if(N==TEXT("AirScope")) { bFleetAirOverview=!bFleetAirOverview;return; }
 if(N==TEXT("AircraftLocate")) {
  auto* PC=Cast<AOVController>(PlayerOwner);auto* Carrier=GetWorld()->GetSubsystem<UOVCarrierSubsystem>();
  if(PC&&PC->Selected&&PC->Selected->bFriendly&&Carrier&&bDetailsOpen&&!bMinimized&&bHangarView&&bAircraftGrid&&!PC->bDeveloperMode&&!bHeroPanelOpen) {
   if(const auto* A=Carrier->Aircraft.FindByPredicate([&](const FOVAircraftRecord& Item){return Item.AircraftId==SelectedAircraftId&&Item.Carrier==PC->Selected&&Item.FactionId==PC->Selected->Systems->FactionId&&Item.State!=EOVAircraftState::Wrecked;})) {
    PC->bFollow=false;PC->CameraFocus=A->State==EOVAircraftState::Stored?PC->Selected->GetActorLocation():A->Position;
    PC->UpdateCamera();PC->LastAction=TEXT("기체 위치로 이동");
   }
  }
  return;
 }
 if(N.ToString().StartsWith(TEXT("AircraftCard_"))) {
  const int32 Id=FCString::Atoi(*N.ToString().Mid(13));
  auto* PC=Cast<AOVController>(PlayerOwner);
  auto* Carrier=GetWorld()->GetSubsystem<UOVCarrierSubsystem>();
  if(PC&&PC->Selected&&Carrier&&(PC->Selected->bFriendly||PC->bDeveloperMode))
   if(Carrier->Aircraft.ContainsByPredicate([&](const FOVAircraftRecord& A){return A.AircraftId==Id&&A.Carrier==PC->Selected&&A.FactionId==PC->Selected->Systems->FactionId;})) SelectedAircraftId=Id;
  return;
 }
 if(N==TEXT("HangarView")) { bHangarView=true;return; }
 if(N==TEXT("HangarDisplayMode")) { bAircraftGrid=!bAircraftGrid;return; }
 if(N==TEXT("External")||N.ToString().StartsWith(TEXT("Deck")))bHangarView=false;
 if(N==TEXT("WindowPin")) {
  auto* PC=Cast<AOVController>(PlayerOwner);
  const int32 Index=PinnedShips.IndexOfByKey(PC->Selected);
  if(Index!=INDEX_NONE){DraggedPin=INDEX_NONE;bPinResizing=false;PinnedShips.RemoveAt(Index);PinnedPreviews[Index]->Release();PinnedPreviews.RemoveAt(Index);PinnedDecks.RemoveAt(Index);PinnedSlots.RemoveAt(Index);PinnedWeapons.RemoveAt(Index);PinnedSlotPages.RemoveAt(Index);PinnedPositions.RemoveAt(Index);PinnedSizes.RemoveAt(Index);}
  else if(PinnedShips.Num()<2){PinnedShips.Add(PC->Selected);PinnedPreviews.Add(NewObject<UOVShipPreview>(this));PinnedDecks.Add(0);PinnedSlots.Add(INDEX_NONE);PinnedWeapons.Add(INDEX_NONE);PinnedSlotPages.Add(0);PinnedPositions.Add(FVector2D(-1,-1));PinnedSizes.Add(FVector2D(266,158));}
  return;
 }
 if(N.ToString().StartsWith(TEXT("PinnedClose_"))) {
  const int32 Index=FCString::Atoi(*N.ToString().Mid(12));
  if(PinnedShips.IsValidIndex(Index)){DraggedPin=INDEX_NONE;bPinResizing=false;PinnedShips.RemoveAt(Index);PinnedPreviews[Index]->Release();PinnedPreviews.RemoveAt(Index);PinnedDecks.RemoveAt(Index);PinnedSlots.RemoveAt(Index);PinnedWeapons.RemoveAt(Index);PinnedSlotPages.RemoveAt(Index);PinnedPositions.RemoveAt(Index);PinnedSizes.RemoveAt(Index);}
  return;
 }
 if(N.ToString().StartsWith(TEXT("PinnedWeaponNext_"))) {
  const int32 Index=FCString::Atoi(*N.ToString().Mid(17));
  if(PinnedShips.IsValidIndex(Index)&&PinnedShips[Index].IsValid()&&PinnedDecks[Index]==0) {
   const auto* S=PinnedShips[Index]->Systems.Get();
   if(S&&S->Layout) {const int32 Count=FMath::Min(S->Layout->WeaponMounts.Num(),S->WeaponMountStates.Num());if(Count>0)PinnedWeapons[Index]=(PinnedWeapons[Index]+1)%Count;}
  }
  return;
 }
 if(N.ToString().StartsWith(TEXT("PinnedHangar_"))) {
  const int32 Index=FCString::Atoi(*N.ToString().Mid(13));
  if(PinnedShips.IsValidIndex(Index)&&!GetPinnedCarrierSummary(Index).IsEmpty()) {
   PinnedDecks[Index]=-1;PinnedSlots[Index]=INDEX_NONE;PinnedSlotPages[Index]=0;
  }
  return;
 }
 if(N.ToString().StartsWith(TEXT("PinnedGrip_"))||N.ToString().StartsWith(TEXT("PinnedResize_")))return;
 if(N.ToString().StartsWith(TEXT("PinnedPage_"))) {
  TArray<FString> Parts;N.ToString().ParseIntoArray(Parts,TEXT("_"),true);
  if(Parts.Num()!=3)return;
  const int32 Index=FCString::Atoi(*Parts[1]),Direction=FCString::Atoi(*Parts[2]);
  if(!PinnedShips.IsValidIndex(Index)||!PinnedShips[Index].IsValid()||!PinnedSlotPages.IsValidIndex(Index))return;
  const auto* S=PinnedShips[Index]->Systems.Get();
  if(!S||!S->Layout||S->FactionId!=0||PinnedDecks[Index]<=0)return;
  int32 Count=0;for(int32 I=0;I<S->Layout->Slots.Num()&&I<S->Slots.Num();++I)if(S->Layout->Slots[I].DeckId==PinnedDecks[Index])++Count;
  PinnedSlotPages[Index]=FMath::Clamp(PinnedSlotPages[Index]+FMath::Clamp(Direction,-1,1),0,FMath::Max(0,(Count-1)/15));
  return;
 }
 if(N.ToString().StartsWith(TEXT("PinnedDeck_"))||N.ToString().StartsWith(TEXT("PinnedSlot_"))) {
  TArray<FString> Parts;N.ToString().ParseIntoArray(Parts,TEXT("_"),true);
  if(Parts.Num()!=3)return;
  const int32 Index=FCString::Atoi(*Parts[1]),Value=FCString::Atoi(*Parts[2]);
  if(!PinnedShips.IsValidIndex(Index)||!PinnedShips[Index].IsValid())return;
  const auto* Systems=PinnedShips[Index]->Systems.Get();
  if(!Systems||!Systems->Layout)return;
  if(Parts[0]==TEXT("PinnedDeck")&&Value>=0&&Value<=Systems->Layout->DeckCount&&(Value==0||Systems->FactionId==0)) {
   PinnedDecks[Index]=Value;PinnedSlots[Index]=INDEX_NONE;PinnedSlotPages[Index]=0;if(Value==0)PinnedWeapons[Index]=INDEX_NONE;
  } else if(Parts[0]==TEXT("PinnedSlot")&&Systems->FactionId==0&&Systems->Layout->Slots.IsValidIndex(Value)&&Systems->Layout->Slots[Value].DeckId==PinnedDecks[Index])PinnedSlots[Index]=Value;
  return;
 }
 if(N.ToString().StartsWith(TEXT("Pinned_"))) {
  const int32 Index=FCString::Atoi(*N.ToString().Mid(7));
  if(PinnedShips.IsValidIndex(Index)&&PinnedShips[Index].IsValid()) {
   Cast<AOVController>(PlayerOwner)->SelectShip(PinnedShips[Index].Get()); OpenDetails();
  }
  return;
 }
 if(N==TEXT("WindowClose")) { bDetailsOpen=false;return; }
 if(N==TEXT("WindowMin")) { bMinimized=!bMinimized;return; }
 if(N==TEXT("WindowMax")) { WindowPosition=FVector2D(210,60);WindowSize=WindowSize.X>500?FVector2D(424,550):FVector2D(650,620);return; }
 if(auto* PC=Cast<AOVController>(PlayerOwner)) PC->Action(N);
}
void AOVHUD::DrawExternal(float X,float Y,float W)
{
 auto* PC=Cast<AOVController>(PlayerOwner); if(!PC||!PC->Selected)return;
 if(!ShipPreview)ShipPreview=NewObject<UOVShipPreview>(this);
 ShipPreview->Update(PC->Selected,PreviewRotation,PreviewZoom,W/192.f);
 DrawRect(FLinearColor(.009f,.02f,.035f),X,Y,W,192);
 if(auto* Texture=ShipPreview->GetTexture())DrawTexture(Texture,X,Y,W,192,0,0,1,1,FLinearColor::White,BLEND_Opaque);
 const auto* Systems=PC->Selected->Systems.Get(); const auto* Layout=Systems->Layout.Get();
 for(int32 I=0;I<Layout->WeaponMounts.Num();++I)
 {
  const FVector2D UV=ShipPreview->Project(Layout->WeaponMounts[I].LocalPosition);
  if(UV.X<.03||UV.X>.97||UV.Y<.04||UV.Y>.9)continue;
  const FVector2D P(X+UV.X*W,Y+UV.Y*192);
  const bool Selected=I==PC->SelectedWeapon;
  const FLinearColor Color=Selected?Accent:Health(Systems->WeaponMountStates[I].HP/Layout->WeaponMounts[I].MaxHP);
  DrawLine(P.X-9,P.Y-9,P.X+9,P.Y-9,Color,Selected?2:1);
  DrawLine(P.X-9,P.Y-9,P.X-9,P.Y+9,Color,Selected?2:1);
  DrawLine(P.X+9,P.Y-9,P.X+9,P.Y+9,Color,Selected?2:1);
  const FName Action(*FString::Printf(TEXT("Weapon_%d"),I));
  AddHitBox((P-FVector2D(11,11))*UIScale,FVector2D(22,22)*UIScale,Action,true,130);
  ActionCenters.Add(Action,P);
 }
 if(!PC->Selected->IsHidden()&&!Systems->bWrecked&&(!PC->Combat||!PC->Combat->IsDeparted(PC->Selected))&&Layout->WeaponMounts.IsValidIndex(PC->SelectedWeapon)) {
  if(!WeaponPreview)WeaponPreview=NewObject<UOVShipPreview>(this);
  WeaponPreview->Update(PC->Selected,FRotator(-25,35,0),1.f,1.7f,PC->SelectedWeapon);
  const float PW=FMath::Min(150.f,W*.4f),PH=88.f,PX=X+W-PW-5,PY=Y+5;
  DrawRect(FLinearColor(.009f,.02f,.035f),PX,PY,PW,PH);
  if(auto* Texture=WeaponPreview->GetTexture())DrawTexture(Texture,PX,PY,PW,PH,0,0,1,1,FLinearColor::White,BLEND_Opaque);
  DrawText(TEXT("선택 무장 · 실시간"),Ink,PX+4,PY+4,GEngine->GetSmallFont());
  const float Ratio=GetSelectedWeaponHealthRatio();
  if(Ratio>=0) {
   DrawRect(FLinearColor(.08f,.1f,.12f),PX+5,PY+PH-8,PW-10,4);
   DrawRect(Health(Ratio),PX+5,PY+PH-8,(PW-10)*Ratio,4);
  }
  AddHitBox(FVector2D(PX,PY)*UIScale,FVector2D(PW,PH)*UIScale,TEXT("WeaponPreviewBackground"),true,140);
 }
 DrawText(TEXT("중클릭 회전 · 휠 확대 · 무장 선택"),Ink,X+8,Y+174,GEngine->GetSmallFont());
}
FString AOVHUD::GetSelectedWeaponSummary() const
{
 const auto* PC=Cast<AOVController>(PlayerOwner);
 return PC?GetWeaponSummary(PC->Selected,PC->SelectedWeapon):TEXT("무장을 선택하세요");
}
FString AOVHUD::GetWeaponSummary(AOVShip* Ship,int32 Weapon) const
{
 const auto* PC=Cast<AOVController>(PlayerOwner);
 if(!PC||!IsValid(Ship)||!Ship->Systems||!Ship->Systems->Layout)return TEXT("무장을 선택하세요");
 const auto* Systems=Ship->Systems.Get();const auto* Layout=Systems->Layout.Get();
 if(!Systems->WeaponMountStates.IsValidIndex(Weapon)||!Layout->WeaponMounts.IsValidIndex(Weapon))return TEXT("무장을 선택하세요");
 const auto& HealthState=Systems->WeaponMountStates[Weapon];
 const auto& Def=Layout->WeaponMounts[Weapon];
 FString Kind=Def.Type==EOVWeaponType::Beam?TEXT("빔"):Def.Type==EOVWeaponType::Cannon?TEXT("함포"):Def.Type==EOVWeaponType::Interception?TEXT("요격"):TEXT("미사일");
 // Enemy exterior inspection cannot reveal fire-control readiness, cooldowns or current PD profiles.
 if(Systems->FactionId!=0&&!PC->bDeveloperMode)
  return FString::Printf(TEXT("%s · T%.0f · 외장 관측\nHP %.0f/%.0f · 운영 정보 미확인"),*Kind,Def.Tier,HealthState.HP,Def.MaxHP);
 float HP=HealthState.HP,MaxHP=Def.MaxHP,Range=0;FString Status=TEXT("무장 미등록");
 bool bRangeKnown=false;
 if(Def.Type==EOVWeaponType::Interception)
 {
  if(const auto* PD=GetWorld()->GetSubsystem<UOVPointDefenseSubsystem>())
  {
   const auto ShipState=PD->GetShipState(Ship);
   if(const auto* Mount=ShipState.Mounts.FindByPredicate([&](const FOVPDMountState& M){return M.MountId==HealthState.WeaponMountId;}))
   {
    Kind=Mount->Profile.Kind==EOVPDWeaponKind::Beam?TEXT("빔 요격"):TEXT("실탄 요격");
    HP=Mount->bLayoutMount?FMath::Min(HealthState.HP,Mount->HP):Mount->HP;
    MaxHP=Mount->bLayoutMount?Def.MaxHP:Mount->Profile.MountHP;
    Range=Mount->Profile.Range*ShipState.RangeScale;bRangeKnown=true;
    const FString Ready=PD->GetReadiness(Ship);
    if(HP<=0)Status=TEXT("무장 파괴");
    else if(Ready==TEXT("DEFENDING"))Status=Mount->Cooldown>UE_KINDA_SMALL_NUMBER?FString::Printf(TEXT("재사용 %.1f초"),Mount->Cooldown):TEXT("자동 방어");
    else if(Ready==TEXT("PD CONTROL OFFLINE"))Status=TEXT("요격 제어실 정지");
    else if(Ready==TEXT("SHIP LOST"))Status=TEXT("함선 완파");
    else if(Ready==TEXT("NO OWNER"))Status=TEXT("지휘 불가");
    else if(Ready==TEXT("DEPARTED"))Status=TEXT("이탈");
    else Status=TEXT("요격 대기");
   }
  }
 }
 else if(PC->Combat)
 {
  const auto ShipState=PC->Combat->GetShipState(Ship);
  if(const auto* Mount=ShipState.Mounts.FindByPredicate([&](const FOVCombatMountState& M){return M.MountId==HealthState.WeaponMountId;}))
  {
   Kind=Mount->Profile.Kind==EOVAttackKind::Beam?TEXT("빔"):Mount->Profile.Kind==EOVAttackKind::Cannon?TEXT("함포"):Mount->Profile.Kind==EOVAttackKind::GuidedMissile?TEXT("유도 미사일"):TEXT("일반 미사일");
   Range=Mount->Profile.Range;bRangeKnown=true;
   const auto Ready=PC->Combat->GetReadiness(Ship,HealthState.WeaponMountId);
   Status=Ready.Reason==TEXT("COOLDOWN")?FString::Printf(TEXT("재사용 %.1f초"),Ready.Cooldown):ReadinessKorean(Ready.Reason);
  }
 }
 if(HP<=0)Status=TEXT("무장 파괴");
 const FString RangeText=bRangeKnown?FString::Printf(TEXT("%.0fm"),Range/100.f):TEXT("사거리 미확인");
 return FString::Printf(TEXT("%s · T%.0f · %s\nHP %.0f/%.0f · %s"),*Kind,Def.Tier,*RangeText,HP,MaxHP,*Status);
}
void AOVHUD::DrawDetails()
{
 auto* PC=Cast<AOVController>(PlayerOwner);if(!PC->Selected||!bDetailsOpen)return;
 if(!bDetailsPositioned){WindowPosition.X=ViewportPixels.X/UIScale-WindowSize.X-16;bDetailsPositioned=true;}
 WindowSize.Y=FMath::Clamp(WindowSize.Y,420.0,FMath::Max(420.0,ViewportPixels.Y/UIScale-230));
 WindowPosition.X=FMath::Clamp(WindowPosition.X,0.0,FMath::Max(0.0,ViewportPixels.X/UIScale-WindowSize.X-12));
 WindowPosition.Y=FMath::Clamp(WindowPosition.Y,68.0,FMath::Max(68.0,ViewportPixels.Y/UIScale-WindowSize.Y-168));
 const auto* S=PC->Selected->Systems.Get();const auto* L=S->Layout.Get();if(!L)return;
 TGuardValue<bool> DetailLayer(bDrawingDetails,true);
 const float X=WindowPosition.X,Y=WindowPosition.Y,W=WindowSize.X;
 // The foreground window consumes blank-area clicks too, including over fire buttons.
 AddHitBox(WindowPosition*UIScale,FVector2D(W,bMinimized?32:WindowSize.Y)*UIScale,TEXT("WindowBackground"),true,100);
 DrawRect(Panel,X,Y,W,bMinimized?32:WindowSize.Y);
 DrawText(PC->Selected->ShipName,Accent,X+10,Y+8,GEngine->GetSmallFont());
 Button(TEXT("WindowMin"),bMinimized?TEXT("+"):TEXT("_"),X+W-90,Y+2,26);Button(TEXT("WindowMax"),TEXT("[]"),X+W-60,Y+2,26);Button(TEXT("WindowClose"),TEXT("X"),X+W-30,Y+2,26);
 if(bMinimized)return;
 Button(TEXT("External"),TEXT("외부 / 무장"),X+10,Y+38,100,PC->ViewedDeck==0&&!bHangarView);
 Button(TEXT("HangarView"),TEXT("격납고"),X+116,Y+38,74,bHangarView);
 Button(TEXT("WindowPin"),TEXT("핀"),X+W-128,Y+2,30,PinnedShips.Contains(PC->Selected));
 const bool Reveal=PC->Selected->bFriendly||PC->bDeveloperMode;
 for(int32 D=1;D<=L->DeckCount;++D) if(Reveal)Button(FName(*FString::Printf(TEXT("Deck%d"),D)),FString::Printf(TEXT("%dF"),D),X+198+(D-1)*46,Y+38,40,PC->ViewedDeck==D);
 if(bHangarView) {
  if(!Reveal) DrawText(TEXT("적 격납고 · 미확인"),Ink,X+16,Y+88,GEngine->GetSmallFont());
  else if(auto* Carrier=GetWorld()->GetSubsystem<UOVCarrierSubsystem>()) {
   if(AircraftPageShip!=PC->Selected){AircraftPageShip=PC->Selected;AircraftPage=0;AircraftFilter=0;SelectedAircraftId=0;}
   const int32 Row=Carrier->Hangars.FilterByPredicate([&](const FOVHangarState& H){return H.Carrier==PC->Selected;}).Num();
   if(Row==0)DrawText(TEXT("장착된 격납고 없음"),Ink,X+16,Y+88,GEngine->GetSmallFont());
   else if(!bAircraftGrid) {
    Button(TEXT("HangarDisplayMode"),TEXT("기체 보기"),X+W-105,Y+72,90);
    int32 I=0;
    for(const auto& Hangar:Carrier->Hangars)if(Hangar.Carrier==PC->Selected) {
     const int32 Stored=Carrier->Aircraft.FilterByPredicate([&](const auto& A){return A.HangarId==Hangar.HangarId&&A.State==EOVAircraftState::Stored;}).Num();
     const auto Availability=Carrier->GetHangarAvailability(Hangar.HangarId);
     const TCHAR* FacilityState=Availability==EOVHangarAvailability::Ready?TEXT("가동"):Availability==EOVHangarAvailability::Destroyed?TEXT("파괴"):Availability==EOVHangarAvailability::ShipLost?TEXT("함선 완파"):Availability==EOVHangarAvailability::Offline?TEXT("정지"):TEXT("미확인");
     FString Contents;
     if(Hangar.Mode==EOVHangarMode::Aircraft) Contents=FString::Printf(TEXT("함재기 %d/%d"),Stored,20+Hangar.ExtraAircraftCapacity);
     else if(const AOVShip* Small=Hangar.StoredSmallShip.Get();Small&&Small->Systems)
      Contents=FString::Printf(TEXT("%s · 선체 %.0f%% · 출격 미지원"),*Small->ShipName.Left(8),100.f*Small->Systems->HullHP/FMath::Max(1.f,Small->Systems->MaxHullHP));
     else Contents=TEXT("소형함 보관 미지원");
     DrawText(FString::Printf(TEXT("격납고 %d · %s · %s"),++I,FacilityState,*Contents),Ink,X+16,Y+105+(I-1)*19,GEngine->GetSmallFont());
    }
    Button(TEXT("HangarLaunchAircraft"),TEXT("출격"),X+W-180,Y+250,78);
    Button(TEXT("HangarRecallAircraft"),TEXT("귀환"),X+W-96,Y+250,78);
   } else {
    const auto Overview=Carrier->GetAircraftOverview(PC->Selected);
    DrawText(FString::Printf(TEXT("격납고 %d · 기체 생존 %d · 손실 %d"),Row,Overview.Surviving,Overview.Wrecked),Ink,X+16,Y+80,GEngine->GetSmallFont());
    Button(TEXT("HangarDisplayMode"),TEXT("시설 보기"),X+W-105,Y+72,90);
    const TCHAR* Filters[]={TEXT("전체"),TEXT("생존"),TEXT("손상"),TEXT("손실")};
    Button(TEXT("AircraftFilter"),Filters[AircraftFilter],X+W-175,Y+72,65);
    TArray<const FOVAircraftRecord*> Records;
    for(const auto& A:Carrier->Aircraft)if(A.Carrier==PC->Selected&&A.FactionId==S->FactionId) {
     const bool Alive=A.State!=EOVAircraftState::Wrecked;
     if(AircraftFilter==1&&!Alive)continue;
     if(AircraftFilter==2&&(!Alive||(A.HP>=Carrier->Profile.Hull&&A.Shield>=Carrier->Profile.Shield)))continue;
     if(AircraftFilter==3&&Alive)continue;
     Records.Add(&A);
    }
    const int32 Pages=FMath::Max(1,FMath::DivideAndRoundUp(Records.Num(),15));
    AircraftPage=FMath::Clamp(AircraftPage,0,Pages-1);
    const float CW=(W-38)/5;
    for(int32 I=0;I<15&&AircraftPage*15+I<Records.Num();++I) {
     const auto& A=*Records[AircraftPage*15+I];const float CX=X+12+(I%5)*(CW+3),CY=Y+105+(I/5)*37;
     const TCHAR* ShortStates[]={TEXT("보관"),TEXT("출격"),TEXT("귀환"),TEXT("대기"),TEXT("상실"),TEXT("완파")};
     Button(FName(*FString::Printf(TEXT("AircraftCard_%d"),A.AircraftId)),FString::Printf(TEXT("%d %s"),AircraftPage*15+I+1,ShortStates[FMath::Clamp(int32(A.State),0,5)]),CX,CY,CW,A.AircraftId==SelectedAircraftId);
     const float Ratio=FMath::Clamp(A.HP/FMath::Max(1.f,Carrier->Profile.Hull),0.f,1.f);
     const FLinearColor Icon=A.State==EOVAircraftState::Wrecked?FLinearColor::Gray:
      A.State==EOVAircraftState::ControlLost?FLinearColor(1,.55f,.2f):Accent;
     // Small schematic fighter silhouette; it is not a final aircraft asset.
     DrawLine(CX+CW-14,CY+6,CX+CW-14,CY+21,Icon,2);
     DrawLine(CX+CW-14,CY+12,CX+CW-20,CY+18,Icon,2);
     DrawLine(CX+CW-14,CY+12,CX+CW-8,CY+18,Icon,2);
     DrawRect(FLinearColor(.12f,.14f,.17f),CX,CY+28,CW,3);
     DrawRect(Health(A.State==EOVAircraftState::Wrecked?0:Ratio),CX,CY+28,CW*Ratio,3);
    }
    Button(TEXT("AircraftPrevious"),TEXT("이전"),X+12,Y+220,62);
    DrawText(FString::Printf(TEXT("%d / %d"),AircraftPage+1,Pages),Ink,X+80,Y+226,GEngine->GetSmallFont());
    Button(TEXT("AircraftNext"),TEXT("다음"),X+125,Y+220,62);
    Button(TEXT("HangarLaunchAircraft"),TEXT("출격"),X+W-180,Y+220,78);
    Button(TEXT("HangarRecallAircraft"),TEXT("귀환"),X+W-96,Y+220,78);
    if(const auto* A=Carrier->Aircraft.FindByPredicate([&](const FOVAircraftRecord& Item){return Item.AircraftId==SelectedAircraftId&&Item.Carrier==PC->Selected&&Item.FactionId==S->FactionId;})) {
     const TCHAR* Labels[]={TEXT("보관"),TEXT("출격"),TEXT("귀환"),TEXT("도킹 대기"),TEXT("관제 상실"),TEXT("완파")};
     DrawText(FString::Printf(TEXT("선택 기체 · %s · 내구 %.0f"),Labels[FMath::Clamp(int32(A->State),0,5)],A->HP),Ink,X+16,Y+258,GEngine->GetSmallFont());
     if(PC->Selected->bFriendly&&A->State!=EOVAircraftState::Wrecked)Button(TEXT("AircraftLocate"),TEXT("위치 찾기"),X+W-105,Y+250,90);
    }
   }
  }
 }
 else if(PC->ViewedDeck==0)DrawExternal(X+10,Y+72,W-20);
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
 DrawText(FString::Printf(TEXT("선체 %.0f/%.0f | 쉴드 %.0f/%.0f %s"),S->HullHP,S->MaxHullHP,S->Shield,S->MaxShield,S->bShieldActive?TEXT("활성"):TEXT("비활성")),Ink,X+10,Y+280,GEngine->GetSmallFont());
 if(S->ArmorSections.IsValidIndex(PC->SelectedArmor)) {
  const auto& A=S->ArmorSections[PC->SelectedArmor];
  DrawText(FString::Printf(TEXT("장갑 %s %.0f/%.0f"),*(PC->bDeveloperMode?A.ArmorSectionId.ToString():FString(TEXT("선택 구역"))),A.HP,L->ArmorSections[PC->SelectedArmor].MaxArmor),Accent,X+10,Y+301,GEngine->GetSmallFont());
 }
 if(!bHangarView&&PC->ViewedDeck==0&&S->WeaponMountStates.IsValidIndex(PC->SelectedWeapon)) {
  TArray<FString> Lines;GetSelectedWeaponSummary().ParseIntoArrayLines(Lines);
  for(int32 I=0;I<FMath::Min(2,Lines.Num());++I) {
   float TextWidth=0,TextHeight=0;GetTextSize(Lines[I],TextWidth,TextHeight,GEngine->GetSmallFont());
   const float TextScale=FMath::Min(1.f,(W-20)/FMath::Max(1.f,TextWidth));
   DrawText(Lines[I],Ink,X+10,Y+322+I*19,GEngine->GetSmallFont(),TextScale);
  }
 }else if(Reveal&&L->Slots.IsValidIndex(PC->SelectedSlot)) {
  const auto& F=S->Slots[PC->SelectedSlot];const auto& D=L->Slots[PC->SelectedSlot];
  DrawText(FString::Printf(TEXT("%s %s 내구 %.0f/%.0f 인원 %.0f/%.0f"),*(PC->bDeveloperMode?F.SlotId.ToString():FString(TEXT("시설"))),*Facility(D.Facility),F.HP,D.MaxFacilityHP,F.Crew,D.RequiredCrew),Ink,X+10,Y+322,GEngine->GetSmallFont());
  if(PC->bDeveloperMode)DrawText(S->GetSlotStatus(F.SlotId).Left(45),Accent,X+10,Y+341,GEngine->GetSmallFont());
  else Button(TEXT("DetailsInteriorToggle"),TEXT("선내 작전 열기"),X+10,Y+390,150);
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
 if(WeaponPreview&&(!HasOrbitPreview()||!IsValid(PC->Selected)||PC->Selected->IsHidden()||PC->Selected->Systems->bWrecked||(PC->Combat&&PC->Combat->IsDeparted(PC->Selected))))WeaponPreview->Release();
 ViewportPixels=FVector2D(Canvas->SizeX,Canvas->SizeY);
 UIScale=FMath::Max(.25f,FMath::Min(1.f,FMath::Min(Canvas->SizeX/1280.f,Canvas->SizeY/740.f)));
 Canvas->Canvas->PushRelativeTransform(FScaleMatrix(FVector(UIScale,UIScale,1)));
 DrawPlayHUD(); DrawWorldIndicators(); DrawPhase2C(); DrawMission();
 DrawBattleNotices();
 DrawPinned();
 if(!PC->bDeveloperMode) { DrawDetails(); DrawInterior(); }
 float MouseX=0,MouseY=0;
 if(DraggedPin==INDEX_NONE && PC->GetMousePosition(MouseX,MouseY) && !IsOverAddedUI(MouseX,MouseY)) {
  const FVector2D Mouse(MouseX/UIScale,MouseY/UIScale);
  FString Name=GetPinnedTitleTooltip(Mouse);
  if(Name.IsEmpty())Name=GetFleetTitleTooltip(Mouse);
  if(!Name.IsEmpty()) {
   TArray<FString> Lines;
   for(int32 Start=0;Start<Name.Len();Start+=24)Lines.Add(Name.Mid(Start,24));
   const float Width=FMath::Min(300.f,ViewportPixels.X/UIScale-8.f);
   const float Height=FMath::Min(float(Lines.Num()*19+10),ViewportPixels.Y/UIScale-8.f);
   const float X=FMath::Clamp(float(Mouse.X+12),4.f,FMath::Max(4.f,float(ViewportPixels.X/UIScale-Width-4)));
   const float Y=FMath::Clamp(float(Mouse.Y+16),4.f,FMath::Max(4.f,float(ViewportPixels.Y/UIScale-Height-4)));
   DrawRect(Panel,X,Y,Width,Height);
   for(int32 I=0;I<Lines.Num() && 5+I*19+16<=Height;++I) {
    float TW=0,TH=0;GetTextSize(Lines[I],TW,TH,GEngine->GetSmallFont());
    DrawText(Lines[I],Ink,X+6,Y+5+I*19,GEngine->GetSmallFont(),FMath::Min(1.f,(Width-12)/FMath::Max(1.f,TW)));
   }
  }
 }
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
void AOVHUD::EndPlay(const EEndPlayReason::Type Reason)
{
 if(ShipPreview)ShipPreview->Release();
 if(WeaponPreview)WeaponPreview->Release();
 for(auto Preview:PinnedPreviews)if(Preview)Preview->Release();
 Super::EndPlay(Reason);
}
int32 AOVHUD::GetPreviewMeshCount() const
{
 return ShipPreview?ShipPreview->GetMeshCount():0;
}
int32 AOVHUD::GetWeaponPreviewVisibleMeshCount() const { return WeaponPreview?WeaponPreview->GetVisibleMeshCount():0; }
int32 AOVHUD::GetWeaponPreviewFocus() const { return WeaponPreview?WeaponPreview->GetFocusMount():INDEX_NONE; }
FString AOVHUD::GetPinnedWeaponSummary(int32 Index) const
{
 return PinnedShips.IsValidIndex(Index)&&PinnedWeapons.IsValidIndex(Index)?GetWeaponSummary(PinnedShips[Index].Get(),PinnedWeapons[Index]):TEXT("무장을 선택하세요");
}
FString AOVHUD::GetPinnedCarrierSummary(int32 Index) const
{
 if(!PinnedShips.IsValidIndex(Index)||!PinnedShips[Index].IsValid())return FString();
 auto* Ship=PinnedShips[Index].Get();
 if(!Ship->Systems||Ship->Systems->FactionId!=0||!GetWorld())return FString();
 const auto* Carrier=GetWorld()->GetSubsystem<UOVCarrierSubsystem>();if(!Carrier)return FString();
 int32 Total=0,Ready=0;
 for(const auto& Hangar:Carrier->Hangars)if(Hangar.Carrier==Ship) {
  ++Total;if(Carrier->GetHangarAvailability(Hangar.HangarId)==EOVHangarAvailability::Ready)++Ready;
 }
 if(Total==0)return FString();
 const auto Air=Carrier->GetAircraftOverview(Ship);
 return FString::Printf(TEXT("격납고 %d · 가동 %d\n생존 %d · 손실 %d\n보관 %d · 출격 %d\n귀환 %d · 관제 상실 %d"),
  Total,Ready,Air.Surviving,Air.Wrecked,Air.Stored,Air.Sortie,Air.Returning,Air.ControlLost);
}
int32 AOVHUD::GetPinnedPreviewFocus(int32 Index) const
{
 return PinnedPreviews.IsValidIndex(Index)&&PinnedPreviews[Index]?PinnedPreviews[Index]->GetFocusMount():INDEX_NONE;
}
float AOVHUD::GetSelectedWeaponHealthRatio() const
{
 const auto* PC=Cast<AOVController>(PlayerOwner);
 if(!PC||!PC->Selected||!PC->Selected->Systems||!PC->Selected->Systems->Layout)return -1;
 const auto* S=PC->Selected->Systems.Get();const auto* L=S->Layout.Get();const int32 I=PC->SelectedWeapon;
 if(!S->WeaponMountStates.IsValidIndex(I)||!L->WeaponMounts.IsValidIndex(I)||L->WeaponMounts[I].MaxHP<=0)return -1;
 float HP=S->WeaponMountStates[I].HP,MaxHP=L->WeaponMounts[I].MaxHP;
 if(L->WeaponMounts[I].Type==EOVWeaponType::Interception&&(S->FactionId==0||PC->bDeveloperMode))
  if(const auto* PD=GetWorld()->GetSubsystem<UOVPointDefenseSubsystem>()) {
   const auto State=PD->GetShipState(PC->Selected);
   if(const auto* M=State.Mounts.FindByPredicate([&](const FOVPDMountState& P){return P.MountId==S->WeaponMountStates[I].WeaponMountId;})) {
    HP=M->bLayoutMount?FMath::Min(HP,M->HP):M->HP;MaxHP=M->bLayoutMount?MaxHP:M->Profile.MountHP;
   }
  }
 return MaxHP>0?FMath::Clamp(HP/MaxHP,0.f,1.f):-1.f;
}
float AOVHUD::PinnedPanelX() const
{
 const float Width=ViewportPixels.X/UIScale;
 const auto* PC=Cast<AOVController>(PlayerOwner);
 if(PC&&PC->Selected&&bDetailsOpen) {
  const float Left=WindowPosition.X-278;
  if(Left>=206)return Left;
  const float Right=WindowPosition.X+WindowSize.X+6;
  if(Right+266<=Width-12)return Right;
 }
 return FMath::Max(206.f,Width-278.f);
}
float AOVHUD::PinnedPanelY() const
{
 const int32 Count=BattleNoticeRows();
 const float NoticeX=ViewportPixels.X/UIScale*.5f-120;
 if(Count<=0||PinnedPanelX()+266<=NoticeX||PinnedPanelX()>=NoticeX+240)return 76;
 const auto* Feed=GetBattleNoticeFeed();
 const int32 Footer=bBattleFeedOpen?66+(Feed&&Feed->DroppedCount>0?20:0):0;
 return FMath::Max(76.f,66.f+Count*32+Footer+4.f);
}
FVector2D AOVHUD::GetPinnedPosition(int32 Index) const
{
 if(!PinnedPositions.IsValidIndex(Index))return FVector2D(-1,-1);
 const FVector2D Default(PinnedPanelX(),PinnedPanelY()+Index*160);
 const FVector2D Value=PinnedPositions[Index].X<0?Default:PinnedPositions[Index];
 const float Width=ViewportPixels.X/UIScale,Height=ViewportPixels.Y/UIScale;
 const FVector2D Size=GetPinnedSize(Index);
 return FVector2D(FMath::Clamp(Value.X,0.,FMath::Max(0.,double(Width-Size.X))),
     FMath::Clamp(Value.Y,56.,FMath::Max(56.,double(Height-Size.Y))));
}
FVector2D AOVHUD::GetPinnedSize(int32 Index) const
{
 if(!PinnedSizes.IsValidIndex(Index))return FVector2D::ZeroVector;
 const float Width=ViewportPixels.X/UIScale,Height=ViewportPixels.Y/UIScale;
 return FVector2D(FMath::Clamp(PinnedSizes[Index].X,266.,FMath::Max(266.,FMath::Min(550.,double(Width)))),
     FMath::Clamp(PinnedSizes[Index].Y,158.,FMath::Max(158.,FMath::Min(400.,double(Height-56)))));
}
FString AOVHUD::GetPinnedTitleTooltip(FVector2D CanvasPoint) const
{
 const FString Action=GetHitActionAtCanvasPoint(CanvasPoint).ToString();
 for(int32 I=0;I<PinnedShips.Num();++I)if(PinnedShips[I].IsValid()&&Action==FString::Printf(TEXT("Pinned_%d"),I)) {
  const int32 Limit=FMath::Clamp(FMath::FloorToInt((GetPinnedSize(I).X-180)/12.f),2,20);
  const FString& Name=PinnedShips[I]->ShipName;
  return Name.Len()>Limit?Name:FString();
 }
 return FString();
}
void AOVHUD::DrawPinned()
{
 const auto* PC=Cast<AOVController>(PlayerOwner);
 for(int32 I=PinnedShips.Num()-1;I>=0;--I)if(!PinnedShips[I].IsValid()||PinnedShips[I]->IsHidden()||(PC&&PC->Combat&&PC->Combat->IsDeparted(PinnedShips[I].Get())))
 {DraggedPin=INDEX_NONE;bPinResizing=false;PinnedPreviews[I]->Release();PinnedPreviews.RemoveAt(I);PinnedShips.RemoveAt(I);PinnedDecks.RemoveAt(I);PinnedSlots.RemoveAt(I);PinnedWeapons.RemoveAt(I);PinnedSlotPages.RemoveAt(I);PinnedPositions.RemoveAt(I);PinnedSizes.RemoveAt(I);}
 if(!PC||PC->bDeveloperMode||bHeroPanelOpen)return;
 for(int32 I=0;I<PinnedShips.Num();++I)
 {
  const FVector2D Pos=GetPinnedPosition(I),Size=GetPinnedSize(I);const float X=Pos.X,Y=Pos.Y,W=Size.X,H=Size.Y;auto* Ship=PinnedShips[I].Get();
  DrawRect(Panel,X,Y,W,H);
  AddHitBox(FVector2D(X,Y)*UIScale,Size*UIScale,FName(*FString::Printf(TEXT("PinnedBackground_%d"),I)),true,5);
  Button(FName(*FString::Printf(TEXT("PinnedGrip_%d"),I)),TEXT("이동"),X+4,Y+4,38);
  const int32 TitleLimit=FMath::Clamp(FMath::FloorToInt((W-180)/12.f),2,20);
  const FString Title=Ship->ShipName.Len()>TitleLimit?Ship->ShipName.Left(TitleLimit-1)+TEXT("…"):Ship->ShipName;
  Button(FName(*FString::Printf(TEXT("Pinned_%d"),I)),Title,X+46,Y+4,W-158);
  Button(FName(*FString::Printf(TEXT("PinnedClose_%d"),I)),TEXT("X"),X+W-38,Y+4,32);
  const auto* Systems=Ship->Systems.Get();const auto* Layout=Systems?Systems->Layout.Get():nullptr;
  if(!Layout)continue;
  if(!Layout->WeaponMounts.IsValidIndex(PinnedWeapons[I])||!Systems->WeaponMountStates.IsValidIndex(PinnedWeapons[I]))PinnedWeapons[I]=INDEX_NONE;
  const auto* Carrier=GetWorld()->GetSubsystem<UOVCarrierSubsystem>();
  const bool HasHangar=Systems->FactionId==0&&Carrier&&Carrier->Hangars.ContainsByPredicate([&](const FOVHangarState& Hangar){return Hangar.Carrier==Ship;});
  Button(FName(*FString::Printf(TEXT("PinnedDeck_%d_0"),I)),TEXT("외부"),X+6,Y+35,55,PinnedDecks[I]==0);
  if(Systems->FactionId==0)for(int32 D=1;D<=Layout->DeckCount;++D)
   Button(FName(*FString::Printf(TEXT("PinnedDeck_%d_%d"),I,D)),FString::Printf(TEXT("%dF"),D),X+64+(D-1)*45,Y+35,40,PinnedDecks[I]==D);
  if(HasHangar)Button(FName(*FString::Printf(TEXT("PinnedHangar_%d"),I)),TEXT("항공대"),X+W-62,Y+35,56,PinnedDecks[I]==-1);
  if(PinnedDecks[I]==0) {
   auto* Preview=PinnedPreviews[I].Get();
   if(Systems->bWrecked)Preview->Release();
   else Preview->Update(Ship,FRotator(-25,35,0),1,(W-12)/FMath::Max(1.f,H-83),PinnedWeapons[I]);
   if(auto* Texture=Preview->GetTexture())DrawTexture(Texture,X+6,Y+64,W-12,H-83,0,0,1,1,FLinearColor::White,BLEND_Opaque);
   else if(Systems->bWrecked)DrawText(TEXT("함선 완파"),Ink,X+12,Y+72,GEngine->GetSmallFont());
   if(Layout->WeaponMounts.Num()>0)Button(FName(*FString::Printf(TEXT("PinnedWeaponNext_%d"),I)),TEXT("무장 +"),X+W-62,Y+(HasHangar?65:35),56,PinnedWeapons[I]>=0);
   if(PinnedWeapons[I]>=0) {
    TArray<FString> Lines;GetWeaponSummary(Ship,PinnedWeapons[I]).ParseIntoArrayLines(Lines);
    if(Lines.Num()>0)DrawText(Lines[0],Ink,X+10,Y+65,GEngine->GetSmallFont(),.9f);
    if(Lines.Num()>1)DrawText(Lines[1],Ink,X+10,Y+H-14,GEngine->GetSmallFont(),.9f);
   } else DrawText(FString::Printf(TEXT("선체 %.0f%% · 쉴드 %.0f%%"),100*Systems->HullHP/FMath::Max(1.f,Systems->MaxHullHP),100*Systems->Shield/FMath::Max(1.f,Systems->MaxShield)),Ink,X+10,Y+H-14,GEngine->GetSmallFont());
  } else if(PinnedDecks[I]==-1&&Systems->FactionId==0) {
   if(PinnedPreviews[I])PinnedPreviews[I]->Release();
   TArray<FString> Lines;GetPinnedCarrierSummary(I).ParseIntoArrayLines(Lines);
   for(int32 Line=0;Line<Lines.Num();++Line)DrawText(Lines[Line],Ink,X+10,Y+65+Line*19,GEngine->GetSmallFont());
  } else if(Systems->FactionId==0) {
   if(PinnedPreviews[I])PinnedPreviews[I]->Release();
   int32 DeckSlots=0;for(int32 S=0;S<Layout->Slots.Num()&&S<Systems->Slots.Num();++S)if(Layout->Slots[S].DeckId==PinnedDecks[I])++DeckSlots;
   PinnedSlotPages[I]=FMath::Clamp(PinnedSlotPages[I],0,FMath::Max(0,(DeckSlots-1)/15));
   if(DeckSlots>15) {
    Button(FName(*FString::Printf(TEXT("PinnedPage_%d_-1"),I)),TEXT("<"),X+W-108,Y+4,26);
    Button(FName(*FString::Printf(TEXT("PinnedPage_%d_1"),I)),TEXT(">"),X+W-68,Y+4,26);
    DrawText(FString::FromInt(PinnedSlotPages[I]+1),Ink,X+W-80,Y+9,GEngine->GetSmallFont(),.8f);
   }
   int32 Cell=0,Seen=0;
   for(int32 S=0;S<Layout->Slots.Num()&&S<Systems->Slots.Num()&&Cell<15;++S)if(Layout->Slots[S].DeckId==PinnedDecks[I]) {
    if(Seen++<PinnedSlotPages[I]*15)continue;
    const auto& Def=Layout->Slots[S];const auto& Slot=Systems->Slots[S];
    const float CW=(W-14)/5,CH=(H-83)/3;
    const float CX=X+7+(Cell%5)*CW,CY=Y+65+(Cell/5)*CH;++Cell;
    const FName Action(*FString::Printf(TEXT("PinnedSlot_%d_%d"),I,S));
    DrawRect(S==PinnedSlots[I]?Accent:FLinearColor(.04f,.08f,.13f),CX,CY,CW-3,CH-4);
    DrawText(Def.Facility==EOVFacilityType::None?TEXT("-"):Facility(Def.Facility).Left(2),Ink,CX+3,CY+1,GEngine->GetSmallFont());
    DrawRect(Health(Def.MaxFacilityHP>0?Slot.HP/Def.MaxFacilityHP:0),CX,CY+CH-7,CW-3,3);
    AddHitBox(FVector2D(CX,CY)*UIScale,FVector2D(CW-3,CH-4)*UIScale,Action,true,10);ActionCenters.Add(Action,FVector2D(CX+(CW-3)*.5f,CY+(CH-4)*.5f));
   }
   const int32 S=PinnedSlots[I];
   if(Layout->Slots.IsValidIndex(S)&&Systems->Slots.IsValidIndex(S))DrawText(FString::Printf(TEXT("%s · 내구 %.0f/%.0f"),*Facility(Layout->Slots[S].Facility),Systems->Slots[S].HP,Layout->Slots[S].MaxFacilityHP),Ink,X+10,Y+H-14,GEngine->GetSmallFont());
   else DrawText(TEXT("시설을 선택하세요"),Ink,X+10,Y+H-14,GEngine->GetSmallFont());
  }
  DrawLine(X+W-14,Y+H-2,X+W-2,Y+H-14,Accent,2);
  AddHitBox(FVector2D(X+W-18,Y+H-18)*UIScale,FVector2D(18,18)*UIScale,FName(*FString::Printf(TEXT("PinnedResize_%d"),I)),true,20);
 }
}
void AOVHUD::ToggleDevelopmentMinimized(){bDevelopmentMinimized=!bDevelopmentMinimized;CancelPointerDrag();SyncAddedUI();}
void AOVHUD::ToggleDevelopmentMaximized()
{
 if(bDevelopmentMaximized){DevelopmentPosition=DevelopmentRestorePosition;DevelopmentSize=DevelopmentRestoreSize;bDevelopmentMaximized=false;}
 else {DevelopmentRestorePosition=DevelopmentPosition;DevelopmentRestoreSize=DevelopmentSize;DevelopmentPosition=FVector2D(8,8);DevelopmentSize=ViewportPixels-FVector2D(16,16);bDevelopmentMaximized=true;}
 bDevelopmentMinimized=false;CancelPointerDrag();SyncAddedUI();
}
void AOVHUD::ResetDevelopmentWindow(){DevelopmentPosition=FVector2D(-1,-1);DevelopmentSize=FVector2D(-1,-1);bDevelopmentMinimized=false;bDevelopmentMaximized=false;CancelPointerDrag();SyncAddedUI();}
void AOVHUD::SyncAddedUI()
{
 auto* PC=Cast<AOVController>(PlayerOwner);if(!PC)return;int32 W=0,H=0;PC->GetViewportSize(W,H);if(W<=0||H<=0)return;
 const float Dpi=FMath::Max(.01f,UWidgetLayoutLibrary::GetViewportScale(this));
 if(DevelopmentSize.X<0){DevelopmentSize=FVector2D(FMath::Min(760.f,W*.8f),FMath::Min(530.f,H*.78f));DevelopmentPosition=FVector2D((W-DevelopmentSize.X)*.5f,(H-DevelopmentSize.Y)*.5f);}
 DevelopmentSize.X=FMath::Clamp(DevelopmentSize.X,FMath::Min(480.,double(W-16)),FMath::Max(1.,double(W-16)));
 DevelopmentSize.Y=FMath::Clamp(DevelopmentSize.Y,FMath::Min(360.,double(H-16)),FMath::Max(1.,double(H-16)));
 const FVector2D DevSize(DevelopmentSize.X,bDevelopmentMinimized?FMath::Min(50.*Dpi,double(H-16)):DevelopmentSize.Y);
 DevelopmentPosition.X=FMath::Clamp(DevelopmentPosition.X,0.,FMath::Max(0.,W-DevSize.X));
 DevelopmentPosition.Y=FMath::Clamp(DevelopmentPosition.Y,0.,FMath::Max(0.,H-DevSize.Y));
 const FVector2D DevPos=DevelopmentPosition;DevelopmentRect=FBox2D(DevPos,DevPos+DevSize);
 const FVector2D HeroSize(FMath::Min(400.f,W*.34f),FMath::Max(260.f,H-240.f));
 const FVector2D HeroPos(W-HeroSize.X-12.f,76.f);HeroRect=FBox2D(HeroPos,HeroPos+HeroSize);
 if(DevelopmentWidget){DevelopmentWidget->SetFrameState(bDevelopmentMinimized,bDevelopmentMaximized);DevelopmentWidget->SetPositionInViewport(DevPos/Dpi,false);DevelopmentWidget->SetDesiredSizeInViewport(DevSize/Dpi);DevelopmentWidget->SetVisibility(PC->bDeveloperMode?ESlateVisibility::Visible:ESlateVisibility::Collapsed);}
 if(HeroWidget){HeroWidget->SetPositionInViewport(HeroPos/Dpi,false);HeroWidget->SetDesiredSizeInViewport(HeroSize/Dpi);HeroWidget->SetVisibility(PC->bDeveloperMode||!bHeroPanelOpen?ESlateVisibility::Collapsed:ESlateVisibility::Visible);}
}
bool AOVHUD::IsOverAddedUI(float X,float Y) const
{
 const auto* PC=Cast<AOVController>(PlayerOwner);if(!PC)return false;const FVector2D P(X,Y);
 return PC->bDeveloperMode?DevelopmentWidget&&DevelopmentRect.IsInsideOrOn(P):bHeroPanelOpen&&HeroWidget&&HeroRect.IsInsideOrOn(P);
}
bool AOVHUD::IsAddedUIEditing() const
{
 const auto* PC=Cast<AOVController>(PlayerOwner);return PC&&PC->bDeveloperMode&&DevelopmentWidget&&DevelopmentWidget->IsEditing();
}
