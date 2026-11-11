#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OVShip.generated.h"

class UStaticMeshComponent;

/** Phase-one placeholder vessel. The controller owns the simulation clock. */
UCLASS()
class ORBITALVANGUARD_API AOVShip : public AActor
{
    GENERATED_BODY()
public:
    AOVShip();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ship")
    TObjectPtr<UStaticMeshComponent> Hull;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ship")
    FString ShipName = TEXT("Vessel");
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ship")
    bool bFriendly = true;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ship")
    bool bManual = true;
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

    void SetDestination(const FVector& Location);
    void Simulate(float SimDelta);
    void Configure(const FString& Name, bool Friendly, float Speed, float RotationRate,
        const FVector& Scale, const FLinearColor& Color);

private:
    UPROPERTY()
    TArray<TObjectPtr<UStaticMeshComponent>> AccentMeshes;
};
