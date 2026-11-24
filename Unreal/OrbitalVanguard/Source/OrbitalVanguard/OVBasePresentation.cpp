#include "OVBasePresentation.h"

#include "Camera/CameraActor.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"

namespace
{
const TCHAR* const MeshesByKind[] = {
    TEXT("/Game/Presentation/Base/SM_OV_Base_CoreHub_5x5x5.SM_OV_Base_CoreHub_5x5x5"),
    TEXT("/Game/Presentation/Base/SM_OV_Base_CorridorStraight_1x1x1.SM_OV_Base_CorridorStraight_1x1x1"),
    TEXT("/Game/Presentation/Base/SM_OV_Base_CorridorHub_1x1x1.SM_OV_Base_CorridorHub_1x1x1"),
    TEXT("/Game/Presentation/Base/SM_OV_Base_StorageVault.SM_OV_Base_StorageVault")
};
}

AOVBasePresentation::AOVBasePresentation()
{
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

void AOVBasePresentation::Show(bool bShow)
{
    if (bShow && !IsValid(Camera))
    {
        Camera = GetWorld()->SpawnActor<ACameraActor>();
        UpdateView(0,0,0,0);
        RefreshMeshes();
    }
    SetActorHiddenInGame(!bShow);
}

void AOVBasePresentation::EndPlay(const EEndPlayReason::Type Reason)
{
    if (IsValid(Camera)) Camera->Destroy();
    Super::EndPlay(Reason);
}

void AOVBasePresentation::UpdateView(float dt,float orbit,float pitch,float zoom)
{
    if (!IsValid(Camera)) return;
    Yaw += orbit * dt * 85.f;
    Pitch = FMath::Clamp(Pitch + pitch * dt * 65.f,20.f,85.f);
    Distance = FMath::Clamp(Distance * FMath::Pow(.85f,zoom),10000.f,120000.f);
    const FVector Offset = FRotator(Pitch,Yaw,0).Vector() * Distance;
    Camera->SetActorLocationAndRotation(GetActorLocation()+Offset,(-Offset).Rotation());
}

FVector AOVBasePresentation::CellCenter(OVBase::Cell Cell) const
{
    return GetActorLocation() + FVector(Cell.X,Cell.Y,Cell.Z)*OVBase::CellCm;
}

void AOVBasePresentation::RefreshMeshes()
{
    for (const auto& Mesh : Meshes) Mesh->DestroyComponent();
    Meshes.Reset();
    for (const auto& Item : Build.Modules())
    {
        const auto Dim = OVBase::Dimensions(Item.Type);
        const auto D = FVector(Dim.X,Dim.Y,Dim.Z) * OVBase::CellCm;
        const auto Rotation = FRotator(0,90*Item.Turns,0);
        const auto PlanarCenter = Rotation.RotateVector(FVector((D.X-OVBase::CellCm)*.5,(D.Y-OVBase::CellCm)*.5,0));
        const auto BottomCenter = CellCenter(Item.Origin)+PlanarCenter-FVector(0,0,OVBase::CellCm*.5);
        auto* Mesh = NewObject<UStaticMeshComponent>(this);
        Mesh->SetupAttachment(GetRootComponent());
        Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Mesh->SetCastShadow(false);
        Mesh->RegisterComponent();
        Meshes.Add(Mesh);
        auto* Asset = LoadObject<UStaticMesh>(nullptr,MeshesByKind[static_cast<int>(Item.Type)],nullptr,LOAD_NoWarn);
        if (!Asset)
        {
            Asset = LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
            Mesh->SetWorldScale3D(D * (Item.Remaining > 0 ? .007 : .009));
            Mesh->SetWorldLocationAndRotation(BottomCenter+FVector(0,0,D.Z*.5),Rotation);
        }
        else
            Mesh->SetWorldLocationAndRotation(BottomCenter,Rotation);
        Mesh->SetStaticMesh(Asset);
    }
}
