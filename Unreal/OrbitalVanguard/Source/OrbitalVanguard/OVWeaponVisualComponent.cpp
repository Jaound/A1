#include "OVWeaponVisualComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "HAL/IConsoleManager.h"

static TAutoConsoleVariable<int32> CVarCIWSBTrial(TEXT("ov.Presentation.CIWSBTrial"), 0,
    TEXT("Opt-in isolated source point-defense articulation trials; not final artwork."));
static TAutoConsoleVariable<int32> CVarLB30Trial(TEXT("ov.Presentation.LB30Trial"), 0,
    TEXT("Opt-in isolated source beam turret trial; not final artwork."));
static TAutoConsoleVariable<int32> CVarBF120SourceTrial(TEXT("ov.Presentation.BF120SourceTrial"), 0,
    TEXT("Opt-in isolated source twin cannon trial; not final artwork."));
static TAutoConsoleVariable<int32> CVarSHC400Trial(TEXT("ov.Presentation.SHC400Trial"), 0,
    TEXT("Opt-in isolated source heavy cannon trial; not final artwork."));
static TAutoConsoleVariable<int32> CVarRG200Trial(TEXT("ov.Presentation.RG200Trial"), 0,
    TEXT("Opt-in isolated source railgun trial; not final artwork."));
static TAutoConsoleVariable<int32> CVarPlasmaTrial(TEXT("ov.Presentation.PlasmaTrial"), 0,
    TEXT("Opt-in isolated source plasma visual trial; no special combat rules."));

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
bool HasCIWS6Trial(const FOVWeaponMountDefinition& Definition)
{
    if (Definition.Type != EOVWeaponType::Interception || Definition.PresentationProfile != TEXT("CIWS6Trial")) return false;
    static TWeakObjectPtr<UStaticMesh> Base, Body, Barrel, Neck;
    if (!Base.IsValid()) Base=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Presentation/WeaponsYawTrial/ciws_gatling_6/SM_Base.SM_Base"),nullptr,LOAD_NoWarn);
    if (!Body.IsValid()) Body=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Presentation/WeaponsPitchTrial/ciws_gatling_6/SM_Body.SM_Body"),nullptr,LOAD_NoWarn);
    if (!Barrel.IsValid()) Barrel=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Presentation/WeaponsPitchTrial/ciws_gatling_6/SM_Barrel.SM_Barrel"),nullptr,LOAD_NoWarn);
    if (!Neck.IsValid()) Neck=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Presentation/WeaponsPitchTrial/ciws_gatling_6/SM_TrialNeck.SM_TrialNeck"),nullptr,LOAD_NoWarn);
    return Base.IsValid() && Body.IsValid() && Barrel.IsValid() && Neck.IsValid();
}
bool HasSourcePDTrial(const FOVWeaponMountDefinition& Definition)
{
    return HasCIWSBTrial(Definition)||HasCIWS6Trial(Definition);
}
bool LB30TrialEnabled() { return CVarLB30Trial.GetValueOnGameThread()!=0; }
bool HasLB30Trial(const FOVWeaponMountDefinition& Definition)
{
    if (Definition.Type!=EOVWeaponType::Beam || Definition.PresentationProfile!=TEXT("LB30Trial")) return false;
    static TWeakObjectPtr<UStaticMesh> Base, Body, Barrel, Neck;
    if (!Base.IsValid()) Base=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Presentation/WeaponsYawTrial/laser_turret_lb30/SM_Base.SM_Base"),nullptr,LOAD_NoWarn);
    if (!Body.IsValid()) Body=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Presentation/WeaponsPitchTrial/laser_turret_lb30/SM_Body.SM_Body"),nullptr,LOAD_NoWarn);
    if (!Barrel.IsValid()) Barrel=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Presentation/WeaponsPitchTrial/laser_turret_lb30/SM_Barrel.SM_Barrel"),nullptr,LOAD_NoWarn);
    if (!Neck.IsValid()) Neck=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Presentation/WeaponsPitchTrial/laser_turret_lb30/SM_TrialNeck.SM_TrialNeck"),nullptr,LOAD_NoWarn);
    return Base.IsValid()&&Body.IsValid()&&Barrel.IsValid()&&Neck.IsValid();
}
bool BF120SourceTrialEnabled() { return CVarBF120SourceTrial.GetValueOnGameThread()!=0; }
bool HasBF120SourceTrial(const FOVWeaponMountDefinition& Definition)
{
    if (Definition.Type!=EOVWeaponType::Cannon || Definition.PresentationProfile!=TEXT("BF120SourceTrial")) return false;
    static TWeakObjectPtr<UStaticMesh> Base, Body, Barrel, Neck;
    if (!Base.IsValid()) Base=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Presentation/WeaponsYawTrial/naval_gun_bf120/SM_Base.SM_Base"),nullptr,LOAD_NoWarn);
    if (!Body.IsValid()) Body=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Presentation/WeaponsPitchTrial/naval_gun_bf120/SM_Body.SM_Body"),nullptr,LOAD_NoWarn);
    if (!Barrel.IsValid()) Barrel=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Presentation/WeaponsPitchTrial/naval_gun_bf120/SM_Barrel.SM_Barrel"),nullptr,LOAD_NoWarn);
    if (!Neck.IsValid()) Neck=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Presentation/WeaponsPitchTrial/naval_gun_bf120/SM_TrialNeck.SM_TrialNeck"),nullptr,LOAD_NoWarn);
    return Base.IsValid()&&Body.IsValid()&&Barrel.IsValid()&&Neck.IsValid();
}
bool HasSHC400Trial(const FOVWeaponMountDefinition& Definition)
{
    if (Definition.Type!=EOVWeaponType::Cannon || Definition.PresentationProfile!=TEXT("SHC400Trial")) return false;
    static TWeakObjectPtr<UStaticMesh> Base, Body, Barrel, Neck;
    if (!Base.IsValid()) Base=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Presentation/WeaponsYawTrial/heavy_cannon_shc400/SM_Base.SM_Base"),nullptr,LOAD_NoWarn);
    if (!Body.IsValid()) Body=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Presentation/WeaponsPitchTrial/heavy_cannon_shc400/SM_Body.SM_Body"),nullptr,LOAD_NoWarn);
    if (!Barrel.IsValid()) Barrel=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Presentation/WeaponsPitchTrial/heavy_cannon_shc400/SM_Barrel.SM_Barrel"),nullptr,LOAD_NoWarn);
    if (!Neck.IsValid()) Neck=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Presentation/WeaponsPitchTrial/heavy_cannon_shc400/SM_TrialNeck.SM_TrialNeck"),nullptr,LOAD_NoWarn);
    return Base.IsValid()&&Body.IsValid()&&Barrel.IsValid()&&Neck.IsValid();
}
bool HasRG200Trial(const FOVWeaponMountDefinition& Definition)
{
    if (Definition.Type!=EOVWeaponType::Cannon || Definition.PresentationProfile!=TEXT("RG200Trial")) return false;
    static TWeakObjectPtr<UStaticMesh> Base, Body, Barrel, Neck;
    if (!Base.IsValid()) Base=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Presentation/WeaponsYawTrial/railgun_rg200/SM_Base.SM_Base"),nullptr,LOAD_NoWarn);
    if (!Body.IsValid()) Body=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Presentation/WeaponsPitchTrial/railgun_rg200/SM_Body.SM_Body"),nullptr,LOAD_NoWarn);
    if (!Barrel.IsValid()) Barrel=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Presentation/WeaponsPitchTrial/railgun_rg200/SM_Barrel.SM_Barrel"),nullptr,LOAD_NoWarn);
    if (!Neck.IsValid()) Neck=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Presentation/WeaponsPitchTrial/railgun_rg200/SM_TrialNeck.SM_TrialNeck"),nullptr,LOAD_NoWarn);
    return Base.IsValid()&&Body.IsValid()&&Barrel.IsValid()&&Neck.IsValid();
}
bool HasPlasmaTrial(const FOVWeaponMountDefinition& Definition)
{
    if (Definition.Type!=EOVWeaponType::Beam || Definition.PresentationProfile!=TEXT("PlasmaTrial")) return false;
    static TWeakObjectPtr<UStaticMesh> Base, Body, Barrel, Neck;
    if (!Base.IsValid()) Base=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Presentation/WeaponsYawTrial/plasma_heavy_cannon/SM_Base.SM_Base"),nullptr,LOAD_NoWarn);
    if (!Body.IsValid()) Body=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Presentation/WeaponsPitchTrial/plasma_heavy_cannon/SM_Body.SM_Body"),nullptr,LOAD_NoWarn);
    if (!Barrel.IsValid()) Barrel=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Presentation/WeaponsPitchTrial/plasma_heavy_cannon/SM_Barrel.SM_Barrel"),nullptr,LOAD_NoWarn);
    if (!Neck.IsValid()) Neck=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Presentation/WeaponsPitchTrial/plasma_heavy_cannon/SM_TrialNeck.SM_TrialNeck"),nullptr,LOAD_NoWarn);
    return Base.IsValid()&&Body.IsValid()&&Barrel.IsValid()&&Neck.IsValid();
}
FName CannonSourceTrialProfile(const FOVWeaponMountDefinition& Definition)
{
    if (Definition.Type!=EOVWeaponType::Cannon || !Definition.PresentationProfile.IsNone()) return Definition.PresentationProfile;
    const bool bBF=BF120SourceTrialEnabled(), bSHC=CVarSHC400Trial.GetValueOnGameThread()!=0, bRG=CVarRG200Trial.GetValueOnGameThread()!=0;
    if (bRG && (!bBF || Definition.Tier>=2) && (!bSHC || Definition.Tier==2)) return TEXT("RG200Trial");
    if (bSHC&&(!bBF||Definition.Tier>=2)) return TEXT("SHC400Trial");
    return bBF?FName(TEXT("BF120SourceTrial")):NAME_None;
}
FName BeamSourceTrialProfile(const FOVWeaponMountDefinition& Definition)
{
    if (Definition.Type!=EOVWeaponType::Beam || !Definition.PresentationProfile.IsNone()) return Definition.PresentationProfile;
    const bool bLB=LB30TrialEnabled(), bPlasma=CVarPlasmaTrial.GetValueOnGameThread()!=0;
    if (bPlasma && (!bLB || Definition.Tier>=2)) return TEXT("PlasmaTrial");
    return bLB ? FName(TEXT("LB30Trial")) : NAME_None;
}

struct FSourceTrialArt
{
    double Span, BaseDepth;
    FVector Pivot, Tip[2], NeckOffset;
    const TCHAR* Base; const TCHAR* Body; const TCHAR* Barrel; const TCHAR* Neck;
    bool bTwin;
};

static bool SourceTrialArt(const FOVWeaponMountDefinition& Definition,FSourceTrialArt& Out)
{
    if (HasRG200Trial(Definition))
    {
        Out={593.592,109.5,FVector(-172.154,.07683,87.5),
             {FVector(-421.438,62.560,-5.849),FVector(-421.439,-63.009,-5.877)},FVector(10,55,0),
             TEXT("/Game/Presentation/WeaponsYawTrial/railgun_rg200/SM_Base.SM_Base"),
             TEXT("/Game/Presentation/WeaponsPitchTrial/railgun_rg200/SM_Body.SM_Body"),
             TEXT("/Game/Presentation/WeaponsPitchTrial/railgun_rg200/SM_Barrel.SM_Barrel"),
             TEXT("/Game/Presentation/WeaponsPitchTrial/railgun_rg200/SM_TrialNeck.SM_TrialNeck"),true};
        return true;
    }
    if (HasPlasmaTrial(Definition))
    {
        Out={422.18,71.25,FVector(-175.235,-.00235,78.75),
             {FVector(-246.94,4.956,-2.45),FVector::ZeroVector},FVector(15,0,0),
             TEXT("/Game/Presentation/WeaponsYawTrial/plasma_heavy_cannon/SM_Base.SM_Base"),
             TEXT("/Game/Presentation/WeaponsPitchTrial/plasma_heavy_cannon/SM_Body.SM_Body"),
             TEXT("/Game/Presentation/WeaponsPitchTrial/plasma_heavy_cannon/SM_Barrel.SM_Barrel"),
             TEXT("/Game/Presentation/WeaponsPitchTrial/plasma_heavy_cannon/SM_TrialNeck.SM_TrialNeck"),false};
        return true;
    }
    if (HasSHC400Trial(Definition))
    {
        Out={241.34561,73.749985,FVector(-76.21249,.072968,70.25),
             {FVector(-165.13312,-.17361,-.88664),FVector::ZeroVector},FVector(15,0,0),
             TEXT("/Game/Presentation/WeaponsYawTrial/heavy_cannon_shc400/SM_Base.SM_Base"),
             TEXT("/Game/Presentation/WeaponsPitchTrial/heavy_cannon_shc400/SM_Body.SM_Body"),
             TEXT("/Game/Presentation/WeaponsPitchTrial/heavy_cannon_shc400/SM_Barrel.SM_Barrel"),
             TEXT("/Game/Presentation/WeaponsPitchTrial/heavy_cannon_shc400/SM_TrialNeck.SM_TrialNeck"),false};
        return true;
    }
    if (HasBF120SourceTrial(Definition))
    {
        Out={342.3109,77.00002,FVector(-110.15539,-.03919,71.5),
             {FVector(-232.13071,41.70198,.21027),FVector(-232.15546,-42.35428,-.02401)},FVector(7,42,0),
             TEXT("/Game/Presentation/WeaponsYawTrial/naval_gun_bf120/SM_Base.SM_Base"),
             TEXT("/Game/Presentation/WeaponsPitchTrial/naval_gun_bf120/SM_Body.SM_Body"),
             TEXT("/Game/Presentation/WeaponsPitchTrial/naval_gun_bf120/SM_Barrel.SM_Barrel"),
             TEXT("/Game/Presentation/WeaponsPitchTrial/naval_gun_bf120/SM_TrialNeck.SM_TrialNeck"),true};
        return true;
    }
    if (HasLB30Trial(Definition))
    {
        Out={324.63175,62.999996,FVector(-148.305,4.37306,82.0),
             {FVector(-176.32675,-.21687,-3.48731),FVector::ZeroVector},FVector(15,0,0),
             TEXT("/Game/Presentation/WeaponsYawTrial/laser_turret_lb30/SM_Base.SM_Base"),
             TEXT("/Game/Presentation/WeaponsPitchTrial/laser_turret_lb30/SM_Body.SM_Body"),
             TEXT("/Game/Presentation/WeaponsPitchTrial/laser_turret_lb30/SM_Barrel.SM_Barrel"),
             TEXT("/Game/Presentation/WeaponsPitchTrial/laser_turret_lb30/SM_TrialNeck.SM_TrialNeck"),false};
        return true;
    }
    if (HasCIWS6Trial(Definition))
    {
        Out={161.01693,62.999996,FVector(-66.83440,-.07568,47.0),
             {FVector(-94.18253,.04104,.67683),FVector::ZeroVector},FVector(7,0,0),
             TEXT("/Game/Presentation/WeaponsYawTrial/ciws_gatling_6/SM_Base.SM_Base"),
             TEXT("/Game/Presentation/WeaponsPitchTrial/ciws_gatling_6/SM_Body.SM_Body"),
             TEXT("/Game/Presentation/WeaponsPitchTrial/ciws_gatling_6/SM_Barrel.SM_Barrel"),
             TEXT("/Game/Presentation/WeaponsPitchTrial/ciws_gatling_6/SM_TrialNeck.SM_TrialNeck"),false};
        return true;
    }
    if (HasCIWSBTrial(Definition))
    {
        Out={164.4709,56.99999,FVector(-70.92006,-.03419,53.0),
             {FVector(-93.55087,0,.96070),FVector::ZeroVector},FVector(7,0,0),
             TEXT("/Game/Presentation/WeaponsYawTrial/ciws_beam_b/SM_Base.SM_Base"),
             TEXT("/Game/Presentation/WeaponsPitchTrial/ciws_beam_b/SM_Body.SM_Body"),
             TEXT("/Game/Presentation/WeaponsPitchTrial/ciws_beam_b/SM_Barrel.SM_Barrel"),
             TEXT("/Game/Presentation/WeaponsPitchTrial/ciws_beam_b/SM_TrialNeck.SM_TrialNeck"),false};
        return true;
    }
    return false;
}

FVector MuzzleLocal(const FOVWeaponMountDefinition& Definition, FRotator Aim,
    FVector HullHalfExtent, bool bFixedVLS, int32 BarrelIndex)
{
    const float Length=BarrelLength(HullHalfExtent,Definition.Tier,Definition.Type==EOVWeaponType::Interception);
    if (bFixedVLS) return Definition.LocalPosition + SurfaceNormal(Definition)*Length;
    FSourceTrialArt Art{};
    if (SourceTrialArt(Definition,Art))
    {
        // Isolated source-derived centimetres. The outlet is a candidate, not approved final art.
        const FVector Normal=SurfaceNormal(Definition);
        const bool bSide=FMath::Abs(Normal.Z)<.999, bBottom=!bSide&&Normal.Z<0;
        const FQuat Surface=bSide?FRotationMatrix::MakeFromZ(Normal).ToQuat():FQuat::Identity;
        const FRotator LocalAim=bSide?Surface.UnrotateVector(Aim.Vector()).Rotation():Aim;
        const FQuat Flip=FRotator(0,0,bBottom?180.f:0.f).Quaternion();
        const FQuat Yaw=FRotator(0,bBottom?180.f-LocalAim.Yaw:180.f+LocalAim.Yaw,0).Quaternion();
        const FQuat Pitch=FRotator(bBottom?LocalAim.Pitch:-LocalAim.Pitch,0,0).Quaternion();
        const double Scale=Length/Art.Span;
        const FVector Lift(0,0,(bBottom?-1.f:1.f)*Art.BaseDepth*Scale);
        const FVector Tip=Art.Tip[Art.bTwin?(BarrelIndex&1):0];
        return Definition.LocalPosition+Surface.RotateVector(Lift+Flip.RotateVector(FVector(0,0,.1*Scale)+Yaw.RotateVector((Art.Pivot+Pitch.RotateVector(Tip))*Scale)));
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

    OVWeaponGeometry::FSourceTrialArt Art{};
    if (OVWeaponGeometry::SourceTrialArt(Definition,Art))
    {
        // Trial only: source base, body and barrel plus a clearly separate generated neck.
        const double Scale=Length/Art.Span;
        auto* Flip=MakePivot(this);
        Flip->SetRelativeLocation(FVector(0,0,Side*Art.BaseDepth*Scale));
        Flip->SetRelativeRotation(FRotator(0,0,State.bBottom?180.f:0.f));
        MakeMesh(Flip,Art.Base,FVector::ZeroVector,FVector(Scale),FRotator::ZeroRotator,false);
        YawPivot=MakePivot(Flip);
        YawPivot->SetRelativeLocation(FVector(0,0,.1*Scale)); // 0.001 Blender-unit trial bearing clearance.
        MakeMesh(YawPivot,Art.Body,FVector::ZeroVector,FVector(Scale),FRotator::ZeroRotator,false);
        PitchPivot=MakePivot(YawPivot);PitchPivot->SetRelativeLocation(Art.Pivot*Scale);
        Barrel=MakeMesh(PitchPivot,Art.Barrel,FVector::ZeroVector,FVector(Scale),FRotator::ZeroRotator,false);
        auto* Neck=MakeMesh(PitchPivot,Art.Neck,Art.NeckOffset*Scale,FVector(Scale),FRotator::ZeroRotator,false);
        if (Base && Neck) { auto* M=UMaterialInstanceDynamic::Create(Base,Neck); M->SetVectorParameterValue(TEXT("Color"),FLinearColor(.19f,.2f,.21f)); Neck->SetMaterial(0,M); }
        Flash=MakeMesh(PitchPivot,TEXT("/Engine/BasicShapes/Sphere.Sphere"),Art.Tip[0]*Scale,FVector(Length*.15/100.f));
        if (Art.bTwin) SecondFlash=MakeMesh(PitchPivot,TEXT("/Engine/BasicShapes/Sphere.Sphere"),Art.Tip[1]*Scale,FVector(Length*.15/100.f));
        UStaticMeshComponent* Flashes[]={Flash.Get(),SecondFlash.Get()};
        for (auto* Part:Flashes) if (Part) {
            if (Base) { auto* M=UMaterialInstanceDynamic::Create(Base,Part);M->SetVectorParameterValue(TEXT("Color"),Definition.Type==EOVWeaponType::Cannon?FLinearColor(1.f,.66f,.08f):FLinearColor(.12f,.85f,1.f));Part->SetMaterial(0,M); }
            Part->SetVisibility(false);
        }
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
    OVWeaponGeometry::FSourceTrialArt Art{};
    const bool bSourceArt=!bFixedVLS&&OVWeaponGeometry::SourceTrialArt(Definition,Art);
    const bool bSource=bBF120||bSourceArt;
    if (YawPivot) YawPivot->SetRelativeRotation(FRotator(0,bSource?(State.bBottom?180.f-State.VisualAim.Yaw:180.f+State.VisualAim.Yaw):State.VisualAim.Yaw,0));
    if (PitchPivot) PitchPivot->SetRelativeRotation(FRotator(bSource?(State.bBottom?State.VisualAim.Pitch:-State.VisualAim.Pitch):State.VisualAim.Pitch,0,0));
    const int32 BarrelIndex=ShotsFired>0?(ShotsFired-1)%2:0;
    State.LocalMuzzle = bSourceArt ? OVWeaponGeometry::MuzzleLocal(Definition,LogicalAim,Extent,false,BarrelIndex) : State.bPointDefense
        ? Definition.LocalPosition+(bFixedVLS?Normal:LogicalAim.Vector())*Length
        : OVWeaponGeometry::MuzzleLocal(Definition,LogicalAim,Extent,bFixedVLS,BarrelIndex);
    State.LastShotId = ShotId;
    State.LastShotTime = ShotTime;
    // Game time keeps a paused muzzle flash frozen. Opening a viewer cannot fire or re-trigger it.
    State.bFlashVisible = !bDestroyed && ShotId > 0 && ShotTime >= 0 && GameTime >= ShotTime && GameTime - ShotTime < .12;
    const bool bTwin=bBF120||(bSourceArt&&Art.bTwin);
    if (Flash) Flash->SetVisibility(State.bFlashVisible&&(!bTwin||BarrelIndex==0));
    if (SecondFlash) SecondFlash->SetVisibility(State.bFlashVisible&&bTwin&&BarrelIndex==1);
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
