#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OVBaseBuildState.h"
#include "OVBasePresentation.generated.h"

class AOVShip;
class UStaticMeshComponent;
class UStaticMesh;

UCLASS()
class AOVBasePresentation : public AActor
{
    GENERATED_BODY()

public:
    AOVBasePresentation();
    void InitializeModules();
    void RefreshMeshes();
    void AdvanceConstruction(float GameSeconds);
    const AOVShip* ConstructionBlocker(int32 Index) const;
    FVector CellCenter(OVBase::Cell Cell) const;
    UFUNCTION(BlueprintCallable) bool ReserveModule(FName ModuleId, FIntVector Origin, int32 QuarterTurns);
    UFUNCTION(BlueprintCallable) bool CancelConstruction(int32 Index);
    UFUNCTION(BlueprintPure) int32 CommonStock() const { return Build.Funds(); }
    UFUNCTION(BlueprintPure) int32 ModuleCount() const { return static_cast<int32>(Build.Modules().size()); }
    UFUNCTION(BlueprintPure) float RemainingSeconds(int32 Index) const;
    OVBase::Error CheckPlacement(const OVBase::Module& Candidate, FString* BlockingName = nullptr) const;
    OVBase::BuildState Build{false}; // 건설 시험장 전용. 일반 BuildState는 비용을 청구한다.

private:
    void UpdateConstructionVisuals();
    const AOVShip* BlockingShip(const OVBase::Module& Module, const FTransform* Snapped) const;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Meshes;
    UPROPERTY() TArray<TObjectPtr<UStaticMesh>> Assets;

};
