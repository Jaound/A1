#include "OVHUD.h"
#include "OVBasePresentation.h"
#include "Misc/ConfigCacheIni.h"
#include "UnrealClient.h"
#include "Camera/CameraActor.h"
#include "OVDevelopmentWidget.h"
#include "OVHeroWidget.h"
#include "OVTitleScreenWidget.h"
#include "OVShipPreview.h"
#include "OVCarrierSubsystem.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "OVController.h"
#include "OVPlayScenarioSubsystem.h"
#include "OVShip.h"
#include "OVShipSystemsComponent.h"
#include "OVCombatSubsystem.h"
#include "OVBattleNotices.h"
#include "OVPointDefenseSubsystem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "CanvasItem.h"
#include "Engine/Font.h"
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
void AOVHUD::DrawText(const FString& Text, FLinearColor Color, float X, float Y, UFont* Font, float Scale)
{
 if (!Canvas || Text.IsEmpty() || Scale <= 0.f) return;
 if (!Font || Font->FontCacheType != EFontCacheType::Runtime) {
  AHUD::DrawText(Text,Color,X,Y,Font,Scale);
  return;
 }
 auto Info=Font->GetLegacySlateFontInfo();
 const float LogicalSize=Info.Size*Scale;
 if (LogicalSize <= 0.f) return;
 // Canvas scales the geometry after looking up glyphs. Choose the atlas size
 // from the final pixel footprint and only compensate for rounding in layout.
 Info.Size=FMath::Max(1,FMath::RoundToInt(LogicalSize*UIScale));
 const FVector2D Origin(FMath::RoundToFloat(X*UIScale)/UIScale,FMath::RoundToFloat(Y*UIScale)/UIScale);
 FCanvasTextItem Item(Origin,FText::FromString(Text),Info,Color);
 Item.Scale=FVector2D(LogicalSize/Info.Size);
 Canvas->DrawItem(Item);
}

FString AOVHUD::FitLabel(UFont* Font,const FString& Text,float MaxWidth) const
{
 float W=0,H=0;GetTextSize(Text,W,H,Font);
 if(W<=MaxWidth)return Text;
 int32 Low=0,High=Text.Len();
 while(Low<High) {
  const int32 Mid=(Low+High+1)/2;GetTextSize(Text.Left(Mid)+TEXT("…"),W,H,Font);
  if(W<=MaxWidth)Low=Mid;else High=Mid-1;
 }
 return Text.Left(Low)+TEXT("…");
}

void AOVHUD::DrawNumberRight(const FString& Number,FLinearColor Color,float Right,float Top,float Scale)
{
 UFont* Font=OVTypography::Font(EOVTextRole::Numeric);
 float Width=0,Height=0;
 GetTextSize(Number,Width,Height,Font,Scale);
 DrawText(Number,Color,Right-Width,Top,Font,Scale);
}

FLinearColor AOVHUD::ShipNameColor(const AOVShip* Ship)
{
 if(Ship && Ship->Systems && Ship->Systems->FactionId<0)return FLinearColor(.68f,.73f,.77f);
 return Ship && !Ship->bFriendly ? FLinearColor(1.f,.32f,.24f) : Accent;
}
FString AOVHUD::ShipDisplayName(const AOVShip* Ship)
{
 if(!Ship)return TEXT("함선 선택");
 if(Ship->Systems && Ship->Systems->FactionId<0)return TEXT("중립 · ")+Ship->ShipName;
 return Ship->bFriendly?Ship->ShipName:TEXT("적 · ")+Ship->ShipName;
}
void AOVHUD::Button(FName N,const FString& L,float X,float Y,float W,bool Active,EOVTextRole TextRole,FLinearColor Tint)
{
 ActionCenters.Add(N,FVector2D(X+W*.5f,Y+13.5f));
 if(bTrackPracticeButtons)PracticeButtonWidths.Add(N,W);
 DrawRect(Active?FLinearColor(.03f,.4f,.55f,.95f):FLinearColor(.04f,.08f,.13f,.98f),X,Y,W,27);
 UFont* LabelFont=OVTypography::Font(TextRole);
 DrawText(FitLabel(LabelFont,L,W-12),Tint!=FLinearColor::Transparent?Tint:(Active?FLinearColor::White:Ink),X+6,Y+5,LabelFont);
 AddHitBox(FVector2D(X,Y)*UIScale,FVector2D(W,27)*UIScale,N,true,bDrawingOperationsModal?280:(N.ToString().StartsWith(TEXT("OpsReady_"))||N.ToString().StartsWith(TEXT("OpsCancel_")))?230:(bDrawingStarMap || (bStarMapOpen && (N==TEXT("StarMapToggle") || N==TEXT("TitleScreen") || N==TEXT("OpenTutorialGuide"))))?310:bDrawingPractice?220:bDrawingInterior?210:bDrawingDetails?110:10);
}
FName AOVHUD::GetHitActionAtCanvasPoint(FVector2D Point) const
{
 for(const FHUDHitBox& Hit:HitBoxMap)
  if(Hit.ConsumesInput() && Hit.Contains(Point*UIScale)) return Hit.GetName();
 return NAME_None;
}
bool AOVHUD::IsOverPanel(float X,float Y) const
{
 if(bBaseViewOpen) {
  const FVector2D P(X/UIScale,Y/UIScale);
  return BaseDragWindow!=INDEX_NONE || BaseWindowAt(P)!=INDEX_NONE ||
   P.Y<64 || (P.Y>ViewportPixels.Y/UIScale-46 && FMath::Abs(P.X-ViewportPixels.X/(2*UIScale))<212);
 }
 if(PickingShip())return false;
 if(bChoosingTransport||bChoosingTransfer)return true;
 if(bPauseMenu || bConsumeDebriefClick || IsDebriefOpen() || LastMapCloseFrame==GFrameCounter || bStarMapOpen || bTitleScreenOpen || IsOverAddedUI(X,Y)) return true;
 const auto* PC=Cast<AOVController>(PlayerOwner); if(!PC) return false;
 const FVector2D P(X/UIScale,Y/UIScale);
 const FString HitAction=GetHitActionAtCanvasPoint(P).ToString();
 if(HitAction.StartsWith(TEXT("Practice")))return true;
 if(HitAction.StartsWith(TEXT("FacilityAlert_"))||HitAction.StartsWith(TEXT("OpsReady_"))||HitAction.StartsWith(TEXT("OpsCancel_"))||HitAction.StartsWith(TEXT("OpsCrewBG_")))return true;
 if (const auto* Play = PC->GetPlayScenario(); Play && Play->GetActiveDefinition() && Play->PracticeChapter==INDEX_NONE && !bSceneMenuOpen &&
     P.X >= 0 && P.X <= (Play->ActiveId==TEXT("Sandbox")?426:508) && P.Y >= 63 && P.Y <= 63+Play->MissionPanelHeight()) return true;


 if(IsOverInteriorPanel(X,Y)) return true;
 if(IsOverMissionPanel(X,Y)) return true;
 if(IsOverStarMapPanel(X,Y)) return true;
 if(bDragging||bResizing||DraggedPin!=INDEX_NONE) return true;
 if(!PC->bDeveloperMode&&!bSceneMenuOpen&&PC->Phase2CScenario>=0&&P.X>=212&&P.X<=484&&P.Y>=76&&P.Y<=224)return true;
 if(!PC->bDeveloperMode&&!bHeroPanelOpen)for(int32 I=0;I<PinnedShips.Num();++I) {
  const FVector2D Pos=GetPinnedPosition(I);
  const FVector2D Size=GetPinnedSize(I);
  if(P.X>=Pos.X&&P.X<=Pos.X+Size.X&&P.Y>=Pos.Y&&P.Y<=Pos.Y+Size.Y)return true;
 }
 const int32 Notices=BattleNoticeRows();const float NoticeX=ViewportPixels.X/UIScale-354;
 const auto* Feed=GetBattleNoticeFeed();const int32 Footer=bBattleFeedOpen?(bNoticeSettings?242:66)+(Feed&&Feed->DroppedCount>0?20:0):0;
 if(Notices>0&&P.X>=NoticeX&&P.X<=NoticeX+340&&P.Y>=BattleNoticeTop()&&P.Y<=BattleNoticeTop()+Notices*36+Footer)return true;
 // The basic HUD remains visible under the separate developer window.
 if(P.Y<62 || (PC->Selected && P.X<620 && P.Y>ViewportPixels.Y/UIScale-CommandPanelHeight()-10) || (!PC->Selection.IsEmpty() && P.X>=ViewportPixels.X/UIScale-FleetPanelWidth(ViewportPixels.X/UIScale)-10 && P.Y>=ViewportPixels.Y/UIScale-(PC->bFormationMenu?310:218)) || (bSceneMenuOpen && P.X>=216 && P.X<=FMath::Min(900.,ViewportPixels.X/UIScale-10) && P.Y<=110+UOVPlayScenarioSubsystem::Definitions().Num()*36)) return true;
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
 return !bStarMapOpen && !bTitleScreenOpen && HasOrbitPreview() && P.X>=10 && P.X<=WindowSize.X-10 && P.Y>=72 && P.Y<=72+WindowSize.Y*.5;
}

void AOVHUD::OrbitPreview(FVector2D Delta)
{
 if(!HasOrbitPreview()) return;
 PreviewRotation.Yaw+=Delta.X;
 PreviewRotation.Pitch=FMath::Clamp(PreviewRotation.Pitch-Delta.Y,-85.,85.);
}

bool AOVHUD::HandlePointer(float X,float Y,bool Pressed,bool Down,float Wheel,bool Orbit,float DX,float DY)
{
 if(PickingShip())return false;
 if(bChoosingTransport||bChoosingTransfer)return true;
 // Canvas buttons dispatch on press. The next field can already be visible
 // when the controller polls that same press later in the frame.
 if(bConsumeDebriefClick) { bConsumeDebriefClick=Down; return true; }
 if(bPauseMenu || IsDebriefOpen()) return true;
 const auto* PC=Cast<AOVController>(PlayerOwner);
 if(PC) { int32 Width=0,Height=0; PC->GetViewportSize(Width,Height); if(Width>0&&Height>0) ViewportPixels=FVector2D(Width,Height); }
 const float BaseScale=FMath::Max(.25f,FMath::Min(ViewportPixels.X/1280.f,ViewportPixels.Y/740.f));
 UIScale=FMath::Clamp(BaseScale*UserUIScaleMultiplier,.25f,4.f);
 if (bStarMapOpen) UIScale=FMath::Min(UIScale,float(FMath::Min(ViewportPixels.X/1210.,ViewportPixels.Y/488.)));
 // AHUD::Canvas is cleared by the viewport after rendering. Input runs outside DrawHUD.
 if (bStarMapOpen || bTitleScreenOpen) return true;
 const bool Over=IsOverPanel(X,Y);
 if(PC&&PC->bDeveloperMode){
  bDragging=false;bResizing=false;bInteriorDragging=false;bInteriorResizing=false;bInteriorScrolling=false;DraggedPin=INDEX_NONE;bPinResizing=false;
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
 if(!bDragging&&!bResizing&&(bInteriorDragging||bInteriorResizing||bInteriorScrolling||IsOverInteriorPanel(X,Y))) {
  if(bInteriorOpen && !bInteriorMinimized && InteriorScrollTrack.bIsValid && InteriorLastColumn>0){
   const float Travel=InteriorScrollTrack.GetSize().X-InteriorThumbWidth;
   if(Pressed && InteriorScrollTrack.IsInsideOrOn(P) && Travel>0){
    const float ThumbLeft=InteriorScrollTrack.Min.X+Travel*InteriorCardColumn/InteriorLastColumn;
    InteriorThumbGrab=P.X>=ThumbLeft && P.X<=ThumbLeft+InteriorThumbWidth ? P.X-ThumbLeft : InteriorThumbWidth*.5f;
    bInteriorScrolling=true;
   }
   if(bInteriorScrolling && Travel>0){
    InteriorCardColumn=FMath::RoundToInt(FMath::Clamp((P.X-InteriorScrollTrack.Min.X-InteriorThumbGrab)/Travel,0.,1.)*InteriorLastColumn);
    if(!Down)bInteriorScrolling=false;
    return true;
   }
   if(Wheel!=0 && P.X>=InteriorScrollTrack.Min.X-32 && P.Y>=InteriorScrollTrack.Min.Y-104)
    InteriorCardColumn=FMath::Clamp(InteriorCardColumn+(Wheel<0?1:-1),0,InteriorLastColumn);
  }else bInteriorScrolling=false;
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
  WindowPosition.Y=FMath::Clamp(WindowPosition.Y,56.0,FMath::Max(56.0,ViewportPixels.Y/UIScale-WindowSize.Y));
 }
 if(bResizing) { WindowSize.X=FMath::Clamp(P.X-WindowPosition.X,400.0,FMath::Max(400.,ViewportPixels.X/UIScale-WindowPosition.X));WindowSize.Y=FMath::Clamp(P.Y-WindowPosition.Y,420.0,FMath::Max(420.,ViewportPixels.Y/UIScale-WindowPosition.Y)); }
 if(IsOverOrbitPreview(FVector2D(X,Y))) {
  if(Orbit) OrbitPreview(FVector2D(DX,DY));
  if(Wheel) PreviewZoom=FMath::Clamp(PreviewZoom+Wheel*.1f,.5f,1.6f);
 }
 return Over||bDragging||bResizing;
}
void AOVHUD::NotifyHitBoxClick(FName N)
{
 if (PracticeAction(N)) return;
 if (HandleBaseAction(N)) return;
 if(PickingShip())return;
 if(bChoosingTransport||bChoosingTransfer){
  const FString Pick=N.ToString();
  if(N!=TEXT("OpsCloseRoutes") && N!=TEXT("OpsPods") && N!=TEXT("OpsSelf") &&
     !Pick.StartsWith(TEXT("OpsVia_")) && !Pick.StartsWith(TEXT("OpsCross_")))return;
 }
 if(!bPauseMenu && !bTitleScreenOpen && !IsDebriefOpen() && HandleShipOperations(N))return;
 if(N==TEXT("SystemMenu")){TogglePauseMenu();return;}
 if(bPauseMenu){
  if(N==TEXT("PauseSettings") && TitleScreenWidget){bTitleScreenOpen=true;TitleScreenWidget->ActivateFieldPanel(EOVTitleMenuPanel::Settings);SyncAddedUI();return;}
  if(N==TEXT("PauseResume")){TogglePauseMenu();return;}
  if(N==TEXT("PauseRestart")||N==TEXT("PauseList")||N==TEXT("PauseTitle")){
   TogglePauseMenu();if(N==TEXT("PauseTitle")){OpenTitleScreen();return;}
   if(N==TEXT("PauseList"))NotifyHitBoxClick(TEXT("PlaySceneExit"));
   else if(auto* PC=Cast<AOVController>(PlayerOwner))PC->Action(TEXT("RestartScene"));
  }
  return;
 }
 if(N==TEXT("ArenaViewA") || N==TEXT("ArenaViewB")){
  auto* PC=Cast<AOVController>(PlayerOwner);
  auto* Play=GetWorld()->GetSubsystem<UOVPlayScenarioSubsystem>();
  if(PC && Play->ActiveId==TEXT("HeroArena") && PC->Ships.Num()>=2){
   InteriorSelectedUnits.Reset();
   const auto Present=PC->Interior->GetUnitsOnShip(PC->Ships[1],0);
   for(int32 Id:Play->ArenaWaves[N==TEXT("ArenaViewB")?1:0].Units){
    const auto U=PC->Interior->GetUnit(Id);
    if(!Present.Contains(Id))continue;
    if(InteriorSelectedUnits.IsEmpty()){
     InteriorShip=U.Ship;InteriorCardColumn=0;InteriorTeamFilter=INDEX_NONE;bInteriorOpen=true;bInteriorMinimized=false;InteriorTargetSlot=NAME_None;
     const auto* L=U.Ship->Systems->Layout.Get();const int32 Slot=L->GetSlotIndex(U.NodeId);
     if(L->Slots.IsValidIndex(Slot))InteriorDeck=L->Slots[Slot].DeckId;
    }
    InteriorSelectedUnits.Add(Id);
   }
   if(InteriorSelectedUnits.IsEmpty())PC->LastAction=TEXT("해당 팀이 아직 적함에 진입하지 않았습니다");
  }
  return;
 }
 if(N.ToString().StartsWith(TEXT("FacilityAlert_"))){ConfirmFacilityAlert(N);return;}
 if(N.ToString().StartsWith(TEXT("SurveyDeck_"))){
  auto* PC=Cast<AOVController>(PlayerOwner);
  if(PC && PC->Selected && PC->Selected->LastSurveyAt>=0){
    PC->ViewedDeck=FCString::Atoi(*N.ToString().Mid(11));bHangarView=false;
  }
  return;
 }
 // Different rendered buttons need unique hitboxes even when they share a game command.
 if(N==TEXT("MissionHeroPanel"))N=TEXT("HeroPanel");
 if(N==TEXT("CommandHeroPanel")||N==TEXT("InteriorHeroPanel"))N=TEXT("HeroPanel");
 if(N==TEXT("HangarLaunchAircraft"))N=TEXT("LaunchAircraft");
 if(N==TEXT("HangarRecallAircraft"))N=TEXT("RecallAircraft");
 if(N==TEXT("DetailsInteriorToggle"))N=TEXT("InteriorToggle");
 if(IsDebriefOpen() && (N==TEXT("ResultField") || N==TEXT("RestartScene"))) bConsumeDebriefClick=true;
 if(N==TEXT("ResultField")) { bDebriefDismissed=true; return; }
 if(N==TEXT("ShowResult")) { bDebriefDismissed=false; return; }
 if(N==TEXT("MissionCards")) {
  if(auto* PC=Cast<AOVController>(PlayerOwner)) OpenMissionCards(PC->GetPlayScenario()->ActiveId,true);
  return;
 }
 if(N==TEXT("CycleDifficulty")) { GetWorld()->GetSubsystem<UOVPlayScenarioSubsystem>()->CycleDifficulty();return; }
 if(N==TEXT("SceneMenuClose")){bSceneMenuOpen=false;return;}
 if(HandleInteriorAction(N)) return;
 if(N==TEXT("HeroPanel")) { SetHeroPanelOpen(!bHeroPanelOpen);return; }
 if(N==TEXT("EditSortie")){auto* pc=CastChecked<AOVController>(PlayerOwner);BeginFleetPreparation(pc->GetPlayScenario()->ActiveId,true);return;}
 if(N==TEXT("TitleScreen")) { OpenTitleScreen(); return; }
 if(N==TEXT("OpenTutorialGuide")) { OpenTutorialGuide(); return; }
 if(N==TEXT("BattleFeedToggle")) { bBattleFeedOpen=!bBattleFeedOpen;BattleFeedPage=0;BattleFeedFilter=0;return; }
 if(N==TEXT("NoticeSettings") && bBattleFeedOpen){bNoticeSettings=!bNoticeSettings;return;}
 if(N.ToString().StartsWith(TEXT("ToastKind")) && bBattleFeedOpen && bNoticeSettings){
  const int32 Kind=FCString::Atoi(*N.ToString().Mid(9));
  if(Kind>=0 && Kind<=int32(EOVBattleNoticeKind::TrooperDeath)){
   NoticeMask^=(1<<Kind);
   GConfig->SetInt(TEXT("OrbitalVanguard.UI"),TEXT("NoticeMask"),NoticeMask,GGameUserSettingsIni);
   GConfig->Flush(false,GGameUserSettingsIni);
  }
  return;
 }
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
   if(Position!=INDEX_NONE&&Position>=Page*3&&Position<Page*3+VisibleBattleNoticeCount()) {PC->GetPlayScenario()->ConfirmPracticeNotice(Feed->Notices[Index].ShipId);PC->bFollow=false;PC->CameraFocus=Feed->Notices[Index].LastPosition;PC->UpdateCamera();}
  }
  return;
 }
 if(N==TEXT("MissionBackground")||N==TEXT("StarMapBackground")) return;
 if(N==TEXT("StarMapClose")) { LastMapCloseFrame=GFrameCounter; bStarMapOpen=false; return; }
 if(N==TEXT("PlotJump")) {
  if(auto* PC=Cast<AOVController>(PlayerOwner))SelectedStarSystemId=PC->CurrentStarSystemId;
  bChoosingJump=true;bStarMapOpen=true;bSceneMenuOpen=false;return;
 }
 if(N.ToString().StartsWith(TEXT("StarSystem_FTL_")) && bStarMapOpen){
  if(auto* PC=Cast<AOVController>(PlayerOwner);PC && PC->DispatchFleetFTL(FName(*N.ToString().Mid(15)))){
   bStarMapOpen=false;bChoosingJump=false;bConsumeDebriefClick=true;LastMapCloseFrame=GFrameCounter;
  }
  return;
 }
 if(N==TEXT("StarMapToggle")) {
  bChoosingJump=false;
  if(bStarMapOpen) LastMapCloseFrame=GFrameCounter;
  bStarMapOpen=!bStarMapOpen;
  if(bStarMapOpen) {
   bSceneMenuOpen=false;
   if(auto* PC=Cast<AOVController>(PlayerOwner)) SelectedStarSystemId=PC->CurrentStarSystemId;
  }
  return;
 }
 if(N.ToString().StartsWith(TEXT("StarSystem_Select_"))) {
  SelectedStarSystemId=FName(*N.ToString().Mid(18));
  return;
 }
 if(N==TEXT("WindowBackground")) return;
 if(N.ToString().StartsWith(TEXT("PinnedBackground_"))) return;
 if(N==TEXT("DetailsTitle"))return;
 if(N==TEXT("CommandShipLabel")||N==TEXT("CommandTargetLabel"))return;
 if(N==TEXT("SceneMenu")) { bSceneMenuOpen=!bSceneMenuOpen; if(bSceneMenuOpen) bStarMapOpen=false; return; }
 if(N==TEXT("PlaySceneExit")) {
  if(auto* PC=Cast<AOVController>(PlayerOwner)) {
   const FName Completed=PC->GetPlayScenario()->ActiveId;
   PC->LeavePlayScenario();
   OpenMissionCards(Completed);
  }
  return;
 }
 if(UOVPlayScenarioSubsystem::Definitions().ContainsByPredicate([N](const auto& Scene){return Scene.Id==N;}))
 {
  if(UOVPlayScenarioSubsystem::IsBudgetMission(N)){BeginFleetPreparation(N);return;}
  if(auto* PC=Cast<AOVController>(PlayerOwner);PC&&PC->StartPlayScenario(N)) bSceneMenuOpen=false;
  return;
 }
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
 if(N==TEXT("HangarView")) { bHangarView=true;bAircraftOnly=false;return; }
 if(N==TEXT("HangarDisplayMode")) { bAircraftGrid=!bAircraftGrid;return; }
 if(N==TEXT("DetailsPagePrevious")||N==TEXT("DetailsPageNext")) {
  const auto* PC=Cast<AOVController>(PlayerOwner);
  if(PC&&PC->Selected&&PC->Selected==DetailsPageShip&&PC->ViewedDeck>0&&PC->ViewedDeck==DetailsPageDeck&&PC->Selected->bFriendly&&bDetailsOpen&&!bMinimized&&!bHangarView) {
   const auto* Systems=PC->Selected->Systems.Get();const auto* Layout=Systems?Systems->Layout.Get():nullptr;int32 Count=0;
   if(Layout)for(int32 I=0;I<Layout->Slots.Num()&&I<Systems->Slots.Num();++I)if(Layout->Slots[I].DeckId==PC->ViewedDeck)++Count;
   DetailsSlotPage=FMath::Clamp(DetailsSlotPage+(N==TEXT("DetailsPageNext")?1:-1),0,FMath::Max(0,(Count-1)/15));
  }
  return;
 }
 if(N==TEXT("External")||N.ToString().StartsWith(TEXT("Deck")))bHangarView=false;
 if(N==TEXT("WindowPin")) {
  auto* PC=Cast<AOVController>(PlayerOwner);
  const int32 Index=PinnedShips.IndexOfByKey(PC->Selected);
  if(Index!=INDEX_NONE){DraggedPin=INDEX_NONE;bPinResizing=false;PinnedShips.RemoveAt(Index);PinnedPreviews[Index]->Release();PinnedPreviews.RemoveAt(Index);PinnedDecks.RemoveAt(Index);PinnedSlots.RemoveAt(Index);PinnedWeapons.RemoveAt(Index);PinnedSlotPages.RemoveAt(Index);PinnedAircraftIds.RemoveAt(Index);PinnedAircraftPages.RemoveAt(Index);PinnedPositions.RemoveAt(Index);PinnedSizes.RemoveAt(Index);}
  else if(PinnedShips.Num()<2){PinnedShips.Add(PC->Selected);PinnedPreviews.Add(NewObject<UOVShipPreview>(this));PinnedDecks.Add(0);PinnedSlots.Add(INDEX_NONE);PinnedWeapons.Add(INDEX_NONE);PinnedSlotPages.Add(0);PinnedAircraftIds.Add(0);PinnedAircraftPages.Add(0);PinnedPositions.Add(FVector2D(-1,-1));PinnedSizes.Add(FVector2D(266,158));}
  return;
 }
 if(N.ToString().StartsWith(TEXT("PinnedClose_"))) {
  const int32 Index=FCString::Atoi(*N.ToString().Mid(12));
  if(PinnedShips.IsValidIndex(Index)){DraggedPin=INDEX_NONE;bPinResizing=false;PinnedShips.RemoveAt(Index);PinnedPreviews[Index]->Release();PinnedPreviews.RemoveAt(Index);PinnedDecks.RemoveAt(Index);PinnedSlots.RemoveAt(Index);PinnedWeapons.RemoveAt(Index);PinnedSlotPages.RemoveAt(Index);PinnedAircraftIds.RemoveAt(Index);PinnedAircraftPages.RemoveAt(Index);PinnedPositions.RemoveAt(Index);PinnedSizes.RemoveAt(Index);}
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
 if(N.ToString().StartsWith(TEXT("PinnedAirGrid_"))||N.ToString().StartsWith(TEXT("PinnedAirBack_"))) {
  const bool bGrid=N.ToString().StartsWith(TEXT("PinnedAirGrid_"));
  const int32 Index=FCString::Atoi(*N.ToString().Mid(14));
  if(PinnedShips.IsValidIndex(Index)&&PinnedAircraftPages.IsValidIndex(Index)&&!GetPinnedCarrierSummary(Index).IsEmpty()) {
   PinnedDecks[Index]=bGrid?-2:-1;PinnedAircraftPages[Index]=0;PinnedAircraftIds[Index]=0;
  }
  return;
 }
 if(N.ToString().StartsWith(TEXT("PinnedAirPage_"))||N.ToString().StartsWith(TEXT("PinnedAirCard_"))) {
  TArray<FString> Parts;N.ToString().ParseIntoArray(Parts,TEXT("_"),true);
  if(Parts.Num()!=3||!Parts[1].IsNumeric()||!Parts[2].IsNumeric())return;
  const int32 Index=FCString::Atoi(*Parts[1]),Value=FCString::Atoi(*Parts[2]);
  if(!PinnedShips.IsValidIndex(Index)||!PinnedAircraftPages.IsValidIndex(Index)||PinnedDecks[Index]!=-2||GetPinnedCarrierSummary(Index).IsEmpty())return;
  const auto* Wing=GetWorld()->GetSubsystem<UOVCarrierSubsystem>();const AOVShip* Ship=PinnedShips[Index].Get();
  if(!Wing||!Ship||!Ship->Systems)return;
  if(Parts[0]==TEXT("PinnedAirPage")) {
   int32 Count=0;for(const auto& A:Wing->Aircraft)if(A.Carrier==Ship&&A.FactionId==Ship->Systems->FactionId)++Count;
   PinnedAircraftPages[Index]=FMath::Clamp(PinnedAircraftPages[Index]+FMath::Clamp(Value,-1,1),0,FMath::Max(0,(Count-1)/5));
  } else if(Value>0&&Wing->Aircraft.ContainsByPredicate([&](const FOVAircraftRecord& A){return A.AircraftId==Value&&A.Carrier==Ship&&A.FactionId==Ship->Systems->FactionId;}))PinnedAircraftIds[Index]=Value;
  return;
 }
 if(N.ToString().StartsWith(TEXT("PinnedLaunch_"))||N.ToString().StartsWith(TEXT("PinnedRecall_"))) {
  const bool bLaunch=N.ToString().StartsWith(TEXT("PinnedLaunch_"));
  const int32 Index=FCString::Atoi(*N.ToString().Mid(13));
  auto* PC=Cast<AOVController>(PlayerOwner);
  if(PC&&PinnedShips.IsValidIndex(Index)&&PinnedDecks.IsValidIndex(Index)&&PinnedDecks[Index]==-1&&!GetPinnedCarrierSummary(Index).IsEmpty())
   PC->CommandCarrier(PinnedShips[Index].Get(),bLaunch);
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
 if(N==TEXT("WindowMax")) { const bool Full=WindowSize.X>=ViewportPixels.X/UIScale-28;WindowPosition=FVector2D(8,68);WindowSize=Full?FVector2D(520,560):ViewportPixels/UIScale-FVector2D(20,76);return; }
 if(auto* PC=Cast<AOVController>(PlayerOwner)) PC->Action(N);
}
void AOVHUD::DrawExternal(float X,float Y,float W)
{
 auto* PC=Cast<AOVController>(PlayerOwner); if(!PC||!PC->Selected)return;
 if(PreviewSubject!=PC->Selected.Get()) { PreviewSubject=PC->Selected.Get(); PreviewRotation=UOVShipPreview::DefaultHullView(); PreviewZoom=1; }
 if(!ShipPreview)ShipPreview=NewObject<UOVShipPreview>(this);
 ShipPreview->Update(PC->Selected,PreviewRotation,PreviewZoom,W/(WindowSize.Y*.5f),INDEX_NONE,FMath::CeilToInt(W*UIScale));
 DrawRect(FLinearColor(.009f,.02f,.035f),X,Y,W,WindowSize.Y*.5f);
 if(auto* Texture=ShipPreview->GetTexture())DrawTexture(Texture,X,Y,W,WindowSize.Y*.5f,0,0,1,1,FLinearColor::White,BLEND_Opaque);
 const auto* Systems=PC->Selected->Systems.Get(); const auto* Layout=Systems->Layout.Get();
 for(int32 I=0;I<Layout->WeaponMounts.Num();++I)
 {
  const FVector2D UV=ShipPreview->Project(Layout->WeaponMounts[I].LocalPosition);
  if(UV.X<.03||UV.X>.97||UV.Y<.04||UV.Y>.9)continue;
  const FVector2D P(X+UV.X*W,Y+UV.Y*WindowSize.Y*.5f);
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
  const float PW=FMath::Min(150.f,W*.4f),PH=88.f,PX=X+W-PW-5,PY=Y+5;
  WeaponPreview->Update(PC->Selected,FRotator(-25,35,0),1.f,PW/PH,PC->SelectedWeapon,FMath::CeilToInt(PW*UIScale));
  DrawRect(FLinearColor(.009f,.02f,.035f),PX,PY,PW,PH);
  if(auto* Texture=WeaponPreview->GetTexture())DrawTexture(Texture,PX,PY,PW,PH,0,0,1,1,FLinearColor::White,BLEND_Opaque);
  DrawText(TEXT("선택 무장 · 실시간"),Ink,PX+4,PY+4,OVTypography::Font(EOVTextRole::Body));
  const float Ratio=GetSelectedWeaponHealthRatio();
  if(Ratio>=0) {
   DrawRect(FLinearColor(.08f,.1f,.12f),PX+5,PY+PH-8,PW-10,4);
   DrawRect(Health(Ratio),PX+5,PY+PH-8,(PW-10)*Ratio,4);
  }
  AddHitBox(FVector2D(PX,PY)*UIScale,FVector2D(PW,PH)*UIScale,TEXT("WeaponPreviewBackground"),true,140);
 }
 DrawText(TEXT("중클릭 회전 · 휠 확대 · 무장 선택"),Ink,X+8,Y+WindowSize.Y*.5f-18,OVTypography::Font(EOVTextRole::Body));
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
    else if(Ready==TEXT("DEFENDING"))Status=Mount->Cooldown>UE_KINDA_SMALL_NUMBER?FString::Printf(TEXT("재사용 %.1f 초"),Mount->Cooldown):TEXT("자동 방어");
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
   Kind=AttackWeaponName(*Mount,&Def);
   Range=Mount->Profile.Range;bRangeKnown=true;
   const auto Ready=PC->Combat->GetReadiness(Ship,HealthState.WeaponMountId);
   Status=Ready.Reason==TEXT("COOLDOWN")?FString::Printf(TEXT("재사용 %.1f 초"),Ready.Cooldown):ReadinessKorean(Ready.Reason);
  }
 }
 if(HP<=0)Status=TEXT("무장 파괴");
 const FString RangeText=bRangeKnown?OVTypography::Metres(Range/100.f):TEXT("사거리 미확인");
 return FString::Printf(TEXT("%s · T%.0f · %s\nHP %.0f/%.0f · %s"),*Kind,Def.Tier,*RangeText,HP,MaxHP,*Status);
}
void AOVHUD::DrawFacilityAlerts()
{
 auto* PC=Cast<AOVController>(PlayerOwner);
 if(!PC || PC->bDeveloperMode || bSceneMenuOpen)return;
 const float Bottom=ViewportPixels.Y/UIScale-CommandPanelHeight()-13;
 int32 Count=0;
 for(int32 ShipId=0;ShipId<PC->Ships.Num();++ShipId){
  auto* Ship=PC->Ships[ShipId].Get();
  if(!IsValid(Ship)||!Ship->bFriendly||!Ship->Systems||!Ship->Systems->Layout)continue;
  for(int32 I=0;I<Ship->Systems->Slots.Num();++I){
   if(!Ship->Systems->Slots[I].bUnreadLoss)continue;
   if(++Count>2)continue;
   const auto& D=Ship->Systems->Layout->Slots[I];
   const FString Label=FString::Printf(TEXT("시설 파괴   %s   ·   %d층 %s   ›"),*Ship->ShipName,D.DeckId,*Facility(D.Facility));
   const float Y=Bottom-Count*29;
   Button(FName(*FString::Printf(TEXT("FacilityAlert_%d_%d"),ShipId,I)),Label,10,Y,385);
   DrawRect(FLinearColor(1,.32f,.08f),10,Y,4,27);
  }
 }
 if(Count>2)DrawText(FString::Printf(TEXT("미확인 %d건"),Count),Ink,400,Bottom-50,OVTypography::Font(EOVTextRole::Numeric));
}

bool AOVHUD::ConfirmFacilityAlert(FName Action)
{
 TArray<FString> Parts;Action.ToString().ParseIntoArray(Parts,TEXT("_"));
 auto* PC=Cast<AOVController>(PlayerOwner);
 if(Parts.Num()!=3 || !PC)return false;
 const int32 ShipId=FCString::Atoi(*Parts[1]), Slot=FCString::Atoi(*Parts[2]);
 if(!PC->Ships.IsValidIndex(ShipId))return false;
 AOVShip* Ship=PC->Ships[ShipId];
 if(!IsValid(Ship)||!Ship->bFriendly||!Ship->Systems->Layout||!Ship->Systems->Slots.IsValidIndex(Slot))return false;
 auto& S=Ship->Systems->Slots[Slot];if(!S.bUnreadLoss)return false;
 S.bUnreadLoss=false;
 PC->SelectShip(Ship);if(!PC->Selected)return true;
 PC->bFollow=true;PC->CameraFocus=Ship->GetActorLocation();PC->UpdateCamera();
 const auto& Slots=Ship->Systems->Layout->Slots;
 PC->ViewedDeck=Slots[Slot].DeckId;PC->SelectedSlot=Slot;
 int32 Before=0;for(int32 I=0;I<Slot;++I)if(Slots[I].DeckId==PC->ViewedDeck)++Before;
 DetailsPageShip=Ship;DetailsPageDeck=PC->ViewedDeck;DetailsSlotPage=Before/15;
 bDetailsOpen=false;bInteriorOpen=true;bInteriorMinimized=false;InteriorShip=Ship;
 InteriorDeck=PC->ViewedDeck;InteriorTargetSlot=Slots[Slot].SlotId;InteriorSelectedUnits.Reset();return true;
}

void AOVHUD::DrawDetails()
{
 auto* PC=Cast<AOVController>(PlayerOwner);if(!PC->Selected||!bDetailsOpen)return;
 if(!bDetailsPositioned){WindowPosition.X=ViewportPixels.X/UIScale-WindowSize.X-16;bDetailsPositioned=true;}
 WindowSize.X=FMath::Min(WindowSize.X,ViewportPixels.X/UIScale);
 WindowSize.Y=FMath::Clamp(WindowSize.Y,420.0,FMath::Max(420.0,ViewportPixels.Y/UIScale-68));
 WindowPosition.X=FMath::Clamp(WindowPosition.X,0.0,FMath::Max(0.0,ViewportPixels.X/UIScale-WindowSize.X-12));
 WindowPosition.Y=FMath::Clamp(WindowPosition.Y,68.0,FMath::Max(68.0,ViewportPixels.Y/UIScale-WindowSize.Y));
 const auto* S=PC->Selected->Systems.Get();const auto* L=S->Layout.Get();if(!L)return;
 if(DetailsPageShip!=PC->Selected||DetailsPageDeck!=PC->ViewedDeck){DetailsPageShip=PC->Selected;DetailsPageDeck=PC->ViewedDeck;DetailsSlotPage=0;}
 TGuardValue<bool> DetailLayer(bDrawingDetails,true);
 const float X=WindowPosition.X,Y=WindowPosition.Y,W=WindowSize.X;
 // The foreground window consumes blank-area clicks too, including over fire buttons.
 AddHitBox(WindowPosition*UIScale,FVector2D(W,bMinimized?32:WindowSize.Y)*UIScale,TEXT("WindowBackground"),true,100);
 DrawRect(Panel,X,Y,W,bMinimized?32:WindowSize.Y);
 const int32 TitleLimit=FMath::Clamp(FMath::FloorToInt((W-150)/12.f),4,24);
 const FString FullTitle=ShipDisplayName(PC->Selected);
 const FString ShortTitle=FullTitle.Len()>TitleLimit?FullTitle.Left(TitleLimit-1)+TEXT("…"):FullTitle;
 DrawText(ShortTitle,ShipNameColor(PC->Selected),X+10,Y+8,OVTypography::Font(EOVTextRole::Heading));
 AddHitBox(FVector2D(X+10,Y+2)*UIScale,FVector2D(W-150,28)*UIScale,TEXT("DetailsTitle"),true,110);
 Button(TEXT("WindowMin"),bMinimized?TEXT("+"):TEXT("_"),X+W-90,Y+2,26);Button(TEXT("WindowMax"),TEXT("[]"),X+W-60,Y+2,26);Button(TEXT("WindowClose"),TEXT("X"),X+W-30,Y+2,26);
 if(bMinimized)return;
 Button(TEXT("External"),TEXT("외부 / 무장"),X+10,Y+38,100,PC->ViewedDeck==0&&!bHangarView);
 Button(TEXT("HangarView"),TEXT("격납고"),X+116,Y+38,74,bHangarView);
 Button(TEXT("WindowPin"),TEXT("핀"),X+W-128,Y+2,30,PinnedShips.Contains(PC->Selected));
 const bool Reveal=PC->Selected->bFriendly||PC->bDeveloperMode;
 if(!Reveal && PC->Selected->LastSurveyAt>=0){
  const auto& Survey=PC->Selected->Survey;
  int32 Decks=0;for(const auto& Room:Survey)Decks=FMath::Max(Decks,Room.Deck);
  for(int32 D=1;D<=Decks;++D)Button(FName(*FString::Printf(TEXT("SurveyDeck_%d"),D)),FString::Printf(TEXT("%d층"),D),X+194+(D-1)*43,Y+38,40,PC->ViewedDeck==D);
  if(!bHangarView && PC->ViewedDeck>0){
   DrawText(FString::Printf(TEXT("내부 정찰 기록   ·   %.0f초 시점"),PC->Selected->LastSurveyAt),Accent,X+12,Y+85,OVTypography::Font(EOVTextRole::Numeric));
   TArray<double> Columns,Rows;
   for(const auto& R:Survey)if(R.Deck==PC->ViewedDeck){Columns.AddUnique(R.Centre.X);Rows.AddUnique(R.Centre.Y);}
   Columns.Sort();Rows.Sort();
   const float CW=(W-24)/FMath::Max(1,Columns.Num()),CH=FMath::Min(56.,(WindowSize.Y-146)/FMath::Max(1,Rows.Num()));
   for(const auto& R:Survey)if(R.Deck==PC->ViewedDeck){
    const float CX=X+12+Columns.IndexOfByKey(R.Centre.X)*CW,CY=Y+125+Rows.IndexOfByKey(R.Centre.Y)*CH;
    DrawRect(Panel,CX,CY,CW-2,CH-2);
    DrawText(Facility(R.Facility),R.Integrity>0?Ink:FLinearColor(1,.35f,.19f),CX+2,CY+2,OVTypography::Font(EOVTextRole::Body));
    DrawRect(FLinearColor(.23f,.28f,.3f),CX+2,CY+CH-9,CW-6,4);
    DrawRect(Accent,CX+2,CY+CH-9,(CW-6)*FMath::Clamp(R.Integrity,0.f,1.f),4);
   }
   return;
  }
 }
 if(Reveal)Button(TEXT("OpsInterior"),TEXT("선내 작전"),X+198,Y+38,90);
 if(PC->Selected->HangarHost.IsValid())Button(TEXT("OpsHost"),TEXT("모함 격납고"),X+294,Y+38,100);
 if(bHangarView) {
  if(!Reveal) DrawText(TEXT("적 격납고 · 미확인"),Ink,X+16,Y+88,OVTypography::Font(EOVTextRole::Body));
  else if(auto* Carrier=GetWorld()->GetSubsystem<UOVCarrierSubsystem>()) {
   if(DrawSmallBays(PC->Selected,X,Y,W))return;
   if(AircraftPageShip!=PC->Selected){AircraftPageShip=PC->Selected;AircraftPage=0;AircraftFilter=0;SelectedAircraftId=0;}
   const int32 Row=Carrier->Hangars.FilterByPredicate([&](const FOVHangarState& H){return H.Carrier==PC->Selected;}).Num();
   if(Row==0)DrawText(TEXT("장착된 격납고 없음"),Ink,X+16,Y+88,OVTypography::Font(EOVTextRole::Body));
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
      Contents=FString::Printf(TEXT("%s · 선체 %.0f%% · 정박"),*Small->ShipName.Left(8),100.f*Small->Systems->HullHP/FMath::Max(1.f,Small->Systems->MaxHullHP));
     else Contents=TEXT("소형함 빈 선석");
     DrawText(FString::Printf(TEXT("격납고 %d · %s · %s"),++I,FacilityState,*Contents),Ink,X+16,Y+105+(I-1)*19,OVTypography::Font(EOVTextRole::Numeric));
    }
    Button(TEXT("HangarLaunchAircraft"),TEXT("출격"),X+W-180,Y+250,78);
    Button(TEXT("HangarRecallAircraft"),TEXT("귀환"),X+W-96,Y+250,78);
   } else {
    const auto Overview=Carrier->GetAircraftOverview(PC->Selected);
    DrawText(FString::Printf(TEXT("격납고 %d · 기체 생존 %d · 손실 %d"),Row,Overview.Surviving,Overview.Wrecked),Ink,X+16,Y+103,OVTypography::Font(EOVTextRole::Numeric));
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
     const auto& A=*Records[AircraftPage*15+I];const float CX=X+12+(I%5)*(CW+3),CY=Y+126+(I/5)*37;
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
    Button(TEXT("AircraftPrevious"),TEXT("이전"),X+12,Y+239,62);
    DrawText(FString::Printf(TEXT("%d / %d"),AircraftPage+1,Pages),Ink,X+80,Y+245,OVTypography::Font(EOVTextRole::Numeric));
    Button(TEXT("AircraftNext"),TEXT("다음"),X+125,Y+239,62);
    Button(TEXT("HangarLaunchAircraft"),TEXT("출격"),X+W*.5f-80,Y+271,78);
    Button(TEXT("HangarRecallAircraft"),TEXT("귀환"),X+W*.5f+2,Y+271,78);
    if(const auto* A=Carrier->Aircraft.FindByPredicate([&](const FOVAircraftRecord& Item){return Item.AircraftId==SelectedAircraftId&&Item.Carrier==PC->Selected&&Item.FactionId==S->FactionId;})) {
     const TCHAR* Labels[]={TEXT("보관"),TEXT("출격"),TEXT("귀환"),TEXT("도킹 대기"),TEXT("관제 상실"),TEXT("완파")};
     DrawText(FString::Printf(TEXT("선택 기체 · %s · 내구 %.0f"),Labels[FMath::Clamp(int32(A->State),0,5)],A->HP),Ink,X+16,Y+310,OVTypography::Font(EOVTextRole::Numeric));
     if(PC->Selected->bFriendly&&A->State!=EOVAircraftState::Wrecked)Button(TEXT("AircraftLocate"),TEXT("위치 찾기"),X+W-105,Y+303,90);
    }
   }
  }
 }
 else if(PC->ViewedDeck==0)DrawExternal(X+10,Y+72,W-20);
 else if(Reveal) {
  int32 Count=0;for(int32 I=0;I<L->Slots.Num()&&I<S->Slots.Num();++I)if(L->Slots[I].DeckId==PC->ViewedDeck)++Count;
  DetailsSlotPage=FMath::Clamp(DetailsSlotPage,0,FMath::Max(0,(Count-1)/15));int32 Cell=0,Seen=0;
  for(int32 I=0;I<L->Slots.Num()&&I<S->Slots.Num()&&Cell<15;++I)if(L->Slots[I].DeckId==PC->ViewedDeck) {
   if(Seen++<DetailsSlotPage*15)continue;
   const float CX=X+10+(Cell%5)*(W-20)/5,CY=Y+78+(Cell/5)*54;++Cell;
   const auto& D=L->Slots[I];const auto& State=S->Slots[I];
   Button(FName(*FString::Printf(TEXT("Slot_%d"),I)),PC->bDeveloperMode?D.SlotId.ToString():Facility(D.Facility),CX,CY,(W-30)/5,I==PC->SelectedSlot);
   DrawRect(Health(D.MaxFacilityHP>0?State.HP/D.MaxFacilityHP:0),CX,CY+30,(W-30)/5,4);
   DrawText(Facility(D.Facility).Left(10),Ink,CX,CY+36,OVTypography::Font(EOVTextRole::Body));
  }
  if(Count>15) {
   Button(TEXT("DetailsPagePrevious"),TEXT("이전"),X+10,Y+246,60);
   DrawText(FString::Printf(TEXT("%d / %d"),DetailsSlotPage+1,FMath::DivideAndRoundUp(Count,15)),Ink,X+82,Y+252,OVTypography::Font(EOVTextRole::Numeric));
   Button(TEXT("DetailsPageNext"),TEXT("다음"),X+135,Y+246,60);
  }
 }
 const float InfoY=Y+(bHangarView&&bAircraftGrid?55.f:0.f)+(!bHangarView&&PC->ViewedDeck==0 ? WindowSize.Y*.5f-192.f : 0.f);
 const float gaugeWidth=(W-32)*.5f;
 auto gauge=[&](const TCHAR* label,float current,float maxValue,float left,FLinearColor color){
  const FString value=FString::Printf(TEXT("%s  %s / %s"),label,*OVTypography::Integer(FMath::RoundToInt(current)),*OVTypography::Integer(FMath::RoundToInt(maxValue)));
  const auto font=OVTypography::Font(EOVTextRole::Numeric);
  DrawText(FitLabel(font,value,gaugeWidth),Ink,left,InfoY+277,font);
  DrawRect(FLinearColor(.1f,.16f,.22f),left,InfoY+295,gaugeWidth,4);
  DrawRect(color,left,InfoY+295,gaugeWidth*FMath::Clamp(current/FMath::Max(1.f,maxValue),0.f,1.f),4);
 };
 gauge(TEXT("선체"),S->HullHP,S->MaxHullHP,X+10,Accent);
 gauge(S->bShieldActive?TEXT("쉴드"):TEXT("쉴드 정지"),S->Shield,S->MaxShield,X+22+gaugeWidth,FLinearColor(.3f,.48f,1));
 if(S->ArmorSections.IsValidIndex(PC->SelectedArmor)) {
  const auto& A=S->ArmorSections[PC->SelectedArmor];
  DrawText(FString::Printf(TEXT("장갑 %s %.0f/%.0f"),*(PC->bDeveloperMode?A.ArmorSectionId.ToString():FString(TEXT("선택 구역"))),A.HP,L->ArmorSections[PC->SelectedArmor].MaxArmor),Accent,X+10,InfoY+301,OVTypography::Font(EOVTextRole::Numeric));
 }
 if(!bHangarView&&PC->ViewedDeck==0&&S->WeaponMountStates.IsValidIndex(PC->SelectedWeapon)) {
  TArray<FString> Lines;GetSelectedWeaponSummary().ParseIntoArrayLines(Lines);
  for(int32 I=0;I<FMath::Min(2,Lines.Num());++I) {
   float TextWidth=0,TextHeight=0;GetTextSize(Lines[I],TextWidth,TextHeight,OVTypography::Font(EOVTextRole::Body));
   const float TextScale=FMath::Min(1.f,(W-20)/FMath::Max(1.f,TextWidth));
   DrawText(Lines[I],Ink,X+10,InfoY+322+I*19,OVTypography::Font(EOVTextRole::Body),TextScale);
  }
 }else if(Reveal&&L->Slots.IsValidIndex(PC->SelectedSlot)) {
  const auto& F=S->Slots[PC->SelectedSlot];const auto& D=L->Slots[PC->SelectedSlot];
  if(F.HP<=0 && D.Facility!=EOVFacilityType::None)
   DrawText(FString::Printf(TEXT("시설 파괴   ·   복구 %.0f%%   ·   수리반 %.0f명"),100*F.RebuildHP/FMath::Max(1.f,D.MaxFacilityHP),F.RepairCrew),
            FLinearColor(1,.57f,.22f),X+10,InfoY+387,OVTypography::Font(EOVTextRole::Body));
  DrawText(FString::Printf(TEXT("%s %s 내구 %.0f/%.0f 인원 %.0f/%.0f"),*(PC->bDeveloperMode?F.SlotId.ToString():FString(TEXT("시설"))),*Facility(D.Facility),F.HP,D.MaxFacilityHP,F.Crew,D.RequiredCrew),Ink,X+10,InfoY+322,OVTypography::Font(EOVTextRole::Numeric));
  if(PC->bDeveloperMode)DrawText(S->GetSlotStatus(F.SlotId).Left(45),Accent,X+10,InfoY+341,OVTypography::Font(EOVTextRole::Body));
  else Button(TEXT("DetailsInteriorToggle"),TEXT("선내 작전 열기"),X+10,InfoY+390,150);
 }
 if(PC->Selected->bFTLCharging && PC->GetPlayScenario()){
  const float Frac=PC->GetPlayScenario()->ChargeFraction(PC->Selected);
  DrawRect(FLinearColor(.12f,.12f,.18f),X+12,InfoY+383,W-24,12);
  DrawRect(Accent,X+12,InfoY+383,(W-24)*Frac,12);DrawText(TEXT("도약 충전"),Ink,X+16,InfoY+397,OVTypography::Font(EOVTextRole::Body));
 }
 if(Reveal)DrawText(FString::Printf(TEXT("엔진 %.0f%% 쉴드 %.0f%% 레이더 %.0f%%"),100*S->GetEfficiency(EOVFacilityType::Engine),100*S->GetEfficiency(EOVFacilityType::ShieldControl),100*S->GetEfficiency(EOVFacilityType::Radar)),Ink,X+10,InfoY+362,OVTypography::Font(EOVTextRole::Numeric));
 else DrawText(TEXT("적 내부 정보 숨김 · 관측된 외장만 표시"),Ink,X+10,InfoY+362,OVTypography::Font(EOVTextRole::Body));
 if(PC->bDeveloperMode) {
  DrawText(TEXT("DEV ONLY: direct tools, not supply repair"),FLinearColor::Yellow,X+10,InfoY+384,OVTypography::Font(EOVTextRole::Body));
  const FName Names[]={TEXT("TestHit"),TEXT("ArmorZero"),TEXT("RepairArmor"),TEXT("Reset"),TEXT("DestroyExt"),TEXT("Sabotage"),TEXT("RepairSlot"),TEXT("Occupy"),TEXT("CrewDown"),TEXT("CrewUp"),TEXT("WeaponHit"),TEXT("RepairWeapon")};
  const TCHAR* Labels[]={TEXT("HIT 150"),TEXT("ARMOR 0"),TEXT("FIX ARMOR"),TEXT("RESET"),TEXT("DESTROY"),TEXT("SABOTAGE"),TEXT("FIX ROOM"),TEXT("OCCUPY"),TEXT("CREW -2"),TEXT("CREW +2"),TEXT("HIT GUN"),TEXT("FIX GUN")};
  for(int32 I=0;I<12;++I)Button(Names[I],I==0?FString::Printf(TEXT("HIT %.0f"),PC->TestDamage):FString(Labels[I]),X+10+(I%4)*(W-20)/4,InfoY+405+(I/4)*33,(W-32)/4);
  DrawText(FString::Printf(TEXT("Event %d: %s -> %s / HP -%.1f"),S->HitEventId,*S->LastArmorSection.ToString(),*S->LastDamageSlot.ToString(),S->LastHullDamage),Accent,X+10,InfoY+510,OVTypography::Font(EOVTextRole::Numeric));
 }
 DrawLine(X+W-16,Y+WindowSize.Y-2,X+W-2,Y+WindowSize.Y-16,Ink,2);
}
bool AOVHUD::IsDebriefOpen() const
{
 const auto* PC=Cast<AOVController>(PlayerOwner);
 const auto* Play=PC ? PC->GetPlayScenario() : nullptr;
 return Play && Play->Result!=EOVPlaySceneResult::Running && !bDebriefDismissed && !bTitleScreenOpen;
}

void AOVHUD::OpenMissionCards(FName Selected, bool bBrowse)
{
 auto* PC=Cast<AOVController>(PlayerOwner);
 if(bBrowse && PC && TitleScreenWidget)
 {
  PauseBeforeCards=PC->bSimulationPaused;
  PC->bSimulationPaused=true;
  bTitleScreenOpen=true;
  bSceneMenuOpen=false;
  TitleScreenWidget->ActivateFieldPanel(EOVTitleMenuPanel::ScenarioSelect);
  SyncAddedUI();
 }
 else OpenTitleScreen();
 if(TitleScreenWidget)
 {
  if(!Selected.IsNone()) TitleScreenWidget->SelectScenario(Selected);
  TitleScreenWidget->ShowPanel(EOVTitleMenuPanel::ScenarioSelect);
 }
}

void AOVHUD::DrawHUD()
{
    CSV_SCOPED_TIMING_STAT_EXCLUSIVE(OV_HUD);
    TRACE_CPUPROFILER_EVENT_SCOPE(OV_HUD);
 SyncAddedUI();
 ActionCenters.Reset();
 PracticeButtonWidths.Reset();
 if(bTitleScreenOpen){ Super::DrawHUD(); return; }
 Super::DrawHUD();auto* PC=Cast<AOVController>(PlayerOwner);if(!PC||!Canvas)return;
 bTrackPracticeButtons=PC->GetPlayScenario() && PC->GetPlayScenario()->PracticeChapter>=0;
 if(WeaponPreview&&(!HasOrbitPreview()||!IsValid(PC->Selected)||PC->Selected->IsHidden()||PC->Selected->Systems->bWrecked||(PC->Combat&&PC->Combat->IsDeparted(PC->Selected))))WeaponPreview->Release();
 ViewportPixels=FVector2D(Canvas->SizeX,Canvas->SizeY);
 const float BaseDrawScale=FMath::Max(.25f,FMath::Min(Canvas->SizeX/1280.f,Canvas->SizeY/740.f));
 UIScale=FMath::Clamp(BaseDrawScale*UserUIScaleMultiplier,.25f,4.f);
 if (bStarMapOpen) UIScale=FMath::Min(UIScale,float(FMath::Min(ViewportPixels.X/1210.,ViewportPixels.Y/488.)));
 Canvas->Canvas->PushRelativeTransform(FScaleMatrix(FVector(UIScale,UIScale,1)));
 if(IsBaseViewOpen()) { DrawBaseView(); Canvas->Canvas->PopTransform(); return; }
 if(PickingShip()){
  DrawText(PC->InputMode==EOVInputMode::BoardingTarget?TEXT("강습할 적함 선택 · Esc 취소"):TEXT("수납할 아군 소형함 선택 · Esc 취소"),FLinearColor::White,18,18,OVTypography::Font(EOVTextRole::Heading));
  Canvas->Canvas->PopTransform();return;
 }

 if(bPauseMenu) {
  const float W=ViewportPixels.X/UIScale,H=ViewportPixels.Y/UIScale;
  const float X=(W-390)*.5f,Y=(H-300)*.5f;
  DrawRect(FLinearColor(.005f,.012f,.025f,.76f),0,0,W,H);DrawRect(Panel,X,Y,390,300);
  DrawRect(Accent,X,Y,390,3);
  DrawText(TEXT("작전 일시정지"),FLinearColor::White,X+28,Y+20,OVTypography::Font(EOVTextRole::Heading),1.85f);
  Button(TEXT("PauseResume"),TEXT("전투 재개"),X+28,Y+78,334,true);
  Button(TEXT("PauseRestart"),TEXT("현재 임무 재시작"),X+28,Y+120,334);
  Button(TEXT("PauseSettings"),TEXT("환경 설정 · 화면 배율"),X+28,Y+162,334);
  Button(TEXT("PauseList"),TEXT("임무 목록"),X+28,Y+204,334);
  Button(TEXT("PauseTitle"),TEXT("타이틀 화면"),X+28,Y+246,334);
  Canvas->Canvas->PopTransform();return;
 }
 if(IsDebriefOpen()) { DrawDebrief(); Canvas->Canvas->PopTransform(); return; }
 DrawPlayHUD(); DrawFacilityAlerts(); DrawWorldIndicators(); DrawPhase2C(); DrawMission();
 DrawBattleNotices();
 DrawPinned();
 if(!PC->bDeveloperMode) { DrawDetails(); DrawInterior(); if(!bStarMapOpen)DrawShipOperations(); DrawStarMap(); }
 if(!bStarMapOpen && !bSceneMenuOpen && !bChoosingTransport && !bChoosingTransfer) DrawPractice();
 float MouseX=0,MouseY=0;
 if(DraggedPin==INDEX_NONE && PC->GetMousePosition(MouseX,MouseY) && !IsOverAddedUI(MouseX,MouseY)) {
  const FVector2D Mouse(MouseX/UIScale,MouseY/UIScale);
  FString Name=GetPinnedTitleTooltip(Mouse);
  if(Name.IsEmpty())Name=GetPinnedFacilityTooltip(Mouse);
  if(Name.IsEmpty())Name=GetFleetTitleTooltip(Mouse);
  if(Name.IsEmpty()&&!bDragging&&!bResizing)Name=GetDetailsTitleTooltip(Mouse);
  if(Name.IsEmpty()&&!bDragging&&!bResizing)Name=GetCommandTitleTooltip(Mouse);
  if(!Name.IsEmpty()) {
   const float Width=FMath::Min(300.f,ViewportPixels.X/UIScale-8.f);
   TArray<FString> Lines;FString Line;
   for(const TCHAR Character:Name) {
    const FString Candidate=Line+FString::Chr(Character);float TW=0,TH=0;
    GetTextSize(Candidate,TW,TH,OVTypography::Font(EOVTextRole::Body));
    if(TW>Width-12&&!Line.IsEmpty()) {
     Lines.Add(Line.TrimEnd());Line=FChar::IsWhitespace(Character)?FString():FString::Chr(Character);
    } else Line=Candidate;
   }
   if(!Line.IsEmpty())Lines.Add(Line);
   const float Height=FMath::Min(float(Lines.Num()*19+10),ViewportPixels.Y/UIScale-8.f);
   const float X=FMath::Clamp(float(Mouse.X+12),4.f,FMath::Max(4.f,float(ViewportPixels.X/UIScale-Width-4)));
   const float Y=FMath::Clamp(float(Mouse.Y+16),4.f,FMath::Max(4.f,float(ViewportPixels.Y/UIScale-Height-4)));
   DrawRect(Panel,X,Y,Width,Height);
   for(int32 I=0;I<Lines.Num() && 5+I*19+16<=Height;++I) {
    float TW=0,TH=0;GetTextSize(Lines[I],TW,TH,OVTypography::Font(EOVTextRole::Body));
    DrawText(Lines[I],Ink,X+6,Y+5+I*19,OVTypography::Font(EOVTextRole::Body),FMath::Min(1.f,(Width-12)/FMath::Max(1.f,TW)));
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
 DrawText(TEXT("2B / LIVE FIRE CONTROL"),Accent,20,226,OVTypography::Font(EOVTextRole::Body));
 DrawText((TEXT("Target: ")+(Ship->AttackTarget?Ship->AttackTarget->ShipName:TEXT("NONE"))).Left(58),Ink,20,247,OVTypography::Font(EOVTextRole::Body));
 const FOVDetectionInfo Detection=C->GetDetection(PC->Selected,Ship->AttackTarget);
 DrawText(FString::Printf(TEXT("Sensor: %s / bonus +%.1f%%"),*Detection.Source,Detection.AccuracyBonus*100),Ink,20,267,OVTypography::Font(EOVTextRole::Numeric));
 const auto& Health=Ship->Systems->WeaponMountStates;
 const FName SelectedId=Health.IsValidIndex(PC->SelectedWeapon)?Health[PC->SelectedWeapon].WeaponMountId:NAME_None;
 if(const auto* Mount=State.Mounts.FindByPredicate([SelectedId](const FOVCombatMountState& M){return M.MountId==SelectedId;})) {
  const auto& M=*Mount;
  const auto R=C->GetReadiness(PC->Selected,M.MountId);
  DrawText(FString::Printf(TEXT("%s / %s"),*M.MountId.ToString(),*M.Profile.ProfileId.ToString()),Accent,20,287,OVTypography::Font(EOVTextRole::Body));
  DrawText(FString::Printf(TEXT("%s | CD %.2fs | aim %.1f deg"),*R.Reason,R.Cooldown,R.AimError),R.bReady?FLinearColor::Green:FLinearColor::Yellow,20,307,OVTypography::Font(EOVTextRole::Numeric));
  const FString AimQuality=M.Profile.Kind==EOVAttackKind::Beam?FString::Printf(TEXT("hit %.1f%%"),100*R.Accuracy):FString::Printf(TEXT("spread %.2f deg"),R.AimSpreadDegrees);
  DrawText(FString::Printf(TEXT("%.0f/%.0f cm | %s | queued %s"),R.Distance,M.Profile.Range,*AimQuality,M.bFireRequested?TEXT("YES"):TEXT("NO")),Ink,20,327,OVTypography::Font(EOVTextRole::Numeric));
 }
 Button(TEXT("Fire"),TEXT("B FIRE"),20,351,90);
 Button(TEXT("FireAll"),TEXT("L FIRE ALL"),118,351,105);
 Button(TEXT("NextWeapon"),TEXT("N NEXT GUN"),231,351,110);
 Button(TEXT("AutoFree"),TEXT("AUTO FREE"),349,351,81);
 DrawText(FString::Printf(TEXT("Shots %d / hits %d / flying %d"),C->ShotCount,C->HitCount,C->Attacks.Num()),Ink,20,386,OVTypography::Font(EOVTextRole::Numeric));
 if(C->RecentEvents.Num()) {
  const auto& E=C->RecentEvents.Last();
  DrawText(FString::Printf(TEXT("#%d %s / hull -%.1f / blasts %d"),E.AttackId,*StaticEnum<EOVAttackResult>()->GetNameStringByValue(static_cast<int64>(E.Result)),E.HullDamage,E.Explosions),Accent,20,407,OVTypography::Font(EOVTextRole::Numeric));
 }
 DrawText(C->GetOrderStatus(PC->Selected),FLinearColor::Yellow,20,454,OVTypography::Font(EOVTextRole::Body));
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
  DrawText(TEXT("DEV injected own-faction interior state"),FLinearColor::Yellow,20,539,OVTypography::Font(EOVTextRole::Body));
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
 if(GConfig)
 {
  GConfig->GetInt(TEXT("OrbitalVanguard.UI"),TEXT("NoticeMask"),NoticeMask,GGameUserSettingsIni);
  float SavedScale=1.f;
  if(GConfig->GetFloat(TEXT("OrbitalVanguard.UI"),TEXT("UIScaleMultiplier"),SavedScale,GGameUserSettingsIni))
  {
   UserUIScaleMultiplier=FMath::Clamp(SavedScale,.80f,1.50f);
  }
 }
 TitleScreenWidget=CreateWidget<UOVTitleScreenWidget>(PlayerOwner,UOVTitleScreenWidget::StaticClass());
 if(TitleScreenWidget){TitleScreenWidget->AddToViewport(50);TitleScreenWidget->SetVisibility(ESlateVisibility::Collapsed);}
 const TCHAR* Cmd=FCommandLine::Get();
 int32 IgnoredInt=-1; FString IgnoredStr;
 const bool bForceTitle=FParse::Param(Cmd,TEXT("OVTitleScreen"));
 const bool bSkipTitle=FParse::Param(Cmd,TEXT("OVSkipTitle"))
  || FParse::Value(Cmd,TEXT("OVPlayScene="),IgnoredStr)
  || FParse::Value(Cmd,TEXT("OV2CScene="),IgnoredInt)
  || FParse::Value(Cmd,TEXT("OVMission="),IgnoredInt);
 const bool bIsPIE=GetWorld()&&GetWorld()->WorldType==EWorldType::PIE;
 if(bForceTitle || (!bIsPIE && !bSkipTitle))
 {
  OpenTitleScreen();
  FString InitialPanel;
  if(FParse::Value(Cmd,TEXT("OVTitlePanel="),InitialPanel))
  {
   if(InitialPanel.Equals(TEXT("ScenarioSelect"),ESearchCase::IgnoreCase)||InitialPanel.Equals(TEXT("MissionSelect"),ESearchCase::IgnoreCase))
   {
    if(TitleScreenWidget) { TitleScreenWidget->ShowPanel(EOVTitleMenuPanel::ScenarioSelect); }
   }
   else if(InitialPanel.Equals(TEXT("Settings"),ESearchCase::IgnoreCase))
   {
    if(TitleScreenWidget) { TitleScreenWidget->ShowPanel(EOVTitleMenuPanel::Settings); }
   }
   else if(InitialPanel.Equals(TEXT("Tutorial"),ESearchCase::IgnoreCase)||InitialPanel.Equals(TEXT("Guide"),ESearchCase::IgnoreCase))
   {
    if(TitleScreenWidget) { TitleScreenWidget->ShowPanel(EOVTitleMenuPanel::Tutorial); }
   }
  }
 }
 SyncAddedUI();

 FString CapturePath;
 if(FParse::Value(Cmd,TEXT("OVCaptureTitleScreen="),CapturePath))
 {
  if (GEngine)
  {
   GEngine->bEnableOnScreenDebugMessages = false;
  }
  if (APlayerController* PC = GetOwningPlayerController())
  {
   PC->ConsoleCommand(TEXT("DisableAllScreenMessages"));
  }

  float CaptureScale;
  if (FParse::Value(Cmd,TEXT("OVCaptureUIScale="),CaptureScale))
   SetUserUIScaleMultiplier(CaptureScale,false);
  FTimerHandle CaptureTimer;
  GetWorldTimerManager().SetTimer(CaptureTimer,[this,CapturePath]()
  {
   int32 Width=0,Height=0;
   GetOwningPlayerController()->GetViewportSize(Width,Height);
   UE_LOG(LogTemp,Display,TEXT("OV capture: %dx%d, UI %.2f, %s"),Width,Height,UserUIScaleMultiplier,*CapturePath);
   FScreenshotRequest::RequestScreenshot(CapturePath,true,false);
   FScreenshotRequest::OnScreenshotRequestProcessed().AddLambda([]()
   {
    FGenericPlatformMisc::RequestExit(false);
   });
  },5.0f,false);

  FTimerHandle SafetyExitTimer;
  GetWorldTimerManager().SetTimer(SafetyExitTimer,[]()
  {
   FGenericPlatformMisc::RequestExit(false);
  },8.0f,false);
 }
}
void AOVHUD::EndPlay(const EEndPlayReason::Type Reason)
{
 if (IsValid(BasePresentation)) BasePresentation->Destroy();
 for (auto& Pair : SelectionPortraits) if (Pair.Value) Pair.Value->Release();
 SelectionPortraits.Reset();
 if(ShipPreview)ShipPreview->Release();
 if(WeaponPreview)WeaponPreview->Release();
 for(auto Preview:PinnedPreviews)if(Preview)Preview->Release();
 if(TitleScreenWidget)TitleScreenWidget->ReleaseBackdrop();
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
FString AOVHUD::GetPinnedAircraftSummary(int32 Index) const
{
 if(!PinnedShips.IsValidIndex(Index)||!PinnedShips[Index].IsValid()||!PinnedAircraftIds.IsValidIndex(Index)||GetPinnedCarrierSummary(Index).IsEmpty())return FString();
 const auto* Wing=GetWorld()?GetWorld()->GetSubsystem<UOVCarrierSubsystem>():nullptr;const auto* Ship=PinnedShips[Index].Get();
 if(!Wing||!Ship||!Ship->Systems)return FString();
 const auto* Craft=Wing->Aircraft.FindByPredicate([&](const FOVAircraftRecord& A){return A.AircraftId==PinnedAircraftIds[Index]&&A.Carrier==Ship&&A.FactionId==Ship->Systems->FactionId;});
 if(!Craft)return FString();
 const TCHAR* Names[]={TEXT("보관"),TEXT("출격"),TEXT("귀환"),TEXT("도킹 대기"),TEXT("관제 상실"),TEXT("완파")};
 return FString::Printf(TEXT("%s · 내구 %.0f/%.0f"),Names[FMath::Clamp(int32(Craft->State),0,5)],Craft->HP,Wing->Profile.Hull);
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
 const float NoticeX=ViewportPixels.X/UIScale-354;
 if(Count<=0||PinnedPanelX()+266<=NoticeX||PinnedPanelX()>=NoticeX+340)return 76;
 const auto* Feed=GetBattleNoticeFeed();
 const int32 Footer=bBattleFeedOpen?(bNoticeSettings?242:66)+(Feed&&Feed->DroppedCount>0?20:0):0;
 return FMath::Max(76.f,BattleNoticeTop()+Count*32+Footer+4.f);
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
FString AOVHUD::GetPinnedFacilityTooltip(FVector2D CanvasPoint) const
{
 const auto* PC=Cast<AOVController>(PlayerOwner);
 if(!PC||PC->bDeveloperMode||bHeroPanelOpen)return FString();
 TArray<FString> Parts;GetHitActionAtCanvasPoint(CanvasPoint).ToString().ParseIntoArray(Parts,TEXT("_"));
 if(Parts.Num()!=3||Parts[0]!=TEXT("PinnedSlot")||!Parts[1].IsNumeric()||!Parts[2].IsNumeric())return FString();
 const int32 Pin=FCString::Atoi(*Parts[1]),Index=FCString::Atoi(*Parts[2]);
 if(!PinnedShips.IsValidIndex(Pin)||!PinnedShips[Pin].IsValid()||!PinnedDecks.IsValidIndex(Pin))return FString();
 const auto* S=PinnedShips[Pin]->Systems.Get();
 if(!S||S->FactionId!=0||!S->Layout||!S->Layout->Slots.IsValidIndex(Index)||!S->Slots.IsValidIndex(Index))return FString();
 const auto& Def=S->Layout->Slots[Index];const auto& State=S->Slots[Index];
 if(Def.DeckId!=PinnedDecks[Pin])return FString();
 if(Def.Facility==EOVFacilityType::None)return TEXT("빈 공간");
 const FString Status=S->GetSlotStatus(Def.SlotId);
 const TCHAR* StatusText=Status==TEXT("Destroyed")?TEXT("파괴"):Status==TEXT("Occupied")?TEXT("점령됨"):
     Status==TEXT("Uncrewed")?TEXT("운영 인원 없음"):Status==TEXT("Operational")?TEXT("가동"):TEXT("상태 확인 불가");
 FString Text=FString::Printf(TEXT("%s · %s · 내구 %.0f/%.0f"),*Facility(Def.Facility),StatusText,State.HP,Def.MaxFacilityHP);
 if(Def.RequiredCrew>0)Text+=FString::Printf(TEXT(" · 운영 인원 %.0f/%.0f"),State.Crew,Def.RequiredCrew);
 return Text;
}
FString AOVHUD::GetDetailsTitleTooltip(FVector2D CanvasPoint) const
{
 const auto* PC=Cast<AOVController>(PlayerOwner);
 if(!PC||!PC->Selected||!bDetailsOpen||PC->bDeveloperMode||GetHitActionAtCanvasPoint(CanvasPoint)!=TEXT("DetailsTitle"))return FString();
 const int32 Limit=FMath::Clamp(FMath::FloorToInt((WindowSize.X-150)/12.f),4,24);
 return PC->Selected->ShipName.Len()>Limit?PC->Selected->ShipName:FString();
}
void AOVHUD::DrawPinned()
{
 const auto* PC=Cast<AOVController>(PlayerOwner);
 for(int32 I=PinnedShips.Num()-1;I>=0;--I)if(!PinnedShips[I].IsValid()||PinnedShips[I]->IsHidden()||(PC&&PC->Combat&&PC->Combat->IsDeparted(PinnedShips[I].Get())))
 {DraggedPin=INDEX_NONE;bPinResizing=false;PinnedPreviews[I]->Release();PinnedPreviews.RemoveAt(I);PinnedShips.RemoveAt(I);PinnedDecks.RemoveAt(I);PinnedSlots.RemoveAt(I);PinnedWeapons.RemoveAt(I);PinnedSlotPages.RemoveAt(I);PinnedAircraftIds.RemoveAt(I);PinnedAircraftPages.RemoveAt(I);PinnedPositions.RemoveAt(I);PinnedSizes.RemoveAt(I);}
 if(!PC||PC->bDeveloperMode||bHeroPanelOpen)return;
 for(int32 I=0;I<PinnedShips.Num();++I)
 {
  const FVector2D Pos=GetPinnedPosition(I),Size=GetPinnedSize(I);const float X=Pos.X,Y=Pos.Y,W=Size.X,H=Size.Y;auto* Ship=PinnedShips[I].Get();
  DrawRect(Panel,X,Y,W,H);
  AddHitBox(FVector2D(X,Y)*UIScale,Size*UIScale,FName(*FString::Printf(TEXT("PinnedBackground_%d"),I)),true,5);
  Button(FName(*FString::Printf(TEXT("PinnedGrip_%d"),I)),TEXT("이동"),X+4,Y+4,38);
  const FString Title=FitLabel(OVTypography::Font(EOVTextRole::Button),ShipDisplayName(Ship),W-172);
  Button(FName(*FString::Printf(TEXT("Pinned_%d"),I)),Title,X+46,Y+4,W-158,false,EOVTextRole::Button,ShipNameColor(Ship));
  Button(FName(*FString::Printf(TEXT("PinnedClose_%d"),I)),TEXT("X"),X+W-38,Y+4,32);
  const auto* Systems=Ship->Systems.Get();const auto* Layout=Systems?Systems->Layout.Get():nullptr;
  if(!Layout)continue;
  if(!Layout->WeaponMounts.IsValidIndex(PinnedWeapons[I])||!Systems->WeaponMountStates.IsValidIndex(PinnedWeapons[I]))PinnedWeapons[I]=INDEX_NONE;
  const auto* Carrier=GetWorld()->GetSubsystem<UOVCarrierSubsystem>();
  const bool HasHangar=Systems->FactionId==0&&Carrier&&Carrier->Hangars.ContainsByPredicate([&](const FOVHangarState& Hangar){return Hangar.Carrier==Ship;});
  if(!HasHangar&&PinnedDecks[I]<0){PinnedDecks[I]=0;PinnedAircraftIds[I]=0;PinnedAircraftPages[I]=0;}
  Button(FName(*FString::Printf(TEXT("PinnedDeck_%d_0"),I)),TEXT("외부"),X+6,Y+35,55,PinnedDecks[I]==0);
  if(Systems->FactionId==0)for(int32 D=1;D<=Layout->DeckCount;++D)
   Button(FName(*FString::Printf(TEXT("PinnedDeck_%d_%d"),I,D)),FString::Printf(TEXT("%dF"),D),X+64+(D-1)*45,Y+35,40,PinnedDecks[I]==D);
  if(HasHangar)Button(FName(*FString::Printf(TEXT("PinnedHangar_%d"),I)),TEXT("항공대"),X+W-62,Y+35,56,PinnedDecks[I]<0);
  if(PinnedDecks[I]==0) {
   auto* Preview=PinnedPreviews[I].Get();
   if(Systems->bWrecked)Preview->Release();
   else Preview->Update(Ship,PinnedWeapons[I]==INDEX_NONE?UOVShipPreview::DefaultHullView():FRotator(-25,35,0),1,(W-12)/FMath::Max(1.f,H-83),PinnedWeapons[I],FMath::CeilToInt((W-12)*UIScale));
   if(auto* Texture=Preview->GetTexture())DrawTexture(Texture,X+6,Y+64,W-12,H-83,0,0,1,1,FLinearColor::White,BLEND_Opaque);
   else if(Systems->bWrecked)DrawText(TEXT("함선 완파"),Ink,X+12,Y+72,OVTypography::Font(EOVTextRole::Body));
   if(Layout->WeaponMounts.Num()>0)Button(FName(*FString::Printf(TEXT("PinnedWeaponNext_%d"),I)),TEXT("무장 +"),X+W-62,Y+(HasHangar?65:35),56,PinnedWeapons[I]>=0);
   if(PinnedWeapons[I]>=0) {
    TArray<FString> Lines;GetWeaponSummary(Ship,PinnedWeapons[I]).ParseIntoArrayLines(Lines);
    if(Lines.Num()>0)DrawText(Lines[0],Ink,X+10,Y+65,OVTypography::Font(EOVTextRole::Body),.9f);
    if(Lines.Num()>1)DrawText(Lines[1],Ink,X+10,Y+H-14,OVTypography::Font(EOVTextRole::Body),.9f);
   } else DrawText(FString::Printf(TEXT("선체 %.0f%% · 쉴드 %.0f%%"),100*Systems->HullHP/FMath::Max(1.f,Systems->MaxHullHP),100*Systems->Shield/FMath::Max(1.f,Systems->MaxShield)),Ink,X+10,Y+H-14,OVTypography::Font(EOVTextRole::Numeric));
  } else if(PinnedDecks[I]==-1&&Systems->FactionId==0) {
   if(PinnedPreviews[I])PinnedPreviews[I]->Release();
   TArray<FString> Lines;GetPinnedCarrierSummary(I).ParseIntoArrayLines(Lines);
   for(int32 Line=0;Line<Lines.Num();++Line)DrawText(Lines[Line],Ink,X+10,Y+64+Line*15,OVTypography::Font(EOVTextRole::Body),.9f);
   if(HasHangar) {
    Button(FName(*FString::Printf(TEXT("PinnedLaunch_%d"),I)),TEXT("출격"),X+10,Y+H-30,68);
    Button(FName(*FString::Printf(TEXT("PinnedRecall_%d"),I)),TEXT("귀환"),X+84,Y+H-30,68);
    Button(FName(*FString::Printf(TEXT("PinnedAirGrid_%d"),I)),TEXT("기체 보기"),X+W-96,Y+H-30,86);
   }
  } else if(PinnedDecks[I]==-2&&HasHangar) {
   if(PinnedPreviews[I])PinnedPreviews[I]->Release();
   TArray<const FOVAircraftRecord*,TInlineAllocator<64>> Aircraft;
   for(const auto& A:Carrier->Aircraft)if(A.Carrier==Ship&&A.FactionId==Systems->FactionId)Aircraft.Add(&A);
   PinnedAircraftPages[I]=FMath::Clamp(PinnedAircraftPages[I],0,FMath::Max(0,(Aircraft.Num()-1)/5));
   if(!Aircraft.ContainsByPredicate([&](const FOVAircraftRecord* A){return A->AircraftId==PinnedAircraftIds[I];}))PinnedAircraftIds[I]=0;
   const TCHAR* Labels[]={TEXT("보관"),TEXT("출격"),TEXT("귀환"),TEXT("대기"),TEXT("상실"),TEXT("완파")};
   const float CW=(W-16)/5.f;
   for(int32 Cell=0;Cell<5;++Cell) {
    const int32 AIndex=PinnedAircraftPages[I]*5+Cell;if(!Aircraft.IsValidIndex(AIndex))break;
    const auto& A=*Aircraft[AIndex];const float CX=X+8+Cell*CW;
    Button(FName(*FString::Printf(TEXT("PinnedAirCard_%d_%d"),I,A.AircraftId)),FString::Printf(TEXT("%d %s"),AIndex+1,Labels[FMath::Clamp(int32(A.State),0,5)]),CX,Y+64,CW-3,A.AircraftId==PinnedAircraftIds[I]);
    DrawRect(Health(A.State==EOVAircraftState::Wrecked?0:A.HP/FMath::Max(1.f,Carrier->Profile.Hull)),CX,Y+92,(CW-3)*FMath::Clamp(A.HP/FMath::Max(1.f,Carrier->Profile.Hull),0.f,1.f),3);
   }
   const FString Summary=GetPinnedAircraftSummary(I);
   DrawText(Summary.IsEmpty()?TEXT("기체를 선택하세요"):Summary,Ink,X+10,Y+101,OVTypography::Font(EOVTextRole::Body),.9f);
   Button(FName(*FString::Printf(TEXT("PinnedAirPage_%d_-1"),I)),TEXT("이전"),X+8,Y+H-30,48);
   Button(FName(*FString::Printf(TEXT("PinnedAirPage_%d_1"),I)),TEXT("다음"),X+59,Y+H-30,48);
   DrawText(FString::Printf(TEXT("%d/%d"),PinnedAircraftPages[I]+1,FMath::Max(1,FMath::DivideAndRoundUp(Aircraft.Num(),5))),Ink,X+112,Y+H-23,OVTypography::Font(EOVTextRole::Numeric),.8f);
   Button(FName(*FString::Printf(TEXT("PinnedAirBack_%d"),I)),TEXT("현황"),X+W-65,Y+H-30,55);
  } else if(Systems->FactionId==0) {
   if(PinnedPreviews[I])PinnedPreviews[I]->Release();
   int32 DeckSlots=0;for(int32 S=0;S<Layout->Slots.Num()&&S<Systems->Slots.Num();++S)if(Layout->Slots[S].DeckId==PinnedDecks[I])++DeckSlots;
   PinnedSlotPages[I]=FMath::Clamp(PinnedSlotPages[I],0,FMath::Max(0,(DeckSlots-1)/15));
   if(DeckSlots>15) {
    Button(FName(*FString::Printf(TEXT("PinnedPage_%d_-1"),I)),TEXT("<"),X+W-108,Y+4,26);
    Button(FName(*FString::Printf(TEXT("PinnedPage_%d_1"),I)),TEXT(">"),X+W-68,Y+4,26);
    DrawText(FString::FromInt(PinnedSlotPages[I]+1),Ink,X+W-80,Y+9,OVTypography::Font(EOVTextRole::Body),.8f);
   }
   int32 Cell=0,Seen=0;
   for(int32 S=0;S<Layout->Slots.Num()&&S<Systems->Slots.Num()&&Cell<15;++S)if(Layout->Slots[S].DeckId==PinnedDecks[I]) {
    if(Seen++<PinnedSlotPages[I]*15)continue;
    const auto& Def=Layout->Slots[S];const auto& Slot=Systems->Slots[S];
    const float CW=(W-14)/5,CH=(H-83)/3;
    const float CX=X+7+(Cell%5)*CW,CY=Y+65+(Cell/5)*CH;++Cell;
    const FName Action(*FString::Printf(TEXT("PinnedSlot_%d_%d"),I,S));
    DrawRect(S==PinnedSlots[I]?Accent:FLinearColor(.04f,.08f,.13f),CX,CY,CW-3,CH-4);
    DrawText(Def.Facility==EOVFacilityType::None?TEXT("-"):Facility(Def.Facility).Left(2),Ink,CX+3,CY+1,OVTypography::Font(EOVTextRole::Body));
    DrawRect(Health(Def.MaxFacilityHP>0?Slot.HP/Def.MaxFacilityHP:0),CX,CY+CH-7,CW-3,3);
    AddHitBox(FVector2D(CX,CY)*UIScale,FVector2D(CW-3,CH-4)*UIScale,Action,true,10);ActionCenters.Add(Action,FVector2D(CX+(CW-3)*.5f,CY+(CH-4)*.5f));
   }
   const int32 S=PinnedSlots[I];
   if(Layout->Slots.IsValidIndex(S)&&Systems->Slots.IsValidIndex(S))DrawText(FString::Printf(TEXT("%s · 내구 %.0f/%.0f"),*Facility(Layout->Slots[S].Facility),Systems->Slots[S].HP,Layout->Slots[S].MaxFacilityHP),Ink,X+10,Y+H-14,OVTypography::Font(EOVTextRole::Numeric));
   else DrawText(TEXT("시설을 선택하세요"),Ink,X+10,Y+H-14,OVTypography::Font(EOVTextRole::Body));
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

void AOVHUD::ResetPlaySessionUI()
{
 if (bBaseViewOpen) ToggleBaseView();
 if (IsValid(BasePresentation)) BasePresentation->Destroy();
 BasePresentation=nullptr; BaseInspect=INDEX_NONE; BasePick=0; BaseLayer=BaseTurns=0; BaseNotice.Reset();
 BaseWindowOrder.Reset(); BaseDragWindow=INDEX_NONE;
 for(auto& Window:BaseWindows) Window=FBaseWindow{};
 BasePage=BaseRemoval=0; BaseRouteCell.Reset(); bBaseAutoConnect=false;
 bChoosingTransport=bChoosingTransfer=bAircraftOnly=false;BoardingSelection.Reset();PendingBay=0;BoardingOrigin.Reset();BoardingShuttle.Reset();
 bChoosingJump=false;bPauseMenu=false;bDebriefDismissed=false;
 CloseTitleScreen();
 CancelPointerDrag();
 SetDetailsOpen(false);
 SetHeroPanelOpen(false);
 bInteriorOpen=false;
 bStarMapOpen=false;
 bBattleFeedOpen=false;
 bHangarView=false;
 FleetPage=AircraftPage=BattleFeedPage=0;
 DetailsPageShip.Reset();
 AircraftPageShip.Reset();
 SelectedAircraftId=0;
 if(ShipPreview) ShipPreview->Release();
 if(WeaponPreview) WeaponPreview->Release();
 for(auto& Preview:PinnedPreviews) if(Preview) Preview->Release();
 PinnedPreviews.Reset();
 PinnedShips.Reset();
 PinnedDecks.Reset();
 PinnedSlots.Reset();
 PinnedWeapons.Reset();
 PinnedSlotPages.Reset();
 PinnedAircraftIds.Reset();
 PinnedAircraftPages.Reset();
 PinnedPositions.Reset();
 PinnedSizes.Reset();
}
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
 if(DevelopmentWidget){DevelopmentWidget->SetFrameState(bDevelopmentMinimized,bDevelopmentMaximized);DevelopmentWidget->SetPositionInViewport(DevPos/Dpi,false);DevelopmentWidget->SetDesiredSizeInViewport(DevSize/Dpi);DevelopmentWidget->SetVisibility(PC->bDeveloperMode&&!bBaseViewOpen?ESlateVisibility::Visible:ESlateVisibility::Collapsed);}
 if(TitleScreenWidget){
  TitleScreenWidget->SetPositionInViewport(FVector2D::ZeroVector,false);
  TitleScreenWidget->SetDesiredSizeInViewport(FVector2D(W/Dpi,H/Dpi));
  TitleScreenWidget->SetVisibility(bTitleScreenOpen?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
 }
 if(bTitleScreenOpen || bPauseMenu || IsDebriefOpen() || PickingShip()){
  if(DevelopmentWidget)DevelopmentWidget->SetVisibility(ESlateVisibility::Collapsed);
  if(HeroWidget)HeroWidget->SetVisibility(ESlateVisibility::Collapsed);
  return;
 }
 if(HeroWidget){HeroWidget->SetPositionInViewport(HeroPos/Dpi,false);HeroWidget->SetDesiredSizeInViewport(HeroSize/Dpi);HeroWidget->SetVisibility(PC->bDeveloperMode||!bHeroPanelOpen||bBaseViewOpen?ESlateVisibility::Collapsed:ESlateVisibility::Visible);}
}
bool AOVHUD::BeginFleetPreparation(FName Id,bool Edit)
{
 auto* pc=Cast<AOVController>(PlayerOwner);
 if(!TitleScreenWidget||!pc||!pc->GetPlayScenario()->OpenSortie(pc,Id,Edit))return false;
 OpenTitleScreen();TitleScreenWidget->ShowPanel(EOVTitleMenuPanel::Sortie);TitleScreenWidget->RefreshSortie();return true;
}
void AOVHUD::OpenTitleScreen()
{
 bTitleScreenOpen=true;
 bSceneMenuOpen=false;
 if(auto* PC=Cast<AOVController>(PlayerOwner)) { PC->bSimulationPaused=true; }
 if(TitleScreenWidget) { TitleScreenWidget->ActivateTitleScreen(); }
 SyncAddedUI();
}
void AOVHUD::OpenTutorialGuide()
{
 bTitleScreenOpen=true;
 bSceneMenuOpen=false;
 if(auto* PC=Cast<AOVController>(PlayerOwner)) { PC->bSimulationPaused=true; }
 if(TitleScreenWidget) { TitleScreenWidget->ActivateInGameTutorial(); }
 SyncAddedUI();
}
void AOVHUD::SetUserUIScaleMultiplier(float NewScale, bool bPersist)
{
 UserUIScaleMultiplier=FMath::Clamp(NewScale,.80f,1.50f);
 if(bPersist && GConfig)
 {
  GConfig->SetFloat(TEXT("OrbitalVanguard.UI"),TEXT("UIScaleMultiplier"),UserUIScaleMultiplier,GGameUserSettingsIni);
  GConfig->Flush(false,GGameUserSettingsIni);
 }
}
void AOVHUD::CloseTitleScreen()
{
 if(!bTitleScreenOpen) return;
 bTitleScreenOpen=false;
 if(TitleScreenWidget) { TitleScreenWidget->DeactivateTitleScreen(); }
 if(auto* PC=Cast<AOVController>(PlayerOwner)) {
  if(PC->RangeCamera) PC->SetViewTarget(PC->RangeCamera);
  if(PauseBeforeCards.IsSet()) PC->bSimulationPaused=PauseBeforeCards.GetValue();
 }
 PauseBeforeCards.Reset();
 SyncAddedUI();
}
bool AOVHUD::IsOverAddedUI(float X,float Y) const
{
 if(bTitleScreenOpen) return true;
 const auto* PC=Cast<AOVController>(PlayerOwner);if(!PC)return false;const FVector2D P(X,Y);
 return PC->bDeveloperMode?DevelopmentWidget&&DevelopmentRect.IsInsideOrOn(P):bHeroPanelOpen&&HeroWidget&&HeroRect.IsInsideOrOn(P);
}
void AOVHUD::TogglePauseMenu()
{
 auto* PC=Cast<AOVController>(PlayerOwner); if(!PC)return;
 if(bPauseMenu) { PC->bSimulationPaused=PausedBeforeMenu; bPauseMenu=false;bConsumeDebriefClick=true; }
 else { PausedBeforeMenu=PC->bSimulationPaused;PC->bSimulationPaused=true;bPauseMenu=true;bSceneMenuOpen=false;PC->Action(TEXT("Cancel")); }
}
bool AOVHUD::CloseTopModal()
{
 if(PickingShip()){CastChecked<AOVController>(PlayerOwner)->InputMode=EOVInputMode::None;return true;}
 if(bChoosingTransport||bChoosingTransfer){bChoosingTransport=bChoosingTransfer=false;return true;}
 if(IsDebriefOpen()) { bDebriefDismissed=true; return true; }
 if (bTitleScreenOpen)
 {
  if (TitleScreenWidget && TitleScreenWidget->GetCurrentPanel()!=EOVTitleMenuPanel::Title)
   TitleScreenWidget->BackFromCurrentPanel();
  else CloseTitleScreen();
  return true;
 }
 if (bStarMapOpen) { bStarMapOpen=false; return true; }
 if(bPauseMenu){TogglePauseMenu();return true;}
 if(bSceneMenuOpen){bSceneMenuOpen=false;return true;}
 if(bHeroPanelOpen){SetHeroPanelOpen(false);return true;}
 if(bInteriorOpen){bInteriorOpen=false;return true;}
 if(bDetailsOpen){SetDetailsOpen(false);return true;}
 if(bBattleFeedOpen){bBattleFeedOpen=false;return true;}
 return false;
}
bool AOVHUD::IsAddedUIEditing() const
{
 const auto* PC=Cast<AOVController>(PlayerOwner);return PC&&PC->bDeveloperMode&&DevelopmentWidget&&DevelopmentWidget->IsEditing();
}
