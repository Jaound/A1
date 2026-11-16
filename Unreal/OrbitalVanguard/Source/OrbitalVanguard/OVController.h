#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "OVOrderInput.h"
#include "OVControlProfile.h"
#include "OVController.generated.h"
class AOVShip;
class ACameraActor;
class UOVCombatSubsystem;
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
 virtual bool InputKey(const FInputKeyEventArgs& Params) override;
 virtual void FlushPressedKeys() override;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera") bool bPlayGame3=true;
 UFUNCTION(BlueprintCallable, Category="Camera") void ApplyCameraInput(FVector2D OrbitDelta, FVector LocalPan, float YawAxis, int32 Wheel, float RealSeconds);
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
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat") TObjectPtr<UOVCombatSubsystem> Combat;
 /** Explicit test-range reset; never rewrites authored layout assets. */
 UFUNCTION(BlueprintCallable, Category="Combat") void StartCombatRange();
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat") int32 CalibrationScenario=-1;
 UFUNCTION(BlueprintCallable, Category="Combat") bool StartCalibrationRange(int32 Scenario=0);
 void ResolveContacts(float SimDelta);
 FOVOrderInput OrderInput;
private:
 float LegacyTraceDistance=0;
 double CommandXY() const;
 double CommandMinZ() const;
 double CommandMaxZ() const;
 double CommandRayLength() const;
 double AltitudeScale() const;
 bool ActionPressed(FName Name) const;
 TMap<int32,TArray<TWeakObjectPtr<AOVShip>>> Groups;
 bool bRightGesture=false;
 FVector2D RightStart=FVector2D::ZeroVector;
 TWeakObjectPtr<AOVShip> RightTarget;
 FOVOrbitGesture OrbitGesture;
 FOVRightMoveGesture RightMove;
 TWeakObjectPtr<AOVShip> RightCommander;
 bool bRightHeading=false;
 bool bConsumeAltLeft=false;
 void ResetCameraProfile();
 void CancelRightMove();
 void UpdateOrbit(FVector2D Cursor);
 void UpdateRightMove(FVector2D Cursor);
 UFUNCTION(BlueprintCallable, Category="Control Range") void HandleRightMove(bool Pressed, FVector2D Cursor, bool InWorld, bool Shift, bool Ctrl);
};
