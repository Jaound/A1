#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "OVBattleNotices.generated.h"

class AOVShip;
struct FOVAttackState;
struct FOVInteriorUnitRecord;

UENUM(BlueprintType)
enum class EOVBattleNoticeKind : uint8 { Hit, ShipWrecked, ShieldDepleted, ArmorDestroyed, FacilityDestroyed, ShieldRestored, HeroDeath, BoardingEntry, BoardingReturn, FacilityCaptured, HostileBoarding, HeroHit, HeroRescue, AircraftHalf, AircraftCritical, AircraftEliminated, AircraftAdjusted };

USTRUCT(BlueprintType)
struct ORBITALVANGUARD_API FOVBattleNotice
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FString ShipName;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName ShipId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FVector LastPosition=FVector::ZeroVector;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) double GameTime=0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 Count=0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 UnitId=0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 Remaining=0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 Baseline=0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bFleetScope=false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 CrossedThresholdMask=0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bShipWrecked=false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bCritical=false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) EOVBattleNoticeKind Kind=EOVBattleNoticeKind::Hit;
    double DisplayTime=0;
};

/** Read-only, bounded presentation of confirmed combat events; never applies damage. */
UCLASS()
class ORBITALVANGUARD_API UOVBattleNotices : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<FOVBattleNotice> Notices;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 DroppedCount=0;
    double LastCriticalDisplayTime=-1000;
    void Reset();
    void RecordCombatHit(const FOVAttackState& Attack,AOVShip* Victim);
    void RecordShieldRestored(AOVShip* Ship,double GameTime);
    void RecordInteriorEvent(FName EventKind,const FOVInteriorUnitRecord& Unit,double GameTime);
    void RecordHeroRescue(int32 Faction,int32 UnitId,FVector Position,double GameTime);
    void RecordAircraftThreshold(AOVShip* Carrier,bool bFleet,int32 PreviousSeverity,int32 Severity,int32 Remaining,int32 Baseline,FVector Position,double GameTime);
    void RecordAircraftAdjustment(AOVShip* Carrier,int32 Remaining,int32 Baseline,double GameTime);
private:
    TArray<int32> RecentAttackIds;
    void Append(const FOVAttackState& Attack,AOVShip* Victim,EOVBattleNoticeKind Kind,int32 Count,int32 UnitId=0);
};
