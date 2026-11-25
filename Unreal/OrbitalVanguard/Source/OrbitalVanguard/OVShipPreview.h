#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "OVShipPreview.generated.h"
class AOVShip;
class UOVWeaponVisualComponent;
class USceneComponent;
struct FOVWeaponMountDefinition;
class USceneCaptureComponent2D;
class UStaticMeshComponent;
class UTextureRenderTarget2D;

/** A display-only mesh mirror. Never spawns a ship or owns combat components. */
UCLASS()
class ORBITALVANGUARD_API UOVShipPreview : public UObject
{
    GENERATED_BODY()
public:
    // Local +X is the bow. Present it toward the viewer and lower right, with the deck visible.
    static FRotator DefaultHullView() { return (FRotator(20,0,0).Quaternion() * FRotator(0,145,0).Quaternion()).Rotator(); }
    void Update(AOVShip* Ship, FRotator Rotation, float Zoom, float Aspect, int32 FocusMount=INDEX_NONE, int32 PixelWidth=800, bool bPortrait=false, const TArray<FOVWeaponMountDefinition>* DesignWeapons=nullptr);
    void Release();
    UTextureRenderTarget2D* GetTexture() const { return Texture; }
    FVector2D Project(FVector LocalPosition) const;
    int32 GetMeshCount() const { return Mirrors.Num(); }
    int32 GetVisibleMeshCount() const;
    int32 GetFocusMount() const { return LastFocusMount; }
private:
    UPROPERTY() TObjectPtr<AActor> Proxy;
    UPROPERTY() TObjectPtr<USceneComponent> DesignRoot;
    UPROPERTY() TArray<TObjectPtr<UOVWeaponVisualComponent>> EquippedModels;

    UPROPERTY() TObjectPtr<USceneCaptureComponent2D> Capture;
    UPROPERTY() TObjectPtr<UTextureRenderTarget2D> Texture;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Mirrors;
    TArray<TWeakObjectPtr<UStaticMeshComponent>> Sources;
    TWeakObjectPtr<AOVShip> Subject;
    FRotator ViewRotation;
    float OrthoWidth=1000, ViewAspect=2, LastZoom=0;
    double LastCapture=-1;
    int32 LastFocusMount=INDEX_NONE;
};
