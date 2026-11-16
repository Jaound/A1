#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "OVCarrierSubsystem.generated.h"

class AOVShip;

UENUM(BlueprintType)
enum class EOVAircraftState : uint8 { Stored, Sortie, Returning, DockWaiting, ControlLost, Wrecked };
UENUM(BlueprintType)
enum class EOVAircraftLeg : uint8 { Ingress, Egress, Regroup };
UENUM(BlueprintType)
enum class EOVAircraftManeuver : uint8 { Approach, AttackRun, ShortEgress, Regroup, Dogfight, Patrol };
UENUM(BlueprintType)
enum class EOVHangarMode : uint8 { Aircraft, SmallShip };

/** D116 fighter candidate, not an economic/production implementation. All distances are cm. */
USTRUCT(BlueprintType)
struct ORBITALVANGUARD_API FOVAircraftProfile
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ProfileId = TEXT("D116_Fighter_2C_v1");
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Hull = 240;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Shield = 80;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Evasion = .62f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Speed = 10000;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float ReturnSpeed = 12000;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Damage = 20;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Period = 1.2f;
    // UE trial geometry/timing: R16/R23/R25, separate from Python aggregate DPS.
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float AttackRange = 12000;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float BoltSpeed = 45000;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float BoltLifetime = 3;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float CollisionRadius = 100;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float DockSeconds = 1;
};

/** R16 시험 기동값. 화력/속도 프로필과 분리하며 이동 위치는 순간 보정하지 않는다. */
USTRUCT(BlueprintType)
struct ORBITALVANGUARD_API FOVAircraftManeuverProfile
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ProfileId = TEXT("Trial_StrikeManeuver_v1");
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float TurnDegreesPerSecond = 300;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float AttackRangeFraction = .50f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float EgressSeconds = .30f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float RegroupTurnRadii = 1.15f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float HullClearance = 400;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 SectionSize = 4;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float SectionDelay = .12f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float DefensiveRangeFraction = .55f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float DefensiveSeconds = 2.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float RejoinSeconds = 3;
};

USTRUCT(BlueprintType)
struct ORBITALVANGUARD_API FOVHangarState
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 HangarId = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TWeakObjectPtr<AOVShip> Carrier;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName SlotId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) EOVHangarMode Mode = EOVHangarMode::Aircraft;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 ExtraAircraftCapacity = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TWeakObjectPtr<AOVShip> StoredSmallShip;
};

USTRUCT(BlueprintType)
struct ORBITALVANGUARD_API FOVAircraftRecord
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 AircraftId = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 HangarId = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 FactionId = -1;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TWeakObjectPtr<AOVShip> Carrier;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) EOVAircraftState State = EOVAircraftState::Stored;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) EOVAircraftState ResumeState = EOVAircraftState::Sortie;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) EOVAircraftLeg Leg = EOVAircraftLeg::Ingress;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float HP = 240;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float Shield = 80;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float Evasion = .62f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FVector Position = FVector::ZeroVector;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FVector PreviousPosition = FVector::ZeroVector;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FVector Velocity = FVector::ZeroVector;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FVector Direction = FVector::ForwardVector;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FVector Waypoint = FVector::ZeroVector;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float Cooldown = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float DockRemaining = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 ShotsFired = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) EOVAircraftManeuver Maneuver = EOVAircraftManeuver::Approach;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TWeakObjectPtr<AOVShip> StrikeTarget;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 InterceptTargetId = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FVector StrikeCenter = FVector::ZeroVector;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FVector ApproachPoint = FVector::ZeroVector;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FVector EgressPoint = FVector::ZeroVector;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FVector RegroupPoint = FVector::ZeroVector;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float AllowedRadius = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float TurnRadius = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float LegRemaining = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float DefensiveRemaining = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float RejoinRemaining = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float AttackDelay = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 AttackPasses = 0;
};

/** Launch attribution is copied, not resolved through the carrier after capture/destruction. */
USTRUCT(BlueprintType)
struct ORBITALVANGUARD_API FOVAircraftBolt
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 BoltId = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 AircraftId = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 FactionId = -1;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName SourceId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TWeakObjectPtr<AOVShip> SourceCarrier;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName TargetId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName HitId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName ProfileId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FVector Origin = FVector::ZeroVector;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FVector Position = FVector::ZeroVector;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FVector Direction = FVector::ForwardVector;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float Damage = 20;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float Speed = 45000;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float Age = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float Lifetime = 3;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bResolved = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bHit = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) double ResolvedAt = 0;
};

USTRUCT(BlueprintType)
struct ORBITALVANGUARD_API FOVCarrierState
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TWeakObjectPtr<AOVShip> Carrier;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bRecallOrdered = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bAbandonOutstanding = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bWasFTLCharging = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 LastFaction = -1;
};

/** No Tick: the same fixed simulation clock that drives combat supplies game time. */
UCLASS()
class ORBITALVANGUARD_API UOVCarrierSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FOVAircraftProfile Profile;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FOVAircraftManeuverProfile ManeuverProfile;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<FOVHangarState> Hangars;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<FOVAircraftRecord> Aircraft;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<FOVAircraftBolt> Bolts;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<FOVAircraftBolt> RecentEvents;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<FOVCarrierState> Carriers;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) double GameTime = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 ShotCount = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 HitCount = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 LossCount = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 DeveloperRepairCount = 0;
    UFUNCTION(BlueprintCallable) void Reset(int32 Seed = 260112);
    // Idempotent. The DEV initial inventory is allocated only for a newly registered carrier.
    UFUNCTION(BlueprintCallable) bool RegisterCarrier(AOVShip* Ship, int32 InitialPerHangar = 20);
    UFUNCTION(BlueprintCallable) int32 Launch(AOVShip* Ship);
    UFUNCTION(BlueprintCallable) void Recall(AOVShip* Ship);
    UFUNCTION(BlueprintCallable) void Simulate(float GameDelta);
    UFUNCTION(BlueprintCallable) bool ApplyAircraftDamage(int32 AircraftId, float Damage);
    UFUNCTION(BlueprintCallable) void SetCapacityBonus(AOVShip* Ship, int32 ExtraPerHangar);
    UFUNCTION(BlueprintCallable) bool SetHangarMode(int32 HangarId, EOVHangarMode Mode);
    UFUNCTION(BlueprintPure) int32 GetHangarCapacity(int32 HangarId) const;
    UFUNCTION(BlueprintPure) int32 CountAircraft(AOVShip* Ship, EOVAircraftState State) const;
    UFUNCTION(BlueprintPure) bool HasOutstandingAircraft(AOVShip* Ship) const;
    UFUNCTION(BlueprintPure) bool CanDepart(AOVShip* Ship) const;
    UFUNCTION(BlueprintCallable) void AbandonOutstanding(AOVShip* Ship);
    // Explicit DEV supply; never called by recall, docking, profile switch or UI refresh.
    UFUNCTION(BlueprintCallable) int32 DebugSupplyRepair(AOVShip* Ship);
    UFUNCTION(BlueprintPure) FString GetCarrierStatus(AOVShip* Ship) const;
    static bool IsAirborneState(EOVAircraftState State);
private:
    double Accumulator = 0;
    int32 NextAircraftId = 1, NextHangarId = 1, NextBoltId = 1;
    FRandomStream Random;
    float PatternOffset = 0;
    struct FStrikeCandidateCache
    {
        TWeakObjectPtr<AOVShip> Source;
        TArray<TWeakObjectPtr<AOVShip>> Candidates;
    };
    // Rebuilt every fixed step; retained capacity avoids per-aircraft detection allocations.
    TArray<FStrikeCandidateCache> StrikeCandidateCache;
    void RebuildStrikeCandidates();
    bool IsHangarOperational(const FOVHangarState& Hangar) const;
    bool IsHangarDestroyed(const FOVHangarState& Hangar) const;
    bool HasControl(AOVShip* Ship) const;
    FVector HangarPosition(const FOVHangarState& Hangar) const;
    FOVHangarState* FindDock(FOVAircraftRecord& Craft);
    void Step(float Delta);
    void AdvanceAircraft(FOVAircraftRecord& Craft, float Delta);
    AOVShip* SelectStrikeTarget(const FOVAircraftRecord& Craft) const;
    const FOVAircraftRecord* SelectAirTarget(FOVAircraftRecord& Craft, AOVShip* StrikeTarget, float Delta);
    void AdvanceStrike(FOVAircraftRecord& Craft, AOVShip* Target, float Delta);
    void AdvanceDogfight(FOVAircraftRecord& Craft, const FOVAircraftRecord& Target, float Delta);
    void FlyToward(FOVAircraftRecord& Craft, FVector Point, float Delta, bool bFiniteTurn);
    void AdvanceBolts(float Delta);
    bool Fire(FOVAircraftRecord& Craft, FVector AimPoint, FName TargetId);
    void DestroyAircraft(FOVAircraftRecord& Craft);
};
