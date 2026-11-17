#include "OVWeaponVisualComponent.h"
#include "OVCombatSubsystem.h"
#include "OVPointDefenseSubsystem.h"
#include "OVShip.h"
#include "OVShipSystemsComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace OVWeaponVisualTest
{
struct FRange
{
    UWorld* World;
    UOVCombatSubsystem* Combat;
    AOVShip* A;
    AOVShip* B;
    FRange()
    {
        World = UWorld::CreateWorld(EWorldType::Game, false);
        Combat = World->GetSubsystem<UOVCombatSubsystem>();
        A = Make(0, FVector::ZeroVector);
        B = Make(1, FVector(1400, 0, 200));
        Combat->EnableCombat({A, B}, 171117);
        Combat->Settings->GuaranteedRange = 100000;
        Combat->SetTarget(A, B);
    }
    ~FRange() { World->DestroyWorld(false); }
    AOVShip* Make(int32 Faction, FVector Position)
    {
        AOVShip* Ship = World->SpawnActor<AOVShip>();
        Ship->Systems->FactionId = Faction;
        Ship->Systems->Initialize(UShipLayoutDataAsset::MakeTestLayout(Ship, 3));
        Ship->SetActorLocation(Position);
        Ship->DebugResetMotion();
        return Ship;
    }
    FOVCombatMountState& Gun(int32 Index = 1) { return Combat->Ships[0].Mounts[Index]; }
    FOVWeaponVisualState Visual(int32 Index = 1)
    {
        const FName Id = Gun(Index).MountId;
        const auto States = A->GetWeaponVisualStates();
        const auto* State = States.FindByPredicate([Id](const FOVWeaponVisualState& V) { return V.MountId == Id; });
        return State ? *State : FOVWeaponVisualState();
    }
    void Accurate(int32 Index = 1)
    {
        auto& Mount = Gun(Index);
        Mount.Profile.Accuracy = 1; Mount.Profile.AimSpreadDegrees = 0;
        Mount.Profile.FireTolerance = 180; Mount.Profile.Range = 100000;
        Mount.Profile.Salvo = 1; Mount.Profile.Damage = 1;
        Mount.LocalAim = (B->GetActorLocation() - A->GetActorTransform().TransformPosition(
            A->Systems->Layout->WeaponMounts[Index].LocalPosition)).Rotation();
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVWeaponVisualPoseTest, "OrbitalVanguard.Presentation.TurretPoseOwnershipAndLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOVWeaponVisualPoseTest::RunTest(const FString&)
{
    OVWeaponVisualTest::FRange R;
    TestEqual(TEXT("Every registered attack mount has one presentation"), R.A->GetWeaponVisualStates().Num(), 3);
    R.Gun().LocalAim = FRotator(12, 105, 0);
    R.A->SetManualAttitude(FRotator(0, 0, 23)); R.A->SetActorRotation(FRotator(0, 0, 23));
    const FRotator Hull = R.A->GetActorRotation();
    R.Combat->Simulate(.1f);
    auto V = R.Visual();
    TestTrue(TEXT("Rendered yaw and pitch are the actual bounded logical aim"), V.VisualAim.Equals(R.Gun().LocalAim, .0001));
    TestTrue(TEXT("Yaw has not snapped to the target"), FMath::Abs(V.VisualAim.Yaw) > 70);
    TestTrue(TEXT("Presentation never changes manual hull posture"), R.A->GetActorRotation().Equals(Hull));
    TestTrue(TEXT("Visual muzzle and gameplay query share transform"), V.WorldMuzzle.Equals(R.A->GetWeaponMuzzleWorld(V.MountId), .001));
    const int32 Components = R.A->GetComponents().Num();
    const int32 Shots = R.Combat->ShotCount;
    const float HP = R.B->Systems->HullHP;
    for (int32 I = 0; I < 100; ++I) { R.A->RefreshWeaponVisuals(); R.A->GetWeaponVisualStates(); }
    TestEqual(TEXT("Repeated display queries create no additional components"), R.A->GetComponents().Num(), Components);
    TestEqual(TEXT("Repeated viewing cannot launch"), R.Combat->ShotCount, Shots);
    TestEqual(TEXT("Repeated viewing cannot damage"), R.B->Systems->HullHP, HP);
    TArray<UOVWeaponVisualComponent*> Visuals;
    R.A->GetComponents(Visuals);
    for (auto* Visual : Visuals)
    {
        TestFalse(TEXT("Display has no independent Tick"), Visual->PrimaryComponentTick.bCanEverTick);
        TArray<USceneComponent*> Children;
        Visual->GetChildrenComponents(true, Children);
        for (auto* Child : Children)
            if (auto* Mesh = Cast<UStaticMeshComponent>(Child))
                TestEqual(TEXT("Visual does not duplicate selection or weapon collisions"), int32(Mesh->GetCollisionEnabled()), int32(ECollisionEnabled::NoCollision));
    }
    R.Combat->DisableCombat();
    TestEqual(TEXT("Scenario teardown removes every weapon visual"), R.A->GetWeaponVisualStates().Num(), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVWeaponVisualShotTest, "OrbitalVanguard.Presentation.MuzzleAttackEventAndGameClock",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOVWeaponVisualShotTest::RunTest(const FString&)
{
    OVWeaponVisualTest::FRange R;
    R.Accurate();
    R.Combat->RequestFire(R.A, R.Gun().MountId);
    R.Combat->Simulate(1.f / 120.f);
    TestEqual(TEXT("One request fires exactly one actual beam"), R.Combat->ShotCount, 1);
    if (R.Combat->RecentEvents.IsEmpty()) return false;
    const auto Attack = R.Combat->RecentEvents.Last();
    auto V = R.Visual();
    TestEqual(TEXT("Muzzle flash uses actual attack ID"), V.LastShotId, Attack.AttackId);
    TestTrue(TEXT("Beam ray starts at visible outlet"), V.WorldMuzzle.Equals(Attack.Origin, .001));
    TestTrue(TEXT("Real launch flashes"), V.bFlashVisible);
    const double Clock = R.Combat->GameTime;
    R.Combat->Simulate(0); R.A->RefreshWeaponVisuals();
    TestTrue(TEXT("Pause preserves game time and flash"), R.Combat->GameTime == Clock && R.Visual().bFlashVisible);
    R.Combat->Simulate(.2f);
    TestFalse(TEXT("Flash expires with game time and is not replayed by refresh"), R.Visual().bFlashVisible);
    TestEqual(TEXT("Animation never emits a second shot"), R.Combat->ShotCount, 1);
    R.Accurate(0); R.Combat->RequestFire(R.A, R.Gun(0).MountId); R.Combat->Simulate(1.f / 120.f);
    TestTrue(TEXT("Physical projectile exists after cannon request"), R.Combat->Attacks.Num() > 0);
    if (R.Combat->Attacks.Num())
        TestTrue(TEXT("Cannon starts at its own actual muzzle"), R.Combat->Attacks.Last().Origin.Equals(R.Visual(0).WorldMuzzle, .001));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVWeaponVisualDamageTest, "OrbitalVanguard.Presentation.UpperLowerVLSAndDestroyedTracking",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOVWeaponVisualDamageTest::RunTest(const FString&)
{
    OVWeaponVisualTest::FRange R;
    R.A->Systems->Layout->WeaponMounts[1].LocalPosition.Z = -70;
    R.Gun().LocalAim = FRotator(-25, -40, 0);
    R.A->RefreshWeaponVisuals();
    auto V = R.Visual();
    TestTrue(TEXT("Lower gun retains local yaw/pitch without flipping hull"), V.bBottom && V.VisualAim.Equals(FRotator(-25, -40, 0)));
    TestTrue(TEXT("Lower muzzle points below its mounting point"), V.LocalMuzzle.Z < -70);
    R.Gun(2).LocalAim = FRotator(15, 120, 0);
    R.A->RefreshWeaponVisuals();
    const auto VLS = R.Visual(2);
    TestTrue(TEXT("Fixed missile outlet stays vertical while fire-control aim turns"), VLS.bFixedVLS && FMath::IsNearlyEqual(VLS.VisualAim.Pitch, 90.0));
    TestTrue(TEXT("VLS retains underlying logical aim for readiness"), VLS.LogicalAim.Equals(R.Gun(2).LocalAim));
    R.A->Systems->DebugHitWeapon(R.Gun().MountId, 100000);
    const FRotator Stopped = R.Gun().LocalAim;
    R.B->SetActorLocation(FVector(-1500, 200, -400));
    R.Combat->Simulate(.2f);
    TestTrue(TEXT("Destroyed mount stops actual tracking and shows damage"), R.Gun().LocalAim.Equals(Stopped) && R.Visual().bDestroyed);
    TestFalse(TEXT("Destroyed weapon never flashes"), R.Visual().bFlashVisible);
    R.A->Systems->DebugRepairWeapon(R.Gun().MountId);
    R.Combat->Simulate(.2f);
    TestTrue(TEXT("Actual repair restores model and tracking"), !R.Visual().bDestroyed && !R.Gun().LocalAim.Equals(Stopped));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVWeaponVisualPDTest, "OrbitalVanguard.Presentation.PDActualTargetMuzzleAndOpportunity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOVWeaponVisualPDTest::RunTest(const FString&)
{
    OVWeaponVisualTest::FRange R;
    R.B->SetActorLocation(FVector(5000, 0, 0));
    R.Accurate(2); R.Gun(2).Profile.Kind = EOVAttackKind::GuidedMissile;
    R.Gun(2).Profile.Speed = 100; R.Gun(2).Profile.InterceptionEvasion = 0;
    R.Combat->RequestFire(R.A, R.Gun(2).MountId); R.Combat->Simulate(1.f / 120.f);
    auto* PD = R.World->GetSubsystem<UOVPointDefenseSubsystem>();
    PD->Reset(); PD->RegisterShip(R.B, 1);
    PD->Ships[0].Mounts[0].Profile.Accuracy = 1;
    PD->Ships[0].Mounts[0].Profile.Damage = 1000;
    PD->Step(1.f / 120.f); R.B->RefreshWeaponVisuals();
    TestEqual(TEXT("PD consumes one actual opportunity"), PD->ShotCount, 1);
    if (PD->RecentEvents.IsEmpty()) return false;
    const auto Event = PD->RecentEvents.Last();
    const auto Visuals = R.B->GetWeaponVisualStates();
    const auto* V = Visuals.FindByPredicate([&Event](const auto& S) { return S.MountId == Event.MountId && S.bPointDefense; });
    TestNotNull(TEXT("Registered PD has a separate physical presentation"), V);
    if (V)
    {
        TestEqual(TEXT("PD flash identifies the real interception event"), V->LastShotId, Event.EventId);
        TestTrue(TEXT("PD event starts at actual outlet"), V->WorldMuzzle.Equals(Event.Origin, .001));
        TestTrue(TEXT("PD displays actual selected direction"), V->VisualAim.Equals(PD->Ships[0].Mounts[0].LocalAim));
    }
    for (int32 I = 0; I < 40; ++I) R.B->RefreshWeaponVisuals();
    TestEqual(TEXT("Repeated viewer updates do not duplicate interception"), PD->ShotCount, 1);
    TestTrue(TEXT("Intercepted attack removed before it can impact a hull"), R.Combat->Attacks.IsEmpty());
    return true;
}
#endif
