#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "OVTitleBackdropDataAsset.h"
#include "OVTitleScreenWidget.generated.h"

class UBorder;
class UImage;
class UOVTitleBackdropStage;
class USlider;
class UScrollBox;
class UTextBlock;
class UVerticalBox;
class UTexture2D;
class UOVTitleScreenWidget;

UENUM(BlueprintType)
enum class EOVTitleMenuPanel : uint8
{
    Title,
    ScenarioSelect,
    Settings,
    Tutorial, Practice, Sortie
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
    FName SortieAction;
    int32 Choice=0;

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
 *  - Slate RoundedBox + 1px outline styling
 *  - Left-to-right gradient scrim + UBackgroundBlur
 *  - Korean typography hierarchy with imported Sans-serif font
 *  - Screen 01 (Title), Screen 02 (Playable Scenarios incl. E1 Black Hole Trial),
 *    Screen 03 (Expanded Settings: Window Mode, Resolution, Graphics Preset, Master/BGM/SFX Volume, UI Scale, Backdrop Way A/B),
 *    Screen 04 (5-Step Beginner Tutorial / Game Guide accessible from Title & In-Game HUD)
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

    /** Opens the 5-step beginner guide modal as an overlay from in-game battle without replacing the combat camera. */
    UFUNCTION(BlueprintCallable, Category = "UI|TitleScreen")
    void ActivateInGameTutorial();
    void ActivateFieldPanel(EOVTitleMenuPanel Panel);

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

    UFUNCTION(BlueprintCallable) void OpenPractice();
    UFUNCTION(BlueprintCallable) void SelectPractice(int32 Index);
    UFUNCTION(BlueprintCallable) void LaunchPractice();
    void BackFromCurrentPanel();
    UFUNCTION(BlueprintCallable) void SortieClick(FName Action,int32 Index=0);
    void RefreshSortie();
    FRotator SortieViewRotation;
    float SortieViewZoom = 1.12f;
    int32 SortieViewHull = INDEX_NONE;
    UTexture* SortiePartImage(int32 Model);

    UFUNCTION(BlueprintPure, Category = "UI|TitleScreen")
    EOVTitleMenuPanel GetCurrentPanel() const { return CurrentPanel; }

    UFUNCTION(BlueprintPure, Category = "UI|TitleScreen")
    UOVTitleBackdropStage* GetBackdropStage() const { return BackdropStage; }

private:
    UPROPERTY() TObjectPtr<UBorder> PracticePanel;
    UPROPERTY() TObjectPtr<UTextBlock> PracticeBrief;
    UPROPERTY() TArray<TObjectPtr<UOVScenarioPickButton>> PracticeRows;
    int32 SortieScrollStage=INDEX_NONE;
    bool bSortieScrollExternal=false;
    TWeakObjectPtr<UScrollBox> SortieColumns[3];
    UPROPERTY() TMap<int32,TObjectPtr<UTexture2D>> SortiePartImages;
    UPROPERTY() TObjectPtr<UBorder> SortiePanel;
    UPROPERTY() TObjectPtr<class UOVShipPreview> SortieMirror;
    void BuildSortie(class UOverlay* Root);
    int32 ChosenPractice=0;
    void BuildPractice(class UOverlay* Root);
    void BuildProgrammaticTree();
    void BuildSettingsModal(class UOverlay* RootOverlay);
    void BuildTutorialModal(class UOverlay* RootOverlay);
    void SyncSettingsWidgetsFromSystem();
    void RefreshBackdropBrush();
    void RefreshScenarioBriefing();
    void RefreshModeLabel();
    void RefreshSettingsLabels();
    void RefreshTutorialReturnLabel();
    void EnsureGradientTexture();
    void PlayClickAudio();

    UFUNCTION()
    void HandleOpenScenarios();

    UFUNCTION()
    void HandleOpenSandbox();

    UFUNCTION()
    void HandleOpenTutorial();

    UFUNCTION()
    void HandleOpenSettings();

    UFUNCTION()
    void HandleBackToTitle();
    UFUNCTION() void CycleScenarioDifficulty();
    UPROPERTY() TObjectPtr<UTextBlock> DifficultyText;

    UFUNCTION()
    void HandleCloseTutorial();

    UFUNCTION()
    void HandleLaunchSelectedScenario();

    UFUNCTION()
    void HandleToggleBackdropMode();

    UFUNCTION()
    void HandleCycleWindowMode();

    UFUNCTION()
    void HandleCycleResolution();

    UFUNCTION()
    void HandleCycleGraphicsQuality();

    UFUNCTION()
    void HandleMasterVolumeChanged(float Value);

    UFUNCTION()
    void HandleBGMVolumeChanged(float Value);

    UFUNCTION()
    void HandleSFXVolumeChanged(float Value);

    UFUNCTION()
    void HandleUIScaleChanged(float Value);

    UFUNCTION()
    void HandleApplyAndSaveSettings();

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
    TObjectPtr<UBorder> TutorialPanelContainer;

    UPROPERTY()
    TObjectPtr<UVerticalBox> ScenarioListBox;

    UPROPERTY()
    TArray<TObjectPtr<UOVScenarioPickButton>> ScenarioButtons;

    UPROPERTY() TObjectPtr<UTextBlock> ScenarioRosterTitle;
    UPROPERTY() TObjectPtr<UTextBlock> ScenarioHullCountTitle;

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
    TObjectPtr<UImage> ScenarioPhoto;

    UPROPERTY()
    TObjectPtr<UTextBlock> CardReturnLabel;

    UPROPERTY()
    TMap<FName, TObjectPtr<UTexture2D>> ScenarioPhotos;

    UPROPERTY()
    TObjectPtr<UTextBlock> BackdropStatusText;

    UPROPERTY()
    TObjectPtr<UTextBlock> FooterUIScaleText;

    // Settings Controls
    UPROPERTY()
    TObjectPtr<UTextBlock> SettingsBackdropModeText;

    UPROPERTY()
    TObjectPtr<UTextBlock> SettingsWindowModeValueText;

    UPROPERTY()
    TObjectPtr<UTextBlock> SettingsResolutionValueText;

    UPROPERTY()
    TObjectPtr<UTextBlock> SettingsQualityValueText;

    UPROPERTY()
    TObjectPtr<UTextBlock> SettingsMasterVolText;

    UPROPERTY()
    TObjectPtr<UTextBlock> SettingsBGMVolText;

    UPROPERTY()
    TObjectPtr<UTextBlock> SettingsSFXVolText;

    UPROPERTY()
    TObjectPtr<UTextBlock> SettingsUIScaleText;

    UPROPERTY()
    TObjectPtr<UTextBlock> SettingsFeedbackText;

    UPROPERTY()
    TObjectPtr<USlider> MasterVolumeSlider;

    UPROPERTY()
    TObjectPtr<USlider> BGMVolumeSlider;

    UPROPERTY()
    TObjectPtr<USlider> SFXVolumeSlider;

    UPROPERTY()
    TObjectPtr<USlider> UIScaleSlider;

    // Tutorial Return Label
    UPROPERTY()
    TObjectPtr<UTextBlock> TutorialCloseButtonText;

    UPROPERTY()
    TObjectPtr<UButton> LaunchScenarioButton;

    EOVTitleBackdropMode PreferredBackdropMode = EOVTitleBackdropMode::LiveProxyStage;
    EOVTitleMenuPanel CurrentPanel = EOVTitleMenuPanel::Title;
    FName SelectedScenarioId = TEXT("Fleet200");
    bool bBackdropActive = false;
    bool bOpenedFromInGame = false;
    bool bSuppressSliderCallbacks = false;

    // Pending settings state
    int32 WindowModeIndex = 1;       // 0=Fullscreen, 1=WindowedFullscreen, 2=Windowed
    int32 ResolutionIndex = 2;       // 0=1280x720, 1=1600x900, 2=1920x1080, 3=2560x1440
    int32 GraphicsQualityIndex = 2;  // 0=Low, 1=Medium, 2=High, 3=Epic
    float PendingMasterVolume = 0.85f;
    float PendingBGMVolume = 0.70f;
    float PendingSFXVolume = 0.85f;
    float PendingUIScale = 1.00f;
};
