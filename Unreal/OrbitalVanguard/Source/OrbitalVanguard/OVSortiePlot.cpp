#include "OVSortiePlot.h"
#include "OVTitleScreenWidget.h"
#include "OVPlayScenarioSubsystem.h"
#include "OVCalibration.h"
#include "OVController.h"
#include "Widgets/Layout/SSpacer.h"
#include "Rendering/DrawElements.h"

namespace {
constexpr double Limit=OVCalibration::XYLimit;
FVector2D Project(FVector p,FVector2D size){return FVector2D(.5+p.X/(2*Limit),.5-(p.Y+.40*p.Z)/(2*Limit))*size;}
}
TSharedRef<SWidget> UOVSortiePlot::RebuildWidget(){return SNew(SSpacer);}
int32 UOVSortiePlot::NativePaint(const FPaintArgs&,const FGeometry& g,const FSlateRect&,FSlateWindowElementList& out,int32 layer,const FWidgetStyle&,bool)const{
 const auto size=g.GetLocalSize();const auto* pc=Cast<AOVController>(GetOwningPlayer());if(!pc)return layer;
 const auto* play=pc->GetPlayScenario();const auto cyan=FLinearColor(.13,.83,.95),red=FLinearColor(1,.35,.28);
 auto line=[&](TArray<FVector2D> points,FLinearColor color,float thick=1){FSlateDrawElement::MakeLines(out,layer+1,g.ToPaintGeometry(),points,ESlateDrawEffect::None,color,true,thick);};
 auto circle=[&](FVector c,double radius,FLinearColor color){TArray<FVector2D> path;for(int z=0;z<=48;++z){double a=z*UE_TWO_PI/48;path.Add(Project(c+FVector(FMath::Cos(a)*radius,FMath::Sin(a)*radius,0),size));}line(path,color);};
 for(int i=0;i<=8;++i){float k=i/8.f;line({{size.X*k,0},{size.X*k,size.Y}},i==4?cyan:FLinearColor(.1,.19,.27));line({{0,size.Y*k},{size.X,size.Y*k}},FLinearColor(.1,.19,.27));}
 if(play->Draft.Map==EOVSpaceHazardKind::BlackHole){circle({},100000,FLinearColor(.73,.45,.12));circle({},4500,red);}
 if(play->Draft.Map==EOVSpaceHazardKind::AsteroidField)for(auto a:pc->ActiveAsteroids)circle(FVector(a.X,a.Y,a.Z),a.W,FLinearColor(.75,.67,.5));
 for(int i=0;i<play->Draft.Hulls.Num();++i){auto& h=play->Draft.Hulls[i];auto p=Project(h.Position,size);auto base=Project(FVector(h.Position.X,h.Position.Y,0),size);line({base,p},FLinearColor(.35,.55,.75));bool on=i==play->SortieSelected;auto tint=on?FLinearColor::White:cyan;float s=on?6:4;line({p+FVector2D(s,0),p+FVector2D(-s,-s),p+FVector2D(-s,s),p+FVector2D(s,0)},tint,2);}
 return layer+2;
}
FReply UOVSortiePlot::NativeOnMouseButtonDown(const FGeometry& g,const FPointerEvent& ev){
 if(ev.GetEffectingButton()!=EKeys::LeftMouseButton)return FReply::Unhandled();
 auto* pc=Cast<AOVController>(GetOwningPlayer());if(!pc)return FReply::Handled();auto* play=pc->GetPlayScenario();
 if(!play->Draft.Hulls.IsValidIndex(play->SortieSelected))return FReply::Handled();
 auto& h=play->Draft.Hulls[play->SortieSelected];auto uv=g.AbsoluteToLocal(ev.GetScreenSpacePosition())/g.GetLocalSize();
 play->SetSortiePosition(FVector((uv.X-.5)*2*Limit,(.5-uv.Y)*2*Limit-h.Position.Z*.40,h.Position.Z),h.Yaw);
 if(Menu.IsValid())Menu->RefreshSortie();return FReply::Handled();
}
