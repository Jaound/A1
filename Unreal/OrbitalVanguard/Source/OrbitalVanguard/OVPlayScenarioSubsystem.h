#pragma once

#include "CoreMinimal.h"
#include "OVSpaceEnvironmentTypes.h"
#include "OVHeroSubsystem.h"
#include "Subsystems/WorldSubsystem.h"
#include "OVPlayScenarioSubsystem.generated.h"

class AStaticMeshActor;
class AOVBlackHoleHazardActor;
class AOVController;
class AOVShip;

UENUM(BlueprintType)
enum class EOVCommandDifficulty : uint8 { Easy, Normal, Hard };

enum class EOVPlaySceneKind : uint8 { Fleet, Logistics, Evacuation, Sandbox, HeroArena, BaseTrial };

struct FOVPlayFleetEntry
{
    FName Hull;
    int32 Count;
    int32 AircraftPerHangar = 0;
    int32 AircraftBays = 2;
    int32 ReplacementStock = -1; // -1: 25 aircraft kits per installed bay
};

struct FOVSortieHull
{
    FOVPlayFleetEntry Entry;
    int32 Key=0;
    FVector Position=FVector::ZeroVector;
    float Yaw=0;
    TArray<FIntPoint> RoomSwaps;
    TMap<FName,FName> Weapons;
};
struct FOVSortieHero
{
    FName Definition; int32 HullKey=0;
    FName StartRoom, DutyRoom; bool bBoard=false;
};
struct FOVSortieDraft
{
    FName Mission;
    EOVSpaceHazardKind Map=EOVSpaceHazardKind::None;
    TArray<FOVSortieHull> Hulls;
    TArray<FOVSortieHero> Heroes;
    int32 NextKey=1;
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
    Running, Victory, Defeat, MutualDestruction, NoCombatCapability, Stalemate, TimeLimit, PartialVictory
};

UCLASS()
class ORBITALVANGUARD_API UOVPlayScenarioSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:

    UFUNCTION(BlueprintCallable) bool StartPractice(AOVController* PC, int32 Chapter);
    UFUNCTION(BlueprintCallable) bool AdvancePractice();
    UFUNCTION(BlueprintCallable) void ObservePractice();
    UPROPERTY(BlueprintReadOnly) int32 PracticeChapter = INDEX_NONE;
    UPROPERTY(BlueprintReadOnly) int32 PracticeStep = 0;
    UPROPERTY(BlueprintReadOnly) bool bPracticeReady = false;
    bool bPracticeHints = true;
    bool bPracticeDetail = false;
    static FString PracticeTitle(int32 Chapter);
    static FString PracticeSummary(int32 Chapter);
    static int32 CompletedPracticeMask();
    FString PracticeObjective() const;
    FString PracticeInput() const;
    FName PracticeControl() const;
    AOVShip* PracticeFocus() const;
    int32 PracticeStepCount() const;
    bool PracticeEquipment(bool Invalid);
    void ConfirmPracticeNotice(FName Subject);
    FVector PracticeGoal = FVector::ZeroVector;
    bool HasPracticeMarker() const;
private:
    void BeginPracticeStep();
    FVector PracticeOrigin, PracticeCamera;
    float PracticeAngle=0, PracticePitch=0, PracticeZoom=0;
    int32 PracticeShots=0, PracticeParticipant=INDEX_NONE;
    bool bPracticeOrder=false;
    TSet<int32> PracticeWing;
    bool bPreparingBoardingLesson=false;
    TWeakObjectPtr<AOVShip> PracticeShip, PracticeEnemy;
public:
    static bool IsBudgetMission(FName Id);
    UFUNCTION(BlueprintCallable) bool OpenSortie(AOVController* PC,FName Mission,bool Edit=false);
    UFUNCTION(BlueprintCallable) bool SortieCommand(FName Action,int32 Index=0);
    UFUNCTION(BlueprintCallable) bool SetSortiePosition(FVector Point,float Yaw);
    UFUNCTION(BlueprintPure) int32 SortieCost() const;
    UFUNCTION(BlueprintPure) int32 SortieBudget() const;
    UFUNCTION(BlueprintPure) int32 SortieHeroLimit() const;
    UFUNCTION(BlueprintPure) FString SortieProblem(int32 Stage) const;
    UFUNCTION(BlueprintCallable) bool CommitSortie();
    UFUNCTION(BlueprintPure) TArray<FName> SortieCatalog() const;
    UFUNCTION(BlueprintPure) int32 SortieFleetCount() const {return Draft.Hulls.Num();}
    UFUNCTION(BlueprintPure) FName SortieHull(int32 Index) const {return Draft.Hulls.IsValidIndex(Index)?Draft.Hulls[Index].Entry.Hull:NAME_None;}
    UFUNCTION(BlueprintPure) FVector SortieLocation(int32 Index) const {return Draft.Hulls.IsValidIndex(Index)?Draft.Hulls[Index].Position:FVector::ZeroVector;}
    UPROPERTY(BlueprintReadOnly) int32 SortieStage=0;
    UPROPERTY(BlueprintReadOnly) FString SortieFeedback;
    FOVSortieDraft Draft, LastSortie;
    int32 SortieSelected=0,SortieHero=0,SortieRoom=0,SortieMount=0,SortieDeck=1,SortieRosterIndex=0;
    bool bSortieExternal=false;
    int32 SortieSizeFilter=0,SortieRoleFilter=0;
    bool bSortieDetails=false;
    UFUNCTION(BlueprintCallable) AOVShip* InspectSortie(int32 Index);
    FString PlacementProblem(int32 Index,FVector Point) const;
    TArray<FName> SortieRoster() const;
private:
    TWeakObjectPtr<AOVShip> SortiePreview;
    int32 PreviewKey=0;
    bool bApplyingSortie=false;
    void ReleaseSortiePreview();
    bool FitSortieHull(AOVShip& Ship,const FOVSortieHull& Design) const;
    void AutoPlaceSortie();
    void RecommendSortie();
    bool ApplySortieHeroes(AOVController& PC);
public:
    static constexpr int32 ArenaHeroBerths=10;
    float MissionPanelHeight() const;
    int32 CurrentHeroBerths(const AOVShip* Ship) const;
    bool bArenaStarted=false;
    int32 ArenaRoute=0;
    int32 ArenaHeroQuota=4;
    bool bArenaFormationOpen=false;
    bool bArenaCustomFormation=false;
    int32 ArenaFormationPage=0;
    int32 ArenaSelectedGroup=0;
    // Each column contains hero IDs and the first soldier ID of each intact squad.
    TArray<int32> ArenaGroups[3];
    TMap<int32,TArray<int32>> ArenaMembers;
    void PresetArenaFormation();
    bool HandleArenaFormation(FName Action,AOVController& PC);
    FString ArenaGroupLabel(int32 Group) const;
    int32 ArenaGroupHeadcount(int32 Column) const;
    int32 ArenaTeamForUnit(int32 Id) const;
    struct FArenaWave { TArray<int32> Troopers; TArray<int32> Units; TWeakObjectPtr<AOVShip> Shuttle; FName Bay; bool Sent=false; bool Recalled=false; bool UsedShuttle=false; bool PlayerDirected=false; TWeakObjectPtr<AOVShip> ActualShuttle; };
    FArenaWave ArenaWaves[2];
    bool PrepareArenaForces(AOVController& PC);
    bool SendArenaWave(AOVController& PC,int32 Index);
    FString ArenaWaveStatus(int32 Index) const;
    void RecallArenaWave(AOVController& PC,int32 Index);
    void StepArena(AOVController& PC,float Delta);
    FString ArenaCandidateLabel() const;
    static TConstArrayView<FOVPlaySceneDefinition> Definitions();
    const FOVPlaySceneDefinition* GetActiveDefinition() const;
    bool IsArenaPreparing() const {return ActiveId==TEXT("HeroArena") && !bArenaStarted;}
    void SelectArenaHero(AOVController& PC,FName DefinitionId);
    bool Start(AOVController& Controller, FName Id);
    void Clear();
    virtual void Deinitialize() override;
    bool bSandboxPlacing = false;
    bool bSandboxHullMenu = false;
    UPROPERTY(BlueprintReadOnly) bool bSandboxEngaged = false;
    int32 SandboxCount = 1;
    bool bSandboxPointValid = false;
    bool bSandboxHasPoint = false;
    int32 SandboxFaction = 0;
    double SandboxAltitude = 2000;
    FName SandboxHull = TEXT("corvette");
    FVector SandboxPoint = FVector::ZeroVector;
    FString SandboxHint;
    bool UpdateSandboxPointer(AOVController& PC, FVector2D Pixel, bool bWorld);
    UFUNCTION(BlueprintCallable) bool PlaceSandboxShip(FVector Point);
    static FVector MiningSite();
    static FVector DeliverySite();
    static float EscortQuota();
    FString GetConvoyStatus() const;
    FString GetEvacuationStatus(AOVShip* Ship) const;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 EscapedShips = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 ArrivedWaves = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 AbandonedAircraft = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float DeliveredCargo = 0;
    bool HandleAction(AOVController& Controller, FName Action);
    void Step(float Delta);
    UPROPERTY(BlueprintReadWrite, Category="Scenario") EOVCommandDifficulty NextDifficulty = EOVCommandDifficulty::Normal;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) EOVCommandDifficulty ActiveDifficulty = EOVCommandDifficulty::Normal;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 EnemyEvasions = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 AlliedEvasions = 0;
    void CycleDifficulty();
    FString DifficultyLabel() const;

    /** Optional explicit time limit override in seconds (0 = disabled, default for D126 battles). */
    UFUNCTION(BlueprintCallable, Category = "Scenario")
    void SetOptionalTimeLimit(float Seconds) { CustomLimitSeconds = FMath::Max(0.0f, Seconds); }

    /** Optional environment hazard override applied to active or next scenario (e.g. BlackHole). */
    UFUNCTION(BlueprintCallable, Category = "Scenario")
    void SetEnvironmentOverride(EOVSpaceHazardKind Kind);

    UFUNCTION(BlueprintPure, Category = "Scenario")
    AOVBlackHoleHazardActor* GetActiveBlackHoleHazard() const { return ActiveBlackHoleActor.Get(); }

    UFUNCTION(BlueprintPure) FString GetStatus() const;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<FString> DebriefLines;
    FString GetOutcomeTitle() const;
    int32 SurvivingCost=0,SurvivingAircraft=0,DepartedCount=0;
    const FString& GetResolution() const { return Resolution; }

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
    void StepCommandAI(AOVController& PC, float Delta);
    void CommandEnemyHeroes(AOVController& PC);
    double NextCommandDecision[2] = {};
    struct FEvasionOrder { FVector Sidestep, PreviousGoal; bool HadGoal; double Until; };
    TMap<TWeakObjectPtr<AOVShip>,FEvasionOrder> Evasions;
    float DecisionRemaining = 0;
    double LastProgressAt = 0;
    double PreviousIntegrity = -1;
    FString Resolution;
    int32 IssuedHeroes[2]={};
    int32 ArenaCandidate=0;
    int32 ArenaLevel=1;
    TSet<FGuid> ArenaPicked;
    TArray<FOVHeroRecord> ArenaSavedRoster;
    TMap<int32,int32> ArenaSavedTiers;
    bool bArenaOwnsRoster=false;
    TWeakObjectPtr<UOVHeroSubsystem> ArenaRoster;
    void RestoreArenaRoster();
    TArray<FGuid> ArenaOwnedCandidates() const;
    bool AddArenaHero(AOVController& PC,int32 Side,int32 Choice);
    TArray<FGuid> ScenarioHeroes;
    void AssignScenarioHeroes(AOVController& PC,int32 Faction);

    TWeakObjectPtr<AStaticMeshActor> OreMarker;
    TArray<TWeakObjectPtr<AOVShip>> Miners;
    TArray<TWeakObjectPtr<AOVShip>> Haulers;
    TSet<TWeakObjectPtr<AOVShip>> DeliveryLeg;
    TMap<TWeakObjectPtr<AOVShip>,float> InitialOre;
    int32 OreNode = 0;
    bool bRaidLaunched = false;
    TArray<TWeakObjectPtr<AOVShip>> RescueFleet;
public:
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) float DischargedStores=0;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) float HandedMinedOre=0;
    float ChargeFraction(AOVShip* Ship) const;
    bool OrderJump(AOVController& PC,FName Destination);
    TMap<TWeakObjectPtr<AOVShip>,FName> JumpDestinations;
    struct FJumpTrail { FVector Start,End; double At; };
    TArray<FJumpTrail> JumpTrails;
private:
    void AdvanceJumps(AOVController& PC);
    TMap<TWeakObjectPtr<AOVShip>, float> ChargeSeconds;
    UPROPERTY(Transient) TArray<TObjectPtr<AStaticMeshActor>> SandboxGhosts;
    TArray<TWeakObjectPtr<AOVShip>> FreshDeployments;
    FVector SandboxExtent = FVector::ZeroVector;
    FVector SandboxPosition(FVector Center, int32 Index) const;
    bool BeginSandboxPlacement();
    void EndSandboxPlacement();
    bool SandboxAction(AOVController& PC, FName Name);
    bool CanPlaceSandboxShip(const AOVController& PC, FVector Point) const;
    void StepEvacuation(AOVController& Controller, float Delta);
    bool EvacuationOrder(AOVController& Controller, FName Action);
    void ResolveEvacuation(AOVController& Controller);
    bool StartEscort(AOVController& Controller);
    void StepEscort(AOVController& Controller, float Delta);
    void OrderConvoyLeg(AOVShip& Ship, int32 Lane, bool bMiner);
    void SyncEnvironmentHazard(AOVController& Controller, EOVSpaceHazardKind HazardKind, const FVector& Location);
    void DestroyActiveHazardActors();
    AOVShip* Spawn(AOVController& Controller, const FOVPlayFleetEntry& Entry, int32 Faction, int32 Ordinal, TOptional<FVector> PositionOverride = {});
    void CheckFleetResult(AOVController& Controller, const FOVPlaySceneDefinition& Definition);
    void Finish(AOVController& Controller, EOVPlaySceneResult Outcome, const TCHAR* Reason);
};
