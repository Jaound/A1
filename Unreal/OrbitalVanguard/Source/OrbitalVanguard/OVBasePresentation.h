#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OVBaseBuildState.h"
#include "OVBasePresentation.generated.h"

class AOVShip;
class UStaticMeshComponent;
class UStaticMesh;

enum class EOVBayPhase : uint8 { Building, Ready, Departing, Field, Returning, Entering, Berthed, Repairing, Refitting };
struct FOVBayOrder
{
    FGuid Id;
    FName Hull, Mount, Equipment;
    int32 Facility=0, Berth=0, ApproachLeg=0, RefitYard=0;
    EOVBayPhase Phase=EOVBayPhase::Building;
    float Time=0, Total=0, LateralCm=0;
    TWeakObjectPtr<AOVShip> Ship;
    TArray<FName> RemovedGear;
    FString Wait;
};
struct FOVResearchOrder { int32 Tech=0, Lab=0; float Time=0; };

UCLASS()
class AOVBasePresentation : public AActor
{
    GENERATED_BODY()

public:
    AOVBasePresentation();
    void InitializeModules();
    UFUNCTION(BlueprintCallable) bool OrderHull(int32 Index, FName Hull);
    UFUNCTION(BlueprintCallable) bool DockVessel(int32 Index, AOVShip* Ship);
    UFUNCTION(BlueprintCallable) bool LaunchVessel(FGuid Id);
    UFUNCTION(BlueprintCallable) bool ServiceVessel(FGuid Id, bool Refit);
    UFUNCTION(BlueprintCallable) bool RefitFacilities(FGuid Id);
    FString FacilityOffer(FGuid Id) const;
    int32 AvailableYard(FName Hull) const;
    UFUNCTION(BlueprintCallable) bool QueueResearch(int32 Tech);
    UFUNCTION(BlueprintCallable) bool PrepareOperationsTrial();
    UFUNCTION(BlueprintPure) FString OperationStatus() const {return OperationMessage;}
    UFUNCTION(BlueprintPure) FGuid VesselId(AOVShip* Ship) const;
    UFUNCTION(BlueprintPure) AOVShip* FindVessel(FGuid Id) const;
    UFUNCTION(BlueprintPure) TArray<FGuid> FacilityOrders(int32 Index) const;
    UFUNCTION(BlueprintPure) FString RefitOffer(FGuid Id) const;
    UFUNCTION(BlueprintPure) int32 VesselPhase(FGuid Id) const;
    FVector ClipTravel(AOVShip& Ship,FVector From,FVector To,const FQuat& Rotation) const;
    void AdvanceOperations(float Seconds);
    int32 ModuleIndex(int32 StableId) const;
    int32 UsedPoints(int32 Index) const;
    FString ShipOffer(FName Hull) const;
    FString ServiceQuote(const FOVBayOrder& Order) const;
    static OVBase::Resources ResearchPrice(int32 Tech);
    static float ResearchSeconds(int32 Tech);
    TArray<FOVBayOrder> Orders;
    TArray<FOVResearchOrder> Science;
    FString OperationMessage;
    bool Reject(const FString& Why) { OperationMessage=Why; return false; }
    bool InService(int32 Index) const;
    void BayPath(int32 Index,FVector& Inside,FVector& Outside, float LateralCm=0) const;
    bool FitBerth(int32 Index,FName Hull,float& Offset) const;
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
