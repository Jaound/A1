#include "OVBlackHoleHazardActor.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "OVController.h"
#include "OVShip.h"
#include "OVShipSystemsComponent.h"

AOVBlackHoleHazardActor::AOVBlackHoleHazardActor()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.033f; // 30 Hz hazard update

    USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    // 1. Central Event Horizon Sphere (Deep black singularity sphere)
    EventHorizonSphere = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EventHorizon"));
    EventHorizonSphere->SetupAttachment(Root);
    EventHorizonSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    EventHorizonSphere->SetCastShadow(true);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMeshFinder(
        TEXT("/Game/BlackHole/Mesh/SM_BlackHole_Sphere.SM_BlackHole_Sphere"));
    if (SphereMeshFinder.Succeeded())
    {
        EventHorizonSphere->SetStaticMesh(SphereMeshFinder.Object);
    }
    EventHorizonSphere->SetRelativeScale3D(FVector(14.0f));

    // 2. Accretion Disk / Gravitational Lensing Inverted Mesh
    AccretionDiskMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AccretionDisk"));
    AccretionDiskMesh->SetupAttachment(Root);
    AccretionDiskMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    AccretionDiskMesh->SetCastShadow(false);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> DiskMeshFinder(
        TEXT("/Game/BlackHole/Mesh/SM_InvertedSphereHQ.SM_InvertedSphereHQ"));
    if (DiskMeshFinder.Succeeded())
    {
        AccretionDiskMesh->SetStaticMesh(DiskMeshFinder.Object);
    }
    AccretionDiskMesh->SetRelativeScale3D(FVector(28.0f));

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> DiskMatFinder(
        TEXT("/Game/BlackHole/Materials/MI_Blackhole_Physical.MI_Blackhole_Physical"));
    if (DiskMatFinder.Succeeded())
    {
        EventHorizonSphere->SetMaterial(0, DiskMatFinder.Object);
        AccretionDiskMesh->SetMaterial(0, DiskMatFinder.Object);
    }
}

void AOVBlackHoleHazardActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    UWorld* World = GetWorld();
    if (!World) return;

    // Respect single simulation clock pause and speed multiplier
    float SimSpeed = 1.0f;
    if (AOVController* OVC = Cast<AOVController>(UGameplayStatics::GetPlayerController(World, 0)))
    {
        if (OVC->bSimulationPaused)
        {
            return;
        }
        SimSpeed = FMath::Max(0.0f, float(OVC->SimulationSpeed));
    }

    const float SimDelta = FMath::Clamp(DeltaSeconds * SimSpeed, 0.0f, 0.25f);
    if (SimDelta <= 0.0f) return;

    // Spin the accretion disk
    AccretionYaw += DiskSpinRate * SimDelta;
    if (AccretionYaw >= 360.0f) AccretionYaw -= 360.0f;
    if (AccretionDiskMesh)
    {
        AccretionDiskMesh->SetRelativeRotation(FRotator(0.0f, AccretionYaw, 0.0f));
    }

    const FVector BlackHolePos = GetActorLocation();

    // Iterate over all active ships and apply localized gravitational pull & tidal shear
    for (TActorIterator<AOVShip> It(World); It; ++It)
    {
        AOVShip* Ship = *It;
        if (!IsValid(Ship) || Ship->IsHidden() || Ship->bFTLCharging) continue;

        const FVector ShipPos = Ship->GetActorLocation();
        const float Distance = FVector::Dist(BlackHolePos, ShipPos);

        if (Distance < GravityPullRadius && Distance > 10.0f)
        {
            const FVector PullDir = (BlackHolePos - ShipPos) / Distance;
            const float Falloff = FMath::Clamp(1.0f - (Distance / GravityPullRadius), 0.0f, 1.0f);
            const float PullSpeed = GravitationalStrength * Falloff * Falloff;

            Ship->AddActorWorldOffset(PullDir * PullSpeed * SimDelta, false);

            // If entering the inner singularity event horizon, apply gravitational shear damage
            if (Distance < SingularityRadius)
            {
                if (Ship->Systems && !Ship->Systems->bWrecked)
                {
                    Ship->Systems->ApplyWorldHit(ShipPos, -PullDir, 120.0f * SimDelta, EOVDamageCause::External, false);
                    Ship->TriggerShieldImpact(ShipPos, -PullDir, 40.0f);
                }
            }
        }
    }
}
