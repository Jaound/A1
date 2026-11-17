#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "OVDevelopmentWidget.generated.h"
class UCheckBox; class AOVShip; class UButton; class UComboBoxString; class UEditableTextBox; class UScrollBox; class UTextBlock; class UVerticalBox; class UOVDevelopmentWidget; class UWrapBox;
UCLASS()
class ORBITALVANGUARD_API UOVCompactCombo : public UComboBoxString
{
 GENERATED_BODY()
public:
 void SetCompactFont() { auto CompactFont=GetFont();CompactFont.Size=14;InitFont(CompactFont); }
};
UCLASS()
class ORBITALVANGUARD_API UOVDevelopmentItem : public UButton
{
 GENERATED_BODY()
public:
 UPROPERTY() TObjectPtr<UOVDevelopmentWidget> Owner;
 FName Id;
 UFUNCTION() void Pick();
};
UCLASS()
class ORBITALVANGUARD_API UOVDevelopmentWidget : public UUserWidget
{
 GENERATED_BODY()
public:
 virtual void NativeOnInitialized() override;
 bool IsEditing() const;
 UFUNCTION(BlueprintCallable, Category="UI") bool OpenPage(int32 Index);
 /** Size: 0=all, 1..4 hull class; Role=None=all. Axis: -1=all, 0..5 planning/data/generation/role/automation/direct. Status: -1=all, otherwise EOVCatalogStatus numeric value. */
 UFUNCTION(BlueprintCallable, Category="UI") bool SetFilters(int32 SizeClass, FName RoleTag, int32 StatusAxis, int32 StatusValue);
 UFUNCTION(BlueprintPure, Category="UI") TArray<FName> GetVisibleDefinitionIds() const { return VisibleDefinitionIds; }
 UFUNCTION(BlueprintCallable, Category="UI") void SetAdvancedDiagnostics(bool bEnabled);
 UFUNCTION(BlueprintPure, Category="UI") bool IsAdvancedDiagnostics() const { return bAdvanced; }
 UFUNCTION(BlueprintPure, Category="UI") FName GetSelectedDefinitionId() const { return SelectedId; }
 UFUNCTION(BlueprintPure, Category="UI") FString GetCompactSelectionText() const;
 UFUNCTION(BlueprintPure, Category="UI") int32 GetCurrentPage() const { return Page; }
 UFUNCTION(BlueprintCallable, Category="UI") void SelectItem(FName Id);
 UFUNCTION(BlueprintCallable, Category="UI") void ExecuteSelectedCreation();
private:
 int32 Page=1;
 bool bAdvanced=false;
 int32 HeroFaction=0;
 int32 HullCategory=0;
 int32 OverviewCategory=0;
 int32 FilterSize=0;
 FName FilterRole=NAME_None;
 int32 FilterAxis=INDEX_NONE;
 int32 FilterStatus=INDEX_NONE;
 bool bUpdatingFilters=false;
 TArray<FName> FilterRoles;
 TArray<FName> VisibleDefinitionIds;
 FName SelectedId;
 UPROPERTY() TObjectPtr<UVerticalBox> Body;
 UPROPERTY() TObjectPtr<UWrapBox> Cards;
 UPROPERTY() TArray<TObjectPtr<UOVDevelopmentItem>> CardButtons;
 UPROPERTY() TObjectPtr<UComboBoxString> SideChoice;
 UPROPERTY() TObjectPtr<UComboBoxString> PresetChoice;
 UPROPERTY() TObjectPtr<UComboBoxString> HeroDestination;
 UPROPERTY() TObjectPtr<UButton> AdvancedButton;
 UPROPERTY() TObjectPtr<UButton> EquipButton;
 UPROPERTY() TArray<TObjectPtr<UWidget>> AdvancedGroups;
 UPROPERTY() TObjectPtr<UScrollBox> List;
 UPROPERTY() TObjectPtr<UEditableTextBox> Search;
 UPROPERTY() TObjectPtr<UTextBlock> Detail;
 UPROPERTY() TObjectPtr<UTextBlock> Feedback;
 UPROPERTY() TObjectPtr<UTextBlock> RuntimeDetail;
 UPROPERTY() TObjectPtr<UComboBoxString> HeroChoice;
 UPROPERTY() TObjectPtr<UComboBoxString> RoleChoice;
 UPROPERTY() TObjectPtr<UComboBoxString> CategoryChoice;
 UPROPERTY() TObjectPtr<UComboBoxString> SizeFilter;
 UPROPERTY() TObjectPtr<UComboBoxString> RoleFilter;
 UPROPERTY() TObjectPtr<UComboBoxString> AxisFilter;
 UPROPERTY() TObjectPtr<UComboBoxString> StatusFilter;
 UPROPERTY() TObjectPtr<UEditableTextBox> HeroName;
 UPROPERTY() TObjectPtr<UEditableTextBox> XPAmount;
 UPROPERTY() TObjectPtr<UEditableTextBox> SupportBasic;
 UPROPERTY() TObjectPtr<UEditableTextBox> SupportAdvanced;
 UPROPERTY() TObjectPtr<UEditableTextBox> SupportRare;
 UPROPERTY() TObjectPtr<UEditableTextBox> SupportCharges;
 UPROPERTY() TObjectPtr<UEditableTextBox> Quantity;
 UPROPERTY() TObjectPtr<UEditableTextBox> X;
 UPROPERTY() TObjectPtr<UEditableTextBox> Y;
 UPROPERTY() TObjectPtr<UEditableTextBox> Z;
 UPROPERTY() TObjectPtr<UEditableTextBox> Spacing;
 UPROPERTY() TObjectPtr<UEditableTextBox> Faction;
 UPROPERTY() TObjectPtr<UEditableTextBox> Inventory;
 UPROPERTY() TObjectPtr<UEditableTextBox> Troops;
 UPROPERTY() TObjectPtr<UCheckBox> MoveAuto;
 UPROPERTY() TObjectPtr<UCheckBox> FireAuto;
 UPROPERTY() TObjectPtr<UComboBoxString> MountChoice;
 UPROPERTY() TObjectPtr<UComboBoxString> TargetChoice;
 UPROPERTY() TObjectPtr<UComboBoxString> RuntimeShips;
 UPROPERTY() TObjectPtr<UComboBoxString> LogisticsTarget;
 UPROPERTY() TObjectPtr<UEditableTextBox> DevAmount;
 UPROPERTY() TObjectPtr<UEditableTextBox> DevRange;
 UPROPERTY() TObjectPtr<UEditableTextBox> DevNode;
 UPROPERTY() TObjectPtr<UEditableTextBox> DevBasic;
 UPROPERTY() TObjectPtr<UEditableTextBox> DevAdvanced;
 UPROPERTY() TObjectPtr<UEditableTextBox> DevRare;
 UPROPERTY() TObjectPtr<UEditableTextBox> DevQuoteBasic;
 UPROPERTY() TObjectPtr<UEditableTextBox> DevQuoteAdvanced;
 UPROPERTY() TObjectPtr<UEditableTextBox> DevQuoteRare;
 TArray<TWeakObjectPtr<AOVShip>> RuntimeShipIds;
 TArray<FName> MountIds;
 TArray<TWeakObjectPtr<AOVShip>> Targets;
 TWeakObjectPtr<AOVShip> WeaponSubject;
 TArray<FGuid> HeroIds;
 void Rebuild();
 void RefreshList();
 void Reply(const FString& Message);
 void UpdateCardSelection();
 UFUNCTION() void ToggleAdvanced();
 UFUNCTION() void SideChanged(FString Item, ESelectInfo::Type Type);
 UFUNCTION() void PresetChanged(FString Item, ESelectInfo::Type Type);
 UFUNCTION() void LocateHero();
 UFUNCTION() void SceneCombat();
 UFUNCTION() void SceneCalibration();
 UFUNCTION() void SceneCarrier();
 UFUNCTION() void SceneMission();
 UFUNCTION() void SceneRoster();
 UFUNCTION() void SceneRestart();
 UFUNCTION() void PauseScene();
 FGuid HeroId() const;
 UFUNCTION() void Overview();
 UFUNCTION() void Hulls();
 UFUNCTION() void Heroes();
 UFUNCTION() void Weapons();
 UFUNCTION() void Close();
 UFUNCTION() void SearchChanged(const FText& Text);
 UFUNCTION() void CategoryChanged(FString Item, ESelectInfo::Type Type);
 UFUNCTION() void FiltersChanged(FString Item, ESelectInfo::Type Type);
 void RefreshShips();
 AOVShip* RuntimeShip() const;
 UFUNCTION() void SelectRuntimeShip();
 UFUNCTION() void FollowRuntimeShip();
 UFUNCTION() void InteriorRuntimeShip();
 UFUNCTION() void SetMaterials();
 UFUNCTION() void AddNode();
 UFUNCTION() void Mine();
 UFUNCTION() void Transfer();
 UFUNCTION() void Repair();
 UFUNCTION() void Spawn();
 UFUNCTION() void LaunchAircraft();
 UFUNCTION() void RecallAircraft();
 UFUNCTION() void LaunchPods();
 UFUNCTION() void RecallPods();
 UFUNCTION() void RefreshRuntime();
 UFUNCTION() void HeroChanged(FString Item, ESelectInfo::Type Type);
 UFUNCTION() void MountChanged(FString Item, ESelectInfo::Type Type);
 UFUNCTION() void Equip();
 UFUNCTION() void Target();
 UFUNCTION() void Fire();
 UFUNCTION() void Delete();
 UFUNCTION() void SpawnHero();
 UFUNCTION() void CreateHero();
 UFUNCTION() void CreateFour();
 UFUNCTION() void GrantXP();
 UFUNCTION() void ConfigureSupport();
 UFUNCTION() void DeployHero();
 UFUNCTION() void ReleaseHero();
 UFUNCTION() void Skill0();
 UFUNCTION() void Skill1();
 UFUNCTION() void Skill2();
 UFUNCTION() void Skill3();
 UFUNCTION() void Save();
 UFUNCTION() void Load();
 void Spend(int32 Index);
};
