#include "OVLogisticsSubsystem.h"
#include "OVShip.h"
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
#endif
