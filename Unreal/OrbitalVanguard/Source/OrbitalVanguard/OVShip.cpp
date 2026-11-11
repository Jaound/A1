#include "OVShip.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#endif

namespace OVShipMotion
{
    // Small substeps make turn-limited movement stable at all three simulation rates.
    static bool Advance(FVector& Position, FQuat& Orientation, const FVector& Destination,
        float Speed, float TurnRate, float Delta)
    {
        if (Delta <= 0.0f) return false;
        const FVector Offset = Destination - Position;
        const double Distance = Offset.Size();
        if (Distance <= 5.0)
        {
            Position = Destination;
            return true;
        }
        const FVector Direction = Offset / Distance;
        const FQuat Desired = Direction.ToOrientationQuat();
        const double AngularDistance = Orientation.AngularDistance(Desired);
        const double MaxTurn = FMath::DegreesToRadians(FMath::Max(0.0f, TurnRate)) * Delta;
        const double Alpha = AngularDistance > UE_SMALL_NUMBER ? FMath::Min(1.0, MaxTurn / AngularDistance) : 1.0;
        Orientation = FQuat::Slerp(Orientation, Desired, Alpha).GetNormalized();
        const double Alignment = FVector::DotProduct(Orientation.GetForwardVector(), Direction);
        // Engines wait for the bow to face the destination, then ease into translation.
        const double Throttle = FMath::Clamp((Alignment - 0.6) / 0.4, 0.0, 1.0);
        const double Travel = FMath::Min(Distance, FMath::Max(0.0f, Speed) * Delta * Throttle);
        Position += Direction * Travel;
        if (Travel >= Distance || FVector::DistSquared(Position, Destination) <= 25.0)
        {
            Position = Destination;
            return true;
        }
        return false;
    }
}

AOVShip::AOVShip()
{
    PrimaryActorTick.bCanEverTick = false;
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
    if (!Location.ContainsNaN())
    {
        Destination = Location;
        bHasDestination = true;
    }
}

void AOVShip::Simulate(float SimDelta)
{
    if (!bHasDestination || !FMath::IsFinite(SimDelta) || SimDelta <= 0.0f) return;
    FVector Position = GetActorLocation();
    FQuat Orientation = GetActorQuat();
    float Remaining = SimDelta;
    while (Remaining > UE_SMALL_NUMBER && bHasDestination)
    {
        const float Step = FMath::Min(Remaining, 1.0f / 30.0f);
        if (OVShipMotion::Advance(Position, Orientation, Destination, MoveSpeed, TurnRate, Step)) bHasDestination = false;
        Remaining -= Step;
    }
    SetActorLocationAndRotation(Position, Orientation, false, nullptr, ETeleportType::TeleportPhysics);
}

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVShipMotionTest, "OrbitalVanguard.Ship.FiniteTurnXYZArrival",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOVShipMotionTest::RunTest(const FString& Parameters)
{
    FVector Position = FVector::ZeroVector;
    FQuat Rotation = FQuat::Identity;
    const FVector Target(0, 1000, 400);
    OVShipMotion::Advance(Position, Rotation, Target, 700, 30, 0.1f);
    TestTrue(TEXT("Turning is limited to three degrees in 0.1 seconds"),
        FQuat::Identity.AngularDistance(Rotation) <= FMath::DegreesToRadians(3.01));
    TestTrue(TEXT("Ship waits while facing away"), Position.IsNearlyZero());
    const FVector PausedPosition = Position;
    const FQuat PausedRotation = Rotation;
    OVShipMotion::Advance(Position, Rotation, Target, 700, 30, 0.0f);
    TestTrue(TEXT("Zero simulation time preserves position and heading"),
        Position.Equals(PausedPosition) && Rotation.Equals(PausedRotation));
    bool Arrived = false;
    for (int32 Index = 0; Index < 1800 && !Arrived; ++Index)
        Arrived = OVShipMotion::Advance(Position, Rotation, Target, 700, 30, 1.0f / 60.0f);
    TestTrue(TEXT("Ship reaches elevated XYZ destination"), Arrived && Position.Equals(Target, 0.01));
    const FVector FinalPosition = Position;
    OVShipMotion::Advance(Position, Rotation, Target, 700, 30, 0.1f);
    TestTrue(TEXT("Arrival does not overshoot"), Position.Equals(FinalPosition, 0.01));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVShipClassTurnTest, "OrbitalVanguard.Ship.ClassTurnRates",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOVShipClassTurnTest::RunTest(const FString& Parameters)
{
    FVector EscortPosition = FVector::ZeroVector;
    FVector CruiserPosition = FVector::ZeroVector;
    FQuat EscortRotation = FQuat::Identity;
    FQuat CruiserRotation = FQuat::Identity;
    const FVector Target(0, 10000, 5000);
    // Same destination and elapsed simulation time, different class maneuverability.
    for (int32 Frame = 0; Frame < 30; ++Frame)
    {
        OVShipMotion::Advance(EscortPosition, EscortRotation, Target, 900, 80, 1.0f / 60.0f);
        OVShipMotion::Advance(CruiserPosition, CruiserRotation, Target, 450, 25, 1.0f / 60.0f);
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
        FQuat Rotation = FQuat::Identity;
        // 3 real seconds at 1x, 1.5 at 2x, and 1 at 3x all mean 3 simulated seconds.
        const int32 RealFrames = 180 / Scale;
        for (int32 Frame = 0; Frame < RealFrames; ++Frame)
            OVShipMotion::Advance(Position, Rotation, FVector(10000, 0, 0), 700, 60, Scale / 60.0f);
        EndPositions[Scale - 1] = Position;
        TestTrue(FString::Printf(TEXT("%dx covers the same 2100-unit distance"), Scale),
            Position.Equals(FVector(2100, 0, 0), 0.1));
    }
    TestTrue(TEXT("1x, 2x and 3x integrate the same simulated duration"),
        EndPositions[0].Equals(EndPositions[1], 0.1) && EndPositions[1].Equals(EndPositions[2], 0.1));
    FVector Position = FVector::ZeroVector;
    FQuat Rotation = FQuat::Identity;
    const FVector NearbyTarget(20, 0, 0);
    const bool Arrived = OVShipMotion::Advance(Position, Rotation, NearbyTarget, 700, 60, 0.1f);
    TestTrue(TEXT("Large final step clamps to destination"), Arrived && Position.Equals(NearbyTarget));
    for (int32 Frame = 0; Frame < 180; ++Frame)
        OVShipMotion::Advance(Position, Rotation, NearbyTarget, 700, 60, 1.0f / 60.0f);
    TestTrue(TEXT("Ship stays stationary for three simulated seconds after arrival"), Position.Equals(NearbyTarget));
    return true;
}
#endif
