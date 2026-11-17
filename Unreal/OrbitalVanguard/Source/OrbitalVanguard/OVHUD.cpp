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
 if(!PC->bDeveloperMode&&!bSceneMenuOpen&&PC->Phase2CScenario>=0&&P.X>=212&&P.X<=484&&P.Y>=76&&P.Y<=224)return true;
 if(!PC->bDeveloperMode&&!bHeroPanelOpen&&!bDetailsOpen&&PinnedShips.Num()>0&&P.X>=ViewportPixels.X/UIScale-278&&P.Y>=76&&P.Y<=76+PinnedShips.Num()*170)return true;
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
 if(PC&&PC->bDeveloperMode){bDragging=false;bResizing=false;return Over;}
 // A drag begun in the existing window retains ownership; a new interior gesture consumes all buttons/wheel.
 if(!bDragging&&!bResizing&&IsOverInteriorPanel(X,Y)) return true;
 if(!PC||!PC->Selected||!bDetailsOpen) { bDragging=false;bResizing=false;return Over; }
 const FVector2D P(X/UIScale,Y/UIScale);
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
 if(N==TEXT("MissionBackground")) return;
 if(N==TEXT("WindowBackground")) return;
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
  if(Index!=INDEX_NONE){PinnedShips.RemoveAt(Index);PinnedPreviews[Index]->Release();PinnedPreviews.RemoveAt(Index);}
  else if(PinnedShips.Num()<2){PinnedShips.Add(PC->Selected);PinnedPreviews.Add(NewObject<UOVShipPreview>(this));}
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
 DrawText(TEXT("중클릭 회전 · 휠 확대 · 무장 선택"),Ink,X+8,Y+174,GEngine->GetSmallFont());
}
FString AOVHUD::GetSelectedWeaponSummary() const
{
 const auto* PC=Cast<AOVController>(PlayerOwner);
 if(!PC||!PC->Selected||!PC->Selected->Systems||!PC->Selected->Systems->Layout)return TEXT("무장을 선택하세요");
 AOVShip* Ship=PC->Selected;
 const auto* Systems=Ship->Systems.Get();const auto* Layout=Systems->Layout.Get();
 if(!Systems->WeaponMountStates.IsValidIndex(PC->SelectedWeapon)||!Layout->WeaponMounts.IsValidIndex(PC->SelectedWeapon))return TEXT("무장을 선택하세요");
 const auto& HealthState=Systems->WeaponMountStates[PC->SelectedWeapon];
 const auto& Def=Layout->WeaponMounts[PC->SelectedWeapon];
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
     DrawText(FString::Printf(TEXT("격납고 %d · %s · %d / %d"),++I,Hangar.Mode==EOVHangarMode::Aircraft?TEXT("함재기"):TEXT("소형함"),Hangar.Mode==EOVHangarMode::Aircraft?Stored:(Hangar.StoredSmallShip.IsValid()?1:0),Hangar.Mode==EOVHangarMode::Aircraft?20+Hangar.ExtraAircraftCapacity:1),Ink,X+16,Y+105+(I-1)*19,GEngine->GetSmallFont());
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
 ViewportPixels=FVector2D(Canvas->SizeX,Canvas->SizeY);
 UIScale=FMath::Max(.25f,FMath::Min(1.f,FMath::Min(Canvas->SizeX/1280.f,Canvas->SizeY/740.f)));
 Canvas->Canvas->PushRelativeTransform(FScaleMatrix(FVector(UIScale,UIScale,1)));
 DrawPlayHUD(); DrawWorldIndicators(); DrawPhase2C(); DrawMission();
 DrawPinned();
 if(!PC->bDeveloperMode) { DrawDetails(); DrawInterior(); }
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
 for(auto Preview:PinnedPreviews)if(Preview)Preview->Release();
 Super::EndPlay(Reason);
}
int32 AOVHUD::GetPreviewMeshCount() const
{
 return ShipPreview?ShipPreview->GetMeshCount():0;
}
void AOVHUD::DrawPinned()
{
 const auto* PC=Cast<AOVController>(PlayerOwner);
 for(int32 I=PinnedShips.Num()-1;I>=0;--I)if(!PinnedShips[I].IsValid()||PinnedShips[I]->IsHidden()||(PC&&PC->Combat&&PC->Combat->IsDeparted(PinnedShips[I].Get())))
 {PinnedPreviews[I]->Release();PinnedPreviews.RemoveAt(I);PinnedShips.RemoveAt(I);}
 if(!PC||PC->bDeveloperMode||bHeroPanelOpen||bDetailsOpen)return;
 const float X=ViewportPixels.X/UIScale-278;
 for(int32 I=0;I<PinnedShips.Num();++I)
 {
  const float Y=76+I*170;auto* Ship=PinnedShips[I].Get();
  DrawRect(Panel,X,Y,266,164);
  Button(FName(*FString::Printf(TEXT("Pinned_%d"),I)),TEXT("핀 · ")+Ship->ShipName.Left(20),X+6,Y+4,254);
  auto* Preview=PinnedPreviews[I].Get();Preview->Update(Ship,FRotator(-25,35,0),1,2.4f);
  if(auto* Texture=Preview->GetTexture())DrawTexture(Texture,X+6,Y+35,254,106,0,0,1,1,FLinearColor::White,BLEND_Opaque);
  DrawText(FString::Printf(TEXT("선체 %.0f%% · 쉴드 %.0f%%"),100*Ship->Systems->HullHP/FMath::Max(1.f,Ship->Systems->MaxHullHP),100*Ship->Systems->Shield/FMath::Max(1.f,Ship->Systems->MaxShield)),Ink,X+10,Y+144,GEngine->GetSmallFont());
 }
}
void AOVHUD::SyncAddedUI()
{
 auto* PC=Cast<AOVController>(PlayerOwner);if(!PC)return;int32 W=0,H=0;PC->GetViewportSize(W,H);if(W<=0||H<=0)return;
 const float Dpi=FMath::Max(.01f,UWidgetLayoutLibrary::GetViewportScale(this));
 const FVector2D DevSize(FMath::Min(760.f,W*.8f),FMath::Min(530.f,H*.78f));
 const FVector2D DevPos((W-DevSize.X)*.5f,(H-DevSize.Y)*.5f);DevelopmentRect=FBox2D(DevPos,DevPos+DevSize);
 const FVector2D HeroSize(FMath::Min(400.f,W*.34f),FMath::Max(260.f,H-240.f));
 const FVector2D HeroPos(W-HeroSize.X-12.f,76.f);HeroRect=FBox2D(HeroPos,HeroPos+HeroSize);
 if(DevelopmentWidget){DevelopmentWidget->SetPositionInViewport(DevPos/Dpi,false);DevelopmentWidget->SetDesiredSizeInViewport(DevSize/Dpi);DevelopmentWidget->SetVisibility(PC->bDeveloperMode?ESlateVisibility::Visible:ESlateVisibility::Collapsed);}
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
