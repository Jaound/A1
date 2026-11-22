#include "OVCombatVFXSubsystem.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "OVShip.h"
#include "OVShipSystemsComponent.h"

void UOVCombatVFXSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    EnsureAssetsLoaded();
}

void UOVCombatVFXSubsystem::Deinitialize()
{
    ActiveProjectiles.Reset();
    TransientEffects.Reset();
    if (IsValid(VFXRootActor))
    {
        VFXRootActor->Destroy();
        VFXRootActor = nullptr;
    }
    Super::Deinitialize();
}

void UOVCombatVFXSubsystem::EnsureAssetsLoaded()
{
    if (CannonShellMesh) return;

    CannonShellMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Presentation/Effects/CannonShell/SM_Source.SM_Source"), nullptr, LOAD_NoWarn);
    LaserBeamMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Presentation/Effects/LaserBeam/SM_Source.SM_Source"), nullptr, LOAD_NoWarn);
    ImpactBurstMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Presentation/Effects/ImpactBurst/SM_Source.SM_Source"), nullptr, LOAD_NoWarn);
    ShockwaveSphereMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"), nullptr, LOAD_NoWarn);

    FriendlyBeamMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Presentation/Effects/LaserBeam/M_Friendly.M_Friendly"), nullptr, LOAD_NoWarn);
    EnemyBeamMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Presentation/Effects/LaserBeam/M_Enemy.M_Enemy"), nullptr, LOAD_NoWarn);
    CannonShellMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Presentation/Effects/CannonShell/M_Source.M_Source"), nullptr, LOAD_NoWarn);
    EffectGlowMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Presentation/Effects/M_EffectGlow.M_EffectGlow"), nullptr, LOAD_NoWarn);

    // Translucent refractive distortion material (never use opaque M_SceneMaterialMaster)
    DistortionMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/MixedVFX/Materials/MaterialInstances/MI_Aura_01.MI_Aura_01"), nullptr, LOAD_NoWarn);
    if (!DistortionMat)
    {
        DistortionMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/SpaceshipVFXStarter/Materials/Instances/MI_FresnelGlow_Standard.MI_FresnelGlow_Standard"), nullptr, LOAD_NoWarn);
    }

    ShieldImpactMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Presentation/Effects/ImpactBurst/M_ShieldImpact.M_ShieldImpact"), nullptr, LOAD_NoWarn);
    HullImpactMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Presentation/Effects/ImpactBurst/M_HullImpact.M_HullImpact"), nullptr, LOAD_NoWarn);
}

void UOVCombatVFXSubsystem::EnsureRootActor()
{
    if (IsValid(VFXRootActor)) return;
    UWorld* World = GetWorld();
    if (!World) return;

    FActorSpawnParameters Params;
    Params.ObjectFlags |= RF_Transient;
    VFXRootActor = World->SpawnActor<AActor>(AActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
    if (VFXRootActor)
    {
        USceneComponent* Root = NewObject<USceneComponent>(VFXRootActor);
        VFXRootActor->SetRootComponent(Root);
        Root->RegisterComponent();
    }
}

void UOVCombatVFXSubsystem::SynchronizeVFX(float DeltaSeconds)
{
    EnsureAssetsLoaded();
    EnsureRootActor();
    if (!IsValid(VFXRootActor)) return;

    UWorld* World = GetWorld();
    if (!World) return;

    UOVCombatSubsystem* Combat = World->GetSubsystem<UOVCombatSubsystem>();
    if (!Combat || !Combat->bEnabled)
    {
        for (auto& Item : ActiveProjectiles)
        {
            if (IsValid(Item.MeshComponent)) Item.MeshComponent->DestroyComponent();
        }
        ActiveProjectiles.Reset();
        for (auto& Effect : TransientEffects)
        {
            if (IsValid(Effect.MeshComponent)) Effect.MeshComponent->DestroyComponent();
        }
        TransientEffects.Reset();
        ProcessedHitEvents.Reset();
        LastCombatGameTime = -1.0;
        return;
    }

    if (LastCombatGameTime >= 0.0 && Combat->GameTime < LastCombatGameTime)
    {
        ProcessedHitEvents.Reset();
    }
    LastCombatGameTime = Combat->GameTime;

    // 1. Synchronize in-flight Laser Beams and Cannon Projectiles ONLY.
    // Strictly skip EOVAttackKind::Missile so the existing missile presentation renders untouched.
    TSet<int32> CurrentAttackIds;
    for (const FOVAttackState& Attack : Combat->Attacks)
    {
        if (Attack.Result != EOVAttackResult::Flying) continue;
        if (Attack.Weapon.Kind != EOVAttackKind::Beam && Attack.Weapon.Kind != EOVAttackKind::Cannon) continue;

        CurrentAttackIds.Add(Attack.AttackId);

        FOVActiveAttackMesh* Found = ActiveProjectiles.FindByPredicate(
            [&Attack](const FOVActiveAttackMesh& M) { return M.AttackId == Attack.AttackId; });

        if (!Found)
        {
            FOVActiveAttackMesh NewItem;
            NewItem.AttackId = Attack.AttackId;
            NewItem.Kind = Attack.Weapon.Kind;

            UStaticMesh* MeshToUse = (Attack.Weapon.Kind == EOVAttackKind::Beam) ? LaserBeamMesh : CannonShellMesh;
            UMaterialInterface* MatToUse = (Attack.Weapon.Kind == EOVAttackKind::Beam)
                ? (Attack.FactionId == 0 ? FriendlyBeamMat : EnemyBeamMat)
                : CannonShellMat;

            if (MeshToUse)
            {
                UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(VFXRootActor);
                Comp->SetupAttachment(VFXRootActor->GetRootComponent());
                Comp->SetStaticMesh(MeshToUse);
                Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                Comp->SetCastShadow(false);
                if (MatToUse)
                {
                    NewItem.DynMaterial = UMaterialInstanceDynamic::Create(MatToUse, Comp);
                    Comp->SetMaterial(0, NewItem.DynMaterial);
                }
                Comp->RegisterComponent();
                NewItem.MeshComponent = Comp;
            }
            ActiveProjectiles.Add(NewItem);
            Found = &ActiveProjectiles.Last();
        }

        if (Found && IsValid(Found->MeshComponent))
        {
            if (Attack.Weapon.Kind == EOVAttackKind::Beam)
            {
                // LaserBeam/SM_Source is a Z-axis cylinder centered at (0,0,0) with BoxExtent Z = 50cm (100cm total height).
                // Place at midpoint between Origin and Position and align +Z along beam vector.
                const FVector Delta = Attack.Position - Attack.Origin;
                const float Length = Delta.Size();
                const FVector Dir = Length > 1.0f ? (Delta / Length) : FVector::ForwardVector;
                const FRotator ZRot = FRotationMatrix::MakeFromZ(Dir).Rotator();

                Found->MeshComponent->SetWorldLocation((Attack.Origin + Attack.Position) * 0.5f);
                Found->MeshComponent->SetWorldRotation(ZRot);
                Found->MeshComponent->SetWorldScale3D(FVector(0.11f, 0.11f, FMath::Max(0.20f, Length / 100.0f)));
            }
            else if (Attack.Weapon.Kind == EOVAttackKind::Cannon)
            {
                // CannonShell/SM_Source has its nose along +Z (BoxExtent Z = 50cm).
                // Align +Z along flight direction so the shell flies horizontally nose-first.
                const FVector Dir = Attack.Direction.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
                const FRotator ZRot = FRotationMatrix::MakeFromZ(Dir).Rotator();

                Found->MeshComponent->SetWorldLocation(Attack.Position);
                Found->MeshComponent->SetWorldRotation(ZRot);
                Found->MeshComponent->SetWorldScale3D(FVector(0.14f, 0.14f, 0.88f));
            }
        }
    }

    // Clean up finished attack meshes
    for (int32 i = ActiveProjectiles.Num() - 1; i >= 0; --i)
    {
        if (!CurrentAttackIds.Contains(ActiveProjectiles[i].AttackId))
        {
            if (IsValid(ActiveProjectiles[i].MeshComponent))
            {
                ActiveProjectiles[i].MeshComponent->DestroyComponent();
            }
            ActiveProjectiles.RemoveAt(i);
        }
    }

    // 2. Consume RecentEvents for Sci-Fi Plasma Impact Sparks & Refractive Distortion Shockwaves
    for (const FOVAttackState& Evt : Combat->RecentEvents)
    {
        if (ProcessedHitEvents.Contains(Evt.AttackId)) continue;
        ProcessedHitEvents.Add(Evt.AttackId);

        if (Evt.Result == EOVAttackResult::Hit)
        {
            const bool bShieldHit = !Evt.bShieldDepleted && Evt.HullDamage <= 0.0f;
            bool bEnemyTarget = (Evt.FactionId == 0);
            for (const FOVCombatShipState& ShipState : Combat->Ships)
            {
                if (IsValid(ShipState.Ship) && ShipState.Ship->GetFName() == Evt.HitShipId)
                {
                    bEnemyTarget = !ShipState.Ship->bFriendly
                        || (ShipState.Ship->Systems && ShipState.Ship->Systems->FactionId != 0);
                    break;
                }
            }

            SpawnImpactSlashSparks(Evt.Position, Evt.Direction, bShieldHit, bEnemyTarget);

            if (Evt.bShieldDepleted || Evt.Explosions > 0)
            {
                SpawnShockwaveDistortion(Evt.Position, 380.0f, bEnemyTarget);
            }
        }
    }

    // 3. Advance transient effects (sci-fi impact flashes, distortion shockwaves)
    const float SafeDt = FMath::Clamp(DeltaSeconds, 0.0f, 0.1f);
    for (int32 i = TransientEffects.Num() - 1; i >= 0; --i)
    {
        FOVTransientEffect& Effect = TransientEffects[i];
        Effect.Age += SafeDt;
        const float Alpha = FMath::Clamp(Effect.Age / FMath::Max(0.01f, Effect.Lifetime), 0.0f, 1.0f);

        if (IsValid(Effect.MeshComponent))
        {
            const FVector CurScale = FMath::Lerp(Effect.BaseScale, Effect.TargetScale, Alpha);
            Effect.MeshComponent->SetWorldScale3D(CurScale);

            if (Effect.DynMaterial)
            {
                const float Fade = 1.0f - Alpha;
                Effect.DynMaterial->SetScalarParameterValue(TEXT("Opacity"), Fade * (Effect.bDistortion ? 0.35f : 0.80f));
                Effect.DynMaterial->SetScalarParameterValue(TEXT("DistortionPower"), Fade * 0.08f);
                Effect.DynMaterial->SetScalarParameterValue(TEXT("Density"), Fade * 0.45f);
                Effect.DynMaterial->SetScalarParameterValue(TEXT("Multi"), Fade * 1.4f);
            }
        }

        if (Alpha >= 1.0f)
        {
            if (IsValid(Effect.MeshComponent)) Effect.MeshComponent->DestroyComponent();
            TransientEffects.RemoveAt(i);
        }
    }
}

void UOVCombatVFXSubsystem::SpawnShockwaveDistortion(FVector Location, float Radius, bool bEnemyTarget)
{
    EnsureAssetsLoaded();
    EnsureRootActor();
    if (!IsValid(VFXRootActor) || !ShockwaveSphereMesh || !DistortionMat) return;

    UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(VFXRootActor);
    Comp->SetupAttachment(VFXRootActor->GetRootComponent());
    Comp->SetStaticMesh(ShockwaveSphereMesh);
    Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Comp->SetCastShadow(false);
    Comp->SetWorldLocation(Location);

    FOVTransientEffect Effect;
    Effect.MeshComponent = Comp;
    Effect.Age = 0.0f;
    Effect.Lifetime = 0.34f;
    Effect.BaseScale = FVector(0.35f);
    Effect.TargetScale = FVector(FMath::Clamp(Radius / 50.0f, 1.5f, 8.5f));
    Effect.bDistortion = true;

    Effect.DynMaterial = UMaterialInstanceDynamic::Create(DistortionMat, Comp);
    if (Effect.DynMaterial)
    {
        const FLinearColor ShockColor = bEnemyTarget
            ? FLinearColor(1.0f, 0.06f, 0.03f, 0.28f)
            : FLinearColor(0.20f, 0.72f, 1.0f, 0.25f);
        Effect.DynMaterial->SetScalarParameterValue(TEXT("DistortionPower"), 0.08f);
        Effect.DynMaterial->SetScalarParameterValue(TEXT("Density"), 0.40f);
        Effect.DynMaterial->SetScalarParameterValue(TEXT("Multi"), 1.2f);
        Effect.DynMaterial->SetVectorParameterValue(TEXT("Color"), ShockColor);
        Comp->SetMaterial(0, Effect.DynMaterial);
    }
    Comp->RegisterComponent();
    TransientEffects.Add(Effect);
}

void UOVCombatVFXSubsystem::SpawnImpactSlashSparks(FVector Location, FVector Normal, bool bShield, bool bEnemyTarget)
{
    EnsureAssetsLoaded();
    EnsureRootActor();
    if (!IsValid(VFXRootActor)) return;

    UStaticMesh* MeshToUse = ImpactBurstMesh ? ImpactBurstMesh : ShockwaveSphereMesh;
    UMaterialInterface* MatToUse = EffectGlowMat ? EffectGlowMat : (bShield ? ShieldImpactMat : HullImpactMat);
    if (!MeshToUse || !MatToUse) return;

    const FVector SafeNormal = Normal.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
    const FRotator ZRot = FRotationMatrix::MakeFromZ(SafeNormal).Rotator();

    UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(VFXRootActor);
    Comp->SetupAttachment(VFXRootActor->GetRootComponent());
    Comp->SetStaticMesh(MeshToUse);
    Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Comp->SetCastShadow(false);
    Comp->SetWorldLocation(Location);
    Comp->SetWorldRotation(ZRot);

    FOVTransientEffect Effect;
    Effect.MeshComponent = Comp;
    Effect.Age = 0.0f;
    Effect.Lifetime = 0.22f;
    Effect.BaseScale = FVector(0.22f, 0.22f, 0.35f);
    Effect.TargetScale = bShield ? FVector(0.95f, 0.95f, 0.55f) : FVector(0.75f, 0.75f, 0.90f);
    Effect.bDistortion = false;

    Effect.DynMaterial = UMaterialInstanceDynamic::Create(MatToUse, Comp);
    if (Effect.DynMaterial)
    {
        const FLinearColor SparkColor = bShield
            ? (bEnemyTarget ? FLinearColor(1.0f, 0.05f, 0.02f, 1.0f) : FLinearColor(0.12f, 0.74f, 1.0f, 1.0f))
            : FLinearColor(1.0f, 0.52f, 0.14f, 1.0f);
        Effect.DynMaterial->SetVectorParameterValue(TEXT("Color"), SparkColor);
        Effect.DynMaterial->SetVectorParameterValue(TEXT("FactionTint"), SparkColor);
        Effect.DynMaterial->SetScalarParameterValue(TEXT("Intensity"), 3.2f);
        Effect.DynMaterial->SetScalarParameterValue(TEXT("Opacity"), 0.82f);
        Comp->SetMaterial(0, Effect.DynMaterial);
    }
    Comp->RegisterComponent();
    TransientEffects.Add(Effect);
}
