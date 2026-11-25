#include "OVTitleScreenWidget.h"
#include "Components/ScrollBox.h"
#include "OVTypography.h"
#include "OVShipPreview.h"
#include "OVCombatSubsystem.h"
#include "OVShipSystemsComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "OVShip.h"
#include "Components/UniformGridPanel.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "OVSortiePlot.h"
#include "OVSortieDesignWidget.h"

#include "OVAudioSubsystem.h"
#include "OVCalibration.h"
#include "OVController.h"
#include "OVHUD.h"
#include "OVPlayScenarioSubsystem.h"
#include "OVTitleBackdropStage.h"

#include "Blueprint/WidgetTree.h"
#include "Components/BackgroundBlur.h"
#include "Components/Border.h"
#include "Components/BorderSlot.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/ScaleBox.h"
#include "Components/Slider.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "GameFramework/GameUserSettings.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"

namespace OVStyle
{
static const FLinearColor CyanAccent(0.00f, 0.898f, 1.00f, 1.0f);       // #00E5FF
static const FLinearColor CyanHover(0.20f, 0.93f, 1.00f, 1.0f);        // #33EBFF
static const FLinearColor CyanPressed(0.00f, 0.72f, 0.82f, 1.0f);      // #00B8D4
static const FLinearColor DarkText(0.02f, 0.043f, 0.078f, 1.0f);        // #050B14

static const FLinearColor DarkNavyBg(0.04f, 0.08f, 0.13f, 0.88f);       // #0A1421
static const FLinearColor DarkNavyHover(0.07f, 0.14f, 0.22f, 0.95f);    // #122438
static const FLinearColor DarkNavyPressed(0.03f, 0.06f, 0.10f, 0.95f);  // #08101A
static const FLinearColor DarkNavyDisabled(0.02f, 0.04f, 0.07f, 0.50f);

static const FLinearColor SlateBorder(0.12f, 0.18f, 0.26f, 1.0f);      // #1F2E42
static const FLinearColor CyanBorder(0.00f, 0.898f, 1.00f, 0.9f);       // #00E5FF
static const FLinearColor SubtleBorder(0.08f, 0.13f, 0.19f, 0.8f);

static const FLinearColor TextWhite(0.96f, 0.98f, 1.00f, 1.0f);
static const FLinearColor TextSlate(0.58f, 0.69f, 0.80f, 1.0f);        // #94A3B8
static const FLinearColor TextDim(0.40f, 0.50f, 0.60f, 1.0f);          // #64748B
static const FLinearColor AmberBadge(0.96f, 0.62f, 0.05f, 1.0f);       // #F59E0B
static const FLinearColor PurpleBadge(0.76f, 0.52f, 1.00f, 1.0f);

static const FIntPoint ResolutionPresets[] = {
    FIntPoint(1280, 720),
    FIntPoint(1600, 900),
    FIntPoint(1920, 1080),
    FIntPoint(2560, 1440),
    FIntPoint(3840, 2160)
};

static const TCHAR* WindowModeLabels[] = {
    TEXT("전체화면"),
    TEXT("테두리 없는 창"),
    TEXT("창모드")
};

static const TCHAR* QualityLabels[] = {
    TEXT("낮음"),
    TEXT("보통"),
    TEXT("높음"),
    TEXT("에픽"),
    TEXT("최상 · 시네마틱")
};

inline FSlateBrush MakeRoundedBoxBrush(
    const FLinearColor& FillColor,
    const FLinearColor& OutlineColor,
    float CornerRadius = 6.0f,
    float OutlineWidth = 1.0f)
{
    FSlateBrush Brush;
    Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
    Brush.TintColor = FSlateColor(FillColor);
    Brush.OutlineSettings = FSlateBrushOutlineSettings(CornerRadius, FSlateColor(OutlineColor), OutlineWidth);
    return Brush;
}

inline FButtonStyle MakePrimaryButtonStyle()
{
    FButtonStyle Style;
    Style.Normal = MakeRoundedBoxBrush(CyanAccent, CyanAccent, 6.0f, 1.0f);
    Style.Hovered = MakeRoundedBoxBrush(CyanHover, FLinearColor::White, 6.0f, 1.5f);
    Style.Pressed = MakeRoundedBoxBrush(CyanPressed, CyanPressed, 6.0f, 1.0f);
    Style.Disabled = MakeRoundedBoxBrush(FLinearColor(0.1f, 0.2f, 0.25f, 0.4f), SubtleBorder, 6.0f, 1.0f);
    return Style;
}

inline FButtonStyle MakeSecondaryButtonStyle(bool bSelected = false)
{
    FButtonStyle Style;
    const FLinearColor BorderCol = bSelected ? CyanBorder : SlateBorder;
    const FLinearColor BgCol = bSelected ? FLinearColor(0.04f, 0.15f, 0.26f, 0.95f) : DarkNavyBg;
    Style.Normal = MakeRoundedBoxBrush(BgCol, BorderCol, 6.0f, bSelected ? 1.5f : 1.0f);
    Style.Hovered = MakeRoundedBoxBrush(DarkNavyHover, CyanBorder, 6.0f, 1.5f);
    Style.Pressed = MakeRoundedBoxBrush(DarkNavyPressed, CyanAccent, 6.0f, 1.0f);
    Style.Disabled = MakeRoundedBoxBrush(DarkNavyDisabled, SubtleBorder, 6.0f, 1.0f);
    return Style;
}

UTextBlock* MakeLabel(UWidgetTree* Tree, const FString& Text, int32 Size, const FLinearColor& Color, bool bBold = false, EOVTextRole Role = EOVTextRole::Body)
{
    UTextBlock* Label = Tree->ConstructWidget<UTextBlock>();
    Label->SetFont(OVTypography::Style(bBold ? EOVTextRole::Heading : Role, Size));
    Label->SetColorAndOpacity(FSlateColor(Color));
    Label->SetText(FText::FromString(Text));
    Label->SetAutoWrapText(true);
    return Label;
}

UButton* MakeMenuButton(
    UWidgetTree* Tree,
    UVerticalBox* Parent,
    const FString& Title,
    const FString& RightBadge = TEXT(""),
    bool bPrimary = false,
    bool bDisabled = false)
{
    UButton* Btn = Tree->ConstructWidget<UButton>();
    Btn->SetStyle(bPrimary ? MakePrimaryButtonStyle() : MakeSecondaryButtonStyle(false));

    UHorizontalBox* HBox = Tree->ConstructWidget<UHorizontalBox>();
    Btn->AddChild(HBox);

    UTextBlock* LeftText = MakeLabel(
        Tree,
        Title,
        bPrimary ? 15 : 14,
        bPrimary ? DarkText : (bDisabled ? TextDim : TextWhite),
        bPrimary || !bDisabled);
    LeftText->SetAutoWrapText(false);

    if (UHorizontalBoxSlot* LeftSlot = HBox->AddChildToHorizontalBox(LeftText))
    {
        LeftSlot->SetPadding(FMargin(14.f, 9.f, 10.f, 9.f));
        LeftSlot->SetVerticalAlignment(VAlign_Center);
    }

    USpacer* Spacer = Tree->ConstructWidget<USpacer>();
    if (UHorizontalBoxSlot* SpacerSlot = HBox->AddChildToHorizontalBox(Spacer))
    {
        SpacerSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    }

    if (!RightBadge.IsEmpty())
    {
        UTextBlock* RightText = MakeLabel(
            Tree,
            RightBadge,
            bPrimary ? 14 : 11,
            bPrimary ? DarkText : (bDisabled ? TextDim : CyanAccent),
            bPrimary);
        RightText->SetAutoWrapText(false);

        if (UHorizontalBoxSlot* RightSlot = HBox->AddChildToHorizontalBox(RightText))
        {
            RightSlot->SetPadding(FMargin(10.f, 9.f, 14.f, 9.f));
            RightSlot->SetVerticalAlignment(VAlign_Center);
        }
    }

    if (bDisabled)
    {
        Btn->SetIsEnabled(false);
    }

    if (UVerticalBoxSlot* Slot = Parent->AddChildToVerticalBox(Btn))
    {
        Slot->SetPadding(FMargin(0.f, 3.5f));
        Slot->SetHorizontalAlignment(HAlign_Fill);
    }
    return Btn;
}
}

void UOVScenarioPickButton::SetSelectedState(bool bInSelected)
{
    SetStyle(OVStyle::MakeSecondaryButtonStyle(bInSelected));
    if (TitleText)
    {
        TitleText->SetColorAndOpacity(FSlateColor(bInSelected ? OVStyle::CyanAccent : OVStyle::TextWhite));
    }
}

void UOVScenarioPickButton::HandleClicked()
{
    if (OwnerWidget)
    {
        if(!SortieAction.IsNone()){OwnerWidget->SortieClick(SortieAction,Choice);return;}
        if(OwnerWidget->GetCurrentPanel()==EOVTitleMenuPanel::Practice)
            OwnerWidget->SelectPractice(ScenarioId.GetNumber()-1);
        else OwnerWidget->SelectScenario(ScenarioId);
    }
}

void UOVTitleScreenWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    EnsureGradientTexture();
    BuildProgrammaticTree();
    SyncSettingsWidgetsFromSystem();
    ShowPanel(EOVTitleMenuPanel::Title);
}

void UOVTitleScreenWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    if (bBackdropActive && BackdropStage)
    {
        BackdropStage->StepIdleMotion(InDeltaTime);
    }
}

void UOVTitleScreenWidget::PlayClickAudio()
{
    if (UOVAudioSubsystem* Audio = UOVAudioSubsystem::Get(this))
    {
        Audio->PlayUIClick();
    }
}

void UOVTitleScreenWidget::EnsureGradientTexture()
{
    if (IsValid(ScrimGradientTexture))
    {
        return;
    }

    constexpr int32 TexWidth = 256;
    constexpr int32 TexHeight = 4;
    ScrimGradientTexture = UTexture2D::CreateTransient(TexWidth, TexHeight, PF_B8G8R8A8);
    if (!ScrimGradientTexture)
    {
        return;
    }

    FColor* Pixels = (FColor*)ScrimGradientTexture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
    for (int32 Y = 0; Y < TexHeight; ++Y)
    {
        for (int32 X = 0; X < TexWidth; ++X)
        {
            const float U = (float)X / (float)(TexWidth - 1);
            const float Falloff = FMath::Clamp(1.0f - FMath::Pow(U, 1.45f), 0.0f, 1.0f);
            const uint8 Alpha = (uint8)(Falloff * 235.f);
            Pixels[Y * TexWidth + X] = FColor(10, 18, 30, Alpha);
        }
    }
    ScrimGradientTexture->GetPlatformData()->Mips[0].BulkData.Unlock();
    ScrimGradientTexture->UpdateResource();
}

void UOVTitleScreenWidget::BuildProgrammaticTree()
{
    if (!WidgetTree || WidgetTree->RootWidget != nullptr)
    {
        return;
    }

    UOverlay* RootOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("TitleRootOverlay"));
    WidgetTree->RootWidget = RootOverlay;

    // Layer 1: Pre-rendered 2D backdrop texture (used only when Way B is selected)
    BackdropImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("BackdropImage"));
    BackdropImage->SetColorAndOpacity(FLinearColor::White);
    BackdropImage->SetVisibility(ESlateVisibility::Collapsed);
    if (UOverlaySlot* BgSlot = RootOverlay->AddChildToOverlay(BackdropImage))
    {
        BgSlot->SetHorizontalAlignment(HAlign_Fill);
        BgSlot->SetVerticalAlignment(VAlign_Fill);
    }

    // Layer 2: Left Menu Scrim (UBackgroundBlur + Horizontal Gradient Scrim Brush)
    USizeBox* LeftScrimBox = WidgetTree->ConstructWidget<USizeBox>();
    LeftScrimBox->SetWidthOverride(500.f);

    UOverlay* ScrimStack = WidgetTree->ConstructWidget<UOverlay>();
    LeftScrimBox->SetContent(ScrimStack);

    UBackgroundBlur* BlurEffect = WidgetTree->ConstructWidget<UBackgroundBlur>();
    BlurEffect->SetBlurStrength(20.0f);
    BlurEffect->SetApplyAlphaToBlur(true);
    if (UOverlaySlot* BlurSlot = ScrimStack->AddChildToOverlay(BlurEffect))
    {
        BlurSlot->SetHorizontalAlignment(HAlign_Fill);
        BlurSlot->SetVerticalAlignment(VAlign_Fill);
    }

    UImage* ScrimGradient = WidgetTree->ConstructWidget<UImage>();
    if (ScrimGradientTexture)
    {
        ScrimGradient->SetBrushFromTexture(ScrimGradientTexture);
    }
    if (UOverlaySlot* GradSlot = ScrimStack->AddChildToOverlay(ScrimGradient))
    {
        GradSlot->SetHorizontalAlignment(HAlign_Fill);
        GradSlot->SetVerticalAlignment(VAlign_Fill);
    }

    UVerticalBox* LeftMenuVBox = WidgetTree->ConstructWidget<UVerticalBox>();
    if (UOverlaySlot* ContentSlot = ScrimStack->AddChildToOverlay(LeftMenuVBox))
    {
        ContentSlot->SetPadding(FMargin(44.f, 34.f, 40.f, 30.f));
        ContentSlot->SetHorizontalAlignment(HAlign_Fill);
        ContentSlot->SetVerticalAlignment(VAlign_Fill);
    }

    UTextBlock* TitleLabel = OVStyle::MakeLabel(
        WidgetTree, TEXT("ORBITAL VANGUARD"), 0, OVStyle::TextWhite, false, EOVTextRole::Logo);
    LeftMenuVBox->AddChildToVerticalBox(TitleLabel)->SetPadding(FMargin(0.f, 6.f, 0.f, 10.f));

    LeftMenuVBox->AddChildToVerticalBox(OVStyle::MakeLabel(
        WidgetTree, TEXT("3D 우주 함대전과 함선 내부 2D 강습 작전을 단일 시뮬레이션으로 지휘합니다."), 13, OVStyle::TextSlate, false))
        ->SetPadding(FMargin(0.f, 0.f, 0.f, 26.f));

    UButton* StartBtn = OVStyle::MakeMenuButton(WidgetTree, LeftMenuVBox, TEXT("작전 개시 · 임무 선택"), TEXT(">"), true);
    StartBtn->OnClicked.AddDynamic(this, &UOVTitleScreenWidget::HandleOpenScenarios);

    OVStyle::MakeMenuButton(WidgetTree, LeftMenuVBox, TEXT("캠페인"), TEXT("준비 중"), false, true);

    UButton* SandboxBtn = OVStyle::MakeMenuButton(WidgetTree, LeftMenuVBox, TEXT("자유 전투 시험장"), TEXT("자유 배치"), false);
    SandboxBtn->OnClicked.AddDynamic(this, &UOVTitleScreenWidget::HandleOpenSandbox);

    auto* PracticeBtn=OVStyle::MakeMenuButton(WidgetTree,LeftMenuVBox,TEXT("시스템 실습"),TEXT("직접 연습"),false);
    PracticeBtn->OnClicked.AddDynamic(this,&UOVTitleScreenWidget::OpenPractice);
    UButton* TutorialBtn = OVStyle::MakeMenuButton(WidgetTree, LeftMenuVBox, TEXT("게임 가이드 / 튜토리얼"), TEXT("5단계 요약"), false);
    TutorialBtn->OnClicked.AddDynamic(this, &UOVTitleScreenWidget::HandleOpenTutorial);

    UButton* SettingsBtn = OVStyle::MakeMenuButton(WidgetTree, LeftMenuVBox, TEXT("환경 설정"), TEXT("설정"), false);
    SettingsBtn->OnClicked.AddDynamic(this, &UOVTitleScreenWidget::HandleOpenSettings);

    UButton* QuitBtn = OVStyle::MakeMenuButton(WidgetTree, LeftMenuVBox, TEXT("게임 종료"), TEXT(""), false);
    QuitBtn->OnClicked.AddDynamic(this, &UOVTitleScreenWidget::HandleQuitGame);

    USpacer* LeftSpacer = WidgetTree->ConstructWidget<USpacer>();
    if (UVerticalBoxSlot* SpacerSlot = LeftMenuVBox->AddChildToVerticalBox(LeftSpacer))
    {
        SpacerSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    }

    UHorizontalBox* LeftFooter = WidgetTree->ConstructWidget<UHorizontalBox>();
    LeftMenuVBox->AddChildToVerticalBox(LeftFooter)->SetPadding(FMargin(0.f, 14.f, 0.f, 0.f));
    LeftFooter->AddChildToHorizontalBox(OVStyle::MakeLabel(
        WidgetTree, TEXT("F5 · F6 · F7  빠른 함대전"), 11, OVStyle::TextDim, false));
    USpacer* FootSpacer = WidgetTree->ConstructWidget<USpacer>();
    LeftFooter->AddChildToHorizontalBox(FootSpacer)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    FooterUIScaleText = OVStyle::MakeLabel(WidgetTree, TEXT("UI 배율: 100%"), 11, OVStyle::TextDim, false, EOVTextRole::Numeric);
    LeftFooter->AddChildToHorizontalBox(FooterUIScaleText);

    TitlePanelContainer = WidgetTree->ConstructWidget<UBorder>();
    TitlePanelContainer->SetBrush(OVStyle::MakeRoundedBoxBrush(FLinearColor::Transparent, FLinearColor::Transparent, 0.f, 0.f));
    TitlePanelContainer->SetPadding(FMargin(0.f));
    TitlePanelContainer->SetContent(LeftScrimBox);
    if (UOverlaySlot* TitleSlot = RootOverlay->AddChildToOverlay(TitlePanelContainer))
    {
        TitleSlot->SetHorizontalAlignment(HAlign_Left);
        TitleSlot->SetVerticalAlignment(VAlign_Fill);
    }

    // =========================================================================
    // LAYER 3: SCREEN 02 - SCENARIO SELECT MODAL CARD
    // =========================================================================
    ScenarioPanelContainer = WidgetTree->ConstructWidget<UBorder>();
    ScenarioPanelContainer->SetBrush(OVStyle::MakeRoundedBoxBrush(
        FLinearColor(0.025f, 0.055f, 0.095f, 0.94f), OVStyle::SlateBorder, 8.0f, 1.0f));
    ScenarioPanelContainer->SetPadding(FMargin(28.f, 22.f, 28.f, 24.f));
    ScenarioPanelContainer->SetVisibility(ESlateVisibility::Collapsed);

    UScaleBox* FitBriefing = WidgetTree->ConstructWidget<UScaleBox>();
    FitBriefing->SetStretch(EStretch::ScaleToFit);
    FitBriefing->SetStretchDirection(EStretchDirection::DownOnly);
    FitBriefing->SetContent(ScenarioPanelContainer);
    // The empty wrapper must never consume input from another title panel.
    FitBriefing->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    UOverlaySlot* FitSlot = RootOverlay->AddChildToOverlay(FitBriefing);
    FitSlot->SetPadding(FMargin(16));
    FitSlot->SetHorizontalAlignment(HAlign_Fill);
    FitSlot->SetVerticalAlignment(VAlign_Fill);

    USizeBox* ModalCardBox = WidgetTree->ConstructWidget<USizeBox>();
    ModalCardBox->SetWidthOverride(1130.f);
    ModalCardBox->SetHeightOverride(710.f);
    ScenarioPanelContainer->SetContent(ModalCardBox);

    UVerticalBox* ModalVBox = WidgetTree->ConstructWidget<UVerticalBox>();
    ModalCardBox->SetContent(ModalVBox);

    UHorizontalBox* ModalHeader = WidgetTree->ConstructWidget<UHorizontalBox>();
    ModalVBox->AddChildToVerticalBox(ModalHeader)->SetPadding(FMargin(0.f, 0.f, 0.f, 16.f));

    UVerticalBox* HeaderTitles = WidgetTree->ConstructWidget<UVerticalBox>();
    ModalHeader->AddChildToHorizontalBox(HeaderTitles)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    HeaderTitles->AddChildToVerticalBox(OVStyle::MakeLabel(
        WidgetTree, FString::Printf(TEXT("임무 선택  ·  %d종"), UOVPlayScenarioSubsystem::Definitions().Num()),
        20, OVStyle::TextWhite, true));
    HeaderTitles->AddChildToVerticalBox(OVStyle::MakeLabel(
        WidgetTree, TEXT("임무를 선택하고 아군 편성과 목표를 확인하세요."), 11, OVStyle::TextDim, false))
        ->SetPadding(FMargin(0.f, 3.f, 0.f, 0.f));



    UButton* DifficultyButton=WidgetTree->ConstructWidget<UButton>();
    DifficultyButton->SetStyle(OVStyle::MakeSecondaryButtonStyle(false));
    DifficultyText=OVStyle::MakeLabel(WidgetTree,TEXT("난이도 · 보통"),14,OVStyle::TextWhite,true);
    DifficultyButton->AddChild(DifficultyText);
    DifficultyButton->OnClicked.AddDynamic(this,&UOVTitleScreenWidget::CycleScenarioDifficulty);
    ModalHeader->AddChildToHorizontalBox(DifficultyButton)->SetVerticalAlignment(VAlign_Top);

    UButton* BackToTitleBtn = WidgetTree->ConstructWidget<UButton>();
    BackToTitleBtn->SetStyle(OVStyle::MakeSecondaryButtonStyle(false));
    CardReturnLabel = OVStyle::MakeLabel(WidgetTree, TEXT("타이틀로"), 14, OVStyle::TextWhite, true);
    BackToTitleBtn->AddChild(CardReturnLabel);
    BackToTitleBtn->OnClicked.AddDynamic(this, &UOVTitleScreenWidget::HandleBackToTitle);
    if (UHorizontalBoxSlot* BackSlot = ModalHeader->AddChildToHorizontalBox(BackToTitleBtn))
    {
        BackSlot->SetPadding(FMargin(12.f, 4.f));
        BackSlot->SetVerticalAlignment(VAlign_Top);
    }

    UHorizontalBox* MainSplit = WidgetTree->ConstructWidget<UHorizontalBox>();
    if (UVerticalBoxSlot* SplitSlot = ModalVBox->AddChildToVerticalBox(MainSplit))
    {
        SplitSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    }

    USizeBox* LeftListWidth = WidgetTree->ConstructWidget<USizeBox>();
    LeftListWidth->SetWidthOverride(320.f);
    MainSplit->AddChildToHorizontalBox(LeftListWidth);

    ScenarioListBox = WidgetTree->ConstructWidget<UVerticalBox>();
    auto* SceneScroll = WidgetTree->ConstructWidget<UScrollBox>();
    LeftListWidth->SetContent(SceneScroll);
    SceneScroll->AddChild(ScenarioListBox);

    for (const FOVPlaySceneDefinition& Def : UOVPlayScenarioSubsystem::Definitions())
    {
        UOVScenarioPickButton* PickBtn = WidgetTree->ConstructWidget<UOVScenarioPickButton>();
        PickBtn->OwnerWidget = this;
        PickBtn->ScenarioId = Def.Id;

        UHorizontalBox* BtnContent = WidgetTree->ConstructWidget<UHorizontalBox>();
        PickBtn->AddChild(BtnContent);

        const FString PhotoId=Def.Id.ToString();
        const FString AssetPath = Def.Id==TEXT("HeroArena") ? FString(TEXT("/Game/UI/Heroes/M02/T_portrait.T_portrait")) : FString::Printf(TEXT("/Game/UI/MissionCards/%s.%s"), *PhotoId, *PhotoId);
        UTexture2D* Photo = LoadObject<UTexture2D>(nullptr, *AssetPath);
        ScenarioPhotos.Add(Def.Id, Photo);
        UImage* Thumbnail = WidgetTree->ConstructWidget<UImage>();
        Thumbnail->SetBrushFromTexture(Photo,true);
        auto* thumbnailFit=WidgetTree->ConstructWidget<UScaleBox>();
        thumbnailFit->SetStretch(EStretch::ScaleToFit);
        thumbnailFit->SetContent(Thumbnail);
        USizeBox* Tile = WidgetTree->ConstructWidget<USizeBox>();
        Tile->SetWidthOverride(104);
        Tile->SetHeightOverride(52);
        Tile->SetContent(thumbnailFit);
        CastChecked<UButtonSlot>(BtnContent->Slot)->SetHorizontalAlignment(HAlign_Fill);
        BtnContent->AddChildToHorizontalBox(Tile)->SetVerticalAlignment(VAlign_Center);

        UVerticalBox* TextCol = WidgetTree->ConstructWidget<UVerticalBox>();
        if (UHorizontalBoxSlot* TextSlot = BtnContent->AddChildToHorizontalBox(TextCol))
        {
            TextSlot->SetPadding(FMargin(12.f, 6.f, 8.f, 6.f));
            TextSlot->SetVerticalAlignment(VAlign_Center);
        }

        PickBtn->TitleText = OVStyle::MakeLabel(WidgetTree, Def.Title, 15, OVStyle::TextWhite, true);
        PickBtn->TitleText->SetAutoWrapText(false);
        TextCol->AddChildToVerticalBox(PickBtn->TitleText);

        PickBtn->SetSelectedState(Def.Id == SelectedScenarioId);
        PickBtn->OnClicked.AddDynamic(PickBtn, &UOVScenarioPickButton::HandleClicked);

        ScenarioListBox->AddChildToVerticalBox(PickBtn)->SetPadding(FMargin(0.f, 2.5f));
        ScenarioButtons.Add(PickBtn);
    }

    USizeBox* RightCardWidth = WidgetTree->ConstructWidget<USizeBox>();
    RightCardWidth->SetWidthOverride(620.f);
    if (UHorizontalBoxSlot* RightSlot = MainSplit->AddChildToHorizontalBox(RightCardWidth))
    {
        RightSlot->SetPadding(FMargin(18.f, 0.f, 0.f, 0.f));
        RightSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    }

    UBorder* BriefingBorder = WidgetTree->ConstructWidget<UBorder>();
    BriefingBorder->SetBrush(OVStyle::MakeRoundedBoxBrush(
        FLinearColor(0.018f, 0.038f, 0.068f, 0.88f), OVStyle::SlateBorder, 6.0f, 1.0f));
    BriefingBorder->SetPadding(FMargin(20.f, 18.f));
    RightCardWidth->SetContent(BriefingBorder);

    UVerticalBox* BriefingBox = WidgetTree->ConstructWidget<UVerticalBox>();
    BriefingBorder->SetContent(BriefingBox);

    ScenarioPhoto = WidgetTree->ConstructWidget<UImage>();
    USizeBox* PhotoFrame = WidgetTree->ConstructWidget<USizeBox>();
    PhotoFrame->SetHeightOverride(185);
    UScaleBox* PhotoFit = WidgetTree->ConstructWidget<UScaleBox>();
    PhotoFit->SetStretch(EStretch::ScaleToFit);
    PhotoFit->SetContent(ScenarioPhoto);
    PhotoFrame->SetContent(PhotoFit);
    BriefingBox->AddChildToVerticalBox(PhotoFrame)->SetPadding(FMargin(0,0,0,10));

    UHorizontalBox* BriefHeader = WidgetTree->ConstructWidget<UHorizontalBox>();
    BriefingBox->AddChildToVerticalBox(BriefHeader)->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));

    ScenarioTitleText = OVStyle::MakeLabel(WidgetTree, TEXT("200코스트 함대전"), 22, OVStyle::TextWhite, true);
    BriefHeader->AddChildToHorizontalBox(ScenarioTitleText);

    USpacer* BriefSpacer = WidgetTree->ConstructWidget<USpacer>();
    BriefHeader->AddChildToHorizontalBox(BriefSpacer)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

    ScenarioBadgeText = OVStyle::MakeLabel(WidgetTree, TEXT("즉시 출격 가능"), 11, OVStyle::CyanAccent, true);
    BriefHeader->AddChildToHorizontalBox(ScenarioBadgeText)->SetVerticalAlignment(VAlign_Center);

    ScenarioObjectiveText = OVStyle::MakeLabel(
        WidgetTree, TEXT(""), 13, OVStyle::TextSlate, false);
    BriefingBox->AddChildToVerticalBox(ScenarioObjectiveText)->SetPadding(FMargin(0.f, 0.f, 0.f, 16.f));

    UBorder* MetricsCard = WidgetTree->ConstructWidget<UBorder>();
    MetricsCard->SetBrush(OVStyle::MakeRoundedBoxBrush(
        FLinearColor(0.010f, 0.020f, 0.038f, 0.95f), OVStyle::SubtleBorder, 4.0f, 1.0f));
    MetricsCard->SetPadding(FMargin(16.f, 10.f));
    BriefingBox->AddChildToVerticalBox(MetricsCard)->SetPadding(FMargin(0.f, 0.f, 0.f, 16.f));

    UHorizontalBox* MetricsHBox = WidgetTree->ConstructWidget<UHorizontalBox>();
    MetricsCard->SetContent(MetricsHBox);

    auto AddMetricCol = [this, &MetricsHBox](const FString& Label, TObjectPtr<UTextBlock>& ValPtr, EOVTextRole Role=EOVTextRole::Numeric)
    {
        UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>();
        UTextBlock* MetricTitle=OVStyle::MakeLabel(WidgetTree, Label, 11, OVStyle::TextDim, false);
        Col->AddChildToVerticalBox(MetricTitle);
        ValPtr = OVStyle::MakeLabel(WidgetTree, TEXT("-"), 17, OVStyle::TextWhite, false, Role);
        ValPtr->SetJustification(ETextJustify::Right);
        Col->AddChildToVerticalBox(ValPtr)->SetPadding(FMargin(0.f, 2.f, 0.f, 0.f));
        if (UHorizontalBoxSlot* ColSlot = MetricsHBox->AddChildToHorizontalBox(Col))
        {
            ColSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        }
        return MetricTitle;
    };
    AddMetricCol(TEXT("편성 비용"), MetricBudgetVal);
    ScenarioHullCountTitle = AddMetricCol(TEXT("아군 함선"), MetricHangarVal);
    AddMetricCol(TEXT("임무 유형"), MetricTimeVal, EOVTextRole::Body);

    ScenarioRosterTitle=OVStyle::MakeLabel(WidgetTree, TEXT("아군 출격 편성"), 12, OVStyle::CyanAccent, true);
    BriefingBox->AddChildToVerticalBox(ScenarioRosterTitle)
        ->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));

    UHorizontalBox* RosterSplit = WidgetTree->ConstructWidget<UHorizontalBox>();
    if (UVerticalBoxSlot* RosterSlot = BriefingBox->AddChildToVerticalBox(RosterSplit))
    {
        RosterSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        RosterSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 14.f));
    }

    RosterColLeft = WidgetTree->ConstructWidget<UVerticalBox>();
    if (UHorizontalBoxSlot* RosterLSlot = RosterSplit->AddChildToHorizontalBox(RosterColLeft))
    {
        RosterLSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        RosterLSlot->SetPadding(FMargin(0.f, 0.f, 12.f, 0.f));
    }

    RosterColRight = WidgetTree->ConstructWidget<UVerticalBox>();
    if (UHorizontalBoxSlot* RosterRSlot = RosterSplit->AddChildToHorizontalBox(RosterColRight))
    {
        RosterRSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    }

    UHorizontalBox* BottomActionRow = WidgetTree->ConstructWidget<UHorizontalBox>();
    BriefingBox->AddChildToVerticalBox(BottomActionRow);

    BottomActionRow->AddChildToHorizontalBox(WidgetTree->ConstructWidget<USpacer>())
        ->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

    LaunchScenarioButton = WidgetTree->ConstructWidget<UButton>();
    LaunchScenarioButton->SetStyle(OVStyle::MakePrimaryButtonStyle());
    UTextBlock* LaunchText = OVStyle::MakeLabel(WidgetTree, TEXT("작전 출격  ▷"), 17, OVStyle::DarkText, true);
    LaunchText->SetAutoWrapText(false);
    LaunchScenarioButton->AddChild(LaunchText);
    LaunchScenarioButton->OnClicked.AddDynamic(this, &UOVTitleScreenWidget::HandleLaunchSelectedScenario);
    if (UHorizontalBoxSlot* LaunchSlot = BottomActionRow->AddChildToHorizontalBox(LaunchScenarioButton))
    {
        LaunchSlot->SetPadding(FMargin(18.f, 8.f));
        LaunchSlot->SetVerticalAlignment(VAlign_Center);
    }

    // LAYER 4: EXPANDED SETTINGS MODAL
    BuildSettingsModal(RootOverlay);

    // LAYER 5: 5-STEP BEGINNER TUTORIAL / GAME GUIDE MODAL
    BuildTutorialModal(RootOverlay);
    BuildPractice(RootOverlay);BuildSortie(RootOverlay);

    RefreshScenarioBriefing();
    RefreshModeLabel();
}

void UOVTitleScreenWidget::BuildSettingsModal(UOverlay* RootOverlay)
{
    SettingsPanelContainer = WidgetTree->ConstructWidget<UBorder>();
    SettingsPanelContainer->SetBrush(OVStyle::MakeRoundedBoxBrush(
        FLinearColor(0.025f, 0.055f, 0.095f, 0.96f), OVStyle::SlateBorder, 8.0f, 1.0f));
    SettingsPanelContainer->SetPadding(FMargin(28.f, 22.f));
    SettingsPanelContainer->SetVisibility(ESlateVisibility::Collapsed);

    if (UOverlaySlot* SettingsSlot = RootOverlay->AddChildToOverlay(SettingsPanelContainer))
    {
        SettingsSlot->SetHorizontalAlignment(HAlign_Center);
        SettingsSlot->SetVerticalAlignment(VAlign_Center);
    }

    USizeBox* SettingsSize = WidgetTree->ConstructWidget<USizeBox>();
    SettingsSize->SetWidthOverride(640.f);
    SettingsPanelContainer->SetContent(SettingsSize);

    UVerticalBox* SettingsBox = WidgetTree->ConstructWidget<UVerticalBox>();
    SettingsSize->SetContent(SettingsBox);

    SettingsBox->AddChildToVerticalBox(OVStyle::MakeLabel(
        WidgetTree, TEXT("환경 설정 · 화면 / 그래픽 / 오디오 / UI"), 20, OVStyle::TextWhite, true))
        ->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));

    auto AddCycleRow = [&](const FString& RowLabel, TObjectPtr<UTextBlock>& OutValueText, UButton*& OutButton)
    {
        UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
        SettingsBox->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.f, 4.f));

        UTextBlock* Label = OVStyle::MakeLabel(WidgetTree, RowLabel, 13, OVStyle::TextSlate, true);
        Label->SetAutoWrapText(false);
        if (UHorizontalBoxSlot* LSlot = Row->AddChildToHorizontalBox(Label))
        {
            LSlot->SetVerticalAlignment(VAlign_Center);
            LSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        }

        OutButton = WidgetTree->ConstructWidget<UButton>();
        OutButton->SetStyle(OVStyle::MakeSecondaryButtonStyle(false));
        OutValueText = OVStyle::MakeLabel(WidgetTree, TEXT("-"), 13, OVStyle::CyanAccent, true);
        OutValueText->SetAutoWrapText(false);
        OutButton->AddChild(OutValueText);

        USizeBox* BtnWidth = WidgetTree->ConstructWidget<USizeBox>();
        BtnWidth->SetWidthOverride(220.f);
        BtnWidth->SetContent(OutButton);
        if (UHorizontalBoxSlot* BSlot = Row->AddChildToHorizontalBox(BtnWidth))
        {
            BSlot->SetVerticalAlignment(VAlign_Center);
        }
    };

    UButton* WindowModeBtn = nullptr;
    AddCycleRow(TEXT("화면 모드"), SettingsWindowModeValueText, WindowModeBtn);
    WindowModeBtn->OnClicked.AddDynamic(this, &UOVTitleScreenWidget::HandleCycleWindowMode);

    UButton* ResolutionBtn = nullptr;
    AddCycleRow(TEXT("해상도"), SettingsResolutionValueText, ResolutionBtn);
    ResolutionBtn->OnClicked.AddDynamic(this, &UOVTitleScreenWidget::HandleCycleResolution);

    UButton* QualityBtn = nullptr;
    AddCycleRow(TEXT("그래픽 품질 프리셋"), SettingsQualityValueText, QualityBtn);
    QualityBtn->OnClicked.AddDynamic(this, &UOVTitleScreenWidget::HandleCycleGraphicsQuality);

    auto AddSliderRow = [&](const FString& RowLabel, float MinVal, float MaxVal, float Step,
        TObjectPtr<USlider>& OutSlider, TObjectPtr<UTextBlock>& OutValText)
    {
        UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
        SettingsBox->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.f, 5.f));

        USizeBox* LabelBox = WidgetTree->ConstructWidget<USizeBox>();
        LabelBox->SetWidthOverride(170.f);
        UTextBlock* Label = OVStyle::MakeLabel(WidgetTree, RowLabel, 13, OVStyle::TextSlate, true);
        Label->SetAutoWrapText(false);
        LabelBox->SetContent(Label);
        Row->AddChildToHorizontalBox(LabelBox)->SetVerticalAlignment(VAlign_Center);

        OutSlider = WidgetTree->ConstructWidget<USlider>();
        OutSlider->SetMinValue(MinVal);
        OutSlider->SetMaxValue(MaxVal);
        OutSlider->SetStepSize(Step);
        OutSlider->SetSliderBarColor(OVStyle::SlateBorder);
        OutSlider->SetSliderHandleColor(OVStyle::CyanAccent);
        if (UHorizontalBoxSlot* SSlot = Row->AddChildToHorizontalBox(OutSlider))
        {
            SSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
            SSlot->SetPadding(FMargin(10.f, 0.f));
            SSlot->SetVerticalAlignment(VAlign_Center);
        }

        USizeBox* ValBox = WidgetTree->ConstructWidget<USizeBox>();
        ValBox->SetWidthOverride(64.f);
        OutValText = OVStyle::MakeLabel(WidgetTree, TEXT("100%"), 13, OVStyle::TextWhite, false, EOVTextRole::Numeric);
        OutValText->SetJustification(ETextJustify::Right);
        OutValText->SetAutoWrapText(false);
        ValBox->SetContent(OutValText);
        Row->AddChildToHorizontalBox(ValBox)->SetVerticalAlignment(VAlign_Center);
    };

    AddSliderRow(TEXT("마스터 볼륨"), 0.0f, 1.0f, 0.05f, MasterVolumeSlider, SettingsMasterVolText);
    MasterVolumeSlider->OnValueChanged.AddDynamic(this, &UOVTitleScreenWidget::HandleMasterVolumeChanged);

    AddSliderRow(TEXT("BGM 볼륨"), 0.0f, 1.0f, 0.05f, BGMVolumeSlider, SettingsBGMVolText);
    BGMVolumeSlider->OnValueChanged.AddDynamic(this, &UOVTitleScreenWidget::HandleBGMVolumeChanged);

    AddSliderRow(TEXT("효과음 (SFX) 볼륨"), 0.0f, 1.0f, 0.05f, SFXVolumeSlider, SettingsSFXVolText);
    SFXVolumeSlider->OnValueChanged.AddDynamic(this, &UOVTitleScreenWidget::HandleSFXVolumeChanged);

    AddSliderRow(TEXT("UI 배율 (80~150%)"), 0.80f, 1.50f, 0.05f, UIScaleSlider, SettingsUIScaleText);
    UIScaleSlider->OnValueChanged.AddDynamic(this, &UOVTitleScreenWidget::HandleUIScaleChanged);

    SettingsBackdropModeText = OVStyle::MakeLabel(WidgetTree, TEXT(""), 12, OVStyle::TextDim, false);
    SettingsBox->AddChildToVerticalBox(SettingsBackdropModeText)->SetPadding(FMargin(0.f, 10.f, 0.f, 6.f));

    UButton* ToggleBackdropBtn = OVStyle::MakeMenuButton(
        WidgetTree, SettingsBox, TEXT("타이틀 배경 방식 전환 (방식 A 실시간 3D ↔ 방식 B 사전 렌더)"), TEXT("전환"), false);
    ToggleBackdropBtn->OnClicked.AddDynamic(this, &UOVTitleScreenWidget::HandleToggleBackdropMode);

    SettingsFeedbackText = OVStyle::MakeLabel(WidgetTree, TEXT("변경 후 [적용 및 저장]을 누르면 로컬 설정에 기록됩니다."), 11, OVStyle::TextDim, false);
    SettingsBox->AddChildToVerticalBox(SettingsFeedbackText)->SetPadding(FMargin(0.f, 6.f, 0.f, 8.f));

    UButton* ApplyBtn = OVStyle::MakeMenuButton(
        WidgetTree, SettingsBox, TEXT("설정 적용 및 저장"), TEXT("Save"), true);
    ApplyBtn->OnClicked.AddDynamic(this, &UOVTitleScreenWidget::HandleApplyAndSaveSettings);

    UButton* BackBtn = OVStyle::MakeMenuButton(
        WidgetTree, SettingsBox, TEXT("← 타이틀로 돌아가기"), TEXT(""), false);
    BackBtn->OnClicked.AddDynamic(this, &UOVTitleScreenWidget::HandleBackToTitle);
}

void UOVTitleScreenWidget::BuildTutorialModal(UOverlay* RootOverlay)
{
    TutorialPanelContainer = WidgetTree->ConstructWidget<UBorder>();
    TutorialPanelContainer->SetBrush(OVStyle::MakeRoundedBoxBrush(
        FLinearColor(0.020f, 0.045f, 0.082f, 0.96f), OVStyle::CyanBorder, 8.0f, 1.2f));
    TutorialPanelContainer->SetPadding(FMargin(28.f, 22.f));
    TutorialPanelContainer->SetVisibility(ESlateVisibility::Collapsed);

    if (UOverlaySlot* TutSlot = RootOverlay->AddChildToOverlay(TutorialPanelContainer))
    {
        TutSlot->SetHorizontalAlignment(HAlign_Center);
        TutSlot->SetVerticalAlignment(VAlign_Center);
    }

    USizeBox* TutSize = WidgetTree->ConstructWidget<USizeBox>();
    TutSize->SetWidthOverride(860.f);
    TutorialPanelContainer->SetContent(TutSize);

    UVerticalBox* TutBox = WidgetTree->ConstructWidget<UVerticalBox>();
    TutSize->SetContent(TutBox);

    TutBox->AddChildToVerticalBox(OVStyle::MakeLabel(
        WidgetTree, TEXT("오비탈 뱅가드 · 초보자 핵심 가이드 (5단계)"), 21, OVStyle::TextWhite, true));
    TutBox->AddChildToVerticalBox(OVStyle::MakeLabel(
        WidgetTree, TEXT("처음 지휘하는 사령관을 위한 핵심 진행 흐름입니다. 전투 중 상단 [가이드] 버튼으로 언제든 다시 열 수 있습니다."),
        12, OVStyle::TextSlate, false))->SetPadding(FMargin(0.f, 4.f, 0.f, 14.f));

    struct FGuideStep
    {
        const TCHAR* Badge;
        const TCHAR* Title;
        const TCHAR* Body;
    };
    const FGuideStep Steps[] = {
        { TEXT("STEP 01"), TEXT("① 어떤 게임인가 — 실시간 + 언제든 일시정지 우주 함대전 RTS"),
          TEXT("Space로 시간을 멈추거나 재개하세요. 숫자 1·2·3은 해당 배속으로 진행합니다. Ctrl+숫자로 선택 함대를 등록하고 Alt+1·2·3으로 앞 세 부대를 다시 선택할 수 있습니다.") },
        { TEXT("STEP 02"), TEXT("② 기지와 보급 · 채굴한 자원 운송"),
          TEXT("은하 지도에서 인접 성계로 FTL 확장하세요. [자원 전초기지]는 채굴선 하역·창고·중형 수송선 왕복 보급 허브 역할을 하며, [전투 전초기지]는 방어 거점으로 분리 운용됩니다.") },
        { TEXT("STEP 03"), TEXT("③ 함선 설계 · 선내 시설과 외장 무장"),
          TEXT("출격 준비의 층별 도면에 시설 카드를 놓고, 외장 무장 화면에서 함선을 돌려 장착점을 찾으세요. 함포는 탄약고, 빔은 축전실, 미사일은 미사일 적재실이 필요합니다.") },
        { TEXT("STEP 04"), TEXT("④ 영웅 선내 강습 · 나포 — 내부 핵심 시설 제압과 방어"),
          TEXT("포드 발사실·강습선으로 영웅과 승무원을 적함 내부에 투입해 코어·함교·무기제어실을 점령하거나, 아군 함선 내부에서 침투 방어·긴급 수리를 지휘합니다.") },
        { TEXT("STEP 05"), TEXT("⑤ 함대전 & 승리 조건 — F5/F6/F7 및 블랙홀(E1) 전술 운용"),
          TEXT("표적 지정 후 쉴드·장갑·무장을 무력화해 적 전투 전력을 소실시키면 승리합니다. 블랙홀(중력권 1,000m · 중심 45m 즉사)은 FTL 충전 중에도 끌어당기므로 항로 관리가 핵심입니다.") }
    };

    for (const FGuideStep& Step : Steps)
    {
        UBorder* StepCard = WidgetTree->ConstructWidget<UBorder>();
        StepCard->SetBrush(OVStyle::MakeRoundedBoxBrush(
            FLinearColor(0.012f, 0.026f, 0.048f, 0.94f), OVStyle::SlateBorder, 6.0f, 1.0f));
        StepCard->SetPadding(FMargin(14.f, 10.f));
        TutBox->AddChildToVerticalBox(StepCard)->SetPadding(FMargin(0.f, 4.f));

        UVerticalBox* CardVBox = WidgetTree->ConstructWidget<UVerticalBox>();
        StepCard->SetContent(CardVBox);

        UHorizontalBox* TitleRow = WidgetTree->ConstructWidget<UHorizontalBox>();
        CardVBox->AddChildToVerticalBox(TitleRow);

        UTextBlock* Badge = OVStyle::MakeLabel(WidgetTree, Step.Badge, 11, OVStyle::CyanAccent, true);
        Badge->SetAutoWrapText(false);
        TitleRow->AddChildToHorizontalBox(Badge)->SetPadding(FMargin(0.f, 1.f, 10.f, 0.f));

        TitleRow->AddChildToHorizontalBox(OVStyle::MakeLabel(WidgetTree, Step.Title, 13, OVStyle::TextWhite, true));

        CardVBox->AddChildToVerticalBox(OVStyle::MakeLabel(WidgetTree, Step.Body, 12, OVStyle::TextSlate, false))
            ->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
    }

    UButton* CloseBtn = WidgetTree->ConstructWidget<UButton>();
    CloseBtn->SetStyle(OVStyle::MakePrimaryButtonStyle());
    TutorialCloseButtonText = OVStyle::MakeLabel(WidgetTree, TEXT("← 타이틀로 돌아가기"), 14, OVStyle::DarkText, true);
    TutorialCloseButtonText->SetAutoWrapText(false);
    CloseBtn->AddChild(TutorialCloseButtonText);
    CloseBtn->OnClicked.AddDynamic(this, &UOVTitleScreenWidget::HandleCloseTutorial);
    if (UVerticalBoxSlot* CloseSlot = TutBox->AddChildToVerticalBox(CloseBtn))
    {
        CloseSlot->SetPadding(FMargin(0.f, 14.f, 0.f, 0.f));
        CloseSlot->SetHorizontalAlignment(HAlign_Right);
    }
}

void UOVTitleScreenWidget::SyncSettingsWidgetsFromSystem()
{
    bSuppressSliderCallbacks = true;

    if (UGameUserSettings* GUS = GEngine ? GEngine->GetGameUserSettings() : nullptr)
    {
        const EWindowMode::Type Mode = GUS->GetFullscreenMode();
        WindowModeIndex = (Mode == EWindowMode::Fullscreen) ? 0 : (Mode == EWindowMode::Windowed ? 2 : 1);

        const FIntPoint CurRes = GUS->GetScreenResolution();
        ResolutionIndex = INDEX_NONE;
        CurrentResolution = CurRes;
        for (int32 I = 0; I < UE_ARRAY_COUNT(OVStyle::ResolutionPresets); ++I)
        {
            if (OVStyle::ResolutionPresets[I] == CurRes)
            {
                ResolutionIndex = I;
                break;
            }
        }

        const int32 Quality = GUS->GetOverallScalabilityLevel();
        GraphicsQualityIndex = Quality;
    }

    if (UOVAudioSubsystem* Audio = UOVAudioSubsystem::Get(this))
    {
        PendingMasterVolume = Audio->GetMasterVolume();
        PendingBGMVolume = Audio->GetBGMVolume();
        PendingSFXVolume = Audio->GetSFXVolume();
    }

    if (UWorld* World = GetWorld())
    {
        if (APlayerController* PC = World->GetFirstPlayerController())
        {
            if (AOVHUD* HUD = Cast<AOVHUD>(PC->GetHUD()))
            {
                PendingUIScale = HUD->GetUserUIScaleMultiplier();
            }
        }
    }

    if (MasterVolumeSlider) MasterVolumeSlider->SetValue(PendingMasterVolume);
    if (BGMVolumeSlider) BGMVolumeSlider->SetValue(PendingBGMVolume);
    if (SFXVolumeSlider) SFXVolumeSlider->SetValue(PendingSFXVolume);
    if (UIScaleSlider) UIScaleSlider->SetValue(PendingUIScale);

    bSuppressSliderCallbacks = false;
    RefreshSettingsLabels();
}

void UOVTitleScreenWidget::RefreshSettingsLabels()
{
    if (SettingsWindowModeValueText)
    {
        SettingsWindowModeValueText->SetText(FText::FromString(
            OVStyle::WindowModeLabels[FMath::Clamp(WindowModeIndex, 0, 2)]));
    }
    if (SettingsResolutionValueText)
    {
        const FIntPoint Res = ResolutionIndex == INDEX_NONE ? CurrentResolution : OVStyle::ResolutionPresets[ResolutionIndex];
        SettingsResolutionValueText->SetText(FText::FromString(FString::Printf(TEXT("%d × %d"), Res.X, Res.Y)));
    }
    if (SettingsQualityValueText)
    {
        SettingsQualityValueText->SetText(FText::FromString(
            GraphicsQualityIndex >= 0 && GraphicsQualityIndex < UE_ARRAY_COUNT(OVStyle::QualityLabels)
                ? OVStyle::QualityLabels[GraphicsQualityIndex] : TEXT("개별 설정")));
    }
    if (SettingsMasterVolText)
    {
        SettingsMasterVolText->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(PendingMasterVolume * 100.f))));
    }
    if (SettingsBGMVolText)
    {
        SettingsBGMVolText->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(PendingBGMVolume * 100.f))));
    }
    if (SettingsSFXVolText)
    {
        SettingsSFXVolText->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(PendingSFXVolume * 100.f))));
    }
    if (SettingsUIScaleText)
    {
        SettingsUIScaleText->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(PendingUIScale * 100.f))));
    }
    if (FooterUIScaleText)
    {
        FooterUIScaleText->SetText(FText::FromString(FString::Printf(TEXT("UI 배율: %d%%"), FMath::RoundToInt(PendingUIScale * 100.f))));
    }
}

void UOVTitleScreenWidget::RefreshTutorialReturnLabel()
{
    if (TutorialCloseButtonText)
    {
        TutorialCloseButtonText->SetText(FText::FromString(
            bOpenedFromInGame ? TEXT("← 전투로 돌아가기") : TEXT("← 타이틀로 돌아가기")));
    }
}

void UOVTitleScreenWidget::ActivateTitleScreen()
{
    bOpenedFromInGame = false;
    if (!BackdropStage)
    {
        BackdropStage = NewObject<UOVTitleBackdropStage>(this);
    }

    if (UWorld* World = GetWorld())
    {
        BackdropStage->Activate(World, SelectedScenarioId, PreferredBackdropMode);
        bBackdropActive = true;
    }

    if (UOVAudioSubsystem* Audio = UOVAudioSubsystem::Get(this))
    {
        Audio->PlayTitleBGM();
    }

    SyncSettingsWidgetsFromSystem();
    RefreshTutorialReturnLabel();
    ShowPanel(EOVTitleMenuPanel::Title);
    RefreshBackdropBrush();
    RefreshModeLabel();
    SetVisibility(ESlateVisibility::Visible);
}

void UOVTitleScreenWidget::ActivateInGameTutorial()
{
    ActivateFieldPanel(EOVTitleMenuPanel::Tutorial);
}

void UOVTitleScreenWidget::ActivateFieldPanel(EOVTitleMenuPanel Panel)
{
    bOpenedFromInGame = true;
    bBackdropActive = false;
    if (BackdropImage)
    {
        BackdropImage->SetVisibility(ESlateVisibility::Collapsed);
    }
    RefreshTutorialReturnLabel();
    ShowPanel(Panel);
    SetVisibility(ESlateVisibility::Visible);
}

void UOVTitleScreenWidget::DeactivateTitleScreen()
{
    SortieViewHull=INDEX_NONE;
    SortieScrollStage=INDEX_NONE;
    if(SortieMirror)SortieMirror->Release();
    bBackdropActive = false;
    bOpenedFromInGame = false;
    SetVisibility(ESlateVisibility::Collapsed);
    if (BackdropStage)
    {
        BackdropStage->Release();
    }
    if (UOVAudioSubsystem* Audio = UOVAudioSubsystem::Get(this))
    {
        Audio->StopTitleBGM();
    }
}

void UOVTitleScreenWidget::ReleaseBackdrop()
{
    bBackdropActive = false;
    if (BackdropStage)
    {
        BackdropStage->Release();
    }
}

void UOVTitleScreenWidget::ShowPanel(EOVTitleMenuPanel Panel)
{
    CurrentPanel = Panel;
    if(SortiePanel)SortiePanel->SetVisibility(Panel==EOVTitleMenuPanel::Sortie?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
    if(Panel!=EOVTitleMenuPanel::Sortie && SortieMirror)SortieMirror->Release();
    if(PracticePanel)PracticePanel->SetVisibility(Panel==EOVTitleMenuPanel::Practice?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
    if(DifficultyText && GetWorld()) if(auto* Play=GetWorld()->GetSubsystem<UOVPlayScenarioSubsystem>())
        DifficultyText->SetText(FText::FromString(Play->DifficultyLabel()));
    if (CardReturnLabel) CardReturnLabel->SetText(FText::FromString(bOpenedFromInGame ? TEXT("전장으로") : TEXT("타이틀로")));
    if (TitlePanelContainer)
    {
        TitlePanelContainer->SetVisibility(Panel == EOVTitleMenuPanel::Title ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    }
    if (ScenarioPanelContainer)
    {
        ScenarioPanelContainer->SetVisibility(Panel == EOVTitleMenuPanel::ScenarioSelect ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    }
    if (SettingsPanelContainer)
    {
        SettingsPanelContainer->SetVisibility(Panel == EOVTitleMenuPanel::Settings ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    }
    if (TutorialPanelContainer)
    {
        TutorialPanelContainer->SetVisibility(Panel == EOVTitleMenuPanel::Tutorial ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    }
}

void UOVTitleScreenWidget::SelectScenario(FName ScenarioId)
{
    PlayClickAudio();
    SelectedScenarioId = ScenarioId;
    RefreshScenarioBriefing();

    for (UOVScenarioPickButton* Btn : ScenarioButtons)
    {
        if (Btn)
        {
            Btn->SetSelectedState(Btn->ScenarioId == SelectedScenarioId);
        }
    }

    if (bBackdropActive && BackdropStage)
    {
        BackdropStage->SetFormation(SelectedScenarioId);
        RefreshBackdropBrush();
        RefreshModeLabel();
    }
}

void UOVTitleScreenWidget::RefreshBackdropBrush()
{
    if (!BackdropImage)
    {
        return;
    }

    if (PreferredBackdropMode == EOVTitleBackdropMode::LiveProxyStage)
    {
        BackdropImage->SetVisibility(ESlateVisibility::Collapsed);
        return;
    }

    if (BackdropStage)
    {
        if (UTexture* Tex = BackdropStage->GetActiveTexture())
        {
            BackdropImage->SetBrushFromTexture(Cast<UTexture2D>(Tex));
            BackdropImage->SetVisibility(ESlateVisibility::Visible);
            return;
        }
    }
    BackdropImage->SetVisibility(ESlateVisibility::Collapsed);
}

void UOVTitleScreenWidget::RefreshScenarioBriefing()
{
    const FOVPlaySceneDefinition* TargetDef = nullptr;
    for (const FOVPlaySceneDefinition& Def : UOVPlayScenarioSubsystem::Definitions())
    {
        if (Def.Id == SelectedScenarioId)
        {
            TargetDef = &Def;
            break;
        }
    }

    if (!TargetDef)
    {
        return;
    }

    if (ScenarioTitleText)
    {
        ScenarioTitleText->SetText(FText::FromString(TargetDef->Title));
    }
    if (ScenarioBadgeText)
    {
        const FString StatusStr = TargetDef->bAvailable ? TEXT("출격 가능") : TEXT("준비 중");
        ScenarioBadgeText->SetText(FText::FromString(StatusStr));
        ScenarioBadgeText->SetColorAndOpacity(FSlateColor(TargetDef->bAvailable ? OVStyle::CyanAccent : OVStyle::AmberBadge));
    }
    if (ScenarioObjectiveText)
    {
        ScenarioObjectiveText->SetText(FText::FromString(TargetDef->Objective));
    }

    if (ScenarioPhoto){
        UTexture2D* Photo=ScenarioPhotos.FindRef(SelectedScenarioId);
        ScenarioPhoto->SetBrushFromTexture(Photo,true);
        if(Photo)ScenarioPhoto->SetDesiredSizeOverride(FVector2D(Photo->GetSizeX(),Photo->GetSizeY()));
    }
    const bool bPlayerPicks=UOVPlayScenarioSubsystem::IsBudgetMission(SelectedScenarioId);
    ScenarioRosterTitle->SetText(FText::FromString(bPlayerPicks?TEXT("추천 편성"):TEXT("아군 출격 편성")));
    ScenarioHullCountTitle->SetText(FText::FromString(bPlayerPicks?TEXT("추천 함선"):TEXT("아군 함선")));
    LaunchScenarioButton->SetIsEnabled(TargetDef->bAvailable);
    int32 Ships = 0, Cost = 0;
    for (const auto& Entry : TargetDef->Fleet)
    {
        FOVHullMetadata Meta;
        if (OVCalibration::GetHullMetadata(Entry.Hull, Meta)) Cost += Entry.Count * Meta.FleetCost;
        Ships += Entry.Count;
    }
    MetricBudgetVal->SetText(FText::FromString(Ships ? FString::Printf(TEXT("%d 코스트"), Cost) : TEXT("자유")));
    MetricHangarVal->SetText(FText::FromString(Ships ? FString::Printf(TEXT("%d척"), Ships) : TEXT("직접 배치")));
    const TCHAR* Kind = TEXT("함대전");
    if (TargetDef->Kind == EOVPlaySceneKind::Logistics) Kind = TEXT("수송 엄호");
    if (TargetDef->Kind == EOVPlaySceneKind::Evacuation) Kind = TEXT("생존 · 퇴각");
    if (TargetDef->Kind == EOVPlaySceneKind::BaseTrial) Kind = TEXT("기지 건설");
    if (TargetDef->Kind == EOVPlaySceneKind::Sandbox) Kind = TEXT("자유 시험");
    if (TargetDef->EnvironmentHazard == EOVSpaceHazardKind::BlackHole) Kind = TEXT("중력전");
    MetricTimeVal->SetText(FText::FromString(Kind));

    if (RosterColLeft && RosterColRight)
    {
        RosterColLeft->ClearChildren();
        RosterColRight->ClearChildren();

        auto AddRosterRow = [this](UVerticalBox* Col, const FString& HullName, int32 Count)
        {
            auto* Row = OVStyle::MakeLabel(WidgetTree, FString::Printf(TEXT("%s   × %d"), *HullName, Count),
                                           15, OVStyle::TextWhite, false);
            Row->SetAutoWrapText(false);
            Col->AddChildToVerticalBox(Row)->SetPadding(FMargin(0,2));
        };
        for (int32 Index = 0; Index < TargetDef->Fleet.Num(); ++Index)
        {
            const auto& Entry = TargetDef->Fleet[Index];
            AddRosterRow(Index % 2 ? RosterColRight : RosterColLeft, OVCalibration::GetHullName(Entry.Hull),
                         Entry.Count);
        }
        if (TargetDef->Fleet.IsEmpty())
            RosterColLeft->AddChildToVerticalBox(OVStyle::MakeLabel(WidgetTree, TEXT("빈 전장에서 시작합니다."), 13, OVStyle::TextSlate, false));
    }
}

void UOVTitleScreenWidget::RefreshModeLabel()
{
    const FString ModeStr = (PreferredBackdropMode == EOVTitleBackdropMode::LiveProxyStage)
        ? TEXT("방식 A (실시간 3D 뷰포트 풀 렌더링)")
        : TEXT("방식 B (사전 렌더링 인게임 고화질 텍스처)");

    if (BackdropStatusText)
    {
        BackdropStatusText->SetText(FText::FromString(FString::Printf(TEXT("BACKDROP: %s"), *ModeStr)));
    }
    if (SettingsBackdropModeText)
    {
        SettingsBackdropModeText->SetText(FText::FromString(FString::Printf(TEXT("현재 활성 배경 파이프라인: %s"), *ModeStr)));
    }
}

void UOVTitleScreenWidget::HandleOpenScenarios()
{
    PlayClickAudio();
    ShowPanel(EOVTitleMenuPanel::ScenarioSelect);
}

void UOVTitleScreenWidget::HandleOpenSandbox()
{
    PlayClickAudio();
    SelectScenario(TEXT("Sandbox"));
    ShowPanel(EOVTitleMenuPanel::ScenarioSelect);
}

void UOVTitleScreenWidget::HandleOpenTutorial()
{
    PlayClickAudio();
    RefreshTutorialReturnLabel();
    ShowPanel(EOVTitleMenuPanel::Tutorial);
}

void UOVTitleScreenWidget::HandleOpenSettings()
{
    PlayClickAudio();
    SyncSettingsWidgetsFromSystem();
    ShowPanel(EOVTitleMenuPanel::Settings);
}

void UOVTitleScreenWidget::BackFromCurrentPanel()
{
    if(CurrentPanel==EOVTitleMenuPanel::Sortie){SortieClick("Cancel");return;}
    if (CurrentPanel == EOVTitleMenuPanel::Tutorial) HandleCloseTutorial();
    else HandleBackToTitle();
}

void UOVTitleScreenWidget::CycleScenarioDifficulty()
{
    auto* Play=GetWorld()->GetSubsystem<UOVPlayScenarioSubsystem>();
    Play->CycleDifficulty();
    DifficultyText->SetText(FText::FromString(Play->DifficultyLabel()));
}

void UOVTitleScreenWidget::HandleBackToTitle()
{
    if (bOpenedFromInGame && (CurrentPanel == EOVTitleMenuPanel::ScenarioSelect || CurrentPanel == EOVTitleMenuPanel::Settings))
    {
        if (auto* HUD = Cast<AOVHUD>(GetOwningPlayer()->GetHUD())) HUD->CloseTitleScreen();
        return;
    }
    PlayClickAudio();
    ShowPanel(EOVTitleMenuPanel::Title);
}

void UOVTitleScreenWidget::HandleCloseTutorial()
{
    PlayClickAudio();
    if (bOpenedFromInGame)
    {
        if (UWorld* World = GetWorld())
        {
            if (APlayerController* PC = World->GetFirstPlayerController())
            {
                if (AOVHUD* HUD = Cast<AOVHUD>(PC->GetHUD()))
                {
                    HUD->CloseTitleScreen();
                    return;
                }
            }
        }
        DeactivateTitleScreen();
        return;
    }
    ShowPanel(EOVTitleMenuPanel::Title);
}

void UOVTitleScreenWidget::HandleLaunchSelectedScenario()
{
    PlayClickAudio();
    UWorld* World = GetWorld();
    if (!World) return;

    AOVController* Controller = Cast<AOVController>(World->GetFirstPlayerController());
    if (!Controller) return;

    if(UOVPlayScenarioSubsystem::IsBudgetMission(SelectedScenarioId)){
        if(auto* hud=Cast<AOVHUD>(Controller->GetHUD()))hud->BeginFleetPreparation(SelectedScenarioId);return;
    }
    if (!Controller->StartPlayScenario(SelectedScenarioId))
    {
        if (AOVHUD* HUD = Cast<AOVHUD>(Controller->GetHUD())) HUD->OpenTitleScreen();
        ShowPanel(EOVTitleMenuPanel::ScenarioSelect);
        ScenarioBadgeText->SetText(FText::FromString(TEXT("출격 실패 · 편성을 확인하세요")));
    }

}

void UOVTitleScreenWidget::HandleToggleBackdropMode()
{
    PlayClickAudio();
    PreferredBackdropMode = (PreferredBackdropMode == EOVTitleBackdropMode::LiveProxyStage)
        ? EOVTitleBackdropMode::PreRenderedInGameImage
        : EOVTitleBackdropMode::LiveProxyStage;

    if (BackdropStage)
    {
        BackdropStage->SetMode(PreferredBackdropMode);
    }
    RefreshBackdropBrush();
    RefreshModeLabel();
}

void UOVTitleScreenWidget::HandleCycleWindowMode()
{
    PlayClickAudio();
    WindowModeIndex = (WindowModeIndex + 1) % 3;
    RefreshSettingsLabels();
}

void UOVTitleScreenWidget::HandleCycleResolution()
{
    PlayClickAudio();
    ResolutionIndex = (ResolutionIndex + 1) % UE_ARRAY_COUNT(OVStyle::ResolutionPresets);
    RefreshSettingsLabels();
}

void UOVTitleScreenWidget::HandleCycleGraphicsQuality()
{
    PlayClickAudio();
    GraphicsQualityIndex = (GraphicsQualityIndex + 1) % UE_ARRAY_COUNT(OVStyle::QualityLabels);
    RefreshSettingsLabels();
}

void UOVTitleScreenWidget::HandleMasterVolumeChanged(float Value)
{
    if (bSuppressSliderCallbacks) return;
    PendingMasterVolume = FMath::Clamp(Value, 0.0f, 1.0f);
    if (UOVAudioSubsystem* Audio = UOVAudioSubsystem::Get(this))
    {
        Audio->SetVolumes(PendingMasterVolume, PendingBGMVolume, PendingSFXVolume, false);
    }
    RefreshSettingsLabels();
}

void UOVTitleScreenWidget::HandleBGMVolumeChanged(float Value)
{
    if (bSuppressSliderCallbacks) return;
    PendingBGMVolume = FMath::Clamp(Value, 0.0f, 1.0f);
    if (UOVAudioSubsystem* Audio = UOVAudioSubsystem::Get(this))
    {
        Audio->SetVolumes(PendingMasterVolume, PendingBGMVolume, PendingSFXVolume, false);
    }
    RefreshSettingsLabels();
}

void UOVTitleScreenWidget::HandleSFXVolumeChanged(float Value)
{
    if (bSuppressSliderCallbacks) return;
    PendingSFXVolume = FMath::Clamp(Value, 0.0f, 1.0f);
    if (UOVAudioSubsystem* Audio = UOVAudioSubsystem::Get(this))
    {
        Audio->SetVolumes(PendingMasterVolume, PendingBGMVolume, PendingSFXVolume, false);
    }
    RefreshSettingsLabels();
}

void UOVTitleScreenWidget::HandleUIScaleChanged(float Value)
{
    if (bSuppressSliderCallbacks) return;
    PendingUIScale = FMath::Clamp(Value, 0.80f, 1.50f);
    if (UWorld* World = GetWorld())
    {
        if (APlayerController* PC = World->GetFirstPlayerController())
        {
            if (AOVHUD* HUD = Cast<AOVHUD>(PC->GetHUD()))
            {
                HUD->SetUserUIScaleMultiplier(PendingUIScale, false);
            }
        }
    }
    RefreshSettingsLabels();
}

void UOVTitleScreenWidget::HandleApplyAndSaveSettings()
{
    PlayClickAudio();
    if (UGameUserSettings* GUS = GEngine ? GEngine->GetGameUserSettings() : nullptr)
    {
        const EWindowMode::Type Mode = WindowModeIndex == 0 ? EWindowMode::Fullscreen
            : (WindowModeIndex == 2 ? EWindowMode::Windowed : EWindowMode::WindowedFullscreen);
        const FIntPoint Res = ResolutionIndex == INDEX_NONE ? CurrentResolution : OVStyle::ResolutionPresets[ResolutionIndex];
        GUS->SetFullscreenMode(Mode);
        GUS->SetScreenResolution(Res);
        if (GraphicsQualityIndex >= 0 && GraphicsQualityIndex < UE_ARRAY_COUNT(OVStyle::QualityLabels))
        {
            GUS->SetOverallScalabilityLevel(GraphicsQualityIndex);
        }
        GUS->ApplySettings(false);
        GUS->SaveSettings();
    }

    if (UOVAudioSubsystem* Audio = UOVAudioSubsystem::Get(this))
    {
        Audio->SetVolumes(PendingMasterVolume, PendingBGMVolume, PendingSFXVolume, true);
    }

    if (UWorld* World = GetWorld())
    {
        if (APlayerController* PC = World->GetFirstPlayerController())
        {
            if (AOVHUD* HUD = Cast<AOVHUD>(PC->GetHUD()))
            {
                HUD->SetUserUIScaleMultiplier(PendingUIScale, true);
            }
        }
    }

    if (SettingsFeedbackText)
    {
        SettingsFeedbackText->SetText(FText::FromString(TEXT("설정이 적용되고 로컬 구성 파일(GameUserSettings.ini)에 저장되었습니다.")));
        SettingsFeedbackText->SetColorAndOpacity(FSlateColor(OVStyle::CyanAccent));
    }
    RefreshSettingsLabels();
}

void UOVTitleScreenWidget::HandleQuitGame()
{
    PlayClickAudio();
    UKismetSystemLibrary::QuitGame(GetWorld(), GetWorld()->GetFirstPlayerController(), EQuitPreference::Quit, false);
}

void UOVTitleScreenWidget::BuildPractice(UOverlay* Root)
{
    PracticePanel=WidgetTree->ConstructWidget<UBorder>();
    PracticePanel->SetBrush(OVStyle::MakeRoundedBoxBrush(FLinearColor(.012,.027,.05,.98),OVStyle::CyanBorder,8,1));
    auto* frame=Root->AddChildToOverlay(PracticePanel);
    frame->SetPadding(FMargin(32,24)); frame->SetHorizontalAlignment(HAlign_Fill); frame->SetVerticalAlignment(VAlign_Fill);
    PracticePanel->SetPadding(FMargin(22));
    auto* column=WidgetTree->ConstructWidget<UVerticalBox>(); PracticePanel->SetContent(column);
    column->AddChildToVerticalBox(OVStyle::MakeLabel(WidgetTree,TEXT("시스템 실습"),24,OVStyle::TextWhite,true));
    auto* row=WidgetTree->ConstructWidget<UHorizontalBox>();
    column->AddChildToVerticalBox(row)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    auto* scroll=WidgetTree->ConstructWidget<UScrollBox>(); row->AddChildToHorizontalBox(scroll)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    for(int32 i=0;i<9;++i){
        auto* button=WidgetTree->ConstructWidget<UOVScenarioPickButton>();
        button->OwnerWidget=this; button->ScenarioId=FName(TEXT("Lesson"),i+1);
        button->TitleText=OVStyle::MakeLabel(WidgetTree,TEXT(""),16,OVStyle::TextWhite,true);
        button->AddChild(button->TitleText); button->SetStyle(OVStyle::MakeSecondaryButtonStyle());
        button->TitleText->SetAutoWrapText(false);button->TitleText->SetMargin(FMargin(10)); button->OnClicked.AddDynamic(button,&UOVScenarioPickButton::HandleClicked);
        scroll->AddChild(button); PracticeRows.Add(button);
    }
    PracticeBrief=OVStyle::MakeLabel(WidgetTree,TEXT(""),18,OVStyle::TextWhite,false);
    PracticeBrief->SetAutoWrapText(true);
    auto* brief=row->AddChildToHorizontalBox(PracticeBrief);brief->SetPadding(FMargin(22,14));brief->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    auto* controls=WidgetTree->ConstructWidget<UHorizontalBox>();column->AddChildToVerticalBox(controls)->SetHorizontalAlignment(HAlign_Right);
    auto* back=WidgetTree->ConstructWidget<UButton>();back->SetStyle(OVStyle::MakeSecondaryButtonStyle());back->AddChild(OVStyle::MakeLabel(WidgetTree,TEXT("타이틀로"),14,OVStyle::TextWhite,true));controls->AddChildToHorizontalBox(back);
    back->OnClicked.AddDynamic(this,&UOVTitleScreenWidget::HandleBackToTitle);
    auto* go=WidgetTree->ConstructWidget<UButton>();go->SetStyle(OVStyle::MakePrimaryButtonStyle());go->AddChild(OVStyle::MakeLabel(WidgetTree,TEXT("실습 시작"),16,OVStyle::DarkText,true));controls->AddChildToHorizontalBox(go);
    go->OnClicked.AddDynamic(this,&UOVTitleScreenWidget::LaunchPractice);
    PracticePanel->SetVisibility(ESlateVisibility::Collapsed);
}
void UOVTitleScreenWidget::OpenPractice()
{
    const int32 mask=UOVPlayScenarioSubsystem::CompletedPracticeMask();
    int32 next=0; while(next<9 && (mask&(1<<next)))++next;
    ShowPanel(EOVTitleMenuPanel::Practice);SelectPractice(next%9);
}
void UOVTitleScreenWidget::SelectPractice(int32 index)
{
    if(!PracticeRows.IsValidIndex(index))return;
    ChosenPractice=index;const int32 mask=UOVPlayScenarioSubsystem::CompletedPracticeMask();
    for(int32 i=0;i<PracticeRows.Num();++i){
        PracticeRows[i]->SetStyle(OVStyle::MakeSecondaryButtonStyle(i==index));
        PracticeRows[i]->TitleText->SetText(FText::FromString(FString::Printf(TEXT("%d. %s   ·   %s"),i+1,*UOVPlayScenarioSubsystem::PracticeTitle(i),(mask&(1<<i))?TEXT("완료"):TEXT("미완료"))));
    }
    PracticeBrief->SetText(FText::FromString(UOVPlayScenarioSubsystem::PracticeSummary(index)));
}
void UOVTitleScreenWidget::LaunchPractice()
{
    auto* pc=Cast<AOVController>(GetOwningPlayer());
    if(pc && !pc->GetPlayScenario()->StartPractice(pc,ChosenPractice)){
        if(auto* hud=Cast<AOVHUD>(pc->GetHUD()))hud->OpenTitleScreen();
        OpenPractice();PracticeBrief->SetText(FText::FromString(TEXT("시험 편성을 준비할 수 없습니다. 다시 시작하세요.")));
    }
}

#include "OVSortieMenu.inl"
