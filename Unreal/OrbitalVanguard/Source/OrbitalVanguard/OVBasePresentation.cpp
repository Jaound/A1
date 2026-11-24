#include "OVBasePresentation.h"

#include "OVController.h"
#include "OVShip.h"
#include "Math/OrientedBox.h"
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

// 두 원본 끝면을 측정해 허브 수평 포트 외곽을 직선 통로의 약 4.3×5.3m에 맞춘다.
FVector ModuleScale(OVBase::Kind kind)
{
    if (kind==OVBase::Kind::Junction) return FVector(.38,.38,.27);
    if (kind==OVBase::Kind::Storage) return FVector(.92);
    if (kind==OVBase::Kind::Core) return FVector(1,1,.91);
    return FVector::OneVector;
}

const TCHAR* const MeshesByKind[] = {
    TEXT("/Game/Presentation/Base/SM_OV_Base_CoreHub_5x5x5.SM_OV_Base_CoreHub_5x5x5"),
    TEXT("/Game/Presentation/Base/SM_OV_Base_CorridorStraight_1x1x1.SM_OV_Base_CorridorStraight_1x1x1"),
    TEXT("/Game/Presentation/Base/SM_OV_Base_CorridorHub_1x1x1.SM_OV_Base_CorridorHub_1x1x1"),
    TEXT("/Game/Presentation/Base/SM_OV_Base_StorageVault.SM_OV_Base_StorageVault"),
    TEXT("/Game/Presentation/Base/SM_OV_Base_YardSmall.SM_OV_Base_YardSmall"),
    TEXT("/Game/Presentation/Base/SM_OV_Base_DockStandard.SM_OV_Base_DockStandard")
};

FOrientedBox MakeBox(const FBox& local, const FTransform& transform)
{
    FOrientedBox box;
    box.Center=transform.TransformPosition(local.GetCenter());
    box.AxisX=transform.GetUnitAxis(EAxis::X);
    box.AxisY=transform.GetUnitAxis(EAxis::Y);
    box.AxisZ=transform.GetUnitAxis(EAxis::Z);
    const FVector extent=local.GetExtent()*transform.GetScale3D().GetAbs();
    box.ExtentX=extent.X; box.ExtentY=extent.Y; box.ExtentZ=extent.Z;
    return box;
}

bool BoxesOverlap(const FOrientedBox& a,const FOrientedBox& b)
{
    const auto separated=[&](const FVector& axis) {
        const auto left=a.Project(axis),right=b.Project(axis);
        return left.Max<right.Min || right.Max<left.Min;
    };
    const FVector axesA[]={a.AxisX,a.AxisY,a.AxisZ}, axesB[]={b.AxisX,b.AxisY,b.AxisZ};
    for (const auto& axis:axesB) if (separated(axis)) return false;
    for (const auto& axis:axesA)
    {
        if (separated(axis)) return false;
        for (const auto& other:axesB)
            if (separated(FVector::CrossProduct(axis,other))) return false;
    }
    return true;
}
}

AOVBasePresentation::AOVBasePresentation()
{
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

void AOVBasePresentation::InitializeModules()
{
    for (const TCHAR* path : MeshesByKind)
        Assets.Add(LoadObject<UStaticMesh>(nullptr,path,nullptr,LOAD_NoWarn));
    RefreshMeshes();
}

FVector AOVBasePresentation::CellCenter(OVBase::Cell Cell) const
{
    return GetActorLocation() + FVector(Cell.X,Cell.Y,Cell.Z)*OVBase::CellCm;
}

OVBase::Error AOVBasePresentation::CheckPlacement(const OVBase::Module& candidate, FString* blockingName) const
{
    const auto logical=Build.Check(candidate);
    if (logical!=OVBase::Error::None) return logical;
    // 한쪽에 스냅한 결과가 다른 연결면과도 5cm 이내여야 고리를 닫을 수 있다.
    TOptional<FVector> anchor;

    const int32 index=static_cast<int32>(candidate.Type);
    const auto* asset=Assets.IsValidIndex(index)?Assets[index].Get():nullptr;

    for (size_t i=0; i<Build.Modules().size(); ++i)
    {
        const auto& other=Build.Modules()[i];
        if (!asset || !OVBase::Connected(other,candidate) || !Meshes.IsValidIndex(i)) continue;
        for (const auto& a:OVBase::Ports(candidate))
            for (const auto& b:OVBase::Ports(other))
            {
                if (!(a.At+a.Direction==b.At) || !(a.Direction+b.Direction==OVBase::Cell{})) continue;
                const auto* socket=asset->FindSocket(SocketName(a.Direction,candidate.Turns));
                const FName otherSocket=SocketName(b.Direction,other.Turns);
                if (!socket || !Meshes[i]->DoesSocketExist(otherSocket)) continue;
                const FVector offset=FRotator(0,90*candidate.Turns,0).RotateVector(socket->RelativeLocation * ModuleScale(candidate.Type));
                const FVector origin=Meshes[i]->GetSocketLocation(otherSocket)-offset;
                if (anchor.IsSet() && FVector::DistSquared(anchor.GetValue(),origin)>25.) return OVBase::Error::Seam;
                anchor=origin;
            }
    }
    const FTransform transform(FRotator(0,90*candidate.Turns,0),anchor.Get(CellCenter(candidate.Origin)),ModuleScale(candidate.Type));
    if (const auto* ship=BlockingShip(candidate,&transform))
    {
        if (blockingName) *blockingName=ship->ShipName;
        return OVBase::Error::Ship;
    }
    return logical;
}

const AOVShip* AOVBasePresentation::BlockingShip(const OVBase::Module& candidate,const FTransform* transform) const
{
    const auto* pc=Cast<AOVController>(GetWorld()->GetFirstPlayerController());
    if (pc)
    {
        const auto cells=OVBase::Dimensions(candidate.Type);
        const FVector halfCell(OVBase::CellCm*.5);
        const FBox footprint(-halfCell,FVector(cells.X,cells.Y,cells.Z)*OVBase::CellCm-halfCell);
        const FRotator rotation(0,90*candidate.Turns,0);
        const auto reserved=MakeBox(footprint,FTransform(rotation,CellCenter(candidate.Origin)));
        TOptional<FOrientedBox> snapped;
        const int32 index=static_cast<int32>(candidate.Type);
        if (transform && Assets.IsValidIndex(index) && Assets[index])
            snapped=MakeBox(Assets[index]->GetBoundingBox(),*transform);
        for (const AOVShip* ship:pc->Ships)
        {
            if (!IsValid(ship) || ship->IsHidden() || !ship->Hull || !ship->Hull->GetStaticMesh()) continue;
            const auto hull=MakeBox(ship->Hull->GetStaticMesh()->GetBoundingBox(),ship->Hull->GetComponentTransform());
            if (BoxesOverlap(reserved,hull) || (snapped.IsSet() && BoxesOverlap(*snapped,hull)))
                return ship;
        }
    }
    return nullptr;
}

const AOVShip* AOVBasePresentation::ConstructionBlocker(int32 index) const
{
    if (!Meshes.IsValidIndex(index) || Build.Modules()[index].Remaining<=0) return nullptr;
    return BlockingShip(Build.Modules()[index],&Meshes[index]->GetComponentTransform());
}

void AOVBasePresentation::AdvanceConstruction(float seconds)
{
    const auto& items=Build.Modules();
    if (std::none_of(items.begin(),items.end(),[](const auto& m){return m.Remaining>0;})) return;
    std::vector<bool> blocked(items.size());
    for (int32 i=1;i<Meshes.Num();++i) blocked[i]=ConstructionBlocker(i)!=nullptr;
    Build.Advance(seconds,blocked);
    UpdateConstructionVisuals();
}

bool AOVBasePresentation::ReserveModule(FName id,FIntVector origin,int32 turns)
{
    OVBase::Kind kind;
    if (id==TEXT("Corridor")) kind=OVBase::Kind::Corridor;
    else if (id==TEXT("Junction")) kind=OVBase::Kind::Junction;
    else if (id==TEXT("Storage")) kind=OVBase::Kind::Storage;
    else if (id==TEXT("Yard")) kind=OVBase::Kind::Yard;
    else if (id==TEXT("Dock")) kind=OVBase::Kind::Dock;
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
        Mesh->SetCastShadow(true);
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
            Mesh->SetWorldScale3D(ModuleScale(Item.Type));
        }
        Mesh->SetStaticMesh(Asset);
        if (OVBase::HasBay(Item.Type) && Assets.IsValidIndex(index) && Assets[index])
        {
            Mesh->SetCollisionProfileName(TEXT("BlockAll"));
            Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        }
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
    UpdateConstructionVisuals();
}

void AOVBasePresentation::UpdateConstructionVisuals()
{
    const auto& items=Build.Modules();
    for (int32 i=1;i<Meshes.Num();++i)
    {
        auto* mesh=Meshes[i].Get();
        const auto& item=items[i];
        if (item.Remaining<=0)
        {
            mesh->SetCustomPrimitiveDataFloat(1,0.f);
            continue;
        }
        const double progress=1.-item.Remaining/OVBase::Duration(item.Type);
        const auto box=mesh->Bounds.GetBox();
        mesh->SetCustomPrimitiveDataFloat(0,FMath::Lerp(box.Min.Z,box.Max.Z,.015+.985*progress));
        mesh->SetCustomPrimitiveDataFloat(1,1.f);
    }
}
