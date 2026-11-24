#include "OVBasePresentation.h"

#include "OVController.h"
#include "OVShip.h"
#include "Math/OrientedBox.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"

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
    TEXT("/Game/Presentation/Base/SM_OV_Base_DockStandard.SM_OV_Base_DockStandard"),
    TEXT("/Game/Presentation/Base/SM_OV_Base_YardMedium.SM_OV_Base_YardMedium"),
    TEXT("/Game/Presentation/Base/SM_OV_Base_YardLarge.SM_OV_Base_YardLarge"),
    TEXT("/Game/Presentation/Base/SM_OV_Base_YardSuperheavy.SM_OV_Base_YardSuperheavy"),
    TEXT("/Game/Presentation/Base/SM_OV_Base_DockMedium.SM_OV_Base_DockMedium"),
    TEXT("/Game/Presentation/Base/SM_OV_Base_DockLarge.SM_OV_Base_DockLarge"),
    TEXT("/Game/Presentation/Base/SM_OV_Base_DockSuperheavy.SM_OV_Base_DockSuperheavy"),
    TEXT("/Game/Presentation/Base/SM_OV_Base_LogisticsTerminal.SM_OV_Base_LogisticsTerminal"),
    TEXT("/Game/Presentation/Base/SM_OV_Base_ResearchLab.SM_OV_Base_ResearchLab"),
    TEXT("/Game/Presentation/Base/SM_OV_Base_HeroAcademy.SM_OV_Base_HeroAcademy"),
    TEXT("/Game/Presentation/Base/SM_OV_Base_CrewSupport.SM_OV_Base_CrewSupport"),
    TEXT("/Game/Presentation/Base/SM_OV_Base_DefensePlatform.SM_OV_Base_DefensePlatform"),
    TEXT("/Game/Presentation/Base/SM_OV_Base_RadarArray.SM_OV_Base_RadarArray"),
    TEXT("/Game/Presentation/Base/SM_OV_Base_ShieldBastion.SM_OV_Base_ShieldBastion"),
};
static_assert(UE_ARRAY_COUNT(MeshesByKind)==static_cast<int32>(OVBase::Kind::Count));

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
    Assets.SetNum(UE_ARRAY_COUNT(MeshesByKind));
    SleeveAssets.SetNum(Assets.Num());
    OriginalModels.Reset();
    for (const TCHAR* path : MeshesByKind)
        OriginalModels.Add(FPackageName::DoesPackageExist(FPackageName::ObjectPathToPackageName(FString(path))));
    for (int32 i=0; i<=static_cast<int32>(OVBase::Kind::Dock); ++i)
        if (OriginalModels[i]) Assets[i]=LoadObject<UStaticMesh>(nullptr,MeshesByKind[i],nullptr,LOAD_NoWarn);
    RefreshMeshes();
}

FVector AOVBasePresentation::CellCenter(OVBase::Cell Cell) const
{
    return GetActorLocation() + FVector(Cell.X,Cell.Y,Cell.Z)*OVBase::CellCm;
}


FTransform AOVBasePresentation::PlacementTransform(const OVBase::Module& candidate,const OVBase::BuildState& state,
    const TArray<FTransform>& transforms,bool& seam) const
{
    TOptional<FVector> anchor;
    const auto type=static_cast<int32>(candidate.Type);
    const UStaticMesh* asset=Assets.IsValidIndex(type)?Assets[type].Get():nullptr;
    if (!asset && HasOriginalModel(candidate.Type))
        asset=LoadObject<UStaticMesh>(nullptr,MeshesByKind[type],nullptr,LOAD_NoWarn);
    const FRotator rotation(0,90*candidate.Turns,0);
    // 끊어진 가지는 격자에 남고, 재연결 시 코어 쪽 소켓을 기준으로 다시 맞춘다.
    const auto anchored=state.Network(true);
    for (int32 i=0; i<transforms.Num(); ++i)
    {
        if (!anchored[i]) continue;
        const auto& other=state.Modules()[i];
        const int32 otherType=static_cast<int32>(other.Type);
        if (!asset || !OVBase::Connected(other,candidate) || !Assets.IsValidIndex(otherType) || !Assets[otherType]) continue;
        for (const auto& a:OVBase::Ports(candidate))
            for (const auto& b:OVBase::Ports(other))
            {
                if (!(a.At+a.Direction==b.At) || !(a.Direction+b.Direction==OVBase::Cell{})) continue;
                const auto* socket=asset->FindSocket(SocketName(a.Direction,candidate.Turns));
                const auto* otherSocket=Assets[otherType]->FindSocket(SocketName(b.Direction,other.Turns));
                if (!socket || !otherSocket) { seam=true; continue; }
                const FVector offset=rotation.RotateVector(socket->RelativeLocation * ModuleScale(candidate.Type));
                const FVector origin=transforms[i].TransformPosition(otherSocket->RelativeLocation)-offset;
                if (anchor.IsSet() && FVector::DistSquared(anchor.GetValue(),origin)>25.) seam=true;
                if (!anchor.IsSet()) anchor=origin;
            }
    }
    return FTransform(rotation,anchor.Get(CellCenter(candidate.Origin)),ModuleScale(candidate.Type));
}

OVBase::Error AOVBasePresentation::CheckPlan(const std::vector<OVBase::Module>& plan,OVBase::BuildState* result,FString* blockingName) const
{
    if (plan.empty()) return OVBase::Error::NoConnection;
    auto pending=Build;
    TArray<FTransform> transforms;
    for (const auto& mesh:Meshes) transforms.Add(mesh->GetComponentTransform());
    for (const auto& candidate:plan)
    {
        const auto logical=pending.Check(candidate);
        if (logical!=OVBase::Error::None) return logical;
        bool seam=false;
        const auto transform=PlacementTransform(candidate,pending,transforms,seam);
        if (seam) return OVBase::Error::Seam;
        if (const auto* ship=BlockingShip(candidate,&transform))
        {
            if (blockingName) *blockingName=ship->ShipName;
            return OVBase::Error::Ship;
        }
        if (!pending.Start(candidate)) return OVBase::Error::Outside;
        transforms.Add(transform);
    }
    if (result) *result=std::move(pending);
    return OVBase::Error::None;
}

OVBase::Error AOVBasePresentation::CheckPlacement(const OVBase::Module& candidate,FString* blockingName) const
{
    return CheckPlan({candidate},nullptr,blockingName);
}

std::vector<OVBase::Module> AOVBasePresentation::PlanConnection(const OVBase::Module& candidate) const
{
    using namespace OVBase;
    if (RouteTarget.IsSet() && RouteTarget->Type==candidate.Type && RouteTarget->Origin==candidate.Origin &&
        RouteTarget->Turns==candidate.Turns) return Route;
    RouteTarget=candidate;
    Route.clear();
    if (Build.CheckSpace(candidate)!=OVBase::Error::None || IsLink(candidate.Type)) return {};
    struct Node { Cell At, From; int32 Parent, Steps; };
    const auto key=[](Cell at){return FIntVector(at.X,at.Y,at.Z);};
    TMap<FIntVector,int32> visited;
    TArray<Node> queue;
    const auto reachable=Build.Network(true);
    const auto targetPorts=Ports(candidate);
    const auto add=[&](const Node& node){
        if (visited.Contains(key(node.At)) || Contains(candidate,node.At) || InFlightLane(candidate,node.At) ||
            Build.CheckSpace({Kind::Junction,node.At})!=OVBase::Error::None) return;
        visited.Add(key(node.At),queue.Add(node));
    };
    for (size_t i=0; i<reachable.size(); ++i)
    {
        if (!reachable[i]) continue;
        for (const auto& port:Ports(Build.Modules()[i]))
        {
            const auto at=port.At+port.Direction;
            if (std::none_of(targetPorts.begin(),targetPorts.end(),[&](const auto& target){
                const auto d=at-(target.At+target.Direction);
                return std::abs(d.X)+std::abs(d.Y)+std::abs(d.Z)<16;
            })) continue;
            if (port.Direction.Z && Build.Modules()[i].Turns!=0) continue;
            add({at,port.At,INDEX_NONE,1});
        }
    }
    constexpr Cell directions[]={{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
    for (int32 cursor=0; cursor<queue.Num(); ++cursor)
    {
        const Node node=queue[cursor];
        for (const auto& port:targetPorts)
        {
            if (!(node.At==port.At+port.Direction) || node.From==port.At ||
                (port.Direction.Z && candidate.Turns!=0)) continue;
            std::vector<Module> path;
            Cell next=port.At;
            for (int32 i=cursor; i!=INDEX_NONE; i=queue[i].Parent)
            {
                const auto& link=queue[i];
                const Cell incoming=link.At-link.From;
                const Cell outgoing=next-link.At;
                if (incoming==outgoing && !incoming.Z)
                    path.push_back({Kind::Corridor,link.At,incoming.Y?1:0});
                else
                    path.push_back({Kind::Junction,link.At});
                next=link.At;
            }
            std::reverse(path.begin(),path.end());
            path.push_back(candidate);
            Route=std::move(path);
            return Route;
        }
        if (node.Steps>=16) continue;
        for (const auto& direction:directions)
            add({node.At+direction,node.At,cursor,node.Steps+1});
    }
    return {};
}

bool AOVBasePresentation::ReservePlan(const std::vector<OVBase::Module>& plan)
{
    OVBase::BuildState next;
    if (CheckPlan(plan,&next)!=OVBase::Error::None) return false;
    Build=std::move(next);
    RefreshMeshes();
    return true;
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
    AdvanceOperations(seconds);
    const auto& items=Build.Modules();
    if (std::none_of(items.begin(),items.end(),[](const auto& m){return m.Remaining>0;})) return;
    std::vector<bool> blocked(items.size());
    for (int32 i=1;i<Meshes.Num();++i) blocked[i]=ConstructionBlocker(i)!=nullptr;
    Build.Advance(seconds,blocked);
    UpdateConstructionVisuals();
}


bool AOVBasePresentation::ReserveModule(FName id,FIntVector origin,int32 turns)
{
    for (int32 i=1; i<static_cast<int32>(OVBase::Kind::Count); ++i)
    {
        const auto kind=static_cast<OVBase::Kind>(i);
        if (id==FName(UTF8_TO_TCHAR(OVBase::Def(kind).Key)))
            return ReservePlan({{kind,{origin.X,origin.Y,origin.Z},turns}});
    }
    return false;
}

bool AOVBasePresentation::DemolishModule(int32 index)
{
    if (index<0 || index>=ModuleCount() || InService(index) || !Build.Demolish(index)) return false;
    RefreshMeshes();
    return true;
}

bool AOVBasePresentation::UpgradeModule(int32 index)
{
    return index>=0 && Build.StartUpgrade(index);
}

bool AOVBasePresentation::PrepareTrialTechnology(int32 id)
{
    return !Science.ContainsByPredicate([id](const auto& j){return j.Tech==id;}) && id>0 && id<static_cast<int32>(OVBase::Technology::Count) &&
        Build.UnlockTestTechnology(static_cast<OVBase::Technology>(id));
}

void AOVBasePresentation::SetTrialAccounting(bool bCharge,bool bFill)
{
    Build.SetTestAccounting(bCharge);
    if (bFill) Build.FillTestStock();
}

int32 AOVBasePresentation::ModuleLevel(int32 index) const
{
    return index>=0 && index<ModuleCount()?Build.Modules()[index].Level:0;
}

bool AOVBasePresentation::IsModuleConnected(int32 index) const
{
    return index>=0 && index<ModuleCount() && Build.Network(false)[index];
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
    RouteTarget.Reset();
    Route.clear();
    for (const auto& sleeve:PortSleeves) if (sleeve) sleeve->DestroyComponent();
    PortSleeves.Reset();
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
        if (HasOriginalModel(Item.Type) && !Assets[index])
            Assets[index]=LoadObject<UStaticMesh>(nullptr,MeshesByKind[index],nullptr,LOAD_NoWarn);
        auto* Asset = Assets.IsValidIndex(index)?Assets[index].Get():nullptr;
        if (!Asset)
        {
            Asset = LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
            Mesh->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Presentation/Base/M_PortSteel.M_PortSteel")));
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
        UStaticMeshComponent* sleeve=nullptr;
        if (index>static_cast<int32>(OVBase::Kind::Dock) && Assets[index])
        {
            if (!SleeveAssets[index])
            {
                FString path(MeshesByKind[index]);
                path.ReplaceInline(TEXT("SM_OV_Base_"),TEXT("SM_OV_Ports_"));
                SleeveAssets[index]=LoadObject<UStaticMesh>(nullptr,*path);
            }
            if (SleeveAssets[index])
            {
                sleeve=NewObject<UStaticMeshComponent>(this);
                sleeve->SetupAttachment(Mesh);
                sleeve->SetStaticMesh(SleeveAssets[index]);
                sleeve->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                sleeve->RegisterComponent();
            }
        }
        PortSleeves.Add(sleeve);
        if (Asset)
        {
            Mesh->SetCollisionProfileName(TEXT("BlockAll"));
            Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
            Mesh->SetCollisionResponseToChannel(ECC_GameTraceChannel18,ECR_Block);
            if (OVBase::HasBay(Item.Type)) Mesh->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
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
        if (OVBase::Completed(item))
        {
            mesh->SetCustomPrimitiveDataFloat(1,0.f);
            if (PortSleeves[i]) PortSleeves[i]->SetVisibility(true);
            continue;
        }
        const double progress=1.-item.Remaining/OVBase::Duration(item.Type);
        const auto box=mesh->Bounds.GetBox();
        const double top=FMath::Lerp(box.Min.Z,box.Max.Z,.015+.985*progress);
        mesh->SetCustomPrimitiveDataFloat(0,top);
        if (PortSleeves[i]) PortSleeves[i]->SetVisibility(top>=PortSleeves[i]->Bounds.Origin.Z);
        mesh->SetCustomPrimitiveDataFloat(1,1.f);
    }
}
