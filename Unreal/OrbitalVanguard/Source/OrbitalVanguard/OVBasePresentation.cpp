#include "OVBasePresentation.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
#include "Engine/World.h"

namespace
{
FName SocketName(OVBase::Cell worldDirection,int turns)
{
    const auto d=OVBase::Rotate(worldDirection,(4-turns)%4);
    if (d.X) return d.X>0?TEXT("XP"):TEXT("XN");
    if (d.Y) return d.Y>0?TEXT("YP"):TEXT("YN");
    return d.Z>0?TEXT("ZP"):TEXT("ZN");
}

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
        for (const TCHAR* path : MeshesByKind)
            Assets.Add(LoadObject<UStaticMesh>(nullptr,path,nullptr,LOAD_NoWarn));
        Camera = GetWorld()->SpawnActor<ACameraActor>();
        if (!Camera) return;
        Camera->GetCameraComponent()->bConstrainAspectRatio=false;
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

OVBase::Error AOVBasePresentation::CheckPlacement(const OVBase::Module& candidate) const
{
    const auto logical=Build.Check(candidate);
    if (logical!=OVBase::Error::None) return logical;
    // 한쪽에 스냅한 결과가 다른 연결면과도 5cm 이내여야 고리를 닫을 수 있다.
    TOptional<FVector> anchor;
    if (!OVBase::IsLink(candidate.Type)) return logical;
    const int32 index=static_cast<int32>(candidate.Type);
    const auto* asset=Assets.IsValidIndex(index)?Assets[index].Get():nullptr;
    if (!asset) return logical;
    for (size_t i=0; i<Build.Modules().size(); ++i)
    {
        const auto& other=Build.Modules()[i];
        if (!OVBase::Connected(other,candidate) || !Meshes.IsValidIndex(i)) continue;
        for (const auto& a:OVBase::Ports(candidate))
            for (const auto& b:OVBase::Ports(other))
            {
                if (!(a.At+a.Direction==b.At) || !(a.Direction+b.Direction==OVBase::Cell{})) continue;
                const auto* socket=asset->FindSocket(SocketName(a.Direction,candidate.Turns));
                const FName otherSocket=SocketName(b.Direction,other.Turns);
                if (!socket || !Meshes[i]->DoesSocketExist(otherSocket)) continue;
                const FVector offset=FRotator(0,90*candidate.Turns,0).RotateVector(socket->RelativeLocation);
                const FVector origin=Meshes[i]->GetSocketLocation(otherSocket)-offset;
                if (anchor.IsSet() && FVector::DistSquared(anchor.GetValue(),origin)>25.) return OVBase::Error::Seam;
                anchor=origin;
            }
    }
    return logical;
}

bool AOVBasePresentation::ReserveModule(FName id,FIntVector origin,int32 turns)
{
    OVBase::Kind kind;
    if (id==TEXT("Corridor")) kind=OVBase::Kind::Corridor;
    else if (id==TEXT("Junction")) kind=OVBase::Kind::Junction;
    else if (id==TEXT("Storage")) kind=OVBase::Kind::Storage;
    else return false;
    const OVBase::Module candidate{kind,{origin.X,origin.Y,origin.Z},turns};
    if (CheckPlacement(candidate)!=OVBase::Error::None || !Build.Start(candidate)) return false;
    RefreshMeshes();
    return true;
}

bool AOVBasePresentation::CancelConstruction(int32 index)
{
    if (index<0 || !Build.Cancel(index)) return false;
    RefreshMeshes();
    return true;
}

float AOVBasePresentation::RemainingSeconds(int32 index) const
{
    return index>=0 && index<ModuleCount()?float(Build.Modules()[index].Remaining):-1.f;
}

void AOVBasePresentation::RefreshMeshes()
{
    for (const auto& Mesh : Meshes) Mesh->DestroyComponent();
    Meshes.Reset();
    const auto& modules=Build.Modules();
    for (const auto& Item : modules)
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
        const int32 index=static_cast<int32>(Item.Type);
        auto* Asset = Assets.IsValidIndex(index)?Assets[index].Get():nullptr;
        if (!Asset)
        {
            Asset = LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
            Mesh->SetWorldScale3D(D * (Item.Remaining > 0 ? .007 : .009));
            Mesh->SetWorldLocationAndRotation(BottomCenter+FVector(0,0,D.Z*.5),Rotation);
        }
        else
        {
            Mesh->SetStaticMesh(Asset);
            const auto LocalAnchor = Item.Type == OVBase::Kind::Core ? GetActorLocation() : CellCenter(Item.Origin);
            Mesh->SetWorldLocationAndRotation(LocalAnchor,Rotation);
            if (Item.Type==OVBase::Kind::Core) Mesh->SetWorldScale3D(FVector(1,1,.91));
            if (Item.Type==OVBase::Kind::Storage) Mesh->SetWorldScale3D(FVector(.92));
        }
        Mesh->SetStaticMesh(Asset);
    }

    // 중간 공사가 취소돼 다른 경로로 연결된 경우에도 코어에서 바깥쪽 순서로 접합한다.
    TArray<int32> queue{0};
    TSet<int32> placed{0};
    for (int32 step=0; step<queue.Num(); ++step)
    {
        const int32 parent=queue[step];
        for (int32 i=1; i<Meshes.Num(); ++i)
        {
            if (placed.Contains(i) || !OVBase::Connected(modules[parent],modules[i])) continue;
            for (const auto& a:OVBase::Ports(modules[parent]))
                for (const auto& b:OVBase::Ports(modules[i]))
                {
                    if (!(a.At+a.Direction==b.At) || !(a.Direction+b.Direction==OVBase::Cell{})) continue;
                    const FName sa=SocketName(a.Direction,modules[parent].Turns);
                    const FName sb=SocketName(b.Direction,modules[i].Turns);
                    if (Meshes[parent]->DoesSocketExist(sa) && Meshes[i]->DoesSocketExist(sb))
                        Meshes[i]->AddWorldOffset(Meshes[parent]->GetSocketLocation(sa)-Meshes[i]->GetSocketLocation(sb));
                }
            queue.Add(i);
            placed.Add(i);
        }
    }
}
