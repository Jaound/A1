#include "OVHUD.h"
#include "OVController.h"
#include "OVShip.h"
#include "OVShipSystemsComponent.h"
#include "OVCombatSubsystem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"

namespace {
const FLinearColor PlayInk(.8f,.9f,1), PlayAccent(.2f,.9f,1), PlayPanel(.008f,.02f,.04f,.94f);
FString ReadinessKorean(const FString& Key)
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
}

FVector2D AOVHUD::GetActionCenter(FName Action) const
{
 if(const FVector2D* Point=ActionCenters.Find(Action))return *Point;
 return FVector2D(-1,-1);
}

void AOVHUD::DrawPlayHUD()
{
 auto* PC=Cast<AOVController>(PlayerOwner);
 const float W=Canvas->SizeX/UIScale,H=Canvas->SizeY/UIScale;
 DrawRect(PlayPanel,10,10,W-20,82);
 DrawText(TEXT("오비탈 뱅가드  /  ")+PC->GetScenarioLabel(),PlayAccent,22,20,GEngine->GetSmallFont());
 Button(TEXT("Pause"),PC->bSimulationPaused?TEXT("F1 시작"):TEXT("F1 정지"),390,16,90,PC->bSimulationPaused);
 Button(TEXT("Speed1"),TEXT("1배"),488,16,52,PC->SimulationSpeed==1);
 Button(TEXT("Speed2"),TEXT("2배"),548,16,52,PC->SimulationSpeed==2);
 Button(TEXT("Speed3"),TEXT("3배"),608,16,52,PC->SimulationSpeed==3);
 Button(TEXT("Overview"),TEXT("Home 전체 보기"),676,16,134);
 Button(TEXT("Developer"),TEXT("F8 개발 도구"),W-144,16,122);
 Button(TEXT("CombatRange"),TEXT("F5 기본 전투"),22,55,130);
 Button(TEXT("CalibrationRange"),TEXT("F6 교정 전투"),160,55,130);
 Button(TEXT("Phase2C"),TEXT("F7 다음 2C 장면"),298,55,158);
 Button(TEXT("RestartScene"),TEXT("이 장면 재시작"),464,55,136);
 DrawText(TEXT("휠 확대 · 중클릭 회전 · 우클릭 드래그 고도 · F 추적"),PlayInk,622,62,GEngine->GetSmallFont());
 for(int32 I=0;I<PC->Ships.Num();++I) {
  const AOVShip* Ship=PC->Ships[I];
  if(PC->Combat&&PC->Combat->bEnabled&&PC->Combat->IsDeparted(PC->Ships[I])) continue;
  const float Y=104+I*36;
  Button(FName(*FString::Printf(TEXT("Pick_%d"),I)),(Ship->bFriendly?TEXT("아군 · "):TEXT("적군 · "))+Ship->ShipName.Left(18),12,Y,184,Ship==PC->Selected);
 }
 const float Y=H-120;
 DrawRect(PlayPanel,10,Y,W-20,110);
 if(!PC->Selected) { DrawText(TEXT("함선을 클릭하거나 왼쪽 목록에서 선택하세요."),PlayInk,22,Y+14,GEngine->GetSmallFont()); return; }
 auto* Ship=PC->Selected.Get();
 DrawText(Ship->ShipName+FString::Printf(TEXT("   선체 %.0f%%   쉴드 %.0f%%"),100*Ship->Systems->HullHP/FMath::Max(1.f,Ship->Systems->MaxHullHP),100*Ship->Systems->Shield/FMath::Max(1.f,Ship->Systems->MaxShield)),PlayAccent,22,Y+10,GEngine->GetSmallFont());
 DrawText(TEXT("표적: ")+(Ship->AttackTarget?Ship->AttackTarget->ShipName:TEXT("없음")),PlayInk,470,Y+10,GEngine->GetSmallFont());
 Button(TEXT("Focus"),TEXT("F 추적"),W-212,Y+4,88,PC->bFollow);
 Button(TEXT("Details"),TEXT("상세 보기"),W-116,Y+4,92,bDetailsOpen);
 if(Ship->bFriendly) {
  const FName Actions[]={TEXT("Move"),TEXT("Stop"),TEXT("Target"),TEXT("Fire"),TEXT("FireAll"),TEXT("NextWeapon"),TEXT("MoveAuto"),TEXT("FireAuto"),TEXT("GunIntent"),TEXT("CancelAttack")};
  const FString Labels[]={TEXT("V 이동"),TEXT("X 정지"),TEXT("T 표적"),TEXT("B 한 무장"),TEXT("L 전 무장"),TEXT("N 무장 변경"),Ship->IsMovementAutomatic()?TEXT("이동 자동"):TEXT("이동 수동"),Ship->IsAttackAutomatic()?TEXT("사격 자동"):TEXT("사격 수동"),TEXT("무장 무력화"),TEXT("사격 취소")};
  for(int32 I=0;I<10;++I) Button(Actions[I],Labels[I],22+I*112,Y+32,104);
  if(PC->Combat&&PC->Combat->bEnabled) {
   const auto State=PC->Combat->GetShipState(Ship);
   if(State.Mounts.IsValidIndex(PC->SelectedWeapon)) {
    const auto& M=State.Mounts[PC->SelectedWeapon];const auto R=PC->Combat->GetReadiness(Ship,M.MountId);
    const TCHAR* Kind=M.Profile.Kind==EOVAttackKind::Beam?TEXT("빔"):M.Profile.Kind==EOVAttackKind::Cannon?TEXT("함포"):M.Profile.Kind==EOVAttackKind::Missile?TEXT("미사일"):TEXT("유도탄");
    DrawText(FString::Printf(TEXT("%s %d  ·  %s  ·  대기 %.1f초%s"),Kind,PC->SelectedWeapon+1,*ReadinessKorean(R.Reason),R.Cooldown,M.bFireRequested?TEXT("  / 발사 요청 접수"):TEXT("")),R.bReady?FLinearColor::Green:FLinearColor(1,.8f,.25f),22,Y+67,GEngine->GetSmallFont());
   }
  }
 }
 DrawText(PC->LastAction.Left(100),PlayInk,22,Y+87,GEngine->GetSmallFont());
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
  const float Radius=Selected?22:16;
  DrawLine(P.X-Radius,P.Y-Radius,P.X-Radius,P.Y+Radius,Color,Selected?3:1);
  DrawLine(P.X+Radius,P.Y-Radius,P.X+Radius,P.Y+Radius,Color,Selected?3:1);
  DrawLine(P.X-Radius,P.Y-Radius,P.X+Radius,P.Y-Radius,Color,Selected?3:1);
  DrawText((Target?TEXT("공격 ▶ "):Selected?TEXT("선택 ▶ "):TEXT(""))+Ship->ShipName,Color,P.X+26,P.Y-23,GEngine->GetSmallFont());
  const float Hull=FMath::Clamp(Ship->Systems->HullHP/FMath::Max(1.f,Ship->Systems->MaxHullHP),0.f,1.f);
  DrawRect(FLinearColor(.06f,.08f,.12f),P.X+26,P.Y-5,86,8);
  DrawRect(Color,P.X+26,P.Y-5,86*Hull,3);
  DrawRect(FLinearColor(.25f,.4f,1),P.X+26,P.Y,86*Ship->Systems->Shield/FMath::Max(1.f,Ship->Systems->MaxShield),3);
  DrawText(FString::Printf(TEXT("고도 %.0fm"),Ship->GetActorLocation().Z/100),PlayInk,P.X+26,P.Y+9,GEngine->GetSmallFont());
  AddHitBox((P-FVector2D(24,24))*UIScale,FVector2D(140,60)*UIScale,FName(*FString::Printf(TEXT("Pick_%d"),I)),true,5);
  if(Selected&&Ship->bHasDestination) { FVector2D D;if(Project(Ship->Destination,D))DrawText(FString::Printf(TEXT("이동 목적지  고도 %.0fm"),Ship->Destination.Z/100),FLinearColor::Green,D.X,D.Y,GEngine->GetSmallFont()); }
 }
 if(PC->bPlacing) { FVector2D P;if(Project(PC->Preview,P))DrawText(FString::Printf(TEXT("고도 %.0fm · 해제/Enter 확정"),PC->Preview.Z/100),FLinearColor::Yellow,P.X+14,P.Y-20,GEngine->GetSmallFont()); }
 if(PC->Combat) for(const auto& E:PC->Combat->RecentEvents) {
  if(PC->Combat->GameTime-E.ResolvedAt>.65 || E.Result!=EOVAttackResult::Hit)continue;
  FVector2D P;if(Project(E.Position,P))DrawText(E.HullDamage>0?TEXT("선체 명중"):TEXT("쉴드 / 장갑 명중"),FLinearColor(1,.8f,.2f),P.X,P.Y-36,GEngine->GetSmallFont());
 }
}
