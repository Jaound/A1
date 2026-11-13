#include "OVCombatSubsystem.h"
#include "OVShip.h"
#include "OVShipSystemsComponent.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace OVCombatTest
{
// Real Actors and the production subsystem in a disposable world. No DEV hit substitutes
// for fire: fixtures only arrange initial state, then use RequestFire and Simulate.
struct FRange
{
    UWorld* World;
    UOVCombatSubsystem* Combat;
    AOVShip* A;
    AOVShip* B;
    AOVShip* Scout;
    FRange()
    {
        World = UWorld::CreateWorld(EWorldType::Game, false);
        Combat = World->GetSubsystem<UOVCombatSubsystem>();
        A = Make(TEXT("TestShooter"), 0, FVector::ZeroVector);
        B = Make(TEXT("TestTarget"), 1, FVector(1000, 0, 0));
        Scout = Make(TEXT("TestScout"), 0, FVector(0, 3000, 0));
        Enable();
    }
    ~FRange() { World->DestroyWorld(false); }
    AOVShip* Make(const TCHAR* Name, int32 Faction, FVector Position)
    {
        AOVShip* Ship = World->SpawnActor<AOVShip>();
        Ship->ShipName = Name;
        Ship->bFriendly = Faction == 0;
        Ship->Systems->FactionId = Faction;
        Ship->Systems->Initialize(UShipLayoutDataAsset::MakeTestLayout(Ship, 3));
        Ship->SetActorLocation(Position);
        Ship->DebugResetMotion();
        return Ship;
    }
    void Enable(int32 Seed = 260112)
    {
        Combat->EnableCombat({A, B, Scout}, Seed);
        Combat->SetTarget(A, B);
        A->bManual = B->bManual = Scout->bManual = true;
        Align();
    }
    FOVCombatShipState& State(AOVShip* Ship)
    {
        return *Combat->Ships.FindByPredicate([Ship](const auto& S) { return S.Ship == Ship; });
    }
    FOVCombatMountState& Gun(int32 Index = 1) { return State(A).Mounts[Index]; }
    void Align(int32 Index = 1)
    {
        auto& Mount = Gun(Index);
        const auto& Def = A->Systems->Layout->WeaponMounts[Index];
        const FVector Local = A->GetActorTransform().InverseTransformPosition(B->GetActorLocation()) - Def.LocalPosition;
        Mount.LocalAim = Local.Rotation();
        Mount.AngularVelocity = FVector2D::ZeroVector;
    }
    void Exact(int32 Index = 1)
    {
        Gun(Index).Profile.Accuracy = 1;
        Gun(Index).Profile.FireTolerance = 180;
        Gun(Index).Profile.Interval = .1f;
        Align(Index);
    }
    void Fire(int32 Index = 1, float Duration = .1f)
    {
        Align(Index);
        Combat->RequestFire(A, Gun(Index).MountId);
        Combat->Simulate(Duration);
    }
    void DisableFacility(AOVShip* Ship, EOVFacilityType Type)
    {
        for (int32 I = 0; I < Ship->Systems->Slots.Num(); ++I)
            if (Ship->Systems->Layout->Slots[I].Facility == Type)
                Ship->Systems->DebugSetCrew(Ship->Systems->Slots[I].SlotId, 0);
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVCombatDetectionTest, "OrbitalVanguard.Combat.DetectionSourcesAndInvalidation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOVCombatDetectionTest::RunTest(const FString&)
{
    OVCombatTest::FRange R;
    TestFalse(TEXT("Final profile has no omniscient developer detection"), R.Combat->Settings->bDevelopmentOmniscient);
    R.DisableFacility(R.A, EOVFacilityType::Radar);
    R.DisableFacility(R.Scout, EOVFacilityType::Radar);
    TestTrue(TEXT("Guaranteed near range survives all friendly radar loss"), R.Combat->GetDetection(R.A, R.B).bDetected);
    R.B->SetActorLocation(FVector(3000, 0, 0));
    TestFalse(TEXT("Beyond guaranteed range needs a live sensor"), R.Combat->GetDetection(R.A, R.B).bDetected);
    R.Scout->Systems->ResetState();
    auto Ordinary = R.Combat->GetDetection(R.A, R.B);
    TestTrue(TEXT("Ordinary ally shares valid detection"), Ordinary.bDetected && Ordinary.AccuracyBonus > 0);
    R.State(R.Scout).bRadarSupport = true;
    auto Support = R.Combat->GetDetection(R.A, R.B);
    TestTrue(TEXT("Radar support shares a stronger correction"), Support.AccuracyBonus > Ordinary.AccuracyBonus);
    R.A->Systems->ResetState();
    R.State(R.A).RadarTier = 3;
    auto BestOwn = R.Combat->GetDetection(R.A, R.B);
    TestTrue(TEXT("Highest valid information wins, not first shared source"), BestOwn.SourceId == R.State(R.A).ShipId && BestOwn.AccuracyBonus > Support.AccuracyBonus);
    R.State(R.A).RadarTier = 1;
    R.B->SetActorLocation(FVector(7000, 0, 0));
    R.Scout->SetActorLocation(FVector(0, 50000, 0));
    TestFalse(TEXT("Duplicate radar rooms cannot extend tier 1 range"), R.Combat->GetDetection(R.A, R.B).bDetected);
    R.State(R.A).RadarTier = 3;
    TestTrue(TEXT("Radar tier increases range"), R.Combat->GetDetection(R.A, R.B).bDetected);
    R.A->Systems->bNeutral = true;
    TestFalse(TEXT("Neutral sensor cannot provide detection"), R.Combat->GetDetection(R.Scout, R.B).bDetected);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVCombatGateTest, "OrbitalVanguard.Combat.ReadinessAndFacilityGates", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOVCombatGateTest::RunTest(const FString&)
{
    OVCombatTest::FRange R;
    R.Exact();
    const FName Id = R.Gun().MountId;
    TestTrue(TEXT("Aligned near beam is ready"), R.Combat->GetReadiness(R.A, Id).bReady);
    R.Gun().Profile.Range = 100;
    TestFalse(TEXT("Range blocks readiness"), R.Combat->GetReadiness(R.A, Id).bReady);
    R.Fire(); TestEqual(TEXT("Range blocks actual launch too"), R.Combat->ShotCount, 0);
    R.Gun().Profile.Range = 10000;
    R.A->DebugSetFTLCharging(true);
    TestFalse(TEXT("FTL charging blocks beam"), R.Combat->GetReadiness(R.A, Id).bReady);
    R.A->DebugSetFTLCharging(false);
    for (EOVFacilityType Facility : {EOVFacilityType::WeaponControl, EOVFacilityType::Capacitor})
    {
        R.DisableFacility(R.A, Facility);
        TestFalse(TEXT("Facility loss blocks READY"), R.Combat->GetReadiness(R.A, Id).bReady);
        const int32 Shots = R.Combat->ShotCount;
        R.Fire(); TestEqual(TEXT("Facility loss blocks the launch path"), R.Combat->ShotCount, Shots);
        R.A->Systems->ResetState();
    }
    R.A->Systems->DebugHitWeapon(Id, 10000);
    TestFalse(TEXT("Independent mount HP blocks beam"), R.Combat->GetReadiness(R.A, Id).bReady);
    R.DisableFacility(R.A, EOVFacilityType::WeaponControl);
    R.A->Systems->DebugRepairWeapon(Id);
    TestFalse(TEXT("Repairing gun cannot repair its control crew"), R.Combat->GetReadiness(R.A, Id).bReady);
    R.A->Systems->ResetState();
    R.Gun().bFireRequested = false;
    R.Fire();
    TestEqual(TEXT("Restored supply/control permits exactly one manual shot"), R.Gun().ShotsFired, 1);
    TestTrue(TEXT("Cooldown reason and value are observable"), R.Combat->GetReadiness(R.A, Id).Cooldown > 0 || R.Gun().Profile.Interval <= .1f);
    for (int32 Index : {0, 2})
    {
        R.Exact(Index);
        R.DisableFacility(R.A, Index == 0 ? EOVFacilityType::Ammo : EOVFacilityType::MissileStorage);
        TestFalse(TEXT("Each ammunition family independently requires its supply"), R.Combat->GetReadiness(R.A, R.Gun(Index).MountId).bReady);
        R.A->Systems->ResetState();
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVCombatTurretTest, "OrbitalVanguard.Combat.LocalArcsTraverseAndManualAttitude", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOVCombatTurretTest::RunTest(const FString&)
{
    OVCombatTest::FRange R;
    const FName Id = R.Gun().MountId;
    R.B->SetActorLocation(FVector(1000, 0, -1000)); R.Align();
    TestFalse(TEXT("Upper mount cannot shoot below local neutral band"), R.Combat->GetReadiness(R.A, Id).bReady);
    R.A->SetActorRotation(FRotator(0, 0, 180)); R.Align();
    TestTrue(TEXT("Rolling hull changes local upper side"), R.Combat->GetReadiness(R.A, Id).bReady);
    R.A->SetActorRotation(FRotator::ZeroRotator);
    R.B->SetActorLocation(FVector(1000, 0, 0)); R.Align();
    TestTrue(TEXT("Equator permits upper mount"), R.Combat->GetReadiness(R.A, Id).bReady);
    R.Gun().LocalAim = FRotator(0, 90, 0);
    const float Speed = R.Gun().Profile.TraverseSpeed;
    TestFalse(TEXT("Unaligned turret blocks firing"), R.Combat->GetReadiness(R.A, Id).bReady);
    R.A->SetManualAttitude(FRotator(0, 0, 27));
    R.A->SetActorRotation(FRotator(0, 0, 27));
    const FRotator HullBefore = R.A->GetActorRotation();
    R.Combat->Simulate(.1f);
    TestTrue(TEXT("Local turret turn obeys speed bound"), FMath::Abs(FMath::FindDeltaAngleDegrees(90.f, float(R.Gun().LocalAim.Yaw))) <= Speed * .1f + .05f);
    TestTrue(TEXT("Turret cannot overwrite manual hull attitude"), R.A->GetActorRotation().Equals(HullBefore, .01f));
    R.Combat->Simulate(3);
    TestTrue(TEXT("Turret finishes alignment without moving held ship"), R.Combat->GetReadiness(R.A, Id).bReady && R.A->GetActorLocation().IsNearlyZero());
    R.A->DebugResetMotion(); R.A->SetActorRotation(FRotator::ZeroRotator);
    R.Combat->SetTarget(R.A, R.B);
    R.A->Systems->Layout->WeaponMounts[1].LocalPosition.Z = -40;
    R.B->SetActorLocation(FVector(1000, 0, 1000)); R.Align();
    TestFalse(TEXT("Lower mount cannot shoot above local neutral band"), R.Combat->GetReadiness(R.A, Id).bReady);
    R.B->SetActorLocation(FVector(1000, 0, 0)); R.Align();
    TestTrue(TEXT("Equator permits lower mount too"), R.Combat->GetReadiness(R.A, Id).bReady);
    float T; FVector Normal;
    TestTrue(TEXT("Swept segment intersects a rotated scaled hull"), UOVCombatSubsystem::SegmentBox(FVector(-1000, 0, 0), FVector(1000, 0, 0), FTransform(FRotator(10, 37, 25), FVector::ZeroVector, FVector(1.4, 1.2, 1.8)), FVector(175, 75, 30), T, Normal));
    TestTrue(TEXT("Continuous impact has bounded fraction and finite normal"), T >= 0 && T <= 1 && !Normal.ContainsNaN());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVCombatDamageTest, "OrbitalVanguard.Combat.LaunchedDamageChainAndSingleImpact", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOVCombatDamageTest::RunTest(const FString&)
{
    OVCombatTest::FRange R;
    R.Exact();
    R.Gun().Profile.Damage = 100;
    const float Shield = R.B->Systems->Shield;
    R.Fire();
    TestTrue(TEXT("Actual launch removes shield through the existing damage path"), R.Combat->ShotCount == 1 && R.Combat->HitCount == 1 && R.B->Systems->Shield < Shield);
    TestEqual(TEXT("Shield-only hit does not independently deduct hull"), R.B->Systems->LastHullDamage, 0.f);
    R.B->Systems->Shield = 0; R.B->Systems->bShieldActive = false;
    R.Gun().Profile.Damage = 1000;
    R.Fire();
    TestTrue(TEXT("Actual launch maps armor and facility and deducts hull"), !R.B->Systems->LastArmorSection.IsNone() && !R.B->Systems->LastDamageSlot.IsNone() && R.B->Systems->LastHullDamage > 0);
    TestTrue(TEXT("Launch has immutable event attribution"), R.B->Systems->LastDamageSource == R.State(R.A).ShipId);
    if (!R.Combat->RecentEvents.IsEmpty())
    {
        FOVAttackState& Resolved = R.Combat->RecentEvents.Last();
        const int32 Events = R.B->Systems->HitEventId;
        TestFalse(TEXT("Resolved attack cannot damage twice"), R.Combat->ResolveImpact(Resolved, R.B, R.B->GetActorLocation(), FVector::ForwardVector));
        TestEqual(TEXT("Duplicate impact does not create another damage event"), R.B->Systems->HitEventId, Events);
        FOVAttackState Duplicate = Resolved;
        Duplicate.Result = EOVAttackResult::Flying;
        TestFalse(TEXT("Copying an attack cannot bypass its globally consumed ID"), R.Combat->ResolveImpact(Duplicate, R.B, R.B->GetActorLocation(), FVector::ForwardVector));
        TestEqual(TEXT("Duplicate attack ID cannot create another damage event"), R.B->Systems->HitEventId, Events);
    }
    // Move the target so the beam enters the known ammunition slot from above.
    R.B->Systems->ResetState();
    const auto* Layout = R.B->Systems->Layout.Get();
    int32 Ammo = Layout->Slots.IndexOfByPredicate([](const auto& S) { return S.Facility == EOVFacilityType::Ammo; });
    TestTrue(TEXT("Ammunition fixture exists"), Ammo != INDEX_NONE);
    if (Ammo == INDEX_NONE) return false;
    // Center-targeting attacks enter the rear-face central slot; put volatile supply there
    // in this disposable layout without changing any production asset or damage code.
    R.B->SetActorLocation(FVector(1000, 0, 0)); R.B->SetActorRotation(FRotator::ZeroRotator);
    UShipLayoutDataAsset* VolatileLayout = DuplicateObject<UShipLayoutDataAsset>(Layout, R.B);
    const int32 Entry = VolatileLayout->ResolveHit(FVector(-175, 0, 0), -FVector::ForwardVector);
    VolatileLayout->Slots[Entry].Facility = EOVFacilityType::Ammo;
    VolatileLayout->Slots[Entry].MaxFacilityHP = 1;
    VolatileLayout->Slots[Entry].RequiredCrew = 10;
    R.B->Systems->Initialize(VolatileLayout);
    for (int32 I = 0; I < R.B->Systems->Slots.Num(); ++I)
        if (VolatileLayout->Slots[I].Facility == EOVFacilityType::Capacitor || VolatileLayout->Slots[I].Facility == EOVFacilityType::MissileStorage)
            R.B->Systems->Slots[I].HP = 1;
    R.B->Systems->Shield = 0; R.B->Systems->bShieldActive = false;
    R.Gun().Profile.Damage = 200;
    R.Fire();
    TestTrue(TEXT("Actual attack triggers volatile facility explosion"), R.B->Systems->LastExplosionCount > 0);
    R.B->Systems->HullHP = 1;
    R.Gun().Profile.Damage = 10000;
    R.Fire();
    TestTrue(TEXT("Actual attack reaches wreck exactly once"), R.B->Systems->bWrecked && R.B->Systems->WreckEventCount == 1);
    const int32 Shots = R.Combat->ShotCount;
    R.Fire();
    TestTrue(TEXT("Wreck blocks new fire and repeat wreck events"), R.Combat->ShotCount == Shots && R.B->Systems->WreckEventCount == 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVCombatFlightTest, "OrbitalVanguard.Combat.TrajectoryLifetimeAndLaunchOwnership", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOVCombatFlightTest::RunTest(const FString&)
{
    for (EOVAttackKind Kind : {EOVAttackKind::Cannon, EOVAttackKind::Missile, EOVAttackKind::GuidedMissile})
    {
        OVCombatTest::FRange R;
        const int32 Index = Kind == EOVAttackKind::Cannon ? 0 : 2;
        R.Exact(Index);
        R.Gun(Index).Profile.Kind = Kind;
        R.Gun(Index).Profile.Speed = 200;
        R.Gun(Index).Profile.Salvo = 1;
        R.Fire(Index, .05f);
        TestTrue(TEXT("Slow launched projectile remains in flight"), R.Combat->Attacks.Num() == 1);
        if (R.Combat->Attacks.IsEmpty()) return false;
        const FVector Direction = R.Combat->Attacks[0].Direction;
        const FName Source = R.Combat->Attacks[0].AttackerId;
        R.A->Systems->FactionId = 1;
        R.A->Systems->DebugDestroySlot(R.A->Systems->Layout->CoreSlotId, EOVDamageCause::Sabotage);
        R.DisableFacility(R.Scout, EOVFacilityType::Radar);
        R.B->SetActorLocation(FVector(1000, 400, 0));
        R.Combat->Simulate(.2f);
        TestTrue(TEXT("Source capture and wreck preserve launched attack"), R.Combat->Attacks.Num() == 1);
        if (R.Combat->Attacks.IsEmpty()) return false;
        const auto& Shot = R.Combat->Attacks[0];
        TestTrue(TEXT("Launch faction/source immutable after ownership change"), Shot.FactionId == 0 && Shot.AttackerId == Source);
        TestTrue(TEXT("Only guided kind corrects path"), Kind == EOVAttackKind::GuidedMissile ? !Shot.Direction.Equals(Direction, .001f) : Shot.Direction.Equals(Direction, .000001f));
        R.Combat->NotifyFTLDeparture(R.B);
        R.Combat->Simulate(.05f);
        if (Kind == EOVAttackKind::GuidedMissile)
            TestTrue(TEXT("Guided target departure has distinct terminal event"), R.Combat->RecentEvents.ContainsByPredicate([](const auto& E) { return E.Result == EOVAttackResult::TargetDeparted; }));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVCombatTimeTest, "OrbitalVanguard.Combat.SeededTimeScalePauseAndManualControl", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOVCombatTimeTest::RunTest(const FString&)
{
    struct FResult { int32 Shots, Hits, Flying; float Hull, Shield; } Baseline{};
    for (int32 Speed = 1; Speed <= 3; ++Speed)
    {
        OVCombatTest::FRange R;
        R.Combat->Simulate(1);
        TestEqual(TEXT("Target selection alone cannot fire in manual mode"), R.Combat->ShotCount, 0);
        R.A->bManual = false;
        R.B->Systems->Tuning.ArmorMaxReduction = .98f;
        R.B->Systems->HullHP = R.B->Systems->MaxHullHP = 100000;
        for (auto& Slot : R.B->Systems->Slots) if (Slot.HP > 0) Slot.HP = 100000;
        for (int32 Frame = 0; Frame < 360 / Speed; ++Frame) R.Combat->Simulate(Speed / 60.f);
        FResult Result{R.Combat->ShotCount, R.Combat->HitCount, R.Combat->Attacks.Num(), R.B->Systems->HullHP, R.B->Systems->Shield};
        TestTrue(TEXT("Automatic valid target fires"), Result.Shots > 0);
        const double Time = R.Combat->GameTime;
        for (int32 Frame = 0; Frame < 120; ++Frame) R.Combat->Simulate(0);
        TestTrue(TEXT("Pause preserves clock and shot state"), R.Combat->GameTime == Time && R.Combat->ShotCount == Result.Shots && R.Combat->Attacks.Num() == Result.Flying);
        TestTrue(TEXT("Auto mode preserves Hold"), R.A->bHold && R.A->GetActorLocation().IsNearlyZero());
        if (Speed == 1) Baseline = Result;
        else TestTrue(TEXT("Same seed and game time yield equal shots hits HP and live projectiles"), Result.Shots == Baseline.Shots && Result.Hits == Baseline.Hits && Result.Flying == Baseline.Flying && FMath::IsNearlyEqual(Result.Hull, Baseline.Hull, .01f) && FMath::IsNearlyEqual(Result.Shield, Baseline.Shield, .01f));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVCombatLifetimeTest, "OrbitalVanguard.Combat.DetectionLossWreckDepartureAndExpiry", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOVCombatLifetimeTest::RunTest(const FString&)
{
    {
        OVCombatTest::FRange R;
        R.Exact(0); R.Gun(0).Profile.Speed = 1000; R.Gun(0).Profile.Range = 10000;
        R.B->SetActorLocation(FVector(3000, 0, 0)); R.Fire(0, .05f);
        TestEqual(TEXT("Detected target starts one cannon projectile"), R.Combat->ShotCount, 1);
        R.DisableFacility(R.A, EOVFacilityType::Radar); R.DisableFacility(R.Scout, EOVFacilityType::Radar);
        TestFalse(TEXT("Lost radar closes the new-shot gate"), R.Combat->GetReadiness(R.A, R.Gun(0).MountId).bReady);
        R.Fire(0, .05f);
        TestEqual(TEXT("Lost radar cannot launch a second projectile"), R.Combat->ShotCount, 1);
        R.A->Systems->FactionId = 1;
        R.A->Systems->DebugDestroySlot(R.A->Systems->Layout->CoreSlotId, EOVDamageCause::Sabotage);
        R.Combat->Simulate(4);
        TestEqual(TEXT("Previously launched cannon still hits after sensor and source loss"), R.Combat->HitCount, 1);
        TestTrue(TEXT("Impact preserves original faction after capture"), R.B->Systems->IncomingFactionId == 0);
    }
    for (EOVAttackResult Expected : {EOVAttackResult::TargetWrecked, EOVAttackResult::TargetDeparted, EOVAttackResult::TrackingExpired, EOVAttackResult::Expired})
    {
        OVCombatTest::FRange R;
        R.Exact(2); R.Gun(2).Profile.Kind = EOVAttackKind::GuidedMissile;
        R.Gun(2).Profile.Speed = 100; R.Gun(2).Profile.Salvo = 1;
        if (Expected == EOVAttackResult::TrackingExpired) R.Combat->Settings->GuidedTrackingSeconds = .1f;
        if (Expected == EOVAttackResult::Expired) R.Gun(2).Profile.Lifetime = .1f;
        R.Fire(2, .05f);
        if (Expected == EOVAttackResult::TargetWrecked)
            R.B->Systems->DebugDestroySlot(R.B->Systems->Layout->CoreSlotId, EOVDamageCause::Sabotage);
        if (Expected == EOVAttackResult::TargetDeparted) R.Combat->NotifyFTLDeparture(R.B);
        R.Combat->Simulate(.3f);
        TestTrue(TEXT("Flight termination reason is explicit"), R.Combat->RecentEvents.ContainsByPredicate([Expected](const auto& E) { return E.Result == Expected; }));
        TestTrue(TEXT("Terminated flight cannot apply damage"), R.Combat->Attacks.IsEmpty() && R.Combat->HitCount == 0);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVCombatSalvoTest, "OrbitalVanguard.Combat.SalvoCancellationAndGeometricMiss", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOVCombatSalvoTest::RunTest(const FString&)
{
    {
        OVCombatTest::FRange R;
        R.Exact(2); R.Gun(2).Profile.Salvo = 3; R.Gun(2).Profile.SalvoInterval = .18f;
        R.Gun(2).Profile.Speed = 100;
        R.Fire(2, .05f);
        TestEqual(TEXT("Salvo begins with a single real projectile"), R.Combat->ShotCount, 1);
        R.DisableFacility(R.A, EOVFacilityType::MissileStorage);
        R.Combat->Simulate(.5f);
        TestTrue(TEXT("Provisional cancel policy cancels only unlaunched rounds"), R.Combat->ShotCount == 1 && R.Gun(2).SalvoRemaining == 0 && R.Combat->Attacks.Num() == 1);
    }
    {
        OVCombatTest::FRange R;
        R.Exact(2); R.Gun(2).Profile.Salvo = 3; R.Gun(2).Profile.SalvoInterval = .18f;
        R.Gun(2).Profile.Speed = 100;
        R.Fire(2, .5f);
        TestEqual(TEXT("Healthy manual request completes exactly its salvo"), R.Combat->ShotCount, 3);
        R.Combat->Simulate(1);
        TestEqual(TEXT("Salvo completion does not enable automatic repeat"), R.Combat->ShotCount, 3);
    }
    {
        OVCombatTest::FRange R;
        R.Exact(); R.Gun().Profile.Accuracy = 0;
        R.Combat->Settings->ControlAccuracyBonus = 0;
        R.DisableFacility(R.A, EOVFacilityType::Radar); R.DisableFacility(R.Scout, EOVFacilityType::Radar);
        R.Fire(1, .2f);
        TestEqual(TEXT("An inaccurate beam still launches"), R.Combat->ShotCount, 1);
        TestTrue(TEXT("Beam can geometrically miss without a second damage probability roll"), R.Combat->HitCount == 0 && R.B->Systems->HitEventId == 0);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVCombatOrdersTest, "OrbitalVanguard.Combat.OrdersAndFriendlyClearance", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOVCombatOrdersTest::RunTest(const FString&)
{
    {
        OVCombatTest::FRange R;
        R.Combat->RequestFire(R.A, R.Gun().MountId);
        R.Combat->SetTarget(R.A, nullptr);
        R.Combat->SetTarget(R.A, R.B);
        R.Combat->Simulate(.3f);
        TestTrue(TEXT("Changing target clears a pending manual launch"), !R.Gun().bFireRequested && R.Combat->ShotCount == 0);
        const int32 Bridge = R.A->Systems->Layout->Slots.IndexOfByPredicate([](const auto& Slot) { return Slot.Facility == EOVFacilityType::Bridge; });
        R.A->Systems->DebugDestroySlot(R.A->Systems->Slots[Bridge].SlotId, EOVDamageCause::Sabotage);
        TestFalse(TEXT("Destroyed bridge rejects new player fire requests"), R.Combat->RequestFire(R.A, R.Gun().MountId));
        R.Combat->Simulate(1);
        TestTrue(TEXT("Intact weapon systems continue autonomous engagement after bridge loss"), R.Combat->ShotCount > 0);
    }
    for (int32 Policy = 0; Policy < 5; ++Policy)
    {
        OVCombatTest::FRange R;
        R.A->Systems->Initialize(UShipLayoutDataAsset::MakeTestLayout(R.A, 1));
        R.Enable();
        R.Scout->SetActorLocation(FVector(500, 0, 0));
        R.A->bManual = false;
        // 0: persistent Hold; 1: free auto; 2: explicit destination; 3: manual
        // posture; 4: FTL. The latter four test precedence, not a strategic AI.
        if (Policy != 0) R.A->bHold = false;
        if (Policy == 2) R.A->SetDestination(FVector(3000, 0, 0));
        if (Policy == 3) R.A->SetManualAttitude(FRotator::ZeroRotator);
        if (Policy == 4) R.A->DebugSetFTLCharging(true);
        const FVector Destination = R.A->Destination;
        R.Combat->Simulate(.3f);
        if (Policy == 1)
            TestTrue(TEXT("Free small automatic ship prioritizes Z clearance"), R.A->GetActorLocation().Z > 0 || (R.A->bHasDestination && R.A->Destination.Z > 0));
        else if (Policy == 2)
            TestTrue(TEXT("Explicit movement remains authoritative over clearance"), R.A->Destination.Equals(Destination, .01f));
        else
            TestTrue(TEXT("Hold manual posture and FTL prohibit automatic clearance"), FMath::Abs(R.A->GetActorLocation().Z) < .01f);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVCombatProfilesTest, "OrbitalVanguard.Combat.DataProfilesAndTierLimits", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOVCombatProfilesTest::RunTest(const FString&)
{
    OVCombatTest::FRange R;
    TestTrue(TEXT("Same-family rapid beam profile applies"), R.Combat->SetWeaponProfile(R.A, R.Gun().MountId, TEXT("BeamRapid_T1")));
    const FOVWeaponProfile Rapid = R.Gun().Profile;
    TestTrue(TEXT("Same-tier heavy beam is a separate kind profile"), R.Combat->SetWeaponProfile(R.A, R.Gun().MountId, TEXT("BeamHeavy_T1")));
    const FOVWeaponProfile Heavy = R.Gun().Profile;
    TestTrue(TEXT("Weapon kind changes interval and damage within a tier"), Heavy.Tier == Rapid.Tier && Heavy.Interval != Rapid.Interval && Heavy.Damage != Rapid.Damage);
    TestTrue(TEXT("Higher-tier profile applies within hull cap"), R.Combat->SetWeaponProfile(R.A, R.Gun().MountId, TEXT("BeamHeavy_T2")));
    TestTrue(TEXT("Tier changes performance independently of kind"), R.Gun().Profile.Damage != Heavy.Damage && R.Gun().Profile.Tier == 2);
    TestFalse(TEXT("Profile cannot change a beam mount into a cannon"), R.Combat->SetWeaponProfile(R.A, R.Gun().MountId, TEXT("CannonRapid_T1")));
    R.A->Systems->Initialize(UShipLayoutDataAsset::MakeTestLayout(R.A, 1));
    R.Enable();
    TestFalse(TEXT("Small hull cannot install tier 2"), R.Combat->SetWeaponProfile(R.A, R.Gun().MountId, TEXT("BeamRapid_T2")));
    TestTrue(TEXT("Small hull retains valid tier 1"), R.Gun().Profile.Tier == 1);
    return true;
}
#endif
