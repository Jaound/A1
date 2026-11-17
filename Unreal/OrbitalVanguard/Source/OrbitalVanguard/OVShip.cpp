#include "OVShip.h"
#include "OVInteriorSubsystem.h"
#include "OVCombatSubsystem.h"
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

// PC14 uses the full source mesh and source-resolution maps; mount surfaces
// still need art alignment. Combat and selection geometry remain unchanged.
static TAutoConsoleVariable<int32> CVarOVImportedHull(TEXT("ov.Presentation.ImportedHull"),1,
    TEXT("PC14 source-derived preview: 1 enables it on the next scenario creation; 0 restores the placeholder. No combat geometry changes."));
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
        bool bManualAttitude = false, const FQuat& ManualOrientation = FQuat::Identity)
    {
        if (Delta <= 0.0f || !FMath::IsFinite(Delta) || Destination.ContainsNaN()) return false;
        const double Radius = FMath::Max(0.1f, ArrivalRadius);
        float Remaining = Delta;
        while (Remaining > UE_SMALL_NUMBER)
        {
            const float Step = FMath::Min(Remaining, 1.0f / 120.0f);
            const FVector Offset = Destination - Position;
            const double Distance = Offset.Size();
            if (Distance <= Radius)
            {
                Position = Destination;
                Velocity = FVector::ZeroVector;
                return true;
            }
            const FVector Direction = Offset / Distance;
            Turn(Orientation, bManualAttitude ? ManualOrientation : Direction.ToOrientationQuat(), TurnRate, Step);
            const double Alignment = FVector::DotProduct(Orientation.GetForwardVector(), Direction);
            const double Throttle = bManualAttitude ? 1.0 : FMath::Clamp((Alignment - 0.6) / 0.4, 0.0, 1.0);
            const double BrakeRate = FMath::Max(1.0f, Braking);
            const double ArrivalSpeed = FMath::Sqrt(2.0 * BrakeRate * FMath::Max(0.0, Distance - Radius));
            const double DesiredSpeed = FMath::Min(FMath::Max(0.0f, Speed) * Throttle, ArrivalSpeed);
            const double Rate = DesiredSpeed < Velocity.Size() ? BrakeRate : FMath::Max(0.0f, Acceleration);
            const FVector Travel = ChangeVelocity(Velocity, Direction * DesiredSpeed, Rate, Step);
            // Swept arrival catches a high-speed step without teleporting across a lateral miss.
            const double TravelSquared = Travel.SizeSquared();
            const double ClosestT = TravelSquared > UE_SMALL_NUMBER
                ? FMath::Clamp(FVector::DotProduct(Offset, Travel) / TravelSquared, 0.0, 1.0) : 0.0;
            if ((Offset - Travel * ClosestT).SizeSquared() <= Radius * Radius)
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
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cone(TEXT("/Engine/BasicShapes/Cone.Cone"));
    Hull = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Hull"));
    Hull->SetupAttachment(Root);
    Hull->SetStaticMesh(Cube.Object);
    Hull->SetRelativeScale3D(FVector(3.2, 1.0, 0.6));
    Hull->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Hull->SetCollisionResponseToAllChannels(ECR_Ignore);
    Hull->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    Hull->SetGenerateOverlapEvents(false);

    auto AddAccent = [&](const TCHAR* Name, UStaticMesh* Mesh, FVector Location, FVector Scale, FRotator Rotation)
    {
        UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(Name);
        Part->SetupAttachment(Root);
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
    for(const auto& Support:PresentationMountSupports)if(IsValid(Support.Get()))Support->DestroyComponent();
    PresentationMountSupports.Reset();
    if(bSavedHullMaterial){Hull->SetMaterial(0,SavedHullMaterial);SavedHullMaterial=nullptr;bSavedHullMaterial=false;}
    Hull->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
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
    if(CVarOVImportedHull.GetValueOnGameThread()!=1 || DefinitionId!=TEXT("corvette") || !Systems || !Systems->Layout)return;
    auto* Mesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Presentation/Ships/SM_PC14_FullMesh.SM_PC14_FullMesh"),nullptr,LOAD_NoWarn);
    if(!Mesh)return;
    for(const auto& Support:PresentationMountSupports)if(IsValid(Support.Get()))Support->DestroyComponent();
    PresentationMountSupports.Reset();
    if(bSavedHullMaterial){Hull->SetMaterial(0,SavedHullMaterial);SavedHullMaterial=nullptr;bSavedHullMaterial=false;}
    if(CVarOVPC14SourceMaterial.GetValueOnGameThread()==1)
        if(auto* Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Presentation/PC14Full/M_PC14_FullReadable.M_PC14_FullReadable"),nullptr,LOAD_NoWarn))
            if(auto* Instance=UMaterialInstanceDynamic::Create(Material,this)) {
                SavedHullMaterial=Hull->GetMaterial(0);bSavedHullMaterial=true;
                Instance->SetVectorParameterValue(TEXT("FactionTint"),bFriendly?FLinearColor(.8f,.9f,1.f):FLinearColor(1.f,.72f,.7f));
                Hull->SetMaterial(0,Instance);
            }
    const FBoxSphereBounds Bounds=Mesh->GetBounds();
    const FVector Available=Systems->Layout->LocalHalfExtent;
    const FVector Ratio=Available/Bounds.BoxExtent.ComponentMax(FVector(1));
    const double Scale=Ratio.GetMin();
    Hull->SetStaticMesh(Mesh);
    Hull->SetRelativeScale3D(FVector(Scale));
    Hull->SetRelativeLocation(-Bounds.Origin*Scale);
    Hull->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    for(auto Part:AccentMeshes)Part->SetVisibility(false);
    // Provisional adapters measured against the full mesh and existing
    // corvette hardpoints. Final art alignment still requires review;
    // these do not alter the physical mount, muzzle or collision OBB.
    const FVector ExpectedExtent(175,75,30);
    const auto& Mounts=Systems->Layout->WeaponMounts;
    if(!Available.Equals(ExpectedExtent,.01)||Mounts.Num()<2)return;
    const FVector ExpectedPositions[]={FVector(-46.6666667,0,42),FVector(46.6666667,0,-42)};
    const double SurfaceAtUnitScale[]={-2.7250/.7723395451,-29.3247/.7723395451};
    for(int32 I=0;I<2;++I)
    {
        const auto& Mount=Mounts[I];
        if(!Mount.LocalPosition.Equals(ExpectedPositions[I],.1))continue;
        const double Side=Mount.LocalPosition.Z>0?1:-1;
        const double Length=OVWeaponGeometry::BarrelLength(Available,Mount.Tier,Mount.Type==EOVWeaponType::Interception);
        const double BaseInner=Mount.LocalPosition.Z-Side*Length*(.23+.09);
        const double Surface=SurfaceAtUnitScale[I]*Scale;
        const double Height=Side*(BaseInner-Surface);
        if(Height<=.01)continue;
        const FName Name=MakeUniqueObjectName(this,UStaticMeshComponent::StaticClass(),TEXT("PC14MountSupport"));
        auto* Support=NewObject<UStaticMeshComponent>(this,Name);
        AddInstanceComponent(Support);Support->SetupAttachment(GetRootComponent());
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
    if(bSavedHullMaterial){Hull->SetMaterial(0,SavedHullMaterial);SavedHullMaterial=nullptr;bSavedHullMaterial=false;}
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
        bool bDestroyed, int32 ShotId, double ShotTime, double Clock, int32 ShotsFired, bool bCannon)
    {
        FOVWeaponMountDefinition VisualDef = Def;
        if (!bCannon) VisualDef.PresentationProfile = NAME_None;
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
            Visual->SetupAttachment(GetRootComponent());
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
                Systems->bWrecked || !Health || Health->HP <= 0, Mount.LastShotId, Mount.LastShotTime, Combat->GameTime, Mount.ShotsFired, Mount.Profile.Kind == EOVAttackKind::Cannon);
        }
    if (PDState)
        for (const FOVPDMountState& Mount : PDState->Mounts)
        {
            FOVWeaponMountDefinition Def;
            Def.WeaponId = Mount.MountId; Def.Type = EOVWeaponType::Interception;
            Def.LocalPosition = Mount.LocalPosition; Def.Tier = Systems->Layout->WeaponTier;
            const auto* Health = Systems->WeaponMountStates.FindByPredicate(
                [&Mount](const FOVWeaponMountState& H) { return H.WeaponMountId == Mount.MountId; });
            Sync(Def, Mount.LocalAim, false, Systems->bWrecked || Mount.HP <= 0
                || (Mount.bLayoutMount && (!Health || Health->HP <= 0)), Mount.LastShotId, Mount.LastShotTime, PD->GameTime, 0, false);
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
                    if (Mount->Profile.Kind != EOVAttackKind::Cannon) EffectiveDef.PresentationProfile = NAME_None;
                    return GetActorTransform().TransformPosition(OVWeaponGeometry::MuzzleLocal(EffectiveDef,Mount->LocalAim,
                        Systems->Layout->LocalHalfExtent,Mount->Profile.Kind == EOVAttackKind::Missile,Mount->ShotsFired>0?(Mount->ShotsFired-1)%2:0));
                }
    const auto* PD = GetWorld()->GetSubsystem<UOVPointDefenseSubsystem>();
    if (PD)
        if (const auto* State = PD->Ships.FindByPredicate([this](const FOVPDShipState& S) { return S.Ship == this; }))
            if (const auto* Mount = State->Mounts.FindByPredicate([MountId](const FOVPDMountState& M) { return M.MountId == MountId; }))
                return GetActorTransform().TransformPosition(OVWeaponGeometry::MuzzleLocal(Mount->LocalPosition, Mount->LocalAim,
                    OVWeaponGeometry::BarrelLength(Systems->Layout->LocalHalfExtent, Systems->Layout->WeaponTier, true), false));
    return GetActorLocation();
}

void AOVShip::SetDestination(const FVector& Location)
{
    if (!Location.ContainsNaN() && !bFTLCharging && Systems && Systems->CanMove())
    {
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

void AOVShip::Simulate(float SimDelta)
{
    if (!FMath::IsFinite(SimDelta) || SimDelta <= 0.0f) return;
    if (bFTLCharging || !Systems || !Systems->CanMove())
    {
        // Gameplay immobilization is separate from an ordinary inertial stop order.
        Velocity = FVector::ZeroVector;
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
            SimDelta, Acceleration, Braking, ArrivalRadius, bManualAttitude, ManualAttitude.Quaternion()))
        {
            bHasDestination = false;
            bHold = true;
        }
    }
    else
    {
        OVShipMotion::Brake(Position, Velocity, Braking, SimDelta);
        if (bManualAttitude) OVShipMotion::Turn(Orientation, ManualAttitude.Quaternion(), TurnRate * EngineEfficiency * (1.f+Duty.TurnBonus), SimDelta);
    }
    SetActorLocationAndRotation(Position, Orientation, false, nullptr, ETeleportType::TeleportPhysics);
}

void AOVShip::DebugResetMotion()
{
    StopMovement(); Velocity=FVector::ZeroVector; bManualAttitude=false;
    ManualAttitude=GetActorRotation(); bFTLCharging=false; AttackTarget=nullptr; SetAutomation(false,false);
}

void AOVShip::DebugSetFTLCharging(bool bCharging)
{
    bFTLCharging=bCharging;
    if(bCharging) { StopMovement(); Velocity=FVector::ZeroVector; }
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
#endif
