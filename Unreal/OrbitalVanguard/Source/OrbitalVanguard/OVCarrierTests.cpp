#include "OVCarrierSubsystem.h"
#include "OVCombatSubsystem.h"
#include "OVShip.h"
#include "OVShipSystemsComponent.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace OVCarrierTest
{
struct FRange
{
    UWorld* World;
    UOVCombatSubsystem* Combat;
    UOVCarrierSubsystem* Fleet;
    AOVShip* Carrier;
    AOVShip* Enemy;
    FRange(int32 Inventory = 4)
    {
        World = UWorld::CreateWorld(EWorldType::Game, false);
        Combat = World->GetSubsystem<UOVCombatSubsystem>();
        Fleet = World->GetSubsystem<UOVCarrierSubsystem>();
        Carrier = Make(TEXT("Carrier"), 0, FVector::ZeroVector);
        Enemy = Make(TEXT("Enemy"), 1, FVector(6000, 0, 0));
        Enemy->SetActorScale3D(FVector(10));
        Combat->EnableCombat({Carrier, Enemy}, 3104);
        Combat->Settings->GuaranteedRange = 100000;
        Fleet->Reset(3104);
        Fleet->RegisterCarrier(Carrier, Inventory);
    }
    ~FRange() { World->DestroyWorld(false); }
    AOVShip* Make(const TCHAR* Name, int32 Faction, FVector Position)
    {
        auto* Ship = World->SpawnActor<AOVShip>();
        Ship->ShipName = Name; Ship->Systems->FactionId = Faction; Ship->bFriendly = Faction == 0;
        Ship->Systems->Initialize(UShipLayoutDataAsset::MakeTestLayout(Ship, 3));
        Ship->SetActorLocation(Position); Ship->DebugResetMotion(); Ship->SetAutomation(false, false);
        return Ship;
    }
    void Advance(float Seconds, float Chunk = 1.f / 60)
    {
        const int32 Count = FMath::RoundToInt(Seconds / Chunk);
        for (int32 I = 0; I < Count; ++I) Fleet->Simulate(Chunk);
    }
    void Facility(EOVFacilityType Type, float Crew, int32 Occupation = -1)
    {
        for (int32 I = 0; I < Carrier->Systems->Slots.Num(); ++I)
            if (Carrier->Systems->Layout->Slots[I].Facility == Type)
            {
                Carrier->Systems->DebugSetCrew(Carrier->Systems->Slots[I].SlotId, Crew);
                Carrier->Systems->DebugSetOccupation(Carrier->Systems->Slots[I].SlotId, Occupation);
            }
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVCarrierInventoryTest, "OrbitalVanguard.Carrier.InventoryCapacityAndRecall", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOVCarrierInventoryTest::RunTest(const FString&)
{
    OVCarrierTest::FRange R(20);
    const int32 Count = R.Fleet->Aircraft.Num();
    TestTrue(TEXT("Fixture actually has a hangar"), Count > 0);
    TestEqual(TEXT("Each hangar starts with exactly twenty stable records"), Count, R.Fleet->Hangars.Num() * 20);
    const int32 Id = R.Fleet->Aircraft[0].AircraftId;
    TestFalse(TEXT("Repeated registration never gives free aircraft"), R.Fleet->RegisterCarrier(R.Carrier, 20));
    R.Fleet->SetCapacityBonus(R.Carrier, 6);
    TestEqual(TEXT("Research expands capacity only"), R.Fleet->GetHangarCapacity(R.Fleet->Hangars[0].HangarId), 26);
    TestEqual(TEXT("Expanded capacity does not manufacture inventory"), R.Fleet->Aircraft.Num(), Count);
    TestFalse(TEXT("Cannot add small ship capacity to occupied fighter hangar"), R.Fleet->SetHangarMode(R.Fleet->Hangars[0].HangarId, EOVHangarMode::SmallShip));
    TestEqual(TEXT("Launch moves all stored craft"), R.Fleet->Launch(R.Carrier), Count);
    R.Fleet->ApplyAircraftDamage(Id, 100);
    const float Damaged = R.Fleet->Aircraft[0].HP;
    R.Advance(.5f); R.Fleet->Recall(R.Carrier); R.Advance(5);
    TestEqual(TEXT("All survivors return as the same records"), R.Fleet->CountAircraft(R.Carrier, EOVAircraftState::Stored), Count);
    TestEqual(TEXT("Recall does not repair hull for free"), R.Fleet->Aircraft[0].HP, Damaged);
    TestEqual(TEXT("Recall preserves ID"), R.Fleet->Aircraft[0].AircraftId, Id);
    TestEqual(TEXT("Relaunch inventory is conserved"), R.Fleet->Launch(R.Carrier), Count);
    TestTrue(TEXT("Single lethal aircraft event"), R.Fleet->ApplyAircraftDamage(Id, 10000));
    TestFalse(TEXT("Wreck cannot take another damage event"), R.Fleet->ApplyAircraftDamage(Id, 10000));
    TestEqual(TEXT("Loss counted once"), R.Fleet->LossCount, 1);
    R.Fleet->Recall(R.Carrier); R.Advance(5); R.Fleet->DebugSupplyRepair(R.Carrier);
    TestEqual(TEXT("Repair supply does not rebuild wreck"), R.Fleet->Aircraft[0].State, EOVAircraftState::Wrecked);
    TestEqual(TEXT("Post-loss launch does not replenish"), R.Fleet->Launch(R.Carrier), Count - 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVCarrierFacilitiesTest, "OrbitalVanguard.Carrier.FacilityControlAndDockQueue", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOVCarrierFacilitiesTest::RunTest(const FString&)
{
    OVCarrierTest::FRange R;
    R.Facility(EOVFacilityType::FighterControl, 0);
    TestEqual(TEXT("No crew blocks launch"), R.Fleet->Launch(R.Carrier), 0);
    R.Facility(EOVFacilityType::FighterControl, 10, 1);
    TestEqual(TEXT("Enemy occupation blocks launch independently"), R.Fleet->Launch(R.Carrier), 0);
    R.Facility(EOVFacilityType::FighterControl, 10);
    R.Fleet->Launch(R.Carrier); R.Advance(.5);
    R.Facility(EOVFacilityType::FighterControl, 0);
    const FVector Position = R.Fleet->Aircraft[0].Position;
    R.Advance(1);
    TestEqual(TEXT("External craft transition to control lost"), R.Fleet->Aircraft[0].State, EOVAircraftState::ControlLost);
    TestTrue(TEXT("Control loss freezes flight"), R.Fleet->Aircraft[0].Position.Equals(Position, .001));
    R.Facility(EOVFacilityType::FighterControl, 10);
    R.Facility(EOVFacilityType::Hangar, 0); R.Fleet->Recall(R.Carrier); R.Advance(5);
    TestEqual(TEXT("Disabled hangar holds return in queue"), R.Fleet->Aircraft[0].State, EOVAircraftState::DockWaiting);
    R.Facility(EOVFacilityType::Hangar, 10); R.Advance(5);
    TestEqual(TEXT("Restored hangar safely docks survivor"), R.Fleet->Aircraft[0].State, EOVAircraftState::Stored);
    const int32 Count = R.Fleet->Aircraft.Num();
    R.Carrier->Systems->DebugDestroySlot(R.Fleet->Hangars[0].SlotId, EOVDamageCause::Sabotage);
    R.Advance(.1);
    TestTrue(TEXT("Destroyed hangar loses stored craft"), R.Fleet->LossCount > 0);
    TestEqual(TEXT("Lost inventory records persist for accounting"), R.Fleet->Aircraft.Num(), Count);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVCarrierOwnershipTest, "OrbitalVanguard.Carrier.OwnershipFTLAndLaunchAttribution", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOVCarrierOwnershipTest::RunTest(const FString&)
{
    OVCarrierTest::FRange R;
    R.Fleet->Launch(R.Carrier); R.Advance(.025f, 1.f / 120);
    TestTrue(TEXT("Aircraft actually issue a projectile"), R.Fleet->ShotCount > 0);
    TestTrue(TEXT("Projectile in flight before capture"), !R.Fleet->Bolts.IsEmpty());
    const int32 Faction = R.Fleet->Bolts.IsEmpty() ? -1 : R.Fleet->Bolts[0].FactionId;
    R.Carrier->Systems->FactionId = 1;
    R.Advance(.025f, 1.f / 120);
    TestEqual(TEXT("Exterior aircraft follow captured carrier"), R.Fleet->Aircraft[0].FactionId, 1);
    if (!R.Fleet->Bolts.IsEmpty()) TestEqual(TEXT("Old projectile keeps original attacker faction"), R.Fleet->Bolts[0].FactionId, Faction);
    R.Carrier->DebugSetFTLCharging(true); R.Advance(.025f, 1.f / 120);
    TestTrue(TEXT("Charging orders emergency recall"), R.Fleet->Carriers[0].bRecallOrdered);
    TestFalse(TEXT("FTL waits for airborne inventory"), R.Fleet->CanDepart(R.Carrier));
    const int32 Count = R.Fleet->Aircraft.Num();
    R.Fleet->AbandonOutstanding(R.Carrier);
    TestTrue(TEXT("Explicit abandon releases recovery gate only"), R.Fleet->CanDepart(R.Carrier));
    TestEqual(TEXT("Abandon neither kills nor creates"), R.Fleet->Aircraft.Num(), Count);
    R.Combat->NotifyFTLDeparture(R.Carrier); R.Advance(.025f, 1.f / 120);
    TestEqual(TEXT("Departure strands external craft"), R.Fleet->Aircraft[0].State, EOVAircraftState::ControlLost);
    TestEqual(TEXT("Stranding is not destruction"), R.Fleet->LossCount, 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVCarrierCombatTest, "OrbitalVanguard.Carrier.RealProjectilesAndFixedTime", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOVCarrierCombatTest::RunTest(const FString&)
{
    int32 BaselineShots = -1, BaselineHits = -1;
    float BaselineShield = 0;
    for (int32 Rate : {1, 2, 3})
    {
        OVCarrierTest::FRange R;
        const float InitialShield = R.Enemy->Systems->Shield;
        R.Fleet->Launch(R.Carrier);
        const FVector Position = R.Fleet->Aircraft[0].Position;
        R.Fleet->Simulate(0);
        TestEqual(TEXT("Paused game issues no aircraft shots"), R.Fleet->ShotCount, 0);
        TestTrue(TEXT("Paused craft do not move"), R.Fleet->Aircraft[0].Position.Equals(Position));
        R.Advance(6, .25f * Rate);
        TestTrue(TEXT("Physical bolts swept into enemy hull"), R.Fleet->HitCount > 0);
        TestTrue(TEXT("Ship shield changed through existing damage route"), R.Enemy->Systems->Shield < InitialShield);
        if (BaselineShots < 0) { BaselineShots = R.Fleet->ShotCount; BaselineHits = R.Fleet->HitCount; BaselineShield = R.Enemy->Systems->Shield; }
        else
        {
            TestEqual(TEXT("Same game time, same shots at 1/2/3"), R.Fleet->ShotCount, BaselineShots);
            TestEqual(TEXT("Same game time, same swept hits at low FPS"), R.Fleet->HitCount, BaselineHits);
            TestEqual(TEXT("Same game time, same damage"), R.Enemy->Systems->Shield, BaselineShield);
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVCarrierDogfightTest, "OrbitalVanguard.Carrier.DogfightAndMotherLoss", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOVCarrierDogfightTest::RunTest(const FString&)
{
    OVCarrierTest::FRange R(2);
    R.Enemy->SetActorScale3D(FVector(1));
    R.Fleet->RegisterCarrier(R.Enemy, 2);
    R.Fleet->Launch(R.Carrier); R.Fleet->Launch(R.Enemy);
    // Separated from ship boxes so the shot is a real aircraft-aircraft collision.
    for (auto& C : R.Fleet->Aircraft) C.Position = FVector(C.FactionId == 0 ? 0 : 600, (C.AircraftId % 2) * 400, 10000);
    const float Before = R.Fleet->Aircraft.Last().Shield;
    R.Advance(2);
    TestTrue(TEXT("Enemy aircraft are actually hit, not ship targets only"),
        R.Fleet->Aircraft.ContainsByPredicate([Before](const FOVAircraftState& C) { return C.FactionId == 1 && C.Shield < Before; }));
    R.Carrier->Systems->bWrecked = true;
    R.Advance(.1);
    for (const auto& C : R.Fleet->Aircraft)
        if (C.Carrier == R.Carrier && C.State != EOVAircraftState::Wrecked)
            TestEqual(TEXT("Mother loss leaves surviving exterior aircraft control-lost"), C.State, EOVAircraftState::ControlLost);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVCarrierProtectionTest, "OrbitalVanguard.Carrier.InteriorProtectionAndInFlightLifetime", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOVCarrierProtectionTest::RunTest(const FString&)
{
    OVCarrierTest::FRange R(2);
    R.Enemy->Systems->SetInteriorPresence(0, true, false, true);
    R.Combat->SetTarget(R.Carrier, R.Enemy);
    const auto ShipState = R.Combat->GetShipState(R.Carrier);
    R.Combat->SetAttackIntent(R.Carrier, EOVAttackIntent::DisableWeapons, ShipState.Mounts[0].MountId);
    R.Fleet->Launch(R.Carrier); R.Advance(1);
    TestEqual(TEXT("Forced ship mount exception cannot leak to aircraft"), R.Fleet->ShotCount, 0);
    R.Enemy->Systems->SetInteriorPresence(0, false, false, true);
    for (auto& C : R.Fleet->Aircraft) { C.Position = FVector(0, C.AircraftId * 400, 1000); C.Leg = EOVAircraftLeg::Ingress; }
    R.Fleet->Profile.BoltSpeed = 1000;
    R.Fleet->Profile.BoltLifetime = 15;
    R.Advance(.025f, 1.f / 120);
    TestTrue(TEXT("Exit releases fighter strike"), R.Fleet->ShotCount > 0);
    const int32 Shots = R.Fleet->ShotCount;
    R.Enemy->Systems->SetInteriorPresence(0, true, false, true);
    R.Carrier->Systems->bWrecked = true;
    R.Advance(10);
    TestEqual(TEXT("Protected target and wrecked source never issue new fighter shots"), R.Fleet->ShotCount, Shots);
    TestTrue(TEXT("Existing fighter bolts survive mother loss and later protection"), R.Fleet->HitCount > 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVCarrierAlternateDockTest, "OrbitalVanguard.Carrier.AlternateHangarAndExclusiveModes", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOVCarrierAlternateDockTest::RunTest(const FString&)
{
    OVCarrierTest::FRange R(0);
    auto* Layout = R.Carrier->Systems->Layout.Get();
    for (auto& Slot : Layout->Slots)
        if (Slot.Facility == EOVFacilityType::None && Slot.AllowedFacilities.Contains(EOVFacilityType::Hangar))
        { Slot.Facility = EOVFacilityType::Hangar; Slot.MaxFacilityHP = 100; Slot.RequiredCrew = 10; break; }
    R.Carrier->Systems->Initialize(Layout);
    R.Fleet->Reset(3104); R.Fleet->RegisterCarrier(R.Carrier, 0);
    TestEqual(TEXT("Fixture has two separately owned hangar rooms"), R.Fleet->Hangars.Num(), 2);
    const int32 H0 = R.Fleet->Hangars[0].HangarId;
    TestTrue(TEXT("Empty hangar can choose small-ship mode"), R.Fleet->SetHangarMode(H0, EOVHangarMode::SmallShip));
    R.Fleet->SetCapacityBonus(R.Carrier, 6);
    TestEqual(TEXT("Research never expands small-ship capacity"), R.Fleet->GetHangarCapacity(H0), 1);
    TestEqual(TEXT("Small-ship mode never generates fighters"), R.Fleet->Aircraft.Num(), 0);
    R.Fleet->Reset(3104); R.Fleet->RegisterCarrier(R.Carrier, 2);
    const int32 Count = R.Fleet->Aircraft.Num();
    R.Fleet->Launch(R.Carrier);
    R.Carrier->Systems->DebugDestroySlot(R.Fleet->Hangars[0].SlotId, EOVDamageCause::Sabotage);
    R.Advance(.1);
    TestEqual(TEXT("External inventory is not lost when its original hangar is destroyed"), R.Fleet->LossCount, 0);
    R.Fleet->Recall(R.Carrier); R.Advance(5);
    TestEqual(TEXT("Other operational hangar recovers actual surviving records"), R.Fleet->CountAircraft(R.Carrier, EOVAircraftState::Stored), Count);
    TestTrue(TEXT("Every returned craft records its new storage hangar"),
        !R.Fleet->Aircraft.ContainsByPredicate([H0](const FOVAircraftState& C) { return C.HangarId == H0; }));
    return true;
}
#endif
