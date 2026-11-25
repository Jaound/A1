#include "OVTitleBackdropStage.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace
{
constexpr double StageIsolationZ = -12000000.0;
const TCHAR* BackdropAssetPath = TEXT("/Game/UI/Title/DA_OVTitleBackdrop.DA_OVTitleBackdrop");
}

void UOVTitleBackdropStage::EnsureFormationsLoaded()
{
    if (!BackdropAsset)
    {
        BackdropAsset = LoadObject<UOVTitleBackdropDataAsset>(nullptr, BackdropAssetPath);
    }

    if (CachedFormations.IsEmpty())
    {
        if (BackdropAsset && !BackdropAsset->Formations.IsEmpty())
        {
            CachedFormations = BackdropAsset->Formations;
        }
        else
        {
            CachedFormations = UOVTitleBackdropDataAsset::BuildDefaultFormations();
        }
    }
}

void UOVTitleBackdropStage::Activate(UWorld* World, FName ScenarioOrFormationId, EOVTitleBackdropMode Mode)
{
    if (!IsValid(World))
    {
        Release();
        return;
    }

    CachedWorld = World;
    EnsureFormationsLoaded();
    ActiveMode = BackdropAsset ? BackdropAsset->PreferredMode : Mode;
    SetFormation(ScenarioOrFormationId);
}

void UOVTitleBackdropStage::SetMode(EOVTitleBackdropMode Mode)
{
    if (ActiveMode == Mode)
    {
        return;
    }

    ActiveMode = Mode;
    if (ActiveFormationId != NAME_None)
    {
        const FName CurrentId = ActiveFormationId;
        ActiveFormationId = NAME_None;
        SetFormation(CurrentId);
    }
}

void UOVTitleBackdropStage::SetFormation(FName ScenarioOrFormationId)
{
    EnsureFormationsLoaded();
    const FOVTitleBackdropFormation* Formation =
        UOVTitleBackdropDataAsset::FindFormationForScenario(CachedFormations, ScenarioOrFormationId);
    if (!Formation)
    {
        return;
    }

    ActiveFormationId = Formation->FormationId;
    ActiveFormationLabel = Formation->DisplayName;
    BaseCameraLocation = Formation->CameraLocation;
    BaseCameraRotation = Formation->CameraRotation;
    IdleElapsedSeconds = 0.f;

    if (ActiveMode == EOVTitleBackdropMode::PreRenderedInGameImage)
    {
        PreRenderedTexture = Cast<UTexture2D>(Formation->PreRenderedTexturePath.TryLoad());
        if (PreRenderedTexture)
        {
            Release();
            return;
        }
        // Pre-rendered texture has not been baked yet; fall back seamlessly to LiveProxyStage.
        ActiveMode = EOVTitleBackdropMode::LiveProxyStage;
    }

    RebuildLiveProxyStage(*Formation);
}

void UOVTitleBackdropStage::RebuildLiveProxyStage(const FOVTitleBackdropFormation& Formation)
{
    UWorld* World = CachedWorld.Get();
    if (!IsValid(World))
    {
        return;
    }

    // Tear down any previous stage
    if (IsValid(TitleCameraActor))
    {
        TitleCameraActor->Destroy();
        TitleCameraActor = nullptr;
    }
    if (IsValid(StageActor))
    {
        StageActor->Destroy();
        StageActor = nullptr;
    }

    SkyboxComponent = nullptr;
    NebulaComponent = nullptr;
    PlanetMeshComponent = nullptr;
    CloudShadowsMeshComponent = nullptr;
    CloudsMeshComponent = nullptr;
    AtmosphereMeshComponent = nullptr;
    KeyLightComponent = nullptr;
    RimLightComponent = nullptr;
    BounceLightComponent = nullptr;
    SkyLightComponent = nullptr;
    ShipMeshComponents.Reset();

    // 1. Stage Anchor Actor
    FActorSpawnParameters SpawnParams;
    SpawnParams.ObjectFlags |= RF_Transient;
    StageActor = World->SpawnActor<AActor>(
        AActor::StaticClass(),
        FVector(0.0, 0.0, StageIsolationZ),
        FRotator::ZeroRotator,
        SpawnParams);

    if (!IsValid(StageActor))
    {
        return;
    }

    USceneComponent* Root = NewObject<USceneComponent>(StageActor);
    StageActor->SetRootComponent(Root);
    Root->RegisterComponent();
    StageActor->SetActorLocation(FVector(0.0, 0.0, StageIsolationZ));
    StageActor->SetActorEnableCollision(false);
    StageActor->SetActorTickEnabled(false);

    // 2. Viewport Camera Actor (Direct viewport rendering, TSR + PostProcess)
    const FVector WorldCameraLoc = FVector(0.0, 0.0, StageIsolationZ) + Formation.CameraLocation;
    TitleCameraActor = World->SpawnActor<ACameraActor>(
        ACameraActor::StaticClass(),
        WorldCameraLoc,
        Formation.CameraRotation,
        SpawnParams);

    if (IsValid(TitleCameraActor))
    {
        UCameraComponent* Cam = TitleCameraActor->GetCameraComponent();
        Cam->SetFieldOfView(Formation.CameraFov);
        Cam->bConstrainAspectRatio = false;

        // Hardware Ray Tracing & Lumen GI + Reflections
        Cam->PostProcessSettings.bOverride_DynamicGlobalIlluminationMethod = true;
        Cam->PostProcessSettings.DynamicGlobalIlluminationMethod = EDynamicGlobalIlluminationMethod::Lumen;
        Cam->PostProcessSettings.bOverride_ReflectionMethod = true;
        Cam->PostProcessSettings.ReflectionMethod = EReflectionMethod::Lumen;
        Cam->PostProcessSettings.bOverride_LumenRayLightingMode = true;
        Cam->PostProcessSettings.LumenRayLightingMode = ELumenRayLightingModeOverride::HitLighting;
        Cam->PostProcessSettings.bOverride_LumenSceneDetail = true;
        Cam->PostProcessSettings.LumenSceneDetail = 2.0f;
        Cam->PostProcessSettings.bOverride_LumenSceneLightingUpdateSpeed = true;
        Cam->PostProcessSettings.LumenSceneLightingUpdateSpeed = 4.0f;
        Cam->PostProcessSettings.bOverride_LumenFinalGatherQuality = true;
        Cam->PostProcessSettings.LumenFinalGatherQuality = 2.0f;
        Cam->PostProcessSettings.bOverride_LumenMaxTraceDistance = true;
        Cam->PostProcessSettings.LumenMaxTraceDistance = 60000.0f;
        Cam->PostProcessSettings.bOverride_LumenReflectionQuality = true;
        Cam->PostProcessSettings.LumenReflectionQuality = 2.0f;
        Cam->PostProcessSettings.bOverride_LumenMaxRoughnessToTraceReflections = true;
        Cam->PostProcessSettings.LumenMaxRoughnessToTraceReflections = 0.8f;
        Cam->PostProcessSettings.bOverride_LumenSurfaceCacheResolution = true;
        Cam->PostProcessSettings.LumenSurfaceCacheResolution = 1.0f;

        // Cinematic Exposure & High Contrast Space Lighting matching reference
        Cam->PostProcessSettings.bOverride_AutoExposureMethod = true;
        Cam->PostProcessSettings.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
        Cam->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
        Cam->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure = false;
        Cam->PostProcessSettings.bOverride_AutoExposureBias = true;
        Cam->PostProcessSettings.AutoExposureBias = -0.75f;

        Cam->PostProcessSettings.bOverride_ColorContrast = true;
        Cam->PostProcessSettings.ColorContrast = FVector4(1.35f, 1.35f, 1.38f, 1.0f);

        Cam->PostProcessSettings.bOverride_ColorGamma = true;
        Cam->PostProcessSettings.ColorGamma = FVector4(0.88f, 0.88f, 0.88f, 1.0f);

        Cam->PostProcessSettings.bOverride_ColorGain = true;
        Cam->PostProcessSettings.ColorGain = FVector4(1.02f, 1.02f, 1.05f, 1.0f);

        Cam->PostProcessSettings.bOverride_VignetteIntensity = true;
        Cam->PostProcessSettings.VignetteIntensity = 0.40f;

        Cam->PostProcessSettings.bOverride_BloomIntensity = true;
        Cam->PostProcessSettings.BloomIntensity = 0.45f;
        Cam->PostProcessSettings.bOverride_BloomThreshold = true;
        Cam->PostProcessSettings.BloomThreshold = 1.0f;
        Cam->PostProcessSettings.bOverride_ToneCurveAmount = true;
        Cam->PostProcessSettings.ToneCurveAmount = 1.0f;

        // Immediately direct the primary viewport camera
        if (APlayerController* PC = World->GetFirstPlayerController())
        {
            PC->SetViewTarget(TitleCameraActor);
        }
    }

    // 3. Directional Key Light (Far-side Sun shining back towards camera; near hulls in dramatic shadow, razor-sharp rim highlights)
    KeyLightComponent = NewObject<UDirectionalLightComponent>(StageActor);
    KeyLightComponent->SetupAttachment(Root);
    KeyLightComponent->SetRelativeRotation(FRotator(-18.f, -152.f, 0.f));
    KeyLightComponent->SetLightColor(FLinearColor(1.0f, 0.98f, 0.95f));
    KeyLightComponent->SetIntensity(8.5f);
    KeyLightComponent->SetAtmosphereSunLight(true);
    KeyLightComponent->SetAtmosphereSunLightIndex(0);
    KeyLightComponent->SetCastShadows(true);
    KeyLightComponent->SetCastRaytracedShadows(ECastRayTracedShadow::Enabled);
    KeyLightComponent->SetDynamicShadowDistanceMovableLight(60000.f);
    KeyLightComponent->SetDynamicShadowCascades(4);
    KeyLightComponent->RegisterComponent();

    // 4. Directional Rim Light (Grazing top-rim catching silhouettes from far-side)
    RimLightComponent = NewObject<UDirectionalLightComponent>(StageActor);
    RimLightComponent->SetupAttachment(Root);
    RimLightComponent->SetRelativeRotation(FRotator(-12.f, -118.f, 0.f));
    RimLightComponent->SetLightColor(FLinearColor(0.2f, 0.65f, 1.0f));
    RimLightComponent->SetIntensity(1.5f);
    RimLightComponent->SetCastShadows(false);
    RimLightComponent->RegisterComponent();

    // 4b. Directional Planetary Bounce Light (Subtle, deep oceanic ambient fill)
    BounceLightComponent = NewObject<UDirectionalLightComponent>(StageActor);
    BounceLightComponent->SetupAttachment(Root);
    BounceLightComponent->SetRelativeRotation(FRotator(60.f, 35.f, 0.f));
    BounceLightComponent->SetLightColor(FLinearColor(0.015f, 0.045f, 0.10f));
    BounceLightComponent->SetIntensity(0.18f);
    BounceLightComponent->SetCastShadows(false);
    BounceLightComponent->RegisterComponent();

    // 5. Ambient Sky Light (Cosmic deep space subtle fill)
    SkyLightComponent = NewObject<USkyLightComponent>(StageActor);
    SkyLightComponent->SetupAttachment(Root);
    SkyLightComponent->SetLightColor(FLinearColor(0.015f, 0.025f, 0.04f));
    SkyLightComponent->SetIntensity(0.06f);
    SkyLightComponent->SetCastShadows(false);
    SkyLightComponent->RegisterComponent();

    // 6. Space Skybox (8K Stars & Milky Way)
    UStaticMesh* SkyboxMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/StarfieldFree/Mesh/SM_StarSphereFree.SM_StarSphereFree"), nullptr, LOAD_NoWarn);
    if (!SkyboxMesh)
    {
        SkyboxMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Presentation/Environment/SpaceSkybox/SM_Source.SM_Source"), nullptr, LOAD_NoWarn);
    }
    if (SkyboxMesh)
    {
        SkyboxComponent = NewObject<UStaticMeshComponent>(StageActor);
        SkyboxComponent->SetupAttachment(Root);
        SkyboxComponent->SetStaticMesh(SkyboxMesh);
        SkyboxComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        SkyboxComponent->SetCastShadow(false);

        UMaterialInterface* SkyMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/StarfieldFree/Materials/Material_Instance/MI_Starfield_Stars_Free.MI_Starfield_Stars_Free"), nullptr, LOAD_NoWarn);
        if (!SkyMat)
        {
            SkyMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Presentation/Environment/SpaceSkybox/M_Source.M_Source"), nullptr, LOAD_NoWarn);
        }
        if (SkyMat)
        {
            SkyboxComponent->SetMaterial(0, SkyMat);
        }

        SkyboxComponent->SetRelativeLocation(Formation.CameraLocation);
        SkyboxComponent->SetRelativeScale3D(FVector(30000.f));
        SkyboxComponent->RegisterComponent();
    }

    // 6b. Concentric Cosmic Nebula Sphere (Deep Blue / Indigo space dust)
    UStaticMesh* NebulaMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/StarfieldFree/Mesh/SM_Nebula_Sphere_Free.SM_Nebula_Sphere_Free"), nullptr, LOAD_NoWarn);
    if (NebulaMesh)
    {
        NebulaComponent = NewObject<UStaticMeshComponent>(StageActor);
        NebulaComponent->SetupAttachment(Root);
        NebulaComponent->SetStaticMesh(NebulaMesh);
        NebulaComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        NebulaComponent->SetCastShadow(false);

        UMaterialInterface* NebulaMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/StarfieldFree/Materials/Material_Instance/MI_Starfield_Nebula_Free_01.MI_Starfield_Nebula_Free_01"), nullptr, LOAD_NoWarn);
        if (!NebulaMat)
        {
            NebulaMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/StarfieldFree/Materials/M_Starfield_Nebula_Free_v02.M_Starfield_Nebula_Free_v02"), nullptr, LOAD_NoWarn);
        }
        if (NebulaMat)
        {
            if (UMaterialInstanceDynamic* NebDyn = UMaterialInstanceDynamic::Create(NebulaMat, NebulaComponent))
            {
                NebDyn->SetScalarParameterValue(TEXT("Nebula 1 intensity"), 0.16f);
                NebulaComponent->SetMaterial(0, NebDyn);
            }
            else
            {
                NebulaComponent->SetMaterial(0, NebulaMat);
            }
        }

        NebulaComponent->SetRelativeLocation(Formation.CameraLocation);
        NebulaComponent->SetRelativeRotation(FRotator(0.f, -40.f, 0.f));
        NebulaComponent->SetRelativeScale3D(FVector(28000.f));
        NebulaComponent->RegisterComponent();
    }

    // 7. Quad-Layer Photorealistic Earth (Curved gracefully across lower screen, positioned safely behind fleet)
    UStaticMesh* PlanetMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Planet_Project/Assets/Static_Meshes/SM_Earth_Planet.SM_Earth_Planet"), nullptr, LOAD_NoWarn);
    if (!PlanetMesh)
    {
        PlanetMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Planet_Project/Assets/Static_Meshes/SM_Custom_Planet.SM_Custom_Planet"), nullptr, LOAD_NoWarn);
    }
    if (!PlanetMesh)
    {
        PlanetMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"), nullptr, LOAD_NoWarn);
    }

    if (PlanetMesh)
    {
        const FRotator CamRot = Formation.CameraRotation;
        const FVector CamForward = CamRot.Vector();
        const FVector CamRight = FRotationMatrix(CamRot).GetScaledAxis(EAxis::Y);
        const FVector CamUp = FRotationMatrix(CamRot).GetScaledAxis(EAxis::Z);

        // Fwd = 13500.f ensures planet front surface is behind all ships (fleet Fwd dist: 3300 to 6200)
        const float FwdDist = 13500.f;
        const float HalfFovX = FMath::DegreesToRadians(Formation.CameraFov * 0.5f);
        const float TanHalfFovX = FMath::Tan(HalfFovX);
        const float TanHalfFovY = TanHalfFovX / (16.f / 9.f);
        const float PlanetRight = (0.85f - 0.5f) * 2.f * TanHalfFovX * FwdDist;
        const float PlanetUp = (0.5f - 1.68f) * 2.f * TanHalfFovY * FwdDist;

        const FVector PlanetLoc = Formation.CameraLocation + (CamForward * FwdDist) + (CamRight * PlanetRight) + (CamUp * PlanetUp);
        const FRotator PlanetRot = FRotator(15.f, -140.f, -10.f);

        // 7a. Base Planet Surface (8K NASA Earth daymap, normal topography relief, deep realistic oceans, night city lights)
        PlanetMeshComponent = NewObject<UStaticMeshComponent>(StageActor);
        PlanetMeshComponent->SetupAttachment(Root);
        PlanetMeshComponent->SetStaticMesh(PlanetMesh);
        PlanetMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        PlanetMeshComponent->SetCastShadow(false);
        PlanetMeshComponent->SetRelativeLocation(PlanetLoc);
        PlanetMeshComponent->SetRelativeRotation(PlanetRot);
        PlanetMeshComponent->SetRelativeScale3D(FVector(152.0f)); // radius = 152 * 50 = 7600

        UMaterialInterface* PlanetMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Planet_Project/Assets/Materials/MI_Planet.MI_Planet"), nullptr, LOAD_NoWarn);
        if (!PlanetMat)
        {
            PlanetMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Presentation/Environment/SpaceSkybox/M_Planet.M_Planet"), nullptr, LOAD_NoWarn);
        }
        if (PlanetMat)
        {
            if (UMaterialInstanceDynamic* PlanetDyn = UMaterialInstanceDynamic::Create(PlanetMat, PlanetMeshComponent))
            {
                PlanetDyn->SetVectorParameterValue(TEXT("Planet_Color_Blend"), FLinearColor(0.35f, 0.50f, 0.70f, 1.0f));
                PlanetDyn->SetScalarParameterValue(TEXT("Planet_Color_Blend_Intensity"), 0.50f);
                PlanetDyn->SetScalarParameterValue(TEXT("Specular_Switch"), 0.0f);
                PlanetDyn->SetScalarParameterValue(TEXT("Specular Weight"), 0.0f);
                PlanetDyn->SetVectorParameterValue(TEXT("Specular Color"), FLinearColor(0.5f, 0.6f, 0.8f, 1.0f));
                PlanetDyn->SetScalarParameterValue(TEXT("Roughness"), 0.60f);
                PlanetDyn->SetScalarParameterValue(TEXT("Normal_Intensity"), 4.5f);
                PlanetDyn->SetScalarParameterValue(TEXT("City_Lights"), 1.2f);
                PlanetDyn->SetScalarParameterValue(TEXT("City_Lights_Shadow_Only"), 1.0f);
                PlanetDyn->SetScalarParameterValue(TEXT("Emissive Strength"), 0.60f);
                PlanetDyn->SetScalarParameterValue(TEXT("Trim Lights"), 1.5f);
                PlanetMeshComponent->SetMaterial(0, PlanetDyn);
            }
            else
            {
                PlanetMeshComponent->SetMaterial(0, PlanetMat);
            }
        }
        PlanetMeshComponent->RegisterComponent();

        // 7b. Swirling 8K Cloud Layer (Translucent NASA cloud swirls)
        UMaterialInterface* CloudMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Planet_Project/Assets/Materials/MI_Clouds.MI_Clouds"), nullptr, LOAD_NoWarn);
        if (CloudMat)
        {
            CloudsMeshComponent = NewObject<UStaticMeshComponent>(StageActor);
            CloudsMeshComponent->SetupAttachment(Root);
            CloudsMeshComponent->SetStaticMesh(PlanetMesh);
            CloudsMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            CloudsMeshComponent->SetCastShadow(false);
            CloudsMeshComponent->SetRelativeLocation(PlanetLoc);
            CloudsMeshComponent->SetRelativeRotation(PlanetRot);
            CloudsMeshComponent->SetRelativeScale3D(FVector(152.8f));
            if (UMaterialInstanceDynamic* CloudDyn = UMaterialInstanceDynamic::Create(CloudMat, CloudsMeshComponent))
            {
                CloudDyn->SetScalarParameterValue(TEXT("Cloud_Opacity"), 1.1f);
                CloudDyn->SetScalarParameterValue(TEXT("Normal_Intensity"), 1.5f);
                CloudDyn->SetVectorParameterValue(TEXT("Cloud Color"), FLinearColor(1.0f, 1.0f, 1.0f, 1.0f));
                CloudsMeshComponent->SetMaterial(0, CloudDyn);
            }
            else
            {
                CloudsMeshComponent->SetMaterial(0, CloudMat);
            }
            CloudsMeshComponent->RegisterComponent();
        }

        // 7c. Cyan Atmospheric Limb Scattering Rim (Additive Rayleigh glow along sunlit crescent edge)
        UMaterialInterface* AtmoMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Planet_Project/Assets/Materials/MI_Atmosphere.MI_Atmosphere"), nullptr, LOAD_NoWarn);
        if (AtmoMat)
        {
            AtmosphereMeshComponent = NewObject<UStaticMeshComponent>(StageActor);
            AtmosphereMeshComponent->SetupAttachment(Root);
            AtmosphereMeshComponent->SetStaticMesh(PlanetMesh);
            AtmosphereMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            AtmosphereMeshComponent->SetCastShadow(false);
            AtmosphereMeshComponent->SetRelativeLocation(PlanetLoc);
            AtmosphereMeshComponent->SetRelativeRotation(PlanetRot);
            AtmosphereMeshComponent->SetRelativeScale3D(FVector(154.5f));
            if (UMaterialInstanceDynamic* AtmoDyn = UMaterialInstanceDynamic::Create(AtmoMat, AtmosphereMeshComponent))
            {
                AtmoDyn->SetScalarParameterValue(TEXT("Atmosphere Intesity"), 0.45f);
                AtmoDyn->SetScalarParameterValue(TEXT("Atmosphere Edge Clamp"), 2.0f);
                AtmoDyn->SetScalarParameterValue(TEXT("Atmosphere Falloff"), 70.0f);
                AtmoDyn->SetVectorParameterValue(TEXT("Atmosphere Color"), FLinearColor(0.05f, 0.65f, 1.0f, 1.0f));
                AtmosphereMeshComponent->SetMaterial(0, AtmoDyn);
            }
            else
            {
                AtmosphereMeshComponent->SetMaterial(0, AtmoMat);
            }
            AtmosphereMeshComponent->RegisterComponent();
        }
    }

    // 8. In-game SM_Hull ship meshes with readable hull plating materials
    for (const FOVTitleBackdropShipSlot& Slot : Formation.Ships)
    {
        UStaticMesh* HullMesh = Cast<UStaticMesh>(Slot.StaticMeshPath.TryLoad());
        if (!HullMesh)
        {
            continue;
        }

        UStaticMeshComponent* MeshComp = NewObject<UStaticMeshComponent>(StageActor);
        MeshComp->SetupAttachment(Root);
        MeshComp->SetStaticMesh(HullMesh);
        MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        MeshComp->SetGenerateOverlapEvents(false);
        MeshComp->SetCastShadow(true);
        MeshComp->SetRelativeLocation(Slot.RelativeLocation);
        MeshComp->SetRelativeRotation(Slot.RelativeRotation);
        MeshComp->SetRelativeScale3D(Slot.RelativeScale);

        // Apply matching M_Readable material for full sci-fi hull plating and bevels
        FString MeshPathStr = Slot.StaticMeshPath.ToString();
        FString MatPathStr = MeshPathStr.Replace(TEXT("SM_Hull.SM_Hull"), TEXT("M_Readable.M_Readable"));
        if (MeshPathStr.Contains(TEXT("SM_PC14_FullMesh")))
        {
            MatPathStr = TEXT("/Game/Presentation/PC14Full/M_PC14_FullReadable.M_PC14_FullReadable");
        }
        if (UMaterialInterface* HullMat = LoadObject<UMaterialInterface>(nullptr, *MatPathStr, nullptr, LOAD_NoWarn))
        {
            if (UMaterialInstanceDynamic* HullDyn = UMaterialInstanceDynamic::Create(HullMat, MeshComp))
            {
                HullDyn->SetVectorParameterValue(TEXT("FactionTint"), FLinearColor(0.18f, 0.22f, 0.28f, 1.0f));
                HullDyn->SetScalarParameterValue(TEXT("EmissiveFill"), 0.0f);
                MeshComp->SetMaterial(0, HullDyn);
            }
            else
            {
                MeshComp->SetMaterial(0, HullMat);
            }
        }

        MeshComp->RegisterComponent();
        ShipMeshComponents.Add(MeshComp);
    }
}

void UOVTitleBackdropStage::StepIdleMotion(float DeltaSeconds)
{
    if (ActiveMode != EOVTitleBackdropMode::LiveProxyStage || !IsValid(TitleCameraActor))
    {
        return;
    }

    IdleElapsedSeconds += FMath::Clamp(DeltaSeconds, 0.f, 0.1f);

    // Smooth, slow cinematic space drift
    const float DriftYaw = FMath::Sin(IdleElapsedSeconds * 0.15f) * 1.5f;
    const float DriftPitch = FMath::Cos(IdleElapsedSeconds * 0.11f) * 0.6f;
    const float DriftZ = FMath::Sin(IdleElapsedSeconds * 0.18f) * 22.f;

    TitleCameraActor->SetActorLocationAndRotation(
        FVector(0.0, 0.0, StageIsolationZ) + BaseCameraLocation + FVector(0.f, 0.f, DriftZ),
        BaseCameraRotation + FRotator(DriftPitch, DriftYaw, 0.f));
}

void UOVTitleBackdropStage::Release()
{
    if (UWorld* World = CachedWorld.Get())
    {
        if (APlayerController* PC = World->GetFirstPlayerController())
        {
            if (PC->GetViewTarget() == TitleCameraActor)
            {
                PC->SetViewTarget(PC->GetPawn() ? Cast<AActor>(PC->GetPawn()) : Cast<AActor>(PC));
            }
        }
    }

    if (IsValid(TitleCameraActor))
    {
        TitleCameraActor->Destroy();
        TitleCameraActor = nullptr;
    }

    if (IsValid(StageActor))
    {
        StageActor->Destroy();
        StageActor = nullptr;
    }

    SkyboxComponent = nullptr;
    NebulaComponent = nullptr;
    PlanetMeshComponent = nullptr;
    CloudShadowsMeshComponent = nullptr;
    CloudsMeshComponent = nullptr;
    AtmosphereMeshComponent = nullptr;
    KeyLightComponent = nullptr;
    RimLightComponent = nullptr;
    BounceLightComponent = nullptr;
    SkyLightComponent = nullptr;
    ShipMeshComponents.Reset();
    PreRenderedTexture = nullptr;
}

UTexture* UOVTitleBackdropStage::GetActiveTexture() const
{
    if (ActiveMode == EOVTitleBackdropMode::PreRenderedInGameImage)
    {
        return PreRenderedTexture;
    }
    return nullptr;
}
