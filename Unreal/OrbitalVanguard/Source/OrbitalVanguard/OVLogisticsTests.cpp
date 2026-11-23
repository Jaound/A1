#include "OVLogisticsSubsystem.h"
#include "OVShip.h"
#include "OVController.h"
#include "OVCombatSubsystem.h"
#include "OVShipSystemsComponent.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVDevLogisticsTest, "OrbitalVanguard.Development.FiniteLogistics", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOVDevLogisticsTest::RunTest(const FString&)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    auto* Logistics = World->GetSubsystem<UOVLogisticsSubsystem>();
    auto Make = [&]() { auto* S = World->SpawnActor<AOVShip>(); S->Systems->Initialize(UShipLayoutDataAsset::MakeTestLayout(S, 3)); return S; };
    AOVShip* Miner = Make(); AOVShip* Transport = Make(); AOVShip* Repair = Make(); AOVShip* Target = Make();
    Logistics->RegisterDevShip(Miner, EOVDevLogisticsRole::Mining, 10, 0);
    Logistics->RegisterDevShip(Transport, EOVDevLogisticsRole::Transport, 20, 10);
    Logistics->RegisterDevShip(Repair, EOVDevLogisticsRole::Repair, 20, 0);
    const int32 Node = Logistics->AddDevNode(FVector::ZeroVector, 6);
    TestEqual(TEXT("Mining limited by node"), Logistics->DevMine(Miner, Node, 20, 100), 6.f);
    TestEqual(TEXT("Depleted node yields nothing"), Logistics->DevMine(Miner, Node, 1, 100), 0.f);
    TestEqual(TEXT("Transport can collect mined cargo"), Logistics->DevTransfer(Miner, Transport, 6, 100), 6.f);
    TestEqual(TEXT("Transfer conserves cargo"), Logistics->DevTransfer(Transport, Repair, 8, 100), 8.f);
    Target->Systems->HullHP -= 100;
    TestEqual(TEXT("Repair limited by material"), Logistics->DevRepairHull(Repair, Target, 100, 2, 100), 4.f);
    TestEqual(TEXT("Empty repair cargo cannot repair"), Logistics->DevRepairHull(Repair, Target, 1, 2, 100), 0.f);
    Target->Systems->FactionId = 1;
    TestEqual(TEXT("Enemy transfer prohibited"), Logistics->DevTransfer(Transport, Target, 1, 100), 0.f);
    TestEqual(TEXT("납품지 밖 화물 보존"), Logistics->UnloadTransport(Transport, 0, FVector(1000,0,0), 100, 20), 0.f);
    TestEqual(TEXT("다른 세력 납품 거부"), Logistics->UnloadTransport(Transport, 1, FVector::ZeroVector, 100, 20), 0.f);
    TestEqual(TEXT("남은 화물만 하역"), Logistics->UnloadTransport(Transport, 0, FVector::ZeroVector, 100, 20), 8.f);
    TestEqual(TEXT("빈 화물 재납품 차단"), Logistics->UnloadTransport(Transport, 0, FVector::ZeroVector, 100, 20), 0.f);
    Logistics->RemoveDevShip(Miner);
    TestEqual(TEXT("Removed ship cannot mine"), Logistics->DevMine(Miner, Node, 1, 100), 0.f);
    World->DestroyWorld(false); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVDevTimedRepairTest, "OrbitalVanguard.Development.TimedRepair", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOVDevTimedRepairTest::RunTest(const FString&)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    auto* Logistics = World->GetSubsystem<UOVLogisticsSubsystem>();
    auto Make = [&]() { auto* Ship = World->SpawnActor<AOVShip>(); Ship->Systems->Initialize(UShipLayoutDataAsset::MakeTestLayout(Ship, 3)); return Ship; };
    AOVShip* Repair = Make(); AOVShip* Target = Make(); AOVShip* Other = Make();
    Logistics->RegisterDevShip(Repair, EOVDevLogisticsRole::Repair, 5000, 0);
    TestTrue(TEXT("Explicit finite three-material cargo"), Logistics->SetDevMaterials(Repair, FVector(1000, 400, 100)));
    Target->Systems->HullHP -= 200;
    const float Before = Target->Systems->HullHP;
    const FVector Quote(1, .5, .25);
    TestTrue(TEXT("Field link starts"), Logistics->StartDevRepair(Repair, Target, false, FGuid(), Quote, 12000, 0, 0, 0));
    TestFalse(TEXT("Same target cannot duplicate link"), Logistics->StartDevRepair(Repair, Target, false, FGuid(), Quote, 12000, 0, 0, 0));
    TestEqual(TEXT("Instant DEV action cannot bypass link"), Logistics->DevRepairHull(Repair, Target, 1, 1, 100), 0.f);
    Other->Systems->HullHP -= 1;
    TestFalse(TEXT("One repair ship cannot service two targets"), Logistics->StartDevRepair(Repair, Other, false, FGuid(), Quote, 12000, 0, 0, 0));
    Logistics->Simulate(3);
    TestEqual(TEXT("Preparation consumes no materials or repair"), Target->Systems->HullHP, Before);
    Logistics->Simulate(1);
    TestEqual(TEXT("Field rate uses half of 64"), Target->Systems->HullHP, Before + 32);
    TestEqual(TEXT("General material costs 1.5 times base quote"), Logistics->Cargo[0].Amount, 952.f);
    TestEqual(TEXT("Advanced material charged proportionally"), Logistics->Cargo[0].Advanced, 376.f);
    TestEqual(TEXT("Rare material charged proportionally"), Logistics->Cargo[0].Rare, 88.f);
    const float Mid = Target->Systems->HullHP;
    Repair->Systems->FactionId = 1;
    Logistics->Simulate(1);
    TestEqual(TEXT("Faction change immediately stops work"), Target->Systems->HullHP, Mid);
    TestEqual(TEXT("Invalid link is removed"), Logistics->RepairLinks.Num(), 0);
    World->DestroyWorld(false); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVDamageControlTest,"OrbitalVanguard.Development.FacilityDamageControl",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOVDamageControlTest::RunTest(const FString&)
{
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
    auto* Service=World->GetSubsystem<UOVLogisticsSubsystem>();
    auto Make=[&](){
        auto* Ship=World->SpawnActor<AOVShip>();
        Ship->HullDefinitionId=TEXT("repair");
        Ship->Systems->Initialize(UShipLayoutDataAsset::MakeTestLayout(Ship,3));
        Service->RegisterDevShip(Ship,EOVDevLogisticsRole::Repair,10000,10000);
        return Ship;
    };
    auto* Tender=Make();auto* Target=Make();auto& S=*Target->Systems;
    const int32 Index=S.Layout->Slots.IndexOfByPredicate([](const auto& D){return D.Facility==EOVFacilityType::Engine;});
    if(Index==INDEX_NONE){AddError(TEXT("엔진 슬롯 누락"));World->DestroyWorld(false);return false;}
    auto& Room=S.Slots[Index];const auto& Def=S.Layout->Slots[Index];
    S.DebugDestroySlot(Def.SlotId,EOVDamageCause::Sabotage);
    S.HullHP=S.MaxHullHP;
    auto Crew=[&](){float Total=0;for(const auto& R:S.Slots)Total+=R.Crew+R.RepairCrew;return Total;};
    const float InitialCrew=Crew(),Stock=Service->FindCargo(Target)->Amount;
    TestTrue(TEXT("시설 손실 경보 생성"),Room.bUnreadLoss);
    TestTrue(TEXT("자가 수리 명령"),Service->ToggleInternalRepair(Target));
    Service->Simulate(0);TestEqual(TEXT("정지 중 공사 없음"),Room.RebuildHP,0.f);
    Service->Simulate(.25f);
    const float InternalWork=Room.RebuildHP,InternalCost=Stock-Service->FindCargo(Target)->Amount;
    TestTrue(TEXT("유상 복구하되 미완료 시설은 작동하지 않음"),InternalWork>0 && Room.HP==0 && InternalCost>0);
    TestEqual(TEXT("정원의 절반 배치"),Room.RepairCrew,.5f*Def.RequiredCrew);
    TestEqual(TEXT("수리반과 근무 인원 보존"),Crew(),InitialCrew);
    Service->ToggleInternalRepair(Target);Service->Simulate(1);
    TestEqual(TEXT("해제하면 즉시 작업 중지"),Room.RebuildHP,InternalWork);
    TestEqual(TEXT("차출 인원 복귀"),Room.RepairCrew,0.f);TestEqual(TEXT("근무 인원 복구"),Crew(),InitialCrew);
    TestTrue(TEXT("선체가 온전한 함선도 시설 수리 접수"),Service->OrderRepair(Tender,Target));
    Service->Simulate(3);Service->Simulate(.25f);
    const float ExternalWork=Room.RebuildHP-InternalWork,ExternalCost=10000-Service->FindCargo(Tender)->Amount;
    TestTrue(TEXT("수리함 대비 30% 속도"),FMath::IsNearlyEqual(InternalWork,ExternalWork*.3f,.01f));
    TestTrue(TEXT("같은 HP당 재료 효율 30%"),FMath::IsNearlyEqual(ExternalCost/ExternalWork,InternalCost/InternalWork*.3f,.002f));
    TestTrue(TEXT("작업 상태 표시"),Service->RepairStatus(Tender).Contains(TEXT("시설 수리 중")));
    Service->Simulate(10000);
    TestEqual(TEXT("시설 최대 HP까지 재건"),Room.HP,Def.MaxFacilityHP);
    TestTrue(TEXT("엔진 가동 및 미확인 알림 유지"),S.CanMove() && Room.bUnreadLoss);
    TestTrue(TEXT("완료 상태 표시"),Service->RepairStatus(Tender).Contains(TEXT("수리 완료")));
    S.DebugDestroySlot(Def.SlotId,EOVDamageCause::Sabotage);
    Service->SetDevMaterials(Target,FVector::ZeroVector);Service->ToggleInternalRepair(Target);Service->Simulate(20);
    TestEqual(TEXT("자원 고갈 시 복구 없음"),Room.RebuildHP,0.f);
    TestEqual(TEXT("불필요한 인원 차출 없음"),Room.RepairCrew,0.f);
    World->DestroyWorld(false);return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVInteriorSurveyTest,"OrbitalVanguard.Development.InteriorSurvey",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOVInteriorSurveyTest::RunTest(const FString&)
{
    auto* World=UWorld::CreateWorld(EWorldType::Game,false);
    auto* PC=World->SpawnActor<AOVController>();PC->Combat=World->GetSubsystem<UOVCombatSubsystem>();
    auto Make=[&](){auto* Ship=World->SpawnActor<AOVShip>();Ship->Systems->Initialize(UShipLayoutDataAsset::MakeTestLayout(Ship,3));PC->Ships.Add(Ship);return Ship;};
    auto* Radar=Make();auto* Enemy=Make();Radar->HullDefinitionId=TEXT("radar");
    Enemy->bFriendly=false;Enemy->Systems->FactionId=1;
    PC->Combat->EnableCombat({Radar,Enemy});PC->SelectShip(Radar);
    TestTrue(TEXT("자체 레이더 정찰 가능"),PC->SurveyEnemy(Enemy));
    const int32 I=Enemy->Systems->Layout->Slots.IndexOfByPredicate([](const auto& D){return D.Facility==EOVFacilityType::Engine;});
    if(I==INDEX_NONE || !Enemy->Survey.IsValidIndex(I)){World->DestroyWorld(false);return false;}
    Enemy->Systems->DebugDestroySlot(Enemy->Systems->Layout->Slots[I].SlotId,EOVDamageCause::Sabotage);
    TestEqual(TEXT("관측 뒤 파괴된 시설의 과거 정보 보존"),Enemy->Survey[I].Integrity,1.f);
    PC->SelectShip(Radar);PC->Combat->GameTime=Radar->ScanReadyAt-.01;
    TestFalse(TEXT("충전 직전 정찰 불가"),PC->SurveyEnemy(Enemy));
    PC->Combat->GameTime=Radar->ScanReadyAt;
    PC->Combat->EnvironmentRadarDisruptRemaining=2;
    TestFalse(TEXT("충전이 끝나도 재밍 중 탐색 불가"),PC->SurveyEnemy(Enemy));
    PC->Combat->EnvironmentRadarDisruptRemaining=0;
    TestTrue(TEXT("충전 뒤 재정찰"),PC->SurveyEnemy(Enemy));
    TestEqual(TEXT("현재 파괴 상태로 관측 갱신"),Enemy->Survey[I].Integrity,0.f);
    PC->Combat->DisableCombat();
    TestTrue(TEXT("다음 교전으로 충전 및 적 정찰 정보가 넘어가지 않음"),Radar->ScanReadyAt==0 && Enemy->Survey.IsEmpty() && Enemy->LastSurveyAt<0);
    World->DestroyWorld(false);return true;
}
#endif
