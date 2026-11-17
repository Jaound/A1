#include "OVHUD.h"
#include "OVController.h"
#include "OVShip.h"
#include "OVShipSystemsComponent.h"
#include "OVCombatSubsystem.h"
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
FString AOVHUD::ReadinessKorean(const FString& Key)
{
 static const TMap<FString,FString> Labels={
  {TEXT("READY"),TEXT("발사 가능")},{TEXT("NO VALID TARGET"),TEXT("표적을 지정하세요")},
  {TEXT("OUT OF RANGE"),TEXT("사거리 밖")},{TEXT("NO DETECTION"),TEXT("탐지 상실")},
  {TEXT("MOUNT BLIND ARC"),TEXT("장착면 사각")},{TEXT("TURRET ALIGNING"),TEXT("포탑 조준 중")},
  {TEXT("FRIENDLY IN LINE"),TEXT("아군이 사선을 가림")},{TEXT("COOLDOWN"),TEXT("재사용 대기")},
  {TEXT("WEAPON CONTROL OFF"),TEXT("무기제어실 정지")},{TEXT("SUPPLY OFF"),TEXT("무장 공급 시설 정지")},
  {TEXT("MOUNT DESTROYED"),TEXT("무장 파괴")},{TEXT("FTL CHARGING"),TEXT("FTL 충전으로 빔 사용 불가")},
  {TEXT("INTERIOR ALLY HOLD"),TEXT("내부 아군 보호 · 사격 보류")},
  {TEXT("NO OBSERVED WEAPONS - WAIT"),TEXT("관측된 적 무장 없음 · 대기")},
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
 const auto* Hit=GetHitBoxAtCoordinates(Pixel,true);
 return Hit && Hit->GetName().ToString().StartsWith(TEXT("WorldPick_"));
}

void AOVHUD::DrawPlayHUD()
{
 auto* PC=Cast<AOVController>(PlayerOwner);
 const float W=Canvas->SizeX/UIScale,H=Canvas->SizeY/UIScale;
 DrawRect(PlayPanel,10,10,W-20,46);
 DrawText(TEXT("오비탈 뱅가드"),PlayAccent,24,18,GEngine->GetMediumFont());
 Button(TEXT("SceneMenu"),TEXT("장면"),226,18,70,bSceneMenuOpen);
 Button(TEXT("Pause"),PC->bSimulationPaused?TEXT("시작 ▷"):TEXT("정지 Ⅱ"),318,18,84,PC->bSimulationPaused);
 Button(TEXT("Speed1"),TEXT("1배"),410,18,52,PC->SimulationSpeed==1);
 Button(TEXT("Speed2"),TEXT("2배"),468,18,52,PC->SimulationSpeed==2);
 Button(TEXT("Speed3"),TEXT("3배"),526,18,52,PC->SimulationSpeed==3);
 Button(TEXT("Overview"),TEXT("전체 보기"),596,18,100);
 Button(TEXT("HeroPanel"),TEXT("영웅"),W-202,18,82,bHeroPanelOpen);
 Button(TEXT("Developer"),TEXT("개발 도구"),W-112,18,96,PC->bDeveloperMode);
 if(LastNoticeSource!=PC->LastAction) {
  LastNoticeSource=PC->LastAction;NoticeExpires=FPlatformTime::Seconds()+3;
  NoticeText=PC->LastAction;
  int32 Separator=INDEX_NONE;if(NoticeText.FindChar(TCHAR('·'),Separator))NoticeText=NoticeText.Left(Separator).TrimEnd();
  if(NoticeText.StartsWith(TEXT("Forced")))NoticeText=TEXT("강제 사격 · 내부 아군 위험");
  if(NoticeText.StartsWith(TEXT("AUTO")))NoticeText=TEXT("자동화 변경");
  NoticeText=NoticeText.Left(24);
 }
 if(FPlatformTime::Seconds()<NoticeExpires)DrawText(NoticeText,PlayInk,716,26,GEngine->GetSmallFont());
 if(bSceneMenuOpen) {
  DrawRect(PlayPanel,216,63,426,139);
  Button(TEXT("CombatRange"),TEXT("기본 전투"),226,72,126);
  Button(TEXT("CalibrationRange"),TEXT("함대 교전"),362,72,126);
  Button(TEXT("Phase2C"),TEXT("모함 · 운송"),498,72,132);
  Button(TEXT("MissionAttack"),TEXT("강습 임무"),226,108,126);
  Button(TEXT("MissionDefense"),TEXT("방어 임무"),362,108,126);
  Button(TEXT("MissionFleetOnly"),TEXT("함대만 비교"),498,108,132);
  Button(TEXT("RestartScene"),TEXT("현재 장면 재시작"),226,148,190);
  Button(TEXT("SceneMenuClose"),TEXT("닫기"),498,148,132,true);
 }
 const int32 PageSize=FMath::Max(2,FMath::FloorToInt((H-360)/58));
 const int32 PageCount=FMath::Max(1,FMath::DivideAndRoundUp(PC->Ships.Num(),PageSize));
 FleetPage=FMath::Clamp(FleetPage,0,PageCount-1);
 DrawRect(PlayPanel,10,70,190,H-304);
 DrawText(TEXT("함대"),PlayAccent,22,80,GEngine->GetSmallFont());
 for(int32 Row=0;Row<PageSize;++Row) {
  const int32 I=FleetPage*PageSize+Row; if(!PC->Ships.IsValidIndex(I))break;
  const auto* Ship=PC->Ships[I].Get();if(!Ship||!Ship->Systems)continue;
  if(PC->Combat&&PC->Combat->bEnabled&&PC->Combat->IsDeparted(PC->Ships[I]))continue;
  const float Y=104+Row*58; const FLinearColor Color=Ship->bFriendly?PlayAccent:FLinearColor(1,.38f,.26f);
  Button(FName(*FString::Printf(TEXT("Pick_%d"),I)),(Ship->bFriendly?TEXT("◆ "):TEXT("◇ "))+Ship->ShipName.Left(15),18,Y,174,Ship==PC->Selected);
  DrawRect(Color,18,Y,3,48);
  DrawRect(FLinearColor(.05f,.1f,.16f),26,Y+32,158,10);
  DrawRect(Color,26,Y+32,158*FMath::Clamp(Ship->Systems->HullHP/FMath::Max(1.f,Ship->Systems->MaxHullHP),0.f,1.f),4);
  DrawRect(FLinearColor(.3f,.48f,1),26,Y+39,158*FMath::Clamp(Ship->Systems->Shield/FMath::Max(1.f,Ship->Systems->MaxShield),0.f,1.f),3);
 }
 if(PageCount>1) {
  Button(TEXT("FleetPrevious"),TEXT("이전"),20,H-268,68);
  Button(TEXT("FleetNext"),TEXT("다음"),120,H-268,68);
 }
 const float Y=H-220;
 DrawRect(PlayPanel,10,Y,610,210);
 if(!PC->Selected) {DrawText(TEXT("함선을 선택하세요"),PlayInk,24,Y+20,GEngine->GetMediumFont());return;}
 auto* Ship=PC->Selected.Get();
 DrawText(Ship->ShipName.Left(24),PlayAccent,24,Y+12,GEngine->GetMediumFont());
 Button(TEXT("Focus"),TEXT("추적"),315,Y+8,60,PC->bFollow);
 Button(TEXT("Details"),TEXT("함선 상세"),380,Y+8,75,bDetailsOpen);
 if(PC->Interior&&PC->Interior->bEnabled)Button(TEXT("InteriorToggle"),TEXT("선내 지도"),460,Y+8,75,bInteriorOpen);
 Button(TEXT("CommandHeroPanel"),TEXT("영웅"),540,Y+8,65,bHeroPanelOpen);
 const float Hull=FMath::Clamp(Ship->Systems->HullHP/FMath::Max(1.f,Ship->Systems->MaxHullHP),0.f,1.f);
 DrawRect(FLinearColor(.08f,.13f,.19f),24,Y+38,210,10);DrawRect(PlayAccent,24,Y+38,210*Hull,4);
 DrawRect(FLinearColor(.3f,.48f,1),24,Y+45,210*FMath::Clamp(Ship->Systems->Shield/FMath::Max(1.f,Ship->Systems->MaxShield),0.f,1.f),3);
 DrawText(TEXT("표적: ")+(Ship->AttackTarget?Ship->AttackTarget->ShipName.Left(20):TEXT("없음")),PlayInk,250,Y+40,GEngine->GetSmallFont());
 if(!Ship->bFriendly) {DrawText(TEXT("적 함선 · 관측된 외장 정보"),PlayInk,24,Y+72,GEngine->GetSmallFont());return;}
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
   const TCHAR* Kind=M.Profile.Kind==EOVAttackKind::Beam?TEXT("빔"):M.Profile.Kind==EOVAttackKind::Cannon?TEXT("함포"):TEXT("미사일");
   DrawText(FString::Printf(TEXT("%s %d  ·  %s%s"),Kind,PC->SelectedWeapon+1,*ReadinessKorean(R.Reason),M.bFireRequested?TEXT(" · 발사 접수"):TEXT("")),R.bReady?PlayAccent:FLinearColor(1,.8f,.25f),24,Y+166,GEngine->GetSmallFont());
  }
 }
 if(auto* Carrier=GetWorld()->GetSubsystem<UOVCarrierSubsystem>();Carrier&&Carrier->Carriers.ContainsByPredicate([&](const auto& C){return C.Carrier==Ship;})) {
  Button(TEXT("LaunchAircraft"),TEXT("함재기 출격"),380,Y+128,108);
  Button(TEXT("RecallAircraft"),TEXT("함재기 귀환"),495,Y+128,108);
 }
}

void AOVHUD::DrawWorldIndicators()
{
 auto* PC=Cast<AOVController>(PlayerOwner);
 auto Project=[&](FVector V,FVector2D& P) { if(!PC->ProjectWorldLocationToScreen(V,P)||IsOverPanel(P.X,P.Y))return false; P/=UIScale; return true; };
 for(int32 I=0;I<PC->Ships.Num();++I) {
  AOVShip* Ship=PC->Ships[I]; if(PC->Combat&&PC->Combat->bEnabled&&PC->Combat->IsDeparted(Ship))continue;
  FVector2D P; if(!Project(Ship->GetActorLocation(),P))continue;
  const bool Selected=PC->Selection.Contains(Ship),Target=PC->Selected&&PC->Selected->AttackTarget==Ship;
  FLinearColor Color=Ship->bFriendly?PlayAccent:FLinearColor(1,.3f,.16f);
  if(Ship->Systems->bWrecked)Color=FLinearColor::Gray;
  FVector2D Edge;
  const float Extent=Ship->Systems->Layout?(Ship->Systems->Layout->LocalHalfExtent*Ship->GetActorScale3D()).Size():200;
  float Radius=Selected?22:16;
  if(PC->ProjectWorldLocationToScreen(Ship->GetActorLocation()+PC->PlayerCameraManager->GetCameraRotation().RotateVector(FVector::RightVector)*Extent,Edge)) Radius=FMath::Clamp(float((Edge/UIScale-P).Size()),Radius,110.f);
  const FVector2D Label(FMath::Clamp(P.X+Radius+7,208.,Canvas->SizeX/UIScale-200.),FMath::Clamp(P.Y-Radius-20,100.,Canvas->SizeY/UIScale-155.));
  DrawLine(P.X-Radius,P.Y-Radius,P.X-Radius,P.Y+Radius,Color,Selected?3:1);
  DrawLine(P.X+Radius,P.Y-Radius,P.X+Radius,P.Y+Radius,Color,Selected?3:1);
  DrawLine(P.X-Radius,P.Y-Radius,P.X+Radius,P.Y-Radius,Color,Selected?3:1);
  if(Selected||Target||PC->CameraDistance<45000) DrawText((Target?TEXT("공격 ▶ "):Selected?TEXT("선택 ▶ "):TEXT(""))+Ship->ShipName,Color,Label.X,Label.Y,GEngine->GetSmallFont());
  const float Hull=FMath::Clamp(Ship->Systems->HullHP/FMath::Max(1.f,Ship->Systems->MaxHullHP),0.f,1.f);
  DrawRect(FLinearColor(.06f,.08f,.12f),Label.X,Label.Y+17,86,8);
  DrawRect(Color,Label.X,Label.Y+17,86*Hull,3);
  DrawRect(FLinearColor(.25f,.4f,1),Label.X,Label.Y+22,86*Ship->Systems->Shield/FMath::Max(1.f,Ship->Systems->MaxShield),3);
  if(Selected||Target)DrawText(FString::Printf(TEXT("고도 %.0fm"),Ship->GetActorLocation().Z/100),PlayInk,Label.X,Label.Y+29,GEngine->GetSmallFont());
  const FName Marker(*FString::Printf(TEXT("WorldPick_%d"),I));
  AddHitBox((P-FVector2D(Radius+6,Radius+6))*UIScale,FVector2D(2*Radius+12,2*Radius+12)*UIScale,Marker,true,5);
  ActionCenters.Add(Marker,P);
  if(Selected&&Ship->bHasDestination) { FVector2D D;if(Project(Ship->Destination,D))DrawText(FString::Printf(TEXT("이동 목적지  고도 %.0fm"),Ship->Destination.Z/100),FLinearColor::Green,D.X,D.Y,GEngine->GetSmallFont()); }
 }
 if(PC->bPlacing) { FVector2D P;if(Project(PC->Preview,P))DrawText(FString::Printf(TEXT("고도 %.0fm · 해제/Enter 확정"),PC->Preview.Z/100),FLinearColor::Yellow,P.X+14,P.Y-20,GEngine->GetSmallFont()); }
 TSet<FName> ShownImpacts;
 if(PC->Combat) for(int32 I=PC->Combat->RecentEvents.Num()-1;I>=0;--I) {
  const auto& E=PC->Combat->RecentEvents[I];
  if(ShownImpacts.Contains(E.HitShipId))continue;
  if(PC->Combat->GameTime-E.ResolvedAt>.65 || E.Result!=EOVAttackResult::Hit)continue;
  ShownImpacts.Add(E.HitShipId);
  FVector2D P;if(Project(E.Position,P))DrawText(E.HullDamage>0?TEXT("선체 명중"):TEXT("쉴드 / 장갑 명중"),FLinearColor(1,.8f,.2f),P.X,P.Y-45,GEngine->GetSmallFont());
 }
}
