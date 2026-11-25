#include "OVSortieDesignWidget.h"
#include "OVTitleScreenWidget.h"
#include "OVController.h"
#include "OVCombatSubsystem.h"
#include "OVPlayScenarioSubsystem.h"
#include "OVShip.h"
#include "OVShipPreview.h"
#include "OVShipSystemsComponent.h"
#include "OVTypography.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Rendering/DrawElements.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
const FLinearColor Accent(.19f,.82f,.92f), Muted(.55f,.66f,.75f), Amber(1.f,.72f,.26f), Purple(.91f,.53f,1.f);
struct FWeaponArt { const TCHAR* Profile; const TCHAR* Label; const TCHAR* Thumbnail; };
const FWeaponArt WeaponArt[] = {
    {TEXT("D116_beam_"), TEXT("빔 포탑"), TEXT("laser_turret_lb30")},
    {TEXT("D116_heavy_beam_"), TEXT("중형 빔 포탑"), TEXT("plasma_heavy_cannon")},
    {TEXT("D116_cannon_"), TEXT("함포"), TEXT("naval_gun_bf120")},
    {TEXT("D116_heavy_cannon_"), TEXT("중포"), TEXT("heavy_cannon_shc400")},
    {TEXT("D116_missile_"), TEXT("미사일 모듈"), TEXT("missile_pod_mm24")},
    {TEXT("D116_guided_"), TEXT("유도 미사일 모듈"), TEXT("missile_pod_mm48")},
    {TEXT("D116_spinal_"), TEXT("거대 함포 · 전용 주포"), TEXT("railgun_rg200")},
    {TEXT("PD_Beam"), TEXT("빔 요격포"), TEXT("ciws_beam_b")},
    {TEXT("PD_Kinetic"), TEXT("기관 요격포"), TEXT("ciws_gatling_6")},
    {TEXT(""), TEXT("대함 미사일 모듈"), TEXT("anti_ship_module")}
};
constexpr int32 RoomsCount = int32(EOVFacilityType::Security)+1;
const TCHAR* RoomNames[RoomsCount] = {TEXT("빈 슬롯"),TEXT("코어"),TEXT("함교"),TEXT("생명 유지"),TEXT("엔진"),TEXT("무기 제어"),TEXT("탄약고"),TEXT("축전실"),TEXT("미사일 적재실"),TEXT("쉴드 제어"),TEXT("레이더"),TEXT("요격 제어"),TEXT("포드 발사실"),TEXT("격납고"),TEXT("함재기 관제"),TEXT("자원 저장소"),TEXT("보안실")};

    // Reserve a separate hit area for every visible marker. If projected centers coincide,
    // move the label to the nearest free cell and keep a leader to its physical socket.
    FBox2D PlaceSlotMarker(const TArray<FBox2D>& Boxes,FVector2D Size,FVector2D Anchor,FVector2D Half)
    {
        const auto Min=Half+FVector2D(2,2),Max=Size-Min;
        if(Min.X>Max.X || Min.Y>Max.Y)return FBox2D(ForceInit);
        auto fit=[&](FVector2D P){return FVector2D(FMath::Clamp(P.X,Min.X,Max.X),FMath::Clamp(P.Y,Min.Y,Max.Y));};
        auto area=[&](FVector2D P){return FBox2D(P-Half,P+Half);};
        auto vacant=[&](const FBox2D& R){
            return !Boxes.ContainsByPredicate([&](const FBox2D& Existing){return Existing.bIsValid && Existing.ExpandBy(2).Intersect(R);});
        };
        const auto Nearest=area(fit(Anchor));
        if(vacant(Nearest))return Nearest;
        const FVector2D Step=Half*2+FVector2D(6,6);
        double Best=TNumericLimits<double>::Max();
        FBox2D Result(ForceInit);
        for(double Y=Min.Y;Y<=Max.Y;Y+=Step.Y)for(double X=Min.X;X<=Max.X;X+=Step.X){
            const auto P=FVector2D(X,Y);
            const double D=FVector2D::DistSquared(P,Anchor);
            if(D<Best && vacant(area(P))){Best=D;Result=area(P);}
        }
        return Result.bIsValid?Result:Nearest;
    }

class SFacilityIllustration : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SFacilityIllustration) : _Kind(0) {} SLATE_ARGUMENT(int32,Kind) SLATE_END_ARGS()
    void Construct(const FArguments& Args){Kind=Args._Kind;}
    virtual FVector2D ComputeDesiredSize(float) const override { return {60,44}; }
    virtual int32 OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& List,int32 Layer,const FWidgetStyle&,bool) const override
    {
        const auto Size=G.GetLocalSize();
        const double Unit=FMath::Min(Size.X,Size.Y)/27.;
        const auto Origin=FVector2D(Size)*.5-FVector2D(12,12)*Unit;
        auto stroke=[&](std::initializer_list<FVector2D> Vertices){
            TArray<FVector2D> Points;
            for(const auto& P:Vertices)Points.Add(Origin+P*Unit);
            FSlateDrawElement::MakeLines(List,Layer,G.ToPaintGeometry(),Points,ESlateDrawEffect::None,Accent,true,1.8f);
        };
        auto ring=[&](FVector2D Center,float Radius){
            TArray<FVector2D> Points;
            for(int32 I=0;I<=24;++I){
                const double Angle=I*UE_TWO_PI/24;
                Points.Add(Origin+(Center+FVector2D(FMath::Cos(Angle),FMath::Sin(Angle))*Radius)*Unit);
            }
            FSlateDrawElement::MakeLines(List,Layer,G.ToPaintGeometry(),Points,ESlateDrawEffect::None,Accent,true,1.8f);
        };
        switch(static_cast<EOVFacilityType>(Kind)){
        case EOVFacilityType::Core:
            stroke({{12,2},{21,7},{21,17},{12,22},{3,17},{3,7},{12,2}});ring({12,12},4);break;
        case EOVFacilityType::Bridge:
            stroke({{2,6},{22,6},{19,13},{14,14},{14,19},{19,20},{5,20},{10,19},{10,14},{5,13},{2,6}});stroke({{6,9},{18,9}});break;
        case EOVFacilityType::LifeSupport:
            stroke({{1,14},{6,14},{9,5},{13,21},{16,10},{18,14},{23,14}});break;
        case EOVFacilityType::Engine:
            stroke({{6,3},{18,3},{18,12},{15,16},{9,16},{6,12},{6,3}});stroke({{9,18},{10,21},{12,19},{14,23},{16,18}});break;
        case EOVFacilityType::WeaponControl:
            ring({12,12},7);stroke({{12,1},{12,8}});stroke({{12,16},{12,23}});stroke({{1,12},{8,12}});stroke({{16,12},{23,12}});break;
        case EOVFacilityType::Ammo:
            for(int32 X:{5,12,19})stroke({{double(X-2),20},{double(X-2),9},{double(X),4},{double(X+2),9},{double(X+2),20},{double(X-2),20}});break;
        case EOVFacilityType::Capacitor:
            stroke({{14,2},{4,14},{11,14},{9,22},{20,10},{13,10},{14,2}});break;
        case EOVFacilityType::MissileStorage:
            stroke({{4,17},{15,6},{21,3},{19,10},{9,20},{4,17}});stroke({{8,12},{4,12},{3,16}});stroke({{13,17},{13,21},{9,22}});break;
        case EOVFacilityType::ShieldControl:
            stroke({{12,2},{21,6},{20,14},{17,19},{12,22},{7,19},{4,14},{3,6},{12,2}});stroke({{12,6},{12,18}});break;
        case EOVFacilityType::Radar:
            ring({12,12},9);ring({12,12},5);stroke({{12,12},{19,5}});break;
        case EOVFacilityType::Interception:
            stroke({{2,18},{7,12},{12,18}});stroke({{10,18},{15,12},{20,18}});stroke({{20,3},{15,8}});stroke({{14,4},{18,9}});break;
        case EOVFacilityType::Boarding:
            stroke({{4,19},{4,13},{14,3},{21,3},{21,10},{11,20},{4,19}});ring({15,9},2);stroke({{1,23},{5,20}});break;
        case EOVFacilityType::Hangar:
            stroke({{2,21},{2,9},{12,3},{22,9},{22,21},{2,21}});stroke({{6,21},{6,12},{18,12},{18,21}});break;
        case EOVFacilityType::FighterControl:
            stroke({{12,2},{15,11},{22,16},{21,18},{14,15},{12,21},{10,15},{3,18},{2,16},{9,11},{12,2}});break;
        case EOVFacilityType::ResourceStorage:
            stroke({{2,7},{12,2},{22,7},{22,17},{12,22},{2,17},{2,7},{12,12},{22,7}});stroke({{12,12},{12,22}});break;
        case EOVFacilityType::Security:
            stroke({{5,10},{19,10},{19,21},{5,21},{5,10}});stroke({{8,10},{8,6},{10,3},{14,3},{16,6},{16,10}});stroke({{12,14},{12,17}});break;
        default: break;
        }
        return Layer;
    }
private:
    int32 Kind=0;
};
}

const TCHAR* OVSortieParts::RoomName(int32 Index){return RoomNames[FMath::Clamp(Index,0,RoomsCount-1)];}
int32 OVSortieParts::WeaponIllustration(FName Profile,bool bAntiShip)
{
    if(bAntiShip)return UE_ARRAY_COUNT(WeaponArt)-1;
    const FString Name=Profile.ToString();
    for(int32 Index=0;Index<UE_ARRAY_COUNT(WeaponArt)-1;++Index)if(Name.StartsWith(WeaponArt[Index].Profile))return Index;
    return INDEX_NONE;
}
FString OVSortieParts::WeaponName(FName Profile,bool bAntiShip)
{
    if(bAntiShip && !Profile.IsNone()){
        return Profile.ToString().StartsWith(TEXT("D116_guided_"))?TEXT("대함 미사일 모듈 · 유도탄"):TEXT("대함 미사일 모듈 · 일반탄");
    }
    const int32 Art=WeaponIllustration(Profile,bAntiShip);
    return Art==INDEX_NONE?TEXT("빈 장착점"):WeaponArt[Art].Label;
}

UTexture* UOVTitleScreenWidget::SortiePartImage(int32 Model)
{
    if(const auto* Found=SortiePartImages.Find(Model))return Found->Get();
    if(Model<0 || Model>=UE_ARRAY_COUNT(WeaponArt))return nullptr;
    const TCHAR* Name=WeaponArt[Model].Thumbnail;
    const FString Path=FString::Printf(TEXT("/Game/UI/MissionCards/Weapons/%s.%s"),Name,Name);
    auto* Texture=LoadObject<UTexture2D>(nullptr,*Path);
    SortiePartImages.Add(Model,Texture);
    return Texture;
}

void UOVSortiePartCard::Configure(UOVTitleScreenWidget* Owner,UOVSortieDesignBoard* Board,FName Action,int32 Part,int32 Hull,const FString& Name,const FString& Detail,int32 Image)
{
    Menu=Owner; SourceBoard=Board; Command=Action; Value=Part; HullKey=Hull; Caption=Name; Description=Detail; Illustration=Image;
    SetVisibility(ESlateVisibility::Visible);
    SetToolTipText(FText::FromString(Caption+TEXT(" · 슬롯으로 끌어 놓기")));
}
TSharedRef<SWidget> UOVSortiePartCard::RebuildWidget()
{
    TSharedRef<SWidget> Art=SNew(SFacilityIllustration).Kind(Illustration);
    if(Command!="SetRoom" && Menu.IsValid())
    {
        if(auto* Texture=Menu->SortiePartImage(Illustration)){
            CardBrush.SetResourceObject(Texture);
            CardBrush.SetImageSize(FVector2f(66,43));
            Art=SNew(SImage).Image(&CardBrush);
        }
    }
    return SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(.035,.08,.12)).Padding(4)
    [
        SNew(SHorizontalBox)
        +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SBox).WidthOverride(66).HeightOverride(43)[Art]]
        +SHorizontalBox::Slot().FillWidth(1).Padding(7,1).VAlign(VAlign_Center)
        [
            SNew(SVerticalBox)
            +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(Caption)).Font(OVTypography::Style(EOVTextRole::Button,14)).AutoWrapText(true)]
            +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(Description)).Font(OVTypography::Style(EOVTextRole::Body,11)).ColorAndOpacity(Muted).AutoWrapText(true)]
        ]
    ];
}
FReply UOVSortiePartCard::NativeOnMouseButtonDown(const FGeometry&,const FPointerEvent& Event)
{
    if(Event.GetEffectingButton()!=EKeys::LeftMouseButton)return FReply::Unhandled();
    bPressed=true;
    return FReply::Handled().DetectDrag(TakeWidget(),EKeys::LeftMouseButton);
}
FReply UOVSortiePartCard::NativeOnMouseButtonUp(const FGeometry&,const FPointerEvent& Event)
{
    if(Event.GetEffectingButton()!=EKeys::LeftMouseButton || !bPressed)return FReply::Unhandled();
    bPressed=false;
    if(Menu.IsValid())Menu->SortieClick(Command,Value);
    return FReply::Handled();
}
void UOVSortiePartCard::NativeOnDragDetected(const FGeometry&,const FPointerEvent&,UDragDropOperation*& Result)
{
    bPressed=false;
    auto* Drag=NewObject<UOVSortiePartDrag>(this);
    Drag->SourceBoard=SourceBoard; Drag->Command=Command; Drag->Value=Value; Drag->HullKey=HullKey;
    auto* Visual=CreateWidget<UOVSortiePartCard>(GetOwningPlayer());
    Visual->Configure(Menu.Get(),SourceBoard.Get(),Command,Value,HullKey,Caption,Description,Illustration);
    Visual->SetVisibility(ESlateVisibility::HitTestInvisible);
    Drag->DefaultDragVisual=Visual;
    Drag->Pivot=EDragPivot::MouseDown;
    Result=Drag;
}

void UOVSortieDesignBoard::Configure(UOVTitleScreenWidget* Owner,AOVShip* Ship,UOVShipPreview* Preview)
{
    Menu=Owner; Subject=Ship; Mirror=Preview;
    auto* PC=Cast<AOVController>(GetOwningPlayer());
    Play=PC?PC->GetPlayScenario():nullptr;
    if(!Play.IsValid() || !Play->Draft.Hulls.IsValidIndex(Play->SortieSelected))return;
    Key=Play->Draft.Hulls[Play->SortieSelected].Key;
    bExternal=Play->SortieStage==2 && Play->bSortieExternal;
    if(Menu->SortieViewHull!=Key){
        Menu->SortieViewHull=Key;
        Menu->SortieViewRotation=UOVShipPreview::DefaultHullView();
        Menu->SortieViewZoom=1.12f;
    }
    SetVisibility(ESlateVisibility::Visible);
    SetClipping(EWidgetClipping::ClipToBounds);
    if(!bExternal || !PC->Combat || !PC->Combat->Settings || !Subject.IsValid() || !Subject->Systems->Layout)return;
    const auto& Layout=*Subject->Systems->Layout;
    Equipped.Reserve(Layout.WeaponMounts.Num());
    for(int32 I=0;I<Layout.WeaponMounts.Num();++I){
        const FName Choice=Play->SortieWeaponAt(I);
        if(Choice.IsNone())continue;
        auto Def=Layout.WeaponMounts[I];
        const int32 Art=OVSortieParts::WeaponIllustration(Choice,Def.IsAntiShipMissile());
        if(Art==INDEX_NONE)continue;
        if(const auto* Profile=PC->Combat->Settings->Weapons.FindByPredicate([&](const auto& W){return W.ProfileId==Choice;})){
            Def.Type=Profile->Family;
            Def.Tier=Profile->Tier;
        }
        const FName Visuals[]={TEXT("LB30Trial"),TEXT("PlasmaTrial"),TEXT("BF120SourceTrial"),TEXT("SHC400Trial"),TEXT("StandardMissilePod"),TEXT("GuidedMissilePod"),TEXT("RG200Trial"),TEXT("CIWSBTrial"),TEXT("CIWS6Trial"),TEXT("AntiShipMissileModule")};
        Def.PresentationProfile=Visuals[Art];
        Equipped.Add(Def);
    }
}
TSharedRef<SWidget> UOVSortieDesignBoard::RebuildWidget(){return SNew(SSpacer);}
void UOVSortieDesignBoard::NativeTick(const FGeometry& G,float Seconds)
{
    Super::NativeTick(G,Seconds);
    if(!bExternal || !Mirror.IsValid() || !Subject.IsValid() || !Menu.IsValid())return;
    const FVector2D Size=G.GetLocalSize();
    if(Size.X<1 || Size.Y<1 || (!bRefreshCapture && Size.Equals(LastCanvasSize, .5)))return;
    bRefreshCapture=false; bSlotGeometryDirty=true; LastCanvasSize=Size;
    Mirror->Update(Subject.Get(),Menu->SortieViewRotation,Menu->SortieViewZoom,Size.X/Size.Y,INDEX_NONE,FMath::Clamp(FMath::CeilToInt(Size.X*G.GetAccumulatedLayoutTransform().GetScale()),256,1536),false,&Equipped);
    PreviewBrush.SetResourceObject(Mirror->GetTexture());
}
const TArray<FBox2D>& UOVSortieDesignBoard::SlotGeometry(FVector2D Size) const
{
    if(bSlotGeometryDirty || !BoxCanvasSize.Equals(Size,.1)){
        CachedBoxes.Reset();
        GeometryFor(Size,CachedBoxes);
        bSlotGeometryDirty=false;
        BoxCanvasSize=Size;
    }
    return CachedBoxes;
}
void UOVSortieDesignBoard::GeometryFor(FVector2D Size,TArray<FBox2D>& Boxes) const
{
    if(!Subject.IsValid() || !Subject->Systems->Layout || !Play.IsValid())return;
    const auto& Layout=*Subject->Systems->Layout;
    if(bExternal)
    {
        if(!Mirror.IsValid() || !PreviewBrush.GetResourceObject())return;
        for(const auto& Mount:Layout.WeaponMounts){
            const FVector2D Anchor=Mirror->Project(Mount.LocalPosition)*Size;
            if(Anchor.X<0||Anchor.Y<0||Anchor.X>Size.X||Anchor.Y>Size.Y)Boxes.Emplace(ForceInit);
            else Boxes.Add(PlaceSlotMarker(Boxes,Size,Anchor,FVector2D(13,13)));
        }
        return;
    }
    FBox2D Bounds(ForceInit);
    Boxes.Init(FBox2D(ForceInit),Layout.Slots.Num());
    for(const auto& Room:Layout.Slots)if(Room.DeckId==Play->SortieDeck)Bounds+=FVector2D(Room.LocalCenter.X,Room.LocalCenter.Y);
    if(!Bounds.bIsValid)return;
    const FVector2D Mid=Bounds.GetCenter(),Extent=Bounds.GetExtent();
    TArray<FVector2D> Centers;
    TArray<int32> Indices;
    for(int32 Index=0;Index<Layout.Slots.Num();++Index){
        const auto& Room=Layout.Slots[Index];
        if(Room.DeckId!=Play->SortieDeck)continue;
        const auto N=(FVector2D(Room.LocalCenter.X,Room.LocalCenter.Y)-Mid)/FVector2D(FMath::Max(1.,Extent.X),FMath::Max(1.,Extent.Y));
        Centers.Add(Size*.5+N*FVector2D(FMath::Max(8.,Size.X*.5-48),FMath::Max(8.,Size.Y*.5-44)));
        Indices.Add(Index);
    }
    for(int32 I=0;I<Indices.Num();++I){
        FVector2D Half(43,16);
        for(int32 J=0;J<Indices.Num();++J)if(I!=J){
            const FVector2D Delta=(Centers[I]-Centers[J]).GetAbs();
            if(Delta.Y<Half.Y*2 && Delta.X>1)Half.X=FMath::Min(Half.X,Delta.X*.45);
        }
        Half.X=FMath::Clamp(Half.X,15.,FMath::Max(15.,Size.X*.16));
        Boxes[Indices[I]]=PlaceSlotMarker(Boxes,Size,Centers[I],Half);
    }
}
int32 UOVSortieDesignBoard::HitSlot(const FGeometry& G,FVector2D Screen)const
{
    const auto& Boxes=SlotGeometry(G.GetLocalSize());
    const FVector2D P=G.AbsoluteToLocal(Screen);
    double Best=TNumericLimits<double>::Max();
    int32 Pick=INDEX_NONE;
    for(int32 I=0;I<Boxes.Num();++I)if(Boxes[I].bIsValid && Boxes[I].IsInsideOrOn(P)){
        const double Distance=FVector2D::DistSquared(P,Boxes[I].GetCenter());
        if(Distance<Best){Best=Distance;Pick=I;}
    }
    return Pick;
}
int32 UOVSortieDesignBoard::NativePaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle&,bool)const
{
    if(!Play.IsValid() || !Subject.IsValid() || !Subject->Systems->Layout)return Layer;
    const auto& Layout=*Subject->Systems->Layout;
    const FVector2D Size=G.GetLocalSize();
    const auto* White=FCoreStyle::Get().GetBrush("WhiteBrush");
    auto Region=[&](FVector2D Pos,FVector2D Dimensions){return G.ToPaintGeometry(FVector2f(Dimensions),FSlateLayoutTransform(FVector2f(Pos)));};
    auto Box=[&](const FBox2D& R,int32 Z,FLinearColor C){FSlateDrawElement::MakeBox(Out,Z,Region(R.Min,R.GetSize()),White,ESlateDrawEffect::None,C);};
    auto Line=[&](TArray<FVector2D> Points,int32 Z,FLinearColor C,float Width){FSlateDrawElement::MakeLines(Out,Z,G.ToPaintGeometry(),Points,ESlateDrawEffect::None,C,true,Width);};
    auto Text=[&](FVector2D P,const FString& Caption,int32 FontSize,FLinearColor Color){
        FSlateDrawElement::MakeText(Out,Layer+5,Region(P,Size),Caption,OVTypography::Style(EOVTextRole::Button,FontSize),ESlateDrawEffect::None,Color);
    };
    Box(FBox2D(FVector2D::ZeroVector,Size),Layer,FLinearColor(.007,.017,.03));
    if(bExternal && PreviewBrush.GetResourceObject())
        FSlateDrawElement::MakeBox(Out,Layer+1,G.ToPaintGeometry(),&PreviewBrush);
    const auto& Boxes=SlotGeometry(Size);
    if(!bExternal){
        for(const auto& Edge:Layout.Connections){
            const int32 A=Layout.GetSlotIndex(Edge.FromSlot),B=Layout.GetSlotIndex(Edge.ToSlot);
            if(!Boxes.IsValidIndex(A)||!Boxes.IsValidIndex(B))continue;
            const bool ShowA=Boxes[A].bIsValid,ShowB=Boxes[B].bIsValid;
            if(ShowA && ShowB)Line({Boxes[A].GetCenter(),Boxes[B].GetCenter()},Layer+1,FLinearColor(.22,.40,.53),2);
            else if(ShowA || ShowB){
                const int32 Here=ShowA?A:B,There=ShowA?B:A;
                Text(FVector2D(Boxes[Here].Min.X,Boxes[Here].Max.Y+1),FString::Printf(TEXT("↕ %d층"),Layout.Slots[There].DeckId),9,Muted);
            }
        }
    }
    for(int32 I=0;I<Boxes.Num();++I){
        if(!Boxes[I].bIsValid)continue;
        const auto& R=Boxes[I];const auto P=R.GetCenter();
        const bool Selected=(bExternal?Play->SortieMount:Play->SortieRoom)==I;
        const bool Focus=I==HoverSlot;
        if(bExternal){
            const auto& Mount=Layout.WeaponMounts[I];
            const bool PD=Mount.Type==EOVWeaponType::Interception,Special=Mount.AuthoredSlotPurpose=="Dedicated"||!Mount.RequiredProfileId.IsNone();
            FLinearColor Color=Special?Purple:PD?Amber:Accent;
            const FVector2D Anchor=Mirror->Project(Mount.LocalPosition)*Size;
            if(FVector2D::DistSquared(P,Anchor)>9){
                Line({Anchor,P},Layer+1,Color,1.4f);
                Box(FBox2D(Anchor-FVector2D(2,2),Anchor+FVector2D(2,2)),Layer+2,Color);
            }
            const bool Empty=Play->SortieWeaponAt(I).IsNone();
            Box(R,Layer+2,Empty?FLinearColor(.02,.045,.07,.77):Color*.43f);
            TArray<FVector2D> Shape;
            if(Special)Shape={P+FVector2D(0,-14),P+FVector2D(14,0),P+FVector2D(0,14),P+FVector2D(-14,0),P+FVector2D(0,-14)};
            else if(PD)Shape={P+FVector2D(0,-14),P+FVector2D(14,12),P+FVector2D(-14,12),P+FVector2D(0,-14)};
            else Shape={R.Min,{R.Max.X,R.Min.Y},R.Max,{R.Min.X,R.Max.Y},R.Min};
            Line(Shape,Layer+3,Color,2);
            if(Selected||Focus){auto S=R.ExpandBy(4);Line({S.Min,{S.Max.X,S.Min.Y},S.Max,{S.Min.X,S.Max.Y},S.Min},Layer+4,FLinearColor::White,1.5);}
            Text(P-FVector2D(I<9?4:8,8),FString::FromInt(I+1),10,FLinearColor::White);
        }else{
            const int32 Room=int32(Play->SortieRoomAt(I));
            Box(R,Layer+2,Focus?Amber:Selected?Accent:Room?FLinearColor(.18,.40,.49):FLinearColor(.17,.27,.32));
            Box(R.ExpandBy(-1.5),Layer+3,FLinearColor(.025,.075,.1));
            const auto Measure=FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
            FString Name=OVSortieParts::RoomName(Room);
            int32 Font=13;
            auto Extent=FVector2D(Measure->Measure(Name,OVTypography::Style(EOVTextRole::Button,Font)));
            const double MaxWidth=R.GetSize().X-6;
            if(Extent.X>MaxWidth){
                Font=FMath::Clamp(FMath::FloorToInt(Font*MaxWidth/Extent.X),9,13);
                Extent=FVector2D(Measure->Measure(Name,OVTypography::Style(EOVTextRole::Button,Font)));
                if(Extent.X>MaxWidth){
                    Name=FString::FromInt(I+1);
                    Extent=FVector2D(Measure->Measure(Name,OVTypography::Style(EOVTextRole::Button,Font)));
                }
            }
            Text(P-Extent*.5,Name,Font,Room?FLinearColor::White:Muted);
        }
    }
    return Layer+5;
}
FReply UOVSortieDesignBoard::NativeOnMouseButtonDown(const FGeometry& G,const FPointerEvent& Event)
{
    if(bExternal && Event.GetEffectingButton()==EKeys::MiddleMouseButton)return FReply::Handled().CaptureMouse(TakeWidget());
    if(Event.GetEffectingButton()!=EKeys::LeftMouseButton || !Menu.IsValid())return FReply::Unhandled();
    const int32 Hit=HitSlot(G,Event.GetScreenSpacePosition());
    if(Hit!=INDEX_NONE)Menu->SortieClick(bExternal?"Slot":"Room",Hit);
    return FReply::Handled();
}
FReply UOVSortieDesignBoard::NativeOnMouseButtonUp(const FGeometry&,const FPointerEvent& Event)
{
    return Event.GetEffectingButton()==EKeys::MiddleMouseButton && HasMouseCapture()?FReply::Handled().ReleaseMouseCapture():FReply::Unhandled();
}
FReply UOVSortieDesignBoard::NativeOnMouseMove(const FGeometry&,const FPointerEvent& Event)
{
    if(!bExternal||!HasMouseCapture()||!Menu.IsValid())return FReply::Unhandled();
    const auto Delta=Event.GetCursorDelta();
    Menu->SortieViewRotation=(FRotator(Delta.Y*.35,0,0).Quaternion()*FRotator(0,Delta.X*.35,0).Quaternion()*Menu->SortieViewRotation.Quaternion()).Rotator();
    bRefreshCapture=true;
    return FReply::Handled();
}
FReply UOVSortieDesignBoard::NativeOnMouseWheel(const FGeometry&,const FPointerEvent& Event)
{
    if(!bExternal||!Menu.IsValid())return FReply::Unhandled();
    Menu->SortieViewZoom=FMath::Clamp(Menu->SortieViewZoom+Event.GetWheelDelta()*.1f,.55f,2.5f);
    bRefreshCapture=true;
    return FReply::Handled();
}
bool UOVSortieDesignBoard::NativeOnDragOver(const FGeometry& G,const FDragDropEvent& Event,UDragDropOperation* Op)
{
    const auto* Part=Cast<UOVSortiePartDrag>(Op);
    if(!Part||Part->SourceBoard.Get()!=this||Part->HullKey!=Key||!Play.IsValid()||Play->SortieStage!=2||bExternal==(Part->Command=="SetRoom"))return false;
    HoverSlot=HitSlot(G,Event.GetScreenSpacePosition());
    return true;
}
void UOVSortieDesignBoard::NativeOnDragLeave(const FDragDropEvent&,UDragDropOperation*){HoverSlot=INDEX_NONE;}
bool UOVSortieDesignBoard::NativeOnDrop(const FGeometry& G,const FDragDropEvent& Event,UDragDropOperation* Op)
{
    if(!NativeOnDragOver(G,Event,Op)||!Menu.IsValid())return false;
    const auto* Part=CastChecked<UOVSortiePartDrag>(Op);
    if(HoverSlot==INDEX_NONE){
        Play->SortieFeedback=bExternal?TEXT("선체 위의 장착점 표식에 무장을 놓으세요."):TEXT("도면에 표시된 방 안에 시설을 놓으세요.");
        Menu->RefreshSortie();
    }else{
        Play->SortieCommand(bExternal?"Slot":"Room",HoverSlot);
        Menu->SortieClick(Part->Command,Part->Value);
    }
    return true;
}


#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVSortieMarkerTest,"OrbitalVanguard.Sortie.DesignBoard.DenseMounts",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FOVSortieMarkerTest::RunTest(const FString&)
{
    // Includes a small, high-scale window. With an end-on view all 24 sockets can
    // project to almost the same location, including sockets at the edge of the panel.
    for(const FVector2D Size:{FVector2D(260,220),FVector2D(480,360),FVector2D(860,650)}){
        for(const FVector2D Anchor:{Size*.5,FVector2D(1,1),Size-FVector2D(1,1)}){
            TArray<FBox2D> Used;
            for(int32 I=0;I<24;++I){
                const FBox2D Marker=PlaceSlotMarker(Used,Size,Anchor,FVector2D(13,13));
                TestTrue(TEXT("Visible marker has a complete click area"),Marker.bIsValid && Marker.GetSize().Equals(FVector2D(26,26)));
                TestTrue(TEXT("Click area stays inside the view"),Marker.Min.X>=0 && Marker.Min.Y>=0 && Marker.Max.X<=Size.X && Marker.Max.Y<=Size.Y);
                TestFalse(TEXT("No two hit areas overlap"),Used.ContainsByPredicate([&](const auto& Other){return Other.Intersect(Marker);}));
                Used.Add(Marker);
            }
        }
    }
    return true;
}
#endif
