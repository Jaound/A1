#pragma once
#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "OVTypography.h"
#include "OVHUD.generated.h"
class AOVShip;
struct FOVCombatMountState;
struct FOVWeaponMountDefinition;
class UOVDevelopmentWidget;
class UOVHeroWidget;
class UOVTitleScreenWidget;
class UOVShipPreview;
class UOVBattleNotices;
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
 bool CloseTopModal();
 bool PickingShip() const;
 UFUNCTION(BlueprintCallable, Category="UI") bool AcceptShipPick(AOVShip* Ship);
 bool HandleShipOperations(FName Action);
 void DrawShipOperations();
 bool DrawSmallBays(AOVShip* Home,float X,float Y,float W);
 int32 PendingBay=0;
 uint64 PickerOpenedFrame=MAX_uint64;
 bool bDrawingOperationsModal=false;
 int32 BayPage=0;
 TWeakObjectPtr<AOVShip> BayPageHost;
 bool bChoosingTransport=false,bChoosingTransfer=false,bAircraftOnly=false;
 TWeakObjectPtr<AOVShip> BoardingOrigin,BoardingShuttle;
 TArray<int32> BoardingSelection;

 UFUNCTION(BlueprintPure, Category="UI") UOVDevelopmentWidget* GetDevelopmentPanel() const { return DevelopmentWidget; }
 UFUNCTION(BlueprintPure, Category="UI") UOVHeroWidget* GetHeroPanel() const { return HeroWidget; }
 UFUNCTION(BlueprintPure, Category="UI") UOVTitleScreenWidget* GetTitleScreen() const { return TitleScreenWidget; }
 UFUNCTION(BlueprintPure, Category="UI") bool IsTitleScreenOpen() const { return bTitleScreenOpen; }
 UFUNCTION(BlueprintCallable, Category="UI") void OpenTitleScreen();
 UFUNCTION(BlueprintCallable, Category="UI") void OpenTutorialGuide();
 UFUNCTION(BlueprintCallable, Category="UI") void CloseTitleScreen();
 UFUNCTION(BlueprintCallable, Category="UI") void SetUserUIScaleMultiplier(float NewScale, bool bPersist = true);
 UFUNCTION(BlueprintPure, Category="UI") float GetUserUIScaleMultiplier() const { return UserUIScaleMultiplier; }
 UFUNCTION(BlueprintPure, Category="UI") int32 GetPreviewMeshCount() const;
 UFUNCTION(BlueprintPure, Category="UI") int32 GetWeaponPreviewVisibleMeshCount() const;
 UFUNCTION(BlueprintPure, Category="UI") int32 GetWeaponPreviewFocus() const;
 UFUNCTION(BlueprintPure, Category="UI") float GetSelectedWeaponHealthRatio() const;
 UFUNCTION(BlueprintPure, Category="UI") FString GetSelectedWeaponSummary() const;
 UFUNCTION(BlueprintPure, Category="UI") int32 GetPinnedCount() const { return PinnedShips.Num(); }
 UFUNCTION(BlueprintPure, Category="UI") AOVShip* GetPinnedShip(int32 Index) const { return PinnedShips.IsValidIndex(Index)?PinnedShips[Index].Get():nullptr; }
 UFUNCTION(BlueprintPure, Category="UI") int32 GetPinnedDeck(int32 Index) const { return PinnedDecks.IsValidIndex(Index)?PinnedDecks[Index]:INDEX_NONE; }
 UFUNCTION(BlueprintPure, Category="UI") int32 GetPinnedSlot(int32 Index) const { return PinnedSlots.IsValidIndex(Index)?PinnedSlots[Index]:INDEX_NONE; }
 UFUNCTION(BlueprintPure, Category="UI") int32 GetPinnedWeapon(int32 Index) const { return PinnedWeapons.IsValidIndex(Index)?PinnedWeapons[Index]:INDEX_NONE; }
 UFUNCTION(BlueprintPure, Category="UI") FVector2D GetPinnedPosition(int32 Index) const;
 UFUNCTION(BlueprintPure, Category="UI") FVector2D GetPinnedSize(int32 Index) const;
 UFUNCTION(BlueprintPure, Category="UI") FString GetPinnedTitleTooltip(FVector2D CanvasPoint) const;
 UFUNCTION(BlueprintPure, Category="UI") FString GetPinnedFacilityTooltip(FVector2D CanvasPoint) const;
 UFUNCTION(BlueprintPure, Category="UI") FString GetFleetTitleTooltip(FVector2D CanvasPoint) const;
 UFUNCTION(BlueprintPure, Category="UI") FString GetSelectedCargoLabel() const;
 UFUNCTION(BlueprintPure, Category="UI") FString GetCommandTitleTooltip(FVector2D CanvasPoint) const;
 UFUNCTION(BlueprintPure, Category="UI") FString GetDetailsTitleTooltip(FVector2D CanvasPoint) const;
 UFUNCTION(BlueprintPure, Category="UI") FString GetPinnedCarrierSummary(int32 Index) const;
 UFUNCTION(BlueprintPure, Category="UI") int32 GetPinnedAircraftId(int32 Index) const { return PinnedAircraftIds.IsValidIndex(Index)?PinnedAircraftIds[Index]:0; }
 UFUNCTION(BlueprintPure, Category="UI") int32 GetPinnedAircraftPage(int32 Index) const { return PinnedAircraftPages.IsValidIndex(Index)?PinnedAircraftPages[Index]:0; }
 UFUNCTION(BlueprintPure, Category="UI") FString GetPinnedAircraftSummary(int32 Index) const;
 UFUNCTION(BlueprintPure, Category="UI") int32 GetDetailsSlotPage() const { return DetailsSlotPage; }
 UFUNCTION(BlueprintPure, Category="UI") int32 GetWarningLevel() const { return WarningLevel; }
 UFUNCTION(BlueprintPure, Category="UI") FString GetPinnedWeaponSummary(int32 Index) const;
 UFUNCTION(BlueprintPure, Category="UI") int32 GetPinnedPreviewFocus(int32 Index) const;
 UFUNCTION(BlueprintCallable, Category="UI") void SetHeroPanelOpen(bool bOpen) { bHeroPanelOpen=bOpen; if(bOpen)bDetailsOpen=false; SyncAddedUI(); }
 UFUNCTION(BlueprintPure, Category="UI") bool IsHeroPanelOpen() const { return bHeroPanelOpen; }
 virtual void NotifyHitBoxClick(FName BoxName) override;
 UFUNCTION(BlueprintCallable, Category="UI") void ExecuteUIAction(FName Action) { NotifyHitBoxClick(Action); }
 UFUNCTION(BlueprintPure, Category="UI") bool IsOverPanel(float X,float Y) const;
 bool bPauseMenu=false;
 void TogglePauseMenu();
 bool AllowsSandboxPlacement() const { return !bSceneMenuOpen && !bTitleScreenOpen && !bStarMapOpen && !IsAddedUIEditing(); }
 UFUNCTION(BlueprintPure, Category="UI") FName GetHitActionAtCanvasPoint(FVector2D Point) const;
 UFUNCTION(BlueprintPure, Category="UI") bool HasOrbitPreview() const;
 UFUNCTION(BlueprintPure, Category="UI") FRotator GetPreviewRotation() const { return PreviewRotation; }
 UFUNCTION(BlueprintPure, Category="UI") float GetPreviewZoom() const { return PreviewZoom; }
 bool IsOverOrbitPreview(FVector2D Cursor) const;
 void OrbitPreview(FVector2D Delta);
 UFUNCTION(BlueprintCallable, Category="UI") void CancelPointerDrag() { bInteriorScrolling=false; bDragging=false; bResizing=false; bInteriorDragging=false; bInteriorResizing=false; bDevDragging=false; bDevResizing=false; DraggedPin=INDEX_NONE; bPinResizing=false; }
 void OpenSurvey(){bHangarView=false;OpenDetails();}
 void OpenDetails() { bInteriorOpen=false;bDetailsOpen=true; bMinimized=false; bHeroPanelOpen=false; SyncAddedUI(); }
 UFUNCTION(BlueprintCallable, Category="UI") bool HandlePointer(float X,float Y,bool Pressed,bool Down,float Wheel,bool Orbit,float DX,float DY);
 UFUNCTION(BlueprintCallable, Category="UI") void SetDetailsOpen(bool Open) { bDetailsOpen=Open; bMinimized=false; }
 UFUNCTION(BlueprintPure, Category="UI") FVector2D GetActionCenter(FName Action) const;
 UFUNCTION(BlueprintPure, Category="UI") FString GetFleetDisplayLabel(AOVShip* Ship) const;
 UFUNCTION(BlueprintPure, Category="UI") bool IsFleetAirOverview() const { return bFleetAirOverview; }
 UFUNCTION(BlueprintPure, Category="UI") bool IsBattleFeedOpen() const { return bBattleFeedOpen; }
 UFUNCTION(BlueprintPure, Category="UI") int32 GetBattleFeedPage() const { return BattleFeedPage; }
 UFUNCTION(BlueprintPure, Category="UI") int32 GetBattleFeedFilter() const { return BattleFeedFilter; }
 UFUNCTION(BlueprintPure, Category="UI") UOVBattleNotices* GetBattleNoticeFeed() const;
 UFUNCTION(BlueprintPure, Category="UI") bool IsOverWorldMarker(FVector2D Pixel) const;
 FName WorldMarkerAt(FVector2D Pixel) const;
 UFUNCTION(BlueprintCallable, Category="UI") bool HandleInteriorAction(FName Action);
 UFUNCTION(BlueprintPure, Category="UI") bool IsOverInteriorPanel(float X,float Y) const;
 UFUNCTION(BlueprintPure, Category="UI") FVector2D GetInteriorWindowPosition() const { return InteriorBounds().Min; }
 UFUNCTION(BlueprintPure, Category="UI") FVector2D GetInteriorWindowSize() const { return InteriorBounds().GetSize(); }
 UFUNCTION(BlueprintPure, Category="UI") FVector2D GetDevelopmentWindowPosition() const { return DevelopmentRect.Min; }
 UFUNCTION(BlueprintPure, Category="UI") FVector2D GetDevelopmentWindowSize() const { return DevelopmentRect.GetSize(); }
 UFUNCTION(BlueprintCallable, Category="UI") void ToggleDevelopmentMinimized();
 UFUNCTION(BlueprintCallable, Category="UI") void ToggleDevelopmentMaximized();
 UFUNCTION(BlueprintCallable, Category="UI") void ResetDevelopmentWindow();
 void ResetPlaySessionUI();
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="UI") bool bDevelopmentMinimized=false;
 void SetDevelopmentWindowGeometry(FVector2D Position,FVector2D Size) { bDevelopmentMaximized=false;DevelopmentPosition=Position;if(!bDevelopmentMinimized)DevelopmentSize=Size;SyncAddedUI(); }
 UFUNCTION(BlueprintPure, Category="UI") bool IsOverMissionPanel(float X,float Y) const;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="UI") bool bInteriorOpen=false;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="UI") bool bInteriorMinimized=false;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="UI") int32 InteriorDeck=1;
 float InteriorMapScale=1;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="UI") TArray<int32> InteriorSelectedUnits;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="UI") FName InteriorTargetSlot;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="UI") TWeakObjectPtr<AOVShip> InteriorShip;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="UI") FString InteriorFeedback;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="UI") bool bDetailsOpen=false;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="UI") bool bMinimized=false;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="UI") bool bStarMapOpen=false;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="UI") FName SelectedStarSystemId=TEXT("home_vanguard");
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="UI") int32 SelectedAircraftId=0;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="UI") FVector2D WindowPosition=FVector2D(840,76);
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="UI") FVector2D WindowSize=FVector2D(424,550);
 bool IsOverStarMapPanel(float X,float Y) const;
private:
 // Rasterize Canvas labels at the display size while preserving logical HUD coordinates.
 FString FitLabel(UFont* Font,const FString& Text,float MaxWidth) const;
 void DrawNumberRight(const FString& Number,FLinearColor Color,float Right,float Top,float Scale=1.f);
 void DrawText(const FString& Text, FLinearColor Color, float X, float Y, UFont* Font, float Scale=1.f);
 UPROPERTY() TObjectPtr<UOVDevelopmentWidget> DevelopmentWidget;
 UPROPERTY() TObjectPtr<UOVHeroWidget> HeroWidget;
 UPROPERTY() TObjectPtr<UOVTitleScreenWidget> TitleScreenWidget;
 bool bTitleScreenOpen=false;
 UPROPERTY() TObjectPtr<UOVShipPreview> ShipPreview;
 UPROPERTY() TObjectPtr<UOVShipPreview> WeaponPreview;
 UPROPERTY() TArray<TObjectPtr<UOVShipPreview>> PinnedPreviews;
 UPROPERTY() TArray<TWeakObjectPtr<AOVShip>> PinnedShips;
 TArray<int32> PinnedDecks, PinnedSlots, PinnedWeapons, PinnedSlotPages, PinnedAircraftIds, PinnedAircraftPages;
 TArray<FVector2D> PinnedPositions;
 TArray<FVector2D> PinnedSizes;
 int32 DraggedPin=INDEX_NONE;
 bool bPinResizing=false;
 FVector2D PinDragOffset=FVector2D::ZeroVector;
 FBox2D DevelopmentRect, HeroRect;
 bool bHeroPanelOpen=false;
 bool bCommandShipClipped=false,bCommandTargetClipped=false;
 bool bSceneMenuOpen=false;
 bool bChoosingJump=false;
 bool bNoticeSettings=false;
 bool PausedBeforeMenu=false;
 int32 NoticeMask=0x3fffe & ~(1<<11);
 UPROPERTY(Transient) TMap<FName,TObjectPtr<UTextureRenderTarget2D>> SelectionPortraits;
 bool bHangarView=false;
 int32 DetailsSlotPage=0,DetailsPageDeck=INDEX_NONE;
 TWeakObjectPtr<AOVShip> DetailsPageShip;
 bool bAircraftGrid=true;
 bool bFleetAirOverview=false;
 bool bBattleFeedOpen=false;
 int32 BattleFeedPage=0;
 int32 BattleFeedFilter=0;
 int32 WarningLevel=2;
 bool bDetailsPositioned=false;
 int32 FleetPage=0;
 int32 AircraftPage=0;
 int32 AircraftFilter=0;
 TWeakObjectPtr<AOVShip> AircraftPageShip;
 FString LastNoticeSource;
 FString NoticeText;
 double NoticeExpires=0;
 void DrawPinned();
 void DrawBattleNotices();
 int32 VisibleBattleNoticeCount() const;
 static const TCHAR* AttackWeaponName(const FOVCombatMountState& Mount,const FOVWeaponMountDefinition* Def);
 static FString ReadinessKorean(const FString& Key);
 TMap<FName,FVector2D> ActionCenters;
 float UIScale=1.f;
 float UserUIScaleMultiplier=1.f;
 FVector2D ViewportPixels=FVector2D(1280,740);
 bool bDragging=false,bResizing=false;
 bool bInteriorDragging=false,bInteriorResizing=false;
 bool bDevDragging=false,bDevResizing=false;
 FVector2D DevelopmentPosition=FVector2D(-1,-1),DevelopmentSize=FVector2D(-1,-1),DevelopmentDragOffset=FVector2D::ZeroVector;
 bool bDevelopmentMaximized=false;
 FVector2D DevelopmentRestorePosition=FVector2D(-1,-1),DevelopmentRestoreSize=FVector2D(-1,-1);
 FVector2D InteriorWindowPosition=FVector2D(-1,-1),InteriorWindowSize=FVector2D(780,550),InteriorDragOffset=FVector2D::ZeroVector;
 bool bInteriorMaximized=false;
 FVector2D InteriorRestorePosition=FVector2D(-1,-1),InteriorRestoreSize=FVector2D(780,550);
 bool bDrawingDetails=false;
 bool bDrawingInterior=false;
 // The two card rows share one horizontally scrollable column index.
 int32 InteriorCardColumn=0, InteriorLastColumn=0, InteriorTeamFilter=INDEX_NONE;
 FBox2D InteriorScrollTrack=FBox2D(ForceInit);
 float InteriorThumbWidth=0, InteriorThumbGrab=0;
 bool bInteriorScrolling=false;
 FBox2D InteriorBounds() const;
 void DrawInterior();
 void DrawMission();
 uint64 LastMapCloseFrame=MAX_uint64;
 bool bDrawingStarMap=false;
 void DrawStarMap();
 FVector2D DragOffset=FVector2D::ZeroVector;
 FRotator PreviewRotation=FRotator(-25,35,0);
 float PreviewZoom=1;
 void DrawDetails();
 void DrawFacilityAlerts();
 bool ConfirmFacilityAlert(FName Action);
 TArray<int32,TInlineAllocator<64>> BattleNoticeIndices() const;
 float BattleNoticeTop() const;
 int32 BattleNoticeRows() const;
 float CommandPanelHeight() const;
 FString GetWeaponSummary(AOVShip* Ship,int32 Weapon) const;
 float PinnedPanelX() const;
 float PinnedPanelY() const;
 void DrawCombat();
 void DrawPlayHUD();
 bool IsDebriefOpen() const;
 void DrawDebrief();
 bool bDebriefDismissed=false;
 bool bConsumeDebriefClick=false;
 void OpenMissionCards(FName Selected, bool bBrowse=false);
 TOptional<bool> PauseBeforeCards;
 void DrawWorldIndicators();
 void DrawPhase2C();
 static float FleetPanelWidth(float ScreenWidth) { return FMath::Clamp(ScreenWidth-640.f,210.f,450.f); }
 void DrawExternal(float X,float Y,float W);
 void Button(FName Name,const FString& Label,float X,float Y,float W,bool Active=false,EOVTextRole TextRole=EOVTextRole::Button);
};
