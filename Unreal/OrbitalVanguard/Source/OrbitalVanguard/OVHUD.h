#pragma once
#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "OVHUD.generated.h"
class AOVShip;
class UOVDevelopmentWidget;
class UOVHeroWidget;
class UOVShipPreview;
UCLASS()
class ORBITALVANGUARD_API AOVHUD : public AHUD {
 GENERATED_BODY()
public:
 virtual void BeginPlay() override;
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
 virtual void DrawHUD() override;
 void SyncAddedUI();
 bool IsOverAddedUI(float X,float Y) const;
 bool IsAddedUIEditing() const;
 UFUNCTION(BlueprintPure, Category="UI") UOVDevelopmentWidget* GetDevelopmentPanel() const { return DevelopmentWidget; }
 UFUNCTION(BlueprintPure, Category="UI") UOVHeroWidget* GetHeroPanel() const { return HeroWidget; }
 UFUNCTION(BlueprintPure, Category="UI") int32 GetPreviewMeshCount() const;
 UFUNCTION(BlueprintPure, Category="UI") FString GetSelectedWeaponSummary() const;
 UFUNCTION(BlueprintPure, Category="UI") int32 GetPinnedCount() const { return PinnedShips.Num(); }
 UFUNCTION(BlueprintCallable, Category="UI") void SetHeroPanelOpen(bool bOpen) { bHeroPanelOpen=bOpen; if(bOpen)bDetailsOpen=false; SyncAddedUI(); }
 UFUNCTION(BlueprintPure, Category="UI") bool IsHeroPanelOpen() const { return bHeroPanelOpen; }
 virtual void NotifyHitBoxClick(FName BoxName) override;
 UFUNCTION(BlueprintCallable, Category="UI") void ExecuteUIAction(FName Action) { NotifyHitBoxClick(Action); }
 UFUNCTION(BlueprintPure, Category="UI") bool IsOverPanel(float X,float Y) const;
 UFUNCTION(BlueprintPure, Category="UI") FName GetHitActionAtCanvasPoint(FVector2D Point) const;
 UFUNCTION(BlueprintPure, Category="UI") bool HasOrbitPreview() const;
 UFUNCTION(BlueprintPure, Category="UI") FRotator GetPreviewRotation() const { return PreviewRotation; }
 UFUNCTION(BlueprintPure, Category="UI") float GetPreviewZoom() const { return PreviewZoom; }
 bool IsOverOrbitPreview(FVector2D Cursor) const;
 void OrbitPreview(FVector2D Delta);
 void CancelPointerDrag() { bDragging=false; bResizing=false; }
 void OpenDetails() { bDetailsOpen=true; bMinimized=false; bHeroPanelOpen=false; SyncAddedUI(); }
 UFUNCTION(BlueprintCallable, Category="UI") bool HandlePointer(float X,float Y,bool Pressed,bool Down,float Wheel,bool Orbit,float DX,float DY);
 UFUNCTION(BlueprintCallable, Category="UI") void SetDetailsOpen(bool Open) { bDetailsOpen=Open; bMinimized=false; }
 UFUNCTION(BlueprintPure, Category="UI") FVector2D GetActionCenter(FName Action) const;
 UFUNCTION(BlueprintPure, Category="UI") bool IsOverWorldMarker(FVector2D Pixel) const;
 UFUNCTION(BlueprintCallable, Category="UI") bool HandleInteriorAction(FName Action);
 UFUNCTION(BlueprintPure, Category="UI") bool IsOverInteriorPanel(float X,float Y) const;
 UFUNCTION(BlueprintPure, Category="UI") bool IsOverMissionPanel(float X,float Y) const;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="UI") bool bInteriorOpen=false;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="UI") int32 InteriorDeck=1;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="UI") TArray<int32> InteriorSelectedUnits;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="UI") FName InteriorTargetSlot;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="UI") TWeakObjectPtr<AOVShip> InteriorShip;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="UI") FString InteriorFeedback;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="UI") bool bDetailsOpen=false;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="UI") bool bMinimized=false;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="UI") int32 SelectedAircraftId=0;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="UI") FVector2D WindowPosition=FVector2D(840,76);
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="UI") FVector2D WindowSize=FVector2D(424,550);
private:
 UPROPERTY() TObjectPtr<UOVDevelopmentWidget> DevelopmentWidget;
 UPROPERTY() TObjectPtr<UOVHeroWidget> HeroWidget;
 UPROPERTY() TObjectPtr<UOVShipPreview> ShipPreview;
 UPROPERTY() TArray<TObjectPtr<UOVShipPreview>> PinnedPreviews;
 UPROPERTY() TArray<TWeakObjectPtr<AOVShip>> PinnedShips;
 FBox2D DevelopmentRect, HeroRect;
 bool bHeroPanelOpen=false;
 bool bSceneMenuOpen=false;
 bool bHangarView=false;
 bool bAircraftGrid=true;
 bool bDetailsPositioned=false;
 int32 FleetPage=0;
 int32 AircraftPage=0;
 int32 AircraftFilter=0;
 TWeakObjectPtr<AOVShip> AircraftPageShip;
 FString LastNoticeSource;
 FString NoticeText;
 double NoticeExpires=0;
 void DrawPinned();
 static FString ReadinessKorean(const FString& Key);
 TMap<FName,FVector2D> ActionCenters;
 float UIScale=1.f;
 FVector2D ViewportPixels=FVector2D(1280,740);
 bool bDragging=false,bResizing=false;
 bool bDrawingDetails=false;
 bool bDrawingInterior=false;
 int32 InteriorUnitPage=0;
 FBox2D InteriorBounds() const;
 void DrawInterior();
 void DrawMission();
 FVector2D DragOffset=FVector2D::ZeroVector;
 FRotator PreviewRotation=FRotator(-25,35,0);
 float PreviewZoom=1;
 void DrawDetails();
 void DrawCombat();
 void DrawPlayHUD();
 void DrawWorldIndicators();
 void DrawPhase2C();
 void DrawExternal(float X,float Y,float W);
 void Button(FName Name,const FString& Label,float X,float Y,float W,bool Active=false);
};
