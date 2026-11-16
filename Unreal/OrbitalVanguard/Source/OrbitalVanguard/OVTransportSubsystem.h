#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "OVShipLayout.h"
#include "OVTransportSubsystem.generated.h"

class AOVShip;

// These are DEV people, not implemented heroes or interior combat units.
UENUM(BlueprintType)
enum class EOVPassengerState : uint8 { Aboard, InPod, InsideTarget, InShuttle, AwaitingRecovery, RescueWaiting, Dead };
UENUM(BlueprintType)
enum class EOVPodFlightState : uint8 { Queued, Outbound, Attached, Returning, Recovered, Destroyed, Stranded, Cancelled };
UENUM(BlueprintType)
enum class EOVShuttleState : uint8 { Ready, Approaching, Docking, Delivered, Recovering, Returning, Lost, RecoveryBlocked };

USTRUCT(BlueprintType)
struct ORBITALVANGUARD_API FOVTransportProfile
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ProfileId = TEXT("Transport_DEV_D116_v1");
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float PodLaunchRange = 25000;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float PodSpeed = 16500;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float PodHP = 160;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float PodEvasion = .40f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float LaunchInterval = .25f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float DockingSeconds = 3;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float DockTolerance = 250;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 ShuttleCapacity = 12;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 ShuttleHeroCapacity = 4;
    // R18/R26: DEV boundary only; no free emergency escape, capture, or interior movement.
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float ShieldContactDamage = 40;
};

USTRUCT(BlueprintType)
struct ORBITALVANGUARD_API FOVPassengerToken
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 PersonId = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 FactionId = -1;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bHeroTypeDEV = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 RuntimeUnitId = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bRuntimeHero = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bBoardingAssigned = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) EOVPassengerState State = EOVPassengerState::Aboard;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TWeakObjectPtr<AOVShip> CurrentShip;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 PodId = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TWeakObjectPtr<AOVShip> Shuttle;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FVector EntryLocalPoint = FVector::ZeroVector;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FVector EntryLocalNormal = FVector::ZeroVector;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) EOVArmorFace EntryFace = EOVArmorFace::Front;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 EntryDeck = 1;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName EntrySlot;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName EntryLayoutVersion;
};

USTRUCT(BlueprintType)
struct ORBITALVANGUARD_API FOVPodState
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 PodId = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 PersonId = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 FactionId = -1;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) EOVPodFlightState State = EOVPodFlightState::Queued;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TWeakObjectPtr<AOVShip> Origin;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TWeakObjectPtr<AOVShip> Target;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TWeakObjectPtr<AOVShip> ReturnDestination;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FVector Position = FVector::ZeroVector;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float HP = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float Evasion = .4f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) double LaunchTime = -1;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) double ScheduledTime = 0;
};

USTRUCT(BlueprintType)
struct ORBITALVANGUARD_API FOVShuttleRecord
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TWeakObjectPtr<AOVShip> Ship;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TWeakObjectPtr<AOVShip> Home;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TWeakObjectPtr<AOVShip> Target;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 FactionId = -1;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) EOVShuttleState State = EOVShuttleState::Ready;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bManualSuspended = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<int32> PassengerIds;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FVector HomePosition = FVector::ZeroVector;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FVector DockLocalPoint = FVector::ZeroVector;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FVector DockLocalNormal = FVector::ForwardVector;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float Progress = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FString Reason;
};

USTRUCT(BlueprintType)
struct ORBITALVANGUARD_API FOVTransportEntryEvent
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 PersonId = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TWeakObjectPtr<AOVShip> Target;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 FactionId = -1;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FVector LocalPoint = FVector::ZeroVector;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) EOVArmorFace Face = EOVArmorFace::Front;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 Deck = 1;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName SlotId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) double Time = 0;
};

/** Transport clock is supplied by the combat owner; no independent Actor Tick. */
UCLASS()
class ORBITALVANGUARD_API UOVTransportSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bEnabled = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FOVTransportProfile Settings;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) double GameTime = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<FOVPassengerToken> Persons;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<FOVPodState> Pods;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<FOVShuttleRecord> Shuttles;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<FOVTransportEntryEvent> EntryEvents;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FString LastMessage;
    // Explicit scenario reset, never called by UI refresh or profile selection.
    UFUNCTION(BlueprintCallable) void ResetScenario(const TArray<AOVShip*>& Ships);
    UFUNCTION(BlueprintCallable) void Disable();
    UFUNCTION(BlueprintCallable) TArray<int32> AddDevPassengers(AOVShip* Origin, int32 HeroTypes, int32 Troops);
    UFUNCTION(BlueprintCallable) int32 RegisterRuntimePassenger(AOVShip* Origin, int32 UnitId, int32 Faction, bool bHero);
    UFUNCTION(BlueprintCallable) bool ReturnRuntimePersonToEntry(int32 PersonId, int32 UnitId, AOVShip* Ship, FVector ActualLocalPoint, FName LayoutVersion);
    UFUNCTION(BlueprintCallable) bool MarkRuntimePersonDead(int32 PersonId, int32 UnitId);
    UFUNCTION(BlueprintCallable) bool SetRuntimeBoardingAssignment(int32 PersonId, int32 UnitId, bool bAssigned);
    UFUNCTION(BlueprintCallable) bool CancelRuntimeRecovery(int32 PersonId, int32 UnitId, AOVShip* InteriorShip);
    UFUNCTION(BlueprintCallable) int32 LaunchPods(AOVShip* Origin, AOVShip* Target);
    UFUNCTION(BlueprintCallable) void RecallPods(AOVShip* Origin);
    // DEV supplied confirmation of reaching original entry point. Interior pathfinding is not implemented.
    UFUNCTION(BlueprintCallable) bool DevReturnPersonToEntry(int32 PersonId);
    UFUNCTION(BlueprintCallable) bool InterceptPod(int32 PodId, float Damage);
    UFUNCTION(BlueprintCallable) bool RegisterShuttle(AOVShip* Ship, AOVShip* Home = nullptr);
    UFUNCTION(BlueprintCallable) bool DispatchShuttle(AOVShip* Ship, AOVShip* Target, const TArray<int32>& PassengerIds);
    UFUNCTION(BlueprintCallable) bool SuspendShuttle(AOVShip* Ship);
    UFUNCTION(BlueprintCallable) bool RecallShuttle(AOVShip* Ship);
    UFUNCTION(BlueprintCallable) void Simulate(float GameDelta);
    UFUNCTION(BlueprintPure) FOVPassengerToken GetPerson(int32 PersonId) const;
    UFUNCTION(BlueprintPure) int32 CountState(EOVPassengerState State) const;
    UFUNCTION(BlueprintPure) FString GetSummary() const;
private:
    UPROPERTY() TArray<TObjectPtr<AOVShip>> ScenarioShips;
    struct FPresence { TWeakObjectPtr<AOVShip> Ship; int32 Faction; bool Baseline; bool BaselineObjective; bool BaselineDev; };
    TArray<FPresence> OwnedPresence;
    int32 NextPersonId = 1;
    int32 NextPodId = 1;
    double Accumulator = 0;
    void Step(float Delta);
    void AdvancePod(FOVPodState& Pod, float Delta);
    void AdvanceShuttle(FOVShuttleRecord& Shuttle, float Delta);
    void Enter(FOVPassengerToken& Person, AOVShip* Target, FVector WorldPoint, FVector WorldNormal);
    void RefreshPresence();
    void DestroyPod(FOVPodState& Pod);
    void BeginReturn(FOVPodState& Pod);
    AOVShip* FindReturn(AOVShip* Preferred, int32 Faction, FVector Position, AOVShip* Exclude = nullptr) const;
    bool Usable(AOVShip* Ship) const;
    bool ReturnPersonAtEntry(FOVPassengerToken& Person);
    FOVPassengerToken* FindPerson(int32 Id);
};
