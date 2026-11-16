#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "OVDevelopmentWidget.generated.h"
class UCheckBox; class AOVShip; class UButton; class UComboBoxString; class UEditableTextBox; class UScrollBox; class UTextBlock; class UVerticalBox; class UOVDevelopmentWidget;
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
 void SelectItem(FName Id);
private:
 int32 Page=0;
 FName SelectedId;
 UPROPERTY() TObjectPtr<UVerticalBox> Body;
 UPROPERTY() TObjectPtr<UScrollBox> List;
 UPROPERTY() TObjectPtr<UEditableTextBox> Search;
 UPROPERTY() TObjectPtr<UTextBlock> Detail;
 UPROPERTY() TObjectPtr<UTextBlock> Feedback;
 UPROPERTY() TObjectPtr<UComboBoxString> HeroChoice;
 UPROPERTY() TObjectPtr<UComboBoxString> RoleChoice;
 UPROPERTY() TObjectPtr<UComboBoxString> CategoryChoice;
 UPROPERTY() TObjectPtr<UEditableTextBox> HeroName;
 UPROPERTY() TObjectPtr<UEditableTextBox> XPAmount;
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
 TArray<FName> MountIds;
 TArray<TWeakObjectPtr<AOVShip>> Targets;
 TWeakObjectPtr<AOVShip> WeaponSubject;
 TArray<FGuid> HeroIds;
 void Rebuild();
 void RefreshList();
 FGuid HeroId() const;
 UFUNCTION() void Overview();
 UFUNCTION() void Hulls();
 UFUNCTION() void Heroes();
 UFUNCTION() void Weapons();
 UFUNCTION() void Close();
 UFUNCTION() void SearchChanged(const FText& Text);
 UFUNCTION() void CategoryChanged(FString Item, ESelectInfo::Type Type);
 UFUNCTION() void Spawn();
 UFUNCTION() void Equip();
 UFUNCTION() void Target();
 UFUNCTION() void Fire();
 UFUNCTION() void Delete();
 UFUNCTION() void SpawnHero();
 UFUNCTION() void CreateHero();
 UFUNCTION() void CreateFour();
 UFUNCTION() void GrantXP();
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
