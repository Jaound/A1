#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "OVCombatSubsystem.h"
#include "OVCombatVFXSubsystem.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;

USTRUCT()
struct FOVActiveAttackMesh
{
    GENERATED_BODY()
    UPROPERTY() int32 AttackId = 0;
    UPROPERTY() EOVAttackKind Kind = EOVAttackKind::Beam;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> MeshComponent = nullptr;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> DynMaterial = nullptr;
};

USTRUCT()
struct FOVTransientEffect
{
    GENERATED_BODY()
    UPROPERTY() TObjectPtr<UStaticMeshComponent> MeshComponent = nullptr;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> DynMaterial = nullptr;
    float Age = 0.0f;
    float Lifetime = 0.30f;
    FVector BaseScale = FVector(1.0f);
    FVector TargetScale = FVector(4.0f);
    bool bDistortion = false;
};

/**
 * World subsystem responsible for rendering laser beams, cannon shells,
 * expanding refractive distortion shockwaves, and sci-fi plasma impact bursts.
 * Strictly leaves EOVAttackKind::Missile untouched so existing missile assets render cleanly.
 */
UCLASS()
class ORBITALVANGUARD_API UOVCombatVFXSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    UFUNCTION(BlueprintCallable, Category = "Presentation")
    void SynchronizeVFX(float DeltaSeconds);

    UFUNCTION(BlueprintCallable, Category = "Presentation")
    void SpawnShockwaveDistortion(FVector Location, float Radius = 420.0f, bool bEnemyTarget = false);

    UFUNCTION(BlueprintCallable, Category = "Presentation")
    void SpawnImpactSlashSparks(FVector Location, FVector Normal, bool bShield, bool bEnemyTarget = false);

private:
    UPROPERTY() TObjectPtr<AActor> VFXRootActor;
    UPROPERTY() TArray<FOVActiveAttackMesh> ActiveProjectiles;
    UPROPERTY() TArray<FOVTransientEffect> TransientEffects;

    TSet<int32> ProcessedHitEvents;
    double LastCombatGameTime = -1.0;

    UPROPERTY() TObjectPtr<UStaticMesh> CannonShellMesh;
    UPROPERTY() TObjectPtr<UStaticMesh> LaserBeamMesh;
    UPROPERTY() TObjectPtr<UStaticMesh> ImpactBurstMesh;
    UPROPERTY() TObjectPtr<UStaticMesh> ShockwaveSphereMesh;

    UPROPERTY() TObjectPtr<UMaterialInterface> FriendlyBeamMat;
    UPROPERTY() TObjectPtr<UMaterialInterface> EnemyBeamMat;
    UPROPERTY() TObjectPtr<UMaterialInterface> CannonShellMat;
    UPROPERTY() TObjectPtr<UMaterialInterface> EffectGlowMat;
    UPROPERTY() TObjectPtr<UMaterialInterface> DistortionMat;
    UPROPERTY() TObjectPtr<UMaterialInterface> ShieldImpactMat;
    UPROPERTY() TObjectPtr<UMaterialInterface> HullImpactMat;

    void EnsureAssetsLoaded();
    void EnsureRootActor();
};
