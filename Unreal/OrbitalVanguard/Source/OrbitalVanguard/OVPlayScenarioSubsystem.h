#pragma once

#include "CoreMinimal.h"
#include "OVSpaceEnvironmentTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "OVPlayScenarioSubsystem.generated.h"

class AOVBlackHoleHazardActor;
class AOVController;
class AOVShip;

enum class EOVPlaySceneKind : uint8 { Fleet, Logistics, Evacuation, Sandbox };

struct FOVPlayFleetEntry
{
    FName Hull;
    int32 Count;
    int32 AircraftPerHangar = 0;
};

struct FOVPlaySceneDefinition
{
    FName Id;
    const TCHAR* Title;
    const TCHAR* Objective;
    EOVPlaySceneKind Kind;
    int32 FleetBudget = 0;
    TArray<FOVPlayFleetEntry> Fleet;
    bool bAvailable = true;
    float StalemateSeconds = 120;
    // D126 battles last 18~26.9 min; 0 disables forced time-limit termination unless explicitly enabled per scenario.
    float LimitSeconds = 0;
    EOVSpaceHazardKind EnvironmentHazard = EOVSpaceHazardKind::None;
    FVector HazardLocation = FVector(0.0f, 0.0f, 2000.0f);
};

UENUM(BlueprintType)
enum class EOVPlaySceneResult : uint8
{
    Running, Victory, Defeat, MutualDestruction, NoCombatCapability, Stalemate, TimeLimit
};

UCLASS()
class ORBITALVANGUARD_API UOVPlayScenarioSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    static TConstArrayView<FOVPlaySceneDefinition> Definitions();
    const FOVPlaySceneDefinition* GetActiveDefinition() const;
    bool Start(AOVController& Controller, FName Id);
    void Clear();
    bool HandleAction(AOVController& Controller, FName Action);
    void Step(float Delta);

    /** Optional explicit time limit override in seconds (0 = disabled, default for D126 battles). */
    UFUNCTION(BlueprintCallable, Category = "Scenario")
    void SetOptionalTimeLimit(float Seconds) { CustomLimitSeconds = FMath::Max(0.0f, Seconds); }

    /** Optional environment hazard override applied to active or next scenario (e.g. BlackHole). */
    UFUNCTION(BlueprintCallable, Category = "Scenario")
    void SetEnvironmentOverride(EOVSpaceHazardKind Kind);

    UFUNCTION(BlueprintPure, Category = "Scenario")
    AOVBlackHoleHazardActor* GetActiveBlackHoleHazard() const { return ActiveBlackHoleActor.Get(); }

    UFUNCTION(BlueprintPure) FString GetStatus() const;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName ActiveId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) EOVPlaySceneResult Result = EOVPlaySceneResult::Running;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) double ElapsedSeconds = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<int32> InitialCosts;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<int32> InitialAircraft;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float CustomLimitSeconds = 0.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) EOVSpaceHazardKind EnvironmentOverride = EOVSpaceHazardKind::None;

private:
    UPROPERTY() TWeakObjectPtr<AOVController> Owner;
    UPROPERTY() TWeakObjectPtr<AOVBlackHoleHazardActor> ActiveBlackHoleActor;
    float DecisionRemaining = 0;
    double LastProgressAt = 0;
    double PreviousIntegrity = -1;
    FString Resolution;
    TSet<TWeakObjectPtr<AOVShip>> InitialSorties;

    void SyncEnvironmentHazard(AOVController& Controller, EOVSpaceHazardKind HazardKind, const FVector& Location);
    void DestroyActiveHazardActors();
    AOVShip* Spawn(AOVController& Controller, const FOVPlayFleetEntry& Entry, int32 Faction, int32 Ordinal);
    void CheckFleetResult(AOVController& Controller, const FOVPlaySceneDefinition& Definition);
    void Finish(AOVController& Controller, EOVPlaySceneResult Outcome, const TCHAR* Reason);
};
