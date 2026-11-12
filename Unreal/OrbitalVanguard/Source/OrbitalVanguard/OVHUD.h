#pragma once
#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "OVHUD.generated.h"
UCLASS()
class ORBITALVANGUARD_API AOVHUD : public AHUD {
 GENERATED_BODY()
public:
 virtual void DrawHUD() override;
 virtual void NotifyHitBoxClick(FName BoxName) override;
 UFUNCTION(BlueprintPure, Category="UI") bool IsOverPanel(float X,float Y) const;
 bool HasOrbitPreview() const;
 bool IsOverOrbitPreview(FVector2D Cursor) const;
 void OrbitPreview(FVector2D Delta);
 void CancelPointerDrag() { bDragging=false; bResizing=false; }
 void OpenDetails() { bDetailsOpen=true; bMinimized=false; }
 UFUNCTION(BlueprintCallable, Category="UI") bool HandlePointer(float X,float Y,bool Pressed,bool Down,float Wheel,bool Orbit,float DX,float DY);
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="UI") bool bDetailsOpen=true;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="UI") bool bMinimized=false;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="UI") FVector2D WindowPosition=FVector2D(840,76);
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="UI") FVector2D WindowSize=FVector2D(424,550);
private:
 float UIScale=1.f;
 FVector2D ViewportPixels=FVector2D(1280,740);
 bool bDragging=false,bResizing=false;
 FVector2D DragOffset=FVector2D::ZeroVector;
 FRotator PreviewRotation=FRotator(-25,35,0);
 float PreviewZoom=1;
 void DrawDetails();
 void DrawCombat();
 void DrawExternal(float X,float Y,float W);
 void Button(FName Name,const FString& Label,float X,float Y,float W,bool Active=false);
};
