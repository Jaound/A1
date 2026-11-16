#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "OVShipLayout.h"
#include "OVCombatSubsystem.generated.h"

class AOVShip;

UENUM(BlueprintType)
enum class EOVAttackKind : uint8 { Beam, Cannon, Missile, GuidedMissile };

UENUM(BlueprintType)
enum class EOVAttackResult : uint8 { Flying, Hit, Miss, Expired, TargetWrecked, TargetDeparted, TrackingExpired, Intercepted };

/** Values here are the explicitly provisional 2B test profile, in cm and game seconds. */
USTRUCT(BlueprintType)
struct ORBITALVANGUARD_API FOVWeaponProfile
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ProfileId = TEXT("BeamRapid_T1");
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EOVWeaponType Family = EOVWeaponType::Beam;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EOVAttackKind Kind = EOVAttackKind::Beam;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Tier = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Range = 6000;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Interval = 0.9f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Damage = 45;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Speed = 60000;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Accuracy = 0.94f;
    // D085: beam probability; for physical projectiles Accuracy is aim quality, not a hit roll.
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float AimSpreadDegrees = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float VLSHeight = 300;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FRotator StandbyAim = FRotator::ZeroRotator;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float TraverseSpeed = 100;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float TraverseAcceleration = 400;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float FireTolerance = 3;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Salvo = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float SalvoInterval = 0.18f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Lifetime = 15;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float GuidanceTurnRate = 75;
    // 2C candidate projectile body, independent of the launcher mount HP and attack damage.
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float ProjectileHP = 24;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float InterceptionEvasion = .25f;
};

USTRUCT(BlueprintType)
struct ORBITALVANGUARD_API FOVRadarProfile
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Tier = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Range = 6000;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float AccuracyBonus = 0.04f;
};

UCLASS(Config=Game, DefaultConfig)
class ORBITALVANGUARD_API UOVCombatSettings : public UObject
{
    GENERATED_BODY()
public:
    UOVCombatSettings();
    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Provisional 2B") TArray<FOVWeaponProfile> Weapons;
    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Provisional 2B") TArray<FOVRadarProfile> Radars;
    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Provisional 2B") float GuaranteedRange = 1600;
    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Provisional 2B") float OrdinaryShareCoefficient = 0;
    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Provisional 2B") float SupportShareCoefficient = 0.8f;
    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Provisional 2B") float GuidedTrackingSeconds = 8;
    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Provisional 2B") float MissOffsetRadiusFactor = 2.5f;
    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Provisional 2B") float ControlAccuracyBonus = 0.02f;
    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Provisional 2B") float AutoClearanceHeight = 600;
    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Provisional 2B") float MountCollisionRadius = 18;
    // Only for the first executable checkpoint. Final 2B defaults to formal detection.
    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Development") bool bDevelopmentOmniscient = false;
};

USTRUCT(BlueprintType)
struct ORBITALVANGUARD_API FOVDetectionInfo
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bDetected = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName SourceId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FString Source = TEXT("NO DETECTION");
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float AccuracyBonus = 0;
};

USTRUCT(BlueprintType)
struct ORBITALVANGUARD_API FOVFireReadiness
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bReady = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FString Reason = TEXT("COMBAT OFF");
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FOVDetectionInfo Detection;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float Cooldown = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float Accuracy = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float AimSpreadDegrees = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float Distance = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float AimError = 0;
};

USTRUCT(BlueprintType)
struct ORBITALVANGUARD_API FOVCombatMountState
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName MountId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FOVWeaponProfile Profile;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FRotator LocalAim = FRotator::ZeroRotator;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FVector2D AngularVelocity = FVector2D::ZeroVector;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float Cooldown = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float SalvoDelay = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 SalvoRemaining = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bFireRequested = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TWeakObjectPtr<AOVShip> RequestedTarget;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TWeakObjectPtr<AOVShip> SalvoTarget;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bAutomaticSalvo = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float TrackingRemaining = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bHadDetection = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FVector LastTrackedWorldPoint = FVector::ZeroVector;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 ShotsFired = 0;
};

UENUM(BlueprintType)
enum class EOVAttackIntent : uint8 { DestroyHull, DisableWeapons };
UENUM(BlueprintType)
enum class EOVProtectionPolicy : uint8 { ContinueFire, UntilObjective, UntilExit };

USTRUCT(BlueprintType)
struct ORBITALVANGUARD_API FOVCombatShipState
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<AOVShip> Ship;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName ShipId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 RadarTier = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bRadarSupport = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) EOVAttackIntent Intent = EOVAttackIntent::DestroyHull;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) EOVProtectionPolicy Protection = EOVProtectionPolicy::UntilExit;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName ForcedMount;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bLastAttackAutomatic = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 OrderFaction = -1;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 TargetFaction = -1;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 WeaponTierCap = 1;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bDeparted = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<FOVCombatMountState> Mounts;
};

/** Launch data survives source destruction, capture and target sensor loss unchanged. */
USTRUCT(BlueprintType)
struct ORBITALVANGUARD_API FOVAttackState
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 AttackId = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName AttackerId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 FactionId = -1;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName WeaponId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FOVWeaponProfile Weapon;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TWeakObjectPtr<AOVShip> Attacker;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TWeakObjectPtr<AOVShip> Target;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FVector Origin = FVector::ZeroVector;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FVector Position = FVector::ZeroVector;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FVector Direction = FVector::ForwardVector;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FVector TargetLocalPoint = FVector::ZeroVector;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FVector AimOffset = FVector::ZeroVector;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FVector FrozenAimPoint = FVector::ZeroVector;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FVector VLSControlPoint = FVector::ZeroVector;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float VLSDuration = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float Age = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) double LaunchTime = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) double ResolvedAt = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float TrackingLifetime = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float Damage = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float Accuracy = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) EOVAttackResult Result = EOVAttackResult::Flying;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float InterceptHP = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName HitShipId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName ImpactWeaponId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName ArmorId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName SlotId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float HullDamage = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 Explosions = 0;
};

/** Single combat clock. Camera/HUD are read-only consumers of these states and events. */
UCLASS()
class ORBITALVANGUARD_API UOVCombatSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bEnabled = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) double GameTime = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 ShotCount = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 HitCount = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UOVCombatSettings> Settings;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<FOVCombatShipState> Ships;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<FOVAttackState> Attacks;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<FOVAttackState> RecentEvents;
    UFUNCTION(BlueprintCallable) void EnableCombat(const TArray<AOVShip*>& InShips, int32 Seed = 260112);
    UFUNCTION(BlueprintCallable) void DisableCombat();
    UFUNCTION(BlueprintCallable) void RegisterShip(AOVShip* Ship, int32 RadarTier = 1, bool bRadarSupport = false, int32 WeaponTierCap = 0);
    UFUNCTION(BlueprintCallable) void Simulate(float GameDelta);
    UFUNCTION(BlueprintCallable) bool SetTarget(AOVShip* Ship, AOVShip* Target);
    UFUNCTION(BlueprintCallable) bool SetAttackIntent(AOVShip* Ship, EOVAttackIntent Intent, FName ForceMount = NAME_None);
    UFUNCTION(BlueprintCallable) void SetProtectionPolicy(AOVShip* Ship, EOVProtectionPolicy Policy);
    UFUNCTION(BlueprintPure) bool IsTargetProtected(AOVShip* Ship, AOVShip* Target) const;
    UFUNCTION(BlueprintPure) FString GetOrderStatus(AOVShip* Ship) const;
    UFUNCTION(BlueprintCallable) void CancelAttackOrder(AOVShip* Ship);
    UFUNCTION(BlueprintCallable) bool RequestFire(AOVShip* Ship, FName MountId = NAME_None);
    UFUNCTION(BlueprintCallable) bool SetWeaponProfile(AOVShip* Ship, FName MountId, FName ProfileId);
    UFUNCTION(BlueprintCallable) void NotifyFTLDeparture(AOVShip* Ship);
    UFUNCTION(BlueprintPure) FOVCombatShipState GetShipState(AOVShip* Ship) const;
    UFUNCTION(BlueprintPure) FOVDetectionInfo GetDetection(AOVShip* Ship, AOVShip* Target) const;
    UFUNCTION(BlueprintPure) FOVFireReadiness GetReadiness(AOVShip* Ship, FName MountId) const;
    UFUNCTION(BlueprintPure) bool IsDeparted(AOVShip* Ship) const;
    UFUNCTION(BlueprintCallable) bool ApplyInterceptionDamage(int32 AttackId, float Damage);
    // Reuses immutable damage metadata; resolved attacks cannot deliver damage twice.
    bool ResolveImpact(FOVAttackState& Attack, AOVShip* HitShip, FVector Point, FVector Normal, FName HitWeaponId = NAME_None);
    static bool SegmentBox(FVector Start, FVector End, const FTransform& Transform,
        FVector HalfExtent, float& HitFraction, FVector& HitNormal);
private:
    bool ResolveAimPoint(const FOVCombatShipState& State, AOVShip* Target, FVector& Point) const;
    void ClearPending(FOVCombatShipState& State);
    double Accumulator = 0;
    FRandomStream Random;
    int32 FindShip(AOVShip* Ship) const;
    TSet<int32> ResolvedAttackIds;
    int32 FindMount(const FOVCombatShipState& State, FName Id) const;
    bool IsValidTarget(AOVShip* Source, AOVShip* Target) const;
    FOVFireReadiness Evaluate(const FOVCombatShipState& State, const FOVCombatMountState& Mount, bool bIgnoreCooldown) const;
    void Step(float Delta);
    void UpdateAim(FOVCombatShipState& State, FOVCombatMountState& Mount, float Delta);
    void Launch(FOVCombatShipState& State, FOVCombatMountState& Mount, bool bAutomatic);
    void AdvanceAttacks(float Delta);
    bool SweepAttack(FOVAttackState& Attack, FVector End);
    void RecordEvent(const FOVAttackState& Attack);
    AOVShip* FindBlockingFriend(AOVShip* Ship, AOVShip* Target, FVector Origin) const;
};
