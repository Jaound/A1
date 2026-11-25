#include "OVPointDefenseSubsystem.h"
#include "OVInteriorSubsystem.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

#include "OVCombatSubsystem.h"
#include "OVCarrierSubsystem.h"
#include "OVTransportSubsystem.h"
#include "OVShip.h"
#include "OVShipSystemsComponent.h"
#include "Engine/World.h"

namespace
{
struct FTarget
{
    EOVPDTargetKind Kind;
    int32 Id, Faction;
    FVector Position;
    float Evasion;
};

void GatherTargets(UWorld* World, TArray<FTarget>& Out)
{
    CSV_SCOPED_TIMING_STAT_EXCLUSIVE(OV_PDGather);
    TRACE_CPUPROFILER_EVENT_SCOPE(OV_PDGather);
    Out.Reset();
    if (const auto* Combat = World->GetSubsystem<UOVCombatSubsystem>())
        for (const FOVAttackState& Attack : Combat->Attacks)
            if (Attack.Result == EOVAttackResult::Flying && Attack.InterceptHP > 0
                && (Attack.Weapon.Kind == EOVAttackKind::Missile || Attack.Weapon.Kind == EOVAttackKind::GuidedMissile))
                Out.Add({EOVPDTargetKind::Missile, Attack.AttackId, Attack.FactionId,
                    Attack.Position, Attack.Weapon.InterceptionEvasion});
    if (const auto* Carrier = World->GetSubsystem<UOVCarrierSubsystem>())
        for (const auto& Aircraft : Carrier->Aircraft)
            if (Aircraft.HP > 0 && UOVCarrierSubsystem::IsAirborneState(Aircraft.State))
                Out.Add({EOVPDTargetKind::Aircraft, Aircraft.AircraftId, Aircraft.FactionId, Aircraft.Position, Aircraft.Evasion});
    if (const auto* Transport = World->GetSubsystem<UOVTransportSubsystem>(); Transport && Transport->bEnabled)
        for (const auto& Pod : Transport->Pods)
            if (Pod.HP > 0 && (Pod.State == EOVPodFlightState::Outbound || Pod.State == EOVPodFlightState::Returning))
                Out.Add({EOVPDTargetKind::Pod, Pod.PodId, Pod.FactionId, Pod.Position, Pod.Evasion});
}

bool DamageTarget(UWorld* World, const FTarget& Target, float Damage)
{
    switch (Target.Kind)
    {
    case EOVPDTargetKind::Missile:
        return World->GetSubsystem<UOVCombatSubsystem>()->ApplyInterceptionDamage(Target.Id, Damage);
    case EOVPDTargetKind::Aircraft:
        return World->GetSubsystem<UOVCarrierSubsystem>()->ApplyAircraftDamage(Target.Id, Damage);
    case EOVPDTargetKind::Pod:
        return World->GetSubsystem<UOVTransportSubsystem>()->InterceptPod(Target.Id, Damage);
    }
    return false;
}
}

FOVPDProfile UOVPointDefenseSubsystem::DefaultProfile(EOVPDWeaponKind Kind)
{
    FOVPDProfile Profile;
    Profile.Kind = Kind;
    if (Kind == EOVPDWeaponKind::Kinetic)
    {
        Profile.ProfileId = TEXT("Trial2C_Kinetic");
        Profile.Range = 12600;
        Profile.Accuracy = .62f;
    }
    return Profile;
}

float UOVPointDefenseSubsystem::HitProbability(float Accuracy, float Evasion)
{
    if (!FMath::IsFinite(Accuracy) || !FMath::IsFinite(Evasion)) return 0;
    return FMath::Clamp(Accuracy * (1.f - Evasion), 0.f, 1.f);
}

void UOVPointDefenseSubsystem::Reset(int32 Seed)
{
    Ships.Reset(); RecentEvents.Reset();
    GameTime = 0; ShotCount = HitCount = KillCount = 0;
    Random.Initialize(Seed);
    bEnabled = true;
}

void UOVPointDefenseSubsystem::Disable()
{
    Reset(); bEnabled = false;
}

bool UOVPointDefenseSubsystem::RegisterShip(AOVShip* Ship, int32 MountCount, float RangeScale)
{
    if (!IsValid(Ship) || !Ship->Systems || !Ship->Systems->Layout || MountCount <= 0
        || !FMath::IsFinite(RangeScale) || RangeScale <= 0) return false;
    if (Ships.ContainsByPredicate([Ship](const FOVPDShipState& S) { return S.Ship == Ship; })) return true;
    FOVPDShipState State;
    State.Ship = Ship; State.RangeScale = RangeScale;
    State.Priority = {EOVPDTargetKind::Missile, EOVPDTargetKind::Aircraft, EOVPDTargetKind::Pod};
    TArray<const FOVWeaponMountDefinition*> LayoutMounts;
    for (const auto& Def : Ship->Systems->Layout->WeaponMounts)
        if (Def.Type == EOVWeaponType::Interception) LayoutMounts.Add(&Def);
    for (int32 I = 0; I < MountCount; ++I)
    {
        FOVPDMountState Mount;
        Mount.MountId = FName(*FString::Printf(TEXT("PD_%02d"), I + 1));
        Mount.Profile = DefaultProfile(I % 2 == 0 ? EOVPDWeaponKind::Beam : EOVPDWeaponKind::Kinetic);
        Mount.HP = Mount.Profile.MountHP;
        const float Angle = 2.f * UE_PI * I / MountCount;
        const FVector Extent = Ship->Systems->Layout->LocalHalfExtent;
        Mount.LocalPosition = FVector(.75f * Extent.X * FMath::Cos(Angle), .85f * Extent.Y * FMath::Sin(Angle), Extent.Z);
        if (LayoutMounts.IsValidIndex(I))
        {
            Mount.bLayoutMount = true;
            Mount.MountId = LayoutMounts[I]->WeaponId;
            if (LayoutMounts[I]->PresentationProfile == TEXT("PD_Beam"))
                Mount.Profile = DefaultProfile(EOVPDWeaponKind::Beam);
            else if (LayoutMounts[I]->PresentationProfile == TEXT("PD_Kinetic"))
                Mount.Profile = DefaultProfile(EOVPDWeaponKind::Kinetic);
            Mount.LocalPosition = LayoutMounts[I]->LocalPosition;
            Mount.HP = LayoutMounts[I]->MaxHP;
        }
        State.Mounts.Add(Mount);
    }
    Ships.Add(State);
    Ship->RefreshWeaponVisuals();
    return true;
}

bool UOVPointDefenseSubsystem::SetPriority(AOVShip* Ship, const TArray<EOVPDTargetKind>& Priority)
{
    if (Priority.Num() != 3 || !Priority.Contains(EOVPDTargetKind::Missile)
        || !Priority.Contains(EOVPDTargetKind::Aircraft) || !Priority.Contains(EOVPDTargetKind::Pod)) return false;
    FOVPDShipState* State = Ships.FindByPredicate([Ship](const FOVPDShipState& S) { return S.Ship == Ship; });
    if (!State) return false;
    State->Priority = Priority;
    return true;
}

FOVPDShipState UOVPointDefenseSubsystem::GetShipState(AOVShip* Ship) const
{
    const FOVPDShipState* State = Ships.FindByPredicate([Ship](const FOVPDShipState& S) { return S.Ship == Ship; });
    return State ? *State : FOVPDShipState();
}

FString UOVPointDefenseSubsystem::GetReadiness(AOVShip* Ship) const
{
    if (!bEnabled) return TEXT("PD OFF");
    if (!IsValid(Ship) || !Ship->Systems || Ship->Systems->bWrecked) return TEXT("SHIP LOST");
    if (Ship->HangarHost.IsValid()) return TEXT("DOCKED");
    if (Ship->Systems->bNeutral || Ship->Systems->FactionId < 0) return TEXT("NO OWNER");
    if (GetWorld()->GetSubsystem<UOVCombatSubsystem>()->IsDeparted(Ship)) return TEXT("DEPARTED");
    const FOVPDShipState* State = Ships.FindByPredicate([Ship](const FOVPDShipState& S) { return S.Ship == Ship; });
    if (!State) return TEXT("NO PD MOUNTS");
    if (!Ship->Systems->CanIntercept()) return TEXT("PD CONTROL OFFLINE");
    if (!State->Mounts.ContainsByPredicate([Ship](const FOVPDMountState& Mount)
        { return Mount.HP > 0 && (!Mount.bLayoutMount || Ship->Systems->IsWeaponAvailable(Mount.MountId)); }))
        return TEXT("PD MOUNTS DISABLED");
    return TEXT("DEFENDING");
}

void UOVPointDefenseSubsystem::Step(float GameDelta)
{
    CSV_SCOPED_TIMING_STAT_EXCLUSIVE(OV_PD);
    TRACE_CPUPROFILER_EVENT_SCOPE(OV_PD);
    if (!bEnabled || !FMath::IsFinite(GameDelta) || GameDelta <= 0) return;
    GameTime += GameDelta;
    TArray<FTarget> Targets;
    for (FOVPDShipState& State : Ships)
    {
        AOVShip* Ship = State.Ship;
        State.Status = GetReadiness(Ship);
        if (State.Status != TEXT("DEFENDING")) continue;
        const auto Duty=GetWorld()->GetSubsystem<UOVInteriorSubsystem>()->GetDutyBonuses(Ship);
        const float Efficiency = Ship->Systems->GetEfficiency(EOVFacilityType::Interception);
        for (FOVPDMountState& Mount : State.Mounts)
        {
            if (Mount.HP <= 0 || (Mount.bLayoutMount && !Ship->Systems->IsWeaponAvailable(Mount.MountId))) continue;
            Mount.Cooldown = FMath::Max(0.f, Mount.Cooldown - GameDelta * Efficiency);
            if (!FMath::IsFinite(Mount.Profile.Damage) || Mount.Profile.Damage <= 0
                || !FMath::IsFinite(Mount.Profile.Interval) || Mount.Profile.Interval <= 0
                || !FMath::IsFinite(Mount.Profile.Range) || Mount.Profile.Range <= 0) continue;
            const FVector Origin = Ship->GetActorTransform().TransformPosition(Mount.LocalPosition);
            const float Range = Mount.Profile.Range * State.RangeScale;
            const FOVWeaponMountDefinition* MountDef = Mount.bLayoutMount ?
                Ship->Systems->Layout->WeaponMounts.FindByPredicate([&Mount](const FOVWeaponMountDefinition& Def) { return Def.WeaponId == Mount.MountId; }) : nullptr;
            GatherTargets(GetWorld(), Targets); // Re-read survivors for each independent mount opportunity.
            const FTarget* Best = nullptr;
            int32 BestRank = MAX_int32;
            double BestDistance = TNumericLimits<double>::Max();
            for (const FTarget& Target : Targets)
            {
                if (Target.Faction < 0 || Target.Faction == Ship->Systems->FactionId) continue;
                const FVector LocalDirection = Ship->GetActorQuat().UnrotateVector(Target.Position - Origin).GetSafeNormal();
                // Explicitly marked mounts cannot shoot through their own hull.
                // Unmarked legacy PD retains its former unrestricted behavior.
                if (MountDef && !MountDef->LocalSurfaceNormal.IsNearlyZero() &&
                    FVector::DotProduct(MountDef->LocalSurfaceNormal.GetSafeNormal(), LocalDirection) < -FMath::Sin(FMath::DegreesToRadians(15.0))) continue;
                const FRotator CandidateAim = LocalDirection.Rotation();
                FVector LocalMuzzle=OVWeaponGeometry::MuzzleLocal(Mount.LocalPosition, CandidateAim,
                    (MountDef ? MountDef->GeometryScale : 1.f) * OVWeaponGeometry::BarrelLength(Ship->Systems->Layout->LocalHalfExtent,Ship->Systems->Layout->WeaponTier,true),false);
                if (OVWeaponGeometry::CIWSBTrialEnabled())
                {
                    FOVWeaponMountDefinition Def;
                    if (MountDef) Def=*MountDef;
                    else { Def.WeaponId=Mount.MountId;Def.Type=EOVWeaponType::Interception;Def.LocalPosition=Mount.LocalPosition;Def.Tier=Ship->Systems->Layout->WeaponTier; }
                    Def.PresentationProfile=Mount.Profile.Kind==EOVPDWeaponKind::Kinetic?TEXT("CIWS6Trial"):TEXT("CIWSBTrial");
                    if (OVWeaponGeometry::HasSourcePDTrial(Def)) LocalMuzzle=OVWeaponGeometry::MuzzleLocal(Def,CandidateAim,Ship->Systems->Layout->LocalHalfExtent,false,0);
                }
                const FVector CandidateMuzzle = Ship->GetActorTransform().TransformPosition(LocalMuzzle);
                const double Distance = FVector::DistSquared(CandidateMuzzle, Target.Position);
                if (Distance > FMath::Square(Range)) continue;
                const int32 Rank = State.Priority.IndexOfByKey(Target.Kind);
                if (Rank == INDEX_NONE) continue;
                if (!Best || Rank < BestRank || (Rank == BestRank && (Distance < BestDistance
                    || (Distance == BestDistance && Target.Id < Best->Id))))
                { Best = &Target; BestRank = Rank; BestDistance = Distance; }
            }
            if (!Best)
            {
                Mount.LocalAim = FMath::RInterpConstantTo(Mount.LocalAim, FRotator::ZeroRotator, GameDelta, 120.f);
                Mount.LocalAim.Roll = 0;
                continue;
            }
            if (Mount.Cooldown > 1.e-5f)
            {
                const FRotator DesiredAim = Ship->GetActorQuat().UnrotateVector(Best->Position - Origin).Rotation();
                Mount.LocalAim = FMath::RInterpConstantTo(Mount.LocalAim, DesiredAim, GameDelta, 240.f);
                Mount.LocalAim.Roll = 0;
                continue;
            }
            const FTarget Chosen = *Best;
            Mount.LocalAim = Ship->GetActorQuat().UnrotateVector(Chosen.Position - Origin).Rotation();
            FOVPDEvent Event;
            Event.EventId = ++ShotCount;
            Event.GameTime = GameTime;
            Event.DefenderId = Ship->GetFName(); Event.DefenderFaction = Ship->Systems->FactionId;
            Event.MountId = Mount.MountId; Event.WeaponKind = Mount.Profile.Kind;
            Event.Origin = Ship->GetWeaponMuzzleWorld(Mount.MountId); Event.TargetPosition = Chosen.Position;
            Event.TargetKind = Chosen.Kind; Event.TargetId = Chosen.Id;
            Event.HitProbability = HitProbability(Mount.Profile.Accuracy+Duty.PDAccuracy, Chosen.Evasion);
            // Exactly one roll. No second evasion check in the target's damage path.
            Event.bHit = Random.FRand() < Event.HitProbability && DamageTarget(GetWorld(), Chosen, Mount.Profile.Damage);
            if (Event.bHit)
            {
                ++HitCount;
                GatherTargets(GetWorld(), Targets);
                Event.bDestroyed = !Targets.ContainsByPredicate([&Chosen](const FTarget& T) { return T.Kind == Chosen.Kind && T.Id == Chosen.Id; });
                if (Event.bDestroyed) ++KillCount;
            }
            ++Mount.ShotCount;
            Mount.LastShotId = Event.EventId; Mount.LastShotTime = GameTime;
            Mount.Cooldown = Mount.Profile.Interval*FMath::Max(1.f/1.3f,Duty.PDInterval);
            Ship->Systems->NotifyCombatParticipation();
            RecentEvents.Add(Event);
            if (RecentEvents.Num() > 128) RecentEvents.RemoveAt(0, RecentEvents.Num() - 128);
        }
    }
}
