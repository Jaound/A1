#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "OVTitleBackdropDataAsset.h"
#include "OVTitleScreenWidget.generated.h"

class UBorder;
class UImage;
class UOVTitleBackdropStage;
class UTextBlock;
class UVerticalBox;
class UTexture2D;
class UOVTitleScreenWidget;

UENUM(BlueprintType)
enum class EOVTitleMenuPanel : uint8
{
    Title,
    ScenarioSelect,
    Settings
};

UCLASS()
class ORBITALVANGUARD_API UOVScenarioPickButton : public UButton
{
    GENERATED_BODY()

public:
    UPROPERTY()
    TObjectPtr<UOVTitleScreenWidget> OwnerWidget;

    UPROPERTY()
    FName ScenarioId;

    UPROPERTY()
    TObjectPtr<UTextBlock> TitleText;

    UPROPERTY()
    TObjectPtr<UTextBlock> SubText;

    UPROPERTY()
    TObjectPtr<UTextBlock> BadgeText;

    void SetSelectedState(bool bInSelected);

    UFUNCTION()
    void HandleClicked();
};

/**
 * Programmatic Start Screen & Main Menu UMG widget for Orbital Vanguard.
 * Features:
 *  - Viewport-direct 3D ship formations via UOVTitleBackdropStage (or Way B high-res backdrop)
 *  - Slate RoundedBox + 1px outline styling (Condition 2)
 *  - Left-to-right gradient scrim + UBackgroundBlur (Condition 2)
 *  - Korean typography hierarchy with imported Sans-serif font (Condition 3)
 *  - Screen 01 (Title) and Screen 02 (6 Playable Scenarios) matching reference designs
 */
UCLASS(Blueprintable, BlueprintType)
class ORBITALVANGUARD_API UOVTitleScreenWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeOnInitialized() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    UFUNCTION(BlueprintCallable, Category = "UI|TitleScreen")
    void ActivateTitleScreen();

    UFUNCTION(BlueprintCallable, Category = "UI|TitleScreen")
    void DeactivateTitleScreen();

    UFUNCTION(BlueprintCallable, Category = "UI|TitleScreen")
    void ReleaseBackdrop();

    UFUNCTION(BlueprintCallable, Category = "UI|TitleScreen")
    void ShowPanel(EOVTitleMenuPanel Panel);

    UFUNCTION(BlueprintCallable, Category = "UI|TitleScreen")
    void SelectScenario(FName ScenarioId);

    UFUNCTION(BlueprintPure, Category = "UI|TitleScreen")
    FName GetSelectedScenarioId() const { return SelectedScenarioId; }

    UFUNCTION(BlueprintPure, Category = "UI|TitleScreen")
    UOVTitleBackdropStage* GetBackdropStage() const { return BackdropStage; }

private:
    void BuildProgrammaticTree();
    void RefreshBackdropBrush();
    void RefreshScenarioBriefing();
    void RefreshModeLabel();
    void EnsureGradientTexture();

    UFUNCTION()
    void HandleOpenScenarios();

    UFUNCTION()
    void HandleOpenSandbox();

    UFUNCTION()
    void HandleOpenSettings();

    UFUNCTION()
    void HandleBackToTitle();

    UFUNCTION()
    void HandleLaunchSelectedScenario();

    UFUNCTION()
    void HandleToggleBackdropMode();

    UFUNCTION()
    void HandleQuitGame();

    UPROPERTY()
    TObjectPtr<UOVTitleBackdropStage> BackdropStage;

    UPROPERTY()
    TObjectPtr<UImage> BackdropImage;

    UPROPERTY()
    TObjectPtr<UTexture2D> ScrimGradientTexture;

    UPROPERTY()
    TObjectPtr<UBorder> TitlePanelContainer;

    UPROPERTY()
    TObjectPtr<UBorder> ScenarioPanelContainer;

    UPROPERTY()
    TObjectPtr<UBorder> SettingsPanelContainer;

    UPROPERTY()
    TObjectPtr<UVerticalBox> ScenarioListBox;

    UPROPERTY()
    TArray<TObjectPtr<UOVScenarioPickButton>> ScenarioButtons;

    // Briefing Card
    UPROPERTY()
    TObjectPtr<UTextBlock> ScenarioTitleText;

    UPROPERTY()
    TObjectPtr<UTextBlock> ScenarioBadgeText;

    UPROPERTY()
    TObjectPtr<UTextBlock> ScenarioObjectiveText;

    UPROPERTY()
    TObjectPtr<UTextBlock> MetricBudgetVal;

    UPROPERTY()
    TObjectPtr<UTextBlock> MetricHangarVal;

    UPROPERTY()
    TObjectPtr<UTextBlock> MetricTimeVal;

    UPROPERTY()
    TObjectPtr<UVerticalBox> RosterColLeft;

    UPROPERTY()
    TObjectPtr<UVerticalBox> RosterColRight;

    UPROPERTY()
    TObjectPtr<UTextBlock> ScenarioCliSnippet;

    UPROPERTY()
    TObjectPtr<UTextBlock> BackdropStatusText;

    UPROPERTY()
    TObjectPtr<UTextBlock> SettingsBackdropModeText;

    UPROPERTY()
    TObjectPtr<UButton> LaunchScenarioButton;

    EOVTitleBackdropMode PreferredBackdropMode = EOVTitleBackdropMode::LiveProxyStage;
    EOVTitleMenuPanel CurrentPanel = EOVTitleMenuPanel::Title;
    FName SelectedScenarioId = TEXT("Fleet200");
    bool bBackdropActive = false;
};
