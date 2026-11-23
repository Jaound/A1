#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "OVInteriorSubsystem.generated.h"

class AOVShip;

UENUM(BlueprintType)
enum class EOVInteriorRole : uint8 { Trooper, Melee, Ranged, Support, Commander };
UENUM(BlueprintType)
enum class EOVInteriorOrder : uint8 { None, Capture, Sabotage, Defend, Retreat };
UENUM(BlueprintType)
enum class EOVInteriorState : uint8 { Aboard, Moving, Fighting, Operating, Defending, Retreating, InTransit, AwaitingRecovery, RescueWaiting, Dead, Blocked };

USTRUCT(BlueprintType)
struct ORBITALVANGUARD_API FOVInteriorRoleProfile
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EOVInteriorRole Role = EOVInteriorRole::Melee;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float HP = 300;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Damage = 25;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Period = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Range = 180;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Speed = 360;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Armor = .18f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Evasion = .08f;
};

/** L1 v0.5 abilities; timing, spaces and casualty details remain replaceable trial policy. */
USTRUCT(BlueprintType)
struct ORBITALVANGUARD_API FOVInteriorProfile
{
    GENERATED_BODY()
    FOVInteriorProfile();
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ProfileId = TEXT("Interior_D116_L1_Trial_v1");
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FOVInteriorRoleProfile> HeroRoles;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float MeleeReduction = .20f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float MeleeDuration = 4;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float MeleeCooldown = 16;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float RangedMultiplier = 2;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float RangedCooldown = 12;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float SupportHeal = 85;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float SupportCooldown = 6;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 InitialSupportCatalysts = 4;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float CommanderAuraRange = 1400;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float CommanderDamageBonus = .12f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float CaptureSeconds = 18;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float BridgeSeconds = 36;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float SabotageSeconds = 30;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float AdditionalWorkerBonus = .15f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float SightRange = 2200;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float ArrivalTolerance = 20;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float TrooperHP = 100;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float TrooperDamage = 8;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float TrooperPeriod = 1.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float TrooperRange = 1200;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float TrooperSpeed = 280;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float FacilityShockFraction = .15f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float DisasterDamagePerSecond = 2;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float DisasterSeconds = 20;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float CapturedBridgeHeroDamageFraction = .65f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 MaxActiveHeroesPerFaction = 15;
};

USTRUCT(BlueprintType)
struct ORBITALVANGUARD_API FOVInteriorUnitRecord
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 UnitId = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 PersonId = 0;
    // 영속 로스터의 개인 ID. 기존 시험용 인원은 유효하지 않은 ID를 유지한다.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FGuid HeroId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName DefinitionId;
    double PodEntryAt=-1000;
    double SuppressedUntil=0;
    double KillRallyUntil=0;
    double RushRefundUntil=0;
    float EquipmentMove=0, EquipmentHeal=0, EquipmentCapture=0;
    double ApexUntil=0;
    double GuardSlowUntil=0;
    TSet<int32> GuardContacts;
    TSet<int32> RushTargets;
    double JamUntil=0;
    double InvasionAlertUntil=0;
    FName InvasionSlot;
    FName LastDutySlot;
    bool bTraumaTreatmentAvailable=true;
    bool bRushWeakened=false;
    bool bPendingIrisArc=false;
    FVector FieldFacing=FVector::ForwardVector;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 SignatureUses=0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 PassiveHits=0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<int32> HeroSkillRanks;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 HeroLevel = 1;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 AmplifierCharges = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FVector DevRepairBaseCost = FVector::ZeroVector;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bDutyRequested = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bDutyActive = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName DutySlot;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) double DutyReadyAt = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) double Skill3ReadyAt = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) double Skill3ActiveUntil = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) double Skill4ReadyAt = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) double Skill4ActiveUntil = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bSkill4Armed = true;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bRushAttack = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 TrackedEnemy = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) double TrackedSince = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) double SlowUntil = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float SlowFraction = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 BurstRemaining = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 BurstTargetId = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 FactionId = -1;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) EOVInteriorRole Role = EOVInteriorRole::Trooper;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FString Name;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float HP = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float MaxHP = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bWounded = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bWasCriticallyInjured = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) EOVInteriorState State = EOVInteriorState::Aboard;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TWeakObjectPtr<AOVShip> Ship;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName NodeId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName LayoutVersion;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FVector LocalPosition = FVector::ZeroVector;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) EOVInteriorOrder Order = EOVInteriorOrder::None;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName TargetSlot;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FString Reason;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float Progress = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 Medkits = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 Shots = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 Hits = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 SkillUses = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 DeathEvents = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float DistanceTravelled = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) double AttackReadyAt = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) double SkillReadyAt = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) double SkillActiveUntil = 0;
    int32 SquadId=0;
    double LastPhaseArcAt=-1000;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) double LastDamagedAt = -1000;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) double OperationContributedAt = -1000;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 EnemyId = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<FName> Path;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TWeakObjectPtr<AOVShip> EntryShip;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName EntrySlot;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FVector EntryLocalPoint = FVector::ZeroVector;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float Damage = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float Period = 1;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float Range = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float Speed = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float Armor = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float Evasion = 0;
    // Stable private navigation bookkeeping travels with the record, never recreated by HUD refresh.
    int32 PathIndex = 0;
    FName PlannedGoal;
    bool bWasInsideTarget = false;
};

USTRUCT(BlueprintType)
struct ORBITALVANGUARD_API FOVHeroDutyBonuses
{
    GENERATED_BODY()
    float PDInterval=1, PDAccuracy=0, MissileInterval=1, GuidedTurn=1, GuidedSpread=1;
    float AccelerationScale=1, BrakingScale=1, LateralScale=1;
    bool bNearTraverse=false, bLongAim=false, bExposedAim=false;
    bool bShieldHarmonizer=false, bShipDisasterField=false, bEngineering=false, bHangarEfficiency=false;
    float ProductionScale=1, DockScale=1, BoltSpeedScale=1, WingPeriodScale=1, WingEvasion=0;
    float SectionScale=1, ReformScale=1, ReturnScale=1, PodScale=1, ShuttleDockScale=1;
    float ShipCombatTempo=0;
    bool bOpeningAim=false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float AccuracyBonus = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float FireRateBonus = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float SpeedBonus = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float TurnBonus = 0;
};

USTRUCT(BlueprintType)
struct ORBITALVANGUARD_API FOVInteriorOperation
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TWeakObjectPtr<AOVShip> Ship;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName SlotId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 FactionId = -1;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) EOVInteriorOrder Kind = EOVInteriorOrder::Capture;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float Progress = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float Checkpoint = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bCompleted = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<int32> ParticipantUnitIds;
};

USTRUCT(BlueprintType)
struct ORBITALVANGUARD_API FOVInteriorEvent
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 Serial = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) double Time = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 UnitId = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 FactionId = -1;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName Kind;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TWeakObjectPtr<AOVShip> Ship;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName SlotId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FString Message;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<int32> ParticipantUnitIds;
};

/** Actual units, navigation and operations. DEV transport tokens never become units implicitly. */
UCLASS()
class ORBITALVANGUARD_API UOVInteriorSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    virtual void Deinitialize() override;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bEnabled = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FOVInteriorProfile Settings;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) double GameTime = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<FOVInteriorUnitRecord> Units;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<FOVInteriorOperation> Operations;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<FOVInteriorEvent> Events;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FString LastMessage;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 TotalDeaths = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 TotalCaptures = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 TotalSabotages = 0;
    UFUNCTION(BlueprintCallable) void ResetScenario(const TArray<AOVShip*>& Ships, int32 Seed = 260112);
    UFUNCTION(BlueprintCallable) void Disable();
    UFUNCTION(BlueprintCallable) bool AddShip(AOVShip* Ship);
    UFUNCTION(BlueprintCallable) bool RemoveShip(AOVShip* Ship);
    UFUNCTION(BlueprintCallable) int32 RegisterUnit(AOVShip* Ship, int32 Faction, EOVInteriorRole Role, FName SlotId = NAME_None);
    UFUNCTION(BlueprintCallable) int32 SpawnRosterHero(AOVShip* Ship, FGuid HeroId, FName SlotId = NAME_None);
    UFUNCTION(BlueprintCallable) bool SetHeroBoardingAssignment(int32 UnitId, bool bAssigned);
    UFUNCTION(BlueprintCallable) bool SetHeroDuty(int32 UnitId, bool bEnable, FName SlotId = NAME_None);
    float FormationRadarBonus(const AOVShip* Receiver) const;
    UFUNCTION(BlueprintPure) FOVHeroDutyBonuses GetDutyBonuses(AOVShip* Ship) const;
    UFUNCTION(BlueprintCallable) TArray<int32> RegisterBoardingTeam(AOVShip* Origin, int32 Faction, int32 HeroCount, int32 Troops);
    UFUNCTION(BlueprintCallable) bool IssueOrder(const TArray<int32>& UnitIds, EOVInteriorOrder Order, FName TargetSlot = NAME_None);
    UFUNCTION(BlueprintCallable) void Simulate(float GameDelta);
    UFUNCTION(BlueprintCallable) bool SetConnectionBlocked(AOVShip* Ship, FName ConnectionId, bool bBlocked);
    UFUNCTION(BlueprintPure) FOVInteriorUnitRecord GetUnit(int32 UnitId) const;
    UFUNCTION(BlueprintPure) TArray<int32> GetPassengerIds(const TArray<int32>& UnitIds) const;
    UFUNCTION(BlueprintPure) TArray<int32> GetUnitsOnShip(AOVShip* Ship, int32 Faction) const;
    int32 CountEmbarkedHeroes(const AOVShip* Ship,int32 Faction) const;
    UFUNCTION(BlueprintPure) int32 CountActiveHeroes(int32 Faction) const;
    UFUNCTION(BlueprintPure) bool CanObserve(int32 ViewerFaction, int32 UnitId) const;
    UFUNCTION(BlueprintPure) bool IsStructureKnown(int32 ViewerFaction, AOVShip* Ship) const;
    UFUNCTION(BlueprintPure) FString GetSummary(int32 ViewerFaction) const;
    UFUNCTION(BlueprintPure) bool ValidateInterior(AOVShip* Ship, TArray<FString>& Errors) const;
private:
    friend class FOVHeroTacticalEffectsTest;
    struct FGraphState
    {
        TWeakObjectPtr<AOVShip> Ship;
        FName Version;
        TMap<FName, TArray<FName>> Neighbors;
        TSet<FName> BlockedConnections;
        TMap<FName, float> PreviousHP;
        TMap<FName, double> Disasters;
        TSet<int32> KnownBy;
        TSet<int32> PresenceFactions;
        TSet<int32> CompletedBy;
    };
    TArray<FGraphState> Graphs;
    FRandomStream Random;
    double Accumulator = 0;
    int32 NextUnitId = 1;
    int32 NextEventSerial = 1;
    FOVInteriorUnitRecord* FindUnit(int32 Id);
    FGraphState* FindGraph(AOVShip* Ship);
    const FGraphState* FindGraph(AOVShip* Ship) const;
    bool PassageOpen(const FGraphState& Graph, FName A, FName B) const;
    bool PlanPath(FOVInteriorUnitRecord& Unit, FName Goal);
    bool MoveTowards(FOVInteriorUnitRecord& Unit, FName Goal, float Delta);
    bool MovePoint(FOVInteriorUnitRecord& Unit, FVector Goal, float Delta);
    bool Visible(const FOVInteriorUnitRecord& A, const FOVInteriorUnitRecord& B, float Range) const;
    float MovementBonus(const FOVInteriorUnitRecord& U) const;
    float ExternalAuraScale(const FOVInteriorUnitRecord& Target) const;
    bool IsOnboard(const FOVInteriorUnitRecord& Unit) const;
    void Step(float Delta);
    void SyncTransport(FOVInteriorUnitRecord& Unit);
    float InteriorReduction(const FOVInteriorUnitRecord& Target,float Ignore,bool bPhaseStrike) const;
    bool ActiveDuty(const FOVInteriorUnitRecord& Unit) const;
    void ReleaseIrisField(FOVInteriorUnitRecord& Unit);
    void AdvanceUnit(FOVInteriorUnitRecord& Unit, float Delta);
    void AdvanceOperations(float Delta);
    void ApplyDamage(FOVInteriorUnitRecord& Unit, float Amount, const FString& Reason);
    void Kill(FOVInteriorUnitRecord& Unit, const FString& Reason);
    void RefreshPresence();
    void ProcessFacilityDamage(float Delta);
    void Emit(FName Kind, const FOVInteriorUnitRecord& Unit, const FString& Message);
    void PersistRoster(bool bDetach);
};
