#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "OVInteriorSubsystem.h"
#include "OVMissionSubsystem.generated.h"

class AOVController;
class AOVShip;

UENUM(BlueprintType)
enum class EOVMissionMode : uint8 { Attack, Defense, FleetOnly };
UENUM(BlueprintType)
enum class EOVMissionPhase : uint8 { Battle, ShieldWindow, Transit, Interior, Extracting, Completed };
UENUM(BlueprintType)
enum class EOVMissionResult : uint8 { Ongoing, Victory, Defeat, Withdrawn, Unresolved };

USTRUCT(BlueprintType)
struct ORBITALVANGUARD_API FOVMissionProfile
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ProfileId=TEXT("IntegratedMission_D116_Trial_v1");
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float DecisionInterval=.1f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 BoardingTroops=6;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 GuardsPerFacility=4;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float CarrierSeparation=24000;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float AutoWithdrawHullFraction=.25f;
};

USTRUCT(BlueprintType)
struct ORBITALVANGUARD_API FOVMissionFactionScore
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 FactionId=-1;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 InitialCost=31;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 LivingCost=0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 ShipsLost=0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 Heroes=0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 HeroesDead=0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 HeroesRescueWaiting=0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 HeroesWounded=0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 Troops=0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 TroopsDead=0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 AircraftLost=0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 AircraftSurviving=0;
};

USTRUCT(BlueprintType)
struct ORBITALVANGUARD_API FOVMissionScore
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) double GameTime=0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) double ShieldBreakAt=-1;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) double FirstEntryAt=-1;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) double ShieldOperationAt=-1;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) double BridgeCaptureAt=-1;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) double RetreatAt=-1;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) double LastReturnAt=-1;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) double FinishedAt=-1;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 Launched=0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 Entered=0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 Returned=0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 ReturnedAfterEntry=0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 AliveWhenRetreatOrdered=0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 ShieldReactivations=0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<FOVMissionFactionScore> Factions;
};

/** 정상 명령과 실제 시스템 결과를 연결한다. 별도 Tick/DEV 성공/무료 보충이 없다. */
UCLASS()
class ORBITALVANGUARD_API UOVMissionSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bEnabled=false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FOVMissionProfile Settings;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) EOVMissionMode Mode=EOVMissionMode::Attack;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) EOVMissionPhase Phase=EOVMissionPhase::Battle;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) EOVMissionResult Result=EOVMissionResult::Ongoing;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FString Status;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 Seed=260112;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 AttackHeroCount=4;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 DefenseHeroCount=2;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) double GameTime=0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bUseShuttle=false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bAutoPlayer=false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<AOVShip> Origin;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<AOVShip> Objective;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<AOVShip> Shuttle;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName ShieldSlot;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName BridgeSlot;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<int32> AttackUnitIds;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<int32> DefenderUnitIds;
    UFUNCTION(BlueprintCallable) bool StartMission(AOVController* Controller, EOVMissionMode InMode=EOVMissionMode::Attack,
        int32 InSeed=260112, int32 AttackHeroes=4, int32 DefenseHeroes=2, bool bShuttle=false);
    UFUNCTION(BlueprintCallable) bool StartRosterMission(AOVController* Controller, bool bDefense=false, int32 InSeed=260112, bool bShuttle=false);
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bUsesOwnedRoster=false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FString BattleId;
    UFUNCTION(BlueprintCallable) void Disable();
    UFUNCTION(BlueprintCallable) void Simulate(float GameDelta);
    UFUNCTION(BlueprintCallable) bool HandleAction(FName Action);
    UFUNCTION(BlueprintCallable) bool Dispatch(bool bShuttle);
    UFUNCTION(BlueprintCallable) bool OrderTeam(EOVInteriorOrder Order,FName Slot=NAME_None);
    UFUNCTION(BlueprintCallable) void SetAutoPlayer(bool bAuto);
    /** Trial mission AI preserves its assault objective after dispatch; manual fire is unaffected. */
    bool ShouldHoldAutoAssaultFire(const AOVShip* Source,const AOVShip* Target) const;
    UFUNCTION(BlueprintPure) TArray<int32> GetPlayerUnitIds() const;
    /** 전체 결과는 검증/임무 종료용. 진행 중 HUD는 적 로스터 수치를 공개하면 안 된다. */
    UFUNCTION(BlueprintPure) FOVMissionScore GetScore() const;
    UFUNCTION(BlueprintPure) float GetHeroParticipationSeconds(FGuid HeroId) const;
    UFUNCTION(BlueprintPure) int32 GetHeroObjectiveXP(FGuid HeroId) const;
    UFUNCTION(BlueprintPure) FString GetPublicStatus() const;
    UFUNCTION(BlueprintCallable) void MarkComparisonTimeout();
private:
    UPROPERTY() TWeakObjectPtr<AOVController> Owner;
    UPROPERTY() TArray<TObjectPtr<AOVShip>> Roster;
    FOVMissionScore Score;
    TSet<int32> LaunchedIds,EnteredIds,ReturnedIds,AutoOrderedIds;
    float DecisionRemaining=0;
    int32 AttackerFaction=0;
    bool bDispatched=false;
    bool bRetreatOrdered=false;
    bool bWasShieldActive=true;
    bool bAutoBridgeOrdered=false;
    bool bDefenderRetakeOrdered=false;
    bool bStarting=false;
    TMap<FGuid,float> HeroParticipation;
    TMap<FGuid,int32> HeroObjectiveXP;
    int32 LastHeroEvent=0;
    void TrackHeroes(float Delta);
    bool IsObserved(AOVShip* Source,AOVShip* Target) const;
    void RegisterGuards(AOVShip* Ship,int32 Heroes,TArray<int32>* OutIds=nullptr);
    void RefreshMetrics();
    void AdvanceOrders();
    void Finish(EOVMissionResult Outcome,const FString& Message);
};
