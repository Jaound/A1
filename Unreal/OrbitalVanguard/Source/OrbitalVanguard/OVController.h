#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "OVOrderInput.h"
#include "OVControlProfile.h"
#include "OVController.generated.h"
class AOVShip;
class ACameraActor;
class UOVCombatSubsystem;
class UOVCarrierSubsystem;
class UOVPointDefenseSubsystem;
class UOVTransportSubsystem;
class UOVInteriorSubsystem;
class UOVMissionSubsystem;
class UOVPlayScenarioSubsystem;
class UOVDevelopmentSubsystem;
class UOVHeroSubsystem;
class UInstancedStaticMeshComponent;
class UStaticMeshComponent;
UENUM(BlueprintType)
enum class EOVInputMode : uint8 { None, Move, Target, Heading, Repair, Survey, BoardingTarget, DockingTarget };
UENUM(BlueprintType)
enum class EOVSpaceEnvironmentType : uint8 { None, AsteroidField, RadiationZone, SolarFlare, BlackHole };
UENUM(BlueprintType)
enum class EOVOutpostKind : uint8 { None, Basic, MiningHub, CombatBase };
USTRUCT(BlueprintType)
struct FOVStarSystemNode
{
 GENERATED_BODY()
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName SystemId;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FString DisplayName;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FString OwnerFaction=TEXT("neutral");
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FString EnemyFactionName;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FVector2D MapNormalizedPos=FVector2D(0.5f,0.5f);
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly) EOVSpaceEnvironmentType Environment=EOVSpaceEnvironmentType::None;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly) EOVOutpostKind Outpost=EOVOutpostKind::None;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 OutpostHP=0;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 OutpostMaxHP=0;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 CommonYield=10;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 AdvancedYield=4;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 SupremeYield=0;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 DangerLevel=1;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FLinearColor NebulaTint=FLinearColor::White;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<FName> Links;
};
struct FOVDeathThroeState
{
 bool bActive=false;
 bool bVanished=false;
 bool bHasBaseTransform=false;
 float TotalDuration=3.1f;
 float Remaining=0.f;
 float BurstAccum=0.f;
 float RollDirection=1.f;
 FVector BaseRelLocation=FVector::ZeroVector;
 FRotator BaseRelRotation=FRotator::ZeroRotator;
 TArray<TPair<FVector,double>> ExtraBursts;
};
UCLASS()
class ORBITALVANGUARD_API AOVController : public APlayerController
{
 GENERATED_BODY()
public:
 AOVController();
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UOVDevelopmentSubsystem> Development;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UOVHeroSubsystem> Heroes;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Interior") TObjectPtr<UOVInteriorSubsystem> Interior;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mission") TObjectPtr<UOVMissionSubsystem> Mission;
 virtual void BeginPlay() override;
 virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
 virtual void PlayerTick(float DeltaTime) override;
 virtual bool InputKey(const FInputKeyEventArgs& Params) override;
 virtual void FlushPressedKeys() override;
 // Refreshed alongside the flight guides, in game seconds from the current frame.
 TMap<TWeakObjectPtr<AOVShip>,double> SelectedMissileApproach;
 /** Render-only copies of live aircraft; gameplay remains in the carrier subsystem. */
 UFUNCTION(BlueprintCallable, Category="Presentation") void UpdateAircraftPresentation();
 UFUNCTION(BlueprintCallable, Category="Presentation") void UpdateCombatPresentation();
 UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Presentation") TObjectPtr<UInstancedStaticMeshComponent> FriendlyAircraftMeshes;
 UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Presentation") TObjectPtr<UInstancedStaticMeshComponent> EnemyAircraftMeshes;
 UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Presentation") TObjectPtr<UInstancedStaticMeshComponent> StandardMissileMeshes;
 UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Presentation") TObjectPtr<UInstancedStaticMeshComponent> AntiShipMissileMeshes;
 UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Presentation") TObjectPtr<UInstancedStaticMeshComponent> CannonProjectileMeshes;
 UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Presentation") TObjectPtr<UInstancedStaticMeshComponent> FriendlyBeamMeshes;
 UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Presentation") TObjectPtr<UInstancedStaticMeshComponent> EnemyBeamMeshes;
 UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Presentation") TObjectPtr<UInstancedStaticMeshComponent> MissilePlumeMeshes;
 UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Presentation") TObjectPtr<UInstancedStaticMeshComponent> ShieldImpactMeshes;
 UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Presentation") TObjectPtr<UInstancedStaticMeshComponent> HullImpactMeshes;
 UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Presentation") TObjectPtr<UStaticMeshComponent> SpaceSkyboxMesh;
 UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Presentation") TObjectPtr<UStaticMeshComponent> DistantPlanetMesh;
 UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Presentation") TObjectPtr<UInstancedStaticMeshComponent> StarfieldMeshes;
 UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Presentation") TObjectPtr<UInstancedStaticMeshComponent> AsteroidMeshes;
 UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Presentation") TObjectPtr<UStaticMeshComponent> BlackHoleMeshes;
 UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Presentation") TObjectPtr<UInstancedStaticMeshComponent> RadiationZoneMeshes;
 UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Presentation") TObjectPtr<UInstancedStaticMeshComponent> SolarFlareMeshes;
 UPROPERTY(Transient) TMap<TWeakObjectPtr<AOVShip>, TObjectPtr<UStaticMeshComponent>> ShieldBubbleMeshes;
 TMap<TWeakObjectPtr<AOVShip>, FOVDeathThroeState> DeathThroes;
 TArray<FTransform> CachedStarTransforms;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Campaign") TArray<FOVStarSystemNode> StarSystems;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Campaign") FName CurrentStarSystemId=TEXT("home_vanguard");
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Campaign") int32 ResourceCommon=320;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Campaign") int32 ResourceAdvanced=140;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Campaign") int32 ResourceSupreme=35;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Environment") EOVSpaceEnvironmentType ActiveEnvironment=EOVSpaceEnvironmentType::None;
 TArray<FVector4> ActiveAsteroids;
 TArray<FVector4> ActiveRadiationZones;
 FVector ActiveBlackHoleCenter=FVector(0,0,250);
 float ActiveBlackHoleRadius=2600.f;
 float EnvironmentTickAccum=0.f;
 float SolarFlareCycleTime=0.f;
 bool bSolarFlareWaveApplied=false;
 float CampaignMiningAccum=0.f;
 void InitializeCampaignSystems();
 UFUNCTION(BlueprintCallable, Category="Environment") void SetSpaceEnvironment(EOVSpaceEnvironmentType NewEnvironment);
 UFUNCTION(BlueprintCallable, Category="Campaign") bool SwitchStarSystem(FName SystemId);
 UFUNCTION(BlueprintCallable, Category="Campaign") bool DispatchFleetFTL(FName TargetSystemId);
 UFUNCTION(BlueprintCallable, Category="Campaign") bool BuildOutpost(FName SystemId, EOVOutpostKind Kind);
 bool IsShipInDeathThroe(const AOVShip* Ship, float& OutRemaining) const;
 bool IsShipVanished(const AOVShip* Ship) const;
 bool IsShipShelteredFromSolarFlare(const AOVShip* Ship) const;
 void ResetDeathThroes();
 static FString GetEnvironmentDisplayName(EOVSpaceEnvironmentType Env);
 static FString GetOutpostDisplayName(EOVOutpostKind Kind);
 UPROPERTY(Transient) TObjectPtr<AActor> AircraftPresentationActor;
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
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Debug") bool bDeveloperMode=false;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Debug") bool bShowDebugOutlines=false;
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
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Control Range") FString LastAction=TEXT("Select a ship to begin");
 UFUNCTION(BlueprintCallable, Category="Control Range")
 void SelectShip(AOVShip* Ship);
 bool bFormationMenu=false;
 TMap<TWeakObjectPtr<AOVShip>,FVector> FormationPreview;
 FName FormationKind;
 static int32 FormationRow(FName Preset,FName Hull);
 bool FormationAction(FName Action);
 void CloseFormation();
 bool CanForm(AOVShip* Ship) const;
 UFUNCTION(BlueprintCallable) int32 SelectFleetRectangle(FVector2D Start, FVector2D End, bool bAdditive);
 bool bFleetDrag = false;
 FVector2D FleetDragStart, FleetDragEnd;
 bool bFleetDragAdd = false;
 UFUNCTION(BlueprintCallable, Category="Control Range") void SelectShipFromUser(AOVShip* Ship, bool bAdditive);
 UFUNCTION(BlueprintCallable, Category="Control Range") void ClearSelection();
 UFUNCTION(BlueprintCallable, Category="Control Range") void SelectAdditional(AOVShip* Ship);
 UFUNCTION(BlueprintCallable, Category="Control Range") void SetTarget(AOVShip* Ship);
 UFUNCTION(BlueprintCallable, Category="Control Range") void SampleOrderCursor(FVector RayOrigin,FVector RayDirection,float MouseY,bool Altitude,bool Blocked);
 UFUNCTION(BlueprintCallable, Category="Control Range") void IssueHeading(FVector WorldPoint);
 UFUNCTION(BlueprintCallable, Category="Debug") void TestArmorHit();
 UFUNCTION(BlueprintCallable, Category="Control Range")
 void ConfirmMove();
 UFUNCTION(BlueprintCallable, Category="Control Range")
 bool SurveyEnemy(AOVShip* Target);
 UFUNCTION(BlueprintCallable, Category="Control Range") void UpdateCamera();
 void UpdatePreview();
 UFUNCTION(BlueprintCallable, Category="Control Range")
 void StepSimulation(float DeltaTime);
 UFUNCTION(BlueprintCallable) bool RequestSmallShipDock(int32 BayId,AOVShip* Guest);
 UFUNCTION(BlueprintCallable) bool LaunchSmallShip(int32 BayId);
 UFUNCTION(BlueprintCallable) bool PrepareBoarding(AOVShip* Origin,AOVShip* Target,AOVShip* Shuttle,const TArray<int32>& Units);
 UFUNCTION(BlueprintCallable) bool TransferDockedCrew(AOVShip* Origin,AOVShip* Destination,const TArray<int32>& Units);
 UFUNCTION(BlueprintCallable) bool ConfirmBoarding(int32 Serial);
 UFUNCTION(BlueprintCallable) void CancelCrewOperation(int32 Serial);
 struct FCrewOperation {
  int32 Serial=0;
  TWeakObjectPtr<AOVShip> Origin,Target,Shuttle;
  FName Room;
  TArray<int32> Units;
  bool bTransfer=false;
 };
 TArray<FCrewOperation> CrewOperations;
 int32 NextCrewOperation=1;
 UFUNCTION(BlueprintPure) int32 LastCrewOperation() const { return CrewOperations.IsEmpty()?0:CrewOperations.Last().Serial; }
 int32 Mustered(const FCrewOperation& Op) const;
 void AdvanceShipOperations();
 void ResetShipOperations();
 bool BeginCrewOperation(FCrewOperation Op);

 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat") TObjectPtr<UOVCombatSubsystem> Combat;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat") TObjectPtr<UOVCarrierSubsystem> Carrier;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat") TObjectPtr<UOVPointDefenseSubsystem> PointDefense;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat") TObjectPtr<UOVTransportSubsystem> Transport;
 /** Explicit test-range reset; never rewrites authored layout assets. */
 UFUNCTION(BlueprintCallable, Category="Combat") void StartCombatRange();
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat") int32 CalibrationScenario=-1;
 UFUNCTION(BlueprintCallable, Category="Combat") bool StartCalibrationRange(int32 Scenario=0);
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat") int32 Phase2CScenario=-1;
 UFUNCTION(BlueprintCallable, Category="Combat") bool StartPhase2CRange(int32 Scenario=0);
 UFUNCTION(BlueprintCallable, Category="Debug") bool StartPhase2CStress(int32 ExtraCarrierPairs=1);
 UFUNCTION(BlueprintCallable, Category="Camera") void FrameEngagement();
 UFUNCTION(BlueprintPure, Category="Combat") FString GetScenarioLabel() const;
 UFUNCTION(BlueprintCallable, Category="Combat") void ResetPhase2C();
 UFUNCTION(BlueprintCallable, Category="Mission") bool StartPlayScenario(FName ScenarioId);
 UFUNCTION(BlueprintCallable, Category="Mission") void LeavePlayScenario();
 UFUNCTION(BlueprintPure, Category="Mission") UOVPlayScenarioSubsystem* GetPlayScenario() const;
 UFUNCTION(BlueprintCallable) void AdvancePhase2C(float GameDelta);
 bool CommandCarrier(AOVShip* Ship, bool bLaunch);
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TMap<AOVShip*,float> TrialFTLRemaining;
 bool Phase2CAction(FName Name);
 void ResolveContacts(float SimDelta);
 FOVOrderInput OrderInput;
private:
 friend class UOVPlayScenarioSubsystem;
 UPROPERTY() TArray<TObjectPtr<AOVShip>> ParkedRangeShips;
 bool bPlayScenarioSession=false;
 void ResetPlayScenarioSession(bool bRestoreLegacy);
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
 FKey SandboxConsumedButton;
 void ResetCameraProfile();
 void CancelRightMove();
 void UpdateOrbit(FVector2D Cursor);
 void UpdateRightMove(FVector2D Cursor);
 UFUNCTION(BlueprintCallable, Category="Control Range") void HandleRightMove(bool Pressed, FVector2D Cursor, bool InWorld, bool Shift, bool Ctrl);
};
