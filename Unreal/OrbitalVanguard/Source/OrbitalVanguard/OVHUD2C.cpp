#include "OVHUD.h"
#include "OVController.h"
#include "OVShip.h"
#include "OVCarrierSubsystem.h"
#include "OVTransportSubsystem.h"
#include "OVPointDefenseSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/Canvas.h"
#include "Engine/World.h"

void AOVHUD::DrawPhase2C()
{
 auto* PC=Cast<AOVController>(PlayerOwner);if(PC->Phase2CScenario<0)return;
 auto* Carrier=GetWorld()->GetSubsystem<UOVCarrierSubsystem>();
 auto* PD=GetWorld()->GetSubsystem<UOVPointDefenseSubsystem>();
 auto* Transport=GetWorld()->GetSubsystem<UOVTransportSubsystem>();
 const FLinearColor Ink(.8f,.9f,1),Cyan(.2f,.9f,1),Gold(1,.75f,.2f);
 auto Project=[&](FVector V,FVector2D& P) { if(!PC->ProjectWorldLocationToScreen(V,P)||IsOverPanel(P.X,P.Y))return false;P/=UIScale;return true; };
 for(const auto& A:Carrier->Aircraft) if(UOVCarrierSubsystem::IsAirborneState(A.State)) {
  FVector2D P;if(!Project(A.Position,P))continue;
  const auto Color=A.FactionId==0?Cyan:FLinearColor(1,.3f,.15f);
  DrawLine(P.X-4,P.Y+3,P.X,P.Y-5,Color,2);DrawLine(P.X,P.Y-5,P.X+4,P.Y+3,Color,2);DrawLine(P.X-4,P.Y+3,P.X+4,P.Y+3,Color,2);
  if(A.State==EOVAircraftState::Returning)DrawText(TEXT("귀환"),Color,P.X+5,P.Y,GEngine->GetSmallFont());
  if(A.State==EOVAircraftState::ControlLost)DrawText(TEXT("관제 상실"),Gold,P.X+5,P.Y,GEngine->GetSmallFont());
 }
 for(const auto& B:Carrier->Bolts) { FVector2D P,Q;if(Project(B.Position,P)&&Project(B.Position-B.Direction*600,Q))DrawLine(P.X,P.Y,Q.X,Q.Y,B.FactionId==0?Cyan:FLinearColor::Red,2); }
 for(const auto& E:Carrier->RecentEvents) if(Carrier->GameTime-E.ResolvedAt<.3&&E.bHit) { FVector2D P;if(Project(E.Position,P)){DrawLine(P.X-7,P.Y-7,P.X+7,P.Y+7,Gold,2);DrawLine(P.X+7,P.Y-7,P.X-7,P.Y+7,Gold,2);} }
 for(const auto& P:Transport->Pods) if(P.State==EOVPodFlightState::Outbound||P.State==EOVPodFlightState::Returning) {
  FVector2D V;if(!Project(P.Position,V))continue;
  DrawRect(Gold,V.X-3,V.Y-3,6,6);
  DrawText((P.State==EOVPodFlightState::Returning?TEXT("귀환 "):TEXT("포드 "))+FString::FromInt(P.PodId),Gold,V.X+6,V.Y,GEngine->GetSmallFont());
 }
 for(const auto& E:PD->RecentEvents) if(PD->GameTime-E.GameTime<.18) {
  FVector2D P,Q;if(!Project(E.Origin,P)||!Project(E.TargetPosition,Q))continue;
  const auto Color=E.WeaponKind==EOVPDWeaponKind::Beam?FLinearColor(.55f,1,.7f):Gold;
  DrawLine(P.X,P.Y,Q.X,Q.Y,Color,E.bHit?2:1);
  if(E.bDestroyed)DrawText(TEXT("요격 성공"),Color,Q.X,Q.Y-14,GEngine->GetSmallFont());
 }
 if(!PC->Selected||!PC->Selected->bFriendly)return;
 const float X=PC->bDeveloperMode?450:10,Y=300;
 DrawRect(FLinearColor(.008f,.02f,.04f,.94f),X,Y,246,296);
 DrawText(FString::Printf(TEXT("요격  발사 %d · 격추 %d"),PD->ShotCount,PD->KillCount),Cyan,X+10,Y+10,GEngine->GetSmallFont());
 const auto Defense=PD->GetShipState(PC->Selected);
 FString Priority;for(const auto Kind:Defense.Priority) { if(!Priority.IsEmpty())Priority+=TEXT(" > ");Priority+=Kind==EOVPDTargetKind::Missile?TEXT("미사일"):Kind==EOVPDTargetKind::Aircraft?TEXT("함재기"):TEXT("포드"); }
 Button(TEXT("PDPriority"),Priority,X+10,Y+32,226);
 const bool HasCarrier=Carrier->Carriers.ContainsByPredicate([&](const auto& S){return S.Carrier==PC->Selected;});
 float Row=Y+68;
 if(HasCarrier) {
  DrawText(FString::Printf(TEXT("보관 %d · 비행 %d · 손실 %d"),Carrier->CountAircraft(PC->Selected,EOVAircraftState::Stored),Carrier->CountAircraft(PC->Selected,EOVAircraftState::Sortie)+Carrier->CountAircraft(PC->Selected,EOVAircraftState::Returning),Carrier->CountAircraft(PC->Selected,EOVAircraftState::Wrecked)),Ink,X+10,Row,GEngine->GetSmallFont());
  Button(TEXT("LaunchAircraft"),TEXT("함재기 출격"),X+10,Row+22,109);Button(TEXT("RecallAircraft"),TEXT("명시 복귀"),X+127,Row+22,109);
  Button(TEXT("TrialFTL"),TEXT("FTL 시험 충전"),X+10,Row+56,109);Button(TEXT("TrialDeparture"),TEXT("회수 후 이탈"),X+127,Row+56,109);
  Button(TEXT("AbandonAircraft"),TEXT("미귀환 기체 포기 선택"),X+10,Row+90,226);
  DrawText(Carrier->GetCarrierStatus(PC->Selected).Left(30),Gold,X+10,Row+121,GEngine->GetSmallFont());
  if(PC->bDeveloperMode)Button(TEXT("DevFighterRepair"),TEXT("DEV 시험 공급 수리"),X+10,Row+144,226);
  if(PC->Phase2CScenario!=5)return;
  // Mixed scene's transport controls remain reachable by selecting the assault ship.
  DrawText(TEXT("강습선 선택 → 운송 명령"),Ink,X+10,Y+268,GEngine->GetSmallFont());return;
 }
 if(PC->Phase2CScenario>=3) {
  DrawText(TEXT("DEV 승객 토큰 · 선내전 미구현"),Gold,X+10,Row,GEngine->GetSmallFont());
  DrawText(Transport->GetSummary().Left(32),Ink,X+10,Row+20,GEngine->GetSmallFont());
  if(PC->Phase2CScenario==3) {
   Button(TEXT("LaunchPods"),TEXT("개인 포드 출발"),X+10,Row+46,109);Button(TEXT("RecallPods"),TEXT("비행 포드 회항"),X+127,Row+46,109);
  } else {
   Button(TEXT("DispatchShuttle"),TEXT("강습선 이송"),X+10,Row+46,109);Button(TEXT("RecallShuttle"),TEXT("수송선 회수"),X+127,Row+46,109);
  }
  Button(TEXT("DevEntryReturn"),TEXT("DEV 원 진입점 복귀 확인"),X+10,Row+82,226);
  DrawText(TEXT("실내 이동 대신 시험 이벤트 입력"),Gold,X+10,Row+116,GEngine->GetSmallFont());
  if(PC->bDeveloperMode&&PC->Phase2CScenario!=3)Button(TEXT("DevLoseShuttle"),TEXT("DEV 수송선 상실"),X+10,Row+143,226);
 }
}
