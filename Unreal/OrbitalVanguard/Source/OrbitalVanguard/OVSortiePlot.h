#pragma once
#include "Blueprint/UserWidget.h"
#include "OVSortiePlot.generated.h"
class UOVTitleScreenWidget;
UCLASS()
class ORBITALVANGUARD_API UOVSortiePlot : public UUserWidget
{
 GENERATED_BODY()
public:
 TWeakObjectPtr<UOVTitleScreenWidget> Menu;
protected:
 virtual TSharedRef<SWidget> RebuildWidget() override;
 virtual int32 NativePaint(const FPaintArgs&,const FGeometry&,const FSlateRect&,FSlateWindowElementList&,int32,const FWidgetStyle&,bool) const override;
 virtual FReply NativeOnMouseButtonDown(const FGeometry&,const FPointerEvent&) override;
};
