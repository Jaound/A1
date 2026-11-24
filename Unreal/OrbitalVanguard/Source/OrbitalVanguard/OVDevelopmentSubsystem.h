#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "OVDevelopmentSubsystem.generated.h"
class AOVShip;

/** Explicit sandbox commands. Widgets only query state and call these validated operations. */
UCLASS()
class ORBITALVANGUARD_API UOVDevelopmentSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FString LastMessage;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<TObjectPtr<AOVShip>> SpawnedShips;
    UFUNCTION(BlueprintCallable) bool OpenRosterRoom();
    UFUNCTION(BlueprintCallable) TArray<AOVShip*> SpawnShips(FName DefinitionId, int32 Faction, int32 Count,
        FVector Location, float Spacing, int32 AircraftPerHangar, int32 Troops, bool bMoveAuto, bool bFireAuto);
    UFUNCTION(BlueprintCallable) bool DeleteShip(AOVShip* Ship);
    UFUNCTION(BlueprintCallable) bool EquipWeapon(AOVShip* Ship, FName MountId, FName ProfileId);
    UFUNCTION(BlueprintCallable) bool SetBF120Trial(AOVShip* Ship, FName MountId, bool bEnable);
    UFUNCTION(BlueprintCallable) bool SetShipTarget(AOVShip* Ship, AOVShip* Target);
    UFUNCTION(BlueprintCallable) bool FireWeapon(AOVShip* Ship, FName MountId);
    UFUNCTION(BlueprintPure) FString GetShipDetails(AOVShip* Ship) const;
    static bool HasPlacementClearance(const class AOVController& PC, FVector Position, double Radius);
private:
    friend class UOVPlayScenarioSubsystem;
    friend class AOVBasePresentation;
    TArray<AOVShip*> SpawnTrialShips(FName Id, int32 Faction, int32 Count, FVector Location, float Spacing,
        int32 AircraftPerHangar, int32 Troops, bool MoveAuto, bool FireAuto, bool bPlayerRange, bool bNewBuild=false);
    bool RemoveTrialShip(AOVShip* Ship);
};
