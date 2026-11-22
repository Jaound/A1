#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "OVTitleBackdropDataAsset.h"
#include "OVTitleBackdropStage.generated.h"

class ACameraActor;
class UDirectionalLightComponent;
class USkyLightComponent;
class UStaticMeshComponent;
class UTexture;
class UTexture2D;

/**
 * Display-only 3D ship backdrop stage for the Start Screen.
 * Uses a real viewport ACameraActor directly framing:
 *  - SpaceSkybox (SM_Source)
 *  - Distant Planet (M_Planet sphere arcing in the lower-right)
 *  - High-poly in-game SM_Hull ship formations at 3/4 oblique angle
 *  - Cool white key light, cyan rim light, sky light, and ACES/Bloom post-processing.
 */
UCLASS(BlueprintType)
class ORBITALVANGUARD_API UOVTitleBackdropStage : public UObject
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "UI|TitleBackdrop")
    void Activate(UWorld* World, FName ScenarioOrFormationId, EOVTitleBackdropMode Mode = EOVTitleBackdropMode::LiveProxyStage);

    UFUNCTION(BlueprintCallable, Category = "UI|TitleBackdrop")
    void SetFormation(FName ScenarioOrFormationId);

    UFUNCTION(BlueprintCallable, Category = "UI|TitleBackdrop")
    void SetMode(EOVTitleBackdropMode Mode);

    UFUNCTION(BlueprintCallable, Category = "UI|TitleBackdrop")
    void StepIdleMotion(float DeltaSeconds);

    UFUNCTION(BlueprintCallable, Category = "UI|TitleBackdrop")
    void Release();

    /** Returns pre-rendered UTexture2D (Way B) or nullptr (Way A live viewport). */
    UFUNCTION(BlueprintPure, Category = "UI|TitleBackdrop")
    UTexture* GetActiveTexture() const;

    UFUNCTION(BlueprintPure, Category = "UI|TitleBackdrop")
    EOVTitleBackdropMode GetActiveMode() const { return ActiveMode; }

    UFUNCTION(BlueprintPure, Category = "UI|TitleBackdrop")
    FName GetActiveFormationId() const { return ActiveFormationId; }

    UFUNCTION(BlueprintPure, Category = "UI|TitleBackdrop")
    int32 GetLoadedShipMeshCount() const { return ShipMeshComponents.Num(); }

    UFUNCTION(BlueprintPure, Category = "UI|TitleBackdrop")
    FString GetActiveFormationLabel() const { return ActiveFormationLabel; }

    UFUNCTION(BlueprintPure, Category = "UI|TitleBackdrop")
    ACameraActor* GetCameraActor() const { return TitleCameraActor; }

private:
    void EnsureFormationsLoaded();
    void RebuildLiveProxyStage(const FOVTitleBackdropFormation& Formation);

    UPROPERTY()
    TWeakObjectPtr<UWorld> CachedWorld;

    UPROPERTY()
    TObjectPtr<UOVTitleBackdropDataAsset> BackdropAsset;

    UPROPERTY()
    TArray<FOVTitleBackdropFormation> CachedFormations;

    UPROPERTY()
    TObjectPtr<AActor> StageActor;

    UPROPERTY()
    TObjectPtr<ACameraActor> TitleCameraActor;

    UPROPERTY()
    TObjectPtr<UTexture2D> PreRenderedTexture;

    UPROPERTY()
    TArray<TObjectPtr<UStaticMeshComponent>> ShipMeshComponents;

    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> SkyboxComponent;

    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> NebulaComponent;

    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> PlanetMeshComponent;

    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> CloudShadowsMeshComponent;

    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> CloudsMeshComponent;

    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> AtmosphereMeshComponent;

    UPROPERTY()
    TObjectPtr<UDirectionalLightComponent> KeyLightComponent;

    UPROPERTY()
    TObjectPtr<UDirectionalLightComponent> RimLightComponent;

    UPROPERTY()
    TObjectPtr<UDirectionalLightComponent> BounceLightComponent;

    UPROPERTY()
    TObjectPtr<USkyLightComponent> SkyLightComponent;

    EOVTitleBackdropMode ActiveMode = EOVTitleBackdropMode::LiveProxyStage;
    FName ActiveFormationId = NAME_None;
    FString ActiveFormationLabel;
    FVector BaseCameraLocation = FVector(-3400.f, -1500.f, 760.f);
    FRotator BaseCameraRotation = FRotator(-10.f, 21.f, 0.f);
    float IdleElapsedSeconds = 0.f;
};
