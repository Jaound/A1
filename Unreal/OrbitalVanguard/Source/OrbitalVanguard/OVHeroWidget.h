#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/ComboBoxString.h"
#include "OVHeroWidget.generated.h"
class AOVShip;
class UComboBoxString;
class UTextBlock;
class UScrollBox;

/** Player-facing own-faction roster. Rules and orders remain in the owning subsystems. */
UCLASS()
class ORBITALVANGUARD_API UOVHeroWidget : public UUserWidget
{
 GENERATED_BODY()
public:
 virtual void NativeOnInitialized() override;
 virtual void NativeTick(const FGeometry& Geometry, float DeltaSeconds) override;
private:
 UPROPERTY() TObjectPtr<UComboBoxString> Choice;
 UPROPERTY() TObjectPtr<UTextBlock> Detail;
 UPROPERTY() TObjectPtr<UTextBlock> EmptyHelp;
 UPROPERTY() TObjectPtr<UTextBlock> Feedback;
 UPROPERTY() TObjectPtr<UScrollBox> Roster;
 UPROPERTY() TArray<TObjectPtr<UTextBlock>> UnitRows;
 UPROPERTY() TObjectPtr<UComboBoxString> DutyChoice;
 TArray<FName> DutySlots;
 FGuid DutyHero;
 TWeakObjectPtr<AOVShip> DutyShip;
 TArray<FGuid> ChoiceIds;
 FString RosterSignature;
 float RefreshElapsed=0;
 void Refresh();
 FGuid SelectedId() const;
 UFUNCTION() void Close();
 UFUNCTION() void Deploy();
 UFUNCTION() void Release();
 UFUNCTION() void OpenInterior();
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
 void Spend(int32 Index);
 UFUNCTION() void SelectionChanged(FString Item, ESelectInfo::Type Type);
};
