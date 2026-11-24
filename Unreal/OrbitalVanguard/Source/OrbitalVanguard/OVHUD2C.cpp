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
 struct FFlightMarker { FVector2D Center=FVector2D::ZeroVector, Heading=FVector2D::ZeroVector; int32 Count=0; bool Friendly=false; };
 TMap<FIntVector,FFlightMarker> Flights;
 FVector CameraPosition; FRotator CameraRotation;
 PC->GetPlayerViewPoint(CameraPosition, CameraRotation);
 const FVector CameraRight = FRotationMatrix(CameraRotation).GetUnitAxis(EAxis::Y);
 const auto WingColor = [&](bool Friendly){ return Friendly ? Cyan : FLinearColor(1,.56f,.22f); };
 for(const auto& A:Carrier->Aircraft) if(UOVCarrierSubsystem::IsAirborneState(A.State)) {
  FVector2D P;if(!Project(A.Position,P))continue;
  const bool Friendly = A.FactionId == 0;
  const auto Color = WingColor(Friendly);
  FVector2D Edge, Nose;
  if (!PC->ProjectWorldLocationToScreen(A.Position+CameraRight*Carrier->Profile.CollisionRadius, Edge) ||
      !PC->ProjectWorldLocationToScreen(A.Position+A.Direction*Carrier->Profile.CollisionRadius, Nose)) continue;
  const double Radius = FVector2D::Distance(P, Edge/UIScale);
  const FVector2D Heading = (Nose/UIScale-P).GetSafeNormal();
  if (Radius < 3.)
  {
   // At strategic zoom several sections share a few pixels. Count visible airborne craft by side and region.
   const FIntVector Key = Radius < 1.5
       ? FIntVector(A.FactionId,-1-FMath::FloorToInt(P.X/72.),FMath::FloorToInt(P.Y/48.))
       : FIntVector(A.FactionId,A.HangarId,(A.AircraftId-1)/FMath::Max(1,Carrier->ManeuverProfile.SectionSize));
   auto& Flight = Flights.FindOrAdd(Key);
   Flight.Center+=P; Flight.Heading+=Heading; ++Flight.Count; Flight.Friendly=Friendly;
  }
  else if (!A.Velocity.IsNearlyZero())
  {
   const FVector2D Exhaust = P - Heading * Radius;
   const FVector2D Tail = Exhaust - Heading * FMath::Clamp(Radius*1.3,3.,12.);
   DrawLine(Exhaust.X,Exhaust.Y,Tail.X,Tail.Y,Color.CopyWithNewOpacity(.65f),1.6f);
  }
  // 기체별 내부 상태는 아군 DEV에만 표시한다. 기본 화면은 선택 모함 한 줄로 요약한다.
  if(PC->bDeveloperMode&&A.FactionId==0) {
   if(A.State==EOVAircraftState::Returning)DrawText(TEXT("귀환"),Color,P.X+5,P.Y,OVTypography::Font(EOVTextRole::Body));
   if(A.State==EOVAircraftState::ControlLost)DrawText(TEXT("관제 상실"),Gold,P.X+5,P.Y,OVTypography::Font(EOVTextRole::Body));
  }
  if(PC->bDeveloperMode && A.FactionId==0 && A.Carrier==PC->Selected && A.StrikeTarget.IsValid()
     && (A.AircraftId-1)%FMath::Max(1,Carrier->ManeuverProfile.SectionSize)==0) {
   const TCHAR* Names[]={TEXT("접근"),TEXT("공격"),TEXT("짧은 이탈"),TEXT("선회·재집결"),TEXT("제한 공중전"),TEXT("대기")};
   DrawText(FString::Printf(TEXT("DEV #%d %s / 통과 %d"),A.AircraftId,Names[static_cast<int32>(A.Maneuver)],A.AttackPasses),Gold,P.X+7,P.Y+9,OVTypography::Font(EOVTextRole::Numeric));
   FVector2D Approach,Egress,Regroup;
   if(Project(A.ApproachPoint,Approach)&&Project(A.EgressPoint,Egress)&&Project(A.RegroupPoint,Regroup)) {
    DrawLine(Approach.X,Approach.Y,Egress.X,Egress.Y,Cyan,1);
    DrawLine(Egress.X,Egress.Y,Regroup.X,Regroup.Y,Gold,1);
    DrawLine(Regroup.X,Regroup.Y,Approach.X,Approach.Y,FLinearColor::Green,1);
    DrawText(TEXT("접근"),Cyan,Approach.X,Approach.Y,OVTypography::Font(EOVTextRole::Body));
    DrawText(TEXT("이탈"),Gold,Egress.X,Egress.Y,OVTypography::Font(EOVTextRole::Body));
    DrawText(TEXT("재집결"),FLinearColor::Green,Regroup.X,Regroup.Y,OVTypography::Font(EOVTextRole::Body));
   }
   if(!ShownStrikeZones.Contains(A.StrikeTarget->GetFName())) {
    ShownStrikeZones.Add(A.StrikeTarget->GetFName());
    DrawDebugSphere(GetWorld(),A.StrikeCenter,A.AllowedRadius,16,FColor::Purple,false,-1,0,1);
   }
  }
 }
 for (const auto& Pair : Flights)
 {
  const auto& F=Pair.Value;
  FVector2D P=F.Center/F.Count;
  if (Pair.Key.Y < 0) P.Y += F.Friendly ? -7. : 7.;
  if (IsOverPanel(P.X*UIScale,P.Y*UIScale)) continue;
  const auto D=F.Heading.IsNearlyZero()?FVector2D(0,-1):F.Heading.GetSafeNormal();
  const FVector2D Side(-D.Y,D.X), Tip=P+D*6, Left=P-D*3+Side*4, Right=P-D*3-Side*4;
  const auto Color=WingColor(F.Friendly);
  DrawLine(Left.X,Left.Y,Tip.X,Tip.Y,Color,1.5f);
  DrawLine(Right.X,Right.Y,Tip.X,Tip.Y,Color,1.5f);
  if (!F.Friendly) {
   const auto Back=P-D*7;
   DrawLine(Left.X,Left.Y,Back.X,Back.Y,Color,1.5f);
   DrawLine(Right.X,Right.Y,Back.X,Back.Y,Color,1.5f);
  }
  DrawText(FString::FromInt(F.Count),Color,P.X+8,P.Y-7,OVTypography::Font(EOVTextRole::Numeric));
 }
 for (const auto& B:Carrier->Bolts)
 {
  FVector2D P,Q;
  if (!Project(B.Position,P) || !Project(B.Position-B.Direction*FMath::Min(160.f,B.Speed*B.Age),Q)) continue;
  if (FVector2D::DistSquared(P,Q)<2.25) continue;
  Q=P+(Q-P).GetClampedToMaxSize(8);
  DrawLine(P.X,P.Y,Q.X,Q.Y,WingColor(B.FactionId==0),1.3f);
 }
 for (const auto& E:Carrier->RecentEvents)
 {
  const float Age=Carrier->GameTime-E.ResolvedAt;
  FVector2D P;
  if (!E.bHit || Age<0 || Age>=.22f || !Project(E.Position,P)) continue;
  FVector2D SizeRef;
  if (!PC->ProjectWorldLocationToScreen(E.Position+CameraRight*Carrier->Profile.CollisionRadius,SizeRef) ||
      FVector2D::DistSquared(P,SizeRef/UIScale)<2.25) continue;
  const float T=Age/.22f;
  const auto Color=(E.bHitShield?Cyan:Gold).CopyWithNewOpacity(1-T);
  const int32 Segments=E.bHitShield?12:3;
  for(int32 I=0;I<Segments;++I)
  {
   const double Angle=UE_TWO_PI*I/Segments + (E.BoltId%17);
   const FVector2D Dir(FMath::Cos(Angle),FMath::Sin(Angle));
   const FVector2D Start=P+Dir*(E.bHitShield?2+5*T:1+4*T);
   const double EndAngle=Angle+UE_TWO_PI/Segments;
   const FVector2D End=E.bHitShield?P+FVector2D(FMath::Cos(EndAngle),FMath::Sin(EndAngle))*(2+5*T):P+Dir*(3+6*T);
   DrawLine(Start.X,Start.Y,End.X,End.Y,Color,1.2f);
  }
 }
 for(const auto& P:Transport->Pods) if(P.State==EOVPodFlightState::Outbound||P.State==EOVPodFlightState::Returning) {
  FVector2D V;if(!Project(P.Position,V))continue;
  DrawRect(Gold,V.X-3,V.Y-3,6,6);
  DrawText((P.State==EOVPodFlightState::Returning?TEXT("귀환 "):TEXT("포드 "))+FString::FromInt(P.PodId),Gold,V.X+6,V.Y,OVTypography::Font(EOVTextRole::Body));
 }
 for(const auto& E:PD->RecentEvents) if(PD->GameTime-E.GameTime<.18) {
  FVector2D P,Q;if(!Project(E.Origin,P)||!Project(E.TargetPosition,Q))continue;
  const auto Color=E.WeaponKind==EOVPDWeaponKind::Beam?FLinearColor(.55f,1,.7f):Gold;
  DrawLine(P.X,P.Y,Q.X,Q.Y,Color,E.bHit?2:1);
  if(E.bDestroyed)DrawText(TEXT("요격 성공"),Color,Q.X,Q.Y-14,OVTypography::Font(EOVTextRole::Body));
 }
 if(PC->bDeveloperMode||bSceneMenuOpen||PC->Phase2CScenario<0||!PC->Selected||!PC->Selected->Systems||PC->Selected->Systems->FactionId!=0)return;
 const float X=212,Y=76;
 DrawRect(FLinearColor(.008f,.02f,.04f,.94f),X,Y,272,148);
 AddHitBox(FVector2D(X,Y)*UIScale,FVector2D(272,148)*UIScale,TEXT("MissionBackground"),true,9);
 const auto Defense=PD->GetShipState(PC->Selected);
 FString Priority;for(const auto Kind:Defense.Priority) {if(!Priority.IsEmpty())Priority+=TEXT(" > ");Priority+=Kind==EOVPDTargetKind::Missile?TEXT("미사일"):Kind==EOVPDTargetKind::Aircraft?TEXT("함재기"):TEXT("포드");}
 DrawText(TEXT("요격 우선순위"),Cyan,X+12,Y+10,OVTypography::Font(EOVTextRole::Body));
 Button(TEXT("PDPriority"),Priority,X+12,Y+30,248);
 const bool HasCarrier=Carrier->Carriers.ContainsByPredicate([&](const auto& S){return S.Carrier==PC->Selected;});
 if(HasCarrier) {
  DrawText(FString::Printf(TEXT("보관 %d   비행 %d   손실 %d"),Carrier->CountAircraft(PC->Selected,EOVAircraftState::Stored),Carrier->CountAircraft(PC->Selected,EOVAircraftState::Sortie)+Carrier->CountAircraft(PC->Selected,EOVAircraftState::Returning),Carrier->CountAircraft(PC->Selected,EOVAircraftState::Wrecked)),Ink,X+12,Y+72,OVTypography::Font(EOVTextRole::Numeric));
  DrawText(TEXT("출격 · 귀환은 하단 명령에서"),Ink,X+12,Y+106,OVTypography::Font(EOVTextRole::Body));
 } else if(PC->Phase2CScenario>=3) {
  DrawText(TEXT("운송 시험 · 임시 승객"),Gold,X+12,Y+72,OVTypography::Font(EOVTextRole::Body));
  Button(PC->Phase2CScenario==3?TEXT("LaunchPods"):TEXT("DispatchShuttle"),TEXT("출발"),X+12,Y+103,118);
  Button(PC->Phase2CScenario==3?TEXT("RecallPods"):TEXT("RecallShuttle"),TEXT("귀환"),X+140,Y+103,118);
 }
}
