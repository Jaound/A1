#include "OVTypography.h"
#include "Engine/Font.h"
#include "Components/Border.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
struct FRoleStyle { const TCHAR* Asset; int32 Points; };
constexpr FRoleStyle Roles[] = {
    {TEXT("Body"),15}, {TEXT("Heading"),19}, {TEXT("Heading"),14},
    {TEXT("Heading"),17}, {TEXT("Numeric"),14}, {TEXT("Logo"),32}, {TEXT("Comms"),14}
};
static_assert(UE_ARRAY_COUNT(Roles) == int32(EOVTextRole::Count));
}

UFont* OVTypography::Font(EOVTextRole Role)
{
    const int32 Index = int32(Role);
    static TStrongObjectPtr<UFont> Fonts[UE_ARRAY_COUNT(Roles)];
    auto& Cached = Fonts[Index];
    if (!Cached.IsValid())
    {
        const FString Path = FString::Printf(TEXT("/Game/UI/Fonts/Typography/Font_%s.Font_%s"),Roles[Index].Asset,Roles[Index].Asset);
        Cached.Reset(LoadObject<UFont>(nullptr,*Path));
        ensureMsgf(Cached.IsValid(),TEXT("Missing UI font %s"),*Path);
    }
    return Cached.Get();
}

UFont* OVTypography::ShipNameFont(const FString& Name)
{
    for (TCHAR Ch : Name)
    {
        if (Ch > 127) return Font(EOVTextRole::Heading);
    }
    return Font(EOVTextRole::Logo);
}

FSlateFontInfo OVTypography::Style(EOVTextRole Role, int32 LayoutSize)
{
    return FSlateFontInfo(Font(Role),LayoutSize > 0 ? LayoutSize : Roles[int32(Role)].Points,TEXT("Regular"));
}

void OVTypography::StyleInput(UEditableTextBox* Input, EOVTextRole Role)
{
    auto StyleData = Input->GetWidgetStyle();
    StyleData.TextStyle.SetFont(Style(Role));
    Input->SetWidgetStyle(StyleData);
}

void OVTypography::Tooltip(UWidget* Owner, const FText& Text)
{
    UBorder* Border = Cast<UBorder>(Owner->GetToolTip());
    UTextBlock* Label = Border ? Cast<UTextBlock>(Border->GetContent()) : nullptr;
    if (!Label)
    {
        Border = NewObject<UBorder>(Owner);
        Border->SetPadding(FMargin(10,7));
        Border->SetBrushColor(FLinearColor(.02f,.04f,.06f,.98f));
        Label = NewObject<UTextBlock>(Border);
        Label->SetFont(Style(EOVTextRole::Body));
        Label->SetColorAndOpacity(FSlateColor(FLinearColor::White));
        Label->SetWrapTextAt(360.f);
        Border->SetContent(Label);
        Owner->SetToolTip(Border);
    }
    if (!Label->GetText().EqualTo(Text)) Label->SetText(Text);
}

FString OVTypography::Integer(int32 Value)
{
    return FText::AsNumber(Value).ToString();
}

FString OVTypography::Metres(double Value)
{
    return Integer(FMath::RoundToInt32(Value)) + TEXT(" m");
}
