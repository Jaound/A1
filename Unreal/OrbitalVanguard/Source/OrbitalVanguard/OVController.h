#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "OVController.generated.h"
class AOVShip;
class ACameraActor;
UCLASS()
class ORBITALVANGUARD_API AOVController : public APlayerController
{
 GENERATED_BODY()
public:
 AOVController();
 virtual void BeginPlay() override;
 virtual void PlayerTick(float DeltaTime) override;
 UFUNCTION(BlueprintCallable, Category="Control Range")
 void Action(FName Name);
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Control Range") TArray<TObjectPtr<AOVShip>> Ships;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Control Range") TObjectPtr<AOVShip> Selected;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Control Range") TObjectPtr<ACameraActor> RangeCamera;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Control Range") bool bSimulationPaused=false;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Control Range") float SimulationSpeed=1;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Control Range") double SimulationSeconds=0;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Control Range") bool bPlacing=false;
 double PlacementPlaneZ=0;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Control Range") FVector Preview=FVector::ZeroVector;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Control Range") FVector CameraFocus=FVector::ZeroVector;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Control Range") float CameraYaw=-45;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Control Range") float CameraPitch=-48;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Control Range") float CameraDistance=6500;
 FString LastAction=TEXT("Select a ship to begin");
 UFUNCTION(BlueprintCallable, Category="Control Range")
 void SelectShip(AOVShip* Ship);
 UFUNCTION(BlueprintCallable, Category="Control Range")
 void ConfirmMove();
 UFUNCTION(BlueprintCallable, Category="Control Range")
 void UpdateCamera();
 void UpdatePreview();
 UFUNCTION(BlueprintCallable, Category="Control Range")
 void StepSimulation(float DeltaTime);
};
