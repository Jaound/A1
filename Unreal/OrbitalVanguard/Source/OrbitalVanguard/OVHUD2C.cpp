#include "OVHUD.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "OVController.h"
#include "OVShip.h"
#include "OVShipSystemsComponent.h"
#include "OVInteriorSubsystem.h"
#include "OVCarrierSubsystem.h"
#include "OVTransportSubsystem.h"
#include "OVPointDefenseSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/Canvas.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"

void AOVHUD::DrawPhase2C()
{
    CSV_SCOPED_TIMING_STAT_EXCLUSIVE(OV_Effects);
    TRACE_CPUPROFILER_EVENT_SCOPE(OV_Effects);
 auto* PC=Cast<AOVController>(PlayerOwner);if(!PC)return;
 PC->UpdateAircraftPresentation();
 if(PC->Phase2CScenario<0&&!(PC->Interior&&PC->Interior->bEnabled))return;
 auto* Carrier=GetWorld()->GetSubsystem<UOVCarrierSubsystem>();
 auto* PD=GetWorld()->GetSubsystem<UOVPointDefenseSubsystem>();
 auto* Transport=GetWorld()->GetSubsystem<UOVTransportSubsystem>();
 const FLinearColor Ink(.8f,.9f,1),Cyan(.2f,.9f,1),Gold(1,.75f,.2f);
 auto Project=[&](FVector V,FVector2D& P) { if(!PC->ProjectWorldLocationToScreen(V,P)||IsOverPanel(P.X,P.Y))return false;P/=UIScale;return true; };
 TSet<FName> ShownStrikeZones;
 for(const auto& A:Carrier->Aircraft) if(UOVCarrierSubsystem::IsAirborneState(A.State)) {
  FVector2D P;if(!Project(A.Position,P))continue;
  const auto Color=A.FactionId==0?Cyan:FLinearColor(1,.3f,.15f);
  DrawLine(P.X-4,P.Y+3,P.X,P.Y-5,Color,2);DrawLine(P.X,P.Y-5,P.X+4,P.Y+3,Color,2);DrawLine(P.X-4,P.Y+3,P.X+4,P.Y+3,Color,2);
  // 기체별 내부 상태는 아군 DEV에만 표시한다. 기본 화면은 선택 모함 한 줄로 요약한다.
  if(PC->bDeveloperMode&&A.FactionId==0) {
   if(A.State==EOVAircraftState::Returning)DrawText(TEXT("귀환"),Color,P.X+5,P.Y,GEngine->GetSmallFont());
   if(A.State==EOVAircraftState::ControlLost)DrawText(TEXT("관제 상실"),Gold,P.X+5,P.Y,GEngine->GetSmallFont());
  }
  if(PC->bDeveloperMode && A.FactionId==0 && A.Carrier==PC->Selected && A.StrikeTarget.IsValid()
     && (A.AircraftId-1)%FMath::Max(1,Carrier->ManeuverProfile.SectionSize)==0) {
   const TCHAR* Names[]={TEXT("접근"),TEXT("공격"),TEXT("짧은 이탈"),TEXT("선회·재집결"),TEXT("제한 공중전"),TEXT("대기")};
   DrawText(FString::Printf(TEXT("DEV #%d %s / 통과 %d"),A.AircraftId,Names[static_cast<int32>(A.Maneuver)],A.AttackPasses),Gold,P.X+7,P.Y+9,GEngine->GetSmallFont());
   FVector2D Approach,Egress,Regroup;
   if(Project(A.ApproachPoint,Approach)&&Project(A.EgressPoint,Egress)&&Project(A.RegroupPoint,Regroup)) {
    DrawLine(Approach.X,Approach.Y,Egress.X,Egress.Y,Cyan,1);
    DrawLine(Egress.X,Egress.Y,Regroup.X,Regroup.Y,Gold,1);
    DrawLine(Regroup.X,Regroup.Y,Approach.X,Approach.Y,FLinearColor::Green,1);
    DrawText(TEXT("접근"),Cyan,Approach.X,Approach.Y,GEngine->GetSmallFont());
    DrawText(TEXT("이탈"),Gold,Egress.X,Egress.Y,GEngine->GetSmallFont());
    DrawText(TEXT("재집결"),FLinearColor::Green,Regroup.X,Regroup.Y,GEngine->GetSmallFont());
   }
   if(!ShownStrikeZones.Contains(A.StrikeTarget->GetFName())) {
    ShownStrikeZones.Add(A.StrikeTarget->GetFName());
    DrawDebugSphere(GetWorld(),A.StrikeCenter,A.AllowedRadius,16,FColor::Purple,false,-1,0,1);
   }
  }
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
 if(PC->bDeveloperMode||bSceneMenuOpen||PC->Phase2CScenario<0||!PC->Selected||!PC->Selected->Systems||PC->Selected->Systems->FactionId!=0)return;
 const float X=212,Y=76;
 DrawRect(FLinearColor(.008f,.02f,.04f,.94f),X,Y,272,148);
 AddHitBox(FVector2D(X,Y)*UIScale,FVector2D(272,148)*UIScale,TEXT("MissionBackground"),true,9);
 const auto Defense=PD->GetShipState(PC->Selected);
 FString Priority;for(const auto Kind:Defense.Priority) {if(!Priority.IsEmpty())Priority+=TEXT(" > ");Priority+=Kind==EOVPDTargetKind::Missile?TEXT("미사일"):Kind==EOVPDTargetKind::Aircraft?TEXT("함재기"):TEXT("포드");}
 DrawText(TEXT("요격 우선순위"),Cyan,X+12,Y+10,GEngine->GetSmallFont());
 Button(TEXT("PDPriority"),Priority,X+12,Y+30,248);
 const bool HasCarrier=Carrier->Carriers.ContainsByPredicate([&](const auto& S){return S.Carrier==PC->Selected;});
 if(HasCarrier) {
  DrawText(FString::Printf(TEXT("보관 %d   비행 %d   손실 %d"),Carrier->CountAircraft(PC->Selected,EOVAircraftState::Stored),Carrier->CountAircraft(PC->Selected,EOVAircraftState::Sortie)+Carrier->CountAircraft(PC->Selected,EOVAircraftState::Returning),Carrier->CountAircraft(PC->Selected,EOVAircraftState::Wrecked)),Ink,X+12,Y+72,GEngine->GetSmallFont());
  DrawText(TEXT("출격 · 귀환은 하단 명령에서"),Ink,X+12,Y+106,GEngine->GetSmallFont());
 } else if(PC->Phase2CScenario>=3) {
  DrawText(TEXT("운송 시험 · 임시 승객"),Gold,X+12,Y+72,GEngine->GetSmallFont());
  Button(PC->Phase2CScenario==3?TEXT("LaunchPods"):TEXT("DispatchShuttle"),TEXT("출발"),X+12,Y+103,118);
  Button(PC->Phase2CScenario==3?TEXT("RecallPods"):TEXT("RecallShuttle"),TEXT("귀환"),X+140,Y+103,118);
 }
}
