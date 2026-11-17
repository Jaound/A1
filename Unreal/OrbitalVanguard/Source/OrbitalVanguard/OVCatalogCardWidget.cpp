#include "OVCatalogCardWidget.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

int32 UOVCatalogIcon::NativePaint(const FPaintArgs& Args, const FGeometry& Geometry,
    const FSlateRect& CullingRect, FSlateWindowElementList& Elements, int32 Layer,
    const FWidgetStyle& Style, bool bParentEnabled) const
{
    Super::NativePaint(Args, Geometry, CullingRect, Elements, Layer, Style, bParentEnabled);
    const FVector2D Size=Geometry.GetLocalSize();
    const auto P=[Size](float X,float Y){return FVector2D(X*Size.X,Y*Size.Y);};
    const auto Line=[&](std::initializer_list<FVector2D> Points,FLinearColor Color,float Thickness=2.f)
    {
        TArray<FVector2D> Path;for(const FVector2D& Point:Points)Path.Add(Point);
        FSlateDrawElement::MakeLines(Elements,Layer+1,Geometry.ToPaintGeometry(),Path,
            ESlateDrawEffect::None,Color,true,Thickness);
    };
    const auto Box=[&](float X,float Y,float W,float H,FLinearColor Color)
    {
        FSlateDrawElement::MakeBox(Elements,Layer,
            Geometry.ToPaintGeometry(P(W,H),FSlateLayoutTransform(P(X,Y))),
            FCoreStyle::Get().GetBrush("WhiteBrush"),ESlateDrawEffect::None,Color);
    };
    Box(.02f,.04f,.96f,.92f,FLinearColor(.025f,.055f,.085f));
    if(Kind==0 || Kind==5)
    {
        const float Width=Kind==5?.22f:.12f+.025f*FMath::Clamp(Variant,1,4);
        Line({P(.08f,.5f),P(.26f,.5f-Width),P(.72f,.5f-Width),P(.93f,.5f),
            P(.72f,.5f+Width),P(.26f,.5f+Width),P(.08f,.5f)},Accent,3);
        Box(.26f,.5f-Width,.42f,Width*2,Accent*.35f);
        Line({P(.28f,.5f),P(.79f,.5f)},Accent,2);
        Line({P(.24f,.4f),P(.1f,.25f),P(.48f,.4f)},Accent,2);
        Line({P(.24f,.6f),P(.1f,.75f),P(.48f,.6f)},Accent,2);
        for(int32 I=0;I<FMath::Clamp(Variant,1,4);++I)Box(.35f+I*.09f,.44f,.035f,.12f,Accent);
        Box(.09f,.44f,.1f,.12f,FLinearColor(.3f,.7f,1));
    }
    else if(Kind==6)
    {
        Line({P(.15f,.35f),P(.65f,.35f),P(.86f,.5f),P(.65f,.65f),P(.15f,.65f),P(.15f,.35f)},Accent,3);
        Box(.26f,.41f,.22f,.18f,Accent*.7f);
        Line({P(.03f,.5f),P(.13f,.5f)},FLinearColor(.9f,.55f,.2f),3);
    }
    else if(Kind==3)
    {
        for(int32 I=0;I<3;++I)
        {
            const float Y=.25f+I*.23f;
            Line({P(.15f,Y),P(.68f,Y),P(.86f,Y+.06f),P(.68f,Y+.12f),P(.15f,Y+.12f),P(.15f,Y)},Accent,2);
        }
    }
    else
    {
        Box(.14f,.61f,.45f,.12f,Accent*.7f);
        Line({P(.22f,.61f),P(.22f,.39f),P(.45f,.3f),P(.58f,.4f),P(.58f,.61f)},Accent,3);
        const int32 Barrels=Kind==4?3:1;
        for(int32 I=0;I<Barrels;++I)Line({P(.5f,.4f+I*.07f),P(.88f,.4f+I*.07f)},Accent,3);
        if(Kind==2)
        {
            Line({P(.78f,.27f),P(.89f,.4f),P(.78f,.53f)},FLinearColor(.5f,.95f,1),2);
            Line({P(.91f,.4f),P(.98f,.4f)},FLinearColor(.65f,1,1),3);
        }
    }
    return Layer+2;
}
