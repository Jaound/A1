#include "OVTitleScreenWidget.h"

#include "OVController.h"
#include "OVHUD.h"
#include "OVPlayScenarioSubsystem.h"
#include "OVTitleBackdropStage.h"

#include "Blueprint/WidgetTree.h"
#include "Components/BackgroundBlur.h"
#include "Components/Border.h"
#include "Components/BorderSlot.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"

namespace OVStyle
{
// High-tech sci-fi color palette matching reference wireframes
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

inline UFont* GetFontRegular()
{
    static TWeakObjectPtr<UFont> Cached;
    if (!Cached.IsValid())
    {
        Cached = LoadObject<UFont>(nullptr, TEXT("/Game/UI/Fonts/Font_Malgun.Font_Malgun"));
    }
    return Cached.Get();
}

inline UFont* GetFontBold()
{
    static TWeakObjectPtr<UFont> Cached;
    if (!Cached.IsValid())
    {
        Cached = LoadObject<UFont>(nullptr, TEXT("/Game/UI/Fonts/Font_MalgunBold.Font_MalgunBold"));
    }
    return Cached.Get();
}

inline FSlateFontInfo MakeFont(int32 Size, bool bBold)
{
    if (UFont* FontObj = bBold ? GetFontBold() : GetFontRegular())
    {
        return FSlateFontInfo(FontObj, (float)Size);
    }
    return FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", (float)Size);
}

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

UTextBlock* MakeLabel(UWidgetTree* Tree, const FString& Text, int32 Size, const FLinearColor& Color, bool bBold = false)
{
    UTextBlock* Label = Tree->ConstructWidget<UTextBlock>();
    Label->SetFont(MakeFont(Size, bBold));
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

    // Left text
    UTextBlock* LeftText = MakeLabel(
        Tree,
        Title,
        bPrimary ? 15 : 14,
        bPrimary ? DarkText : (bDisabled ? TextDim : TextWhite),
        bPrimary || !bDisabled);
    LeftText->SetAutoWrapText(false);

    if (UHorizontalBoxSlot* LeftSlot = HBox->AddChildToHorizontalBox(LeftText))
    {
        LeftSlot->SetPadding(FMargin(14.f, 10.f, 10.f, 10.f));
        LeftSlot->SetVerticalAlignment(VAlign_Center);
    }

    // Spacer
    USpacer* Spacer = Tree->ConstructWidget<USpacer>();
    if (UHorizontalBoxSlot* SpacerSlot = HBox->AddChildToHorizontalBox(Spacer))
    {
        SpacerSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    }

    // Right badge / chevron
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
            RightSlot->SetPadding(FMargin(10.f, 10.f, 14.f, 10.f));
            RightSlot->SetVerticalAlignment(VAlign_Center);
        }
    }

    if (bDisabled)
    {
        Btn->SetIsEnabled(false);
    }

    if (UVerticalBoxSlot* Slot = Parent->AddChildToVerticalBox(Btn))
    {
        Slot->SetPadding(FMargin(0.f, 4.f));
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
        OwnerWidget->SelectScenario(ScenarioId);
    }
}

void UOVTitleScreenWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    EnsureGradientTexture();
    BuildProgrammaticTree();
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
            // Left 35% deep navy, smooth cubic falloff to transparent on the right
            const float Falloff = FMath::Clamp(1.0f - FMath::Pow(U, 1.45f), 0.0f, 1.0f);
            const uint8 Alpha = (uint8)(Falloff * 235.f);
            Pixels[Y * TexWidth + X] = FColor(10, 18, 30, Alpha); // Slate Navy #0A121E
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

    // Layer 2: Left Menu Scrim (Condition 2: UBackgroundBlur + Horizontal Gradient Scrim Brush)
    USizeBox* LeftScrimBox = WidgetTree->ConstructWidget<USizeBox>();
    LeftScrimBox->SetWidthOverride(500.f);
    if (UOverlaySlot* ScrimOverlaySlot = RootOverlay->AddChildToOverlay(LeftScrimBox))
    {
        ScrimOverlaySlot->SetHorizontalAlignment(HAlign_Left);
        ScrimOverlaySlot->SetVerticalAlignment(VAlign_Fill);
    }

    UOverlay* ScrimStack = WidgetTree->ConstructWidget<UOverlay>();
    LeftScrimBox->SetContent(ScrimStack);

    // 2a. Background blur behind left menu so 3D ships blur gently
    UBackgroundBlur* BlurEffect = WidgetTree->ConstructWidget<UBackgroundBlur>();
    BlurEffect->SetBlurStrength(20.0f);
    BlurEffect->SetApplyAlphaToBlur(true);
    if (UOverlaySlot* BlurSlot = ScrimStack->AddChildToOverlay(BlurEffect))
    {
        BlurSlot->SetHorizontalAlignment(HAlign_Fill);
        BlurSlot->SetVerticalAlignment(VAlign_Fill);
    }

    // 2b. Gradient scrim image
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

    // 2c. Left Menu Content Column
    UVerticalBox* LeftMenuVBox = WidgetTree->ConstructWidget<UVerticalBox>();
    if (UOverlaySlot* ContentSlot = ScrimStack->AddChildToOverlay(LeftMenuVBox))
    {
        ContentSlot->SetPadding(FMargin(44.f, 36.f, 40.f, 32.f));
        ContentSlot->SetHorizontalAlignment(HAlign_Fill);
        ContentSlot->SetVerticalAlignment(VAlign_Fill);
    }

    // Top Meta Strip
    UHorizontalBox* TopMeta = WidgetTree->ConstructWidget<UHorizontalBox>();
    LeftMenuVBox->AddChildToVerticalBox(TopMeta)->SetPadding(FMargin(0.f, 0.f, 0.f, 32.f));
    TopMeta->AddChildToHorizontalBox(OVStyle::MakeLabel(
        WidgetTree, TEXT("MAP: /Game/Maps/ControlRange · MODE: AOVGameMode"), 11, OVStyle::TextDim, false));

    // Subtitle (11pt Cyan with tracking)
    LeftMenuVBox->AddChildToVerticalBox(OVStyle::MakeLabel(
        WidgetTree, TEXT("3D FLEET COMMAND & BOARDING TACTICS"), 11, OVStyle::CyanAccent, true));

    // Title (44pt Bold)
    UTextBlock* TitleLabel = OVStyle::MakeLabel(
        WidgetTree, TEXT("ORBITAL VANGUARD"), 40, OVStyle::TextWhite, true);
    LeftMenuVBox->AddChildToVerticalBox(TitleLabel)->SetPadding(FMargin(0.f, 6.f, 0.f, 12.f));

    // Description (13pt Slate)
    LeftMenuVBox->AddChildToVerticalBox(OVStyle::MakeLabel(
        WidgetTree, TEXT("3D 우주 함대전과 함선 내부 2D 강습 작전을 단일 시뮬레이션으로 지휘합니다."), 13, OVStyle::TextSlate, false))
        ->SetPadding(FMargin(0.f, 0.f, 0.f, 32.f));

    // Menu Buttons
    UButton* StartBtn = OVStyle::MakeMenuButton(WidgetTree, LeftMenuVBox, TEXT("작전 개시 · 임무 선택"), TEXT(">"), true);
    StartBtn->OnClicked.AddDynamic(this, &UOVTitleScreenWidget::HandleOpenScenarios);

    UButton* CampaignBtn = OVStyle::MakeMenuButton(WidgetTree, LeftMenuVBox, TEXT("[잠김] 캠페인"), TEXT("4단계 예정"), false, true);

    UButton* SandboxBtn = OVStyle::MakeMenuButton(WidgetTree, LeftMenuVBox, TEXT("자유 전투 시험장"), TEXT("Sandbox"), false);
    SandboxBtn->OnClicked.AddDynamic(this, &UOVTitleScreenWidget::HandleOpenSandbox);

    UButton* SettingsBtn = OVStyle::MakeMenuButton(WidgetTree, LeftMenuVBox, TEXT("환경 설정"), TEXT("설정"), false);
    SettingsBtn->OnClicked.AddDynamic(this, &UOVTitleScreenWidget::HandleOpenSettings);

    UButton* QuitBtn = OVStyle::MakeMenuButton(WidgetTree, LeftMenuVBox, TEXT("게임 종료"), TEXT(""), false);
    QuitBtn->OnClicked.AddDynamic(this, &UOVTitleScreenWidget::HandleQuitGame);

    // Spacer pushing footer to bottom
    USpacer* LeftSpacer = WidgetTree->ConstructWidget<USpacer>();
    if (UVerticalBoxSlot* SpacerSlot = LeftMenuVBox->AddChildToVerticalBox(LeftSpacer))
    {
        SpacerSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    }

    // Left Footer
    UHorizontalBox* LeftFooter = WidgetTree->ConstructWidget<UHorizontalBox>();
    LeftMenuVBox->AddChildToVerticalBox(LeftFooter)->SetPadding(FMargin(0.f, 16.f, 0.f, 0.f));
    LeftFooter->AddChildToHorizontalBox(OVStyle::MakeLabel(
        WidgetTree, TEXT("조작 프로필: PlayGame3 · F5/F6/F7 빠른 함대전"), 11, OVStyle::TextDim, false));
    USpacer* FootSpacer = WidgetTree->ConstructWidget<USpacer>();
    LeftFooter->AddChildToHorizontalBox(FootSpacer)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    LeftFooter->AddChildToHorizontalBox(OVStyle::MakeLabel(
        WidgetTree, TEXT("UI 배율: 100%"), 11, OVStyle::TextDim, false));

    // Wrap Left Menu in TitlePanelContainer so it can be toggled
    TitlePanelContainer = WidgetTree->ConstructWidget<UBorder>();
    TitlePanelContainer->SetBrush(OVStyle::MakeRoundedBoxBrush(FLinearColor::Transparent, FLinearColor::Transparent, 0.f, 0.f));
    TitlePanelContainer->SetPadding(FMargin(0.f));
    // The Left Scrim is already part of Title presentation; we keep TitlePanelContainer referencing the overlay
    TitlePanelContainer->SetContent(LeftScrimBox);
    RootOverlay->AddChildToOverlay(TitlePanelContainer);

    // =========================================================================
    // LAYER 3: SCREEN 02 - SCENARIO SELECT MODAL CARD (Matching Image 2)
    // =========================================================================
    ScenarioPanelContainer = WidgetTree->ConstructWidget<UBorder>();
    ScenarioPanelContainer->SetBrush(OVStyle::MakeRoundedBoxBrush(
        FLinearColor(0.025f, 0.055f, 0.095f, 0.94f), OVStyle::SlateBorder, 8.0f, 1.0f));
    ScenarioPanelContainer->SetPadding(FMargin(28.f, 22.f, 28.f, 24.f));
    ScenarioPanelContainer->SetVisibility(ESlateVisibility::Collapsed);

    if (UOverlaySlot* ModalSlot = RootOverlay->AddChildToOverlay(ScenarioPanelContainer))
    {
        ModalSlot->SetHorizontalAlignment(HAlign_Center);
        ModalSlot->SetVerticalAlignment(VAlign_Center);
    }

    USizeBox* ModalCardBox = WidgetTree->ConstructWidget<USizeBox>();
    ModalCardBox->SetWidthOverride(980.f);
    ModalCardBox->SetHeightOverride(580.f);
    ScenarioPanelContainer->SetContent(ModalCardBox);

    UVerticalBox* ModalVBox = WidgetTree->ConstructWidget<UVerticalBox>();
    ModalCardBox->SetContent(ModalVBox);

    // Header Row: Left Title + Subtitle, Right "타이틀로" button
    UHorizontalBox* ModalHeader = WidgetTree->ConstructWidget<UHorizontalBox>();
    ModalVBox->AddChildToVerticalBox(ModalHeader)->SetPadding(FMargin(0.f, 0.f, 0.f, 18.f));

    UVerticalBox* HeaderTitles = WidgetTree->ConstructWidget<UVerticalBox>();
    ModalHeader->AddChildToHorizontalBox(HeaderTitles);
    HeaderTitles->AddChildToVerticalBox(OVStyle::MakeLabel(
        WidgetTree, TEXT("플레이 임무 선택 (6종)"), 20, OVStyle::TextWhite, true));
    HeaderTitles->AddChildToVerticalBox(OVStyle::MakeLabel(
        WidgetTree, TEXT("UOVPlayScenarioSubsystem::Definitions() 실데이터 기준"), 11, OVStyle::TextDim, false))
        ->SetPadding(FMargin(0.f, 3.f, 0.f, 0.f));

    USpacer* HeaderSpacer = WidgetTree->ConstructWidget<USpacer>();
    ModalHeader->AddChildToHorizontalBox(HeaderSpacer)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

    UButton* BackToTitleBtn = WidgetTree->ConstructWidget<UButton>();
    BackToTitleBtn->SetStyle(OVStyle::MakeSecondaryButtonStyle(false));
    BackToTitleBtn->AddChild(OVStyle::MakeLabel(WidgetTree, TEXT("타이틀로"), 12, OVStyle::TextWhite, true));
    BackToTitleBtn->OnClicked.AddDynamic(this, &UOVTitleScreenWidget::HandleBackToTitle);
    if (UHorizontalBoxSlot* BackSlot = ModalHeader->AddChildToHorizontalBox(BackToTitleBtn))
    {
        BackSlot->SetPadding(FMargin(12.f, 4.f));
        BackSlot->SetVerticalAlignment(VAlign_Top);
    }

    // Main 2-Column Split
    UHorizontalBox* MainSplit = WidgetTree->ConstructWidget<UHorizontalBox>();
    if (UVerticalBoxSlot* SplitSlot = ModalVBox->AddChildToVerticalBox(MainSplit))
    {
        SplitSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    }

    // Left Column: 6 Mission Buttons (Width 320px)
    USizeBox* LeftListWidth = WidgetTree->ConstructWidget<USizeBox>();
    LeftListWidth->SetWidthOverride(320.f);
    MainSplit->AddChildToHorizontalBox(LeftListWidth);

    ScenarioListBox = WidgetTree->ConstructWidget<UVerticalBox>();
    LeftListWidth->SetContent(ScenarioListBox);

    for (const FOVPlaySceneDefinition& Def : UOVPlayScenarioSubsystem::Definitions())
    {
        UOVScenarioPickButton* PickBtn = WidgetTree->ConstructWidget<UOVScenarioPickButton>();
        PickBtn->OwnerWidget = this;
        PickBtn->ScenarioId = Def.Id;

        UHorizontalBox* BtnContent = WidgetTree->ConstructWidget<UHorizontalBox>();
        PickBtn->AddChild(BtnContent);

        UVerticalBox* TextCol = WidgetTree->ConstructWidget<UVerticalBox>();
        if (UHorizontalBoxSlot* TextSlot = BtnContent->AddChildToHorizontalBox(TextCol))
        {
            TextSlot->SetPadding(FMargin(12.f, 8.f, 8.f, 8.f));
            TextSlot->SetVerticalAlignment(VAlign_Center);
        }

        PickBtn->TitleText = OVStyle::MakeLabel(
            WidgetTree, Def.Title, 14, OVStyle::TextWhite, true);
        TextCol->AddChildToVerticalBox(PickBtn->TitleText);

        const FString SubStr = (Def.FleetBudget > 0)
            ? FString::Printf(TEXT("%s · %d Cost"), *Def.Id.ToString(), Def.FleetBudget)
            : Def.Id.ToString();
        PickBtn->SubText = OVStyle::MakeLabel(
            WidgetTree, SubStr, 11, OVStyle::TextDim, false);
        TextCol->AddChildToVerticalBox(PickBtn->SubText);

        USpacer* BtnSpacer = WidgetTree->ConstructWidget<USpacer>();
        BtnContent->AddChildToHorizontalBox(BtnSpacer)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

        // Right Badge (F5 / F6 / F7 / 준비 중 / Sandbox)
        FString BadgeStr;
        FLinearColor BadgeCol = OVStyle::CyanAccent;
        if (Def.Id == TEXT("Fleet100")) { BadgeStr = TEXT("F5"); }
        else if (Def.Id == TEXT("Fleet200")) { BadgeStr = TEXT("F6"); }
        else if (Def.Id == TEXT("Fleet300")) { BadgeStr = TEXT("F7"); }
        else if (!Def.bAvailable) { BadgeStr = TEXT("준비 중"); BadgeCol = OVStyle::AmberBadge; }
        else if (Def.Id == TEXT("Sandbox")) { BadgeStr = TEXT("Sandbox"); BadgeCol = OVStyle::TextDim; }

        PickBtn->BadgeText = OVStyle::MakeLabel(WidgetTree, BadgeStr, 11, BadgeCol, true);
        if (UHorizontalBoxSlot* BadgeSlot = BtnContent->AddChildToHorizontalBox(PickBtn->BadgeText))
        {
            BadgeSlot->SetPadding(FMargin(8.f, 8.f, 14.f, 8.f));
            BadgeSlot->SetVerticalAlignment(VAlign_Center);
        }

        PickBtn->SetSelectedState(Def.Id == SelectedScenarioId);
        PickBtn->OnClicked.AddDynamic(PickBtn, &UOVScenarioPickButton::HandleClicked);

        ScenarioListBox->AddChildToVerticalBox(PickBtn)->SetPadding(FMargin(0.f, 3.f));
        ScenarioButtons.Add(PickBtn);
    }

    // Right Column: Rich Briefing Card (Width 620px)
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

    // Briefing Header: Title + Status Badge
    UHorizontalBox* BriefHeader = WidgetTree->ConstructWidget<UHorizontalBox>();
    BriefingBox->AddChildToVerticalBox(BriefHeader)->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));

    ScenarioTitleText = OVStyle::MakeLabel(
        WidgetTree, TEXT("200코스트 함대전"), 22, OVStyle::TextWhite, true);
    BriefHeader->AddChildToHorizontalBox(ScenarioTitleText);

    USpacer* BriefSpacer = WidgetTree->ConstructWidget<USpacer>();
    BriefHeader->AddChildToHorizontalBox(BriefSpacer)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

    ScenarioBadgeText = OVStyle::MakeLabel(
        WidgetTree, TEXT("PIE API 51/51 통과 · 즉시 출격 가능"), 11, OVStyle::CyanAccent, true);
    BriefHeader->AddChildToHorizontalBox(ScenarioBadgeText)->SetVerticalAlignment(VAlign_Center);

    // Briefing Objective / Description
    ScenarioObjectiveText = OVStyle::MakeLabel(
        WidgetTree, TEXT("초대형 주력함과 호위 함대로 상대를 무력화하세요."), 13, OVStyle::TextSlate, false);
    BriefingBox->AddChildToVerticalBox(ScenarioObjectiveText)->SetPadding(FMargin(0.f, 0.f, 0.f, 16.f));

    // 3-Metric Summary Block (3 columns inside dark card)
    UBorder* MetricsCard = WidgetTree->ConstructWidget<UBorder>();
    MetricsCard->SetBrush(OVStyle::MakeRoundedBoxBrush(
        FLinearColor(0.010f, 0.020f, 0.038f, 0.95f), OVStyle::SubtleBorder, 4.0f, 1.0f));
    MetricsCard->SetPadding(FMargin(16.f, 10.f));
    BriefingBox->AddChildToVerticalBox(MetricsCard)->SetPadding(FMargin(0.f, 0.f, 0.f, 16.f));

    UHorizontalBox* MetricsHBox = WidgetTree->ConstructWidget<UHorizontalBox>();
    MetricsCard->SetContent(MetricsHBox);

    auto AddMetricCol = [this, &MetricsHBox](const FString& Label, TObjectPtr<UTextBlock>& ValPtr)
    {
        UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>();
        Col->AddChildToVerticalBox(OVStyle::MakeLabel(WidgetTree, Label, 11, OVStyle::TextDim, false));
        ValPtr = OVStyle::MakeLabel(WidgetTree, TEXT("-"), 18, OVStyle::TextWhite, true);
        Col->AddChildToVerticalBox(ValPtr)->SetPadding(FMargin(0.f, 2.f, 0.f, 0.f));
        if (UHorizontalBoxSlot* ColSlot = MetricsHBox->AddChildToHorizontalBox(Col))
        {
            ColSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        }
    };
    AddMetricCol(TEXT("한 쪽 코스트"), MetricBudgetVal);
    AddMetricCol(TEXT("격납고 / 함재기"), MetricHangarVal);
    AddMetricCol(TEXT("교착 / 제한 시간"), MetricTimeVal);

    // Roster Subtitle
    BriefingBox->AddChildToVerticalBox(OVStyle::MakeLabel(
        WidgetTree, TEXT("양측 기본 편성 (12척 · 영웅 0명 격리 세션)"), 12, OVStyle::CyanAccent, true))
        ->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));

    // Roster Breakdown 2-Column Grid
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

    // Bottom Action Row: CLI Snippet on left, Primary "▷ 작전 출격" Button on right
    UHorizontalBox* BottomActionRow = WidgetTree->ConstructWidget<UHorizontalBox>();
    BriefingBox->AddChildToVerticalBox(BottomActionRow);

    ScenarioCliSnippet = OVStyle::MakeLabel(
        WidgetTree, TEXT("Start(Controller, \"Fleet200\") · -OVPlayScene=Fleet200"), 11, OVStyle::TextDim, false);
    if (UHorizontalBoxSlot* CliSlot = BottomActionRow->AddChildToHorizontalBox(ScenarioCliSnippet))
    {
        CliSlot->SetVerticalAlignment(VAlign_Center);
        CliSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    }

    LaunchScenarioButton = WidgetTree->ConstructWidget<UButton>();
    LaunchScenarioButton->SetStyle(OVStyle::MakePrimaryButtonStyle());
    UTextBlock* LaunchText = OVStyle::MakeLabel(WidgetTree, TEXT("▷ 작전 출격"), 15, OVStyle::DarkText, true);
    LaunchText->SetAutoWrapText(false);
    LaunchScenarioButton->AddChild(LaunchText);
    LaunchScenarioButton->OnClicked.AddDynamic(this, &UOVTitleScreenWidget::HandleLaunchSelectedScenario);
    if (UHorizontalBoxSlot* LaunchSlot = BottomActionRow->AddChildToHorizontalBox(LaunchScenarioButton))
    {
        LaunchSlot->SetPadding(FMargin(18.f, 8.f));
        LaunchSlot->SetVerticalAlignment(VAlign_Center);
    }

    // =========================================================================
    // LAYER 4: SCREEN 03 - SETTINGS MODAL
    // =========================================================================
    SettingsPanelContainer = WidgetTree->ConstructWidget<UBorder>();
    SettingsPanelContainer->SetBrush(OVStyle::MakeRoundedBoxBrush(
        FLinearColor(0.025f, 0.055f, 0.095f, 0.94f), OVStyle::SlateBorder, 8.0f, 1.0f));
    SettingsPanelContainer->SetPadding(FMargin(28.f));
    SettingsPanelContainer->SetVisibility(ESlateVisibility::Collapsed);

    if (UOverlaySlot* SettingsSlot = RootOverlay->AddChildToOverlay(SettingsPanelContainer))
    {
        SettingsSlot->SetHorizontalAlignment(HAlign_Center);
        SettingsSlot->SetVerticalAlignment(VAlign_Center);
    }

    USizeBox* SettingsSize = WidgetTree->ConstructWidget<USizeBox>();
    SettingsSize->SetWidthOverride(500.f);
    SettingsPanelContainer->SetContent(SettingsSize);

    UVerticalBox* SettingsBox = WidgetTree->ConstructWidget<UVerticalBox>();
    SettingsSize->SetContent(SettingsBox);

    SettingsBox->AddChildToVerticalBox(OVStyle::MakeLabel(
        WidgetTree, TEXT("환경 설정 · 타이틀 배경 파이프라인"), 18, OVStyle::TextWhite, true))
        ->SetPadding(FMargin(0.f, 0.f, 0.f, 14.f));

    SettingsBackdropModeText = OVStyle::MakeLabel(
        WidgetTree, TEXT(""), 13, OVStyle::TextSlate, false);
    SettingsBox->AddChildToVerticalBox(SettingsBackdropModeText)->SetPadding(FMargin(0.f, 0.f, 0.f, 14.f));

    UButton* ToggleBackdropBtn = OVStyle::MakeMenuButton(
        WidgetTree, SettingsBox, TEXT("배경 방식 전환 (방식 A 실시간 3D ↔ 방식 B 사전 렌더)"), TEXT(""), true);
    ToggleBackdropBtn->OnClicked.AddDynamic(this, &UOVTitleScreenWidget::HandleToggleBackdropMode);

    UButton* BackBtn = OVStyle::MakeMenuButton(
        WidgetTree, SettingsBox, TEXT("← 타이틀로 돌아가기"), TEXT(""), false);
    BackBtn->OnClicked.AddDynamic(this, &UOVTitleScreenWidget::HandleBackToTitle);

    RefreshScenarioBriefing();
    RefreshModeLabel();
}

void UOVTitleScreenWidget::ActivateTitleScreen()
{
    if (!BackdropStage)
    {
        BackdropStage = NewObject<UOVTitleBackdropStage>(this);
    }

    if (UWorld* World = GetWorld())
    {
        BackdropStage->Activate(World, SelectedScenarioId, PreferredBackdropMode);
        bBackdropActive = true;
    }

    ShowPanel(EOVTitleMenuPanel::Title);
    RefreshBackdropBrush();
    RefreshModeLabel();
    SetVisibility(ESlateVisibility::Visible);
}

void UOVTitleScreenWidget::DeactivateTitleScreen()
{
    bBackdropActive = false;
    SetVisibility(ESlateVisibility::Collapsed);
    if (BackdropStage)
    {
        BackdropStage->Release();
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
}

void UOVTitleScreenWidget::SelectScenario(FName ScenarioId)
{
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

    // In Way A (LiveProxyStage), direct viewport camera renders the full 3D scene; BackdropImage is collapsed
    if (PreferredBackdropMode == EOVTitleBackdropMode::LiveProxyStage)
    {
        BackdropImage->SetVisibility(ESlateVisibility::Collapsed);
        return;
    }

    // In Way B (PreRendered), display the baked 2D texture
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
        const FString StatusStr = TargetDef->bAvailable
            ? TEXT("PIE API 51/51 통과 · 즉시 출격 가능")
            : TEXT("준비 중 · 프로토타입 단계");
        ScenarioBadgeText->SetText(FText::FromString(StatusStr));
        ScenarioBadgeText->SetColorAndOpacity(FSlateColor(TargetDef->bAvailable ? OVStyle::CyanAccent : OVStyle::AmberBadge));
    }
    if (ScenarioObjectiveText)
    {
        ScenarioObjectiveText->SetText(FText::FromString(TargetDef->Objective));
    }

    // Metrics
    if (MetricBudgetVal)
    {
        const FString BudgetStr = TargetDef->FleetBudget > 0
            ? FString::Printf(TEXT("%d"), TargetDef->FleetBudget)
            : (TargetDef->Id == TEXT("Sandbox") ? TEXT("무제한") : TEXT("500 자원"));
        MetricBudgetVal->SetText(FText::FromString(BudgetStr));
    }
    if (MetricHangarVal)
    {
        FString HangarStr = TEXT("0 / 0기");
        if (TargetDef->Id == TEXT("Fleet100")) { HangarStr = TEXT("1 / 10기"); }
        else if (TargetDef->Id == TEXT("Fleet200")) { HangarStr = TEXT("2 / 20기"); }
        else if (TargetDef->Id == TEXT("Fleet300")) { HangarStr = TEXT("2 / 20기"); }
        else if (TargetDef->Id == TEXT("Logistics")) { HangarStr = TEXT("4척"); }
        else if (TargetDef->Id == TEXT("Evacuation")) { HangarStr = TEXT("60% 이탈"); }
        else if (TargetDef->Id == TEXT("Sandbox")) { HangarStr = TEXT("자유"); }
        MetricHangarVal->SetText(FText::FromString(HangarStr));
    }
    if (MetricTimeVal)
    {
        const FString TimeStr = TargetDef->LimitSeconds > 0
            ? FString::Printf(TEXT("%.0fs / %.0fs"), TargetDef->StalemateSeconds, TargetDef->LimitSeconds)
            : TEXT("무제한");
        MetricTimeVal->SetText(FText::FromString(TimeStr));
    }

    if (ScenarioCliSnippet)
    {
        ScenarioCliSnippet->SetText(FText::FromString(
            FString::Printf(TEXT("Start(Controller, \"%s\") · -OVPlayScene=%s"), *TargetDef->Id.ToString(), *TargetDef->Id.ToString())));
    }

    // Populate Roster Table
    if (RosterColLeft && RosterColRight)
    {
        RosterColLeft->ClearChildren();
        RosterColRight->ClearChildren();

        auto AddRosterRow = [this](UVerticalBox* Col, const FString& HullName, int32 Count, int32 CostPt)
        {
            UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
            UTextBlock* NameLabel = OVStyle::MakeLabel(
                WidgetTree, FString::Printf(TEXT("%s ×%d"), *HullName, Count), 12, OVStyle::TextWhite, false);
            Row->AddChildToHorizontalBox(NameLabel);

            USpacer* Spacer = WidgetTree->ConstructWidget<USpacer>();
            Row->AddChildToHorizontalBox(Spacer)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

            UTextBlock* CostLabel = OVStyle::MakeLabel(
                WidgetTree, FString::Printf(TEXT("%dpt"), CostPt), 12, OVStyle::CyanAccent, true);
            Row->AddChildToHorizontalBox(CostLabel);

            Col->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.f, 2.f));
        };

        if (TargetDef->Id == TEXT("Fleet100"))
        {
            AddRosterRow(RosterColLeft, TEXT("초계함"), 3, 3);
            AddRosterRow(RosterColLeft, TEXT("보급수리함"), 1, 1);
            AddRosterRow(RosterColLeft, TEXT("바스티온급 구축함"), 1, 5);
            AddRosterRow(RosterColLeft, TEXT("레이더함"), 1, 5);
            AddRosterRow(RosterColLeft, TEXT("전함"), 2, 50);

            AddRosterRow(RosterColRight, TEXT("호위함"), 1, 1);
            AddRosterRow(RosterColRight, TEXT("벡터급 구축함"), 1, 5);
            AddRosterRow(RosterColRight, TEXT("방공함"), 1, 5);
            AddRosterRow(RosterColRight, TEXT("항공모함"), 1, 25);
        }
        else if (TargetDef->Id == TEXT("Fleet200"))
        {
            AddRosterRow(RosterColLeft, TEXT("초계함"), 3, 3);
            AddRosterRow(RosterColLeft, TEXT("보급수리함"), 1, 1);
            AddRosterRow(RosterColLeft, TEXT("바스티온급 구축함"), 1, 5);
            AddRosterRow(RosterColLeft, TEXT("레이더함"), 1, 5);
            AddRosterRow(RosterColLeft, TEXT("전함"), 1, 25);

            AddRosterRow(RosterColRight, TEXT("호위함"), 1, 1);
            AddRosterRow(RosterColRight, TEXT("벡터급 구축함"), 1, 5);
            AddRosterRow(RosterColRight, TEXT("방공함"), 1, 5);
            AddRosterRow(RosterColRight, TEXT("구축전함"), 1, 125);
            AddRosterRow(RosterColRight, TEXT("항공모함"), 1, 25);
        }
        else if (TargetDef->Id == TEXT("Fleet300"))
        {
            AddRosterRow(RosterColLeft, TEXT("초계함"), 3, 3);
            AddRosterRow(RosterColLeft, TEXT("보급수리함"), 1, 1);
            AddRosterRow(RosterColLeft, TEXT("바스티온급 구축함"), 1, 5);
            AddRosterRow(RosterColLeft, TEXT("레이더함"), 1, 5);
            AddRosterRow(RosterColLeft, TEXT("항공순양함"), 1, 125);

            AddRosterRow(RosterColRight, TEXT("호위함"), 1, 1);
            AddRosterRow(RosterColRight, TEXT("벡터급 구축함"), 1, 5);
            AddRosterRow(RosterColRight, TEXT("방공함"), 1, 5);
            AddRosterRow(RosterColRight, TEXT("구축전함"), 1, 125);
            AddRosterRow(RosterColRight, TEXT("항공모함"), 1, 25);
        }
        else if (TargetDef->Id == TEXT("Logistics"))
        {
            AddRosterRow(RosterColLeft, TEXT("수송선 (호송 대상)"), 2, 10);
            AddRosterRow(RosterColLeft, TEXT("채굴선"), 2, 10);
            AddRosterRow(RosterColRight, TEXT("순양함 (호위 주력)"), 1, 25);
            AddRosterRow(RosterColRight, TEXT("호위함"), 2, 2);
        }
        else if (TargetDef->Id == TEXT("Evacuation"))
        {
            AddRosterRow(RosterColLeft, TEXT("전함 (탈출 대상)"), 1, 25);
            AddRosterRow(RosterColLeft, TEXT("구축전함 (탈출 대상)"), 1, 125);
            AddRosterRow(RosterColRight, TEXT("방공함 (방어 호위)"), 2, 10);
            AddRosterRow(RosterColRight, TEXT("초계함"), 3, 3);
        }
        else
        {
            AddRosterRow(RosterColLeft, TEXT("자유 배치"), 1, 0);
            AddRosterRow(RosterColRight, TEXT("시험 세션"), 1, 0);
        }
    }
}

void UOVTitleScreenWidget::RefreshModeLabel()
{
    const FString ModeStr = (PreferredBackdropMode == EOVTitleBackdropMode::LiveProxyStage)
        ? TEXT("방식 A (실시간 3D 뷰포트 풀 렌더링)")
        : TEXT("방식 B (사전 렌더링 인게임 고화질 텍스처)");

    if (BackdropStatusText)
    {
        BackdropStatusText->SetText(FText::FromString(
            FString::Printf(TEXT("BACKDROP: %s"), *ModeStr)));
    }
    if (SettingsBackdropModeText)
    {
        SettingsBackdropModeText->SetText(FText::FromString(
            FString::Printf(TEXT("현재 활성 배경 파이프라인: %s"), *ModeStr)));
    }
}

void UOVTitleScreenWidget::HandleOpenScenarios()
{
    ShowPanel(EOVTitleMenuPanel::ScenarioSelect);
}

void UOVTitleScreenWidget::HandleOpenSandbox()
{
    SelectScenario(TEXT("Sandbox"));
    ShowPanel(EOVTitleMenuPanel::ScenarioSelect);
}

void UOVTitleScreenWidget::HandleOpenSettings()
{
    ShowPanel(EOVTitleMenuPanel::Settings);
}

void UOVTitleScreenWidget::HandleBackToTitle()
{
    ShowPanel(EOVTitleMenuPanel::Title);
}

void UOVTitleScreenWidget::HandleLaunchSelectedScenario()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    AOVController* Controller = Cast<AOVController>(World->GetFirstPlayerController());
    if (!Controller)
    {
        return;
    }

    if (AOVHUD* HUD = Cast<AOVHUD>(Controller->GetHUD()))
    {
        HUD->CloseTitleScreen();
    }

    if (UOVPlayScenarioSubsystem* ScenarioSubsystem = World->GetSubsystem<UOVPlayScenarioSubsystem>())
    {
        ScenarioSubsystem->Start(*Controller, SelectedScenarioId);
    }
}

void UOVTitleScreenWidget::HandleToggleBackdropMode()
{
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

void UOVTitleScreenWidget::HandleQuitGame()
{
    UKismetSystemLibrary::QuitGame(GetWorld(), GetWorld()->GetFirstPlayerController(), EQuitPreference::Quit, false);
}
