#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "OVLogisticsSubsystem.generated.h"
class AOVShip;

// Development-only, explicitly supplied quantities; no campaign economy is synthesized.
UENUM(BlueprintType)
enum class EOVDevLogisticsRole : uint8 { Repair, Mining, Transport };

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

/** Explicit finite DEV operations. Caller controls simulation timing and supplies candidate prices. */
UCLASS()
class ORBITALVANGUARD_API UOVLogisticsSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<FOVDevCargo> Cargo;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<FOVDevResourceNode> Nodes;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FString LastReason;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<FOVDevRepairLink> RepairLinks;
    UFUNCTION(BlueprintCallable) bool RegisterDevShip(AOVShip* Ship, EOVDevLogisticsRole Role, float Capacity, float InitialAmount);
    UFUNCTION(BlueprintCallable) void RemoveDevShip(AOVShip* Ship);
    UFUNCTION(BlueprintCallable) int32 AddDevNode(FVector Position, float Amount);
    UFUNCTION(BlueprintCallable) float DevMine(AOVShip* Ship, int32 NodeId, float Requested, float MaxRange);
    UFUNCTION(BlueprintCallable) float DevTransfer(AOVShip* From, AOVShip* To, float Requested, float MaxRange);
    UFUNCTION(BlueprintCallable) bool SetDevMaterials(AOVShip* Ship, FVector Materials);
    // Explicit DEV base quote per HP. Field cost is 1.5 times this quote.
    UFUNCTION(BlueprintCallable) bool StartDevRepair(AOVShip* Source, AOVShip* Target, bool bHero, FGuid HeroId, FVector BaseCostPerHP, float Range, float HeroRate, float HeroDuration, float HeroTotalHP);
    UFUNCTION(BlueprintCallable) bool CancelDevRepair(AOVShip* Target, FGuid HeroId);
    UFUNCTION(BlueprintCallable) void Simulate(float GameDelta);
    UFUNCTION(BlueprintCallable) float DevRepairHull(AOVShip* RepairShip, AOVShip* Target, float RequestedHP, float CostPerHP, float MaxRange);
private:
    int32 NextNodeId = 1;
};
