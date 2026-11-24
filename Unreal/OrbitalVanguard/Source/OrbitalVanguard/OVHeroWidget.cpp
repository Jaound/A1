#include "OVHeroWidget.h"
#include "OVTypography.h"
#include "OVPortraitWidget.h"
#include "OVDevelopmentWidget.h"
#include "OVHeroSubsystem.h"
#include "OVInteriorSubsystem.h"
#include "OVTransportSubsystem.h"
#include "OVController.h"
#include "OVHUD.h"
#include "OVDevelopmentSubsystem.h"
#include "OVMissionSubsystem.h"
#include "OVShipSystemsComponent.h"
#include "OVShipLayout.h"
#include "OVShip.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/EditableTextBox.h"
#include "OVPlayScenarioSubsystem.h"
#include "Components/ScaleBox.h"
#include "Engine/Texture2D.h"
#include "Components/ButtonSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WrapBox.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

namespace OVHeroUI
{
UTextBlock* Text(UWidgetTree* Tree, const FString& Value, int32 Size = 0, EOVTextRole Role=EOVTextRole::Body)
{
    auto* Label = Tree->ConstructWidget<UTextBlock>();
    Label->SetFont(OVTypography::Style(Role,Size));
    Label->SetColorAndOpacity(FSlateColor(FLinearColor(.88f,.94f,1)));
    Label->SetText(FText::FromString(Value)); Label->SetAutoWrapText(true);
    return Label;
}
void StyleButton(UButton* Button)
{
    Button->SetBackgroundColor(FLinearColor(.045f,.15f,.23f));
    Button->SetColorAndOpacity(FLinearColor(.95f,.98f,1));
}
UButton* Button(UWidgetTree* Tree, UVerticalBox* Parent, const TCHAR* Label)
{
    auto* Result = Tree->ConstructWidget<UButton>(); StyleButton(Result);
    Result->AddChild(Text(Tree,Label,0,EOVTextRole::Button));
    Parent->AddChildToVerticalBox(Result)->SetPadding(FMargin(0,3));
    return Result;
}
UButton* RowButton(UWidgetTree* Tree, UHorizontalBox* Parent, const TCHAR* Label)
{
    auto* Result = Tree->ConstructWidget<UButton>(); StyleButton(Result);
    Result->AddChild(Text(Tree,Label,0,EOVTextRole::Button));
    auto* Slot = Parent->AddChildToHorizontalBox(Result);
    Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); Slot->SetPadding(FMargin(2,3));
    return Result;
}
FString Role(int32 Index)
{
    const TCHAR* Names[] = { TEXT("병력"), TEXT("근거리"), TEXT("원거리"), TEXT("지원"), TEXT("지휘") };
    return Names[FMath::Clamp(Index,0,4)];
}
FString State(const FOVInteriorUnitRecord& Unit)
{
    if (Unit.bDutyActive) return TEXT("보직 수행");
    if (Unit.bDutyRequested) return TEXT("보직 이동");
    if (Unit.State == EOVInteriorState::Operating) return Unit.Order == EOVInteriorOrder::Sabotage ? TEXT("공작") : TEXT("점령");
    const TCHAR* Names[] = { TEXT("대기"),TEXT("이동"),TEXT("교전"),TEXT("작업"),TEXT("수호"),TEXT("철수"),TEXT("운송"),TEXT("회수 대기"),TEXT("구조 대기"),TEXT("사망"),TEXT("이동 불가") };
    return Names[FMath::Clamp(int32(Unit.State),0,10)];
}
FString Facility(EOVFacilityType Type)
{
    const TCHAR* Names[] = {TEXT("빈 방"),TEXT("코어"),TEXT("함교"),TEXT("생명 유지"),TEXT("엔진"),TEXT("무기 제어"),TEXT("탄약고"),TEXT("축전실"),TEXT("미사일실"),TEXT("쉴드 제어"),TEXT("레이더"),TEXT("요격 제어"),TEXT("포드 발사"),TEXT("격납고"),TEXT("관제실"),TEXT("저장소"),TEXT("보안실")};
    return Names[FMath::Clamp(int32(Type),0,16)];
}
FString Location(const FOVInteriorUnitRecord& Unit)
{
    auto* Ship = Unit.Ship.Get();
    if (Unit.State == EOVInteriorState::InTransit) return Ship ? Ship->ShipName + TEXT(" · 운송") : TEXT("포드 운송");
    if (!Ship) return Unit.State == EOVInteriorState::RescueWaiting ? TEXT("우주 구조 대기") : TEXT("위치 미확인");
    FString Result = Ship->ShipName;
    if (Ship->Systems && Ship->Systems->Layout)
    {
        const auto* Layout = Ship->Systems->Layout.Get(); const int32 Index = Layout->GetSlotIndex(Unit.NodeId);
        if (Layout->Slots.IsValidIndex(Index)) Result += FString::Printf(TEXT(" · %d층 %s"),Layout->Slots[Index].DeckId,*Facility(Layout->Slots[Index].Facility));
    }
    return Result;
}
FString SkillName(int32 RoleIndex, int32 Index)
{
    static const TCHAR* Names[4][4] = {
        {TEXT("역장 방어"),TEXT("역장 엄호"),TEXT("강화 돌입"),TEXT("생존 한계 해방")},
        {TEXT("초감각 조준"),TEXT("약점 감지"),TEXT("전자기 제압"),TEXT("인지 가속")},
        {TEXT("생체 재구성"),TEXT("생존장 전개"),TEXT("재생 공명장"),TEXT("촉매 동조")},
        {TEXT("신경 공명"),TEXT("신경 접속"),TEXT("전투 동조"),TEXT("집단 기동 예지")}
    };
    return Names[FMath::Clamp(RoleIndex,1,4)-1][FMath::Clamp(Index,0,3)];
}
}

void UOVHeroCard::Pick() { if (Owner) Owner->PickCard(HeroId,UnitId); }
void UOVHeroCard::Update(const FOVHeroCardView& View)
{
    HeroId = View.HeroId; UnitId = View.UnitId;
    if(Portrait) Portrait->Configure(View.RoleIndex,View.bSelected,View.bLost);
    FString DisplayName=View.Name;
    if(View.bMissionOnly)DisplayName.RemoveFromEnd(TEXT(" 영웅"));
    if(DisplayName.Len()>14)DisplayName=DisplayName.Left(13)+TEXT("…");
    NameLabel->SetText(FText::FromString(DisplayName));
    RoleLabel->SetText(FText::FromString(FString::Printf(TEXT("%s · Lv.%d"),*View.Role,View.Level)));
    StateLabel->SetText(FText::FromString((View.bSelected ? TEXT("✓ ") : TEXT("")) + View.State));
    Health->SetVisibility(View.bHPKnown ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
    Health->SetPercent(View.bHPKnown ? FMath::Clamp(View.HP/FMath::Max(1.f,View.MaxHP),0.f,1.f) : 0.f);
    Health->SetFillColorAndOpacity(View.bLost ? FLinearColor(.4f,.4f,.45f) : FLinearColor(.25f,.85f,.6f));
    SetBackgroundColor(View.bSelected ? FLinearColor(.1f,.34f,.44f) : FLinearColor(.04f,.09f,.15f));
    OVTypography::Tooltip(this,FText::FromString(View.Name+TEXT("\n")+View.Location + (View.bHPKnown ? FString::Printf(TEXT("\nHP %.0f / %.0f"),View.HP,View.MaxHP) : TEXT(""))));
}

void UOVHeroWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    using namespace OVHeroUI;
    auto* Border = WidgetTree->ConstructWidget<UBorder>();
    Border->SetBrushColor(FLinearColor(.008f,.02f,.04f,.97f)); Border->SetPadding(FMargin(10)); WidgetTree->RootWidget = Border;
    auto* Frame = WidgetTree->ConstructWidget<UVerticalBox>(); Border->AddChild(Frame);
    auto* Outer = WidgetTree->ConstructWidget<UScrollBox>();
    auto* Box = WidgetTree->ConstructWidget<UVerticalBox>(); Outer->AddChild(Box);
    auto* Top = WidgetTree->ConstructWidget<UHorizontalBox>(); Frame->AddChildToVerticalBox(Top);
    Header = Text(WidgetTree,TEXT("영웅"),0,EOVTextRole::Heading); Top->AddChildToHorizontalBox(Header)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    auto* CloseButton=RowButton(WidgetTree,Top,TEXT("닫기"));
    CloseButton->OnClicked.AddDynamic(this,&UOVHeroWidget::Close);
    if(auto* CloseSlot=Cast<UHorizontalBoxSlot>(CloseButton->Slot))CloseSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
    // Selection and its primary actions remain visible while the roster scrolls.
    StandingBox=WidgetTree->ConstructWidget<USizeBox>(); StandingBox->SetHeightOverride(205);
    Box->AddChildToVerticalBox(StandingBox);
    auto* ArtFit=WidgetTree->ConstructWidget<UScaleBox>();ArtFit->SetStretch(EStretch::ScaleToFit);
    StandingBox->AddChild(ArtFit); Standing=WidgetTree->ConstructWidget<UImage>();ArtFit->AddChild(Standing);
    Detail = Text(WidgetTree,TEXT("초상화를 선택하세요.")); Frame->AddChildToVerticalBox(Detail)->SetPadding(FMargin(0,6,0,0));
    DetailNumbers = Text(WidgetTree,TEXT(""),0,EOVTextRole::Numeric);
    Frame->AddChildToVerticalBox(DetailNumbers)->SetPadding(FMargin(0,0,0,4));
    auto* Actions = WidgetTree->ConstructWidget<UHorizontalBox>(); Frame->AddChildToVerticalBox(Actions);
    LocateButton = RowButton(WidgetTree,Actions,TEXT("위치 찾기")); LocateButton->OnClicked.AddDynamic(this,&UOVHeroWidget::Locate);
    InteriorButton = RowButton(WidgetTree,Actions,TEXT("선내 지도")); InteriorButton->OnClicked.AddDynamic(this,&UOVHeroWidget::OpenInterior);
    RetreatButton = RowButton(WidgetTree,Actions,TEXT("철수")); RetreatButton->OnClicked.AddDynamic(this,&UOVHeroWidget::Retreat);
    OVTypography::Tooltip(RetreatButton,FText::FromString(TEXT("원 진입점까지 이동해 원래 운송수단으로 회수합니다.")));
    Feedback = Text(WidgetTree,TEXT("")); Feedback->SetColorAndOpacity(FSlateColor(FLinearColor(1,.78f,.3f)));
    Feedback->SetVisibility(ESlateVisibility::Collapsed); Frame->AddChildToVerticalBox(Feedback);
    Frame->AddChildToVerticalBox(Outer)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    EmptyHelp = Text(WidgetTree,TEXT("보유 영웅 없음")); Box->AddChildToVerticalBox(EmptyHelp);
    Search=WidgetTree->ConstructWidget<UEditableTextBox>();
    OVTypography::StyleInput(Search);
    Search->SetHintText(FText::FromString(TEXT("이름 · 유형 · 위치 · 상태 검색")));
    Search->SetForegroundColor(FLinearColor(.05f,.10f,.15f));
    Box->AddChildToVerticalBox(Search);
    auto* Filters=WidgetTree->ConstructWidget<UHorizontalBox>();
    Box->AddChildToVerticalBox(Filters);
    FilterCategory=WidgetTree->ConstructWidget<UOVCompactCombo>();
    CastChecked<UOVCompactCombo>(FilterCategory)->SetCompactFont();
    FilterValue=WidgetTree->ConstructWidget<UOVCompactCombo>();
    CastChecked<UOVCompactCombo>(FilterValue)->SetCompactFont();
    for(const TCHAR* Label:{TEXT("유형"),TEXT("임무"),TEXT("위치"),TEXT("상태")})FilterCategory->AddOption(Label);
    FilterCategory->SetSelectedIndex(0);
    Filters->AddChildToHorizontalBox(FilterCategory);
    Filters->AddChildToHorizontalBox(FilterValue);
    OwnedLabel = Text(WidgetTree,TEXT("보유 영웅")); Box->AddChildToVerticalBox(OwnedLabel)->SetPadding(FMargin(0,6));
    OwnedCards = WidgetTree->ConstructWidget<UWrapBox>(); OwnedCards->SetInnerSlotPadding(FVector2D(6,6)); Box->AddChildToVerticalBox(OwnedCards);
    MissionLabel = Text(WidgetTree,TEXT("임무 전용 영웅")); Box->AddChildToVerticalBox(MissionLabel)->SetPadding(FMargin(0,6));
    MissionCards = WidgetTree->ConstructWidget<UWrapBox>(); MissionCards->SetInnerSlotPadding(FVector2D(6,6)); Box->AddChildToVerticalBox(MissionCards);
    SkillsLabel = Text(WidgetTree,TEXT("자동 스킬")); Box->AddChildToVerticalBox(SkillsLabel)->SetPadding(FMargin(0,8,0,2));
    auto* Skills = WidgetTree->ConstructWidget<UWrapBox>(); Skills->SetInnerSlotPadding(FVector2D(4,4)); Box->AddChildToVerticalBox(Skills);
    for (int32 Index=0; Index<4; ++Index)
    {
        auto* Size = WidgetTree->ConstructWidget<USizeBox>(); Size->SetWidthOverride(148); Size->SetMinDesiredHeight(48); SkillSizes.Add(Size);
        auto* Skill = WidgetTree->ConstructWidget<UButton>(); StyleButton(Skill); Size->AddChild(Skill); Skills->AddChildToWrapBox(Size);
        auto* Label = Text(WidgetTree,TEXT(""),14); Skill->AddChild(Label); SkillButtons.Add(Skill); SkillLabels.Add(Label);
    }
    SkillButtons[0]->OnClicked.AddDynamic(this,&UOVHeroWidget::Skill0); SkillButtons[1]->OnClicked.AddDynamic(this,&UOVHeroWidget::Skill1);
    SkillButtons[2]->OnClicked.AddDynamic(this,&UOVHeroWidget::Skill2); SkillButtons[3]->OnClicked.AddDynamic(this,&UOVHeroWidget::Skill3);
    Button(WidgetTree,Box,TEXT("편성 · 보직 설정"))->OnClicked.AddDynamic(this,&UOVHeroWidget::ToggleFormation);
    FormationBox = WidgetTree->ConstructWidget<UVerticalBox>(); Box->AddChildToVerticalBox(FormationBox); FormationBox->SetVisibility(ESlateVisibility::Collapsed);
    TemplateChoice=WidgetTree->ConstructWidget<UOVCompactCombo>();
    CastChecked<UOVCompactCombo>(TemplateChoice)->SetCompactFont();
    for(const auto& D:GetGameInstance()->GetSubsystem<UOVHeroSubsystem>()->Definitions)
        TemplateChoice->AddOption(D.Name);
    TemplateChoice->SetSelectedIndex(0); FormationBox->AddChildToVerticalBox(TemplateChoice);
    ArenaAssign=Button(WidgetTree,FormationBox,TEXT("선택한 후보를 시험장에 배치"));
    ArenaAssign->OnClicked.AddDynamic(this,&UOVHeroWidget::AssignArenaCandidate);
    Button(WidgetTree,FormationBox,TEXT("편성실에서 시험 영웅 생성"))->OnClicked.AddDynamic(this,&UOVHeroWidget::CreateTemplate);
    Button(WidgetTree,FormationBox,TEXT("개발 · 정의와 아트 현황"))->OnClicked.AddDynamic(this,&UOVHeroWidget::ToggleCatalogue);
    CatalogueReport=Text(WidgetTree,TEXT(""),13); FormationBox->AddChildToVerticalBox(CatalogueReport);
    CatalogueReport->SetVisibility(ESlateVisibility::Collapsed);
    auto* ReserveRow = WidgetTree->ConstructWidget<UHorizontalBox>(); FormationBox->AddChildToVerticalBox(ReserveRow);
    auto* Reserve = RowButton(WidgetTree,ReserveRow,TEXT("편성")); Reserve->OnClicked.AddDynamic(this,&UOVHeroWidget::Deploy); OwnedActions.Add(Reserve);
    auto* Unreserve = RowButton(WidgetTree,ReserveRow,TEXT("편성 해제")); Unreserve->OnClicked.AddDynamic(this,&UOVHeroWidget::Release); OwnedActions.Add(Unreserve);
    auto* Spawn = Button(WidgetTree,FormationBox,TEXT("선택 함선에 배치")); Spawn->OnClicked.AddDynamic(this,&UOVHeroWidget::SpawnHero); OwnedActions.Add(Spawn);
    auto* BoardingRow = WidgetTree->ConstructWidget<UHorizontalBox>(); FormationBox->AddChildToVerticalBox(BoardingRow);
    auto* Boarding = RowButton(WidgetTree,BoardingRow,TEXT("강습 편성")); Boarding->OnClicked.AddDynamic(this,&UOVHeroWidget::BoardingOn); OwnedActions.Add(Boarding);
    auto* Unboard = RowButton(WidgetTree,BoardingRow,TEXT("강습 해제")); Unboard->OnClicked.AddDynamic(this,&UOVHeroWidget::BoardingOff); OwnedActions.Add(Unboard);
    auto* Combo = WidgetTree->ConstructWidget<UOVCompactCombo>(); Combo->SetCompactFont(); DutyChoice = Combo; FormationBox->AddChildToVerticalBox(DutyChoice);
    auto* DutyRow = WidgetTree->ConstructWidget<UHorizontalBox>(); FormationBox->AddChildToVerticalBox(DutyRow);
    auto* Duty = RowButton(WidgetTree,DutyRow,TEXT("보직 배치")); Duty->OnClicked.AddDynamic(this,&UOVHeroWidget::DutyOn); OwnedActions.Add(Duty);
    auto* Unduty = RowButton(WidgetTree,DutyRow,TEXT("보직 해제")); Unduty->OnClicked.AddDynamic(this,&UOVHeroWidget::DutyOff); OwnedActions.Add(Unduty);
    Button(WidgetTree,FormationBox,TEXT("편성실 이동"))->OnClicked.AddDynamic(this,&UOVHeroWidget::RosterRoom);
    Button(WidgetTree,FormationBox,TEXT("보유 편제로 출전"))->OnClicked.AddDynamic(this,&UOVHeroWidget::StartMission);
    auto* Trial = Button(WidgetTree,Box,TEXT("영웅 시험 장면")); Trial->OnClicked.AddDynamic(this,&UOVHeroWidget::StartTrial);
    OVTypography::Tooltip(Trial,FText::FromString(TEXT("무장 없는 구축전함에서 10명씩 편성합니다. 보유 명단은 시험 후 복원됩니다.")));
    Refresh();
}

void UOVHeroWidget::NativeTick(const FGeometry& Geometry, float DeltaSeconds)
{
    Super::NativeTick(Geometry,DeltaSeconds);
    LayoutCards(Geometry.GetLocalSize().X);
    RefreshElapsed += DeltaSeconds;
    if (RefreshElapsed >= .25f) { RefreshElapsed=0; Refresh(); }
}
void UOVHeroWidget::RefreshCards() { Refresh(); }

void UOVHeroWidget::RebuildCards()
{
    OwnedCards->ClearChildren(); MissionCards->ClearChildren(); Cards.Reset(); CardSizes.Reset(); PortraitSizes.Reset(); ++CardRebuildCount;
    for (const auto& View : Views)
    {
        auto* Card = WidgetTree->ConstructWidget<UOVHeroCard>(); Card->Owner=this; OVHeroUI::StyleButton(Card);
        Card->OnClicked.AddDynamic(Card,&UOVHeroCard::Pick);
        auto* Size = WidgetTree->ConstructWidget<USizeBox>(); Size->SetWidthOverride(148); Size->AddChild(Card); CardSizes.Add(Size);
        (View.bMissionOnly ? MissionCards : OwnedCards)->AddChildToWrapBox(Size);
        auto* Stack = WidgetTree->ConstructWidget<UVerticalBox>();
        if(auto* ContentSlot=Cast<UButtonSlot>(Card->AddChild(Stack)))
        {
            ContentSlot->SetHorizontalAlignment(HAlign_Fill); ContentSlot->SetVerticalAlignment(VAlign_Fill);
            ContentSlot->SetPadding(FMargin(4));
        }
        auto* PictureSize = WidgetTree->ConstructWidget<USizeBox>(); PictureSize->SetHeightOverride(78); Stack->AddChildToVerticalBox(PictureSize); PortraitSizes.Add(PictureSize);
        const auto* Def=GetGameInstance()->GetSubsystem<UOVHeroSubsystem>()->Definition(View.DefinitionId);
        if(Def && Def->Portrait)
        {
            auto* Fit=WidgetTree->ConstructWidget<UScaleBox>(); Fit->SetStretch(EStretch::ScaleToFit); PictureSize->AddChild(Fit);
            auto* Image=WidgetTree->ConstructWidget<UImage>(); Image->SetBrushFromTexture(Def->Portrait,true); Fit->AddChild(Image);
        }
        else {
        Card->Portrait = WidgetTree->ConstructWidget<UOVPortraitWidget>(); Card->Portrait->SetVisibility(ESlateVisibility::HitTestInvisible); PictureSize->AddChild(Card->Portrait);
        }
        Card->NameLabel = OVHeroUI::Text(WidgetTree,View.Name,0,EOVTextRole::HeroName); Stack->AddChildToVerticalBox(Card->NameLabel);
        Card->RoleLabel = OVHeroUI::Text(WidgetTree,TEXT(""),14); Stack->AddChildToVerticalBox(Card->RoleLabel);
        auto* BarSize = WidgetTree->ConstructWidget<USizeBox>(); BarSize->SetHeightOverride(5); Stack->AddChildToVerticalBox(BarSize)->SetPadding(FMargin(0,3));
        Card->Health = WidgetTree->ConstructWidget<UProgressBar>(); BarSize->AddChild(Card->Health);
        Card->StateLabel = OVHeroUI::Text(WidgetTree,TEXT(""),14); Stack->AddChildToVerticalBox(Card->StateLabel);
        Cards.Add(Card); Card->Update(View);
    }
    LayoutCards(LastLayoutWidth>0?LastLayoutWidth:340.f,true);
}

void UOVHeroWidget::LayoutCards(float AvailableWidth, bool bForce)
{
    if(AvailableWidth<120 || (!bForce&&FMath::Abs(AvailableWidth-LastLayoutWidth)<1.f))return;
    LastLayoutWidth=AvailableWidth;
    // Two columns in widget space, independent of viewport DPI. Only geometry changes resize the children.
    const float CardWidth=FMath::Max(60.f,FMath::FloorToFloat((AvailableWidth-44.f)*.5f));
    for(const auto& Size:CardSizes)Size->SetWidthOverride(CardWidth);
    for(const auto& Size:SkillSizes)Size->SetWidthOverride(CardWidth);
    for(const auto& Size:PortraitSizes)Size->SetHeightOverride(FMath::Clamp(CardWidth*.53f,78.f,140.f));
}

void UOVHeroWidget::Refresh()
{
    auto* PC = Cast<AOVController>(GetOwningPlayer());
    auto* Heroes = GetGameInstance() ? GetGameInstance()->GetSubsystem<UOVHeroSubsystem>() : nullptr;
    if (!PC || !Heroes || !OwnedCards || !MissionCards) return;
    Views.Reset();
    const auto Owned = Heroes->GetHeroes(0);
    for (const auto& Hero : Owned)
    {
        FOVHeroCardView View; View.HeroId=Hero.HeroId; View.DefinitionId=Hero.DefinitionId; View.Name=Hero.Name; View.RoleIndex=int32(Hero.Role); View.Role=OVHeroUI::Role(View.RoleIndex); View.Level=Hero.Level;
        const TCHAR* States[] = {TEXT("대기"),TEXT("편성"),TEXT("구조 대기"),TEXT("포로"),TEXT("실종"),TEXT("사망")};
        View.State=States[FMath::Clamp(int32(Hero.Status),0,5)]; View.Location=TEXT("대기 명단"); View.bLost=Hero.Status==EOVHeroStatus::Dead;
        if (PC->Interior && Hero.RuntimeUnitId>0)
        {
            const auto Unit=PC->Interior->GetUnit(Hero.RuntimeUnitId);
            if (Unit.UnitId==Hero.RuntimeUnitId && Unit.HeroId==Hero.HeroId && Unit.FactionId==0)
            {
                View.UnitId=Unit.UnitId; View.State=OVHeroUI::State(Unit); View.Location=OVHeroUI::Location(Unit);
                View.HP=Unit.HP; View.MaxHP=Unit.MaxHP; View.bHPKnown=true; View.bLost=Unit.State==EOVInteriorState::Dead;
            }
        }
        Views.Add(View);
    }
    if (PC->Interior) for (const auto& Unit : PC->Interior->Units)
    {
        if (Unit.FactionId!=0 || Unit.Role==EOVInteriorRole::Trooper || Unit.HeroId.IsValid()) continue;
        FOVHeroCardView View; View.UnitId=Unit.UnitId; View.Name=Unit.Name; View.RoleIndex=int32(Unit.Role); View.Role=OVHeroUI::Role(View.RoleIndex); View.Level=Unit.HeroLevel;
        View.State=OVHeroUI::State(Unit); View.Location=OVHeroUI::Location(Unit); View.HP=Unit.HP; View.MaxHP=Unit.MaxHP;
        View.bHPKnown=true; View.bMissionOnly=true; View.bLost=Unit.State==EOVInteriorState::Dead; Views.Add(View);
    }
    const int32 Category=FilterCategory->GetSelectedIndex();
    auto ValueOf=[Category](const FOVHeroCardView& V)->FString{
        switch(Category){
        case 1: return V.bMissionOnly?TEXT("임무 전용"):V.UnitId>0?TEXT("작전 참가"):TEXT("대기 명단");
        case 2: return V.Location;
        case 3: return V.State;
        default:return V.Role;
        }
    };
    TArray<FString> Options;
    for(const auto& V:Views)Options.AddUnique(ValueOf(V));
    Options.Sort();
    if(LastFilterCategory!=Category || Options!=LastFilterOptions){
        const FString Previous=LastFilterCategory==Category?FilterValue->GetSelectedOption():FString();
        FilterValue->ClearOptions();FilterValue->AddOption(TEXT("전체"));
        for(const auto& Option:Options)FilterValue->AddOption(Option);
        FilterValue->SetSelectedIndex(Options.Find(Previous)+1);
        LastFilterOptions=MoveTemp(Options);LastFilterCategory=Category;
    }
    if(FilterValue->GetSelectedIndex()>0){
        const FString SelectedValue=FilterValue->GetSelectedOption();
        Views.RemoveAll([&](const auto& V){return ValueOf(V)!=SelectedValue;});
    }
    const FString Filter=Search->GetText().ToString().TrimStartAndEnd();
    if(!Filter.IsEmpty())
        Views.RemoveAll([&](const auto& V){return !(V.Name+TEXT(" ")+V.Role+TEXT(" ")+V.Location+TEXT(" ")+V.State).Contains(Filter);});
    const bool HasSelection = Views.ContainsByPredicate([&](const FOVHeroCardView& V) { return SelectedHeroId.IsValid() ? V.HeroId==SelectedHeroId : V.bMissionOnly && V.UnitId==SelectedMissionUnitId; });
    if (!HasSelection)
    {
        SelectedHeroId=Views.Num()?Views[0].HeroId:FGuid();
        SelectedMissionUnitId=Views.Num()&&Views[0].bMissionOnly?Views[0].UnitId:0;
    }
    FString Signature;
    for (auto& View : Views)
    {
        View.bSelected=SelectedHeroId.IsValid()?View.HeroId==SelectedHeroId:View.bMissionOnly&&View.UnitId==SelectedMissionUnitId;
        Signature+=View.HeroId.ToString()+FString::FromInt(View.UnitId)+View.Name+TEXT(";");
    }
    if (RosterSignature!=Signature) { RosterSignature=Signature; RebuildCards(); }
    for (int32 I=0; I<Cards.Num()&&I<Views.Num(); ++I) Cards[I]->Update(Views[I]);
    const int32 OwnedCount=Owned.Num(), MissionCount=Views.FilterByPredicate([](const auto& V){return V.bMissionOnly;}).Num();
    Header->SetText(FText::FromString(FString::Printf(TEXT("영웅 · 현역 %d / %d"),Heroes->CountDeployed(0),Heroes->DeploymentLimit(0))));
    OwnedLabel->SetText(FText::FromString(FString::Printf(TEXT("보유 영웅 %d / 30"),OwnedCount)));
    OwnedLabel->SetVisibility(OwnedCount?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
    MissionLabel->SetText(FText::FromString(FString::Printf(TEXT("임무 전용 영웅 %d"),MissionCount)));
    MissionLabel->SetVisibility(MissionCount?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
    EmptyHelp->SetVisibility(Views.IsEmpty()?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
    const auto* Selected=Views.FindByPredicate([](const FOVHeroCardView& V){return V.bSelected;});
    const bool bOwned=Selected&&Selected->HeroId.IsValid();
    const bool bUnit=Selected&&Selected->UnitId>0;
    LocateButton->SetIsEnabled(bUnit); InteriorButton->SetIsEnabled(bUnit); RetreatButton->SetIsEnabled(bUnit&&!Selected->bLost);
    for (const auto& Action : OwnedActions) Action->SetIsEnabled(bOwned);
    DutyChoice->SetIsEnabled(bOwned&&bUnit);
    const auto* Scenario=PC->GetPlayScenario();
    const bool Preparing=Scenario && Scenario->IsArenaPreparing();
    ArenaAssign->SetVisibility(Preparing?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
    const auto* Def=Selected?Heroes->Definition(Selected->DefinitionId):nullptr;
    StandingBox->SetVisibility(Def && Def->Standing?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
    if(Def && Def->Standing && Standing->GetBrush().GetResourceObject()!=Def->Standing)
    {
        FSlateBrush Brush;
        Brush.SetResourceObject(Def->Standing);
        Brush.SetUVRegion(Def->StandingRegion);
        Brush.SetImageSize(Def->StandingRegion.GetSize()*FVector2D(Def->Standing->GetSizeX(),Def->Standing->GetSizeY()));
        Standing->SetBrush(Brush);
    }
    if (!Selected)
    {
        Detail->SetText(FText::FromString(TEXT("시험 장면에서 대표 영웅을 만나보세요.")));
        DetailNumbers->SetVisibility(ESlateVisibility::Collapsed);
        SkillsLabel->SetVisibility(ESlateVisibility::Collapsed);
        for (const auto& Skill:SkillButtons) Skill->SetVisibility(ESlateVisibility::Collapsed);
        return;
    }
    const auto Hero=Heroes->GetHero(Selected->HeroId);
    const auto Unit=PC->Interior&&bUnit?PC->Interior->GetUnit(Selected->UnitId):FOVInteriorUnitRecord();
    FString Summary=Selected->Name+TEXT(" · ")+Selected->State+TEXT("\n")+Selected->Location;
    TArray<FString, TInlineAllocator<3>> Stats;
    if (Selected->bHPKnown)
        Stats.Add(FString::Printf(TEXT("HP %s / %s"),*OVTypography::Integer(FMath::RoundToInt(Selected->HP)),*OVTypography::Integer(FMath::RoundToInt(Selected->MaxHP))));
    if (bOwned) Stats.Add(TEXT("경험치 ") + OVTypography::Integer(Hero.XP));
    if (Selected->RoleIndex==int32(EOVInteriorRole::Support)) Stats.Add(TEXT("촉매 ") + OVTypography::Integer(bUnit ? Unit.Medkits : Hero.Catalysts));
    DetailNumbers->SetText(FText::FromString(FString::Join(Stats,TEXT("  ·  "))));
    DetailNumbers->SetVisibility(Stats.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
    if(bOwned && (Hero.ArmorTier || Hero.FocusTier || Hero.TacticalTier))
        Summary+=FString::Printf(TEXT("\n초점 T%d · 외피 T%d · 증폭 T%d"),Hero.FocusTier,Hero.ArmorTier,Hero.TacticalTier);
    if(Def && !Def->Passive.IsEmpty())Summary+=Def->Id==TEXT("M02")?TEXT("\n고경감 파쇄 · 역장 종료 시 잔영 베기"):TEXT("\n고유 능력 · 마우스를 올려 설명 확인");
    Detail->SetText(FText::FromString(Summary)); OVTypography::Tooltip(Detail,FText::FromString(Def ? Def->Story+TEXT("\n")+Def->Passive+TEXT("\n")+Def->Signature+TEXT("\n")+Unit.Reason : Unit.Reason));
    AOVShip* UnitShip=Unit.Ship.Get();
    if (DutyHero!=SelectedHeroId || DutyShip.Get()!=UnitShip)
    {
        DutyHero=SelectedHeroId; DutyShip=UnitShip; DutySlots.Reset(); DutyChoice->ClearOptions();
        TMap<FString,int32> LabelCounts;
        if (UnitShip&&UnitShip->Systems&&UnitShip->Systems->Layout) for (const auto& FacilitySlot:UnitShip->Systems->Layout->Slots)
        {
            DutySlots.Add(FacilitySlot.SlotId);
            FString Label=FString::Printf(TEXT("%d층 · %s"),FacilitySlot.DeckId,*OVHeroUI::Facility(FacilitySlot.Facility));
            const int32 Number=++LabelCounts.FindOrAdd(Label);
            if(Number>1)Label+=FString::Printf(TEXT(" %d"),Number);
            DutyChoice->AddOption(Label);
        }
        if (DutySlots.Num()) DutyChoice->SetSelectedIndex(0);
    }
    SkillsLabel->SetVisibility(ESlateVisibility::Visible);
    SkillsLabel->SetText(FText::FromString(bOwned?FString::Printf(TEXT("자동 스킬  ·  성장 %d점"),Hero.SkillPoints):TEXT("자동 스킬")));
    const double Time=PC->Interior?PC->Interior->GameTime:0;
    const double Ready[]={Unit.SkillReadyAt,0,Unit.Skill3ReadyAt,Unit.Skill4ReadyAt};
    for (int32 I=0; I<4; ++I)
    {
        const int32 Rank=bOwned&&Hero.SkillRanks.IsValidIndex(I)?Hero.SkillRanks[I]:Unit.HeroSkillRanks.IsValidIndex(I)?Unit.HeroSkillRanks[I]:(I==0?1:0);
        const float Cooldown=bUnit?FMath::Max(0.,Ready[I]-Time):Hero.CooldownRemaining.IsValidIndex(I)?Hero.CooldownRemaining[I]:0.f;
        const FString Status=Rank<=0?TEXT("잠김"):Cooldown>0?FString::Printf(TEXT("%.1f초"),Cooldown):TEXT("자동");
        SkillButtons[I]->SetVisibility(ESlateVisibility::Visible);
        SkillLabels[I]->SetText(FText::FromString(FString::Printf(TEXT("%s\n%d등급 · %s"),*OVHeroUI::SkillName(Selected->RoleIndex,I),Rank,*Status)));
        const bool CanSpend=bOwned&&Heroes->CanChangeRoster()&&Hero.SkillPoints>0&&Rank>0&&Rank<3;
        SkillButtons[I]->SetIsEnabled(CanSpend);
        OVTypography::Tooltip(SkillButtons[I],FText::FromString(bOwned?Heroes->GetSkillDescription(SelectedHeroId,I)+(CanSpend?TEXT("\n클릭: 성장 포인트 1점 투자"):TEXT("\n전투 중 자동 사용")):TEXT("임무 전용 인원 · 전투 조건에 따라 자동 사용")));
    }
}

bool UOVHeroWidget::SelectHero(FGuid HeroId)
{
    auto* Heroes=GetGameInstance()?GetGameInstance()->GetSubsystem<UOVHeroSubsystem>():nullptr;
    if (!Heroes || !HeroId.IsValid() || Heroes->GetHero(HeroId).FactionId!=0) return false;
    SelectedHeroId=HeroId; SelectedMissionUnitId=0; Refresh(); return true;
}
bool UOVHeroWidget::SelectMissionHero(int32 UnitId)
{
    auto* PC=Cast<AOVController>(GetOwningPlayer()); if (!PC||!PC->Interior) return false;
    const auto Unit=PC->Interior->GetUnit(UnitId);
    if (UnitId<=0||Unit.UnitId!=UnitId||Unit.FactionId!=0||Unit.Role==EOVInteriorRole::Trooper||Unit.HeroId.IsValid()) return false;
    SelectedHeroId=FGuid(); SelectedMissionUnitId=UnitId; Refresh(); return true;
}
void UOVHeroWidget::PickCard(FGuid HeroId,int32 UnitId) { if (HeroId.IsValid()) SelectHero(HeroId); else SelectMissionHero(UnitId); }
int32 UOVHeroWidget::GetSelectedUnitId() const
{
    const auto* View=Views.FindByPredicate([](const FOVHeroCardView& V){return V.bSelected;}); return View?View->UnitId:0;
}
TArray<FString> UOVHeroWidget::GetSelectedSkillLabels() const
{
    TArray<FString> Result;
    for(const auto& Label:SkillLabels)Result.Add(Label?Label->GetText().ToString():FString());
    return Result;
}
void UOVHeroWidget::SetFeedback(const FString& Message)
{
    if (!Feedback) return;
    FString Short=Message; Short.ReplaceInline(TEXT("\r"),TEXT(" ")); Short.ReplaceInline(TEXT("\n"),TEXT(" "));
    Feedback->SetText(FText::FromString(Short.Left(36))); OVTypography::Tooltip(Feedback,FText::FromString(Message));
    Feedback->SetVisibility(Message.IsEmpty()?ESlateVisibility::Collapsed:ESlateVisibility::Visible);
}
bool UOVHeroWidget::LocateSelectedHero()
{
    auto* PC=Cast<AOVController>(GetOwningPlayer()); if (!PC||!PC->Interior) return false;
    const auto Unit=PC->Interior->GetUnit(GetSelectedUnitId());
    if (Unit.UnitId<=0||Unit.FactionId!=0) return false;
    if (Unit.State==EOVInteriorState::InTransit && PC->Transport)
    {
        const auto Person=PC->Transport->GetPerson(Unit.PersonId);
        if (Person.PersonId==Unit.PersonId && Person.RuntimeUnitId==Unit.UnitId && Person.FactionId==0 && Person.State==EOVPassengerState::InPod)
        {
            const auto* Pod=PC->Transport->Pods.FindByPredicate([&](const FOVPodState& Entry)
            {
                return Entry.PodId==Person.PodId && Entry.PersonId==Person.PersonId && Entry.FactionId==0 &&
                    (Entry.State==EOVPodFlightState::Outbound || Entry.State==EOVPodFlightState::Returning);
            });
            if (Pod)
            {
                PC->bFollow=false; PC->CameraFocus=Pod->Position;
                SetFeedback(Unit.Name+TEXT(" · 비행 중인 포드 위치")); return true;
            }
        }
    }
    auto* Ship=Unit.Ship.Get();
    if (!Ship) { SetFeedback(TEXT("현재 위치를 찾을 수 없습니다.")); return false; }
    PC->SelectShip(Ship); PC->CameraFocus=Ship->GetActorLocation(); PC->bFollow=true;
    SetFeedback(Unit.Name+TEXT(" 위치로 이동")); return true;
}
void UOVHeroWidget::Locate() { LocateSelectedHero(); }
void UOVHeroWidget::OpenInterior()
{
    auto* PC=Cast<AOVController>(GetOwningPlayer()); if (!PC||!PC->Interior) return;
    auto* HUD=Cast<AOVHUD>(PC->GetHUD()); if (!HUD) return;
    const auto Unit=PC->Interior->GetUnit(GetSelectedUnitId()); auto* Ship=Unit.Ship.Get();
    if (Unit.State==EOVInteriorState::InTransit) { SetFeedback(TEXT("운송 완료 후 선내 위치를 확인할 수 있습니다.")); return; }
    if (Unit.UnitId<=0||!Ship||!PC->Interior->IsStructureKnown(0,Ship)) { SetFeedback(TEXT("내부 위치 미확인")); return; }
    HUD->bInteriorOpen=true; HUD->InteriorShip=Ship; HUD->InteriorSelectedUnits={Unit.UnitId}; HUD->InteriorTargetSlot=Unit.TargetSlot;
    if (Ship->Systems&&Ship->Systems->Layout)
    {
        const int32 Index=Ship->Systems->Layout->GetSlotIndex(Unit.NodeId);
        HUD->InteriorDeck=Ship->Systems->Layout->Slots.IsValidIndex(Index)?Ship->Systems->Layout->Slots[Index].DeckId:1;
    }
    HUD->SetHeroPanelOpen(false);
}
void UOVHeroWidget::Retreat()
{
    auto* PC=Cast<AOVController>(GetOwningPlayer()); if (!PC||!PC->Interior) return;
    const int32 Id=GetSelectedUnitId(); const auto Unit=PC->Interior->GetUnit(Id);
    if (Id<=0||Unit.FactionId!=0) return;
    PC->Interior->IssueOrder({Id},EOVInteriorOrder::Retreat,NAME_None); SetFeedback(PC->Interior->LastMessage); Refresh();
}
void UOVHeroWidget::ToggleFormation() { FormationBox->SetVisibility(FormationBox->GetVisibility()==ESlateVisibility::Collapsed?ESlateVisibility::Visible:ESlateVisibility::Collapsed); }
void UOVHeroWidget::Deploy() { if (auto* Heroes=GetGameInstance()->GetSubsystem<UOVHeroSubsystem>()) { Heroes->SetDeployment(SelectedId(),true); SetFeedback(Heroes->LastMessage); Refresh(); } }
void UOVHeroWidget::Release() { if (auto* Heroes=GetGameInstance()->GetSubsystem<UOVHeroSubsystem>()) { Heroes->SetDeployment(SelectedId(),false); SetFeedback(Heroes->LastMessage); Refresh(); } }
void UOVHeroWidget::SpawnHero()
{
    auto* PC=Cast<AOVController>(GetOwningPlayer());
    if (PC&&PC->Interior) { PC->Interior->SpawnRosterHero(PC->Selected,SelectedId(),NAME_None); SetFeedback(PC->Interior->LastMessage); }
    Refresh();
}
void UOVHeroWidget::DutyOn()
{
    auto* PC=Cast<AOVController>(GetOwningPlayer()); auto* Heroes=GetGameInstance()->GetSubsystem<UOVHeroSubsystem>();
    if (PC&&PC->Interior&&Heroes) { const int32 Index=DutyChoice->GetSelectedIndex(); PC->Interior->SetHeroDuty(Heroes->GetHero(SelectedId()).RuntimeUnitId,true,DutySlots.IsValidIndex(Index)?DutySlots[Index]:NAME_None); SetFeedback(PC->Interior->LastMessage); }
    Refresh();
}
void UOVHeroWidget::DutyOff()
{
    auto* PC=Cast<AOVController>(GetOwningPlayer()); auto* Heroes=GetGameInstance()->GetSubsystem<UOVHeroSubsystem>();
    if (PC&&PC->Interior&&Heroes) { PC->Interior->SetHeroDuty(Heroes->GetHero(SelectedId()).RuntimeUnitId,false,NAME_None); SetFeedback(PC->Interior->LastMessage); }
    Refresh();
}
void UOVHeroWidget::Spend(int32 Index) { if (auto* Heroes=GetGameInstance()->GetSubsystem<UOVHeroSubsystem>()) { Heroes->SpendSkillPoint(SelectedId(),Index); SetFeedback(Heroes->LastMessage); } Refresh(); }
void UOVHeroWidget::Skill0(){Spend(0);} void UOVHeroWidget::Skill1(){Spend(1);} void UOVHeroWidget::Skill2(){Spend(2);} void UOVHeroWidget::Skill3(){Spend(3);}
void UOVHeroWidget::AssignArenaCandidate()
{
    auto* PC=Cast<AOVController>(GetOwningPlayer());
    auto* Scenario=PC?PC->GetPlayScenario():nullptr;
    const auto* Roster=GetGameInstance()->GetSubsystem<UOVHeroSubsystem>();
    const int32 Index=TemplateChoice->GetSelectedIndex();
    if(!Scenario || !Roster->Definitions.IsValidIndex(Index))return;
    Scenario->SelectArenaHero(*PC,Roster->Definitions[Index].Id);
    SetFeedback(PC->LastAction);Refresh();
}

void UOVHeroWidget::CreateTemplate()
{
    auto* Roster=GetGameInstance()->GetSubsystem<UOVHeroSubsystem>();
    const int32 Index=TemplateChoice->GetSelectedIndex();
    if(!Roster->CanChangeRoster()){SetFeedback(TEXT("먼저 편성실로 이동하세요"));return;}
    if(!Roster->Definitions.IsValidIndex(Index))return;
    const auto Id=Roster->CreateDefinedHero(0,Roster->Definitions[Index].Id);
    if(Id.IsValid()){SelectedHeroId=Id;Search->SetText(FText::GetEmpty());}
    SetFeedback(Roster->LastMessage);Refresh();
}
void UOVHeroWidget::ToggleCatalogue()
{
    if(CatalogueReport->GetVisibility()!=ESlateVisibility::Collapsed){CatalogueReport->SetVisibility(ESlateVisibility::Collapsed);return;}
    FString Lines=TEXT("정의  ·  스탠딩 / 초상 / 선내  ·  고유 효과");
    for(const auto& D:GetGameInstance()->GetSubsystem<UOVHeroSubsystem>()->Definitions)
        Lines+=FString::Printf(TEXT("\n%s  %s   %s / %s / %s   %s"),*D.Id.ToString(),*D.Name,
            D.Standing?TEXT("●"):TEXT("―"),D.Portrait?TEXT("●"):TEXT("―"),D.SD?TEXT("●"):TEXT("―"),
            D.Passive.IsEmpty()?TEXT("공통 기술"):(D.ValidationNote.IsEmpty()?TEXT("판정 연결 · 발동 검증 대기"):*D.ValidationNote));
    CatalogueReport->SetText(FText::FromString(Lines));CatalogueReport->SetVisibility(ESlateVisibility::Visible);
}
void UOVHeroWidget::RosterRoom() { if (auto* Development=GetWorld()->GetSubsystem<UOVDevelopmentSubsystem>()) { Development->OpenRosterRoom(); SetFeedback(Development->LastMessage); Refresh(); } }
void UOVHeroWidget::StartMission()
{
    auto* PC=Cast<AOVController>(GetOwningPlayer()); if (PC&&PC->Mission) { PC->Mission->StartRosterMission(PC,false,260112,false); SetFeedback(PC->Mission->Status); Refresh(); }
}
void UOVHeroWidget::StartTrial()
{
    auto* PC=Cast<AOVController>(GetOwningPlayer()); if(PC)PC->StartPlayScenario(TEXT("HeroArena"));
}
void UOVHeroWidget::Close() { if (auto* PC=Cast<AOVController>(GetOwningPlayer())) if (auto* HUD=Cast<AOVHUD>(PC->GetHUD())) HUD->SetHeroPanelOpen(false); }
void UOVHeroWidget::SetBoarding(bool bAssigned)
{
    auto* PC=Cast<AOVController>(GetOwningPlayer()); auto* Heroes=GetGameInstance()?GetGameInstance()->GetSubsystem<UOVHeroSubsystem>():nullptr;
    if (PC&&PC->Interior&&Heroes) { PC->Interior->SetHeroBoardingAssignment(Heroes->GetHero(SelectedId()).RuntimeUnitId,bAssigned); SetFeedback(PC->Interior->LastMessage); Refresh(); }
}
void UOVHeroWidget::BoardingOn(){SetBoarding(true);} void UOVHeroWidget::BoardingOff(){SetBoarding(false);}
