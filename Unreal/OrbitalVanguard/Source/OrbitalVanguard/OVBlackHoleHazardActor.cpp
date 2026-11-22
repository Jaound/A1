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

    ApplyEnvironmentProfile(FOVSpaceEnvironmentProfile::MakeBlackHoleE1Profile());

    USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    // 1. Central Event Horizon Sphere (45 m = 4,500 cm radius; Engine/Fab unit sphere radius = 50 cm -> scale ~90)
    EventHorizonSphere = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EventHorizon"));
    EventHorizonSphere->SetupAttachment(Root);
    EventHorizonSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    EventHorizonSphere->SetCastShadow(false);
    EventHorizonSphere->SetRelativeScale3D(FVector(90.0f));

    // 2. Accretion Disk / Gravitational Lensing Inverted Mesh
    AccretionDiskMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AccretionDisk"));
    AccretionDiskMesh->SetupAttachment(Root);
    AccretionDiskMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    AccretionDiskMesh->SetCastShadow(false);
    AccretionDiskMesh->SetRelativeScale3D(FVector(180.0f));
}

void AOVBlackHoleHazardActor::BeginPlay()
{
    Super::BeginPlay();
    EnsureVisualMeshes();
}

void AOVBlackHoleHazardActor::EnsureVisualMeshes()
{
    if (bVisualsInitialized) return;
    bVisualsInitialized = true;

    // Safe null/fallback loading: works seamlessly even in clean clones where .gitignore'd /Game/BlackHole is absent.
    UStaticMesh* SphereMesh = LoadObject<UStaticMesh>(
        nullptr, TEXT("/Game/BlackHole/Mesh/SM_BlackHole_Sphere.SM_BlackHole_Sphere"), nullptr, LOAD_NoWarn | LOAD_Quiet);
    if (!SphereMesh)
    {
        SphereMesh = LoadObject<UStaticMesh>(
            nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"), nullptr, LOAD_NoWarn | LOAD_Quiet);
    }
    if (SphereMesh && EventHorizonSphere)
    {
        EventHorizonSphere->SetStaticMesh(SphereMesh);
    }

    UStaticMesh* DiskMesh = LoadObject<UStaticMesh>(
        nullptr, TEXT("/Game/BlackHole/Mesh/SM_InvertedSphereHQ.SM_InvertedSphereHQ"), nullptr, LOAD_NoWarn | LOAD_Quiet);
    if (DiskMesh && AccretionDiskMesh)
    {
        AccretionDiskMesh->SetStaticMesh(DiskMesh);
    }
    else if (AccretionDiskMesh)
    {
        AccretionDiskMesh->SetVisibility(false);
    }

    UMaterialInterface* DiskMat = LoadObject<UMaterialInterface>(
        nullptr, TEXT("/Game/BlackHole/Materials/MI_Blackhole_Physical.MI_Blackhole_Physical"), nullptr, LOAD_NoWarn | LOAD_Quiet);
    if (!DiskMat)
    {
        DiskMat = LoadObject<UMaterialInterface>(
            nullptr, TEXT("/Game/Presentation/Environment/BlackHole/M_Source.M_Source"), nullptr, LOAD_NoWarn | LOAD_Quiet);
    }
    if (DiskMat)
    {
        if (EventHorizonSphere) EventHorizonSphere->SetMaterial(0, DiskMat);
        if (AccretionDiskMesh && DiskMesh) AccretionDiskMesh->SetMaterial(0, DiskMat);
    }
}

void AOVBlackHoleHazardActor::ApplyEnvironmentProfile(const FOVSpaceEnvironmentProfile& InProfile)
{
    EnvironmentProfile = InProfile;
    GravityPullRadius = InProfile.InfluenceRadiusCm;
    SingularityRadius = InProfile.CoreHazardRadiusCm;
    GravitationalStrength = InProfile.MaxAccelerationCmPerSec2;
}

float AOVBlackHoleHazardActor::ComputeGravityAccelerationCm(float DistanceCm) const
{
    return FOVSpaceEnvironmentProfile::ComputeBlackHoleGravityAccelerationCm(
        DistanceCm, SingularityRadius, GravityPullRadius, GravitationalStrength);
}

void AOVBlackHoleHazardActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (bExternallyStepped) return;

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

    SimulateHazard(SimDelta);
}

void AOVBlackHoleHazardActor::SimulateHazard(float SimDelta)
{
    if (!FMath::IsFinite(SimDelta) || SimDelta <= 0.0f) return;

    UWorld* World = GetWorld();
    if (!World) return;

    // Spin the accretion disk
    AccretionYaw += DiskSpinRate * SimDelta;
    if (AccretionYaw >= 360.0f) AccretionYaw -= 360.0f;
    if (AccretionDiskMesh)
    {
        AccretionDiskMesh->SetRelativeRotation(FRotator(0.0f, AccretionYaw, 0.0f));
    }

    const FVector BlackHolePos = GetActorLocation();

    // Prune stale weak ship references
    for (auto It = ShipGravityVelocities.CreateIterator(); It; ++It)
    {
        if (!It.Key().IsValid())
        {
            It.RemoveCurrent();
        }
    }

    // Iterate over all active ships — E1 rule: FTL-charging ships STILL experience gravity displacement and core instant-wreck.
    for (TActorIterator<AOVShip> It(World); It; ++It)
    {
        AOVShip* Ship = *It;
        if (!IsValid(Ship) || Ship->IsHidden()) continue;
        if (Ship->Systems && Ship->Systems->bWrecked)
        {
            ShipGravityVelocities.Remove(Ship);
            continue;
        }

        const FVector ShipPos = Ship->GetActorLocation();
        const FVector ToCenter = BlackHolePos - ShipPos;
        const float Distance = static_cast<float>(ToCenter.Size());

        auto WreckInsideSingularity = [&]()
        {
            if (Ship->Systems && !Ship->Systems->bWrecked)
            {
                if (EnvironmentProfile.bInstantWreckInCore)
                {
                    Ship->Systems->ForceWreckInstant();
                    Ship->TriggerWreckDissolve();
                    ++CoreInstantWreckCount;
                }
                else
                {
                    const FVector Normal = Distance > KINDA_SMALL_NUMBER ? (-ToCenter / Distance) : FVector::UpVector;
                    Ship->Systems->ApplyWorldHit(ShipPos, Normal, 120.0f * SimDelta, EOVDamageCause::External, false);
                }
            }
            ShipGravityVelocities.Remove(Ship);
        };

        if (Distance <= SingularityRadius)
        {
            WreckInsideSingularity();
            continue;
        }

        if (Distance < GravityPullRadius)
        {
            const FVector PullDir = ToCenter / FMath::Max(Distance, KINDA_SMALL_NUMBER);
            const float Accel = ComputeGravityAccelerationCm(Distance);
            if (Accel > 0.0f)
            {
                FVector& PullVel = ShipGravityVelocities.FindOrAdd(Ship, FVector::ZeroVector);
                // Exact constant-acceleration kinematic displacement: dx = v0*dt + 0.5*a*dt^2, v1 = v0 + a*dt.
                // Works identically for moving, holding, and FTL-charging ships.
                const FVector Displacement = PullVel * SimDelta + PullDir * (0.5f * Accel * SimDelta * SimDelta);
                PullVel += PullDir * (Accel * SimDelta);
                Ship->AddActorWorldOffset(Displacement, false);

                const float PostDistance = static_cast<float>(FVector::Dist(BlackHolePos, Ship->GetActorLocation()));
                if (PostDistance <= SingularityRadius)
                {
                    WreckInsideSingularity();
                }
            }
        }
        else
        {
            // Outside gravity well: clear residual pull velocity
            ShipGravityVelocities.Remove(Ship);
        }
    }
}
