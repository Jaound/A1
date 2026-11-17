#include "OVWeaponVisualComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace OVWeaponGeometry
{
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
    FVector Position, FVector Scale, FRotator Rotation)
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
    if (BodyMaterial) Part->SetMaterial(0, BodyMaterial);
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
    State.bBottom = Definition.LocalPosition.Z < 0;
    Length = OVWeaponGeometry::BarrelLength(Extent, Definition.Tier, State.bPointDefense);
    SetRelativeLocation(Definition.LocalPosition);
    const float Side = State.bBottom ? -1.f : 1.f;
    UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    if (Base) BodyMaterial = UMaterialInstanceDynamic::Create(Base, this);

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
        && Definition.Tier == Other.Tier && Definition.LocalPosition.Equals(Other.LocalPosition)
        && Extent.Equals(HullHalfExtent);
}

void UOVWeaponVisualComponent::Synchronize(FRotator LogicalAim, bool bFixedVLS, bool bDestroyed, bool bFriendly,
    int32 ShotId, double ShotTime, double GameTime)
{
    State.LogicalAim = LogicalAim;
    State.bFixedVLS = bFixedVLS;
    State.bDestroyed = bDestroyed;
    State.VisualAim = bFixedVLS ? FRotator(State.bBottom ? -90 : 90, 0, 0) : LogicalAim;
    if (YawPivot) YawPivot->SetRelativeRotation(FRotator(0, State.VisualAim.Yaw, 0));
    if (PitchPivot) PitchPivot->SetRelativeRotation(FRotator(State.VisualAim.Pitch, 0, 0));
    State.LocalMuzzle = OVWeaponGeometry::MuzzleLocal(Definition.LocalPosition, LogicalAim, Length, bFixedVLS);
    State.LastShotId = ShotId;
    State.LastShotTime = ShotTime;
    // Game time keeps a paused muzzle flash frozen. Opening a viewer cannot fire or re-trigger it.
    State.bFlashVisible = !bDestroyed && ShotId > 0 && ShotTime >= 0 && GameTime >= ShotTime && GameTime - ShotTime < .12;
    if (Flash) Flash->SetVisibility(State.bFlashVisible);
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
