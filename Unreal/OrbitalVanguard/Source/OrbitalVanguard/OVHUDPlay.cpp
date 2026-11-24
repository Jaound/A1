#include "OVShipPreview.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/Texture2D.h"
#include "OVHUD.h"
#include "OVCalibration.h"
#include "OVController.h"
#include "OVPlayScenarioSubsystem.h"
#include "OVTransportSubsystem.h"
#include "OVLogisticsSubsystem.h"
#include "OVShip.h"
#include "OVShipSystemsComponent.h"
#include "OVCombatSubsystem.h"
#include "OVBattleNotices.h"
#include "OVInteriorSubsystem.h"
#include "OVCarrierSubsystem.h"
#include "OVHeroSubsystem.h"
#include "OVMissionSubsystem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Camera/PlayerCameraManager.h"

namespace {
const FLinearColor PlayInk(.8f,.9f,1), PlayAccent(.2f,.9f,1), PlayPanel(.008f,.02f,.04f,.94f);

}
const TCHAR* AOVHUD::AttackWeaponName(const FOVCombatMountState& Mount,const FOVWeaponMountDefinition* Def)
{
 if(Def && Def->IsAntiShipMissile())return TEXT("대함 미사일");
 switch(Mount.Profile.Kind){
  case EOVAttackKind::Cannon:return TEXT("함포");
  case EOVAttackKind::Beam:return TEXT("빔");
  case EOVAttackKind::GuidedMissile:return TEXT("유도 미사일");
  default:return TEXT("일반 미사일");
 }
}

FString AOVHUD::ReadinessKorean(const FString& Key)
{
 static const TMap<FString,FString> Labels={
  {TEXT("READY"),TEXT("발사 가능")},{TEXT("NO VALID TARGET"),TEXT("표적을 지정하세요")},
  {TEXT("OUT OF RANGE"),TEXT("사거리 밖")},{TEXT("NO DETECTION"),TEXT("탐지 상실")},
  {TEXT("MOUNT BLIND ARC"),TEXT("장착면 사각")},{TEXT("TURRET ALIGNING"),TEXT("포탑 조준 중")},
  {TEXT("FRIENDLY IN FLIGHT PATH"),TEXT("아군이 비행 경로에 있음")},{TEXT("FRIENDLY IN LINE"),TEXT("아군이 사선을 가림")},{TEXT("COOLDOWN"),TEXT("재사용 대기")},
  {TEXT("WEAPON CONTROL OFF"),TEXT("무기제어실 정지")},{TEXT("SUPPLY OFF"),TEXT("무장 공급 시설 정지")},
  {TEXT("MOUNT DESTROYED"),TEXT("무장 파괴")},{TEXT("FTL CHARGING"),TEXT("FTL 충전으로 빔 사용 불가")},
  {TEXT("INTERIOR ALLY HOLD"),TEXT("내부 아군 보호 · 사격 보류")},
  {TEXT("NO OBSERVED WEAPONS - WAIT"),TEXT("관측된 적 무장 없음 · 대기")},
  {TEXT("NO ACCESSIBLE WEAPONS - WAIT"),TEXT("적 무장이 선체에 가려짐 · 대기")},
  {TEXT("WRECKED"),TEXT("완파")},{TEXT("NEUTRAL"),TEXT("중립 함선")},{TEXT("DEPARTED"),TEXT("이탈")}};
 if(const FString* Value=Labels.Find(Key)) return *Value;
 return TEXT("사용 불가 · DEV에서 상세 원인 확인");
}

FVector2D AOVHUD::GetActionCenter(FName Action) const
{
 if(const FVector2D* Point=ActionCenters.Find(Action))return *Point;
 return FVector2D(-1,-1);
}
bool AOVHUD::IsOverWorldMarker(FVector2D Pixel) const
{
 return !WorldMarkerAt(Pixel).IsNone();
}
FName AOVHUD::WorldMarkerAt(FVector2D Pixel) const
{
 const auto* Hit=GetHitBoxAtCoordinates(Pixel,true);
 return Hit && Hit->GetName().ToString().StartsWith(TEXT("WorldPick_")) ? Hit->GetName() : NAME_None;
}

FString AOVHUD::GetFleetDisplayLabel(AOVShip* Ship) const
{
 const auto* PC=Cast<AOVController>(PlayerOwner);
 if(!PC||!Ship)return FString();
 int32 Same=0,Ordinal=0;
 for(const auto& Entry:PC->Ships) {
  const auto* Other=Entry.Get();
  if(!Other||Other->bFriendly!=Ship->bFriendly||Other->ShipName!=Ship->ShipName)continue;
  ++Same;if(Other==Ship)Ordinal=Same;
 }
 const FString& Full=Ship->ShipName;
 FString Name=Full;
 if(Full.Len()>14){
  int32 Space=INDEX_NONE;
  const FString Suffix=Full.FindLastChar(' ',Space) && Full.Mid(Space+1).IsNumeric() ? Full.Mid(Space) : FString();
  Name=Full.Left(FMath::Max(1,13-Suffix.Len()))+TEXT("…")+Suffix;
 }
 return Name+(Same>1?FString::Printf(TEXT(" %d"),Ordinal):FString());
}
FString AOVHUD::GetFleetTitleTooltip(FVector2D CanvasPoint) const
{
 const auto* PC=Cast<AOVController>(PlayerOwner);if(!PC)return FString();
 const FString Action=GetHitActionAtCanvasPoint(CanvasPoint).ToString();
 if(!Action.StartsWith(TEXT("Pick_")))return FString();
 for(int32 I=0;I<PC->Ships.Num();++I) {
  const auto* Ship=PC->Ships[I].Get();
  if(Ship&&Ship->bFriendly && Action==FString::Printf(TEXT("Pick_%d"),I))return Ship->ShipName;
 }
 return FString();
}
FString AOVHUD::GetCommandTitleTooltip(FVector2D CanvasPoint) const
{
 const auto* PC=Cast<AOVController>(PlayerOwner);if(!PC||!PC->Selected||PC->bDeveloperMode)return FString();
 const FName Action=GetHitActionAtCanvasPoint(CanvasPoint);
 if(Action==TEXT("CommandShipLabel")) {
  return bCommandShipClipped?PC->Selected->ShipName:FString();
 }
 if(Action==TEXT("CommandTargetLabel")&&PC->Selected->AttackTarget) {
  return bCommandTargetClipped?TEXT("표적: ")+PC->Selected->AttackTarget->ShipName:FString();
 }
 return FString();
}

FString AOVHUD::GetSelectedCargoLabel() const
{
 const auto* PC = Cast<AOVController>(PlayerOwner);
 const AOVShip* Ship = PC ? PC->Selected.Get() : nullptr;
 if (!IsValid(Ship) || !Ship->bFriendly || !Ship->Systems || Ship->Systems->FactionId != 0 || !Ship->Systems->Layout) return {};
 const bool HasStore = Ship->Systems->Layout->Slots.ContainsByPredicate([](const auto& Slot) {
  return Slot.Facility == EOVFacilityType::ResourceStorage;
 });
 if (!HasStore) return {};
 const auto* Logistics = GetWorld()->GetSubsystem<UOVLogisticsSubsystem>();
 const auto* Cargo = Logistics ? Logistics->FindCargo(Ship) : nullptr;
 if (!Cargo) return TEXT("적재 자원 0   ·   용량 미설정");
 const float Used = Cargo->Amount + 3.f * Cargo->Advanced + 8.f * Cargo->Rare;
 return FString::Printf(TEXT("적재 %.1f / %.1f    일반 %.1f   고급 %.1f   최고급 %.1f"),
  Used, Cargo->Capacity, Cargo->Amount, Cargo->Advanced, Cargo->Rare);
}

void AOVHUD::DrawDebrief()
{
 const auto* PC=CastChecked<AOVController>(PlayerOwner);
 const auto* Play=PC->GetPlayScenario();
 const float W=Canvas->SizeX/UIScale,H=Canvas->SizeY/UIScale;
 const float Width=FMath::Min(720.f,W-28), Height=256.f+Play->DebriefLines.Num()*24.f;
 const float X=(W-Width)*.5f,Y=(H-Height)*.5f;
 DrawRect(FLinearColor(0,0,.02f,.46f),0,0,W,H);
 DrawRect(FLinearColor(.12f,.25f,.34f),X-1,Y-1,Width+2,Height+2);
 DrawRect(FLinearColor(.006f,.015f,.029f,.98f),X,Y,Width,Height);
 const auto Tint=Play->Result==EOVPlaySceneResult::Defeat ? FLinearColor(1,.46f,.32f) : PlayAccent;
 DrawRect(Tint,X,Y,Width,3);
 DrawText(Play->GetActiveDefinition()->Title,PlayInk,X+24,Y+18,OVTypography::Font(EOVTextRole::Heading));
 DrawText(TEXT("작전 ")+Play->GetOutcomeTitle(),FLinearColor::White,X+24,Y+39,OVTypography::Font(EOVTextRole::Heading),2.05f);
 const int32 Seconds=FMath::FloorToInt(Play->ElapsedSeconds);
 DrawText(TEXT("작전 결과 보고"),PlayInk,X+Width-135,Y+25,OVTypography::Font(EOVTextRole::Heading));
 const FString Reason = Play->ActiveId == TEXT("Evacuation") && Play->Result != EOVPlaySceneResult::NoCombatCapability
  ? (Play->Result==EOVPlaySceneResult::Victory ? TEXT("함대 이탈 목표 달성") :
     Play->Result==EOVPlaySceneResult::PartialVictory ? TEXT("일부 함선이 퇴각에 성공했습니다") : TEXT("이탈에 성공한 아군 함선이 없습니다"))
  : Play->GetResolution();
 DrawText(Reason,PlayInk,X+24,Y+90,OVTypography::Font(EOVTextRole::Body));
 const FString Values[]={FString::Printf(TEXT("%02d:%02d"),Seconds/60,Seconds%60),FString::Printf(TEXT("%d / %d"),Play->SurvivingCost,Play->InitialCosts.IsValidIndex(0)?Play->InitialCosts[0]:0),FString::Printf(TEXT("%d기"),Play->SurvivingAircraft),FString::Printf(TEXT("%d척"),Play->DepartedCount)};
 const TCHAR* Captions[]={TEXT("작전 시간"),TEXT("아군 잔존 코스트"),TEXT("함재기 생존"),TEXT("초광속 이탈")};
 for(int32 I=0;I<4;++I){
  const float CW=(Width-48)/4,CX=X+24+I*CW;
  DrawRect(FLinearColor(.017f,.037f,.061f),CX,Y+118,CW-5,62);
  DrawRect(Tint,CX,Y+118,22,2);
  DrawText(Captions[I],PlayInk,CX+9,Y+124,OVTypography::Font(EOVTextRole::Body));
  DrawNumberRight(Values[I],FLinearColor::White,CX+CW-14,Y+146,1.27f);
 }
 float LineY=Y+192;
 for(const FString& Line : Play->DebriefLines) {
  DrawText(Line,PlayInk,X+24,LineY,OVTypography::Font(EOVTextRole::Body));
  LineY+=24;
 }
 Button(TEXT("ResultField"),TEXT("전장 보기"),X+24,Y+Height-48,88);
 Button(TEXT("RestartScene"),TEXT("다시 도전"),X+Width-128,Y+Height-48,104,true);
 Button(TEXT("PlaySceneExit"),TEXT("임무 목록"),X+Width-236,Y+Height-48,94);
}

void AOVHUD::DrawPlayHUD()
{
 auto* PC=Cast<AOVController>(PlayerOwner);
 const float W=Canvas->SizeX/UIScale,H=Canvas->SizeY/UIScale;
 const auto* Play=PC->GetPlayScenario();
 if(PC->bFleetDrag) {
  const FVector2D A=PC->FleetDragStart.ComponentMin(PC->FleetDragEnd)/UIScale;
  const FVector2D B=PC->FleetDragStart.ComponentMax(PC->FleetDragEnd)/UIScale;
  DrawRect(FLinearColor(.08f,.65f,1.f,.14f),A.X,A.Y,B.X-A.X,B.Y-A.Y);
  DrawLine(A.X,A.Y,B.X,A.Y,PlayAccent); DrawLine(B.X,A.Y,B.X,B.Y,PlayAccent);
  DrawLine(B.X,B.Y,A.X,B.Y,PlayAccent); DrawLine(A.X,B.Y,A.X,A.Y,PlayAccent);
 }
 if (Play && Play->ActiveId == TEXT("Logistics") && !bStarMapOpen && !bTitleScreenOpen && !bSceneMenuOpen) {
  FVector2D Mine, Depot;
  const bool bMine = PC->ProjectWorldLocationToScreen(Play->MiningSite(), Mine, true);
  const bool bDepot = PC->ProjectWorldLocationToScreen(Play->DeliverySite(), Depot, true);
  if (bMine) Mine /= UIScale;
  if (bDepot) Depot /= UIScale;
  if (bMine && bDepot) DrawLine(Mine.X, Mine.Y, Depot.X, Depot.Y, FLinearColor(.3f,.8f,.7f,.35f), 1);
  if (bMine) DrawText(TEXT("◇ 채굴 · 인계"), PlayAccent, Mine.X, Mine.Y - 27, OVTypography::Font(EOVTextRole::Body));
  if (bDepot) DrawText(TEXT("◇ 납품지"), PlayAccent, Depot.X, Depot.Y - 27, OVTypography::Font(EOVTextRole::Body));
 }
 DrawRect(PlayPanel,10,10,W-20,46);
 const float CommandWidths[]={88,74,88,49,49,49,96,76,94,91,77,94,76};
 float TX=16;const float Step=FMath::Min(1.28f,(W-26)/1004.f);
 auto Top=[&](int32 I,FName Id,const FString& Label,bool Active=false){
  const float BW=CommandWidths[I]*Step;Button(Id,Label,TX,18,BW-4,Active);TX+=BW;
 };
 Top(0,TEXT("SystemMenu"),TEXT("메뉴  ESC"));
 Top(1,TEXT("SceneMenu"),TEXT("임무"),bSceneMenuOpen);
 Top(2,TEXT("Pause"),PC->bSimulationPaused?TEXT("시작 ▷"):TEXT("정지 Ⅱ"),PC->bSimulationPaused);
 Top(3,TEXT("Speed1"),TEXT("1배"),PC->SimulationSpeed==1);Top(4,TEXT("Speed2"),TEXT("2배"),PC->SimulationSpeed==2);Top(5,TEXT("Speed3"),TEXT("3배"),PC->SimulationSpeed==3);
 Top(6,TEXT("Overview"),TEXT("전체 보기"));Top(7,TEXT("OpenTutorialGuide"),TEXT("가이드"));Top(8,TEXT("StarMapToggle"),TEXT("은하 지도"),bStarMapOpen);
 Top(9,TEXT("BattleFeedToggle"),TEXT("전투 알림"),bBattleFeedOpen);Top(10,TEXT("HeroPanel"),TEXT("영웅"),bHeroPanelOpen);Top(11,TEXT("Developer"),TEXT("개발 도구"),PC->bDeveloperMode);Top(12,TEXT("BaseView"),TEXT("기지"));
 if(LastNoticeSource!=PC->LastAction) {
  LastNoticeSource=PC->LastAction;NoticeExpires=FPlatformTime::Seconds()+3;
  NoticeText=PC->LastAction;
  int32 Separator=INDEX_NONE;if(NoticeText.FindChar(TCHAR('·'),Separator))NoticeText=NoticeText.Left(Separator).TrimEnd();
  if(NoticeText.StartsWith(TEXT("Forced")))NoticeText=TEXT("강제 사격 · 내부 아군 위험");
  if(NoticeText.StartsWith(TEXT("AUTO")))NoticeText=TEXT("자동화 변경");
  NoticeText=NoticeText.Left(24);
 }
 const float NoticeWidth = W-TX-10;
 if(FPlatformTime::Seconds()<NoticeExpires && NoticeWidth > 42)
  DrawText(FitLabel(OVTypography::Font(EOVTextRole::Body),NoticeText,NoticeWidth),PlayInk,TX,26,OVTypography::Font(EOVTextRole::Body));
 if(!bSceneMenuOpen && Play && Play->GetActiveDefinition()) {
  const bool Evac = Play->ActiveId == TEXT("Evacuation");
  const bool Sandbox=Play->ActiveId==TEXT("Sandbox");
  DrawRect(PlayPanel,0,63,Sandbox?426:508,Play->MissionPanelHeight());
  DrawRect(PlayAccent,0,63,4,Sandbox?140:110);
  if(!Sandbox){
   const FLinearColor Sig=Evac?FLinearColor(1,.70f,.32f):PlayAccent;
   DrawLine(14,83,23,74,Sig,2);DrawLine(23,74,32,83,Sig,2);DrawLine(32,83,23,92,Sig,2);DrawLine(23,92,14,83,Sig,2);
  }
  DrawText(Play->GetActiveDefinition()->Title,PlayAccent,Sandbox?13:39,68,OVTypography::Font(EOVTextRole::Heading),1.36f);
  if(Sandbox) {
   const bool Active=Play->bSandboxEngaged;
   const FString Tip=Play->SandboxHint.IsEmpty()?FString(TEXT("진영과 함종을 고른 뒤 위치 배치를 누르세요")):Play->SandboxHint;
   DrawText(FitLabel(OVTypography::Font(EOVTextRole::Body),Tip,404),PlayInk,10,98,OVTypography::Font(EOVTextRole::Body));
   if(!Active) {
    Button(TEXT("SBAlly"),TEXT("아군"),10,122,49,Play->SandboxFaction==0);
    Button(TEXT("SBEnemy"),TEXT("적군"),62,122,49,Play->SandboxFaction==1);
    Button(TEXT("SBMenu"),FString(TEXT("함종  "))+OVCalibration::GetHullName(Play->SandboxHull),116,122,124,Play->bSandboxHullMenu);
    Button(TEXT("SBCount"),FString::Printf(TEXT("%d척씩"),Play->SandboxCount),244,122,55);
    Button(TEXT("SBDown"),TEXT("−"),304,122,26);
    DrawText(FString::Printf(TEXT("%dm"),FMath::RoundToInt(Play->SandboxAltitude/100)),PlayInk,333,129,OVTypography::Font(EOVTextRole::Numeric));
    Button(TEXT("SBUp"),TEXT("+"),390,122,26);
    Button(TEXT("SBPlace"),Play->bSandboxPlacing?TEXT("배치 마침"):TEXT("위치 배치"),10,158,93,Play->bSandboxPlacing);
    Button(TEXT("SBRemove"),TEXT("선택 제거"),108,158,92);
    Button(TEXT("SBStart"),TEXT("교전 시작 ▷"),278,158,138,true);
    if(Play->bSandboxHullMenu) {
     static const TArray<FName> Hulls=OVCalibration::GetHullIds();
     for(int32 I=0;I<Hulls.Num();++I)
      Button(FName(*(FString(TEXT("SBHull_"))+Hulls[I].ToString())),OVCalibration::GetHullName(Hulls[I]),
             10+(I%3)*136,204+(I/3)*31,132,Hulls[I]==Play->SandboxHull);
    }
    if(Play->bSandboxHasPoint) {
     FVector2D P;
     if(PC->ProjectWorldLocationToScreen(Play->SandboxPoint,P)) {
      P/=UIScale;
      const auto Tint=!Play->bSandboxPointValid?FLinearColor(1,.22f,.12f):Play->SandboxFaction?FLinearColor(1,.64f,.24f):PlayAccent;
      DrawLine(P.X-12,P.Y,P.X+12,P.Y,Tint,2);DrawLine(P.X,P.Y-12,P.X,P.Y+12,Tint,2);
      DrawText(Play->bSandboxPointValid?(Play->SandboxFaction?TEXT("적군 배치"):TEXT("아군 배치")):TEXT("배치 불가"),Tint,P.X+16,P.Y+10,OVTypography::Font(EOVTextRole::Body));
     }
    }
   } else {
    DrawText(TEXT("중단하면 이동·사격·함재기·비행 중 탄이 멈춥니다"),PlayInk,10,129,OVTypography::Font(EOVTextRole::Body));
    Button(TEXT("SBStop"),TEXT("교전 중단 Ⅱ"),278,158,138,true);
   }
  } else {
   DrawText(Play->GetStatus(),PlayInk,14,100,OVTypography::Font(EOVTextRole::Body));
   if(Play->ActiveId!=TEXT("HeroArena"))DrawRect(FLinearColor(.12f,.21f,.29f),14,Evac?180:151,475,1);
   if(Play->ActiveId!=TEXT("HeroArena"))DrawText(FitLabel(OVTypography::Font(EOVTextRole::Body),Play->GetActiveDefinition()->Objective,470),PlayAccent,14,Evac?190:160,OVTypography::Font(EOVTextRole::Body));
  }
  if(Play->ActiveId==TEXT("HeroArena") && !Play->bArenaStarted) {
   Button(TEXT("ArenaPrev"),TEXT("◀"),12,122,30);
   Button(TEXT("ArenaNext"),TEXT("다음 ▶"),45,122,60);
   Button(TEXT("ArenaAdd"),TEXT("1명 배치"),108,122,92);
   Button(TEXT("ArenaFill"),TEXT("빈자리 채우기"),204,122,116);
   Button(TEXT("ArenaQuota"),FString(Play->ArenaHeroQuota==4?TEXT("보병 기본안"):TEXT("혼성 기본안")),324,122,95);
   Button(TEXT("ArenaLevel"),TEXT("생성 레벨"),423,122,76);
  }
  if(Play->ActiveId==TEXT("HeroArena") && Play->Result==EOVPlaySceneResult::Running){
   Button(TEXT("OpsArenaInterior"),TEXT("선내 지휘"),12,154,110);
   if(!Play->bArenaStarted)Button(TEXT("ArenaFormation"),Play->bArenaFormationOpen?TEXT("편성 닫기"):TEXT("팀 편성"),408,154,90,Play->bArenaFormationOpen);
   const float Wait=PC->Ships.Num()>=2?PC->Transport->NextTeamDelay(PC->Ships[1]):0;
   DrawText(Play->ArenaWaves[0].Sent && Play->ArenaWaves[1].Sent?FString(TEXT("두 팀 출발 완료")):Wait>.001f?FString::Printf(TEXT("다음 팀까지 %d초"),FMath::CeilToInt(Wait)):TEXT("목표당 두 팀 · 출발 간격 30초"),PlayInk,210,161,OVTypography::Font(EOVTextRole::Numeric));
   for(int32 I=0;I<2;++I){
    const auto& Team=Play->ArenaWaves[I];
    const float Row=188+I*35;
    DrawText(FString(I?TEXT("B팀  "):TEXT("A팀  "))+Play->ArenaWaveStatus(I),PlayInk,12,Row+5,OVTypography::Font(EOVTextRole::Body));
    if(Team.Sent){
     Button(I?TEXT("ArenaViewB"):TEXT("ArenaViewA"),TEXT("지휘"),354,Row,66);
     Button(I?TEXT("ArenaReturnB"):TEXT("ArenaReturnA"),TEXT("철수"),426,Row,66);
    }
   }
  }
  if(Play->bArenaFormationOpen && Play->ActiveId==TEXT("HeroArena") && !Play->bArenaStarted){
   const int32 Page=Play->ArenaFormationPage;
   DrawText(Play->ArenaSelectedGroup?Play->ArenaGroupLabel(Play->ArenaSelectedGroup)+TEXT(" → 목적지 선택"):TEXT("영웅 또는 분대 선택 → 배치할 열 선택"),PlayAccent,12,267,OVTypography::Font(EOVTextRole::Body));
   const TCHAR* Titles[]={TEXT("A팀"),TEXT("B팀"),TEXT("예비")};
   for(int32 Column=0;Column<3;++Column){
    const float X=12+Column*164;
    const auto& Groups=Play->ArenaGroups[Column];
    const FString Header=FString::Printf(TEXT("%s · %d명 %s"),Titles[Column],Play->ArenaGroupHeadcount(Column),
       Play->ArenaSelectedGroup?TEXT("← 배치"):TEXT(""));
    Button(FName(*FString::Printf(TEXT("ArenaAssign_%d"),Column)),Header,X,292,155,Play->ArenaSelectedGroup!=0);
    for(int32 Row=0;Row<4 && Groups.IsValidIndex(Page*4+Row);++Row){
     const int32 Key=Groups[Page*4+Row];
     Button(FName(*FString::Printf(TEXT("ArenaPick_%d"),Key)),FitLabel(OVTypography::Font(EOVTextRole::Button),Play->ArenaGroupLabel(Key),143),
            X,325+Row*30,155,Key==Play->ArenaSelectedGroup);
    }
   }
   Button(TEXT("ArenaPagePrev"),TEXT("이전"),12,451,64);
   DrawText(FString::Printf(TEXT("%d쪽"),Page+1),PlayInk,84,455,OVTypography::Font(EOVTextRole::Numeric));
   Button(TEXT("ArenaPageNext"),TEXT("다음"),128,451,64);
  }
  if (Play->ActiveId == TEXT("Logistics")) {
   DrawText(Play->GetConvoyStatus(), PlayInk, 10, 127, OVTypography::Font(EOVTextRole::Numeric));
   if (Play->Result == EOVPlaySceneResult::Running)
    Button(TEXT("ResumeConvoy"), TEXT("항로 복귀"), 336, 109, 82);

  }
  if(Evac && Play->Result == EOVPlaySceneResult::Running) {
   DrawText(PC->Selected ? Play->GetEvacuationStatus(PC->Selected) : FString(TEXT("아군 함대를 드래그해 선택하세요")),PlayInk,10,124,OVTypography::Font(EOVTextRole::Body));
   DrawText(TEXT("퇴로 개방 후 함선 명령에서 도약"),PlayInk,10,144,OVTypography::Font(EOVTextRole::Body));
   Button(TEXT("EvacAbandon"),TEXT("잔여기 포기"),246,142,90);
   if(Play->EscapedShips>0) Button(TEXT("EvacFinish"),TEXT("철수 종료"),338,142,80);
  }
  if(Play->Result!=EOVPlaySceneResult::Running) Button(TEXT("ShowResult"),TEXT("결과 보기"),167,69,89);
  Button(TEXT("RestartScene"),TEXT("재시작"),263,69,72);
  Button(TEXT("PlaySceneExit"),TEXT("목록 복귀"),341,69,75);
 }
 if(bSceneMenuOpen) {
  const float ListWidth=FMath::Min(684.f,W-226);
  DrawRect(PlayPanel,216,63,ListWidth,47+UOVPlayScenarioSubsystem::Definitions().Num()*36);
  DrawText(TEXT("임무 선택"),PlayAccent,226,71,OVTypography::Font(EOVTextRole::Heading));
  if(Play)Button(TEXT("CycleDifficulty"),Play->DifficultyLabel(),309,70,110);
  Button(TEXT("MissionCards"),TEXT("임무 카드 보기"),424,70,126);
  Button(TEXT("SceneMenuClose"),TEXT("닫기"),558,70,72,true);
  float Y=106;
  for(const auto& Definition:UOVPlayScenarioSubsystem::Definitions()) {
   if(Definition.bAvailable) Button(Definition.Id,Definition.Title,226,Y,157,Play&&Play->ActiveId==Definition.Id);
   else DrawText(Definition.Title,PlayInk,226,Y+6,OVTypography::Font(EOVTextRole::Body));
   const FString Description=Definition.bAvailable?FString(Definition.Objective):TEXT("준비 중");
   DrawText(FitLabel(OVTypography::Font(EOVTextRole::Body),Description,ListWidth-190),PlayInk,393,Y+6,OVTypography::Font(EOVTextRole::Body));
   Y+=36;
  }
 } else if(!bStarMapOpen && PC->ActiveEnvironment!=EOVSpaceEnvironmentType::None) {
  const float BX=W*0.5f-220.f,BY=60.f;
  DrawRect(FLinearColor(0.02f,0.05f,0.09f,0.92f),BX,BY,440,24);
  FString EnvStatus=FString::Printf(TEXT("우주 환경: %s"),*AOVController::GetEnvironmentDisplayName(PC->ActiveEnvironment));
  FLinearColor EnvColor=PlayAccent;
  if(PC->ActiveEnvironment==EOVSpaceEnvironmentType::SolarFlare) {
   if(PC->SolarFlareCycleTime>=9.f && PC->SolarFlareCycleTime<13.f) {
    EnvStatus=FString::Printf(TEXT("⚠ 태양 플레어 예고 (%.1fs) · 방향 +X→-X · 대형 운석 후방 엄폐 권고"),13.f-PC->SolarFlareCycleTime);
    EnvColor=FLinearColor(1.f,0.75f,0.2f);
   } else if(PC->SolarFlareCycleTime>=13.f && PC->SolarFlareCycleTime<16.f) {
    EnvStatus=TEXT("⚡ 태양 플레어 파동 통과 중 · 비엄폐 쉴드 부하 및 레이더 교란");
    EnvColor=FLinearColor(1.f,0.38f,0.2f);
   } else {
    EnvStatus=FString::Printf(TEXT("태양 플레어 주기 감시 중 (다음 경보까지 %.1fs)"),FMath::Max(0.f,9.f-PC->SolarFlareCycleTime));
   }
  } else if(PC->ActiveEnvironment==EOVSpaceEnvironmentType::AsteroidField) {
   EnvStatus=TEXT("운석지대 · 직사/투사체 물리 차폐 및 항로 우회 활성");
  } else if(PC->ActiveEnvironment==EOVSpaceEnvironmentType::RadiationZone) {
   EnvStatus=TEXT("우주 방사능 지대 · 구역 내 지속 방사능 하중 (활성 쉴드 우선 흡수)");
   EnvColor=FLinearColor(0.4f,1.f,0.75f);
  } else if(PC->ActiveEnvironment==EOVSpaceEnvironmentType::BlackHole) {
   EnvStatus=TEXT("소규모 블랙홀(E1) · 중력권 1,000m(0.8m/s²) · FTL 중에도 적용 · 45m 즉사");
   EnvColor=FLinearColor(0.95f,0.72f,1.f);
  }
  DrawText(EnvStatus,EnvColor,BX+10,BY+4,OVTypography::Font(EOVTextRole::Body));
 }
 const float PanelHeight=CommandPanelHeight();
 const float Y=H-PanelHeight-10;
 const float GridWidth=FleetPanelWidth(W), GX=W-GridWidth-10;
 const float GridY=H-218;
 TArray<AOVShip*,TInlineAllocator<32>> Friends;
 for(AOVShip* S:PC->Selection) if(IsValid(S)&&S->bFriendly && S->Systems && !S->Systems->bWrecked && (!PC->Combat||!PC->Combat->IsDeparted(S))) Friends.Add(S);
 if(!Friends.IsEmpty()){
  const int32 Cols=FMath::Max(1,FMath::FloorToInt((GridWidth-16)/110)),PerPage=Cols*2;
  const int32 Pages=FMath::DivideAndRoundUp(Friends.Num(),PerPage);
  FleetPage=FMath::Clamp(FleetPage,0,Pages-1);
  DrawRect(PlayPanel,GX,GridY,GridWidth,208);
  DrawText(FString::Printf(TEXT("선택 함대   %d척"),Friends.Num()),PlayAccent,GX+9,GridY+6,OVTypography::Font(EOVTextRole::Numeric));
  if(Pages>1){Button(TEXT("FleetPrevious"),TEXT("◀"),GX+GridWidth-72,GridY+2,30);Button(TEXT("FleetNext"),TEXT("▶"),GX+GridWidth-38,GridY+2,30);}
  if(Friends.Num()>1)Button(TEXT("FormationMenu"),TEXT("진형"),GX+GridWidth-126,GridY+2,48,PC->bFormationMenu);
  if(PC->bFormationMenu){
   const float FY=GridY-92, BW=(GridWidth-12)/3;
   DrawRect(PlayPanel,GX,FY,GridWidth,90);
   DrawText(TEXT("진형 미리보기"),PlayAccent,GX+6,FY+4,OVTypography::Font(EOVTextRole::Body));
   const FName Keys[]={TEXT("FormationScreen"),TEXT("FormationCarrier"),TEXT("FormationConvoy")};
   const TCHAR* Labels[]={TEXT("균형 전열"),TEXT("항모 호위"),TEXT("수송 호위")};
   for(int32 I=0;I<3;++I)Button(Keys[I],Labels[I],GX+4+I*BW,FY+26,BW-2,PC->FormationKind==Keys[I]);
   if(!PC->FormationPreview.IsEmpty())Button(TEXT("FormationApply"),TEXT("배치 적용"),GX+4,FY+57,84,true);
   Button(TEXT("FormationClose"),TEXT("닫기"),GX+GridWidth-60,FY+57,55);
  }
  const float Cell=(GridWidth-16)/Cols;
  for(int32 J=0;J<PerPage && FleetPage*PerPage+J<Friends.Num();++J){
   auto* S=Friends[FleetPage*PerPage+J];const float X=GX+8+(J%Cols)*Cell, CY=GridY+31+(J/Cols)*56;
   const FName Key=S->HullDefinitionId;
   auto* Art=SelectionPortraits.FindRef(Key).Get();
   if(!Art&&!Key.IsNone()&&FApp::CanEverRender()){
    auto* Preview=NewObject<UOVShipPreview>(this);Preview->Update(S,FRotator(-18,68,-15),1.12f,2.8f,INDEX_NONE,384,true);
    Art=Preview->GetTexture();if(Art)SelectionPortraits.Add(Key,Art);Preview->Release();
   }
   const FName Pick(*FString::Printf(TEXT("Pick_%d"),PC->Ships.IndexOfByKey(S)));
   const float CardWidth=Cell-3;
   DrawRect(S==PC->Selected?PlayAccent:FLinearColor(.17f,.32f,.42f),X,CY,CardWidth,53);
   DrawRect(FLinearColor(.012f,.026f,.042f),X+1,CY+1,CardWidth-2,51);
   const float PictureWidth=FMath::Min(CardWidth-4,30.f*2.8f);
   if(Art)DrawTexture(Art,X+(CardWidth-PictureWidth)*.5f,CY+1,PictureWidth,30,0,0,1,1,FLinearColor::White,BLEND_Opaque);
   if(PC->Interior){
    const int32 Count=PC->Interior->CountEmbarkedHeroes(S,S->Systems->FactionId);
    const bool Limited=Play && Play->GetActiveDefinition();
    const FString Berths=Limited?FString::Printf(TEXT("%d/%d"),Count,Play->CurrentHeroBerths(S)):FString::FromInt(Count);
    const FLinearColor Gold=Count?FLinearColor(1,.83f,.40f):FLinearColor(.64f,.69f,.73f);
    float TW,TH; GetTextSize(Berths,TW,TH,OVTypography::Font(EOVTextRole::Body));
    const float BX=X+CardWidth-TW-20;
    DrawRect(FLinearColor(.035f,.055f,.078f,.95f),BX,CY+1,TW+18,16);
    DrawText(TEXT("★"),Gold,BX+2,CY+1,OVTypography::Font(EOVTextRole::Body),.87f);
    DrawText(Berths,Gold,BX+15,CY+1,OVTypography::Font(EOVTextRole::Body));
   }
   if(S->bFTLCharging&&Play){
    DrawRect(FLinearColor(.19f,.11f,.35f),X+2,CY+17,CardWidth-4,13);
    DrawRect(FLinearColor(.57f,.38f,1.f),X+2,CY+17,(CardWidth-4)*Play->ChargeFraction(S),13);
    DrawText(TEXT("도약"),FLinearColor::White,X+4,CY+17,OVTypography::Font(EOVTextRole::Body),.83f);
   }
   FString Label=GetFleetDisplayLabel(S);
   UFont* Font=OVTypography::ShipNameFont(Label);
   float LabelWidth=0,LabelHeight=0; GetTextSize(Label,LabelWidth,LabelHeight,Font);
   const float LabelScale=FMath::Clamp((CardWidth-10)/FMath::Max(1.f,LabelWidth),.9f,1.f);
   const float LabelRoom=(CardWidth-10)/LabelScale;
   int32 LastSpace=INDEX_NONE;
   if(Label.FindLastChar(' ',LastSpace) && Label.Mid(LastSpace+1).IsNumeric()){
    const FString Number=Label.Mid(LastSpace);
    float NumberWidth=0,Height=0; GetTextSize(Number,NumberWidth,Height,Font);
    Label=FitLabel(Font,Label.Left(LastSpace),LabelRoom-NumberWidth)+Number;
   } else Label=FitLabel(Font,Label,LabelRoom);
   DrawText(Label,PlayInk,X+4,CY+31,Font,LabelScale);
   DrawRect(FLinearColor(.05f,.08f,.12f),X+2,CY+48,CardWidth-4,3);
   DrawRect(FLinearColor(.27f,.92f,.51f),X+2,CY+48,(CardWidth-4)*S->Systems->HullHP/FMath::Max(1.f,S->Systems->MaxHullHP),3);
   AddHitBox(FVector2D(X,CY)*UIScale,FVector2D(CardWidth,53)*UIScale,Pick,true,7);
   ActionCenters.Add(Pick,FVector2D(X+CardWidth*.5f,CY+26));
  }
  const auto* Store=GetWorld()->GetSubsystem<UOVLogisticsSubsystem>()->FindCargo(PC->Selected);
  if(PC->Selected && PC->Selected->bFriendly && Store && Store->Capacity>0) {
   const float CY=GridY+146, Width=GridWidth-22;
   const float Used=Store->Amount+3*Store->Advanced+8*Store->Rare;
   DrawText(TEXT("화물"),PlayInk,GX+10,CY,OVTypography::Font(EOVTextRole::Heading));
   const FString Total=OVTypography::Integer(FMath::RoundToInt(Used))+TEXT(" / ")+OVTypography::Integer(FMath::RoundToInt(Store->Capacity));
   DrawNumberRight(Total,PlayInk,GX+Width-44,CY);
   DrawNumberRight(FString::Printf(TEXT("%.0f%%"),100*Used/Store->Capacity),PlayInk,GX+10+Width,CY);
   DrawRect(FLinearColor(.10f,.17f,.22f),GX+10,CY+23,Width,10);
   float Offset=GX+10;
   const float Weights[]={Store->Amount,Store->Advanced*3,Store->Rare*8};
   const FLinearColor Colors[]={PlayAccent,FLinearColor(.45f,.65f,1),FLinearColor(.94f,.65f,.22f)};
   for(int32 K=0;K<3;++K){const float Seg=Width*Weights[K]/Store->Capacity;DrawRect(Colors[K],Offset,CY+23,Seg,10);Offset+=Seg;}
   const TCHAR* ResourceNames[]={TEXT("일반"),TEXT("고급"),TEXT("희귀")};
   const float Amounts[]={Store->Amount,Store->Advanced,Store->Rare};
   for(int32 Index=0; Index<3; ++Index){
    const float Left=GX+10+Index*Width/3;
    DrawText(ResourceNames[Index],PlayInk,Left,CY+39,OVTypography::Font(EOVTextRole::Body));
    DrawNumberRight(OVTypography::Integer(FMath::RoundToInt(Amounts[Index])),PlayInk,Left+Width/3-5,CY+39);
   }
  }
 }
 bCommandShipClipped=bCommandTargetClipped=false;
 DrawRect(PlayPanel,10,Y,610,PanelHeight);
 if(!PC->Selected) {DrawText(TEXT("함선을 선택하세요"),PlayInk,24,Y+20,OVTypography::Font(EOVTextRole::Body));return;}
 auto* Ship=PC->Selected.Get();
 const FString ShipLabel=FitLabel(OVTypography::Font(EOVTextRole::Body),Ship->ShipName,280);
 bCommandShipClipped=ShipLabel!=Ship->ShipName;
 DrawText(ShipLabel,PlayAccent,24,Y+12,OVTypography::Font(EOVTextRole::Body));
 AddHitBox(FVector2D(24,Y+8)*UIScale,FVector2D(280,26)*UIScale,TEXT("CommandShipLabel"),true,8);
 ActionCenters.Add(TEXT("CommandShipLabel"),FVector2D(164,Y+21));
 Button(TEXT("Focus"),TEXT("추적"),315,Y+8,60,PC->bFollow);
 Button(TEXT("Details"),TEXT("함선 상세"),380,Y+8,75,bDetailsOpen);
 if(PC->Interior&&PC->Interior->bEnabled)Button(TEXT("InteriorToggle"),TEXT("선내 지도"),460,Y+8,75,bInteriorOpen);
 Button(TEXT("CommandHeroPanel"),TEXT("영웅"),540,Y+8,65,bHeroPanelOpen);
 const float Hull=FMath::Clamp(Ship->Systems->HullHP/FMath::Max(1.f,Ship->Systems->MaxHullHP),0.f,1.f);
 DrawRect(FLinearColor(.08f,.13f,.19f),24,Y+38,210,10);DrawRect(PlayAccent,24,Y+38,210*Hull,4);
 DrawRect(FLinearColor(.3f,.48f,1),24,Y+45,210*FMath::Clamp(Ship->Systems->Shield/FMath::Max(1.f,Ship->Systems->MaxShield),0.f,1.f),3);
 const FString TargetLabel=TEXT("표적: ")+(Ship->AttackTarget?Ship->AttackTarget->ShipName:TEXT("없음"));
 const FString VisibleTarget=FitLabel(OVTypography::Font(EOVTextRole::Body),TargetLabel,350);
 bCommandTargetClipped=VisibleTarget!=TargetLabel;
 DrawText(VisibleTarget,PlayInk,250,Y+40,OVTypography::Font(EOVTextRole::Body));
 if(Ship->AttackTarget) {
  AddHitBox(FVector2D(250,Y+38)*UIScale,FVector2D(350,16)*UIScale,TEXT("CommandTargetLabel"),true,8);
  ActionCenters.Add(TEXT("CommandTargetLabel"),FVector2D(425,Y+46));
 }
 if(!Ship->bFriendly) {DrawText(TEXT("적 함선 · 관측된 외장 정보"),PlayInk,24,Y+72,OVTypography::Font(EOVTextRole::Body));return;}
 auto* Logistics=GetWorld()->GetSubsystem<UOVLogisticsSubsystem>();
 const float ServiceY=Y+PanelHeight-55;
 if(PC->Combat && PC->Combat->bEnabled && Ship->Systems->Layout) {
  const auto State=PC->Combat->GetShipState(Ship);
  const float OwnRange=Ship->Systems->GetRadarRange()/100.f;
  FString Line;
  if(OwnRange>0)Line=FString::Printf(TEXT("자체 레이더 %s"),*OVTypography::Metres(OwnRange));
  else Line=Ship->Systems->Layout->Slots.ContainsByPredicate([](const auto& D){return D.Facility==EOVFacilityType::Radar;})
   ?TEXT("자체 레이더 정지"):TEXT("자체 레이더 없음");
  if(State.Mounts.IsValidIndex(PC->SelectedWeapon))
   Line+=FString::Printf(TEXT("   ·   무장 %s"),*OVTypography::Metres(State.Mounts[PC->SelectedWeapon].Profile.Range/100.f));
  if(IsValid(Ship->AttackTarget)) {
   const auto D=PC->Combat->GetDetection(Ship,Ship->AttackTarget);
   const TCHAR* Source=!D.bDetected?TEXT("미탐지"):D.Source==TEXT("SUPPORT RADAR")?TEXT("레이더함 공유"):
    D.Source==TEXT("SHARED RADAR")?TEXT("아군 공유"):D.Source==TEXT("OWN RADAR")?TEXT("자체 탐지"):
    D.Source==TEXT("GUARANTEED NEAR")?TEXT("근거리 탐지"):TEXT("시험 탐지");
   Line+=FString::Printf(TEXT("   ·   표적 %s (%s)"),*OVTypography::Metres(FVector::Dist(Ship->GetActorLocation(),Ship->AttackTarget->GetActorLocation())/100.),Source);
  }
  DrawText(FitLabel(OVTypography::Font(EOVTextRole::Numeric),Line,578),PlayInk,24,Y+PanelHeight-21,OVTypography::Font(EOVTextRole::Numeric));
 }
 if(Ship->Systems->Layout->Slots.ContainsByPredicate([](const auto& D){return D.Facility==EOVFacilityType::ResourceStorage;})){
  const bool Enabled=Logistics->InternalRepairEnabled(Ship);
  Button(TEXT("InternalRepair"),Enabled?TEXT("자가 수리 켜짐"):TEXT("자가 수리 꺼짐"),24,ServiceY,110,Enabled);
  DrawText(Logistics->InternalRepairStatus(Ship),PlayInk,143,ServiceY+5,OVTypography::Font(EOVTextRole::Body));
 }
 if(Ship->HullDefinitionId==TEXT("repair"))
  DrawText(Logistics->RepairStatus(Ship),PlayAccent,321,ServiceY+5,OVTypography::Font(EOVTextRole::Body));
 if(Ship->HullDefinitionId==TEXT("radar") && PC->Combat){
  const int32 Wait=FMath::Max(0,FMath::CeilToInt(Ship->ScanReadyAt-PC->Combat->GameTime));
  const bool Offline=!Ship->Systems->CanCommand() || !Ship->Systems->HasRadar() || Ship->bFTLCharging || PC->Combat->EnvironmentRadarDisruptRemaining>0;
  Button(TEXT("RadarSurvey"),Offline?FString(TEXT("정찰 불가")):Wait?FString::Printf(TEXT("정찰 충전 · %d초"),Wait):PC->InputMode==EOVInputMode::Survey?TEXT("적 함선을 선택"):TEXT("적 내부 정찰"),344,ServiceY,156,PC->InputMode==EOVInputMode::Survey);
 }
 if(Ship->HullDefinitionId==TEXT("repair") && PC->InputMode==EOVInputMode::Repair)DrawText(TEXT("수리할 아군을 클릭하세요"),PlayAccent,212,Y+181,OVTypography::Font(EOVTextRole::Body));
 if(Ship->HullDefinitionId==TEXT("repair")) Button(TEXT("RepairOrder"),TEXT("아군 수리"),198,Y+128,112,PC->InputMode==EOVInputMode::Repair);
 if(Ship->bFTLCharging && PC->Carrier && !PC->Carrier->CanDepart(Ship))Button(TEXT("AbandonJumpWing"),TEXT("잔여기 포기"),377,Y+164,108);
 if(Play && !Play->ActiveId.IsNone())Button(TEXT("PlotJump"),TEXT("초광속 이동"),492,Y+164,110,Ship->bFTLCharging);
 const bool Disable=PC->Combat&&PC->Combat->GetShipState(Ship).Intent==EOVAttackIntent::DisableWeapons;
 const FName Actions[]={TEXT("Move"),TEXT("Stop"),TEXT("Target"),TEXT("FireAll"),TEXT("MoveAuto"),TEXT("FireAuto"),TEXT("Fire"),TEXT("NextWeapon"),Disable?FName(TEXT("HullIntent")):FName(TEXT("GunIntent")),TEXT("CancelAttack")};
 const FString Labels[]={TEXT("이동"),TEXT("정지"),TEXT("표적 지정"),TEXT("전체 발사"),Ship->IsMovementAutomatic()?TEXT("이동 자동 ✓"):TEXT("이동 수동"),Ship->IsAttackAutomatic()?TEXT("사격 자동 ✓"):TEXT("사격 수동"),TEXT("선택 발사"),TEXT("다음 무장"),Disable?TEXT("목적: 무장"):TEXT("목적: 선체"),TEXT("공격 취소")};
 const float BW=110;
 for(int32 I=0;I<10;++I)Button(Actions[I],Labels[I],24+(I%5)*(BW+7),Y+56+(I/5)*32,BW,(I==4&&Ship->IsMovementAutomatic())||(I==5&&Ship->IsAttackAutomatic()));
 if(PC->Combat&&PC->Combat->bEnabled) {
  const auto State=PC->Combat->GetShipState(Ship);
  const TCHAR* Policies[]={TEXT("보호: 사격 계속"),TEXT("보호: 목표 달성까지"),TEXT("보호: 철수까지")};
  Button(TEXT("Protection"),Policies[static_cast<int32>(State.Protection)],24,Y+128,165);
  if(Ship->AttackTarget&&Ship->AttackTarget->Systems->HasInteriorPresence(Ship->Systems->FactionId))
   Button(TEXT("ForceGun"),TEXT("선택 강제 · 아군 위험"),195,Y+128,178,!State.ForcedMount.IsNone());
  if(State.Mounts.IsValidIndex(PC->SelectedWeapon)) {
   const auto& M=State.Mounts[PC->SelectedWeapon];const auto R=PC->Combat->GetReadiness(Ship,M.MountId);
   const auto* Def=Ship->Systems->Layout->WeaponMounts.FindByPredicate([&M](const auto& Slot){return Slot.WeaponId==M.MountId;});
   const TCHAR* Kind=AttackWeaponName(M,Def);
   const FString Status=R.Reason==TEXT("COOLDOWN")?FString::Printf(TEXT("재사용 %.1f 초"),R.Cooldown):ReadinessKorean(R.Reason);
   DrawText(FString::Printf(TEXT("%s %d  ·  %s%s"),Kind,PC->SelectedWeapon+1,*Status,M.bFireRequested?TEXT(" · 발사 접수"):TEXT("")),R.bReady?PlayAccent:FLinearColor(1,.8f,.25f),24,Y+166,OVTypography::Font(EOVTextRole::Numeric));
  }
 }
 if(auto* Carrier=GetWorld()->GetSubsystem<UOVCarrierSubsystem>();Carrier&&Carrier->Carriers.ContainsByPredicate([&](const auto& C){return C.Carrier.IsValid() && PC->Selection.Contains(C.Carrier.Get());})) {
  Button(TEXT("LaunchAircraft"),TEXT("함재기 출격"),380,Y+128,108);
  Button(TEXT("RecallAircraft"),TEXT("함재기 귀환"),495,Y+128,108);
  if(auto* State=Carrier->Carriers.FindByPredicate([Ship](const auto& C){return C.Carrier==Ship;})){
   Button(TEXT("AutoWing"),State->bRecallOrdered?TEXT("자동 재개"):State->bAutoLaunch?TEXT("자동 출격 ✓"):TEXT("자동 출격"),393,Y+209,100,State->bAutoLaunch);
   Button(TEXT("WingSupply"),TEXT("항공 보급"),499,Y+209,102);
  }
  FOVAircraftOverview Air;
  for(const auto& C:Carrier->Carriers) {
   AOVShip* CarrierShip=C.Carrier.Get();
   if(!CarrierShip||!CarrierShip->bFriendly||(!bFleetAirOverview && !PC->Selection.Contains(CarrierShip)))continue;
   const FOVAircraftOverview Part=Carrier->GetAircraftOverview(CarrierShip);
   Air.Capacity+=Part.Capacity; Air.ReplacementKits+=Part.ReplacementKits;
   Air.ProductionFraction=FMath::Max(Air.ProductionFraction,Part.ProductionFraction);
   Air.Surviving+=Part.Surviving;Air.Stored+=Part.Stored;Air.Sortie+=Part.Sortie;
   Air.Returning+=Part.Returning;Air.ControlLost+=Part.ControlLost;Air.Wrecked+=Part.Wrecked;
  }
  Button(TEXT("AirScope"),bFleetAirOverview?TEXT("함대 전체"):TEXT("선택 모함"),24,Y+178,75,bFleetAirOverview);
  DrawText(FString::Printf(TEXT("대기 %d/%d · 생산 %.0f%% · 보충 %d대분"),
      Air.Stored,Air.Capacity,100*Air.ProductionFraction,Air.ReplacementKits),PlayInk,105,Y+184,OVTypography::Font(EOVTextRole::Body));
  DrawRect(FLinearColor(.08f,.15f,.20f),106,Y+202,280,4);
  DrawRect(PlayAccent,106,Y+202,280*Air.ProductionFraction,4);
 }
}

TArray<int32,TInlineAllocator<64>> AOVHUD::BattleNoticeIndices() const
{
 TArray<int32,TInlineAllocator<64>> Result;const auto* Feed=GetWorld()?GetWorld()->GetSubsystem<UOVBattleNotices>():nullptr;
 const auto* PC=Cast<AOVController>(PlayerOwner);if(!Feed||!PC)return Result;
 if(!bBattleFeedOpen) {
  const double Now=FPlatformTime::Seconds();
  for(int32 Tier=1;Tier>=0;--Tier) for(int32 I=Feed->Notices.Num()-1;I>=0 && Result.Num()<3;--I){
   const auto& N=Feed->Notices[I];
   const bool Critical=N.bCritical || N.Kind==EOVBattleNoticeKind::FacilityDestroyed || N.Kind==EOVBattleNoticeKind::ShieldDepleted || N.Kind==EOVBattleNoticeKind::HeroDeath;
   if(int32(Critical)==Tier && (NoticeMask&(1<<int32(N.Kind))) && Now-N.DisplayTime <= (Critical?18.:10.))Result.Add(I);
  }
  return Result;
 }
 for(int32 I=Feed->Notices.Num()-1;I>=0;--I) {
  const auto& N=Feed->Notices[I];const auto K=N.Kind;bool Match=true;
  if(bBattleFeedOpen)switch(BattleFeedFilter) {
   case 1:Match=N.bCritical;break;
   case 2:Match=K==EOVBattleNoticeKind::Hit||K==EOVBattleNoticeKind::ShieldDepleted||K==EOVBattleNoticeKind::ArmorDestroyed||K==EOVBattleNoticeKind::FacilityDestroyed||K==EOVBattleNoticeKind::HeroHit;break;
   case 3:Match=K==EOVBattleNoticeKind::BoardingEntry||K==EOVBattleNoticeKind::BoardingReturn||K==EOVBattleNoticeKind::TrooperDeath||K==EOVBattleNoticeKind::FacilityCaptured||K==EOVBattleNoticeKind::HostileBoarding||K==EOVBattleNoticeKind::HeroDeath||K==EOVBattleNoticeKind::HeroRescue;break;
   case 4:Match=K==EOVBattleNoticeKind::AircraftHalf||K==EOVBattleNoticeKind::AircraftCritical||K==EOVBattleNoticeKind::AircraftEliminated||K==EOVBattleNoticeKind::AircraftAdjusted;break;
   case 5:Match=IsValid(PC->Selected)&&!N.ShipId.IsNone()&&N.ShipId==PC->Selected->GetFName();break;
   default:break;
  }
  if(Match)Result.Add(I);
 }
 return Result;
}
int32 AOVHUD::VisibleBattleNoticeCount() const
{
 const auto* PC=Cast<AOVController>(PlayerOwner);const auto* Feed=GetWorld()?GetWorld()->GetSubsystem<UOVBattleNotices>():nullptr;
 if(!PC||PC->bDeveloperMode||bSceneMenuOpen||!Feed||Feed->Notices.IsEmpty())return 0;
 if(bBattleFeedOpen){const int32 Total=BattleNoticeIndices().Num();if(Total==0)return 0;const int32 Page=FMath::Clamp(BattleFeedPage,0,(Total-1)/3);return FMath::Min(3,Total-Page*3);}
 return BattleNoticeIndices().Num();
}
float AOVHUD::BattleNoticeTop() const
{
 const auto* PC=Cast<AOVController>(PlayerOwner);
 const auto* Scene=PC ? PC->GetPlayScenario() : nullptr;
 if(Scene && Scene->ActiveId==TEXT("Sandbox")) return Scene->bSandboxHullMenu?401.f:218.f;
 return Scene && Scene->ActiveId==TEXT("Evacuation") ? 184.f : 66.f;
}
float AOVHUD::CommandPanelHeight() const
{
 const auto* PC=Cast<AOVController>(PlayerOwner);
 const auto* Ship=PC?PC->Selected.Get():nullptr;
 if(!Ship || !Ship->bFriendly || !Ship->Systems || !Ship->Systems->Layout)return 210.f;
 const bool HasServiceRow=Ship->HullDefinitionId==TEXT("radar") || Ship->HullDefinitionId==TEXT("repair") ||
     Ship->Systems->Layout->Slots.ContainsByPredicate([](const auto& D){return D.Facility==EOVFacilityType::ResourceStorage;});
 return (Ship->Systems->Layout->bAllowAircraftOperations?242.f:210.f)+(HasServiceRow?36.f:0.f)+22.f;
}

int32 AOVHUD::BattleNoticeRows() const
{
 const auto* PC=Cast<AOVController>(PlayerOwner);if(!PC||PC->bDeveloperMode||bSceneMenuOpen)return 0;
 const int32 Count=VisibleBattleNoticeCount();return Count>0?Count:(bBattleFeedOpen?1:0);
}

void AOVHUD::DrawBattleNotices()
{
 const auto* Feed=GetWorld()->GetSubsystem<UOVBattleNotices>();const int32 Count=VisibleBattleNoticeCount(),Rows=BattleNoticeRows();
 if(!Feed||Rows==0)return;
 const float W=Canvas->SizeX/UIScale,H=Canvas->SizeY/UIScale,X=W-354;
 const auto Indices=BattleNoticeIndices();const int32 Pages=FMath::Max(1,(Indices.Num()+2)/3);
 const int32 Page=bBattleFeedOpen?FMath::Clamp(BattleFeedPage,0,Pages-1):0;
 if(WarningLevel>0&&FPlatformTime::Seconds()-Feed->LastCriticalDisplayTime<3) {
  const FLinearColor Warning(.8f,.04f,.03f,WarningLevel==1?.08f:.20f);
  DrawRect(Warning,0,0,W,5);DrawRect(Warning,0,H-5,W,5);DrawRect(Warning,0,0,5,H);DrawRect(Warning,W-5,0,5,H);
 }
 for(int32 I=0;I<Count;++I) {
  const int32 Index=Indices[Page*3+I];const auto& N=Feed->Notices[Index];
  const TCHAR* Names[]={TEXT("피격"),TEXT("완파"),TEXT("쉴드 고갈"),TEXT("장갑 파괴"),TEXT("시설 파괴"),TEXT("쉴드 재가동"),TEXT("영웅 전사"),TEXT("강습 진입"),TEXT("강습 귀환"),TEXT("시설 장악"),TEXT("적 침입"),TEXT("영웅 피격"),TEXT("구조 대기"),TEXT("함재기 절반 미만"),TEXT("함재기 10% 이하"),TEXT("함재기 전멸"),TEXT("항공대 편제 변경"),TEXT("전사")};
  const FString Suffix=N.Baseline>0?FString::Printf(TEXT(" %d/%d"),N.Remaining,N.Baseline):N.Count>1?FString::Printf(TEXT(" ×%d"),N.Count):FString();
  const int32 Seconds=FMath::Max(0,FMath::FloorToInt(N.GameTime));
  const FString Label=FString::Printf(TEXT("%02d:%02d %s · %s%s"),Seconds/60,Seconds%60,*(N.Subject.IsEmpty()?N.ShipName.Left(6):N.Subject),Names[FMath::Clamp(int32(N.Kind),0,int32(UE_ARRAY_COUNT(Names))-1)],*Suffix);
  const float RowY=BattleNoticeTop()+I*36;
  const bool bComms=!bBattleFeedOpen && (N.Kind==EOVBattleNoticeKind::BoardingEntry || N.Kind==EOVBattleNoticeKind::HostileBoarding || N.Kind==EOVBattleNoticeKind::BoardingReturn);
  Button(FName(*FString::Printf(TEXT("BattleNotice_%lld"),N.Serial)),Label,X,RowY,340,N.bCritical,bComms?EOVTextRole::Comms:EOVTextRole::Body);
  if(!N.FriendlyHeroDefinition.IsNone()){
   const auto* Definition=GetGameInstance()->GetSubsystem<UOVHeroSubsystem>()->Definition(N.FriendlyHeroDefinition);
   if(Definition && Definition->Portrait)DrawTexture(Definition->Portrait,X+307,RowY+1,26,26,0,0,1,1);
  }
 }
 if(Count==0)DrawText(TEXT("표시할 기록 없음"),PlayInk,X+8,BattleNoticeTop()+6,OVTypography::Font(EOVTextRole::Body));
 if(bBattleFeedOpen) {
  const float Y=BattleNoticeTop()+Rows*36;
  Button(TEXT("BattleFeedOlder"),TEXT("이전"),X,Y,64);
  const TCHAR* Filters[]={TEXT("전체"),TEXT("위기"),TEXT("피해"),TEXT("강습"),TEXT("항공"),TEXT("선택함")};
  Button(TEXT("BattleFeedFilter"),FString::Printf(TEXT("%s %d/%d"),Filters[FMath::Clamp(BattleFeedFilter,0,5)],Page+1,Pages),X+68,Y,104);
  Button(TEXT("BattleFeedNewer"),TEXT("다음"),X+176,Y,64);
  const TCHAR* Levels[]={TEXT("꺼짐"),TEXT("약하게"),TEXT("기본")};
  Button(TEXT("BattleWarningLevel"),FString::Printf(TEXT("화면 경고: %s"),Levels[FMath::Clamp(WarningLevel,0,2)]),X,Y+32,150);
  Button(TEXT("NoticeSettings"),TEXT("표시할 알림 선택"),X+156,Y+32,178,bNoticeSettings);
  if(bNoticeSettings){
   static const TCHAR* Names[]={TEXT("일반 피격"),TEXT("함선 완파"),TEXT("쉴드 붕괴"),TEXT("장갑 파괴"),TEXT("시설 파괴"),TEXT("쉴드 복구"),TEXT("영웅 전사"),TEXT("강습 진입"),TEXT("철수 복귀"),TEXT("시설 장악"),TEXT("적 침입"),TEXT("영웅 피격"),TEXT("구조 대기"),TEXT("기체 절반"),TEXT("기체 위기"),TEXT("기체 전멸"),TEXT("편제 변경"),TEXT("일반병 전사")};
   for(int32 K=0;K<UE_ARRAY_COUNT(Names);++K){
    const bool Enabled=(NoticeMask&(1<<K))!=0;
    Button(FName(*FString::Printf(TEXT("ToastKind%d"),K)),(Enabled?FString(TEXT("✓ ")):FString(TEXT("□ ")))+Names[K],X+(K%3)*112,Y+66+(K/3)*29,108,Enabled);
   }
  }
  if(Feed->DroppedCount>0)DrawText(FString::Printf(TEXT("이전 알림 %d건 생략"),Feed->DroppedCount),PlayInk,X,Y+(bNoticeSettings?242:66),OVTypography::Font(EOVTextRole::Numeric));
 }
}

void AOVHUD::DrawWorldIndicators()
{
 auto* PC=Cast<AOVController>(PlayerOwner);
 auto Project=[&](FVector V,FVector2D& P) { if(!PC->ProjectWorldLocationToScreen(V,P)||IsOverPanel(P.X,P.Y))return false; P/=UIScale; return true; };
 auto Ring=[this](FVector2D Center,float RX,float RY,FLinearColor Color,float Thickness){
  FVector2D Last=Center+FVector2D(RX,0);
  for(int32 J=1;J<=24;++J){
   const float Angle=J*(2*PI/24);
   const FVector2D Next=Center+FVector2D(RX*FMath::Cos(Angle),RY*FMath::Sin(Angle));
   DrawLine(Last.X,Last.Y,Next.X,Next.Y,Color,Thickness);Last=Next;
  }
 };
 if(const auto* Play=PC->GetPlayScenario()) for(const auto& Trail:Play->JumpTrails){
  FVector2D A,B;if(!PC->ProjectWorldLocationToScreen(Trail.Start,A)||!PC->ProjectWorldLocationToScreen(Trail.End,B))continue;
  A/=UIScale;B/=UIScale;const float Age=FMath::Clamp((Play->ElapsedSeconds-Trail.At)/1.2,0.,1.);
  Ring(A,12+62*Age,5+27*Age,FLinearColor(.30f,.72f,1,1-Age),3*(1-Age));
  const FVector2D Tail=FMath::Lerp(A,B,double(Age*Age)), Head=FMath::Lerp(A,B,double(FMath::Min(1.f,.22f+Age*2)));
  DrawLine(Tail.X,Tail.Y,Head.X,Head.Y,FLinearColor(.15f,.6f,1.f,1-Age),9*(1-Age));DrawLine(Tail.X,Tail.Y,Head.X,Head.Y,FLinearColor(.85f,.96f,1,1-Age),3*(1-Age));
 }
 for(int32 I=0;I<PC->Ships.Num();++I) {
  AOVShip* Ship=PC->Ships[I]; if(!IsValid(Ship)||Ship->HangarHost.IsValid()||(PC->Combat&&PC->Combat->bEnabled&&PC->Combat->IsDeparted(Ship)))continue;
  if(PC->IsShipVanished(Ship))continue;
  FVector2D P; if(!Project(Ship->GetActorLocation(),P))continue;
  const bool Selected=PC->Selection.Contains(Ship),Target=PC->Selected&&PC->Selected->AttackTarget==Ship;
  FLinearColor Color=Ship->bFriendly?PlayAccent:FLinearColor(1,.3f,.16f);
  float ThroeRemaining=0.f;
  const bool bInThroe=PC->IsShipInDeathThroe(Ship,ThroeRemaining);
  if(Ship->Systems->bWrecked)Color=bInThroe?FLinearColor(1.f,0.45f,0.2f):FLinearColor::Gray;
  FVector2D Edge;
  const float Extent=Ship->Systems->Layout?(Ship->Systems->Layout->LocalHalfExtent*Ship->GetActorScale3D()).Size():200;
  float Radius=Selected?22:16;
  if(PC->ProjectWorldLocationToScreen(Ship->GetActorLocation()+PC->PlayerCameraManager->GetCameraRotation().RotateVector(FVector::RightVector)*Extent,Edge)) Radius=FMath::Clamp(float((Edge/UIScale-P).Size()),Radius,110.f);
  if(Ship->bFTLCharging){
   if(const auto* Scene=PC->GetPlayScenario()){
    const float Charge=Scene->ChargeFraction(Ship), Cycle=FMath::Frac(float(Scene->ElapsedSeconds)*(.75f+Charge));
    const float R=Radius*(1.45f-.48f*Cycle);
    Ring(P,R,R*.52f,FLinearColor(.36f,.68f,1.f,.35f+Charge*.45f),1.5f+Charge);
   }
  }
  const FVector2D Label(FMath::Clamp(P.X+Radius+7,208.,Canvas->SizeX/UIScale-200.),FMath::Clamp(P.Y-Radius-20,100.,Canvas->SizeY/UIScale-155.));
  if(bInThroe) {
   DrawLine(P.X-10,P.Y-10,P.X+10,P.Y+10,FLinearColor(1.f,0.45f,0.2f),2.2f);
   DrawLine(P.X+10,P.Y-10,P.X-10,P.Y+10,FLinearColor(1.f,0.45f,0.2f),2.2f);
  }
  if(PC->bShowDebugOutlines) {
   DrawLine(P.X-Radius,P.Y-Radius,P.X-Radius,P.Y+Radius,Color,Selected?3:1);
   DrawLine(P.X+Radius,P.Y-Radius,P.X+Radius,P.Y+Radius,Color,Selected?3:1);
   DrawLine(P.X-Radius,P.Y-Radius,P.X+Radius,P.Y-Radius,Color,Selected?3:1);
  }
  if(!PC->FormationPreview.Contains(Ship) && (Selected||Target||bInThroe||PC->CameraDistance<45000)) {
   const FString Name=(Target?TEXT("공격 ▶ "):Selected?TEXT("선택 ▶ "):TEXT(""))+Ship->ShipName;
   DrawText(FitLabel(OVTypography::Font(EOVTextRole::Body),Name,180),Color,Label.X,Label.Y,OVTypography::Font(EOVTextRole::Body));
  }
  const float Hull=FMath::Clamp(Ship->Systems->HullHP/FMath::Max(1.f,Ship->Systems->MaxHullHP),0.f,1.f);
  DrawRect(FLinearColor(.06f,.08f,.12f),Label.X,Label.Y+17,86,8);
  DrawRect(Color,Label.X,Label.Y+17,86*Hull,3);
  DrawRect(FLinearColor(.25f,.4f,1),Label.X,Label.Y+22,86*Ship->Systems->Shield/FMath::Max(1.f,Ship->Systems->MaxShield),3);
  if(bInThroe)DrawText(FString::Printf(TEXT("★ 침몰 붕괴 (%.1fs)"),ThroeRemaining),FLinearColor(1.f,0.45f,0.2f),Label.X,Label.Y+29,OVTypography::Font(EOVTextRole::Numeric));
  else if((Selected||Target) && !PC->FormationPreview.Contains(Ship))DrawText(FString::Printf(TEXT("고도 %s"),*OVTypography::Metres(Ship->GetActorLocation().Z/100)),PlayInk,Label.X,Label.Y+29,OVTypography::Font(EOVTextRole::Numeric));
  if(const double* ETA=PC->SelectedMissileApproach.Find(Ship)) {
   const FString Warning=FString::Printf(TEXT("미사일 최근접 %.1f초"),*ETA);
   float TW,TH;GetTextSize(Warning,TW,TH,OVTypography::Font(EOVTextRole::Body));
   const float WY=Label.Y-TH-7;
   DrawRect(FLinearColor(.075f,.042f,.024f,.98f),Label.X,WY,TW+14,TH+5);
   DrawRect(FLinearColor(1,.52f,.16f),Label.X,WY,3,TH+5);
   DrawText(Warning,FLinearColor(1,.80f,.48f),Label.X+7,WY+2,OVTypography::Font(EOVTextRole::Body));
  }
  const FName Marker(*FString::Printf(TEXT("WorldPick_%d"),I));
  AddHitBox((P-FVector2D(Radius+6,Radius+6))*UIScale,FVector2D(2*Radius+12,2*Radius+12)*UIScale,Marker,true,5);
  ActionCenters.Add(Marker,P);
  if(Selected&&Ship->bHasDestination && PC->FormationPreview.IsEmpty()) { FVector2D D;if(Project(Ship->Destination,D))DrawText(FString::Printf(TEXT("이동 목적지  고도 %s"),*OVTypography::Metres(Ship->Destination.Z/100)),FLinearColor::Green,D.X,D.Y,OVTypography::Font(EOVTextRole::Numeric)); }
 }
 for(const auto& Slot:PC->FormationPreview){
  FVector2D P;
  if(auto* Ship=Slot.Key.Get();Ship && Project(Slot.Value,P)){
   float Width=0,Height=0;
   GetTextSize(Ship->ShipName,Width,Height,OVTypography::Font(EOVTextRole::Body));
   Ring(P,10,6,PlayAccent,2);
   DrawRect(PlayPanel,P.X+11,P.Y+3,Width+6,Height+2);
   DrawText(Ship->ShipName,PlayInk,P.X+14,P.Y+4,OVTypography::Font(EOVTextRole::Body));
  }
 }
 if(PC->bPlacing) { FVector2D P;if(Project(PC->Preview,P))DrawText(FString::Printf(TEXT("고도 %s · 해제/Enter 확정"),*OVTypography::Metres(PC->Preview.Z/100)),FLinearColor::Yellow,P.X+14,P.Y-20,OVTypography::Font(EOVTextRole::Numeric)); }
 TSet<FName> ShownImpacts;
 if(PC->Combat) for(int32 I=PC->Combat->RecentEvents.Num()-1;I>=0;--I) {
  const auto& E=PC->Combat->RecentEvents[I];
  if(ShownImpacts.Contains(E.HitShipId))continue;
  if(PC->Combat->GameTime-E.ResolvedAt>.65 || E.Result!=EOVAttackResult::Hit)continue;
  ShownImpacts.Add(E.HitShipId);
  FVector2D P;if(Project(E.Position,P))DrawText(E.HullDamage>0?TEXT("선체 명중"):TEXT("쉴드 / 장갑 명중"),FLinearColor(1,.8f,.2f),P.X,P.Y-45,OVTypography::Font(EOVTextRole::Body));
 }
}

bool AOVHUD::IsOverStarMapPanel(float X,float Y) const
{
 const auto* PC=Cast<AOVController>(PlayerOwner);
 if(!PC||PC->bDeveloperMode||!bStarMapOpen) return false;
 const FVector2D P(X/UIScale,Y/UIScale);
 const float W=ViewportPixels.X/UIScale,H=ViewportPixels.Y/UIScale;
 const float MX=FMath::Clamp((W-900.f)*.5f,8.f,216.f), MY=60.f, MW=W-2*MX, MH=FMath::Max(410.f,H-MY-12.f);
 return P.X>=MX&&P.X<=MX+MW&&P.Y>=MY&&P.Y<=MY+MH;
}

void AOVHUD::DrawStarMap()
{
 auto* PC=Cast<AOVController>(PlayerOwner);
 if(!PC||PC->bDeveloperMode||!bStarMapOpen||PC->StarSystems.IsEmpty()) return;
 const float W=Canvas->SizeX/UIScale,H=Canvas->SizeY/UIScale;
 const float MX=FMath::Clamp((W-900.f)*.5f,8.f,216.f), MY=60.f, MW=W-2*MX, MH=FMath::Max(410.f,H-MY-12.f);
 TGuardValue<bool> MapLayer(bDrawingStarMap,true);
 DrawRect(FLinearColor(0.006f,0.016f,0.032f,0.97f),MX,MY,MW,MH);
 AddHitBox(FVector2D::ZeroVector,ViewportPixels,TEXT("StarMapBackground"),true,300);
 DrawText(bChoosingJump?TEXT("초광속 이동 · 목적지 선택"):TEXT("은하 지도 · 성계 캠페인 및 우주 환경"),PlayAccent,MX+14,MY+10,OVTypography::Font(EOVTextRole::Body));
 if(!bChoosingJump)DrawText(FString::Printf(TEXT("보유 자원 · 일반 %d  |  고급 %d  |  최고급 %d"),PC->ResourceCommon,PC->ResourceAdvanced,PC->ResourceSupreme),
  FLinearColor(0.65f,0.95f,0.85f),MX+280,MY+12,OVTypography::Font(EOVTextRole::Body));
 Button(TEXT("StarMapClose"),TEXT("닫기 X"),MX+MW-78,MY+6,68,true);
 const float MapX=MX+12.f,MapY=MY+40.f,MapW=MW*0.56f,MapH=MH-52.f;
 DrawRect(FLinearColor(0.01f,0.025f,0.05f,0.96f),MapX,MapY,MapW,MapH);
 auto SysScreenPos=[&](const FOVStarSystemNode& Node) -> FVector2D {
  return FVector2D(MapX+28.f+Node.MapNormalizedPos.X*(MapW-96.f),MapY+24.f+Node.MapNormalizedPos.Y*(MapH-56.f));
 };
 // Draw hyperlanes
 for(const FOVStarSystemNode& Node:PC->StarSystems) {
  const FVector2D P1=SysScreenPos(Node);
  for(const FName& LinkId:Node.Links) {
   const FOVStarSystemNode* Target=PC->StarSystems.FindByPredicate([LinkId](const FOVStarSystemNode& N){return N.SystemId==LinkId;});
   if(!Target || Node.SystemId.LexicalLess(Target->SystemId)) continue;
   const FVector2D P2=SysScreenPos(*Target);
   const bool bConnectedToCurrent=(Node.SystemId==PC->CurrentStarSystemId || Target->SystemId==PC->CurrentStarSystemId);
   DrawLine(P1.X,P1.Y,P2.X,P2.Y,bConnectedToCurrent?FLinearColor(0.25f,0.78f,1.f,0.85f):FLinearColor(0.18f,0.32f,0.48f,0.6f),bConnectedToCurrent?2.2f:1.2f);
  }
 }
 if(SelectedStarSystemId.IsNone()) SelectedStarSystemId=PC->CurrentStarSystemId;
 // Draw star system nodes
 for(const FOVStarSystemNode& Node:PC->StarSystems) {
  const FVector2D P=SysScreenPos(Node);
  const bool bIsSelected=(Node.SystemId==SelectedStarSystemId);
  const bool bIsCurrent=(Node.SystemId==PC->CurrentStarSystemId);
  const FLinearColor FactionCol=Node.OwnerFaction==TEXT("player")?FLinearColor(0.2f,0.88f,1.f):
   (Node.OwnerFaction==TEXT("enemy")?FLinearColor(1.f,0.35f,0.28f):FLinearColor(0.95f,0.82f,0.35f));
  if(bIsCurrent) DrawRect(FLinearColor(0.2f,0.9f,1.f,0.25f),P.X-44,P.Y-18,88,36);
  Button(FName(*FString::Printf(TEXT("StarSystem_Select_%s"),*Node.SystemId.ToString())),
   Node.DisplayName.Left(7),P.X-40,P.Y-14,80,bIsSelected);
  DrawRect(FactionCol,P.X-40,P.Y-14,4,27);
  const TCHAR* EnvTag=Node.Environment==EOVSpaceEnvironmentType::AsteroidField?TEXT("[운석]"):
   Node.Environment==EOVSpaceEnvironmentType::RadiationZone?TEXT("[방사능]"):
   Node.Environment==EOVSpaceEnvironmentType::SolarFlare?TEXT("[플레어]"):
   Node.Environment==EOVSpaceEnvironmentType::BlackHole?TEXT("[블랙홀]"):TEXT("[안정]");
  DrawText(FString::Printf(TEXT("%s%s"),bIsCurrent?TEXT("★현재 "):TEXT(""),EnvTag),FactionCol,P.X-36,P.Y+14,OVTypography::Font(EOVTextRole::Body));
 }
 // Right Inspector Panel
 const float IX=MapX+MapW+12.f,IY=MapY,IW=MW-MapW-36.f,IH=MapH;
 DrawRect(FLinearColor(0.012f,0.028f,0.052f,0.96f),IX,IY,IW,IH);
 const FOVStarSystemNode* Sel=PC->StarSystems.FindByPredicate([this](const FOVStarSystemNode& N){return N.SystemId==SelectedStarSystemId;});
 if(!Sel) Sel=&PC->StarSystems[0];
 const FLinearColor OwnerColor=Sel->OwnerFaction==TEXT("player")?PlayAccent:(Sel->OwnerFaction==TEXT("enemy")?FLinearColor(1.f,0.4f,0.3f):FLinearColor(1.f,0.85f,0.35f));
 const FString OwnerLabel=Sel->OwnerFaction==TEXT("player")?TEXT("선봉 함대 (아군 통제)"):
  (Sel->OwnerFaction==TEXT("enemy")?FString::Printf(TEXT("적 통제 (%s)"),*Sel->EnemyFactionName):TEXT("중립 성계 (개척 가능)"));
 DrawText(Sel->DisplayName,PlayAccent,IX+12,IY+10,OVTypography::Font(EOVTextRole::Body));
 DrawText(FString::Printf(TEXT("통제 세력: %s  ·  위험도 %d단계"),*OwnerLabel,Sel->DangerLevel),OwnerColor,IX+12,IY+36,OVTypography::Font(EOVTextRole::Numeric));
 DrawText(FString::Printf(TEXT("우주 환경: %s"),*AOVController::GetEnvironmentDisplayName(Sel->Environment)),FLinearColor(0.5f,0.95f,1.f),IX+12,IY+58,OVTypography::Font(EOVTextRole::Body));
 const TCHAR* EnvDesc=Sel->Environment==EOVSpaceEnvironmentType::AsteroidField?TEXT("• 3D 운석이 직사/투사체를 물리 차폐하고 항로 폭을 제한"):
  Sel->Environment==EOVSpaceEnvironmentType::RadiationZone?TEXT("• 경계 구역 내 지속 방사능 하중 (활성 쉴드가 우선 흡수)"):
  Sel->Environment==EOVSpaceEnvironmentType::SolarFlare?TEXT("• 예고→파동 통과 주기 사건 · 비엄폐 시 쉴드 부하 및 레이더 교란"):
  Sel->Environment==EOVSpaceEnvironmentType::BlackHole?TEXT("• 중력권 1,000m(0.8m/s²) · FTL 충전 중에도 중력 변위 · 중심 45m 즉시 완파"):
  TEXT("• 환경 제약 없는 표준 우주 전장");
 DrawText(EnvDesc,PlayInk,IX+12,IY+78,OVTypography::Font(EOVTextRole::Body));
 if(bChoosingJump){
  const auto* From=PC->StarSystems.FindByPredicate([PC](const auto& N){return N.SystemId==PC->CurrentStarSystemId;});
  const bool Linked=From && From->Links.Contains(Sel->SystemId);
  DrawText(FString::Printf(TEXT("도약 함대   %d척"),PC->Selection.Num()),PlayAccent,IX+12,IY+115,OVTypography::Font(EOVTextRole::Numeric));
  DrawText(Linked?TEXT("충전 후 함재기 회수를 기다립니다"):TEXT("연결선이 닿는 이웃 성계를 고르세요"),PlayInk,IX+12,IY+146,OVTypography::Font(EOVTextRole::Body));
  if(Linked)Button(FName(*FString::Printf(TEXT("StarSystem_FTL_%s"),*Sel->SystemId.ToString())),TEXT("이 성계로 도약"),IX+12,IY+184,IW-24,true);
  if(!PC->LastAction.IsEmpty())DrawText(FitLabel(OVTypography::Font(EOVTextRole::Body),PC->LastAction,IW-24),PlayInk,IX+12,IY+224,OVTypography::Font(EOVTextRole::Body));
  return;
 }
 DrawText(FString::Printf(TEXT("자원 산출 (주기당): 일반 +%d  ·  고급 +%d  ·  최고급 +%d"),Sel->CommonYield,Sel->AdvancedYield,Sel->SupremeYield),
  FLinearColor(0.7f,0.95f,0.8f),IX+12,IY+102,OVTypography::Font(EOVTextRole::Body));
 DrawText(FString::Printf(TEXT("설치 기지: %s %s"),*AOVController::GetOutpostDisplayName(Sel->Outpost),
  Sel->OutpostMaxHP>0?*FString::Printf(TEXT("(내구 %d/%d)"),Sel->OutpostHP,Sel->OutpostMaxHP):TEXT("")),PlayInk,IX+12,IY+124,OVTypography::Font(EOVTextRole::Body));
 const FString IdStr=Sel->SystemId.ToString();
 const float BtnW=(IW-30.f)*0.5f;
 Button(FName(*FString::Printf(TEXT("StarSystem_Enter_%s"),*IdStr)),TEXT("전장 전환 (성계 진입)"),IX+12,IY+154,BtnW,Sel->SystemId==PC->CurrentStarSystemId);
 Button(FName(*FString::Printf(TEXT("StarSystem_FTL_%s"),*IdStr)),TEXT("이 성계로 도약"),IX+18+BtnW,IY+154,BtnW);
 DrawText(TEXT("전초기지 건설 / 업그레이드 (중립·아군 성계)"),PlayAccent,IX+12,IY+192,OVTypography::Font(EOVTextRole::Body));
 Button(FName(*FString::Printf(TEXT("StarSystem_BuildBasic_%s"),*IdStr)),TEXT("기본 기지 (80/20/0)"),IX+12,IY+214,IW-24,Sel->Outpost==EOVOutpostKind::Basic);
 Button(FName(*FString::Printf(TEXT("StarSystem_BuildMine_%s"),*IdStr)),TEXT("자원 채굴 기지 (110/45/5 · 산출+45%)"),IX+12,IY+246,IW-24,Sel->Outpost==EOVOutpostKind::MiningHub);
 Button(FName(*FString::Printf(TEXT("StarSystem_BuildCombat_%s"),*IdStr)),TEXT("전투 요새 기지 (140/65/12)"),IX+12,IY+278,IW-24,Sel->Outpost==EOVOutpostKind::CombatBase);
 DrawText(TEXT("현재 전장 우주 환경 즉시 전환"),PlayAccent,IX+12,IY+316,OVTypography::Font(EOVTextRole::Body));
 const float EW=(IW-36.f)/5.f;
 Button(TEXT("Env_None"),TEXT("안정"),IX+12,IY+336,EW,PC->ActiveEnvironment==EOVSpaceEnvironmentType::None);
 Button(TEXT("Env_AsteroidField"),TEXT("운석"),IX+15+EW,IY+336,EW,PC->ActiveEnvironment==EOVSpaceEnvironmentType::AsteroidField);
 Button(TEXT("Env_RadiationZone"),TEXT("방사능"),IX+18+EW*2,IY+336,EW,PC->ActiveEnvironment==EOVSpaceEnvironmentType::RadiationZone);
 Button(TEXT("Env_SolarFlare"),TEXT("플레어"),IX+21+EW*3,IY+336,EW,PC->ActiveEnvironment==EOVSpaceEnvironmentType::SolarFlare);
 Button(TEXT("Env_BlackHole"),TEXT("블랙홀"),IX+24+EW*4,IY+336,EW,PC->ActiveEnvironment==EOVSpaceEnvironmentType::BlackHole);
}
