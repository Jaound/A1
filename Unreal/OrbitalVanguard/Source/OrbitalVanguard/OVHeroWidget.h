#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "OVHeroWidget.generated.h"

class AOVShip;
class UTextBlock;
class UProgressBar;
class UVerticalBox;
class UWrapBox;
class USizeBox;
class UOVHeroWidget;
class UOVPortraitWidget;

/** Read-only player view. Stable identity remains separate from the scene unit identity. */
USTRUCT(BlueprintType)
struct ORBITALVANGUARD_API FOVHeroCardView
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FGuid HeroId;
    UPROPERTY(BlueprintReadOnly) FName DefinitionId;
    UPROPERTY(BlueprintReadOnly) int32 UnitId = 0;
    UPROPERTY(BlueprintReadOnly) FString Name;
    UPROPERTY(BlueprintReadOnly) FString Role;
    UPROPERTY(BlueprintReadOnly) FString State;
    UPROPERTY(BlueprintReadOnly) FString Location;
    UPROPERTY(BlueprintReadOnly) int32 RoleIndex = 1;
    UPROPERTY(BlueprintReadOnly) int32 Level = 1;
    UPROPERTY(BlueprintReadOnly) float HP = 0;
    UPROPERTY(BlueprintReadOnly) float MaxHP = 0;
    UPROPERTY(BlueprintReadOnly) bool bHPKnown = false;
    UPROPERTY(BlueprintReadOnly) bool bMissionOnly = false;
    UPROPERTY(BlueprintReadOnly) bool bSelected = false;
    UPROPERTY(BlueprintReadOnly) bool bLost = false;
};

UCLASS()
class ORBITALVANGUARD_API UOVHeroCard : public UButton
{
    GENERATED_BODY()
public:
    UPROPERTY() TObjectPtr<UOVHeroWidget> Owner;
    UPROPERTY() TObjectPtr<UOVPortraitWidget> Portrait;
    UPROPERTY() TObjectPtr<UTextBlock> NameLabel;
    UPROPERTY() TObjectPtr<UTextBlock> RoleLabel;
    UPROPERTY() TObjectPtr<UTextBlock> StateLabel;
    UPROPERTY() TObjectPtr<UProgressBar> Health;
    FGuid HeroId;
    int32 UnitId = 0;
    UFUNCTION() void Pick();
    void Update(const FOVHeroCardView& View);
};

/** Compact player roster. All commands use the existing gameplay services. */
UCLASS()
class ORBITALVANGUARD_API UOVHeroWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    virtual void NativeOnInitialized() override;
    virtual void NativeTick(const FGeometry& Geometry, float DeltaSeconds) override;
    UFUNCTION(BlueprintCallable, Category="UI") bool SelectHero(FGuid HeroId);
    UFUNCTION(BlueprintCallable, Category="UI") bool SelectMissionHero(int32 UnitId);
    UFUNCTION(BlueprintCallable, Category="UI") void RefreshCards();
    UFUNCTION(BlueprintPure, Category="UI") TArray<FOVHeroCardView> GetCardViews() const { return Views; }
    UFUNCTION(BlueprintPure, Category="UI") FGuid GetSelectedHeroId() const { return SelectedHeroId; }
    UFUNCTION(BlueprintPure, Category="UI") int32 GetSelectedUnitId() const;
    UFUNCTION(BlueprintPure, Category="UI") int32 GetCardRebuildCount() const { return CardRebuildCount; }
    UFUNCTION(BlueprintPure, Category="UI") TArray<FString> GetSelectedSkillLabels() const;
    UFUNCTION(BlueprintCallable, Category="UI") bool LocateSelectedHero();
    void PickCard(FGuid HeroId, int32 UnitId);
private:
    UPROPERTY() TObjectPtr<class UEditableTextBox> Search;
    UPROPERTY() TObjectPtr<UComboBoxString> FilterCategory;
    UPROPERTY() TObjectPtr<UComboBoxString> FilterValue;
    TArray<FString> LastFilterOptions;
    int32 LastFilterCategory=INDEX_NONE;
    UPROPERTY() TObjectPtr<UComboBoxString> TemplateChoice;
    UPROPERTY() TObjectPtr<UTextBlock> CatalogueReport;
    UPROPERTY() TObjectPtr<UButton> ArenaAssign;
    UFUNCTION() void AssignArenaCandidate();
    UFUNCTION() void CreateTemplate();
    UFUNCTION() void ToggleCatalogue();
    UPROPERTY() TObjectPtr<UTextBlock> Header;
    UPROPERTY() TObjectPtr<UTextBlock> Detail;
    UPROPERTY() TObjectPtr<class UImage> Standing;
    UPROPERTY() TObjectPtr<class USizeBox> StandingBox;
    UPROPERTY() TObjectPtr<UTextBlock> EmptyHelp;
    UPROPERTY() TObjectPtr<UTextBlock> Feedback;
    UPROPERTY() TObjectPtr<UTextBlock> OwnedLabel;
    UPROPERTY() TObjectPtr<UTextBlock> MissionLabel;
    UPROPERTY() TObjectPtr<UTextBlock> SkillsLabel;
    UPROPERTY() TObjectPtr<UWrapBox> OwnedCards;
    UPROPERTY() TObjectPtr<UWrapBox> MissionCards;
    UPROPERTY() TObjectPtr<UVerticalBox> FormationBox;
    UPROPERTY() TObjectPtr<UComboBoxString> DutyChoice;
    UPROPERTY() TArray<TObjectPtr<UOVHeroCard>> Cards;
    UPROPERTY() TArray<TObjectPtr<USizeBox>> CardSizes;
    UPROPERTY() TArray<TObjectPtr<USizeBox>> PortraitSizes;
    UPROPERTY() TArray<TObjectPtr<USizeBox>> SkillSizes;
    UPROPERTY() TArray<TObjectPtr<UButton>> SkillButtons;
    UPROPERTY() TArray<TObjectPtr<UTextBlock>> SkillLabels;
    UPROPERTY() TArray<TObjectPtr<UButton>> OwnedActions;
    UPROPERTY() TObjectPtr<UButton> LocateButton;
    UPROPERTY() TObjectPtr<UButton> InteriorButton;
    UPROPERTY() TObjectPtr<UButton> RetreatButton;
    UPROPERTY() TArray<FOVHeroCardView> Views;
    TArray<FName> DutySlots;
    FGuid DutyHero;
    TWeakObjectPtr<AOVShip> DutyShip;
    FGuid SelectedHeroId;
    int32 SelectedMissionUnitId = 0;
    FString RosterSignature;
    float RefreshElapsed = 0;
    int32 CardRebuildCount = 0;
    float LastLayoutWidth = 0;
    void Refresh();
    void RebuildCards();
    void LayoutCards(float AvailableWidth, bool bForce = false);
    FGuid SelectedId() const { return SelectedHeroId; }
    void SetFeedback(const FString& Message);
    UFUNCTION() void Close();
    UFUNCTION() void Deploy();
    UFUNCTION() void Release();
    UFUNCTION() void OpenInterior();
    UFUNCTION() void Locate();
    UFUNCTION() void Retreat();
    UFUNCTION() void ToggleFormation();
    UFUNCTION() void SpawnHero();
    UFUNCTION() void BoardingOn();
    UFUNCTION() void BoardingOff();
    void SetBoarding(bool bAssigned);
    UFUNCTION() void DutyOn();
    UFUNCTION() void DutyOff();
    UFUNCTION() void Skill0();
    UFUNCTION() void Skill1();
    UFUNCTION() void Skill2();
    UFUNCTION() void Skill3();
    UFUNCTION() void RosterRoom();
    UFUNCTION() void StartMission();
    UFUNCTION() void StartTrial();
    void Spend(int32 Index);
};
