#include "OVShipPreview.h"
#include "OVShip.h"
#include "OVShipSystemsComponent.h"
#include "OVWeaponVisualComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "ProfilingDebugging/CsvProfiler.h"

int32 UOVShipPreview::GetVisibleMeshCount() const
{
    int32 Count=0;for(const auto& Mesh:Mirrors)if(IsValid(Mesh)&&Mesh->IsVisible())++Count;return Count;
}

void UOVShipPreview::Release()
{
    if (IsValid(Proxy)) Proxy->Destroy();
    Proxy=nullptr; Capture=nullptr; Texture=nullptr; DesignRoot=nullptr; EquippedModels.Reset();
    Mirrors.Reset(); Sources.Reset(); Subject.Reset(); LastCapture=-1; LastFocusMount=INDEX_NONE;
}

void UOVShipPreview::Update(AOVShip* Ship, FRotator Rotation, float Zoom, float Aspect, int32 FocusMount, int32 PixelWidth, bool bPortrait, const TArray<FOVWeaponMountDefinition>* DesignWeapons)
{
    TRACE_CPUPROFILER_EVENT_SCOPE(OV_ShipPreview);
    CSV_SCOPED_TIMING_STAT_EXCLUSIVE(OV_Preview);
    if (!IsValid(Ship) || !Ship->Systems || !Ship->Systems->Layout) { Release(); return; }
    if (Subject!=Ship) Release();
    Aspect=FMath::Max(DesignWeapons?.05f:.5f,Aspect);
    const int32 WidthPx=FMath::Clamp(PixelWidth,1,FMath::FloorToInt(4096.f*FMath::Min(1.f,Aspect)));
    const int32 HeightPx=FMath::Max(1,FMath::RoundToInt(WidthPx/Aspect));
    const bool bSameResolution=Texture && Texture->SizeX==WidthPx && Texture->SizeY==HeightPx;
    const double Now=FPlatformTime::Seconds();
    bool bEquipmentChanged=(DesignWeapons!=nullptr)!=(DesignRoot!=nullptr);
    if(DesignWeapons && !bEquipmentChanged)
    {
        bEquipmentChanged=DesignWeapons->Num()!=EquippedModels.Num();
        for(int32 I=0;!bEquipmentChanged && I<EquippedModels.Num();++I)
            bEquipmentChanged=!EquippedModels[I]->Matches((*DesignWeapons)[I],Ship->Systems->Layout->LocalHalfExtent);
    }
    if (!bEquipmentChanged && bSameResolution && Now-LastCapture<.05 && FMath::IsNearlyEqual(Zoom,LastZoom) && Rotation.Equals(ViewRotation) && FMath::IsNearlyEqual(ViewAspect,Aspect) && LastFocusMount==FocusMount) return;
    LastCapture=Now;LastFocusMount=FocusMount;
    if (!Proxy)
    {
        FActorSpawnParameters Params; Params.ObjectFlags|=RF_Transient;
        Proxy=Ship->GetWorld()->SpawnActor<AActor>(AActor::StaticClass(),FVector(0,0,-10000000),FRotator::ZeroRotator,Params);
        auto* Root=NewObject<USceneComponent>(Proxy); Proxy->SetRootComponent(Root); Root->RegisterComponent();
        Proxy->SetActorLocation(FVector(0,0,-10000000));
        Proxy->SetActorEnableCollision(false); Proxy->SetActorTickEnabled(false);
        Texture=NewObject<UTextureRenderTarget2D>(this);
        Texture->ClearColor=FLinearColor(.009f,.02f,.035f);
        Texture->Filter=bPortrait ? TF_Trilinear : TF_Bilinear;
        Texture->bAutoGenerateMips=bPortrait;
        Texture->InitAutoFormat(WidthPx,HeightPx);
        Capture=NewObject<USceneCaptureComponent2D>(Proxy);
        Capture->SetupAttachment(Root); Capture->RegisterComponent();
        Capture->TextureTarget=Texture; Capture->ProjectionType=ECameraProjectionMode::Orthographic;
        Capture->CaptureSource=ESceneCaptureSource::SCS_FinalColorLDR;
        Capture->PrimitiveRenderMode=ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
        Capture->bCaptureEveryFrame=false; Capture->bCaptureOnMovement=false;
        Capture->ShowFlags.SetLighting(true); Capture->ShowFlags.SetAtmosphere(false);
        Capture->ShowFlags.SetFog(false); Capture->ShowFlags.SetPostProcessing(false);
        Subject=Ship;
    }
    TArray<UStaticMeshComponent*> Current; Ship->GetComponents<UStaticMeshComponent>(Current);
    bool Rebuild=Current.Num()!=Sources.Num();
    if (!Rebuild) for (int32 I=0;I<Current.Num();++I) if (Sources[I]!=Current[I]) { Rebuild=true; break; }
    if (Rebuild)
    {
        Capture->ShowOnlyComponents.Reset();
        for (auto Mesh:Mirrors) if (Mesh) Mesh->DestroyComponent();
        Sources.Reset(); Mirrors.Reset();
        for (auto* Source:Current)
        {
            auto* Mesh=NewObject<UStaticMeshComponent>(Proxy);
            Mesh->SetupAttachment(Proxy->GetRootComponent()); Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Mesh->SetCastShadow(false); Mesh->RegisterComponent();
            Capture->ShowOnlyComponent(Mesh); Sources.Add(Source); Mirrors.Add(Mesh);
        }
    }
    if(bEquipmentChanged || (Rebuild && DesignWeapons))
    {
        for(auto Model:EquippedModels)if(Model)Model->DestroyComponent();
        EquippedModels.Reset();
        Capture->ShowOnlyComponents.Reset();
        for(auto Part:Mirrors)Capture->ShowOnlyComponent(Part);
        if(DesignRoot){DesignRoot->DestroyComponent();DesignRoot=nullptr;}
        if(DesignWeapons)
        {
            DesignRoot=NewObject<USceneComponent>(Proxy);
            DesignRoot->SetupAttachment(Proxy->GetRootComponent());
            DesignRoot->RegisterComponent();
            for(const auto& Def:*DesignWeapons)
            {
                auto* Model=NewObject<UOVWeaponVisualComponent>(Proxy);
                Model->SetupAttachment(DesignRoot);
                Model->RegisterComponent();
                Model->Initialize(Def,Ship->Systems->Layout->LocalHalfExtent);
                Model->Synchronize(FRotator::ZeroRotator,Def.PresentationProfile==TEXT("StandardMissilePod"),false,true,0,-1,0);
                EquippedModels.Add(Model);
                TArray<USceneComponent*> Parts;
                Model->GetChildrenComponents(true,Parts);
                for(auto* Part:Parts)if(auto* Mesh=Cast<UStaticMeshComponent>(Part))Capture->ShowOnlyComponent(Mesh);
            }
        }
    }
    if(DesignRoot)DesignRoot->SetRelativeRotation(Rotation);
    LastZoom=Zoom; ViewRotation=Rotation; ViewAspect=Aspect;
    FVector Extent=Ship->Systems->Layout->LocalHalfExtent;
    double Width=1,Height=1;
    for (int I=0;I<8;++I)
    {
        const FVector P=Rotation.RotateVector(Extent*FVector((I&1)?1:-1,(I&2)?1:-1,(I&4)?1:-1));
        Width=FMath::Max(Width,FMath::Abs(P.Y)); Height=FMath::Max(Height,FMath::Abs(P.Z));
    }
    OrthoWidth=2.6*FMath::Max(Width,Height*ViewAspect)/FMath::Max(.5f,Zoom);
    FVector Focus=FVector::ZeroVector;
    if(Ship->Systems->Layout->WeaponMounts.IsValidIndex(FocusMount)) {
        const auto& Mount=Ship->Systems->Layout->WeaponMounts[FocusMount];
        Focus=Rotation.RotateVector(Mount.LocalPosition);
        OrthoWidth=FMath::Max(16.f,Mount.GeometryScale*OVWeaponGeometry::BarrelLength(Extent,Mount.Tier,Mount.Type==EOVWeaponType::Interception)*4.f);
    }
    if (Texture->SizeX!=WidthPx || Texture->SizeY!=HeightPx) Texture->ResizeTarget(WidthPx,HeightPx);
    Capture->OrthoWidth=OrthoWidth;
    Capture->SetRelativeLocation(FVector(-FMath::Max(5000.,Extent.Size()*4),Focus.Y,Focus.Z));
    Capture->SetRelativeRotation(FRotator::ZeroRotator);
    for (int32 I=0;I<Sources.Num();++I)
    {
        auto* Source=Sources[I].Get(); auto* Mirror=Mirrors[I].Get(); if (!Source) continue;
        Mirror->SetStaticMesh(Source->GetStaticMesh());
        for (int32 M=0;M<Source->GetNumMaterials();++M) Mirror->SetMaterial(M,Source->GetMaterial(M));
        const FTransform Local=Source->GetComponentTransform().GetRelativeTransform(Ship->GetActorTransform());
        Mirror->SetRelativeTransform(Local*FTransform(Rotation));
        bool bIncluded=true;
        if(DesignWeapons || Ship->Systems->Layout->WeaponMounts.IsValidIndex(FocusMount)) {
            bIncluded=DesignWeapons!=nullptr;
            for(const USceneComponent* Parent=Source->GetAttachParent();Parent;Parent=Parent->GetAttachParent())
                if(const auto* Weapon=Cast<UOVWeaponVisualComponent>(Parent)) {
                    bIncluded=!DesignWeapons && Weapon->GetMountId()==Ship->Systems->Layout->WeaponMounts[FocusMount].WeaponId;
                    break;
                }
        }
        Mirror->SetVisibility(Source->IsVisible()&&bIncluded);
    }
    Capture->CaptureScene();
}

FVector2D UOVShipPreview::Project(FVector LocalPosition) const
{
    const FVector P=ViewRotation.RotateVector(LocalPosition);
    return FVector2D(.5+P.Y/OrthoWidth,.5-P.Z/(OrthoWidth/ViewAspect));
}
