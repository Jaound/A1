#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "OVOrderInput.h"
#include "OVController.generated.h"
class AOVShip;
class ACameraActor;
UENUM(BlueprintType)
enum class EOVInputMode : uint8 { None, Move, Target, Heading, Repair };
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
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Control Range") EOVInputMode InputMode=EOVInputMode::None;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Control Range") TArray<TObjectPtr<AOVShip>> Selection;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Control Range") bool bFollow=false;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Debug") bool bDeveloperMode=true;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Debug") int32 ViewedDeck=0;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Debug") int32 SelectedSlot=0;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Debug") int32 SelectedArmor=0;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Debug") int32 SelectedWeapon=0;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Debug") float TestDamage=150;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input") float AltitudeUnitsPerPixel=10;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input") float ProjectionNearDistance=350;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Control Range") int32 PairResolutionCount=0;
 double PlacementPlaneZ=0;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Control Range") FVector Preview=FVector::ZeroVector;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Control Range") FVector CameraFocus=FVector::ZeroVector;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Control Range") float CameraYaw=-45;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Control Range") float CameraPitch=-48;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Control Range") float CameraDistance=6500;
 FString LastAction=TEXT("Select a ship to begin");
 UFUNCTION(BlueprintCallable, Category="Control Range")
 void SelectShip(AOVShip* Ship);
 UFUNCTION(BlueprintCallable, Category="Control Range") void ClearSelection();
 UFUNCTION(BlueprintCallable, Category="Control Range") void SelectAdditional(AOVShip* Ship);
 UFUNCTION(BlueprintCallable, Category="Control Range") void SetTarget(AOVShip* Ship);
 UFUNCTION(BlueprintCallable, Category="Control Range") void SampleOrderCursor(FVector RayOrigin,FVector RayDirection,float MouseY,bool Altitude,bool Blocked);
 UFUNCTION(BlueprintCallable, Category="Control Range") void IssueHeading(FVector WorldPoint);
 UFUNCTION(BlueprintCallable, Category="Debug") void TestArmorHit();
 UFUNCTION(BlueprintCallable, Category="Control Range")
 void ConfirmMove();
 UFUNCTION(BlueprintCallable, Category="Control Range")
 void UpdateCamera();
 void UpdatePreview();
 UFUNCTION(BlueprintCallable, Category="Control Range")
 void StepSimulation(float DeltaTime);
 FOVOrderInput OrderInput;
private:
 bool ActionPressed(FName Name) const;
 void ResolveContacts(float SimDelta);
 TMap<int32,TArray<TWeakObjectPtr<AOVShip>>> Groups;
 bool bRightGesture=false;
 FVector2D RightStart=FVector2D::ZeroVector;
 TWeakObjectPtr<AOVShip> RightTarget;
 bool bWasOverUI=false;
};
