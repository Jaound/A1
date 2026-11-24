#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OVBaseBuildState.h"
#include "OVBasePresentation.generated.h"

class ACameraActor;
class UStaticMeshComponent;
class UStaticMesh;

UCLASS()
class AOVBasePresentation : public AActor
{
    GENERATED_BODY()

public:
    AOVBasePresentation();
    void Show(bool bShow);
    void UpdateView(float DeltaTime, float OrbitInput, float PitchInput, float ZoomInput);
    void RefreshMeshes();
    ACameraActor* View() const { return Camera; }
    FVector CellCenter(OVBase::Cell Cell) const;
    UFUNCTION(BlueprintCallable) bool ReserveModule(FName ModuleId, FIntVector Origin, int32 QuarterTurns);
    UFUNCTION(BlueprintCallable) bool CancelConstruction(int32 Index);
    UFUNCTION(BlueprintPure) int32 CommonStock() const { return Build.Funds(); }
    UFUNCTION(BlueprintPure) int32 ModuleCount() const { return static_cast<int32>(Build.Modules().size()); }
    UFUNCTION(BlueprintPure) float RemainingSeconds(int32 Index) const;
    OVBase::Error CheckPlacement(const OVBase::Module& Candidate) const;
    OVBase::BuildState Build;

protected:
    void EndPlay(const EEndPlayReason::Type Reason) override;

private:
    UPROPERTY() TObjectPtr<ACameraActor> Camera;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Meshes;
    UPROPERTY() TArray<TObjectPtr<UStaticMesh>> Assets;
    float Yaw = 35.f, Pitch = 50.f, Distance = 22500.f;
};
