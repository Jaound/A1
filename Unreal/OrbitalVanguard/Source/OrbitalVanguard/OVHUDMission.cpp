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
 const auto* PC=Cast<AOVController>(PlayerOwner);if(!PC||PC->bDeveloperMode)return false;
 const auto* Mission=GetWorld()->GetSubsystem<UOVMissionSubsystem>();
 const FVector2D P(X/UIScale,Y/UIScale);
 return Mission&&Mission->bEnabled&&P.X>=212&&P.X<=514&&P.Y>=76&&P.Y<=302;
}
void AOVHUD::DrawMission()
{
 auto* PC=Cast<AOVController>(PlayerOwner);if(!PC||PC->bDeveloperMode||bSceneMenuOpen)return;
 auto* Mission=GetWorld()->GetSubsystem<UOVMissionSubsystem>();if(!Mission||!Mission->bEnabled)return;
 const FLinearColor Ink(.8f,.9f,1),Accent(.2f,.9f,1),Warning(1,.8f,.25f);
 const float X=212,Y=76;
 AddHitBox(FVector2D(X,Y)*UIScale,FVector2D(302,226)*UIScale,TEXT("MissionBackground"),true,9);
 DrawRect(FLinearColor(.008f,.02f,.04f,.94f),X,Y,302,226);
 const TCHAR* Phases[]={TEXT("적 쉴드 돌파"),TEXT("강습 투입 가능"),TEXT("강습 인원 이동 중"),TEXT("선내 목표를 지시하세요"),TEXT("생존 인원 회수 중"),TEXT("작전 종료")};
 DrawText(Phases[FMath::Clamp(int32(Mission->Phase),0,5)],Accent,X+12,Y+10,GEngine->GetMediumFont());
 for(int32 I=0;I<6;++I)DrawRect(I<=int32(Mission->Phase)?Accent:FLinearColor(.07f,.13f,.2f),X+12+I*46,Y+38,40,3);
 auto Pair=[&](FName A,const TCHAR* AL,FName B,const TCHAR* BL,float Row)
 {Button(A,AL,X+12,Y+Row,134);Button(B,BL,X+154,Y+Row,134);};
 if(Mission->Mode==EOVMissionMode::Attack) {
  Pair(TEXT("MissionPods"),TEXT("포드 투입"),TEXT("MissionShuttle"),TEXT("강습선 투입"),54);
  Pair(TEXT("MissionShield"),TEXT("쉴드실 점령"),TEXT("MissionBridge"),TEXT("함교 점령"),88);
  Pair(TEXT("MissionSabotage"),TEXT("쉴드실 파괴"),TEXT("MissionRetreat"),TEXT("철수"),122);
 } else if(Mission->Mode==EOVMissionMode::Defense) {
  Pair(TEXT("MissionDefendShield"),TEXT("쉴드실 수호"),TEXT("MissionDefendBridge"),TEXT("함교 수호"),54);
  Pair(TEXT("MissionShield"),TEXT("쉴드실 탈환"),TEXT("MissionBridge"),TEXT("함교 탈환"),88);
 } else DrawText(TEXT("함대만 교전 중"),Ink,X+12,Y+64,GEngine->GetSmallFont());
 Pair(TEXT("InteriorMissionToggle"),TEXT("선내 지도"),TEXT("HeroPanel"),TEXT("영웅 위치"),158);
 const auto Score=Mission->GetScore();
 DrawText(FString::Printf(TEXT("진입 %d  ·  회수 %d"),Score.Entered,Score.Returned),Ink,X+12,Y+194,GEngine->GetSmallFont());
 if(Mission->Result!=EOVMissionResult::Ongoing) {
  const TCHAR* Results[]={TEXT("진행"),TEXT("성공"),TEXT("실패"),TEXT("철수 완료"),TEXT("미해결")};
  DrawText(Results[FMath::Clamp(int32(Mission->Result),0,4)],Warning,X+216,Y+194,GEngine->GetSmallFont());
 }
}
