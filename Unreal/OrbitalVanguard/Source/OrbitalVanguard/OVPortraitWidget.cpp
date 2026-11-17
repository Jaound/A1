#include "OVPortraitWidget.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

void UOVPortraitWidget::Configure(int32 InRole, bool bInSelected, bool bInLost)
{
    RoleIndex = FMath::Clamp(InRole, 1, 4);
    bSelected = bInSelected;
    bLost = bInLost;
    InvalidateLayoutAndVolatility();
}

int32 UOVPortraitWidget::NativePaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect,
    FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool bEnabled) const
{
    const FVector2D Size = Geometry.GetLocalSize();
    const float Scale = FMath::Min(Size.X / 100.f, Size.Y / 78.f);
    const FVector2D Origin((Size.X - 100 * Scale) * .5f, (Size.Y - 78 * Scale) * .5f);
    const FLinearColor Colors[] = { FLinearColor(.15f,.72f,1), FLinearColor(1,.56f,.19f), FLinearColor(.3f,.9f,.6f), FLinearColor(.71f,.5f,1) };
    const FLinearColor Accent = bLost ? FLinearColor(.38f,.4f,.44f) : Colors[RoleIndex - 1];
    auto Rect = [&](float X, float Y, float W, float H, FLinearColor Color)
    {
        FSlateDrawElement::MakeBox(Elements, Layer++, Geometry.ToPaintGeometry(FVector2D(W * Scale, H * Scale),
            FSlateLayoutTransform(Origin + FVector2D(X, Y) * Scale)), FCoreStyle::Get().GetBrush("WhiteBrush"), ESlateDrawEffect::None, Color);
    };
    Rect(0,0,100,78,FLinearColor(.015f,.035f,.065f));
    Rect(5,6,2,60,Accent*.35f); Rect(93,12,2,60,Accent*.35f);
    // Strong shoulder silhouette, neck, face planes and a cybernetic visor remain readable at card size.
    Rect(19,58,62,20,FLinearColor(.1f,.17f,.25f));
    Rect(13,57,23,13,Accent*.65f); Rect(64,57,23,13,Accent*.65f);
    Rect(26,53,12,22,FLinearColor(.18f,.27f,.34f)); Rect(62,53,12,22,FLinearColor(.18f,.27f,.34f));
    Rect(43,43,14,17,FLinearColor(.4f,.31f,.29f));
    Rect(36,18,28,29,bLost?FLinearColor(.36f,.37f,.4f):FLinearColor(.72f,.53f,.43f));
    Rect(39,42,22,8,bLost?FLinearColor(.36f,.37f,.4f):FLinearColor(.62f,.43f,.36f));
    Rect(53,23,11,20,FLinearColor(.31f,.27f,.29f));
    Rect(33,17,6,25,FLinearColor(.13f,.19f,.26f)); Rect(61,17,6,25,FLinearColor(.13f,.19f,.26f));
    Rect(36,12,28,9,FLinearColor(.09f,.12f,.18f)); Rect(40,8,20,7,FLinearColor(.09f,.12f,.18f));
    Rect(39,28,8,3,FLinearColor(.02f,.06f,.1f)); Rect(53,28,8,3,Accent);
    Rect(41,29,3,2,FLinearColor(.78f,.95f,1)); Rect(48,37,5,2,FLinearColor(.39f,.25f,.22f));
    Rect(42,44,17,2,FLinearColor(.14f,.2f,.25f));
    Rect(37,60,26,18,FLinearColor(.06f,.12f,.19f)); Rect(47,60,6,9,Accent);
    if (RoleIndex == 1)
    {
        // Armored brow and shield generator.
        Rect(32,19,36,6,Accent*.7f); Rect(10,36,15,32,Accent*.35f);
        Rect(12,38,11,24,Accent*.6f); Rect(15,41,5,17,FLinearColor(.65f,.9f,1));
    }
    else if (RoleIndex == 2)
    {
        Rect(49,25,18,8,FLinearColor(.07f,.12f,.17f)); Rect(52,27,13,4,Accent);
        Rect(78,23,5,48,FLinearColor(.35f,.4f,.44f)); Rect(76,28,9,8,Accent*.7f);
        Rect(80,15,2,11,FLinearColor(.6f,.64f,.67f));
    }
    else if (RoleIndex == 3)
    {
        Rect(35,15,30,5,Accent*.75f); Rect(22,56,5,14,FLinearColor(.75f,1,.9f));
        Rect(18,60,13,5,FLinearColor(.75f,1,.9f)); Rect(76,39,7,19,Accent*.7f);
        Rect(77,36,5,4,FLinearColor(.8f,.9f,.93f)); Rect(78,42,3,10,FLinearColor(.8f,1,.9f));
    }
    else
    {
        Rect(25,12,2,26,Accent*.5f); Rect(73,12,2,26,Accent*.5f);
        Rect(25,10,50,2,Accent*.5f); Rect(38,8,24,6,Accent*.8f);
        Rect(33,59,4,17,Accent); Rect(63,59,4,17,Accent);
        Rect(28,30,5,3,Accent); Rect(67,30,5,3,Accent);
    }
    if (bSelected)
    {
        Rect(0,0,100,2,Accent); Rect(0,76,100,2,Accent);
        Rect(0,0,2,78,Accent); Rect(98,0,2,78,Accent);
    }
    return Layer;
}
