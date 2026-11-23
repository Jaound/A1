#include "OVShip.h"
#include "OVAudioSubsystem.h"
#include "OVInteriorSubsystem.h"
#include "OVCombatSubsystem.h"
#include "OVCombatVFXSubsystem.h"
#include "OVPointDefenseSubsystem.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "OVShipSystemsComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "HAL/IConsoleManager.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"

// PC14 uses the full source mesh and source-resolution maps; mount surfaces
// still need art alignment. Combat and selection geometry remain unchanged.
static TAutoConsoleVariable<int32> CVarOVImportedHull(TEXT("ov.Presentation.ImportedHull"),1,
    TEXT("Source hulls: 1 enables available meshes on the next scenario creation; 0 restores the placeholders. No combat geometry changes."));
static TAutoConsoleVariable<int32> CVarOVPC14FieldMaterial(TEXT("ov.Presentation.PC14FieldMaterial"),1,TEXT("PC14 field lighting material; zero restores the original tonal response."));
static TAutoConsoleVariable<float> CVarOVPC14Lift(TEXT("ov.Presentation.PC14Exposure"),0.45f,TEXT("PC14 source-map brightness in fleet and preview rendering."));
static TAutoConsoleVariable<int32> CVarOVPC14SourceMaterial(TEXT("ov.Presentation.PC14SourceMaterial"),1,
    TEXT("PC14 source-map material with provisional visibility fill; 0 uses faction placeholder material."));

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#endif

namespace OVShipMotion
{
    static void Turn(FQuat& Orientation, const FQuat& Desired, float TurnRate, float Delta)
    {
        if (Delta <= 0.0f || !FMath::IsFinite(Delta)) return;
        const double Angle = Orientation.AngularDistance(Desired);
        const double MaxTurn = FMath::DegreesToRadians(FMath::Max(0.0f, TurnRate)) * Delta;
        const double Alpha = Angle > UE_SMALL_NUMBER ? FMath::Min(1.0, MaxTurn / Angle) : 1.0;
        Orientation = FQuat::Slerp(Orientation, Desired, Alpha).GetNormalized();
    }

    // Integrate a bounded velocity change, including any constant-speed remainder.
    static FVector ChangeVelocity(FVector& Velocity, const FVector& Desired, double Rate, double Delta)
    {
        const FVector Difference = Desired - Velocity;
        const double DifferenceSize = Difference.Size();
        if (DifferenceSize <= UE_SMALL_NUMBER || Rate <= UE_SMALL_NUMBER)
            return Velocity * Delta;
        const double ChangingTime = FMath::Min(Delta, DifferenceSize / Rate);
        const FVector Accel = Difference / DifferenceSize * Rate;
        FVector Travel = Velocity * ChangingTime + Accel * (0.5 * ChangingTime * ChangingTime);
        Velocity += Accel * ChangingTime;
        if (ChangingTime < Delta) Velocity = Desired;
        Travel += Velocity * (Delta - ChangingTime);
        if (Velocity.SizeSquared() < 0.000001) Velocity = FVector::ZeroVector;
        return Travel;
    }

    static void Brake(FVector& Position, FVector& Velocity, float Braking, float Delta)
    {
        if (Delta <= 0.0f || !FMath::IsFinite(Delta)) return;
        Position += ChangeVelocity(Velocity, FVector::ZeroVector, FMath::Max(1.0f, Braking), Delta);
    }

    // Translation has inertia but is independent of a deliberate broadside posture.
    // Fixed small substeps keep arrival braking stable at 1/2/3x and during long frames.
    static bool Advance(FVector& Position, FQuat& Orientation, FVector& Velocity,
        const FVector& Destination, float Speed, float TurnRate, float Delta,
        float Acceleration = 1700.0f, float Braking = 2200.0f, float ArrivalRadius = 5.0f,
        bool bManualAttitude = false, const FQuat& ManualOrientation = FQuat::Identity, float LateralRate = 1700.f)
    {
        if (Delta <= 0.0f || !FMath::IsFinite(Delta) || Destination.ContainsNaN()) return false;
        const double Radius = FMath::Max(0.1f, ArrivalRadius);
        float Remaining = Delta;
        while (Remaining > UE_SMALL_NUMBER)
        {
            const float Step = FMath::Min(Remaining, 1.0f / 120.0f);
            const FVector Offset = Destination - Position;
            const double Distance = Offset.Size();
            const double BrakeRate = FMath::Max(1.f, Braking);
            // Only finish a sub-centimetre numerical remainder after braking.
            if (Distance <= FMath::Min(Radius, .01) && Velocity.Size() <= BrakeRate * Step)
            {
                Position = Destination;
                Velocity = FVector::ZeroVector;
                return true;
            }
            const FVector Direction = Offset.GetSafeNormal();
            Turn(Orientation, bManualAttitude ? ManualOrientation : Direction.ToOrientationQuat(), TurnRate, Step);
            const double Alignment = FVector::DotProduct(Orientation.GetForwardVector(), Direction);
            const double Throttle = bManualAttitude ? 1.0 : FMath::Clamp((Alignment - 0.6) / 0.4, 0.0, 1.0);
            // Reserve one integration step of reaction time. This avoids crossing a near goal
            // and waiting for a 180-degree turnaround when the throttle closes.
            const double Reaction = 2. * BrakeRate * Step;
            const double ArrivalSpeed = FMath::Sqrt(Reaction*Reaction + 1.6*BrakeRate*Distance) - Reaction;
            const double DesiredSpeed = FMath::Min(FMath::Max(0.f, Speed) * Throttle, ArrivalSpeed);
            const double Along = FVector::DotProduct(Velocity, Direction);
            FVector Axial = Direction * Along;
            FVector Lateral = Velocity - Axial;
            FVector Travel = ChangeVelocity(Axial, Direction * DesiredSpeed, Along > DesiredSpeed || Along < 0 ? BrakeRate : Acceleration, Step);
            Travel += ChangeVelocity(Lateral, FVector::ZeroVector, FMath::Max(1.f,LateralRate), Step);
            Velocity = Axial + Lateral;
            if (Distance <= Radius && Velocity.Size() <= BrakeRate * Step * 2 &&
                (Offset-Travel).SizeSquared() < .0001)
            {
                Position = Destination;
                Velocity = FVector::ZeroVector;
                return true;
            }
            Position += Travel;
            Remaining -= Step;
        }
        return false;
    }
}

AOVShip::AOVShip()
{
    PrimaryActorTick.bCanEverTick = false;
    Systems = CreateDefaultSubobject<UOVShipSystemsComponent>(TEXT("ShipSystems"));
    USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("ShipRoot"));
    SetRootComponent(Root);
    VisualRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ShipPresentation"));
    VisualRoot->SetupAttachment(Root);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cone(TEXT("/Engine/BasicShapes/Cone.Cone"));
    Hull = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Hull"));
    Hull->SetupAttachment(VisualRoot);
    Hull->SetStaticMesh(Cube.Object);
    Hull->SetRelativeScale3D(FVector(3.2, 1.0, 0.6));
    Hull->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Hull->SetCollisionResponseToAllChannels(ECR_Ignore);
    Hull->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    Hull->SetGenerateOverlapEvents(false);

    auto AddAccent = [&](const TCHAR* Name, UStaticMesh* Mesh, FVector Location, FVector Scale, FRotator Rotation)
    {
        UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(Name);
        Part->SetupAttachment(VisualRoot);
        Part->SetStaticMesh(Mesh);
        Part->SetRelativeLocation(Location);
        Part->SetRelativeScale3D(Scale);
        Part->SetRelativeRotation(Rotation);
        Part->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        Part->SetCollisionResponseToAllChannels(ECR_Ignore);
        Part->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
        AccentMeshes.Add(Part);
    };
    AddAccent(TEXT("Bow"), Cone.Object, FVector(190, 0, 0), FVector(1.0, 1.0, 1.25), FRotator(-90, 0, 0));
    AddAccent(TEXT("Bridge"), Cube.Object, FVector(-35, 0, 45), FVector(0.75, 0.65, 0.4), FRotator::ZeroRotator);
    AddAccent(TEXT("PortEngine"), Cube.Object, FVector(-125, -75, 0), FVector(1.0, 0.35, 0.45), FRotator::ZeroRotator);
    AddAccent(TEXT("StarboardEngine"), Cube.Object, FVector(-125, 75, 0), FVector(1.0, 0.35, 0.45), FRotator::ZeroRotator);
    CalibrationSelection=CreateDefaultSubobject<UBoxComponent>(TEXT("CalibrationSelection"));
    CalibrationSelection->SetupAttachment(Root);
    CalibrationSelection->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    CalibrationSelection->SetCollisionResponseToAllChannels(ECR_Ignore);
    CalibrationSelection->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
    CalibrationSelection->SetGenerateOverlapEvents(false);
}

void AOVShip::SetCalibrationHull(const FVector* LocalHalfExtent)
{
    ResetShipVisualVFX();
    for(const auto& Support:PresentationMountSupports)if(IsValid(Support.Get()))Support->DestroyComponent();
    PresentationMountSupports.Reset();
    if(bSavedHullMaterial){Hull->SetMaterial(0,SavedHullMaterial);SavedHullMaterial=nullptr;bSavedHullMaterial=false;}
    Hull->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
    Hull->SetRelativeRotation(FRotator::ZeroRotator);
    // Readable temporary silhouette inside the existing coarse combat OBB.
    // Selection retains exactly that OBB; neither actor scale nor combat dimensions change.
    CalibrationSelection->SetCollisionEnabled(LocalHalfExtent?ECollisionEnabled::QueryOnly:ECollisionEnabled::NoCollision);
    Hull->SetCollisionEnabled(LocalHalfExtent?ECollisionEnabled::NoCollision:ECollisionEnabled::QueryOnly);
    if(LocalHalfExtent) {
        const FVector E=*LocalHalfExtent; CalibrationSelection->SetBoxExtent(E);
        const bool Carrier=ShipName.Contains(TEXT("CARRIER"));
        Hull->SetRelativeLocation(FVector(-E.X*.2,0,0));
        Hull->SetRelativeScale3D(E/50.0*FVector(.8,Carrier?.85:.55,.65));
        AccentMeshes[0]->SetRelativeLocation(FVector(E.X*.6,0,0));
        AccentMeshes[0]->SetRelativeScale3D(FVector(E.Z*1.3/50,E.Y*(Carrier?.85:.55)/50,E.X*.8/100));
        AccentMeshes[1]->SetRelativeLocation(FVector(-E.X*.25,0,E.Z*.8));
        AccentMeshes[1]->SetRelativeScale3D(FVector(E.X*.28/50,E.Y*.35/50,E.Z*.2/50));
        for(int32 I=2;I<4;++I) {
            AccentMeshes[I]->SetRelativeLocation(FVector(-E.X*.6,(I==2?-1:1)*E.Y*.8,-E.Z*.1));
            AccentMeshes[I]->SetRelativeScale3D(FVector(E.X*.4/50,E.Y*.2/50,E.Z*.5/50));
        }
    } else {
        Hull->SetRelativeLocation(FVector::ZeroVector); Hull->SetRelativeScale3D(FVector(3.2,1,.6));
        const FVector Locations[]={{190,0,0},{-35,0,45},{-125,-75,0},{-125,75,0}};
        const FVector Scales[]={{1,1,1.25},{.75,.65,.4},{1,.35,.45},{1,.35,.45}};
        for(int32 I=0;I<AccentMeshes.Num();++I) { AccentMeshes[I]->SetRelativeLocation(Locations[I]); AccentMeshes[I]->SetRelativeScale3D(Scales[I]); }
    }
    for(UStaticMeshComponent* Part:AccentMeshes) { Part->SetVisibility(true);Part->SetCollisionEnabled(LocalHalfExtent?ECollisionEnabled::NoCollision:ECollisionEnabled::QueryOnly); }
}

void AOVShip::ApplyPresentationHull(FName DefinitionId)
{
    HullDefinitionId=DefinitionId;
    ResetShipVisualVFX();
    if(CVarOVImportedHull.GetValueOnGameThread()!=1 || !Systems || !Systems->Layout)return;
    const bool bCorvette=DefinitionId==TEXT("corvette");
    const TCHAR* MeshPath=bCorvette?TEXT("/Game/Presentation/Ships/SM_PC14_FullMesh.SM_PC14_FullMesh"):
        DefinitionId==TEXT("vector")?TEXT("/Game/Presentation/Ships/VectorFull/SM_Hull.SM_Hull"):
        DefinitionId==TEXT("bastion")?TEXT("/Game/Presentation/Ships/BastionFull/SM_Hull.SM_Hull"):
        DefinitionId==TEXT("carrier")?TEXT("/Game/Presentation/Ships/CarrierFull/SM_Hull.SM_Hull"):
        DefinitionId==TEXT("assault")?TEXT("/Game/Presentation/Ships/AssaultFull/SM_Hull.SM_Hull"):
        DefinitionId==TEXT("missile_ship")?TEXT("/Game/Presentation/Ships/Missile_ShipFull/SM_Hull.SM_Hull"):
        DefinitionId==TEXT("air_defense")?TEXT("/Game/Presentation/Ships/Air_DefenseFull/SM_Hull.SM_Hull"):
        DefinitionId==TEXT("frigate")?TEXT("/Game/Presentation/Ships/FrigateFull/SM_Hull.SM_Hull"):
        DefinitionId==TEXT("cruiser")?TEXT("/Game/Presentation/Ships/CruiserFull/SM_Hull.SM_Hull"):
        DefinitionId==TEXT("radar")?TEXT("/Game/Presentation/Ships/RadarFull/SM_Hull.SM_Hull"):
        DefinitionId==TEXT("repair")?TEXT("/Game/Presentation/Ships/RepairFull/SM_Hull.SM_Hull"):
        DefinitionId==TEXT("mining")?TEXT("/Game/Presentation/Ships/MiningFull/SM_Hull.SM_Hull"):
        DefinitionId==TEXT("transport")?TEXT("/Game/Presentation/Ships/TransportFull/SM_Hull.SM_Hull"):
        DefinitionId==TEXT("battleship")?TEXT("/Game/Presentation/Ships/BattleshipFull/SM_Hull.SM_Hull"):
        DefinitionId==TEXT("dreadnought")?TEXT("/Game/Presentation/Ships/DreadnoughtFull/SM_Hull.SM_Hull"):
        DefinitionId==TEXT("battlecarrier")?TEXT("/Game/Presentation/Ships/BattlecarrierFull/SM_Hull.SM_Hull"):nullptr;
    const TCHAR* MaterialPath=bCorvette?(CVarOVPC14FieldMaterial.GetValueOnGameThread()!=0?TEXT("/Game/Presentation/PC14Full/M_PC14_FieldReadable.M_PC14_FieldReadable"):TEXT("/Game/Presentation/PC14Full/M_PC14_FullReadable.M_PC14_FullReadable")):
        DefinitionId==TEXT("vector")?TEXT("/Game/Presentation/Ships/VectorFull/M_Readable.M_Readable"):
        DefinitionId==TEXT("bastion")?TEXT("/Game/Presentation/Ships/BastionFull/M_Readable.M_Readable"):
        DefinitionId==TEXT("carrier")?TEXT("/Game/Presentation/Ships/CarrierFull/M_Readable.M_Readable"):
        DefinitionId==TEXT("assault")?TEXT("/Game/Presentation/Ships/AssaultFull/M_Readable.M_Readable"):
        DefinitionId==TEXT("missile_ship")?TEXT("/Game/Presentation/Ships/Missile_ShipFull/M_Readable.M_Readable"):
        DefinitionId==TEXT("air_defense")?TEXT("/Game/Presentation/Ships/Air_DefenseFull/M_Readable.M_Readable"):
        DefinitionId==TEXT("frigate")?TEXT("/Game/Presentation/Ships/FrigateFull/M_Readable.M_Readable"):
        DefinitionId==TEXT("cruiser")?TEXT("/Game/Presentation/Ships/CruiserFull/M_Readable.M_Readable"):
        DefinitionId==TEXT("radar")?TEXT("/Game/Presentation/Ships/RadarFull/M_Readable.M_Readable"):
        DefinitionId==TEXT("repair")?TEXT("/Game/Presentation/Ships/RepairFull/M_Readable.M_Readable"):
        DefinitionId==TEXT("mining")?TEXT("/Game/Presentation/Ships/MiningFull/M_Readable.M_Readable"):
        DefinitionId==TEXT("transport")?TEXT("/Game/Presentation/Ships/TransportFull/M_Readable.M_Readable"):
        DefinitionId==TEXT("battleship")?TEXT("/Game/Presentation/Ships/BattleshipFull/M_Readable.M_Readable"):
        DefinitionId==TEXT("dreadnought")?TEXT("/Game/Presentation/Ships/DreadnoughtFull/M_Readable.M_Readable"):
        DefinitionId==TEXT("battlecarrier")?TEXT("/Game/Presentation/Ships/BattlecarrierFull/M_Readable.M_Readable"):nullptr;
    if(!MeshPath||!MaterialPath)return;
    auto* Mesh=LoadObject<UStaticMesh>(nullptr,MeshPath,nullptr,LOAD_NoWarn);
    auto* Material=LoadObject<UMaterialInterface>(nullptr,MaterialPath,nullptr,LOAD_NoWarn);
    if(!Mesh||!Material)return;
    for(const auto& Support:PresentationMountSupports)if(IsValid(Support.Get()))Support->DestroyComponent();
    PresentationMountSupports.Reset();
    if(bSavedHullMaterial){Hull->SetMaterial(0,SavedHullMaterial);SavedHullMaterial=nullptr;bSavedHullMaterial=false;}
    if(!bCorvette||CVarOVPC14SourceMaterial.GetValueOnGameThread()==1)
            if(auto* Instance=UMaterialInstanceDynamic::Create(Material,this)) {
                SavedHullMaterial=Hull->GetMaterial(0);bSavedHullMaterial=true;
                Instance->SetVectorParameterValue(TEXT("FactionTint"),(bFriendly?FLinearColor(.8f,.9f,1.f):FLinearColor(1.f,.72f,.7f)) * (bCorvette ? CVarOVPC14Lift.GetValueOnGameThread() : 1.f));
                Hull->SetMaterial(0,Instance);
            }
    const FBoxSphereBounds Bounds=Mesh->GetBounds();
    const FVector Available=Systems->Layout->LocalHalfExtent;
    const FVector Ratio=Available/Bounds.BoxExtent.ComponentMax(FVector(1));
    const double Scale=Ratio.GetMin();
    Hull->SetStaticMesh(Mesh);
    // The source bows point toward -X; gameplay forward is +X.
    const FRotator SourceRotation(0,180,0);
    Hull->SetRelativeRotation(SourceRotation);
    Hull->SetRelativeScale3D(FVector(Scale));
    Hull->SetRelativeLocation(SourceRotation.RotateVector(-Bounds.Origin*Scale));
    Hull->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    for(auto Part:AccentMeshes)Part->SetVisibility(false);
    // Provisional adapters measured against the full mesh and existing
    // corvette hardpoints. Final art alignment still requires review;
    // these do not alter the physical mount, muzzle or collision OBB.
    if(!bCorvette)return;
    const auto& Mounts=Systems->Layout->WeaponMounts;
    if(Mounts.Num()<2)return;
    const FVector ExpectedPositions[]={FVector(-46.6666667,0,42),FVector(46.6666667,0,-42)};
    const double SurfaceAtUnitScale[]={7.3664/.7723395451,-28.0984/.7723395451};
    for(int32 I=0;I<2;++I)
    {
        const auto& Mount=Mounts[I];
        if(!Mount.LocalPosition.Equals(ExpectedPositions[I],.1))continue;
        const double Side=Mount.LocalPosition.Z>0?1:-1;
        const double Length=26.0;
        const double BaseInner=Mount.LocalPosition.Z-Side*Length*(.23+.09);
        const double Surface=SurfaceAtUnitScale[I]*Scale;
        const double Height=Side*(BaseInner-Surface);
        if(Height<=.01)continue;
        const FName Name=MakeUniqueObjectName(this,UStaticMeshComponent::StaticClass(),TEXT("PC14MountSupport"));
        auto* Support=NewObject<UStaticMeshComponent>(this,Name);
        AddInstanceComponent(Support);Support->SetupAttachment(VisualRoot);
        Support->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
        Support->SetCollisionEnabled(ECollisionEnabled::NoCollision);Support->SetGenerateOverlapEvents(false);
        Support->SetRelativeLocation(FVector(Mount.LocalPosition.X,Mount.LocalPosition.Y,(BaseInner+Surface)*.5));
        Support->SetRelativeScale3D(FVector(Length*.42/100,Length*.42/100,Height/100));
        // The provisional adapter is a primitive, so do not stretch the source
        // hull's UV-mapped texture across it.
        Support->SetMaterial(0,bSavedHullMaterial?SavedHullMaterial.Get():Hull->GetMaterial(0));
        Support->RegisterComponent();PresentationMountSupports.Add(Support);
    }
}

void AOVShip::Configure(const FString& Name, bool Friendly, float Speed, float RotationRate,
    const FVector& Scale, const FLinearColor& Color)
{
    ResetShipVisualVFX();
    if(bSavedHullMaterial){Hull->SetMaterial(0,SavedHullMaterial);SavedHullMaterial=nullptr;bSavedHullMaterial=false;}
    ConfigureClassMotion(0);
    EnvironmentPullVelocity = FVector::ZeroVector;
    ShipName = Name;
    bFriendly = Friendly;
    MoveSpeed = FMath::Max(0.0f, Speed);
    TurnRate = FMath::Max(0.0f, RotationRate);
    SetActorScale3D(Scale);
    UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    if (Base)
    {
        UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Base, this);
        Material->SetVectorParameterValue(TEXT("Color"), Color);
        Hull->SetMaterial(0, Material);
        UMaterialInstanceDynamic* Accent = UMaterialInstanceDynamic::Create(Base, this);
        Accent->SetVectorParameterValue(TEXT("Color"), FLinearColor(Color.R * 0.5f + 0.15f, Color.G * 0.5f + 0.15f, Color.B * 0.5f + 0.15f));
        for (UStaticMeshComponent* Part : AccentMeshes) Part->SetMaterial(0, Accent);
    }
}

void AOVShip::ClearWeaponVisuals()
{
    for (UOVWeaponVisualComponent* Visual : WeaponVisuals)
        if (IsValid(Visual)) Visual->DestroyComponent();
    WeaponVisuals.Reset();
}

void AOVShip::RefreshWeaponVisuals()
{
    if (!Systems || !Systems->Layout || !GetWorld()) { ClearWeaponVisuals(); return; }
    const auto* Combat = GetWorld()->GetSubsystem<UOVCombatSubsystem>();
    const auto* PD = GetWorld()->GetSubsystem<UOVPointDefenseSubsystem>();
    const FOVCombatShipState* CombatState = Combat && Combat->bEnabled ? Combat->Ships.FindByPredicate(
        [this](const FOVCombatShipState& S) { return S.Ship == this; }) : nullptr;
    const FOVPDShipState* PDState = PD && PD->bEnabled ? PD->Ships.FindByPredicate(
        [this](const FOVPDShipState& S) { return S.Ship == this; }) : nullptr;
    TArray<FName, TInlineAllocator<32>> Active;
    auto Sync = [&](const FOVWeaponMountDefinition& Def, FRotator Aim, bool bFixedVLS,
        bool bDestroyed, int32 ShotId, double ShotTime, double Clock, int32 ShotsFired, bool bCannon, bool bKineticPD, bool bBeamAttack)
    {
        FOVWeaponMountDefinition VisualDef = Def;
        if(Def.Type==EOVWeaponType::Missile && Def.PresentationProfile!=TEXT("AntiShipMissileModule"))
            VisualDef.PresentationProfile=bFixedVLS ? TEXT("StandardMissilePod") : TEXT("GuidedMissilePod");
        else if(!bCannon && !bBeamAttack && Def.Type!=EOVWeaponType::Missile) VisualDef.PresentationProfile=NAME_None;
        if (bCannon) VisualDef.PresentationProfile=OVWeaponGeometry::CannonSourceTrialProfile(VisualDef);
        if (Def.Type==EOVWeaponType::Interception && OVWeaponGeometry::CIWSBTrialEnabled()) VisualDef.PresentationProfile=bKineticPD?TEXT("CIWS6Trial"):TEXT("CIWSBTrial");
        if (Def.Type==EOVWeaponType::Beam && bBeamAttack) VisualDef.PresentationProfile=OVWeaponGeometry::BeamSourceTrialProfile(VisualDef);
        Active.Add(Def.WeaponId);
        UOVWeaponVisualComponent* Visual = nullptr;
        for (auto& Existing : WeaponVisuals)
            if (Existing && Existing->GetMountId() == Def.WeaponId)
            {
                if (Existing->Matches(VisualDef, Systems->Layout->LocalHalfExtent)) Visual = Existing;
                else { Existing->DestroyComponent(); Existing = nullptr; }
                break;
            }
        if (!Visual)
        {
            Visual = NewObject<UOVWeaponVisualComponent>(this);
            AddInstanceComponent(Visual);
            Visual->SetupAttachment(VisualRoot);
            Visual->RegisterComponent();
            Visual->Initialize(VisualDef, Systems->Layout->LocalHalfExtent);
            WeaponVisuals.Add(Visual);
        }
        Visual->Synchronize(Aim, bFixedVLS, bDestroyed, Systems->FactionId == 0, ShotId, ShotTime, Clock, ShotsFired);
    };
    if (CombatState)
        for (const FOVCombatMountState& Mount : CombatState->Mounts)
        {
            const auto* Def = Systems->Layout->WeaponMounts.FindByPredicate(
                [&Mount](const FOVWeaponMountDefinition& D) { return D.WeaponId == Mount.MountId; });
            if (!Def) continue;
            const auto* Health = Systems->WeaponMountStates.FindByPredicate(
                [&Mount](const FOVWeaponMountState& H) { return H.WeaponMountId == Mount.MountId; });
            Sync(*Def, Mount.LocalAim, Mount.Profile.Kind == EOVAttackKind::Missile,
                Systems->bWrecked || !Health || Health->HP <= 0, Mount.LastShotId, Mount.LastShotTime, Combat->GameTime, Mount.ShotsFired, Mount.Profile.Kind == EOVAttackKind::Cannon,false,Mount.Profile.Kind==EOVAttackKind::Beam);
        }
    if (PDState)
        for (const FOVPDMountState& Mount : PDState->Mounts)
        {
            FOVWeaponMountDefinition Def;
            const auto* Authored=Mount.bLayoutMount?Systems->Layout->WeaponMounts.FindByPredicate(
                [&Mount](const FOVWeaponMountDefinition& D){return D.WeaponId==Mount.MountId;}):nullptr;
            if(Authored)Def=*Authored;
            else { Def.WeaponId=Mount.MountId;Def.Type=EOVWeaponType::Interception;
                Def.LocalPosition=Mount.LocalPosition;Def.Tier=Systems->Layout->WeaponTier; }
            const auto* Health = Systems->WeaponMountStates.FindByPredicate(
                [&Mount](const FOVWeaponMountState& H) { return H.WeaponMountId == Mount.MountId; });
            Sync(Def, Mount.LocalAim, false, Systems->bWrecked || Mount.HP <= 0
                || (Mount.bLayoutMount && (!Health || Health->HP <= 0)), Mount.LastShotId, Mount.LastShotTime, PD->GameTime, 0, false,Mount.Profile.Kind==EOVPDWeaponKind::Kinetic,false);
        }
    for (int32 I = WeaponVisuals.Num() - 1; I >= 0; --I)
        if (!WeaponVisuals[I] || !Active.Contains(WeaponVisuals[I]->GetMountId()))
        {
            if (WeaponVisuals[I]) WeaponVisuals[I]->DestroyComponent();
            WeaponVisuals.RemoveAt(I);
        }
}

TArray<FOVWeaponVisualState> AOVShip::GetWeaponVisualStates() const
{
    TArray<FOVWeaponVisualState> Result;
    for (const UOVWeaponVisualComponent* Visual : WeaponVisuals)
        if (IsValid(Visual)) Result.Add(Visual->ReadState());
    return Result;
}

FVector AOVShip::GetWeaponMuzzleWorld(FName MountId) const
{
    if (!Systems || !Systems->Layout || !GetWorld()) return GetActorLocation();
    const auto* Combat = GetWorld()->GetSubsystem<UOVCombatSubsystem>();
    if (Combat)
        if (const auto* State = Combat->Ships.FindByPredicate([this](const FOVCombatShipState& S) { return S.Ship == this; }))
            if (const auto* Mount = State->Mounts.FindByPredicate([MountId](const FOVCombatMountState& M) { return M.MountId == MountId; }))
                if (const auto* Def = Systems->Layout->WeaponMounts.FindByPredicate([MountId](const FOVWeaponMountDefinition& D) { return D.WeaponId == MountId; }))
                {
                    FOVWeaponMountDefinition EffectiveDef = *Def;
                    if (Mount->Profile.Kind != EOVAttackKind::Cannon && Mount->Profile.Kind != EOVAttackKind::Beam) EffectiveDef.PresentationProfile = NAME_None;
                    if (Mount->Profile.Kind == EOVAttackKind::Cannon) EffectiveDef.PresentationProfile=OVWeaponGeometry::CannonSourceTrialProfile(EffectiveDef);
                    if (Mount->Profile.Kind == EOVAttackKind::Beam) EffectiveDef.PresentationProfile=OVWeaponGeometry::BeamSourceTrialProfile(EffectiveDef);
                    return GetActorTransform().TransformPosition(OVWeaponGeometry::MuzzleLocal(EffectiveDef,Mount->LocalAim,
                        Systems->Layout->LocalHalfExtent,Mount->Profile.Kind == EOVAttackKind::Missile,Mount->ShotsFired>0?(Mount->ShotsFired-1)%2:0));
                }
    const auto* PD = GetWorld()->GetSubsystem<UOVPointDefenseSubsystem>();
    if (PD)
        if (const auto* State = PD->Ships.FindByPredicate([this](const FOVPDShipState& S) { return S.Ship == this; }))
            if (const auto* Mount = State->Mounts.FindByPredicate([MountId](const FOVPDMountState& M) { return M.MountId == MountId; }))
            {
                FOVWeaponMountDefinition Def;
                const auto* Authored = Mount->bLayoutMount ? Systems->Layout->WeaponMounts.FindByPredicate(
                    [MountId](const FOVWeaponMountDefinition& D){return D.WeaponId == MountId;}) : nullptr;
                if (Authored) Def = *Authored;
                else { Def.Type=EOVWeaponType::Interception; Def.LocalPosition=Mount->LocalPosition; Def.Tier=Systems->Layout->WeaponTier; }
                if (OVWeaponGeometry::CIWSBTrialEnabled())
                    Def.PresentationProfile=Mount->Profile.Kind==EOVPDWeaponKind::Kinetic?TEXT("CIWS6Trial"):TEXT("CIWSBTrial");
                return GetActorTransform().TransformPosition(OVWeaponGeometry::MuzzleLocal(Def,Mount->LocalAim,Systems->Layout->LocalHalfExtent,false,0));
            }
    return GetActorLocation();
}

void AOVShip::SetDestination(const FVector& Location)
{
    if (!Location.ContainsNaN() && !bFTLCharging && Systems && Systems->CanMove())
    {
        bPlayerHold = false;
        Destination = Location;
        bHasDestination = true;
        bHold = false;
    }
}

void AOVShip::StopMovement()
{
    bHasDestination = false;
    bHold = true;
    // Cancel an unfinished facing turn without resetting the current pitch or roll.
    if (bManualAttitude) ManualAttitude = GetActorRotation();
}

void AOVShip::SetManualAttitude(const FRotator& Rotation)
{
    if (Rotation.ContainsNaN() || bFTLCharging || !Systems || !Systems->CanMove()) return;
    ManualAttitude = Rotation.GetNormalized();
    bManualAttitude = true;
}

void AOVShip::SetAutomation(bool bMove, bool bAttack)
{
    bMovementAutomatic = bMove; bAttackAutomatic = bAttack;
    bManual = !bMove && !bAttack; bLastManual = bManual;
}

bool AOVShip::IsMovementAutomatic() const
{
    return bManual != bLastManual ? !bManual : bMovementAutomatic;
}

bool AOVShip::IsAttackAutomatic() const
{
    return bManual != bLastManual ? !bManual : bAttackAutomatic;
}

void AOVShip::ConfigureClassMotion(int32 Size)
{
    struct FResponse { float AccelerateSeconds, BrakeSeconds, LateralSeconds; };
    static constexpr FResponse Trial[] = {{2.4f,1.8f,3.f},{4.f,3.f,5.f},{6.f,4.5f,8.f},{9.f,7.f,12.f}};
    if (Size < 1 || Size > 4)
    {
        Acceleration=1700; Braking=2200; LateralAcceleration=1700;
        return;
    }
    const FResponse& Response=Trial[Size-1];
    Acceleration=MoveSpeed/Response.AccelerateSeconds;
    Braking=MoveSpeed/Response.BrakeSeconds;
    LateralAcceleration=MoveSpeed/Response.LateralSeconds;
}

void AOVShip::Simulate(float SimDelta)
{
    if (!FMath::IsFinite(SimDelta) || SimDelta <= 0.0f) return;
    if (bFTLCharging || !Systems || !Systems->CanMove())
    {
        // Gameplay immobilization is separate from an ordinary inertial stop order.
        if (!bFTLCharging && Systems && !Systems->bWrecked && !Systems->bNeutral)
            AddActorWorldOffset(Velocity * SimDelta, false);
        else Velocity = FVector::ZeroVector;
        if (bFTLCharging || (Systems && (Systems->bWrecked || Systems->bNeutral)))
        {
            bHasDestination = false;
            bHold = true;
        }
        if (Systems && (Systems->bWrecked || Systems->bNeutral)) AttackTarget = nullptr;
        return;
    }
    const float EngineEfficiency = Systems && Systems->Layout ? Systems->GetEfficiency(EOVFacilityType::Engine) : 1.f;
    const FOVHeroDutyBonuses Duty = GetWorld()->GetSubsystem<UOVInteriorSubsystem>()->GetDutyBonuses(this);
    FVector Position = GetActorLocation();
    FQuat Orientation = GetActorQuat();
    if (bHasDestination && !bHold)
    {
        if (OVShipMotion::Advance(Position, Orientation, Velocity, Destination, MoveSpeed * EngineEfficiency * (1.f+Duty.SpeedBonus), TurnRate * EngineEfficiency * (1.f+Duty.TurnBonus),
            SimDelta, Acceleration*Duty.AccelerationScale, Braking*Duty.BrakingScale, ArrivalRadius, bManualAttitude, ManualAttitude.Quaternion(), LateralAcceleration*Duty.LateralScale))
        {
            bHasDestination = false;
            bHold = true;
        }
    }
    else
    {
        OVShipMotion::Brake(Position, Velocity, Braking*Duty.BrakingScale, SimDelta);
        if (bManualAttitude) OVShipMotion::Turn(Orientation, ManualAttitude.Quaternion(), TurnRate * EngineEfficiency * (1.f+Duty.TurnBonus), SimDelta);
    }
    SetActorLocationAndRotation(Position, Orientation, false, nullptr, ETeleportType::TeleportPhysics);
}

void AOVShip::DebugResetMotion()
{
    StopMovement(); Velocity=FVector::ZeroVector; bManualAttitude=false;
    ManualAttitude=GetActorRotation(); bFTLCharging=false; AttackTarget=nullptr; SetAutomation(false,false);
    ResetShipVisualVFX();
}

void AOVShip::DebugSetFTLCharging(bool bCharging)
{
    bFTLCharging=bCharging;
    if(bCharging) { StopMovement(); Velocity=FVector::ZeroVector; }
}

bool AOVShip::IsEnemyShip() const
{
    return !bFriendly || (Systems && Systems->FactionId != 0);
}

void AOVShip::UpdateShieldImpactVisuals(float Pulse)
{
    const bool bShow = Pulse > 0.0f && !bDissolving && (!Systems || !Systems->bWrecked);
    if (!bShow)
    {
        if (IsValid(ShieldImpactWFMesh)) ShieldImpactWFMesh->SetVisibility(false);
        if (IsValid(ShieldImpactGlowMesh)) ShieldImpactGlowMesh->SetVisibility(false);
        return;
    }

    const bool bEnemy = IsEnemyShip();
    const FVector LayoutExt = (Systems && Systems->Layout) ? Systems->Layout->LocalHalfExtent : FVector(175.f, 75.f, 30.f);
    const float ClampedPulse = FMath::Clamp(Pulse, 0.0f, 1.0f);
    const float RippleExpand = 1.0f + 0.022f * (1.0f - ClampedPulse);
    const FVector BaseScale = GetLocalShieldRadii() * (1.006 / 50.0);

    // Deep saturated faction hues so UE5 ACES Filmic tonemapper preserves rich crimson red (enemy)
    // and crisp electric cyan-blue (friendly) without desaturating to peach/white under bloom.
    const FLinearColor WFColor = bEnemy
        ? FLinearColor(1.0f, 0.025f, 0.015f, 1.0f)
        : FLinearColor(0.06f, 0.60f, 1.0f, 1.0f);
    const FLinearColor GlowColor = bEnemy
        ? FLinearColor(1.0f, 0.02f, 0.01f, 1.0f)
        : FLinearColor(0.04f, 0.52f, 1.0f, 1.0f);

    const FLinearColor HitLocParam(LastShieldHitLocation.X, LastShieldHitLocation.Y, LastShieldHitLocation.Z, 1.0f);
    const float ImpactRad = FMath::Max(160.0f, float(LayoutExt.X) * FMath::Max(0.5f, float(GetActorScale3D().X)) * 0.95f);

    if (IsValid(ShieldImpactWFMesh))
    {
        ShieldImpactWFMesh->SetRelativeLocation(FVector::ZeroVector);
        ShieldImpactWFMesh->SetRelativeScale3D(BaseScale * RippleExpand);
        ShieldImpactWFMesh->SetVisibility(true);
        if (IsValid(ShieldImpactWFDyn))
        {
            ShieldImpactWFDyn->SetVectorParameterValue(TEXT("ShieldColor"), WFColor);
            ShieldImpactWFDyn->SetScalarParameterValue(TEXT("Intensity"), 0.62f * LastShieldHitStrength * ClampedPulse);
            ShieldImpactWFDyn->SetVectorParameterValue(TEXT("HitLocation"), HitLocParam);
            ShieldImpactWFDyn->SetScalarParameterValue(TEXT("ImpactRadius"), ImpactRad);
            ShieldImpactWFDyn->SetScalarParameterValue(TEXT("HotspotStrength"), 1.8f);
        }
    }

    if (IsValid(ShieldImpactGlowMesh))
    {
        ShieldImpactGlowMesh->SetRelativeLocation(FVector::ZeroVector);
        ShieldImpactGlowMesh->SetRelativeScale3D(BaseScale * (RippleExpand * 1.004f));
        ShieldImpactGlowMesh->SetVisibility(true);
        if (IsValid(ShieldImpactGlowDyn))
        {
            ShieldImpactGlowDyn->SetVectorParameterValue(TEXT("ShieldColor"), GlowColor);
            ShieldImpactGlowDyn->SetScalarParameterValue(TEXT("Intensity"), 0.34f * LastShieldHitStrength * ClampedPulse);
            ShieldImpactGlowDyn->SetVectorParameterValue(TEXT("HitLocation"), HitLocParam);
            ShieldImpactGlowDyn->SetScalarParameterValue(TEXT("ImpactRadius"), ImpactRad);
            ShieldImpactGlowDyn->SetScalarParameterValue(TEXT("HotspotStrength"), 1.5f);
        }
    }
}

void AOVShip::ResetShipVisualVFX()
{
    bDissolving = false;
    DissolveProgress = 0.0f;
    ShieldImpactRemaining = 0.0f;
    LastShieldHitLocation = FVector::ZeroVector;
    LastShieldHitStrength = 1.0f;
    if (IsValid(ShieldVFX) && ShieldVFX->IsActive())
    {
        ShieldVFX->Deactivate();
    }
    if (IsValid(ShieldImpactWFMesh))
    {
        ShieldImpactWFMesh->SetVisibility(false);
    }
    if (IsValid(ShieldImpactGlowMesh))
    {
        ShieldImpactGlowMesh->SetVisibility(false);
    }
    for (UNiagaraComponent* Comp : WingTrailVFX)
    {
        if (IsValid(Comp) && Comp->IsActive())
        {
            Comp->Deactivate();
        }
    }
}

FVector AOVShip::GetLocalShieldRadii() const
{
    return Systems && Systems->Layout ? Systems->Layout->GetShieldRadii() : FVector(213.5,99,42.6);
}

FVector AOVShip::GetVisualHullHalfExtent() const
{
    if (Hull && Hull->GetStaticMesh() && !Hull->GetStaticMesh()->GetPathName().StartsWith(TEXT("/Engine/BasicShapes/")))
    {
        const FVector MeshExtent = Hull->GetStaticMesh()->GetBounds().BoxExtent * Hull->GetRelativeScale3D();
        if (MeshExtent.X > 5.0f && MeshExtent.Y > 2.0f)
        {
            return FVector(
                FMath::Max(MeshExtent.X, 40.0f),
                FMath::Max(MeshExtent.Y, 18.0f),
                FMath::Max(MeshExtent.Z, 14.0f));
        }
    }
    return (Systems && Systems->Layout) ? Systems->Layout->LocalHalfExtent : FVector(160.f, 50.f, 30.f);
}

FVector AOVShip::GetVisualHullCenterOffset() const
{
    if (Hull && Hull->GetStaticMesh() && !Hull->GetStaticMesh()->GetPathName().StartsWith(TEXT("/Engine/BasicShapes/")))
    {
        const FVector MeshOrigin = Hull->GetStaticMesh()->GetBounds().Origin * Hull->GetRelativeScale3D();
        return Hull->GetRelativeLocation() + Hull->GetRelativeRotation().RotateVector(MeshOrigin);
    }
    return FVector::ZeroVector;
}

void AOVShip::EnsureShipVFXInitialized()
{
    if (bVFXInitialized || !GetWorld()) return;
    bVFXInitialized = true;

    static TWeakObjectPtr<UNiagaraSystem> CachedThrusterSys;
    static TWeakObjectPtr<UNiagaraSystem> CachedWingTrailSys;
    static TWeakObjectPtr<UNiagaraSystem> CachedShieldSys;
    if (!CachedThrusterSys.IsValid())
        CachedThrusterSys = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/SpaceshipVFXStarter/VFX/ThrusterTrail/NS_ThrusterTrailAdditive_Seamless.NS_ThrusterTrailAdditive_Seamless"), nullptr, LOAD_NoWarn);
    if (!CachedWingTrailSys.IsValid())
        CachedWingTrailSys = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/SpaceshipVFXStarter/VFX/WingTrail/NS_WingTrail.NS_WingTrail"), nullptr, LOAD_NoWarn);
    if (!CachedShieldSys.IsValid())
        CachedShieldSys = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/SpaceshipVFXStarter/VFX/Shield/NS_ShieldStandard.NS_ShieldStandard"), nullptr, LOAD_NoWarn);

    const FVector E = GetVisualHullHalfExtent();
    const FVector C = GetVisualHullCenterOffset();

    // 1. Dual Directional Main Engine Thruster Plumes (compact ignition core + tapered blue-white ion jet aft of stern)
    static TWeakObjectPtr<UStaticMesh> CachedThrusterSphereMesh;
    static TWeakObjectPtr<UStaticMesh> CachedThrusterConeMesh;
    static TWeakObjectPtr<UMaterialInterface> CachedThrusterCoreMat;
    static TWeakObjectPtr<UMaterialInterface> CachedThrusterJetMat;
    if (!CachedThrusterSphereMesh.IsValid())
        CachedThrusterSphereMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"), nullptr, LOAD_NoWarn);
    if (!CachedThrusterConeMesh.IsValid())
        CachedThrusterConeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cone.Cone"), nullptr, LOAD_NoWarn);
    if (!CachedThrusterCoreMat.IsValid())
        CachedThrusterCoreMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Presentation/Effects/M_ThrusterCore.M_ThrusterCore"), nullptr, LOAD_NoWarn);
    if (!CachedThrusterJetMat.IsValid())
        CachedThrusterJetMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Presentation/Effects/M_ThrusterJet.M_ThrusterJet"), nullptr, LOAD_NoWarn);

    const FRotator AsternZRot = FRotationMatrix::MakeFromZ(FVector(-1.0f, 0.0f, 0.0f)).Rotator();
    for (int32 i = 0; i < 2; ++i)
    {
        const float Sign = (i == 0) ? -1.0f : 1.0f;
        const FVector NozzleExit = C + FVector(-E.X * 1.02f, Sign * E.Y * 0.30f, 0.0f);

        if (CachedThrusterSphereMesh.IsValid() && CachedThrusterCoreMat.IsValid())
        {
            const FName CoreName = (i == 0) ? FName(TEXT("ThrusterCoreMesh_0")) : FName(TEXT("ThrusterCoreMesh_1"));
            UStaticMeshComponent* CoreComp = NewObject<UStaticMeshComponent>(this, CoreName);
            AddInstanceComponent(CoreComp);
            CoreComp->SetupAttachment(VisualRoot);
            CoreComp->SetStaticMesh(CachedThrusterSphereMesh.Get());
            CoreComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            CoreComp->SetCollisionResponseToAllChannels(ECR_Ignore);
            CoreComp->SetGenerateOverlapEvents(false);
            CoreComp->SetCanEverAffectNavigation(false);
            CoreComp->SetCastShadow(false);
            CoreComp->SetRelativeLocation(NozzleExit + FVector(-4.5f, 0.0f, 0.0f));
            CoreComp->SetRelativeScale3D(FVector(0.15f, 0.13f, 0.13f));
            UMaterialInstanceDynamic* CoreDyn = UMaterialInstanceDynamic::Create(CachedThrusterCoreMat.Get(), this);
            if (CoreDyn)
            {
                CoreComp->SetMaterial(0, CoreDyn);
            }
            CoreComp->RegisterComponent();
            ThrusterCoreMeshes.Add(CoreComp);
            ThrusterCoreDyns.Add(CoreDyn);
        }

        if (CachedThrusterConeMesh.IsValid() && CachedThrusterJetMat.IsValid())
        {
            const FName JetName = (i == 0) ? FName(TEXT("ThrusterJetMesh_0")) : FName(TEXT("ThrusterJetMesh_1"));
            UStaticMeshComponent* JetComp = NewObject<UStaticMeshComponent>(this, JetName);
            AddInstanceComponent(JetComp);
            JetComp->SetupAttachment(VisualRoot);
            JetComp->SetStaticMesh(CachedThrusterConeMesh.Get());
            JetComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            JetComp->SetCollisionResponseToAllChannels(ECR_Ignore);
            JetComp->SetGenerateOverlapEvents(false);
            JetComp->SetCanEverAffectNavigation(false);
            JetComp->SetCastShadow(false);
            JetComp->SetRelativeLocation(NozzleExit + FVector(-22.0f, 0.0f, 0.0f));
            JetComp->SetRelativeRotation(AsternZRot);
            JetComp->SetRelativeScale3D(FVector(0.16f, 0.16f, 0.38f));
            UMaterialInstanceDynamic* JetDyn = UMaterialInstanceDynamic::Create(CachedThrusterJetMat.Get(), this);
            if (JetDyn)
            {
                JetComp->SetMaterial(0, JetDyn);
            }
            JetComp->RegisterComponent();
            ThrusterJetMeshes.Add(JetComp);
            ThrusterJetDyns.Add(JetDyn);
        }
    }

    // Keep standby Niagara thruster components deactivated (prevents NS_ThrusterTrailAdditive_Seamless's
    // unscaled spherical yellow glow sprite from overlapping the stern nozzles like an explosion).
    if (CachedThrusterSys.IsValid())
    {
        const FVector ThrusterOffsets[] = {
            C + FVector(-E.X * 1.02f, -E.Y * 0.30f, 0.0f),
            C + FVector(-E.X * 1.02f,  E.Y * 0.30f, 0.0f)
        };
        for (int32 i = 0; i < 2; ++i)
        {
            UNiagaraComponent* Comp = NewObject<UNiagaraComponent>(this);
            Comp->SetupAttachment(VisualRoot);
            Comp->SetAsset(CachedThrusterSys.Get());
            Comp->SetRelativeLocation(ThrusterOffsets[i]);
            Comp->SetRelativeRotation(FRotator(0.f, 90.f, 90.f));
            Comp->SetRelativeScale3D(FVector(0.10f));
            Comp->SetAutoActivate(false);
            Comp->RegisterComponent();
            Comp->Deactivate();
            ThrusterVFX.Add(Comp);
        }
    }

    // 2. Dual Wing Ribbon Slipstream Trails (Origin-centered at port/starboard rear wingtips inside shield)
    if (CachedWingTrailSys.IsValid())
    {
        const FVector WingOffsets[] = {
            C + FVector(-E.X * 0.72f, -E.Y * 0.78f, 0.0f),
            C + FVector(-E.X * 0.72f,  E.Y * 0.78f, 0.0f)
        };
        for (int32 i = 0; i < 2; ++i)
        {
            UNiagaraComponent* Comp = NewObject<UNiagaraComponent>(this);
            Comp->SetupAttachment(VisualRoot);
            Comp->SetAsset(CachedWingTrailSys.Get());
            Comp->SetRelativeLocation(WingOffsets[i]);
            Comp->SetRelativeScale3D(FVector(0.16f));
            Comp->SetVariableFloat(TEXT("User.Width"), 8.0f);
            Comp->RegisterComponent();
            WingTrailVFX.Add(Comp);
        }
    }

    // 3. Ellipsoid-Scaled Faction-Colored Shield Impact Meshes (flush with M_ShieldBubble surface)
    // Enemy ships render crimson-red wireframe + red Fresnel rim + localized impact hotspot;
    // friendly ships render cyan-blue wireframe + blue Fresnel rim + localized impact hotspot.
    static TWeakObjectPtr<UStaticMesh> CachedShieldSphereMesh;
    static TWeakObjectPtr<UMaterialInterface> CachedShieldWFMat;
    static TWeakObjectPtr<UMaterialInterface> CachedShieldGlowMat;
    if (!CachedShieldSphereMesh.IsValid())
        CachedShieldSphereMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"), nullptr, LOAD_NoWarn);
    if (!CachedShieldWFMat.IsValid())
        CachedShieldWFMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Presentation/Effects/M_ShieldImpactWF.M_ShieldImpactWF"), nullptr, LOAD_NoWarn);
    if (!CachedShieldGlowMat.IsValid())
        CachedShieldGlowMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Presentation/Effects/M_ShieldImpactGlow.M_ShieldImpactGlow"), nullptr, LOAD_NoWarn);

    const FVector MeshShieldScale = GetLocalShieldRadii() * (1.006 / 50.0);
    if (CachedShieldSphereMesh.IsValid())
    {
        if (CachedShieldWFMat.IsValid())
        {
            ShieldImpactWFMesh = NewObject<UStaticMeshComponent>(this, TEXT("ShieldImpactWFMesh"));
            AddInstanceComponent(ShieldImpactWFMesh);
            ShieldImpactWFMesh->SetupAttachment(VisualRoot);
            ShieldImpactWFMesh->SetStaticMesh(CachedShieldSphereMesh.Get());
            ShieldImpactWFMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            ShieldImpactWFMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
            ShieldImpactWFMesh->SetGenerateOverlapEvents(false);
            ShieldImpactWFMesh->SetCanEverAffectNavigation(false);
            ShieldImpactWFMesh->SetCastShadow(false);
            ShieldImpactWFMesh->SetRelativeLocation(FVector::ZeroVector);
            ShieldImpactWFMesh->SetRelativeScale3D(MeshShieldScale);
            ShieldImpactWFDyn = UMaterialInstanceDynamic::Create(CachedShieldWFMat.Get(), this);
            if (ShieldImpactWFDyn)
            {
                ShieldImpactWFMesh->SetMaterial(0, ShieldImpactWFDyn);
            }
            ShieldImpactWFMesh->SetVisibility(false);
            ShieldImpactWFMesh->RegisterComponent();
        }
        if (CachedShieldGlowMat.IsValid())
        {
            ShieldImpactGlowMesh = NewObject<UStaticMeshComponent>(this, TEXT("ShieldImpactGlowMesh"));
            AddInstanceComponent(ShieldImpactGlowMesh);
            ShieldImpactGlowMesh->SetupAttachment(VisualRoot);
            ShieldImpactGlowMesh->SetStaticMesh(CachedShieldSphereMesh.Get());
            ShieldImpactGlowMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            ShieldImpactGlowMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
            ShieldImpactGlowMesh->SetGenerateOverlapEvents(false);
            ShieldImpactGlowMesh->SetCanEverAffectNavigation(false);
            ShieldImpactGlowMesh->SetCastShadow(false);
            ShieldImpactGlowMesh->SetRelativeLocation(FVector::ZeroVector);
            ShieldImpactGlowMesh->SetRelativeScale3D(MeshShieldScale * 1.004f);
            ShieldImpactGlowDyn = UMaterialInstanceDynamic::Create(CachedShieldGlowMat.Get(), this);
            if (ShieldImpactGlowDyn)
            {
                ShieldImpactGlowMesh->SetMaterial(0, ShieldImpactGlowDyn);
            }
            ShieldImpactGlowMesh->SetVisibility(false);
            ShieldImpactGlowMesh->RegisterComponent();
        }
    }

    // Keep standby ShieldVFX component deactivated (faction-colored impact is rendered by ShieldImpactWFMesh/GlowMesh
    // so enemy hits never display NS_ShieldStandard's hardcoded blue particle color or demo Points sprites).
    if (CachedShieldSys.IsValid())
    {
        const FVector ShieldScale = GetLocalShieldRadii() / 1000.0;
        ShieldVFX = NewObject<UNiagaraComponent>(this);
        ShieldVFX->SetupAttachment(VisualRoot);
        ShieldVFX->SetAsset(CachedShieldSys.Get());
        ShieldVFX->SetRelativeLocation(FVector::ZeroVector);
        ShieldVFX->SetRelativeScale3D(ShieldScale);
        ShieldVFX->SetVariableFloat(TEXT("User.SpriteSize"), 0.0f);
        ShieldVFX->SetVariableVec2(TEXT("User.SpriteSize"), FVector2D::ZeroVector);
        ShieldVFX->SetAutoActivate(false);
        ShieldVFX->RegisterComponent();
        ShieldVFX->Deactivate();
    }
}

void AOVShip::RefreshShipVisualVFX(float DeltaSeconds)
{
    EnsureShipVFXInitialized();

    const float SafeDt = FMath::Clamp(DeltaSeconds, 0.0f, 0.25f);
    const FVector E = GetVisualHullHalfExtent();
    const FVector C = GetVisualHullCenterOffset();
    const float CurrentSpeed = Velocity.Size();
    const float SpeedRatio = MoveSpeed > 0.0f ? FMath::Clamp(CurrentSpeed / MoveSpeed, 0.0f, 1.0f) : 0.0f;
    const bool bMoving = !bHold && CurrentSpeed > 10.0f && !bDissolving && (!Systems || !Systems->bWrecked);

    // Keep directional thruster ignition cores + blue-white ion jets strictly aft of stern nozzles (zero hull overlap)
    // Compact proportions stay cleanly inside the ship's ellipsoid shield bubble while streaming straight astern (-X).
    const bool bThrusterAlive = !bDissolving && (!Systems || !Systems->bWrecked);
    const float CoreRadius = bMoving
        ? FMath::Clamp(E.Y * FMath::Lerp(0.105f, 0.125f, SpeedRatio), 4.6f, 6.6f)
        : FMath::Clamp(E.Y * 0.095f, 4.2f, 5.8f);
    const float JetRadius = bMoving
        ? FMath::Clamp(E.Y * FMath::Lerp(0.115f, 0.140f, SpeedRatio), 5.0f, 7.6f)
        : FMath::Clamp(E.Y * 0.105f, 4.6f, 6.6f);
    const float JetLength = bMoving
        ? FMath::Clamp(E.X * FMath::Lerp(0.19f, 0.24f, SpeedRatio), 30.0f, 41.0f)
        : FMath::Clamp(E.X * 0.17f, 26.0f, 32.0f);
    const float CoreIntensity = bMoving ? FMath::Lerp(0.76f, 0.96f, SpeedRatio) : 0.68f;
    const float JetIntensity = bMoving ? FMath::Lerp(0.95f, 1.25f, SpeedRatio) : 0.85f;
    const FRotator AsternZRot = FRotationMatrix::MakeFromZ(FVector(-1.0f, 0.0f, 0.0f)).Rotator();

    for (int32 i = 0; i < 2; ++i)
    {
        const float Sign = (i == 0) ? -1.0f : 1.0f;
        const FVector NozzleExit = C + FVector(-E.X * 1.015f, Sign * E.Y * 0.30f, 0.0f);

        if (ThrusterCoreMeshes.IsValidIndex(i) && IsValid(ThrusterCoreMeshes[i]))
        {
            UStaticMeshComponent* CoreComp = ThrusterCoreMeshes[i];
            CoreComp->SetVisibility(bThrusterAlive);
            CoreComp->SetRelativeLocation(NozzleExit + FVector(-CoreRadius * 0.70f, 0.0f, 0.0f));
            CoreComp->SetRelativeRotation(FRotator::ZeroRotator);
            CoreComp->SetRelativeScale3D(FVector(
                (CoreRadius * 1.15f) / 50.0f,
                CoreRadius / 50.0f,
                CoreRadius / 50.0f));
            if (ThrusterCoreDyns.IsValidIndex(i) && IsValid(ThrusterCoreDyns[i]))
            {
                ThrusterCoreDyns[i]->SetVectorParameterValue(TEXT("CoreColor"), FLinearColor(1.0f, 0.90f, 0.68f, 1.0f));
                ThrusterCoreDyns[i]->SetVectorParameterValue(TEXT("HaloColor"), FLinearColor(1.0f, 0.36f, 0.02f, 1.0f));
                ThrusterCoreDyns[i]->SetScalarParameterValue(TEXT("Intensity"), CoreIntensity);
            }
        }

        if (ThrusterJetMeshes.IsValidIndex(i) && IsValid(ThrusterJetMeshes[i]))
        {
            UStaticMeshComponent* JetComp = ThrusterJetMeshes[i];
            JetComp->SetVisibility(bThrusterAlive);
            JetComp->SetRelativeLocation(NozzleExit + FVector(-CoreRadius * 0.55f - JetLength * 0.5f, 0.0f, 0.0f));
            JetComp->SetRelativeRotation(AsternZRot);
            JetComp->SetRelativeScale3D(FVector(
                JetRadius / 50.0f,
                JetRadius / 50.0f,
                JetLength / 100.0f));
            if (ThrusterJetDyns.IsValidIndex(i) && IsValid(ThrusterJetDyns[i]))
            {
                ThrusterJetDyns[i]->SetVectorParameterValue(TEXT("OuterColor"), FLinearColor(0.03f, 0.32f, 1.0f, 1.0f));
                ThrusterJetDyns[i]->SetVectorParameterValue(TEXT("InnerColor"), FLinearColor(0.68f, 0.90f, 1.0f, 1.0f));
                ThrusterJetDyns[i]->SetScalarParameterValue(TEXT("Intensity"), JetIntensity);
            }
        }
    }

    // Keep standby legacy Niagara thruster components deactivated and synced to nozzle positions
    for (int32 i = 0; i < ThrusterVFX.Num(); ++i)
    {
        UNiagaraComponent* Comp = ThrusterVFX[i];
        if (IsValid(Comp))
        {
            const float Sign = (i == 0) ? -1.0f : 1.0f;
            Comp->SetRelativeLocation(C + FVector(-E.X * 1.02f, Sign * E.Y * 0.30f, 0.0f));
            if (Comp->IsActive())
            {
                Comp->Deactivate();
            }
        }
    }

    // Wing trails activate dynamically when maneuvering/cruising
    for (int32 i = 0; i < WingTrailVFX.Num(); ++i)
    {
        UNiagaraComponent* Comp = WingTrailVFX[i];
        if (IsValid(Comp))
        {
            const float Sign = (i == 0) ? -1.0f : 1.0f;
            Comp->SetRelativeLocation(C + FVector(-E.X * 0.72f, Sign * E.Y * 0.78f, 0.0f));
            Comp->SetRelativeScale3D(FVector(0.16f));
            Comp->SetVariableFloat(TEXT("User.Width"), 8.0f);
            if (bMoving && !Comp->IsActive())
            {
                Comp->Activate(false);
            }
            else if (!bMoving && Comp->IsActive())
            {
                Comp->Deactivate();
            }
        }
    }

    // Keep faction-colored shield impact meshes synchronized and smoothly decay after impact flash duration
    if (IsValid(ShieldVFX) && ShieldVFX->IsActive())
    {
        ShieldVFX->Deactivate();
    }
    if (ShieldImpactRemaining > 0.0f)
    {
        ShieldImpactRemaining = FMath::Max(0.0f, ShieldImpactRemaining - SafeDt);
        UpdateShieldImpactVisuals(ShieldImpactRemaining / 0.65f);
    }
    else
    {
        UpdateShieldImpactVisuals(0.0f);
    }

    // Dissolve sequence on wreck
    if (bDissolving)
    {
        const float Step = SafeDt * 0.45f;
        DissolveProgress = FMath::Clamp(DissolveProgress + Step, 0.0f, 1.0f);
        if (Hull)
        {
            for (int32 SlotIdx = 0; SlotIdx < Hull->GetNumMaterials(); ++SlotIdx)
            {
                if (UMaterialInstanceDynamic* DynMat = Cast<UMaterialInstanceDynamic>(Hull->GetMaterial(SlotIdx)))
                {
                    DynMat->SetScalarParameterValue(TEXT("DissolveAmount"), DissolveProgress);
                    DynMat->SetScalarParameterValue(TEXT("DissolvePower"), DissolveProgress);
                    DynMat->SetScalarParameterValue(TEXT("Dissolve"), DissolveProgress);
                }
            }
        }
        for (UStaticMeshComponent* Part : AccentMeshes)
        {
            if (IsValid(Part))
            {
                if (UMaterialInstanceDynamic* DynMat = Cast<UMaterialInstanceDynamic>(Part->GetMaterial(0)))
                {
                    DynMat->SetScalarParameterValue(TEXT("DissolveAmount"), DissolveProgress);
                    DynMat->SetScalarParameterValue(TEXT("DissolvePower"), DissolveProgress);
                }
            }
        }
        if (DissolveProgress >= 1.0f)
        {
            for (UNiagaraComponent* Comp : ThrusterVFX) if (IsValid(Comp)) Comp->Deactivate();
            for (UStaticMeshComponent* Comp : ThrusterCoreMeshes) if (IsValid(Comp)) Comp->SetVisibility(false);
            for (UStaticMeshComponent* Comp : ThrusterJetMeshes) if (IsValid(Comp)) Comp->SetVisibility(false);
            for (UNiagaraComponent* Comp : WingTrailVFX) if (IsValid(Comp)) Comp->Deactivate();
            if (IsValid(ShieldVFX)) ShieldVFX->Deactivate();
            UpdateShieldImpactVisuals(0.0f);
        }
    }
}

void AOVShip::TriggerShieldImpact(FVector HitLocation, FVector HitNormal, float Damage)
{
    EnsureShipVFXInitialized();
    if (IsValid(ShieldVFX) && ShieldVFX->IsActive())
    {
        ShieldVFX->Deactivate();
    }
    LastShieldHitLocation = HitLocation;
    LastShieldHitStrength = FMath::Clamp(Damage / 45.0f, 0.75f, 1.85f);
    ShieldImpactRemaining = 0.65f;
    UpdateShieldImpactVisuals(1.0f);
    if (UOVAudioSubsystem* Audio = UOVAudioSubsystem::Get(this))
    {
        Audio->PlayCombatSFX(EOVAudioCue::ShieldHit, HitLocation);
    }
}

void AOVShip::TriggerWreckDissolve()
{
    if (bDissolving) return;
    bDissolving = true;
    DissolveProgress = 0.0f;
    if (UOVAudioSubsystem* Audio = UOVAudioSubsystem::Get(this))
    {
        Audio->PlayCombatSFX(EOVAudioCue::WreckExplosion, GetActorLocation());
    }

    // Swap to burning dissolve material across all hull material slots on wreck
    static TWeakObjectPtr<UMaterialInterface> CachedDissolveMat;
    if (!CachedDissolveMat.IsValid())
    {
        CachedDissolveMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/MixedVFX/Materials/M_DissolverMaster.M_DissolverMaster"), nullptr, LOAD_NoWarn);
        if (!CachedDissolveMat.IsValid())
        {
            CachedDissolveMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/MixedVFX/Materials/M_SimpleDissolver.M_SimpleDissolver"), nullptr, LOAD_NoWarn);
        }
    }
    if (CachedDissolveMat.IsValid() && Hull)
    {
        if (UMaterialInstanceDynamic* DissolveDyn = UMaterialInstanceDynamic::Create(CachedDissolveMat.Get(), this))
        {
            // Color_01 = dark charred gunmetal hull body; Color_02 = scorching molten orange-red fracture border
            DissolveDyn->SetVectorParameterValue(TEXT("Color_01"), FLinearColor(0.015f, 0.015f, 0.020f, 1.0f));
            DissolveDyn->SetVectorParameterValue(TEXT("Color_02"), FLinearColor(4.2f, 0.45f, 0.02f, 1.0f));
            DissolveDyn->SetScalarParameterValue(TEXT("DissolveTilling"), 8.0f);
            DissolveDyn->SetScalarParameterValue(TEXT("DissolvePower"), 0.48f);
            const int32 NumMats = FMath::Max(1, Hull->GetNumMaterials());
            for (int32 SlotIdx = 0; SlotIdx < NumMats; ++SlotIdx)
            {
                Hull->SetMaterial(SlotIdx, DissolveDyn);
            }
        }
    }
}

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVShipMotionTest, "OrbitalVanguard.Ship.FiniteTurnXYZArrival",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOVShipMotionTest::RunTest(const FString& Parameters)
{
    FVector Position = FVector::ZeroVector;
    FVector Velocity = FVector::ZeroVector;
    FQuat Rotation = FQuat::Identity;
    const FVector Target(0, 1000, 400);
    OVShipMotion::Advance(Position, Rotation, Velocity, Target, 700, 30, 0.1f);
    TestTrue(TEXT("Turning is limited to three degrees in 0.1 seconds"),
        FQuat::Identity.AngularDistance(Rotation) <= FMath::DegreesToRadians(3.01));
    TestTrue(TEXT("Ship waits while facing away"), Position.IsNearlyZero());
    const FVector PausedPosition = Position;
    const FQuat PausedRotation = Rotation;
    OVShipMotion::Advance(Position, Rotation, Velocity, Target, 700, 30, 0.0f);
    TestTrue(TEXT("Zero simulation time preserves position and heading"),
        Position.Equals(PausedPosition) && Rotation.Equals(PausedRotation));
    bool Arrived = false;
    for (int32 Index = 0; Index < 1800 && !Arrived; ++Index)
        Arrived = OVShipMotion::Advance(Position, Rotation, Velocity, Target, 700, 30, 1.0f / 60.0f);
    TestTrue(TEXT("Ship reaches elevated XYZ destination"), Arrived && Position.Equals(Target, 0.01));
    const FVector FinalPosition = Position;
    OVShipMotion::Advance(Position, Rotation, Velocity, Target, 700, 30, 0.1f);
    TestTrue(TEXT("Arrival does not overshoot"), Position.Equals(FinalPosition, 0.01));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVShipClassTurnTest, "OrbitalVanguard.Ship.ClassTurnRates",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOVShipClassTurnTest::RunTest(const FString& Parameters)
{
    FVector EscortPosition = FVector::ZeroVector;
    FVector CruiserPosition = FVector::ZeroVector;
    FVector EscortVelocity = FVector::ZeroVector, CruiserVelocity = FVector::ZeroVector;
    FQuat EscortRotation = FQuat::Identity;
    FQuat CruiserRotation = FQuat::Identity;
    const FVector Target(0, 10000, 5000);
    // Same destination and elapsed simulation time, different class maneuverability.
    for (int32 Frame = 0; Frame < 30; ++Frame)
    {
        OVShipMotion::Advance(EscortPosition, EscortRotation, EscortVelocity, Target, 900, 80, 1.0f / 60.0f);
        OVShipMotion::Advance(CruiserPosition, CruiserRotation, CruiserVelocity, Target, 450, 25, 1.0f / 60.0f);
    }
    const double EscortAngle = FMath::RadiansToDegrees(FQuat::Identity.AngularDistance(EscortRotation));
    const double CruiserAngle = FMath::RadiansToDegrees(FQuat::Identity.AngularDistance(CruiserRotation));
    TestTrue(TEXT("Escort turns approximately forty degrees in half a second"), FMath::Abs(EscortAngle - 40.0) < 0.05);
    TestTrue(TEXT("Cruiser turns approximately twelve and a half degrees"), FMath::Abs(CruiserAngle - 12.5) < 0.05);
    TestTrue(TEXT("Light vessel aligns sooner than heavy vessel"), EscortAngle > CruiserAngle * 2.0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVShipTimeScaleTest, "OrbitalVanguard.Ship.TimeScaleAndStoppedArrival",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOVShipTimeScaleTest::RunTest(const FString& Parameters)
{
    FVector EndPositions[3];
    for (int32 Scale = 1; Scale <= 3; ++Scale)
    {
        FVector Position = FVector::ZeroVector;
    FVector Velocity = FVector::ZeroVector;
        FQuat Rotation = FQuat::Identity;
        // 3 real seconds at 1x, 1.5 at 2x, and 1 at 3x all mean 3 simulated seconds.
        const int32 RealFrames = 180 / Scale;
        for (int32 Frame = 0; Frame < RealFrames; ++Frame)
            OVShipMotion::Advance(Position, Rotation, Velocity, FVector(10000, 0, 0), 700, 60, Scale / 60.0f);
        EndPositions[Scale - 1] = Position;
        TestTrue(FString::Printf(TEXT("%dx covers the same accelerated distance"), Scale),
            Position.Equals(FVector(1955.88235294, 0, 0), 0.1));
    }
    TestTrue(TEXT("1x, 2x and 3x integrate the same simulated duration"),
        EndPositions[0].Equals(EndPositions[1], 0.1) && EndPositions[1].Equals(EndPositions[2], 0.1));
    FVector Position = FVector::ZeroVector;
    FVector Velocity = FVector::ZeroVector;
    FQuat Rotation = FQuat::Identity;
    const FVector NearbyTarget(20, 0, 0);
    const bool Arrived = OVShipMotion::Advance(Position, Rotation, Velocity, NearbyTarget, 700, 60, 0.5f);
    TestTrue(TEXT("Large final step clamps to destination"), Arrived && Position.Equals(NearbyTarget));
    for (int32 Frame = 0; Frame < 180; ++Frame)
        OVShipMotion::Advance(Position, Rotation, Velocity, NearbyTarget, 700, 60, 1.0f / 60.0f);
    TestTrue(TEXT("Ship stays stationary for three simulated seconds after arrival"), Position.Equals(NearbyTarget));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVInertiaFrames, "OrbitalVanguard.Ship.InertialRetargetAtLowFPS",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOVInertiaFrames::RunTest(const FString&)
{
    FVector Baseline, BaselineVelocity;
    for (const int FPS : {60, 10})
        for (const int Speed : {1,2,3})
        {
            FVector Position=FVector::ZeroVector, Velocity(800,0,0);
            FQuat Rotation=FQuat::Identity;
            const float Dt=float(Speed)/FPS;
            for (int I=0; I<3*FPS/Speed; ++I)
            {
                OVShipMotion::Advance(Position,Rotation,Velocity,FVector(0,20000,0),1600,20,Dt,400,600,5,true,FQuat::Identity,180);
                if (I==0) TestTrue(TEXT("Retarget retains previous transverse momentum"),Velocity.X>700 && Velocity.Y>0);
            }
            if (FPS==60 && Speed==1) { Baseline=Position; BaselineVelocity=Velocity; }
            TestTrue(TEXT("Equal game time across frame rates and speeds"),Position.Equals(Baseline,.1) && Velocity.Equals(BaselineVelocity,.1));
            TestTrue(TEXT("Broadside attitude retained"),Rotation.Equals(FQuat::Identity,.001));
        }
    return true;
}
#endif
