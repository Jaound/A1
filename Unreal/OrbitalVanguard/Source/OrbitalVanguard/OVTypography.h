#pragma once
#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"

class UFont;
class UWidget;
class UEditableTextBox;

enum class EOVTextRole : uint8 { Body, Heading, Button, HeroName, Numeric, Logo, Comms, Count };

namespace OVTypography
{
    UFont* Font(EOVTextRole Role);
    UFont* ShipNameFont(const FString& Name);
    FSlateFontInfo Style(EOVTextRole Role, int32 LayoutSize = 0);
    void StyleInput(UEditableTextBox* Input, EOVTextRole Role = EOVTextRole::Body);
    void Tooltip(UWidget* Owner, const FText& Text);
    FString Integer(int32 Value);
    FString Metres(double Value);
}
