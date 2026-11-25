#pragma once
#include "Blueprint/UserWidget.h"
#include "Blueprint/DragDropOperation.h"
#include "OVShipLayout.h"
#include "OVSortieDesignWidget.generated.h"

class AOVShip;
class UOVPlayScenarioSubsystem;
class UOVShipPreview;
class UOVTitleScreenWidget;
class UOVSortieDesignBoard;

UCLASS()
class UOVSortiePartDrag : public UDragDropOperation
{
    GENERATED_BODY()
public:
    TWeakObjectPtr<UOVSortieDesignBoard> SourceBoard;
    FName Command;
    int32 Value = INDEX_NONE;
    int32 HullKey = INDEX_NONE;
};

UCLASS()
class UOVSortiePartCard : public UUserWidget
{
    GENERATED_BODY()
public:
    void Configure(UOVTitleScreenWidget* Owner, UOVSortieDesignBoard* Board, FName Action, int32 Part, int32 Hull, const FString& Name, const FString& Detail, int32 Image);
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry&, const FPointerEvent&) override;
    virtual FReply NativeOnMouseButtonUp(const FGeometry&, const FPointerEvent&) override;
    virtual void NativeOnDragDetected(const FGeometry&, const FPointerEvent&, UDragDropOperation*&) override;
private:
    TWeakObjectPtr<UOVTitleScreenWidget> Menu;
    TWeakObjectPtr<UOVSortieDesignBoard> SourceBoard;
    FSlateBrush CardBrush;
    FName Command;
    FString Caption, Description;
    int32 Value = INDEX_NONE, HullKey = INDEX_NONE, Illustration = 0;
    bool bPressed = false;
};

UCLASS()
class UOVSortieDesignBoard : public UUserWidget
{
    GENERATED_BODY()
public:
    void Configure(UOVTitleScreenWidget* Owner, AOVShip* Ship, UOVShipPreview* Preview);
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual int32 NativePaint(const FPaintArgs&, const FGeometry&, const FSlateRect&, FSlateWindowElementList&, int32, const FWidgetStyle&, bool) const override;
    virtual void NativeTick(const FGeometry&,float) override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry&, const FPointerEvent&) override;
    virtual FReply NativeOnMouseButtonUp(const FGeometry&, const FPointerEvent&) override;
    virtual FReply NativeOnMouseMove(const FGeometry&, const FPointerEvent&) override;
    virtual FReply NativeOnMouseWheel(const FGeometry&, const FPointerEvent&) override;
    virtual bool NativeOnDrop(const FGeometry&, const FDragDropEvent&, UDragDropOperation*) override;
    virtual bool NativeOnDragOver(const FGeometry&, const FDragDropEvent&, UDragDropOperation*) override;
    virtual void NativeOnDragLeave(const FDragDropEvent&, UDragDropOperation*) override;
private:
    TWeakObjectPtr<UOVTitleScreenWidget> Menu;
    TWeakObjectPtr<AOVShip> Subject;
    TWeakObjectPtr<UOVShipPreview> Mirror;
    TWeakObjectPtr<UOVPlayScenarioSubsystem> Play;
    int32 Key = INDEX_NONE, HoverSlot = INDEX_NONE;
    bool bExternal = false, bRefreshCapture = true;
    FVector2D LastCanvasSize = FVector2D::ZeroVector;
    FSlateBrush PreviewBrush;
    TArray<FOVWeaponMountDefinition> Equipped;
    mutable TArray<FBox2D> CachedBoxes;
    mutable FVector2D BoxCanvasSize = FVector2D::ZeroVector;
    mutable bool bSlotGeometryDirty = true;
    const TArray<FBox2D>& SlotGeometry(FVector2D Size) const;
    void GeometryFor(FVector2D Size, TArray<FBox2D>& Boxes) const;
    int32 HitSlot(const FGeometry&, FVector2D ScreenPoint) const;
};

namespace OVSortieParts
{
    const TCHAR* RoomName(int32 Facility);
    FString WeaponName(FName Profile, bool bAntiShip = false);
    int32 WeaponIllustration(FName Profile, bool bAntiShip = false);
}
