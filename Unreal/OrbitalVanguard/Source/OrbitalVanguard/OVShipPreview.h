#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "OVShipPreview.generated.h"
class AOVShip;
class USceneCaptureComponent2D;
class UStaticMeshComponent;
class UTextureRenderTarget2D;

/** A display-only mesh mirror. Never spawns a ship or owns combat components. */
UCLASS()
class ORBITALVANGUARD_API UOVShipPreview : public UObject
{
    GENERATED_BODY()
public:
    void Update(AOVShip* Ship, FRotator Rotation, float Zoom, float Aspect);
    void Release();
    UTextureRenderTarget2D* GetTexture() const { return Texture; }
    FVector2D Project(FVector LocalPosition) const;
    int32 GetMeshCount() const { return Mirrors.Num(); }
private:
    UPROPERTY() TObjectPtr<AActor> Proxy;
    UPROPERTY() TObjectPtr<USceneCaptureComponent2D> Capture;
    UPROPERTY() TObjectPtr<UTextureRenderTarget2D> Texture;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Mirrors;
    TArray<TWeakObjectPtr<UStaticMeshComponent>> Sources;
    TWeakObjectPtr<AOVShip> Subject;
    FRotator ViewRotation;
    float OrthoWidth=1000, ViewAspect=2;
    double LastCapture=-1;
};
