#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OVBlackHoleHazardActor.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;

/**
 * Environmental Space Hazard: Black Hole with gravitational lensing accretion disk,
 * tidal pull physics on vessels (respecting pause & simulation speed), and singularity event horizon shear.
 */
UCLASS()
class ORBITALVANGUARD_API AOVBlackHoleHazardActor : public AActor
{
    GENERATED_BODY()

public:
    AOVBlackHoleHazardActor();

    virtual void Tick(float DeltaSeconds) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BlackHole")
    TObjectPtr<UStaticMeshComponent> EventHorizonSphere;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BlackHole")
    TObjectPtr<UStaticMeshComponent> AccretionDiskMesh;

    /** Tactical radius within which nearby ships feel gravitational pull (in cm). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlackHole")
    float GravityPullRadius = 3200.0f;

    /** Inner horizon radius where tidal shear crushes vessels. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlackHole")
    float SingularityRadius = 550.0f;

    /** Base gravitational acceleration constant. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlackHole")
    float GravitationalStrength = 220.0f;

    /** Rotational speed of the glowing accretion disk. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlackHole")
    float DiskSpinRate = 8.0f;

private:
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> AccretionDiskMID;
    float AccretionYaw = 0.0f;
};
