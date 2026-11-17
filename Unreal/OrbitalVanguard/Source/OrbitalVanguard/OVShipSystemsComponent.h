#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "OVShipLayout.h"
#include "OVShipSystemsComponent.generated.h"

UENUM(BlueprintType)
enum class EOVDamageCause : uint8 { External, Explosion, Sabotage };

/** Legacy serialized enum; D083 now always uses inactive-only gradual recovery. */
UENUM(BlueprintType)
enum class EOVShieldRecoveryPolicy : uint8 { Gradual, FullAfterNoHitInterval };

USTRUCT(BlueprintType)
struct ORBITALVANGUARD_API FOVDamageTuning
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Provisional") float AdditionalShieldCapacity = .5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Provisional") float CrewDamageCoefficient = .20f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Provisional") float ShieldBase = 300;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Provisional") float ShieldRegenFraction = 0.006f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Provisional") float ShieldNonCombatRegenFraction = 0.018f;
    // Legacy serialized field ignored under D116.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Provisional") float ShieldRecoveryDelay = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Provisional") float NoHitRecoveryInterval = 15;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Provisional") EOVShieldRecoveryPolicy RecoveryPolicy = EOVShieldRecoveryPolicy::Gradual;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Provisional") bool bAllowManualShieldToggle = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Provisional") float ShieldOverflowFraction = 0.25f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Provisional") float ArmorMaxReduction = 0.6f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Provisional") float FrontArmorMaxReduction = 0.75f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Provisional") float ArmorWear = 0.3f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Provisional") float FacilityHPToHull = 0.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Provisional") float ExplosionBase = 70;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Provisional") float ExplosionTierCoefficient = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Provisional") float RadarAccuracyBase = 0.1f;
};

USTRUCT(BlueprintType)
struct ORBITALVANGUARD_API FOVFacilityState
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="State") FName SlotId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="State") float HP = 0;
    /** Compatibility display: minimum linked armor HP; ArmorSections is authoritative. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="State") float Armor = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="State") float Crew = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="State") int32 OccupyingFaction = -1;
};

USTRUCT(BlueprintType)
struct ORBITALVANGUARD_API FOVArmorSectionState
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="State") FName ArmorSectionId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="State") float HP = 0;
};

USTRUCT(BlueprintType)
struct ORBITALVANGUARD_API FOVWeaponMountState
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="State") FName WeaponMountId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="State") float HP = 0;
};

/** Per-vessel runtime state. Only the controller's supplied simulation time advances it. */
UCLASS(Config=Game, DefaultConfig, ClassGroup=(OrbitalVanguard), meta=(BlueprintSpawnableComponent))
class ORBITALVANGUARD_API UOVShipSystemsComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UOVShipSystemsComponent();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Systems") TObjectPtr<UShipLayoutDataAsset> Layout;
    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Systems") FOVDamageTuning Tuning;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Systems") TArray<FOVFacilityState> Slots;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Systems") TArray<FOVArmorSectionState> ArmorSections;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Systems") TArray<FOVWeaponMountState> WeaponMountStates;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Systems") int32 FactionId = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Systems") float HullHP = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Systems") float MaxHullHP = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Systems") float Shield = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Systems") float MaxShield = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Systems") float ShieldRegen = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Systems") float ShieldCrewEfficiency = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Systems") float ShieldRecoveryRemaining = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Systems") float TimeSinceExternalHit = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Systems") bool bShieldActive = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Systems") bool bShieldManualOff = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Systems") bool bWrecked = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Systems") bool bNeutral = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Systems") bool bBridgeCaptured = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Systems") bool bCapturedBridgeDestroyed = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Systems") int32 CaptureCount = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Diagnostics") int32 WreckEventCount = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Diagnostics") int32 LastExplosionCount = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Diagnostics") int32 LastDamageIndex = INDEX_NONE;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Diagnostics") FName LastDamageSlot;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Diagnostics") FName LastArmorSection;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Diagnostics") FName LastWeaponMount;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Diagnostics") FVector LastWorldHit = FVector::ZeroVector;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Diagnostics") FVector LastLocalHit = FVector::ZeroVector;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Diagnostics") FVector LastLocalNormal = FVector::ZeroVector;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Diagnostics") EOVDamageCause LastCause = EOVDamageCause::External;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Diagnostics") FName LastDamageSource;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Diagnostics") FName IncomingSourceId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Diagnostics") FName IncomingWeaponId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Diagnostics") int32 IncomingFactionId = -1;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Diagnostics") int32 HitEventId = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Diagnostics") float LastHullDamage = 0;

    UFUNCTION(BlueprintCallable, Category="Systems") void Initialize(UShipLayoutDataAsset* InLayout);
    UFUNCTION(BlueprintCallable, Category="Systems") void ResetState();
    UFUNCTION(BlueprintCallable, Category="Systems") void Simulate(float SimDelta);
    UFUNCTION(BlueprintPure, Category="Systems") float GetEfficiency(EOVFacilityType Type) const;
    UFUNCTION(BlueprintPure, Category="Systems") bool CanMove() const;
    UFUNCTION(BlueprintPure, Category="Systems") bool CanCommand() const;
    UFUNCTION(BlueprintPure, Category="Systems") bool CanFireCannons() const;
    UFUNCTION(BlueprintPure, Category="Systems") bool CanFireBeams() const;
    UFUNCTION(BlueprintPure, Category="Systems") bool CanFireMissiles() const;
    UFUNCTION(BlueprintPure, Category="Systems") bool HasLifeSupport() const;
    UFUNCTION(BlueprintPure, Category="Systems") bool HasRadar() const;
    UFUNCTION(BlueprintPure, Category="Systems") bool CanIntercept() const;
    UFUNCTION(BlueprintPure, Category="Systems") bool CanLaunchFighters() const;
    UFUNCTION(BlueprintPure, Category="Systems") bool CanUseHangar() const;
    UFUNCTION(BlueprintPure, Category="Systems") bool CanUseResources() const;
    UFUNCTION(BlueprintPure, Category="Systems") float GetRadarRange() const;
    UFUNCTION(BlueprintPure, Category="Systems") float GetRadarAccuracyMultiplier() const;
    UFUNCTION(BlueprintPure, Category="Systems") FOVFacilityState GetSlotState(FName SlotId) const;
    UFUNCTION(BlueprintPure, Category="Systems") FString GetSlotStatus(FName SlotId) const;
    UFUNCTION(BlueprintCallable, Category="Damage") void ApplyWorldHit(FVector WorldPoint, FVector WorldNormal, float Damage, EOVDamageCause Cause, bool bCombatAttack = false);
    /** D089: the external shield protects mounts; remaining damage never spills into the hull. */
    UFUNCTION(BlueprintCallable, Category="Damage") void ApplyWorldWeaponHit(FName WeaponMountId, FVector WorldPoint, FVector WorldNormal, float Damage, bool bCombatAttack = false);
    UFUNCTION(BlueprintCallable, Category="Debug") void DebugDestroySlot(FName SlotId, EOVDamageCause Cause);
    UFUNCTION(BlueprintCallable, Category="Debug") void DebugRepairSlot(FName SlotId);
    UFUNCTION(BlueprintCallable, Category="Debug") void DebugSetCrew(FName SlotId, float Crew);
    UFUNCTION(BlueprintCallable, Category="Debug") void DebugSetOccupation(FName SlotId, int32 OccupyingFaction);
    UFUNCTION(BlueprintCallable, Category="Debug") void DebugSetArmor(FName ArmorSectionId, float HP);
    UFUNCTION(BlueprintCallable, Category="Debug") void DebugHitWeapon(FName WeaponMountId, float Damage);
    UFUNCTION(BlueprintCallable, Category="Debug") void DebugRepairWeapon(FName WeaponMountId);
    UFUNCTION(BlueprintPure, Category="Systems") bool IsWeaponAvailable(FName WeaponMountId) const;
    UFUNCTION(BlueprintCallable, Category="Debug") bool SetShieldManualOff(bool bOff);
    UFUNCTION(BlueprintCallable, Category="Debug") void RefreshDerivedState();

    // Read-only estimate for a facility before damage is applied.
    float GetExplosionDamage(int32 SlotIndex) const;
    void NotifyCombatParticipation();
    // Authoritative faction-specific adapter for future interior operations. No roster is synthesized.
    UFUNCTION(BlueprintCallable) void SetInteriorPresence(int32 Faction, bool bPresent, bool bObjectiveComplete, bool bDeveloperInput = false);
    UFUNCTION(BlueprintPure) bool HasInteriorPresence(int32 Faction) const;
    UFUNCTION(BlueprintPure) bool IsInteriorObjectiveComplete(int32 Faction) const;
    UFUNCTION(BlueprintPure) bool IsInteriorDeveloperInput(int32 Faction) const;
    // Runtime interior authority is separate from the retained DEV/transport adapter.
    UFUNCTION(BlueprintCallable) void SetRuntimeInteriorPresence(int32 Faction, bool bPresent, bool bObjectiveComplete);
    UFUNCTION(BlueprintCallable) bool CompleteInteriorOperation(FName SlotId, int32 ActingFaction, bool bSabotage);
    bool HasLegacyInteriorPresence(int32 Faction) const { return !bWrecked && InteriorObjectives.Contains(Faction); }
    bool IsLegacyInteriorObjectiveComplete(int32 Faction) const { const bool* V=InteriorObjectives.Find(Faction); return V && *V; }
    bool IsLegacyInteriorDeveloperInput(int32 Faction) const { return InteriorDeveloperInputs.Contains(Faction); }


private:
    TMap<int32, bool> InteriorObjectives;
    TSet<int32> InteriorDeveloperInputs;
    TMap<int32, bool> RuntimeInteriorObjectives;
    bool bProcessingCombatDamage = false;
    bool IsOperational(int32 Index) const;
    bool CanAct() const;
    void MarkWrecked();
    void NotifyShieldActivated();
    void SyncArmorDisplay();
    void BeginDamageEvent(EOVDamageCause Cause, FName Source);
    float AbsorbExternalDamage(float Damage);
    void ProcessDamage(int32 SlotIndex, int32 ArmorIndex, float Damage, EOVDamageCause Cause, bool bBypassArmor, bool bDirectHull);
    void DestroyFacility(int32 SlotIndex, EOVDamageCause Cause, TArray<int32>& ExplosionQueue);
    float ExplosionDamage(int32 SlotIndex) const;
};
