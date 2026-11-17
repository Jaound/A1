#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "OVShipLayout.h"
#include "OVWeaponVisualComponent.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;

/** Shared authored geometry: gameplay and rendering use the same muzzle, in ship-local cm. */
namespace OVWeaponGeometry
{
    ORBITALVANGUARD_API float BarrelLength(const FVector& HullHalfExtent, float Tier, bool bPointDefense);
    ORBITALVANGUARD_API FVector MuzzleLocal(FVector MountPosition, FRotator Aim,
        float BarrelLength, bool bFixedVLS);
    ORBITALVANGUARD_API bool HasBF120Trial(const FOVWeaponMountDefinition& Definition);
    ORBITALVANGUARD_API FVector MuzzleLocal(const FOVWeaponMountDefinition& Definition, FRotator Aim,
        FVector HullHalfExtent, bool bFixedVLS, int32 BarrelIndex);
}

USTRUCT(BlueprintType)
struct ORBITALVANGUARD_API FOVWeaponVisualState
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName MountId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FRotator LogicalAim = FRotator::ZeroRotator;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FRotator VisualAim = FRotator::ZeroRotator;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FVector LocalMuzzle = FVector::ZeroVector;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FVector WorldMuzzle = FVector::ZeroVector;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bFixedVLS = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bPointDefense = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bBottom = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bDestroyed = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bFlashVisible = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 LastShotId = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) double LastShotTime = -1;
};

/** Display-only components. No independent Tick, targets, combat actors, damage or collision. */
UCLASS()
class ORBITALVANGUARD_API UOVWeaponVisualComponent : public USceneComponent
{
    GENERATED_BODY()
public:
    UOVWeaponVisualComponent();
    void Initialize(const FOVWeaponMountDefinition& Definition, FVector HullHalfExtent);
    void Synchronize(FRotator LogicalAim, bool bFixedVLS, bool bDestroyed, bool bFriendly,
        int32 ShotId, double ShotTime, double GameTime, int32 ShotsFired = 0);
    FOVWeaponVisualState ReadState() const;
    FName GetMountId() const { return Definition.WeaponId; }
    bool Matches(const FOVWeaponMountDefinition& Other, FVector HullHalfExtent) const;
    virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;
private:
    FOVWeaponMountDefinition Definition;
    FVector Extent;
    float Length = 0;
    FOVWeaponVisualState State;
    bool bLastDestroyed = false;
    bool bLastFriendly = true;
    bool bColorInitialized = false;
    UPROPERTY() TObjectPtr<USceneComponent> YawPivot;
    UPROPERTY() TObjectPtr<USceneComponent> PitchPivot;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Barrel;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Flash;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> SecondFlash;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> BodyMaterial;
    UPROPERTY() TArray<TObjectPtr<USceneComponent>> Parts;
    USceneComponent* MakePivot(USceneComponent* Parent);
    UStaticMeshComponent* MakeMesh(USceneComponent* Parent, const TCHAR* Asset,
        FVector Position, FVector Scale, FRotator Rotation = FRotator::ZeroRotator, bool bUseBodyMaterial = true);
};
