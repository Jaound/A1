#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "OVCatalogCardWidget.generated.h"

/** Read-only schematic artwork. The icon does not create simulation actors or assets. */
UCLASS()
class ORBITALVANGUARD_API UOVCatalogIcon : public UUserWidget
{
    GENERATED_BODY()
public:
    int32 Kind = 0; // 0 hull, 1 gun, 2 beam, 3 missile, 4 PD, 5 fighter, 6 pod.
    int32 Variant = 1;
    FLinearColor Accent = FLinearColor(.23f,.74f,.94f);
protected:
    virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& Geometry,
        const FSlateRect& CullingRect, FSlateWindowElementList& Elements, int32 Layer,
        const FWidgetStyle& Style, bool bParentEnabled) const override;
};
