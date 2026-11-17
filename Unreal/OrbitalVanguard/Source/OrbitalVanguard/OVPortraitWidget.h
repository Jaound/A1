#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "OVPortraitWidget.generated.h"

/** Lightweight temporary character artwork. It only paints; it owns no hero state. */
UCLASS()
class ORBITALVANGUARD_API UOVPortraitWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    UPROPERTY(BlueprintReadOnly, Category="UI") int32 RoleIndex = 1;
    UPROPERTY(BlueprintReadOnly, Category="UI") bool bSelected = false;
    UPROPERTY(BlueprintReadOnly, Category="UI") bool bLost = false;
    UFUNCTION(BlueprintCallable, Category="UI") void Configure(int32 InRole, bool bInSelected = false, bool bInLost = false);
protected:
    virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& CullingRect,
        FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
};
