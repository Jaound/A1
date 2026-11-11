#include "OVShip.h"
#include "OVShipSystemsComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#endif

namespace OVShipMotion
{
    static void Turn(FQuat& Orientation, const FQuat& Desired, float TurnRate, float Delta)
    {
        if (Delta <= 0.0f || !FMath::IsFinite(Delta)) return;
        const double Angle = Orientation.AngularDistance(Desired);
        const double MaxTurn = FMath::DegreesToRadians(FMath::Max(0.0f, TurnRate)) * Delta;
        const double Alpha = Angle > UE_SMALL_NUMBER ? FMath::Min(1.0, MaxTurn / Angle) : 1.0;
        Orientation = FQuat::Slerp(Orientation, Desired, Alpha).GetNormalized();
    }

    // Integrate a bounded velocity change, including any constant-speed remainder.
    static FVector ChangeVelocity(FVector& Velocity, const FVector& Desired, double Rate, double Delta)
    {
        const FVector Difference = Desired - Velocity;
        const double DifferenceSize = Difference.Size();
        if (DifferenceSize <= UE_SMALL_NUMBER || Rate <= UE_SMALL_NUMBER)
            return Velocity * Delta;
        const double ChangingTime = FMath::Min(Delta, DifferenceSize / Rate);
        const FVector Accel = Difference / DifferenceSize * Rate;
        FVector Travel = Velocity * ChangingTime + Accel * (0.5 * ChangingTime * ChangingTime);
        Velocity += Accel * ChangingTime;
        if (ChangingTime < Delta) Velocity = Desired;
        Travel += Velocity * (Delta - ChangingTime);
        if (Velocity.SizeSquared() < 0.000001) Velocity = FVector::ZeroVector;
        return Travel;
    }

    static void Brake(FVector& Position, FVector& Velocity, float Braking, float Delta)
    {
        if (Delta <= 0.0f || !FMath::IsFinite(Delta)) return;
        Position += ChangeVelocity(Velocity, FVector::ZeroVector, FMath::Max(1.0f, Braking), Delta);
    }

    // Translation has inertia but is independent of a deliberate broadside posture.
    // Fixed small substeps keep arrival braking stable at 1/2/3x and during long frames.
    static bool Advance(FVector& Position, FQuat& Orientation, FVector& Velocity,
        const FVector& Destination, float Speed, float TurnRate, float Delta,
        float Acceleration = 1700.0f, float Braking = 2200.0f, float ArrivalRadius = 5.0f,
        bool bManualAttitude = false, const FQuat& ManualOrientation = FQuat::Identity)
    {
        if (Delta <= 0.0f || !FMath::IsFinite(Delta) || Destination.ContainsNaN()) return false;
        const double Radius = FMath::Max(0.1f, ArrivalRadius);
        float Remaining = Delta;
        while (Remaining > UE_SMALL_NUMBER)
        {
            const float Step = FMath::Min(Remaining, 1.0f / 120.0f);
            const FVector Offset = Destination - Position;
            const double Distance = Offset.Size();
            if (Distance <= Radius)
            {
                Position = Destination;
                Velocity = FVector::ZeroVector;
                return true;
            }
            const FVector Direction = Offset / Distance;
            Turn(Orientation, bManualAttitude ? ManualOrientation : Direction.ToOrientationQuat(), TurnRate, Step);
            const double Alignment = FVector::DotProduct(Orientation.GetForwardVector(), Direction);
            const double Throttle = bManualAttitude ? 1.0 : FMath::Clamp((Alignment - 0.6) / 0.4, 0.0, 1.0);
            const double BrakeRate = FMath::Max(1.0f, Braking);
            const double ArrivalSpeed = FMath::Sqrt(2.0 * BrakeRate * FMath::Max(0.0, Distance - Radius));
            const double DesiredSpeed = FMath::Min(FMath::Max(0.0f, Speed) * Throttle, ArrivalSpeed);
            const double Rate = DesiredSpeed < Velocity.Size() ? BrakeRate : FMath::Max(0.0f, Acceleration);
            const FVector Travel = ChangeVelocity(Velocity, Direction * DesiredSpeed, Rate, Step);
            // Swept arrival catches a high-speed step without teleporting across a lateral miss.
            const double TravelSquared = Travel.SizeSquared();
            const double ClosestT = TravelSquared > UE_SMALL_NUMBER
                ? FMath::Clamp(FVector::DotProduct(Offset, Travel) / TravelSquared, 0.0, 1.0) : 0.0;
            if ((Offset - Travel * ClosestT).SizeSquared() <= Radius * Radius)
            {
                Position = Destination;
                Velocity = FVector::ZeroVector;
                return true;
            }
            Position += Travel;
            Remaining -= Step;
        }
        return false;
    }
}

AOVShip::AOVShip()
{
    PrimaryActorTick.bCanEverTick = false;
    Systems = CreateDefaultSubobject<UOVShipSystemsComponent>(TEXT("ShipSystems"));
    USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("ShipRoot"));
    SetRootComponent(Root);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cone(TEXT("/Engine/BasicShapes/Cone.Cone"));
    Hull = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Hull"));
    Hull->SetupAttachment(Root);
    Hull->SetStaticMesh(Cube.Object);
    Hull->SetRelativeScale3D(FVector(3.2, 1.0, 0.6));
    Hull->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Hull->SetCollisionResponseToAllChannels(ECR_Ignore);
    Hull->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    Hull->SetGenerateOverlapEvents(false);

    auto AddAccent = [&](const TCHAR* Name, UStaticMesh* Mesh, FVector Location, FVector Scale, FRotator Rotation)
    {
        UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(Name);
        Part->SetupAttachment(Root);
        Part->SetStaticMesh(Mesh);
        Part->SetRelativeLocation(Location);
        Part->SetRelativeScale3D(Scale);
        Part->SetRelativeRotation(Rotation);
        Part->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        Part->SetCollisionResponseToAllChannels(ECR_Ignore);
        Part->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
        AccentMeshes.Add(Part);
    };
    AddAccent(TEXT("Bow"), Cone.Object, FVector(190, 0, 0), FVector(1.0, 1.0, 1.25), FRotator(-90, 0, 0));
    AddAccent(TEXT("Bridge"), Cube.Object, FVector(-35, 0, 45), FVector(0.75, 0.65, 0.4), FRotator::ZeroRotator);
    AddAccent(TEXT("PortEngine"), Cube.Object, FVector(-125, -75, 0), FVector(1.0, 0.35, 0.45), FRotator::ZeroRotator);
    AddAccent(TEXT("StarboardEngine"), Cube.Object, FVector(-125, 75, 0), FVector(1.0, 0.35, 0.45), FRotator::ZeroRotator);
}

void AOVShip::Configure(const FString& Name, bool Friendly, float Speed, float RotationRate,
    const FVector& Scale, const FLinearColor& Color)
{
    ShipName = Name;
    bFriendly = Friendly;
    MoveSpeed = FMath::Max(0.0f, Speed);
    TurnRate = FMath::Max(0.0f, RotationRate);
    SetActorScale3D(Scale);
    UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    if (Base)
    {
        UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Base, this);
        Material->SetVectorParameterValue(TEXT("Color"), Color);
        Hull->SetMaterial(0, Material);
        UMaterialInstanceDynamic* Accent = UMaterialInstanceDynamic::Create(Base, this);
        Accent->SetVectorParameterValue(TEXT("Color"), FLinearColor(Color.R * 0.5f + 0.15f, Color.G * 0.5f + 0.15f, Color.B * 0.5f + 0.15f));
        for (UStaticMeshComponent* Part : AccentMeshes) Part->SetMaterial(0, Accent);
    }
}

void AOVShip::SetDestination(const FVector& Location)
{
    if (!Location.ContainsNaN() && !bFTLCharging && (!Systems || !Systems->Layout || Systems->CanMove()))
    {
        Destination = Location;
        bHasDestination = true;
        bHold = false;
    }
}

void AOVShip::StopMovement()
{
    bHasDestination = false;
    bHold = true;
    // Cancel an unfinished facing turn without resetting the current pitch or roll.
    if (bManualAttitude) ManualAttitude = GetActorRotation();
}

void AOVShip::SetManualAttitude(const FRotator& Rotation)
{
    if (Rotation.ContainsNaN() || bFTLCharging || (Systems && Systems->Layout && !Systems->CanMove())) return;
    ManualAttitude = Rotation.GetNormalized();
    bManualAttitude = true;
}

void AOVShip::Simulate(float SimDelta)
{
    if (!FMath::IsFinite(SimDelta) || SimDelta <= 0.0f) return;
    if (bFTLCharging || (Systems && Systems->Layout && !Systems->CanMove()))
    {
        // Gameplay immobilization is separate from an ordinary inertial stop order.
        Velocity = FVector::ZeroVector;
        if (bFTLCharging || (Systems && (Systems->bWrecked || Systems->bNeutral)))
        {
            bHasDestination = false;
            bHold = true;
        }
        if (Systems && (Systems->bWrecked || Systems->bNeutral)) AttackTarget = nullptr;
        return;
    }
    const float EngineEfficiency = Systems && Systems->Layout ? Systems->GetEfficiency(EOVFacilityType::Engine) : 1.f;
    FVector Position = GetActorLocation();
    FQuat Orientation = GetActorQuat();
    if (bHasDestination && !bHold)
    {
        if (OVShipMotion::Advance(Position, Orientation, Velocity, Destination, MoveSpeed * EngineEfficiency, TurnRate * EngineEfficiency,
            SimDelta, Acceleration, Braking, ArrivalRadius, bManualAttitude, ManualAttitude.Quaternion()))
        {
            bHasDestination = false;
            bHold = true;
        }
    }
    else
    {
        OVShipMotion::Brake(Position, Velocity, Braking, SimDelta);
        if (bManualAttitude) OVShipMotion::Turn(Orientation, ManualAttitude.Quaternion(), TurnRate * EngineEfficiency, SimDelta);
    }
    SetActorLocationAndRotation(Position, Orientation, false, nullptr, ETeleportType::TeleportPhysics);
}

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVShipMotionTest, "OrbitalVanguard.Ship.FiniteTurnXYZArrival",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOVShipMotionTest::RunTest(const FString& Parameters)
{
    FVector Position = FVector::ZeroVector;
    FVector Velocity = FVector::ZeroVector;
    FQuat Rotation = FQuat::Identity;
    const FVector Target(0, 1000, 400);
    OVShipMotion::Advance(Position, Rotation, Velocity, Target, 700, 30, 0.1f);
    TestTrue(TEXT("Turning is limited to three degrees in 0.1 seconds"),
        FQuat::Identity.AngularDistance(Rotation) <= FMath::DegreesToRadians(3.01));
    TestTrue(TEXT("Ship waits while facing away"), Position.IsNearlyZero());
    const FVector PausedPosition = Position;
    const FQuat PausedRotation = Rotation;
    OVShipMotion::Advance(Position, Rotation, Velocity, Target, 700, 30, 0.0f);
    TestTrue(TEXT("Zero simulation time preserves position and heading"),
        Position.Equals(PausedPosition) && Rotation.Equals(PausedRotation));
    bool Arrived = false;
    for (int32 Index = 0; Index < 1800 && !Arrived; ++Index)
        Arrived = OVShipMotion::Advance(Position, Rotation, Velocity, Target, 700, 30, 1.0f / 60.0f);
    TestTrue(TEXT("Ship reaches elevated XYZ destination"), Arrived && Position.Equals(Target, 0.01));
    const FVector FinalPosition = Position;
    OVShipMotion::Advance(Position, Rotation, Velocity, Target, 700, 30, 0.1f);
    TestTrue(TEXT("Arrival does not overshoot"), Position.Equals(FinalPosition, 0.01));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVShipClassTurnTest, "OrbitalVanguard.Ship.ClassTurnRates",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOVShipClassTurnTest::RunTest(const FString& Parameters)
{
    FVector EscortPosition = FVector::ZeroVector;
    FVector CruiserPosition = FVector::ZeroVector;
    FVector EscortVelocity = FVector::ZeroVector, CruiserVelocity = FVector::ZeroVector;
    FQuat EscortRotation = FQuat::Identity;
    FQuat CruiserRotation = FQuat::Identity;
    const FVector Target(0, 10000, 5000);
    // Same destination and elapsed simulation time, different class maneuverability.
    for (int32 Frame = 0; Frame < 30; ++Frame)
    {
        OVShipMotion::Advance(EscortPosition, EscortRotation, EscortVelocity, Target, 900, 80, 1.0f / 60.0f);
        OVShipMotion::Advance(CruiserPosition, CruiserRotation, CruiserVelocity, Target, 450, 25, 1.0f / 60.0f);
    }
    const double EscortAngle = FMath::RadiansToDegrees(FQuat::Identity.AngularDistance(EscortRotation));
    const double CruiserAngle = FMath::RadiansToDegrees(FQuat::Identity.AngularDistance(CruiserRotation));
    TestTrue(TEXT("Escort turns approximately forty degrees in half a second"), FMath::Abs(EscortAngle - 40.0) < 0.05);
    TestTrue(TEXT("Cruiser turns approximately twelve and a half degrees"), FMath::Abs(CruiserAngle - 12.5) < 0.05);
    TestTrue(TEXT("Light vessel aligns sooner than heavy vessel"), EscortAngle > CruiserAngle * 2.0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVShipTimeScaleTest, "OrbitalVanguard.Ship.TimeScaleAndStoppedArrival",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOVShipTimeScaleTest::RunTest(const FString& Parameters)
{
    FVector EndPositions[3];
    for (int32 Scale = 1; Scale <= 3; ++Scale)
    {
        FVector Position = FVector::ZeroVector;
    FVector Velocity = FVector::ZeroVector;
        FQuat Rotation = FQuat::Identity;
        // 3 real seconds at 1x, 1.5 at 2x, and 1 at 3x all mean 3 simulated seconds.
        const int32 RealFrames = 180 / Scale;
        for (int32 Frame = 0; Frame < RealFrames; ++Frame)
            OVShipMotion::Advance(Position, Rotation, Velocity, FVector(10000, 0, 0), 700, 60, Scale / 60.0f);
        EndPositions[Scale - 1] = Position;
        TestTrue(FString::Printf(TEXT("%dx covers the same accelerated distance"), Scale),
            Position.Equals(FVector(1955.88235294, 0, 0), 0.1));
    }
    TestTrue(TEXT("1x, 2x and 3x integrate the same simulated duration"),
        EndPositions[0].Equals(EndPositions[1], 0.1) && EndPositions[1].Equals(EndPositions[2], 0.1));
    FVector Position = FVector::ZeroVector;
    FVector Velocity = FVector::ZeroVector;
    FQuat Rotation = FQuat::Identity;
    const FVector NearbyTarget(20, 0, 0);
    const bool Arrived = OVShipMotion::Advance(Position, Rotation, Velocity, NearbyTarget, 700, 60, 0.5f);
    TestTrue(TEXT("Large final step clamps to destination"), Arrived && Position.Equals(NearbyTarget));
    for (int32 Frame = 0; Frame < 180; ++Frame)
        OVShipMotion::Advance(Position, Rotation, Velocity, NearbyTarget, 700, 60, 1.0f / 60.0f);
    TestTrue(TEXT("Ship stays stationary for three simulated seconds after arrival"), Position.Equals(NearbyTarget));
    return true;
}
#endif
