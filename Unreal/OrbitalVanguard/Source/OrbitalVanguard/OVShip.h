#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OVWeaponVisualComponent.h"
#include "OVShip.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class UOVShipSystemsComponent;

/** Placeholder vessel. The controller supplies the single simulation clock. */
UCLASS()
class ORBITALVANGUARD_API AOVShip : public AActor
{
    GENERATED_BODY()
public:
    AOVShip();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ship")
    TObjectPtr<UStaticMeshComponent> Hull;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ship")
    TObjectPtr<UOVShipSystemsComponent> Systems;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ship")
    FString ShipName = TEXT("Vessel");
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ship")
    bool bFriendly = true;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ship")
    bool bManual = true;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Orders") bool bMovementAutomatic = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Orders") bool bAttackAutomatic = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ship")
    bool bSelected = false;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ship")
    float MoveSpeed = 700.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ship")
    float TurnRate = 60.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ship")
    FVector Destination = FVector::ZeroVector;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ship")
    bool bHasDestination = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Motion")
    FVector Velocity = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Motion", meta=(ClampMin="0"))
    float Acceleration = 1700.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Motion", meta=(ClampMin="1"))
    float Braking = 2200.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Motion", meta=(ClampMin="0.1"))
    float ArrivalRadius = 5.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Orders")
    bool bHold = true;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Orders", meta=(ScriptName="has_manual_attitude"))
    bool bManualAttitude = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Orders")
    FRotator ManualAttitude = FRotator::ZeroRotator;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Orders")
    bool bFTLCharging = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Orders")
    TObjectPtr<AOVShip> AttackTarget;

    /** Releases Hold but preserves an explicit manual attitude, including pitch/roll. */
    UFUNCTION(BlueprintCallable, Category="Orders") void SetDestination(const FVector& Location);
    /** Brakes to rest; automatic behavior must not replace this persistent Hold order. */
    UFUNCTION(BlueprintCallable, Category="Orders") void StopMovement();
    /** Sets a bounded-turn posture independently of destination and attack target. */
    UFUNCTION(BlueprintCallable, Category="Orders") void SetManualAttitude(const FRotator& Rotation);
    UFUNCTION(BlueprintCallable, Category="Orders") void SetAutomation(bool bMove, bool bAttack);
    UFUNCTION(BlueprintPure, Category="Orders") bool IsMovementAutomatic() const;
    UFUNCTION(BlueprintPure, Category="Orders") bool IsAttackAutomatic() const;
    /** Explicit test controls; no FTL travel or production repair behavior. */
    UFUNCTION(BlueprintCallable, Category="Debug") void DebugResetMotion();
    UFUNCTION(BlueprintCallable, Category="Debug") void DebugSetFTLCharging(bool bCharging);
    void Simulate(float SimDelta);
    void SetCalibrationHull(const FVector* LocalHalfExtent);
    void Configure(const FString& Name, bool Friendly, float Speed, float RotationRate,
        const FVector& Scale, const FLinearColor& Color);
    /** Read-only rendering synchronization, called once after a simulation batch, never a second simulation. */
    UFUNCTION(BlueprintCallable, Category="Presentation") void RefreshWeaponVisuals();
    UFUNCTION(BlueprintPure, Category="Presentation") TArray<FOVWeaponVisualState> GetWeaponVisualStates() const;
    UFUNCTION(BlueprintPure, Category="Presentation") FVector GetWeaponMuzzleWorld(FName MountId) const;
    void ClearWeaponVisuals();

private:
    // Detect legacy direct bManual writes while preserving the four D101 combinations.
    bool bLastManual = true;
    UPROPERTY()
    TArray<TObjectPtr<UStaticMeshComponent>> AccentMeshes;
    UPROPERTY() TObjectPtr<UBoxComponent> CalibrationSelection;
    UPROPERTY() TArray<TObjectPtr<UOVWeaponVisualComponent>> WeaponVisuals;
};
