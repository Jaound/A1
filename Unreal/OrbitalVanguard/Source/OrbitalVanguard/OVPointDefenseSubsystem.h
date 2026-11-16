#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "OVPointDefenseSubsystem.generated.h"

class AOVShip;

// Deliberately no Ship entry: assault transports are regular anti-ship targets.
UENUM(BlueprintType)
enum class EOVPDTargetKind : uint8 { Missile, Aircraft, Pod };
UENUM(BlueprintType)
enum class EOVPDWeaponKind : uint8 { Beam, Kinetic };

/** D091 relationship is fixed; absolute values are the explicitly provisional 2C profile. */
USTRUCT(BlueprintType)
struct ORBITALVANGUARD_API FOVPDProfile
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ProfileId = TEXT("Trial2C_Beam");
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EOVPDWeaponKind Kind = EOVPDWeaponKind::Beam;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Range = 11100;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Accuracy = .72f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Interval = .5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Damage = 12;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float MountHP = 100;
};

USTRUCT(BlueprintType)
struct ORBITALVANGUARD_API FOVPDMountState
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName MountId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FOVPDProfile Profile;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FVector LocalPosition = FVector::ZeroVector;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float HP = 100;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float Cooldown = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 ShotCount = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bLayoutMount = false;
};

USTRUCT(BlueprintType)
struct ORBITALVANGUARD_API FOVPDShipState
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<AOVShip> Ship;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<FOVPDMountState> Mounts;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<EOVPDTargetKind> Priority;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float RangeScale = 1;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FString Status;
};

USTRUCT(BlueprintType)
struct ORBITALVANGUARD_API FOVPDEvent
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 EventId = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) double GameTime = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName DefenderId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 DefenderFaction = -1;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName MountId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) EOVPDWeaponKind WeaponKind = EOVPDWeaponKind::Beam;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) EOVPDTargetKind TargetKind = EOVPDTargetKind::Missile;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 TargetId = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FVector Origin = FVector::ZeroVector;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FVector TargetPosition = FVector::ZeroVector;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float HitProbability = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bHit = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bDestroyed = false;
};

/** Called by the shared combat fixed step; never ticks independently or advances paused time. */
UCLASS()
class ORBITALVANGUARD_API UOVPointDefenseSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bEnabled = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) double GameTime = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 ShotCount = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 HitCount = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 KillCount = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<FOVPDShipState> Ships;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<FOVPDEvent> RecentEvents;
    UFUNCTION(BlueprintCallable) void Reset(int32 Seed = 260112);
    UFUNCTION(BlueprintCallable) void Disable();
    // Registration is idempotent. Re-registering cannot heal mounts/reset their firing opportunities.
    UFUNCTION(BlueprintCallable) bool RegisterShip(AOVShip* Ship, int32 MountCount, float RangeScale = 1);
    UFUNCTION(BlueprintCallable) bool SetPriority(AOVShip* Ship, const TArray<EOVPDTargetKind>& Priority);
    UFUNCTION(BlueprintPure) FOVPDShipState GetShipState(AOVShip* Ship) const;
    UFUNCTION(BlueprintPure) FString GetReadiness(AOVShip* Ship) const;
    UFUNCTION(BlueprintCallable) void Step(float GameDelta);
    UFUNCTION(BlueprintPure) static float HitProbability(float Accuracy, float Evasion);
    static FOVPDProfile DefaultProfile(EOVPDWeaponKind Kind);
private:
    FRandomStream Random;
};
