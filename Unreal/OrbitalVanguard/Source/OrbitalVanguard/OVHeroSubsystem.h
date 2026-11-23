#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameFramework/SaveGame.h"
#include "OVInteriorSubsystem.h"
#include "OVHeroSubsystem.generated.h"

UENUM(BlueprintType)
enum class EOVHeroStatus : uint8 { Reserve, Deployed, RescueWaiting, Captured, Missing, Dead };

USTRUCT(BlueprintType)
struct FOVHeroDefinition
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FName Id;
    UPROPERTY(BlueprintReadOnly) FString Name;
    UPROPERTY(BlueprintReadOnly) EOVInteriorRole Role=EOVInteriorRole::Melee;
    UPROPERTY(BlueprintReadOnly) FString Origin;
    UPROPERTY(BlueprintReadOnly) FString Story;
    UPROPERTY(BlueprintReadOnly) FString Passive;
    UPROPERTY(BlueprintReadOnly) FString Signature;
    UPROPERTY(Transient) TObjectPtr<UTexture2D> Portrait;
    UPROPERTY(Transient) TObjectPtr<UTexture2D> Standing;
    UPROPERTY(Transient) TObjectPtr<UTexture2D> SD;
    FVector2D Foot=FVector2D(.5,.95);
    float Scale=1;
};

/** D114 v0.5 성장/정원은 실게임 검증 전 후보. HeroId는 개인, UnitId/PersonId는 장면 실행 ID다. */
USTRUCT(BlueprintType)
struct ORBITALVANGUARD_API FOVHeroRecord
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FGuid HeroId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName DefinitionId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 FactionId = -1;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FString Name;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) EOVInteriorRole Role = EOVInteriorRole::Melee;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) EOVHeroStatus Status = EOVHeroStatus::Reserve;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bDeploymentReserved = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 FocusTier=0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 ArmorTier=0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 TacticalTier=0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName TacticalMode;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 XP = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 Level = 1;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 SkillPoints = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<int32> SkillRanks;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 Catalysts = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 AmplifierCharges = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FVector DevRepairBaseCost = FVector::ZeroVector;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float CurrentHP = -1;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bWounded = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bSkill4Armed = true;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<float> CooldownRemaining;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<FString> AwardedBattleIds;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 RuntimeUnitId = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 RuntimePersonId = 0;
};

UCLASS()
class ORBITALVANGUARD_API UOVHeroSaveGame : public USaveGame
{
    GENERATED_BODY()
public:
    UPROPERTY() int32 Version = 2;
    UPROPERTY() TMap<int32,int32> CommandTiers;
    UPROPERTY() TArray<FOVHeroRecord> Heroes;
};

UCLASS()
class ORBITALVANGUARD_API UOVHeroSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    UPROPERTY(BlueprintReadOnly) TMap<int32,int32> CommandTiers;
    UFUNCTION(BlueprintPure) int32 DeploymentLimit(int32 Faction) const;
    UFUNCTION(BlueprintCallable) bool SetTrialCommandTier(int32 Faction,int32 Tier);
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    UPROPERTY(BlueprintReadOnly) TArray<FOVHeroDefinition> Definitions;
    const FOVHeroDefinition* Definition(FName Id) const;
    UFUNCTION(BlueprintCallable) FGuid CreateDefinedHero(int32 FactionId,FName DefinitionId);
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName ProfileId = TEXT("Hero_D114_v05_Candidate");
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<FOVHeroRecord> Heroes;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FString LastMessage;
    UFUNCTION(BlueprintCallable) FGuid CreateDevHero(int32 FactionId, EOVInteriorRole Role, const FString& Name);
    UFUNCTION(BlueprintPure) FOVHeroRecord GetHero(FGuid HeroId) const;
    UFUNCTION(BlueprintPure) FString GetSkillDescription(FGuid HeroId, int32 SkillIndex) const;
    UFUNCTION(BlueprintPure) TArray<FOVHeroRecord> GetHeroes(int32 FactionId) const;
    UFUNCTION(BlueprintPure) int32 CountDeployed(int32 FactionId) const;
    UFUNCTION(BlueprintCallable) bool SetDeployment(FGuid HeroId, bool bDeploy);
    UFUNCTION(BlueprintCallable) bool SpendSkillPoint(FGuid HeroId, int32 SkillIndex);
    UFUNCTION(BlueprintCallable) bool GrantDevXP(FGuid HeroId, int32 Amount);
    UFUNCTION(BlueprintCallable) bool ConfigureDevSupportRepair(FGuid HeroId, FVector BaseCostPerHP, int32 Charges);
    UFUNCTION(BlueprintPure) int32 GetCatalystCapacity(FGuid HeroId) const;
    UFUNCTION(BlueprintCallable) bool AwardBattleXP(FGuid HeroId, const FString& BattleId, bool bVictory, float ParticipationSeconds, int32 HighestObjectiveXP);
    UFUNCTION(BlueprintCallable) bool SaveRoster(const FString& SlotName, int32 UserIndex = 0);
    UFUNCTION(BlueprintCallable) bool LoadRoster(const FString& SlotName, int32 UserIndex = 0);
    bool CanChangeRoster() const;
    FOVHeroRecord* FindHero(FGuid Id);
private:
    friend class UOVPlayScenarioSubsystem;
    void SetArenaLevel(FGuid Id,int32 Level);
    FGuid CreateRosterEntry(int32 FactionId,EOVInteriorRole Role,const FString& Name,bool bDeploy);
};
