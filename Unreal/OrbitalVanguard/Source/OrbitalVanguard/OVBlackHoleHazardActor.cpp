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

    EventHorizonSphere = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EventHorizon"));
    EventHorizonSphere->SetupAttachment(Root);
    EventHorizonSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    EventHorizonSphere->SetCastShadow(false);
    // Preserve the named component for existing Blueprint references.
    // The original material draws the accretion disk and horizon in one pass.
    AccretionDiskMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AccretionDisk"));
    AccretionDiskMesh->SetupAttachment(Root);
    AccretionDiskMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    AccretionDiskMesh->SetVisibility(false);
}

void AOVBlackHoleHazardActor::BeginPlay()
{
    Super::BeginPlay();
    if (!bExternallyStepped)
        ConfigureVisualMesh(*EventHorizonSphere, SingularityRadius);
}

void AOVBlackHoleHazardActor::ConfigureVisualMesh(UStaticMeshComponent& Visual, float CoreRadius)
{
    UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr,TEXT("/Game/BlackHole/Mesh/SM_BlackHole_Sphere.SM_BlackHole_Sphere"),nullptr,LOAD_NoWarn);
    UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/BlackHole/Materials/MI_Blackhole_Physical.MI_Blackhole_Physical"),nullptr,LOAD_NoWarn);
    double Scale;
    if (Mesh && Material)
    {
        // The source uses a single inward-facing sphere. Its
        // shader traces a local event horizon of 2 * Mass (default mass = 1).
        Scale = CoreRadius / 2.0;
    }
    else
    {
        Mesh = LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Presentation/Environment/BlackHole/SM_Source.SM_Source"));
        Material = LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Presentation/Environment/BlackHole/M_Source.M_Source"));
        Scale = Mesh ? CoreRadius / FMath::Max(1.0,Mesh->GetBounds().BoxExtent.GetMax()) : 1.0;
    }
    Visual.SetStaticMesh(Mesh);
    Visual.SetMaterial(0,Material);
    // A fixed world inclination presents the disk obliquely in the fleet overview.
    Visual.SetRelativeRotation(FRotator(0,40,-66));
    Visual.SetRelativeScale3D(FVector(Scale * .8)); // Presentation size, independent of the E1 hazard radii.
    Visual.SetVisibleInRayTracing(false);
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
