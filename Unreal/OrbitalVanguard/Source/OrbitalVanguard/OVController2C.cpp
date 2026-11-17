#include "OVController.h"
#include "OVHUD.h"
#include "OVShip.h"
#include "OVShipSystemsComponent.h"
#include "OVCalibration.h"
#include "OVCombatSubsystem.h"
#include "OVCarrierSubsystem.h"
#include "OVPointDefenseSubsystem.h"
#include "OVTransportSubsystem.h"
#include "OVInteriorSubsystem.h"
#include "OVMissionSubsystem.h"
#include "OVDevelopmentSubsystem.h"
#include "OVLogisticsSubsystem.h"
#include "Engine/World.h"

void AOVController::ResetPhase2C()
{
 // An explicit scene reset detaches persistent heroes before clearing transient records.
 // Merely disabling these systems left old passengers/operations alive on the reused 3 actors.
 auto* Inside=GetWorld()->GetSubsystem<UOVInteriorSubsystem>();
 Inside->ResetScenario({}); Inside->Disable();
 GetWorld()->GetSubsystem<UOVMissionSubsystem>()->Disable();
 GetWorld()->GetSubsystem<UOVCarrierSubsystem>()->Reset();
 auto* Transit=GetWorld()->GetSubsystem<UOVTransportSubsystem>();
 Transit->ResetScenario({}); Transit->Disable();
 GetWorld()->GetSubsystem<UOVPointDefenseSubsystem>()->Disable();
 GetWorld()->GetSubsystem<UOVDevelopmentSubsystem>()->SpawnedShips.Reset();
 auto* Logistics=GetWorld()->GetSubsystem<UOVLogisticsSubsystem>();
 Logistics->Cargo.Reset(); Logistics->Nodes.Reset(); Logistics->RepairLinks.Reset();
 // Only explicitly created transient benchmark actors are removed; original three survive.
 while(Ships.Num()>3) { AOVShip* Extra=Ships.Last(); Ships.Pop(); if(IsValid(Extra))Extra->Destroy(); }
 TrialFTLRemaining.Reset();
 Phase2CScenario=-1;
}

FString AOVController::GetScenarioLabel() const
{
 if(Mission && Mission->bEnabled) return Mission->Mode==EOVMissionMode::Defense?TEXT("강습 방어 임무"):Mission->Mode==EOVMissionMode::FleetOnly?TEXT("함대전 비교 임무"):TEXT("강습 통합 임무");
 static const TCHAR* Names[]={TEXT("일반 호위"),TEXT("방공함 호위"),TEXT("항공모함 교전"),TEXT("개인 포드 운송"),TEXT("강습선 운송"),TEXT("혼성 교전")};
 if(Phase2CScenario>=0&&Phase2CScenario<6) return Names[Phase2CScenario];
 if(CalibrationScenario>=0) return TEXT("후보 함체 교정 전투");
 return Combat&&Combat->bEnabled?TEXT("기본 전투 시험장"):TEXT("조작 시험장");
}

void AOVController::FrameEngagement()
{
 FBox Bounds(ForceInit);
 for(AOVShip* Ship:Ships) if(IsValid(Ship)&&!(Combat&&Combat->bEnabled&&Combat->IsDeparted(Ship))) {
  const FVector Extent=Ship->Systems->Layout?Ship->Systems->Layout->LocalHalfExtent*Ship->GetActorScale3D():FVector(200);
  Bounds+=Ship->GetActorLocation()-Extent; Bounds+=Ship->GetActorLocation()+Extent;
 }
 if(!Bounds.IsValid)return;
 bFollow=false; CameraFocus=Bounds.GetCenter(); CameraYaw=-65; CameraPitch=-55;
 CameraDistance=FMath::Clamp(Bounds.GetExtent().Size()*2.15,CalibrationScenario>=0?20000.:4500.,CalibrationScenario>=0?OVCalibration::MaxZoom:16000.);
 UpdateCamera(); LastAction=TEXT("전체 교전 구도 · F로 선택 함선 추적");
}

bool AOVController::StartPhase2CRange(int32 Scenario)
{
 if(Scenario<0||Scenario>5||Ships.Num()<3||!Combat) return false;
 // Uses the established candidate settings; only an explicit scene reset creates a new fixture.
 if(!StartCalibrationRange(1)) return false;
 Combat->EnableCombat({},260112);
 // EnableCombat reinstates legacy defaults, so reuse the candidate initialization before clearing the roster.
 TArray<AOVShip*> Existing;for(AOVShip* Ship:Ships)Existing.Add(Ship);
 if(!OVCalibration::Start(*Combat,Existing,1))return false;
 Combat->Ships.Reset(); Combat->Attacks.Reset(); Combat->RecentEvents.Reset();
 auto* PD=GetWorld()->GetSubsystem<UOVPointDefenseSubsystem>();
 auto* Wing=GetWorld()->GetSubsystem<UOVCarrierSubsystem>();
 auto* Transit=GetWorld()->GetSubsystem<UOVTransportSubsystem>();
 PD->Reset(260112); Wing->Reset(260112);
 const bool CarrierScene=Scenario==2||Scenario==5;
 const bool ShuttleScene=Scenario==4||Scenario==5;
 const bool Close=Scenario>=3;
 const FVector Locations[]={FVector(Close?-10000:-24000,-3500,2000),FVector(Close?-14000:-26000,7000,2000),FVector(Close?10000:24000,0,2000)};
 const FName Keys[]={CarrierScene?FName(TEXT("carrier")):FName(TEXT("vector")),
  ShuttleScene?FName(TEXT("assault")):Scenario==1?FName(TEXT("air_defense")):FName(TEXT("vector")),
  Scenario<=1?FName(TEXT("missile_ship")):CarrierScene?FName(TEXT("carrier")):FName(TEXT("bastion"))};
 const double Scale[]={CarrierScene?22.:10.,ShuttleScene?6.:Scenario==1?13.:10.,CarrierScene?22.:Scenario<=1?18.:13.};
 for(int32 I=0;I<3;++I) if(!OVCalibration::ConfigureHull(*Combat,*Ships[I],Keys[I],I!=2,Locations[I],Scale[I]))return false;
 const TCHAR* FriendlyNames[]={TEXT("백터 · 방어 대상"),TEXT("백터 · 일반 호위"),TEXT("항공모함 · 푸른날개"),TEXT("백터 · 포드 모함"),TEXT("백터 · 운송 모함"),TEXT("항공모함 · 혼성 편대")};
 Ships[0]->ShipName=FriendlyNames[Scenario];
 Ships[1]->ShipName=ShuttleScene?TEXT("강습선 · 수송 01"):Scenario==1?TEXT("방공함 · 호위 01"):TEXT("백터 · 호위 01");
 Ships[2]->ShipName=CarrierScene?TEXT("적 항공모함"):Scenario<=1?TEXT("적 미사일함"):TEXT("적 바스티온 · 진입 시험");
 for(AOVShip* Ship:Ships) {
  int32 Count=0;for(const auto& Mount:Ship->Systems->Layout->WeaponMounts)if(Mount.Type==EOVWeaponType::Interception)++Count;
  // Air-defense range multiplier is an explicit trial role value, not a global combat buff.
  PD->RegisterShip(Ship,Count,Ship==Ships[1]&&Scenario==1?1.5f:1.f);
 }
 if(Scenario==3) for(auto& Defender:PD->Ships) if(Defender.Ship==Ships[2]) for(auto& Mount:Defender.Mounts) { Mount.Profile.ProfileId=TEXT("DEV_PodInterception160"); Mount.Profile.Damage=160; }
 Transit->ResetScenario(Existing);
 if(CarrierScene) { Wing->RegisterCarrier(Ships[0],20); Wing->RegisterCarrier(Ships[2],20); }
 if(Scenario>=3) {
  Transit->AddDevPassengers(Ships[0],4,6);
  // A labelled DEV precondition, not boarding disabling shields or completing an interior objective.
  for(const auto& Slot:Ships[2]->Systems->Layout->Slots)if(Slot.Facility==EOVFacilityType::ShieldControl)Ships[2]->Systems->DebugSetCrew(Slot.SlotId,0);
  Ships[2]->Systems->Simulate(.001f);
 }
 if(ShuttleScene)Transit->RegisterShuttle(Ships[1],Ships[0]);
 if(Scenario<=2||Scenario==5) {
  Ships[2]->SetAutomation(false,true); Combat->SetTarget(Ships[2],Ships[0]);
 }
 if(CarrierScene)Wing->Launch(Ships[2]);
 Phase2CScenario=Scenario; CalibrationScenario=1;
 bSimulationPaused=true; SimulationSpeed=1; SimulationSeconds=0;
 SelectShip(Ships[0]); Combat->SetTarget(Ships[0],Ships[2]);
 if(auto* HUD=Cast<AOVHUD>(GetHUD()))HUD->SetDetailsOpen(false);
 FrameEngagement();
 LastAction=Scenario==3?TEXT("DEV 부분 요격 장면 · 적 PD 피해160 시험값 / 기본12 · 승객4+6명"):Scenario>=3?TEXT("DEV 승객 4+6명 · 적 쉴드 정지 시험 상태 · 선내전/나포 없음"):TEXT("F1 진행 · 교전 시 함재기 자동 출격 / 명시 복귀 · F7 다음 장면");
 return true;
}

void AOVController::AdvancePhase2C(float Delta)
{
 for(auto& Item:TrialFTLRemaining) if(IsValid(Item.Key)&&Item.Key->bFTLCharging)Item.Value=FMath::Max(0.f,Item.Value-Delta);
}

bool AOVController::Phase2CAction(FName Name)
{
 if(Phase2CScenario<0||!Selected||!Selected->bFriendly)return false;
 auto* Wing=GetWorld()->GetSubsystem<UOVCarrierSubsystem>();
 auto* Transit=GetWorld()->GetSubsystem<UOVTransportSubsystem>();
 auto* PD=GetWorld()->GetSubsystem<UOVPointDefenseSubsystem>();
 if(Name==TEXT("LaunchAircraft")) { const int32 Count=Wing->Launch(Selected); LastAction=FString::Printf(TEXT("출격 %d대 · 보유 기체 이동, 신규 생성 없음"),Count); return true; }
 if(Name==TEXT("RecallAircraft")) { Wing->Recall(Selected); LastAction=TEXT("함재기 복귀 명령 · 도킹만으로 무료 수리되지 않음"); return true; }
 if(Name==TEXT("PDPriority")) {
  auto Priority=PD->GetShipState(Selected).Priority;
  if(Priority.Num()==3) { const auto First=Priority[0];Priority.RemoveAt(0);Priority.Add(First);PD->SetPriority(Selected,Priority); }
  LastAction=TEXT("요격 우선순위 변경"); return true;
 }
 if(Name==TEXT("LaunchPods")) { Transit->LaunchPods(Selected,Selected->AttackTarget);LastAction=Transit->LastMessage;return true; }
 if(Name==TEXT("RecallPods")) { Transit->RecallPods(Selected);LastAction=TEXT("비행 포드 회항 · 내부 승객은 진입점 복귀 확인 필요");return true; }
 if(Name==TEXT("DispatchShuttle")) {
  TArray<int32> Ids;for(const auto& P:Transit->Persons)if(P.FactionId==0)Ids.Add(P.PersonId);
  Transit->DispatchShuttle(Ships[1],Ships[2],Ids);LastAction=Transit->LastMessage;return true;
 }
 if(Name==TEXT("RecallShuttle")) { Transit->RecallShuttle(Ships[1]);LastAction=Transit->LastMessage;return true; }
 if(Name==TEXT("DevEntryReturn")) {
  for(const auto& P:Transit->Persons)if(P.FactionId==0)Transit->DevReturnPersonToEntry(P.PersonId);
  LastAction=TEXT("DEV: 원 진입점 도착 입력 · 선내 경로/작전 성공을 모사하지 않음");return true;
 }
 if(Name==TEXT("TrialFTL")) { Selected->bFTLCharging=true; TrialFTLRemaining.Add(Selected,10); Wing->Recall(Selected);LastAction=TEXT("시험 FTL 10초 충전 · 회수 대기 · 성계 여행 아님");return true; }
 if(Name==TEXT("AbandonAircraft")) { Wing->AbandonOutstanding(Selected);LastAction=TEXT("미귀환 기체 포기 선택 · 기체를 완파/재생성하지 않음");return true; }
 if(Name==TEXT("TrialDeparture")) {
  const float* Remaining=TrialFTLRemaining.Find(Selected);
  if(Remaining&&*Remaining<=0&&Selected->bFTLCharging&&Wing->CanDepart(Selected)) { Combat->NotifyFTLDeparture(Selected);LastAction=TEXT("FTL 시험 이탈 이벤트 · 성계 여행 미구현"); }
  else LastAction=TEXT("이탈 대기 · 충전 완료와 기체 회수/명시 포기 필요");return true;
 }
 if(Name==TEXT("DevFighterRepair")&&bDeveloperMode) { Wing->DebugSupplyRepair(Selected);LastAction=TEXT("DEV 시험 공급 수리 · 경제/자원 차감 미구현");return true; }
 if(Name==TEXT("DevLoseShuttle")&&bDeveloperMode) { Ships[1]->Systems->DebugDestroySlot(Ships[1]->Systems->Layout->CoreSlotId,EOVDamageCause::Sabotage);LastAction=TEXT("DEV 강습선 상실 입력");return true; }
 return false;
}

bool AOVController::StartPhase2CStress(int32 ExtraCarrierPairs)
{
 if(ExtraCarrierPairs<0||ExtraCarrierPairs>3||!StartPhase2CRange(5))return false;
 for(int32 Pair=0;Pair<ExtraCarrierPairs;++Pair) {
  AOVShip* PairShips[2]={};
  for(int32 Side=0;Side<2;++Side) {
   auto* Ship=GetWorld()->SpawnActor<AOVShip>(); Ships.Add(Ship); PairShips[Side]=Ship;
   if(!OVCalibration::ConfigureHull(*Combat,*Ship,TEXT("carrier"),Side==0,FVector(Side?26000:-26000,(Pair+1)*43000,4000),22))return false;
   Ship->ShipName=FString::Printf(TEXT("DEV 항모 %d-%d"),Pair,Side);PointDefense->RegisterShip(Ship,5);
   Carrier->RegisterCarrier(Ship,20);
  }
  for(int32 Side=0;Side<2;++Side) { Combat->SetTarget(PairShips[Side],PairShips[1-Side]);PairShips[Side]->SetAutomation(false,true);Carrier->Launch(PairShips[Side]); }
 }
 FrameEngagement(); LastAction=TEXT("DEV 부하 측정 편제 · 확정 밸런스 비교 아님");return true;
}
