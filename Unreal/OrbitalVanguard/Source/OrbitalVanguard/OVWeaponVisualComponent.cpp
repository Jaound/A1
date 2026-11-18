#include "OVWeaponVisualComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "HAL/IConsoleManager.h"

static TAutoConsoleVariable<int32> CVarCIWSBTrial(TEXT("ov.Presentation.CIWSBTrial"), 0,
    TEXT("Opt-in isolated CIWS-B articulation trial; not final artwork."));

namespace OVWeaponGeometry
{
FVector SurfaceNormal(const FOVWeaponMountDefinition& Definition)
{
    if (!Definition.LocalSurfaceNormal.ContainsNaN() && !Definition.LocalSurfaceNormal.IsNearlyZero())
        return Definition.LocalSurfaceNormal.GetSafeNormal();
    return Definition.LocalPosition.Z < 0 ? FVector::DownVector : FVector::UpVector;
}
float BarrelLength(const FVector& HullHalfExtent, float Tier, bool bPointDefense)
{
    // Display geometry only; no range/damage/speed or hit-volume multiplier.
    const float Scale = FMath::Clamp(float(HullHalfExtent.GetMin()) / 30.f, 1.f, 12.f);
    return (bPointDefense ? 16.f : 26.f + FMath::Max(0.f, Tier - 1.f) * 3.f) * Scale;
}

FVector MuzzleLocal(FVector MountPosition, FRotator Aim, float Length, bool bFixedVLS)
{
    return MountPosition + (bFixedVLS ? FVector(0, 0, MountPosition.Z < 0 ? -1 : 1) : Aim.Vector()) * Length;
}

bool HasBF120Trial(const FOVWeaponMountDefinition& Definition)
{
    if (Definition.Type != EOVWeaponType::Cannon || Definition.PresentationProfile != TEXT("BF120Trial")) return false;
    if (FMath::Abs(SurfaceNormal(Definition).Z)<.999) return false;
    static TWeakObjectPtr<UStaticMesh> Base, Body, Pitch;
    if (!Base.IsValid()) Base = LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Presentation/BF120Trial/SM_BF120_BaseTrial.SM_BF120_BaseTrial"),nullptr,LOAD_NoWarn);
    if (!Body.IsValid()) Body = LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Presentation/BF120Trial/SM_BF120_YawBody.SM_BF120_YawBody"),nullptr,LOAD_NoWarn);
    if (!Pitch.IsValid()) Pitch = LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Presentation/BF120Trial/SM_BF120_PitchTrial.SM_BF120_PitchTrial"),nullptr,LOAD_NoWarn);
    return Base.IsValid() && Body.IsValid() && Pitch.IsValid();
}

bool CIWSBTrialEnabled() { return CVarCIWSBTrial.GetValueOnGameThread() != 0; }
bool HasCIWSBTrial(const FOVWeaponMountDefinition& Definition)
{
    if (Definition.Type != EOVWeaponType::Interception || Definition.PresentationProfile != TEXT("CIWSBTrial")) return false;
    static TWeakObjectPtr<UStaticMesh> Base, Body, Barrel, Neck;
    if (!Base.IsValid()) Base=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Presentation/WeaponsYawTrial/ciws_beam_b/SM_Base.SM_Base"),nullptr,LOAD_NoWarn);
    if (!Body.IsValid()) Body=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Presentation/WeaponsPitchTrial/ciws_beam_b/SM_Body.SM_Body"),nullptr,LOAD_NoWarn);
    if (!Barrel.IsValid()) Barrel=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Presentation/WeaponsPitchTrial/ciws_beam_b/SM_Barrel.SM_Barrel"),nullptr,LOAD_NoWarn);
    if (!Neck.IsValid()) Neck=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Presentation/WeaponsPitchTrial/ciws_beam_b/SM_TrialNeck.SM_TrialNeck"),nullptr,LOAD_NoWarn);
    return Base.IsValid() && Body.IsValid() && Barrel.IsValid() && Neck.IsValid();
}

FVector MuzzleLocal(const FOVWeaponMountDefinition& Definition, FRotator Aim,
    FVector HullHalfExtent, bool bFixedVLS, int32 BarrelIndex)
{
    const float Length=BarrelLength(HullHalfExtent,Definition.Tier,Definition.Type==EOVWeaponType::Interception);
    if (bFixedVLS) return Definition.LocalPosition + SurfaceNormal(Definition)*Length;
    if (HasCIWSBTrial(Definition))
    {
        // Isolated source-derived centimetres. The outlet is a candidate, not approved final art.
        const FVector Normal=SurfaceNormal(Definition);
        const bool bSide=FMath::Abs(Normal.Z)<.999, bBottom=!bSide&&Normal.Z<0;
        const FQuat Surface=bSide?FRotationMatrix::MakeFromZ(Normal).ToQuat():FQuat::Identity;
        const FRotator LocalAim=bSide?Surface.UnrotateVector(Aim.Vector()).Rotation():Aim;
        const FQuat Flip=FRotator(0,0,bBottom?180.f:0.f).Quaternion();
        const FQuat Yaw=FRotator(0,bBottom?180.f-LocalAim.Yaw:180.f+LocalAim.Yaw,0).Quaternion();
        const FQuat Pitch=FRotator(bBottom?LocalAim.Pitch:-LocalAim.Pitch,0,0).Quaternion();
        const double Scale=Length/164.4709;
        const FVector Lift(0,0,(bBottom?-1.f:1.f)*56.99999*Scale);
        const FVector Pivot(-70.92006,-.03419,53.0000), Tip(-93.55087,0,.96070);
        return Definition.LocalPosition+Surface.RotateVector(Lift+Flip.RotateVector(FVector(0,0,.1*Scale)+Yaw.RotateVector((Pivot+Pitch.RotateVector(Tip))*Scale)));
    }
    if (!HasBF120Trial(Definition)) return MuzzleLocal(Definition.LocalPosition,Aim,Length,false);
    // Measured isolated UE trial, cm. One projectile uses one barrel; shots alternate without changing cadence or damage.
    const FVector YawOrigin(99.5,2.5,-41),PitchOrigin(8,0,50);
    const FVector Tips[]={FVector(-241.143,41.673,49.709),FVector(-241.167,-42.358,49.477)};
    const bool bBottom=SurfaceNormal(Definition).Z<0;
    const FQuat Flip=FRotator(0,0,bBottom?180.f:0.f).Quaternion();
    const FQuat Yaw=FRotator(0,bBottom?180.f-Aim.Yaw:180.f+Aim.Yaw,0).Quaternion();
    const FQuat Pitch=FRotator(bBottom?Aim.Pitch:-Aim.Pitch,0,0).Quaternion();
    const double Scale=Length/249.143;
    const FVector InYaw=(PitchOrigin-YawOrigin)+Pitch.RotateVector(Tips[BarrelIndex&1]-PitchOrigin);
    return Definition.LocalPosition+Flip.RotateVector(Yaw.RotateVector(InYaw))*Scale;
}
}

UOVWeaponVisualComponent::UOVWeaponVisualComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetMobility(EComponentMobility::Movable);
}

USceneComponent* UOVWeaponVisualComponent::MakePivot(USceneComponent* Parent)
{
    USceneComponent* Part = NewObject<USceneComponent>(GetOwner());
    GetOwner()->AddInstanceComponent(Part);
    Part->SetMobility(EComponentMobility::Movable);
    Part->SetupAttachment(Parent);
    Part->RegisterComponent();
    Parts.Add(Part);
    return Part;
}

UStaticMeshComponent* UOVWeaponVisualComponent::MakeMesh(USceneComponent* Parent, const TCHAR* Asset,
    FVector Position, FVector Scale, FRotator Rotation, bool bUseBodyMaterial)
{
    UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(GetOwner());
    GetOwner()->AddInstanceComponent(Part);
    Part->SetMobility(EComponentMobility::Movable);
    Part->SetupAttachment(Parent);
    Part->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, Asset));
    Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Part->SetCollisionResponseToAllChannels(ECR_Ignore);
    Part->SetGenerateOverlapEvents(false);
    Part->SetCanEverAffectNavigation(false);
    Part->SetCastShadow(false);
    Part->SetRelativeLocation(Position);
    Part->SetRelativeRotation(Rotation);
    Part->SetRelativeScale3D(Scale);
    if (BodyMaterial && bUseBodyMaterial) Part->SetMaterial(0, BodyMaterial);
    Part->RegisterComponent();
    Parts.Add(Part);
    return Part;
}

void UOVWeaponVisualComponent::Initialize(const FOVWeaponMountDefinition& InDefinition, FVector HullHalfExtent)
{
    Definition = InDefinition;
    Extent = HullHalfExtent;
    State.MountId = Definition.WeaponId;
    State.bPointDefense = Definition.Type == EOVWeaponType::Interception;
    const FVector Normal=OVWeaponGeometry::SurfaceNormal(Definition);
    const bool bSide=FMath::Abs(Normal.Z)<.999;
    State.bBottom = !bSide && Normal.Z < 0;
    Length = OVWeaponGeometry::BarrelLength(Extent, Definition.Tier, State.bPointDefense);
    SetRelativeLocation(Definition.LocalPosition);
    if(bSide) SetRelativeRotation(FRotationMatrix::MakeFromZ(Normal).Rotator());
    const float Side = State.bBottom ? -1.f : 1.f;
    UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    if (Base) BodyMaterial = UMaterialInstanceDynamic::Create(Base, this);

    if (OVWeaponGeometry::HasCIWSBTrial(Definition))
    {
        // Trial only: source base, body and barrel plus a clearly separate generated neck.
        const double Scale=Length/164.4709;
        const FVector Pivot(-70.92006,-.03419,53.0000), Tip(-93.55087,0,.96070);
        auto* Flip=MakePivot(this);
        Flip->SetRelativeLocation(FVector(0,0,Side*56.99999*Scale));
        Flip->SetRelativeRotation(FRotator(0,0,State.bBottom?180.f:0.f));
        MakeMesh(Flip,TEXT("/Game/Presentation/WeaponsYawTrial/ciws_beam_b/SM_Base.SM_Base"),FVector::ZeroVector,FVector(Scale),FRotator::ZeroRotator,false);
        YawPivot=MakePivot(Flip);
        YawPivot->SetRelativeLocation(FVector(0,0,.1*Scale)); // 0.001 Blender-unit trial bearing clearance.
        MakeMesh(YawPivot,TEXT("/Game/Presentation/WeaponsPitchTrial/ciws_beam_b/SM_Body.SM_Body"),FVector::ZeroVector,FVector(Scale),FRotator::ZeroRotator,false);
        PitchPivot=MakePivot(YawPivot);PitchPivot->SetRelativeLocation(Pivot*Scale);
        Barrel=MakeMesh(PitchPivot,TEXT("/Game/Presentation/WeaponsPitchTrial/ciws_beam_b/SM_Barrel.SM_Barrel"),FVector::ZeroVector,FVector(Scale),FRotator::ZeroRotator,false);
        auto* Neck=MakeMesh(PitchPivot,TEXT("/Game/Presentation/WeaponsPitchTrial/ciws_beam_b/SM_TrialNeck.SM_TrialNeck"),FVector(7,0,0)*Scale,FVector(Scale),FRotator::ZeroRotator,false);
        if (Base && Neck) { auto* M=UMaterialInstanceDynamic::Create(Base,Neck); M->SetVectorParameterValue(TEXT("Color"),FLinearColor(.19f,.2f,.21f)); Neck->SetMaterial(0,M); }
        Flash=MakeMesh(PitchPivot,TEXT("/Engine/BasicShapes/Sphere.Sphere"),Tip*Scale,FVector(Length*.15/100.f));
        if (Base && Flash) { auto* M=UMaterialInstanceDynamic::Create(Base,Flash); M->SetVectorParameterValue(TEXT("Color"),FLinearColor(.12f,.85f,1.f)); Flash->SetMaterial(0,M); }
        if (Flash) Flash->SetVisibility(false);
        return;
    }
    if (OVWeaponGeometry::HasBF120Trial(Definition))
    {
        // Isolated trial measurements; source meshes bake their original world offsets.
        const FVector YawOrigin(99.5,2.5,-41),PitchOrigin(8,0,50),Clearance(0,0,1);
        const FVector Tips[]={FVector(-241.143,41.673,49.709),FVector(-241.167,-42.358,49.477)};
        const double Scale=Length/249.143;
        auto* Flip=MakePivot(this);Flip->SetRelativeRotation(FRotator(0,0,State.bBottom?180.f:0.f));
        MakeMesh(Flip,TEXT("/Game/Presentation/BF120Trial/SM_BF120_BaseTrial.SM_BF120_BaseTrial"),-YawOrigin*Scale,FVector(Scale),FRotator::ZeroRotator,false);
        YawPivot=MakePivot(Flip);
        MakeMesh(YawPivot,TEXT("/Game/Presentation/BF120Trial/SM_BF120_YawBody.SM_BF120_YawBody"),(-YawOrigin+Clearance)*Scale,FVector(Scale),FRotator::ZeroRotator,false);
        PitchPivot=MakePivot(YawPivot);PitchPivot->SetRelativeLocation((PitchOrigin-YawOrigin)*Scale);
        Barrel=MakeMesh(PitchPivot,TEXT("/Game/Presentation/BF120Trial/SM_BF120_PitchTrial.SM_BF120_PitchTrial"),(-PitchOrigin+Clearance)*Scale,FVector(Scale),FRotator::ZeroRotator,false);
        Flash=MakeMesh(PitchPivot,TEXT("/Engine/BasicShapes/Sphere.Sphere"),(Tips[0]-PitchOrigin)*Scale,FVector(Length*.15/100.f));
        SecondFlash=MakeMesh(PitchPivot,TEXT("/Engine/BasicShapes/Sphere.Sphere"),(Tips[1]-PitchOrigin)*Scale,FVector(Length*.15/100.f));
        UStaticMeshComponent* FlashParts[]={Flash.Get(),SecondFlash.Get()};
        for (auto* Part : FlashParts) if (Part) {
            if (Base) {auto* Material=UMaterialInstanceDynamic::Create(Base,Part);Material->SetVectorParameterValue(TEXT("Color"),FLinearColor(1.f,.66f,.08f));Part->SetMaterial(0,Material);}
            Part->SetVisibility(false);
        }
        return;
    }
    // Fixed plinth remains level with the hull. Only the trunnion and barrel elevate.
    MakeMesh(this, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"), FVector(0, 0, -Side * Length * .23),
        FVector(Length * .8, Length * .8, Length * .18) / 100.f);
    YawPivot = MakePivot(this);
    MakeMesh(YawPivot, TEXT("/Engine/BasicShapes/Cube.Cube"), FVector(-Length * .1, 0, -Side * Length * .08),
        FVector(Length * .5, Length * .52, Length * .28) / 100.f);
    PitchPivot = MakePivot(YawPivot);
    Barrel = MakeMesh(PitchPivot, TEXT("/Engine/BasicShapes/Cube.Cube"), FVector(Length * .5, 0, 0),
        FVector(Length, Length * (Definition.Type == EOVWeaponType::Beam ? .24 : .13), Length * .13) / 100.f);
    // The visible outlet coincides exactly with the shared gameplay muzzle.
    MakeMesh(PitchPivot, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"), FVector(Length, 0, 0),
        FVector(Length * .22, Length * .22, Length * .07) / 100.f, FRotator(90, 0, 0));
    Flash = MakeMesh(PitchPivot, TEXT("/Engine/BasicShapes/Sphere.Sphere"), FVector(Length, 0, 0), FVector(Length * .30 / 100.f));
    if (Base)
    {
        UMaterialInstanceDynamic* FlashMaterial = UMaterialInstanceDynamic::Create(Base, Flash);
        FlashMaterial->SetVectorParameterValue(TEXT("Color"), Definition.Type == EOVWeaponType::Beam
            ? FLinearColor(.12f, .85f, 1.f) : FLinearColor(1.f, .66f, .08f));
        Flash->SetMaterial(0, FlashMaterial);
    }
    Flash->SetVisibility(false);
}

bool UOVWeaponVisualComponent::Matches(const FOVWeaponMountDefinition& Other, FVector HullHalfExtent) const
{
    return Definition.WeaponId == Other.WeaponId && Definition.Type == Other.Type
        && Definition.Tier == Other.Tier && Definition.PresentationProfile==Other.PresentationProfile && Definition.LocalPosition.Equals(Other.LocalPosition)
        && Definition.LocalSurfaceNormal.Equals(Other.LocalSurfaceNormal)
        && Extent.Equals(HullHalfExtent);
}

void UOVWeaponVisualComponent::Synchronize(FRotator LogicalAim, bool bFixedVLS, bool bDestroyed, bool bFriendly,
    int32 ShotId, double ShotTime, double GameTime, int32 ShotsFired)
{
    State.LogicalAim = LogicalAim;
    State.bFixedVLS = bFixedVLS;
    State.bDestroyed = bDestroyed;
    const FVector Normal=OVWeaponGeometry::SurfaceNormal(Definition);
    const bool bSide=FMath::Abs(Normal.Z)<.999;
    State.VisualAim = bSide ? (bFixedVLS ? FRotator(90,0,0) : GetRelativeRotation().UnrotateVector(LogicalAim.Vector()).Rotation())
        : (bFixedVLS ? FRotator(State.bBottom ? -90 : 90, 0, 0) : LogicalAim);
    const bool bBF120=OVWeaponGeometry::HasBF120Trial(Definition)&&!bFixedVLS;
    const bool bCIWSB=OVWeaponGeometry::HasCIWSBTrial(Definition)&&!bFixedVLS;
    const bool bSource=bBF120||bCIWSB;
    if (YawPivot) YawPivot->SetRelativeRotation(FRotator(0,bSource?(State.bBottom?180.f-State.VisualAim.Yaw:180.f+State.VisualAim.Yaw):State.VisualAim.Yaw,0));
    if (PitchPivot) PitchPivot->SetRelativeRotation(FRotator(bSource?(State.bBottom?State.VisualAim.Pitch:-State.VisualAim.Pitch):State.VisualAim.Pitch,0,0));
    const int32 BarrelIndex=ShotsFired>0?(ShotsFired-1)%2:0;
    State.LocalMuzzle = bCIWSB ? OVWeaponGeometry::MuzzleLocal(Definition,LogicalAim,Extent,false,0) : State.bPointDefense
        ? Definition.LocalPosition+(bFixedVLS?Normal:LogicalAim.Vector())*Length
        : OVWeaponGeometry::MuzzleLocal(Definition,LogicalAim,Extent,bFixedVLS,BarrelIndex);
    State.LastShotId = ShotId;
    State.LastShotTime = ShotTime;
    // Game time keeps a paused muzzle flash frozen. Opening a viewer cannot fire or re-trigger it.
    State.bFlashVisible = !bDestroyed && ShotId > 0 && ShotTime >= 0 && GameTime >= ShotTime && GameTime - ShotTime < .12;
    if (Flash) Flash->SetVisibility(State.bFlashVisible&&(!bBF120||BarrelIndex==0));
    if (SecondFlash) SecondFlash->SetVisibility(State.bFlashVisible&&bBF120&&BarrelIndex==1);
    if (Barrel) Barrel->SetVisibility(!bDestroyed);
    if (BodyMaterial && (!bColorInitialized || bLastDestroyed != bDestroyed || bLastFriendly != bFriendly))
    {
        BodyMaterial->SetVectorParameterValue(TEXT("Color"), bDestroyed ? FLinearColor(.15f, .12f, .12f)
            : bFriendly ? FLinearColor(.42f, .74f, .91f) : FLinearColor(.92f, .43f, .28f));
        bColorInitialized = true; bLastDestroyed = bDestroyed; bLastFriendly = bFriendly;
    }
}

FOVWeaponVisualState UOVWeaponVisualComponent::ReadState() const
{
    FOVWeaponVisualState Result = State;
    Result.WorldMuzzle = GetOwner()->GetActorTransform().TransformPosition(State.LocalMuzzle);
    return Result;
}

void UOVWeaponVisualComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
    for (int32 I = Parts.Num() - 1; I >= 0; --I)
        if (IsValid(Parts[I])) Parts[I]->DestroyComponent();
    Parts.Reset();
    Super::OnComponentDestroyed(bDestroyingHierarchy);
}
