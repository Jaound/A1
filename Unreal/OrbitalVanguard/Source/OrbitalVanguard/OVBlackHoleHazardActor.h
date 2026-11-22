#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OVSpaceEnvironmentTypes.h"
#include "OVBlackHoleHazardActor.generated.h"

class AOVShip;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;

/**
 * Environmental Space Hazard: E1-calibrated Black Hole with gravitational lensing accretion disk,
 * quadratic gravity attenuation over 1,000 m (100,000 cm), continuous displacement even while FTL-charging,
 * and immediate HP/Shield-ignoring hull destruction inside the 45 m (4,500 cm) singularity core.
 */
UCLASS()
class ORBITALVANGUARD_API AOVBlackHoleHazardActor : public AActor
{
    GENERATED_BODY()

public:
    AOVBlackHoleHazardActor();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    /** Applies a space-environment profile (defaults to E1 Black Hole profile) and synchronizes scalar fields. */
    UFUNCTION(BlueprintCallable, Category = "BlackHole")
    void ApplyEnvironmentProfile(const FOVSpaceEnvironmentProfile& InProfile);

    /** Returns E1 gravitational acceleration in cm/s^2 at DistanceCm from center. */
    UFUNCTION(BlueprintPure, Category = "BlackHole")
    float ComputeGravityAccelerationCm(float DistanceCm) const;

    /** Advances black hole rotation, gravitational pull kinematics, and 45 m singularity instant-wreck rules. */
    UFUNCTION(BlueprintCallable, Category = "BlackHole")
    void SimulateHazard(float SimDelta);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BlackHole")
    TObjectPtr<UStaticMeshComponent> EventHorizonSphere;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BlackHole")
    TObjectPtr<UStaticMeshComponent> AccretionDiskMesh;

    /** Shared E1 environment configuration (extensible across BlackHole/SolarFlare/Radiation/Asteroid). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlackHole")
    FOVSpaceEnvironmentProfile EnvironmentProfile;

    /** E1 gravity influence radius in cm (1,000 m = 100,000 cm). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlackHole")
    float GravityPullRadius = 100000.0f;

    /** E1 singularity core radius in cm (45 m = 4,500 cm). Entering causes immediate total wreck. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlackHole")
    float SingularityRadius = 4500.0f;

    /** E1 peak gravitational acceleration in cm/s^2 (0.8 m/s^2 = 80 cm/s^2). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlackHole")
    float GravitationalStrength = 80.0f;

    /** Rotational speed of the glowing accretion disk (deg/s). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlackHole")
    float DiskSpinRate = 8.0f;

    /** When true, an external subsystem (e.g. OVPlayScenarioSubsystem) drives SimulateHazard explicitly. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlackHole")
    bool bExternallyStepped = false;

    /** Diagnostic counter of vessels instantly wrecked inside the singularity core. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BlackHole")
    int32 CoreInstantWreckCount = 0;

private:
    void EnsureVisualMeshes();

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> AccretionDiskMID;

    TMap<TWeakObjectPtr<AOVShip>, FVector> ShipGravityVelocities;
    float AccretionYaw = 0.0f;
    bool bVisualsInitialized = false;
};
