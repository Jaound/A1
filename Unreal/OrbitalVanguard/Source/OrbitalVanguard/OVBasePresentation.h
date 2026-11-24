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
    UFUNCTION(BlueprintCallable) bool DemolishModule(int32 Index);
    UFUNCTION(BlueprintCallable) bool UpgradeModule(int32 Index);
    UFUNCTION(BlueprintCallable) bool SaveTrial(FString& Message) const;
    UFUNCTION(BlueprintCallable) bool LoadTrial(FString& Message);
    UFUNCTION(BlueprintCallable) bool PrepareTrialTechnology(int32 TechnologyId);
    UFUNCTION(BlueprintCallable) void SetTrialAccounting(bool bCharge, bool bFill);
    UFUNCTION(BlueprintPure) int32 ModuleLevel(int32 Index) const;
    UFUNCTION(BlueprintPure) bool IsModuleConnected(int32 Index) const;
    UFUNCTION(BlueprintPure) int32 AdvancedCapacity() const { return Build.Capacity().Advanced; }
    UFUNCTION(BlueprintPure) int32 CommonStock() const { return Build.Funds(); }
    UFUNCTION(BlueprintPure) int32 ModuleCount() const { return static_cast<int32>(Build.Modules().size()); }
    UFUNCTION(BlueprintPure) float RemainingSeconds(int32 Index) const;
    OVBase::Error CheckPlacement(const OVBase::Module& Candidate, FString* BlockingName = nullptr) const;
    OVBase::Error CheckPlan(const std::vector<OVBase::Module>& Plan, OVBase::BuildState* Result = nullptr, FString* BlockingName = nullptr) const;
    std::vector<OVBase::Module> PlanConnection(const OVBase::Module& Candidate) const;
    bool ReservePlan(const std::vector<OVBase::Module>& Plan);
    bool HasOriginalModel(OVBase::Kind Type) const
    {
        const auto Index=static_cast<int32>(Type);
        return OriginalModels.IsValidIndex(Index) && OriginalModels[Index];
    }
    OVBase::BuildState Build{false}; // 건설 시험장 전용. 일반 BuildState는 비용을 청구한다.

private:
    FTransform PlacementTransform(const OVBase::Module& Item, const OVBase::BuildState& State,
        const TArray<FTransform>& Transforms, bool& bSeam) const;
    void UpdateConstructionVisuals();
    const AOVShip* BlockingShip(const OVBase::Module& Module, const FTransform* Snapped) const;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Meshes;
    UPROPERTY() TArray<TObjectPtr<UStaticMesh>> Assets;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> PortSleeves;
    UPROPERTY() TArray<TObjectPtr<UStaticMesh>> SleeveAssets;
    TArray<bool> OriginalModels;
    mutable TOptional<OVBase::Module> RouteTarget;
    mutable std::vector<OVBase::Module> Route;

};
