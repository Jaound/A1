#include "OVHUD.h"
#include "OVController.h"
#include "OVMissionSubsystem.h"
#include "OVInteriorSubsystem.h"
#include "OVShip.h"
#include "OVShipSystemsComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

bool AOVHUD::IsOverMissionPanel(float X,float Y) const
{
 const auto* PC=Cast<AOVController>(PlayerOwner);if(!PC)return false;
 const FVector2D P(X/UIScale,Y/UIScale);
 const float EntryX=PC->bDeveloperMode?210:622;
 if(P.Y>=55&&P.Y<=82&&P.X>=EntryX&&P.X<=EntryX+416)return true;
 const auto* Mission=GetWorld()->GetSubsystem<UOVMissionSubsystem>();
 const float PanelX=PC->bDeveloperMode?450:10;
 return Mission&&Mission->bEnabled&&P.X>=PanelX&&P.X<=PanelX+328&&P.Y>=300&&P.Y<=598;
}

void AOVHUD::DrawMission()
{
 auto* PC=Cast<AOVController>(PlayerOwner);if(!PC)return;
 auto* Mission=GetWorld()->GetSubsystem<UOVMissionSubsystem>();if(!Mission)return;
 const FLinearColor Ink(.8f,.9f,1),Accent(.2f,.9f,1),Warning(1,.8f,.25f);
 const float EntryX=PC->bDeveloperMode?210:622;
 Button(TEXT("MissionAttack"),TEXT("공격 임무"),EntryX,55,126,Mission->bEnabled&&Mission->Mode==EOVMissionMode::Attack);
 Button(TEXT("MissionDefense"),TEXT("방어 임무"),EntryX+134,55,126,Mission->bEnabled&&Mission->Mode==EOVMissionMode::Defense);
 Button(TEXT("MissionFleetOnly"),TEXT("함대만 비교"),EntryX+268,55,146,Mission->bEnabled&&Mission->Mode==EOVMissionMode::FleetOnly);
 if(!Mission->bEnabled)return;
 const float X=PC->bDeveloperMode?450:10,Y=300;
 AddHitBox(FVector2D(X,Y)*UIScale,FVector2D(328,298)*UIScale,TEXT("MissionBackground"),true,9);
 DrawRect(FLinearColor(.008f,.02f,.04f,.98f),X,Y,328,298);
 const TCHAR* Mode=Mission->Mode==EOVMissionMode::Defense?TEXT("방어 작전"):Mission->Mode==EOVMissionMode::FleetOnly?TEXT("함대 교전 비교"):TEXT("통합 강습 작전");
 DrawText(FString::Printf(TEXT("%s · %.0f 게임초"),Mode,Mission->GameTime),Accent,X+10,Y+10,GEngine->GetSmallFont());
 const FString Public=Mission->GetPublicStatus();
 DrawText(Public.Left(40),Warning,X+10,Y+32,GEngine->GetSmallFont());
 if(Public.Len()>40)DrawText(Public.Mid(40,40),Warning,X+10,Y+48,GEngine->GetSmallFont());
 auto Pair=[&](FName A,const TCHAR* AL,FName B,const TCHAR* BL,float Row)
 { Button(A,AL,X+10,Y+Row,150);Button(B,BL,X+168,Y+Row,150); };
 if(Mission->Mode==EOVMissionMode::Attack)
 {
  Pair(TEXT("MissionPods"),TEXT("개인 포드 투입"),TEXT("MissionShuttle"),TEXT("강습선 투입"),68);
  Pair(TEXT("MissionShield"),TEXT("쉴드실 확보"),TEXT("MissionBridge"),TEXT("함교 확보"),101);
  Pair(TEXT("MissionSabotage"),TEXT("쉴드실 공작"),TEXT("MissionRetreat"),TEXT("철수 · 실제 회수"),134);
 }
 else if(Mission->Mode==EOVMissionMode::Defense)
 {
  Pair(TEXT("MissionDefendShield"),TEXT("쉴드실 수호"),TEXT("MissionDefendBridge"),TEXT("함교 수호"),68);
  Pair(TEXT("MissionShield"),TEXT("쉴드실 재확보"),TEXT("MissionBridge"),TEXT("함교 재확보"),101);
  DrawText(TEXT("선내 지도에서 실제 아군의 목표를 변경하세요."),Ink,X+10,Y+140,GEngine->GetSmallFont());
 }
 else
 {
  DrawText(TEXT("같은 함대 편성 · 영웅 강습 개입 없음"),Ink,X+10,Y+78,GEngine->GetSmallFont());
  DrawText(TEXT("전투 시간을 강제로 늘리거나 회복하지 않습니다."),Ink,X+10,Y+102,GEngine->GetSmallFont());
 }
 Pair(TEXT("MissionAircraftLaunch"),TEXT("함재기 출격"),TEXT("MissionAircraftRecall"),TEXT("함재기 명시 복귀"),167);
 if(PC->Interior&&PC->Interior->bEnabled)Pair(TEXT("InteriorMissionToggle"),TEXT("선내 지도 / 명령"),TEXT("MissionRestart"),TEXT("임무 다시 시작"),200);
 else Button(TEXT("MissionRestart"),TEXT("임무 다시 시작"),X+168,Y+200,150);
 // 자체 손실만 표시한다. 적 로스터·예비 기체·레벨·수리 화물은 표시하지 않는다.
 const auto Score=Mission->GetScore();
 const auto* Own=Score.Factions.FindByPredicate([](const auto& F){return F.FactionId==0;});
 if(Own)
 {
  DrawText(FString::Printf(TEXT("아군 손실  함선 %d · 영웅 %d · 병력 %d"),Own->ShipsLost,Own->HeroesDead,Own->TroopsDead),Ink,X+10,Y+237,GEngine->GetSmallFont());
  DrawText(FString::Printf(TEXT("기체 생존 %d · 손실 %d / 구조 대기 %d"),Own->AircraftSurviving,Own->AircraftLost,Own->HeroesRescueWaiting),Ink,X+10,Y+257,GEngine->GetSmallFont());
 }
 if(Mission->Result!=EOVMissionResult::Ongoing)
 {
  const TCHAR* Results[]={TEXT("진행"),TEXT("임무 성공"),TEXT("임무 실패"),TEXT("철수 완료"),TEXT("작전 미해결")};
  DrawText(Results[FMath::Clamp(int32(Mission->Result),0,4)],Accent,X+10,Y+278,GEngine->GetSmallFont());
 }
 else DrawText(TEXT("내부 아군 보호 유지 · 기발사 공격은 계속 진행"),Ink,X+10,Y+278,GEngine->GetSmallFont());
}
