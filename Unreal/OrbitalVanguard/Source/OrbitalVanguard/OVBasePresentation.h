#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OVBaseBuildState.h"
#include "OVBasePresentation.generated.h"

class ACameraActor;
class UStaticMeshComponent;

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
    OVBase::BuildState Build;

protected:
    void EndPlay(const EEndPlayReason::Type Reason) override;

private:
    UPROPERTY() TObjectPtr<ACameraActor> Camera;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Meshes;
    float Yaw = 35.f, Pitch = 50.f, Distance = 22500.f;
};
