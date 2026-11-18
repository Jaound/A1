#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "OVPlayScenarioSubsystem.generated.h"

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
    float LimitSeconds = 900;
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
    UFUNCTION(BlueprintPure) FString GetStatus() const;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName ActiveId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) EOVPlaySceneResult Result = EOVPlaySceneResult::Running;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) double ElapsedSeconds = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<int32> InitialCosts;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<int32> InitialAircraft;
private:
    UPROPERTY() TWeakObjectPtr<AOVController> Owner;
    float DecisionRemaining = 0;
    double LastProgressAt = 0;
    double PreviousIntegrity = -1;
    FString Resolution;
    TSet<TWeakObjectPtr<AOVShip>> InitialSorties;
    AOVShip* Spawn(AOVController& Controller, const FOVPlayFleetEntry& Entry, int32 Faction, int32 Ordinal);
    void CheckFleetResult(AOVController& Controller, const FOVPlaySceneDefinition& Definition);
    void Finish(AOVController& Controller, EOVPlaySceneResult Outcome, const TCHAR* Reason);
};
