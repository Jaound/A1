#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "OVLogisticsSubsystem.generated.h"
class AOVShip;

// Development-only, explicitly supplied quantities; no campaign economy is synthesized.
UENUM(BlueprintType)
enum class EOVDevLogisticsRole : uint8 { Repair, Mining, Transport, Carrier };

/** D126 R37/R29 remote star-system outpost specialization. */
UENUM(BlueprintType)
enum class EOVOutpostRole : uint8
{
    ResourceOutpost,
    CombatOutpost
};

/** Remote star-system outpost state (distinct from Home Base). */
USTRUCT(BlueprintType)
struct ORBITALVANGUARD_API FOVRemoteOutpostState
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 OutpostId = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName StarSystemId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 FactionId = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) EOVOutpostRole Role = EOVOutpostRole::ResourceOutpost;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 SystemDistanceHops = 2;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float WarehouseCapacity = 600.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float StoredResources = 0.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float TotalUnloaded = 0.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float TotalDispatched = 0.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float WarehouseCongestedSeconds = 0.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bWarehouseBottleneck = false;
};

/** Mining ship assignment operating either directly to Home Base or locally to a ResourceOutpost. */
USTRUCT(BlueprintType)
struct ORBITALVANGUARD_API FOVRemoteMiningAssignment
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 AssignmentId = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 FactionId = 0;
    /** 0 = direct multi-system haul to Home Base; >0 = local offload to ResourceOutpost. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 OutpostId = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 MinerCount = 1;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float CargoPerMiner = 80.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float MiningSecondsPerLoad = 200.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float RoundTripSeconds = 20.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float CycleProgressSeconds = 0.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float PendingUnload = 0.0f;
};

/** D126 R37 Medium Transport shuttle route (ResourceOutpost <-> Home Base). */
USTRUCT(BlueprintType)
struct ORBITALVANGUARD_API FOVMediumTransportRoute
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 RouteId = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 FactionId = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 OutpostId = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 TransportCount = 1;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float CargoCapacityPerShip = 250.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float OneWayTransitSeconds = 220.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float LoadUnloadSeconds = 15.0f;
    /** 0 = Waiting/Loading at Outpost, 1 = Inbound to Home Base, 2 = Returning to Outpost */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 Phase = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float PhaseRemainingSeconds = 0.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float CarriedCargo = 0.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float TotalDelivered = 0.0f;
};

USTRUCT(BlueprintType)
struct ORBITALVANGUARD_API FOVDevCargo
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<AOVShip> Ship;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) EOVDevLogisticsRole Role = EOVDevLogisticsRole::Transport;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float Amount = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float Capacity = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float Advanced = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float Rare = 0;
};

USTRUCT(BlueprintType)
struct ORBITALVANGUARD_API FOVDevResourceNode
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 Id = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FVector Position = FVector::ZeroVector;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float Amount = 0;
};

USTRUCT(BlueprintType)
struct ORBITALVANGUARD_API FOVDevRepairLink
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<AOVShip> Source;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<AOVShip> Target;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FGuid HeroId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bHero = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 Faction = -1;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FVector BaseCostPerHP = FVector::ZeroVector;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float Range = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float Rate = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float SetupRemaining = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float TimeRemaining = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float HPRemaining = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FString State;
};

/** Explicit finite DEV operations + D126 R37/R29 Outpost & Medium Transport logistics loop. */
UCLASS()
class ORBITALVANGUARD_API UOVLogisticsSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<FOVDevCargo> Cargo;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<FOVDevResourceNode> Nodes;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FString LastReason;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<FOVDevRepairLink> RepairLinks;

    // D126 R37/R29 Remote Outpost & Medium Transport logistics state
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<FOVRemoteOutpostState> Outposts;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<FOVRemoteMiningAssignment> RemoteMiners;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<FOVMediumTransportRoute> TransportRoutes;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TMap<int32, float> HomeBaseDelivered;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) double OutpostLogisticsElapsedSeconds = 0.0;

    UFUNCTION(BlueprintCallable) bool RegisterDevShip(AOVShip* Ship, EOVDevLogisticsRole Role, float Capacity, float InitialAmount);
    UFUNCTION(BlueprintCallable) void RemoveDevShip(AOVShip* Ship);
    UFUNCTION(BlueprintCallable) int32 AddDevNode(FVector Position, float Amount);
    UFUNCTION(BlueprintCallable) float DevMine(AOVShip* Ship, int32 NodeId, float Requested, float MaxRange);
    UFUNCTION(BlueprintCallable) float DevTransfer(AOVShip* From, AOVShip* To, float Requested, float MaxRange);
    // Withdraws only cargo physically delivered to the specified friendly mission depot.
    float UnloadTransport(AOVShip* Ship, int32 DepotFaction, FVector Depot, float Range, float Requested);
    static float FieldCapacity(FName Hull);
    bool StockFieldShip(AOVShip* Ship, int32 ReplacementStock=-1);
    int32 AircraftKits(const AOVShip* Ship) const;
    bool ConsumeAircraftKit(AOVShip* Ship);
    int32 ResupplyCarrier(AOVShip* Ship);
    void LoseStoredCargo(AOVShip* Ship);
    bool OrderRepair(AOVShip* Source, AOVShip* Target);
    void CancelFieldOrder(AOVShip* Source);
    void ResetFieldOrders(){Approaches.Reset();}
    const FOVDevCargo* FindCargo(const AOVShip* Ship) const;
    UFUNCTION(BlueprintCallable) bool SetDevMaterials(AOVShip* Ship, FVector Materials);
    // Explicit DEV base quote per HP. Field cost is 1.5 times this quote.
    UFUNCTION(BlueprintCallable) bool StartDevRepair(AOVShip* Source, AOVShip* Target, bool bHero, FGuid HeroId, FVector BaseCostPerHP, float Range, float HeroRate, float HeroDuration, float HeroTotalHP);
    UFUNCTION(BlueprintCallable) bool CancelDevRepair(AOVShip* Target, FGuid HeroId);
    UFUNCTION(BlueprintCallable) void Simulate(float GameDelta);
    UFUNCTION(BlueprintCallable) float DevRepairHull(AOVShip* RepairShip, AOVShip* Target, float RequestedHP, float CostPerHP, float MaxRange);

    // D126 Outpost & R37 Medium Transport 3-Step Logistics API
    UFUNCTION(BlueprintCallable, Category = "Logistics|Outpost")
    void ResetOutpostLogistics();

    /** Registers a remote star-system outpost (rejects Home Base / DistanceHops <= 0). */
    UFUNCTION(BlueprintCallable, Category = "Logistics|Outpost")
    int32 AddRemoteOutpost(FName StarSystemId, int32 FactionId, EOVOutpostRole Role, int32 SystemDistanceHops, float WarehouseCapacity);

    /** Assigns mining ships locally to a ResourceOutpost (OutpostId > 0) or directly to Home Base (OutpostId == 0). Rejects CombatOutpost. */
    UFUNCTION(BlueprintCallable, Category = "Logistics|Outpost")
    int32 AssignRemoteMiners(int32 FactionId, int32 OutpostId, int32 MinerCount, float CargoPerMiner, float MiningSecondsPerLoad, float RoundTripSeconds);

    /** Establishes an R37 Medium Transport shuttle route between a ResourceOutpost and Home Base. Rejects CombatOutpost. */
    UFUNCTION(BlueprintCallable, Category = "Logistics|Outpost")
    int32 AddMediumTransportRoute(int32 FactionId, int32 OutpostId, int32 TransportCount, float CargoCapacityPerShip, float OneWayTransitSeconds, float LoadUnloadSeconds = 15.0f);

    /** Advances the 3-step remote mining -> outpost warehouse -> R37 medium transport -> home base delivery simulation. */
    UFUNCTION(BlueprintCallable, Category = "Logistics|Outpost")
    void SimulateOutpostLogistics(float GameDeltaSeconds);

    UFUNCTION(BlueprintPure, Category = "Logistics|Outpost")
    float GetHomeBaseDelivered(int32 FactionId) const;

    UFUNCTION(BlueprintPure, Category = "Logistics|Outpost")
    float GetHomeBaseRatePerMinute(int32 FactionId) const;

    /** Diagnoses whether an outpost warehouse is congested and returns a Korean operational diagnosis string. */
    UFUNCTION(BlueprintCallable, Category = "Logistics|Outpost")
    bool DiagnoseOutpostBottleneck(int32 OutpostId, FString& OutDiagnosis) const;

private:
    TMap<TWeakObjectPtr<AOVShip>, TWeakObjectPtr<AOVShip>> Approaches;
    int32 NextNodeId = 1;
    int32 NextOutpostId = 1;
    int32 NextMinerAssignmentId = 1;
    int32 NextTransportRouteId = 1;
};
