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
 bool IsOverPanel(float X,float Y) const;
private:
 float UIScale=1.f;
 void Button(FName Name,const FString& Label,float X,float Y,float W,bool Active=false);
};
