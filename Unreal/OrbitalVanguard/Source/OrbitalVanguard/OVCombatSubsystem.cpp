#include "OVCombatSubsystem.h"
#include "OVCombatVFXSubsystem.h"
#include "OVBattleNotices.h"
#include "OVCarrierSubsystem.h"
#include "OVTransportSubsystem.h"
#include "OVInteriorSubsystem.h"
#include "OVLogisticsSubsystem.h"
#include "OVMissionSubsystem.h"
#include "OVPlayScenarioSubsystem.h"
#include "OVPointDefenseSubsystem.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "OVShip.h"
#include "OVShipSystemsComponent.h"
#include "OVController.h"
#include "Engine/World.h"

namespace
{
constexpr float FixedStep = 1.f / 120.f;
constexpr float NeutralElevation = 15.f;
constexpr float LostDetectionTracking = 3.5f;

const FOVWeaponMountDefinition* MountDefinition(const FOVCombatShipState& State, FName Id)
{
    if (!IsValid(State.Ship) || !State.Ship->Systems || !State.Ship->Systems->Layout) return nullptr;
    return State.Ship->Systems->Layout->WeaponMounts.FindByPredicate(
        [Id](const FOVWeaponMountDefinition& Mount) { return Mount.WeaponId == Id; });
}

EOVFacilityType SupplyFor(EOVWeaponType Type)
{
    return Type == EOVWeaponType::Beam ? EOVFacilityType::Capacitor
        : Type == EOVWeaponType::Cannon ? EOVFacilityType::Ammo : EOVFacilityType::MissileStorage;
}

FVector MountOrigin(const FOVCombatShipState& State, const FOVWeaponMountDefinition& Def)
{
    const FOVCombatMountState* Mount = State.Mounts.FindByPredicate(
        [&Def](const FOVCombatMountState& M) { return M.MountId == Def.WeaponId; });
    FOVWeaponMountDefinition EffectiveDef = Def;
    if (!Mount || (Mount->Profile.Kind != EOVAttackKind::Cannon && Mount->Profile.Kind != EOVAttackKind::Beam)) EffectiveDef.PresentationProfile = NAME_None;
    if (Mount && Mount->Profile.Kind==EOVAttackKind::Cannon) EffectiveDef.PresentationProfile=OVWeaponGeometry::CannonSourceTrialProfile(EffectiveDef);
    if (Mount && Mount->Profile.Kind==EOVAttackKind::Beam) EffectiveDef.PresentationProfile=OVWeaponGeometry::BeamSourceTrialProfile(EffectiveDef);
    const FVector Local = OVWeaponGeometry::MuzzleLocal(EffectiveDef,Mount ? Mount->LocalAim : FRotator::ZeroRotator,
        State.Ship->Systems->Layout->LocalHalfExtent,Mount && Mount->Profile.Kind == EOVAttackKind::Missile,
        Mount ? Mount->ShotsFired%2 : 0);
    return State.Ship->GetActorTransform().TransformPosition(Local);
}

float TargetRadius(const AOVShip* Ship)
{
    return Ship && Ship->Systems && Ship->Systems->Layout
        ? (Ship->Systems->Layout->LocalHalfExtent * Ship->GetActorScale3D()).Size() : 100.f;
}

void TurnAxis(double& Angle, double& Velocity, double Desired, float Speed, float Acceleration, float Delta)
{
    const double Error = FMath::FindDeltaAngleDegrees(Angle, Desired);
    const double BrakingSpeed = FMath::Sqrt(2.0 * FMath::Max(1.f, Acceleration) * FMath::Abs(Error));
    const double Wanted = FMath::Sign(Error) * FMath::Min(double(Speed), BrakingSpeed);
    Velocity = FMath::FInterpConstantTo(Velocity, Wanted, double(Delta), double(Acceleration));
    double Travel = Velocity * Delta;
    if (FMath::Sign(Travel) == FMath::Sign(Error) && FMath::Abs(Travel) >= FMath::Abs(Error))
    { Travel = Error; Velocity = 0; }
    Angle = FRotator::NormalizeAxis(Angle + Travel);
}

FVector FixedVLSPoint(const FOVAttackState& Attack, double Alpha)
{
    const double Inverse = 1.0 - Alpha;
    return Inverse * Inverse * Attack.Origin + 2.0 * Inverse * Alpha * Attack.VLSControlPoint
        + Alpha * Alpha * Attack.FrozenAimPoint;
}

void PrepareMissileFlight(FOVAttackState& Shot, const FVector& Normal)
{
    const FVector Offset = Shot.FrozenAimPoint - Shot.Origin;
    Shot.Direction = Offset.GetSafeNormal();
    Shot.VLSControlPoint = Shot.Origin + Normal * FMath::Max<double>(Shot.Weapon.VLSHeight, Offset.Length() * .22);
    if (Shot.Weapon.Kind == EOVAttackKind::GuidedMissile)
    {
        Shot.Direction = (Shot.Direction + Normal * .8).GetSafeNormal();
        return;
    }
    FVector Last = Shot.Origin;
    double Length = 0;
    for (int32 I = 1; I <= 16; ++I)
    {
        const FVector Point = FixedVLSPoint(Shot, I / 16.);
        Length += FVector::Dist(Last, Point);
        Last = Point;
    }
    Shot.VLSDuration = FMath::Max(.01f, float(Length / Shot.Weapon.Speed));
    Shot.Direction = Normal;
}

FVector GuidedDirection(FVector Direction, FVector Desired, float TurnRate, double Delta)
{
    const FQuat Before = Direction.ToOrientationQuat(), Wanted = Desired.ToOrientationQuat();
    const double Angle = Before.AngularDistance(Wanted);
    return FQuat::Slerp(Before, Wanted, Angle > UE_SMALL_NUMBER ?
        FMath::Min(1., FMath::DegreesToRadians(double(TurnRate))*Delta/Angle) : 1.).GetForwardVector();
}

bool SegmentMount(FVector Start, FVector End, const FTransform& Transform, FVector LocalCenter,
    float Radius, float& Fraction, FVector& Normal)
{
    if (Radius <= 0) return false;
    const FVector S = Transform.InverseTransformPosition(Start) - LocalCenter;
    const FVector D = Transform.InverseTransformPosition(End) - Transform.InverseTransformPosition(Start);
    const double A = D.SizeSquared();
    if (A < UE_SMALL_NUMBER) return false;
    const double B = FVector::DotProduct(S, D);
    const double C = S.SizeSquared() - Radius * Radius;
    const double Discriminant = B * B - A * C;
    if (Discriminant < 0) return false;
    const double Entry = C <= 0 ? 0.0 : (-B - FMath::Sqrt(Discriminant)) / A;
    if (Entry < 0 || Entry > 1) return false;
    Fraction = float(Entry);
    const FVector LocalNormal = (S + D * Entry).GetSafeNormal();
    Normal = Transform.TransformVectorNoScale(LocalNormal / Transform.GetScale3D()).GetSafeNormal();
    return true;
}

bool SegmentEllipsoid(FVector Start, FVector End, const FTransform& Transform,
    FVector Radii, float& HitFraction, FVector& HitNormal)
{
    if (Radii.GetMin() <= 0) return false;
    const FVector InvRadii(1.0 / Radii.X, 1.0 / Radii.Y, 1.0 / Radii.Z);
    const FVector S = Transform.InverseTransformPosition(Start) * InvRadii;
    const FVector E = Transform.InverseTransformPosition(End) * InvRadii;
    const FVector D = E - S;
    const double A = D.SizeSquared();
    if (A < 1.e-9) return false;
    const double B = FVector::DotProduct(S, D);
    const double C = S.SizeSquared() - 1.0;
    const double Discriminant = B * B - A * C;
    if (Discriminant < 0) return false;
    const double Entry = C <= 0 ? 0.0 : (-B - FMath::Sqrt(Discriminant)) / A;
    if (Entry < 0.0 || Entry > 1.0) return false;
    HitFraction = float(Entry);
    FVector LocalNormal = (S + D * Entry).GetSafeNormal();
    if (LocalNormal.IsNearlyZero()) LocalNormal = -D.GetSafeNormal();
    HitNormal = Transform.TransformVectorNoScale(LocalNormal / Transform.GetScale3D()).GetSafeNormal();
    return true;
}
}

UOVCombatSettings::UOVCombatSettings()
{
    for (int32 Tier = 1; Tier <= 4; ++Tier)
    {
        FOVRadarProfile Radar;
        Radar.Tier = Tier; Radar.Range = 6000.f + (Tier - 1) * 1500.f;
        Radar.AccuracyBonus = 0.04f + (Tier - 1) * 0.02f;
        Radars.Add(Radar);
        FOVWeaponProfile Beam;
        Beam.ProfileId = FName(*FString::Printf(TEXT("BeamRapid_T%d"), Tier));
        Beam.Tier = Tier; Beam.Damage = 45.f * Tier;
        Beam.Range = 6000.f + (Tier - 1) * 800.f;
        Weapons.Add(Beam);
        FOVWeaponProfile HeavyBeam = Beam;
        HeavyBeam.ProfileId = FName(*FString::Printf(TEXT("BeamHeavy_T%d"), Tier));
        HeavyBeam.Interval = 2.2f; HeavyBeam.Damage = 95.f * Tier;
        HeavyBeam.Range += 1000.f; HeavyBeam.TraverseSpeed = 35; HeavyBeam.TraverseAcceleration = 100;
        Weapons.Add(HeavyBeam);
        FOVWeaponProfile Cannon;
        Cannon.ProfileId = FName(*FString::Printf(TEXT("CannonRapid_T%d"), Tier));
        Cannon.Family = EOVWeaponType::Cannon; Cannon.Kind = EOVAttackKind::Cannon; Cannon.Tier = Tier;
        Cannon.Range = 6500.f + (Tier - 1) * 900.f; Cannon.Interval = 1.8f; Cannon.Damage = 95.f * Tier;
        Cannon.Speed = 2200; Cannon.Accuracy = .76f; Cannon.TraverseSpeed = 70; Cannon.TraverseAcceleration = 220;
        Cannon.AimSpreadDegrees = 5;
        Cannon.Lifetime = 8;
        Weapons.Add(Cannon);
        FOVWeaponProfile HeavyCannon = Cannon;
        HeavyCannon.ProfileId = FName(*FString::Printf(TEXT("CannonHeavy_T%d"), Tier));
        HeavyCannon.Interval = 3.8f; HeavyCannon.Damage = 190.f * Tier; HeavyCannon.Range += 1500;
        HeavyCannon.Speed = 1800; HeavyCannon.TraverseSpeed = 22; HeavyCannon.TraverseAcceleration = 65;
        Weapons.Add(HeavyCannon);
        FOVWeaponProfile Missile;
        Missile.ProfileId = FName(*FString::Printf(TEXT("Missile_T%d"), Tier));
        Missile.Family = EOVWeaponType::Missile; Missile.Kind = EOVAttackKind::Missile; Missile.Tier = Tier;
        Missile.Range = 7500.f + (Tier - 1) * 1000.f; Missile.Interval = 6.5f; Missile.Damage = 145.f * Tier;
        Missile.Speed = 1000; Missile.Accuracy = .64f; Missile.Salvo = 2; Missile.SalvoInterval = .18f;
        Missile.AimSpreadDegrees = 8;
        Missile.TraverseSpeed = 60; Missile.TraverseAcceleration = 180; Missile.Lifetime = 15;
        Weapons.Add(Missile);
        FOVWeaponProfile Guided = Missile;
        Guided.ProfileId = FName(*FString::Printf(TEXT("GuidedMissile_T%d"), Tier));
        Guided.Kind = EOVAttackKind::GuidedMissile; Guided.Interval = 8.f; Guided.Damage = 155.f * Tier;
        Guided.Speed = 900; Guided.Accuracy = .72f; Guided.Salvo = 3;
        Guided.AimSpreadDegrees = 6;
        Weapons.Add(Guided);
    }
}

void UOVCombatSubsystem::EnableCombat(const TArray<AOVShip*>& InShips, int32 Seed)
{
    DisableCombat();
    Settings = DuplicateObject<UOVCombatSettings>(GetDefault<UOVCombatSettings>(), this);
    Random.Initialize(Seed);
    bEnabled = true;
    for (AOVShip* Ship : InShips) RegisterShip(Ship);
}

void UOVCombatSubsystem::DisableCombat()
{
    if(auto* World=GetWorld())if(auto* Notices=World->GetSubsystem<UOVBattleNotices>())Notices->Reset();
    for (const auto& State : Ships) if (IsValid(State.Ship))
    {
        State.Ship->ClearWeaponVisuals();
        State.Ship->Survey.Reset(); State.Ship->LastSurveyAt=-1; State.Ship->ScanReadyAt=0;
    }
    bEnabled = false; GameTime = 0; Accumulator = 0;
    ShotCount = 0; HitCount = 0; Ships.Reset(); Attacks.Reset(); RecentEvents.Reset();
    ResolvedAttackIds.Reset(); EnvironmentAsteroids.Reset(); EnvironmentRadarDisruptRemaining = 0.f;
}

int32 UOVCombatSubsystem::FindShip(AOVShip* Ship) const
{
    return Ships.IndexOfByPredicate([Ship](const FOVCombatShipState& State) { return State.Ship == Ship; });
}

int32 UOVCombatSubsystem::FindMount(const FOVCombatShipState& State, FName Id) const
{
    return State.Mounts.IndexOfByPredicate([Id](const FOVCombatMountState& Mount) { return Mount.MountId == Id; });
}

void UOVCombatSubsystem::RegisterShip(AOVShip* Ship, int32 RadarTier, bool bRadarSupport, int32 WeaponTierCap)
{
    if (!IsValid(Ship) || !Ship->Systems || !Ship->Systems->Layout || !Settings) return;
    // Deck count alone cannot distinguish large from extra-large. T4 is explicit.
    const int32 DefaultCap = Ship->Systems->Layout->bSmallHull ? 1 : Ship->Systems->Layout->DeckCount < 3 ? 2 : 3;
    const int32 Cap = WeaponTierCap > 0 ? FMath::Clamp(WeaponTierCap, 1, 4) : DefaultCap;
    const int32 Existing = FindShip(Ship);
    if (Existing != INDEX_NONE)
    { Ships[Existing].RadarTier = FMath::Clamp(RadarTier, 1, 4); Ships[Existing].bRadarSupport = bRadarSupport; Ships[Existing].WeaponTierCap = Cap; return; }
    FOVCombatShipState State;
    State.Ship = Ship; State.ShipId = Ship->GetFName(); State.OrderFaction=Ship->Systems->FactionId; State.bLastAttackAutomatic=Ship->IsAttackAutomatic();
    State.RadarTier = FMath::Clamp(RadarTier, 1, 4); State.bRadarSupport = bRadarSupport;
    State.WeaponTierCap = Cap;
    for (const FOVWeaponMountDefinition& Def : Ship->Systems->Layout->WeaponMounts)
    {
        if (Def.Type == EOVWeaponType::Interception) continue;
        FOVCombatMountState Mount;
        Mount.MountId = Def.WeaponId;
        const int32 Tier = FMath::Clamp(FMath::RoundToInt(Def.Tier), 1, 4);
        const FOVWeaponProfile* Profile = Settings->Weapons.FindByPredicate(
            [&](const FOVWeaponProfile& P) { return P.Family == Def.Type && P.Tier == Tier; });
        if (Profile) Mount.Profile = *Profile;
        else { Mount.Profile.ProfileId = NAME_None; Mount.Profile.Family = Def.Type; }
        State.Mounts.Add(Mount);
    }
    Ships.Add(State);
    Ship->RefreshWeaponVisuals();
}

FOVCombatShipState UOVCombatSubsystem::GetShipState(AOVShip* Ship) const
{
    const int32 Index = FindShip(Ship);
    return Ships.IsValidIndex(Index) ? Ships[Index] : FOVCombatShipState();
}

bool UOVCombatSubsystem::IsDeparted(AOVShip* Ship) const
{
    const int32 Index = FindShip(Ship);
    return Ships.IsValidIndex(Index) && Ships[Index].bDeparted;
}

bool UOVCombatSubsystem::IsValidTarget(AOVShip* Source, AOVShip* Target) const
{
    return IsValid(Source) && IsValid(Target) && Source != Target && Source->Systems && Target->Systems
        && FindShip(Target) != INDEX_NONE && !Target->Systems->bWrecked && !Target->Systems->bNeutral
        && !IsDeparted(Target) && Target->Systems->FactionId >= 0
        && Source->Systems->FactionId != Target->Systems->FactionId;
}

bool UOVCombatSubsystem::SetTarget(AOVShip* Ship, AOVShip* Target)
{
    if (!bEnabled || FindShip(Ship) == INDEX_NONE || IsDeparted(Ship) || !Ship->Systems->CanCommand()
        || (Target && !IsValidTarget(Ship, Target))) return false;
    const bool bChangedTarget = Ship->AttackTarget != Target;
    Ship->AttackTarget = Target;
    auto& Order=Ships[FindShip(Ship)]; Order.ForcedMount=NAME_None; Order.TargetFaction=Target?Target->Systems->FactionId:-1;
    // A change of target cannot carry a queued volley to a new recipient.
    for (FOVCombatMountState& Mount : Ships[FindShip(Ship)].Mounts)
    {
        Mount.bFireRequested = false; Mount.RequestedTarget.Reset(); Mount.SalvoRemaining = 0; Mount.SalvoTarget.Reset();
        // An explicit order changes identity. AI sensor-loss clearing below retains the separate last-known memory.
        if (bChangedTarget)
        { Mount.TrackingRemaining = 0; Mount.bHadDetection = false; Mount.LastTrackedWorldPoint = FVector::ZeroVector; }
    }
    return true;
}

bool UOVCombatSubsystem::RequestFire(AOVShip* Ship, FName MountId)
{
    const int32 Index = FindShip(Ship);
    if (!bEnabled || !Ships.IsValidIndex(Index) || Ships[Index].bDeparted || !Ship->Systems->CanCommand()
        || !IsValidTarget(Ship, Ship->AttackTarget)) return false;
    bool bAccepted = false;
    for (FOVCombatMountState& Mount : Ships[Index].Mounts)
        if (MountId.IsNone() || Mount.MountId == MountId)
        { Mount.bFireRequested = true; Mount.RequestedTarget = Ship->AttackTarget; bAccepted = true; }
    return bAccepted;
}

bool UOVCombatSubsystem::SetWeaponProfile(AOVShip* Ship, FName MountId, FName ProfileId)
{
    const int32 Index = FindShip(Ship);
    if (!Ships.IsValidIndex(Index) || !Settings) return false;
    const int32 MountIndex = FindMount(Ships[Index], MountId);
    const FOVWeaponMountDefinition* Def = MountDefinition(Ships[Index], MountId);
    const FOVWeaponProfile* Profile = Settings->Weapons.FindByPredicate(
        [ProfileId](const FOVWeaponProfile& P) { return P.ProfileId == ProfileId; });
    if (!Def || !Profile || MountIndex == INDEX_NONE) return false;
    if (!Def->RequiredProfileId.IsNone() && Def->RequiredProfileId != ProfileId) return false;
    // Changing kind is supported. Re-equipping a different tier requires a new authored
    // loadout, keeping the existing storage-explosion tier data authoritative.
    if (Profile->Tier > Ships[Index].WeaponTierCap || Profile->Tier != FMath::RoundToInt(Def->Tier)) return false;
    const bool bGeneral = Def->AuthoredSlotPurpose == TEXT("General") && Def->RequiredProfileId.IsNone();
    const bool bFamilyChange = Profile->Family != Def->Type;
    if (bFamilyChange && (!bGeneral || Profile->MountMaxHP <= 0 || Profile->Family == EOVWeaponType::Interception)) return false;
    if (bFamilyChange && !Ship->Systems->Layout->Slots.ContainsByPredicate(
        [Profile](const FOVSlotDefinition& Slot) { return Slot.Facility == SupplyFor(Profile->Family); })) return false;
    // The layout may be shared by many ships: re-equipment must change only this ship.
    // A calibrated module supplies its own max HP; current HP is never restored for free.
    if (bGeneral && Profile->MountMaxHP > 0 && (bFamilyChange || !FMath::IsNearlyEqual(Def->MaxHP, Profile->MountMaxHP)))
    {
        UOVShipSystemsComponent* Systems = Ship->Systems;
        FOVWeaponMountState* Health = Systems ? Systems->WeaponMountStates.FindByPredicate(
            [MountId](const FOVWeaponMountState& S) { return S.WeaponMountId == MountId; }) : nullptr;
        if (!Systems || !Systems->Layout || !Health) return false;
        UShipLayoutDataAsset* Copy = DuplicateObject<UShipLayoutDataAsset>(Systems->Layout, Ship);
        if (!Copy) return false;
        FOVWeaponMountDefinition* Updated = Copy->WeaponMounts.FindByPredicate(
            [MountId](const FOVWeaponMountDefinition& M) { return M.WeaponId == MountId; });
        if (!Updated) return false;
        Updated->Type = Profile->Family;
        Updated->MaxHP = Profile->MountMaxHP;
        if (bFamilyChange) Updated->PresentationProfile = NAME_None;
        Copy->BeamSlots = Copy->CannonSlots = Copy->MissileSlots = 0;
        for (const FOVWeaponMountDefinition& M : Copy->WeaponMounts)
        {
            if (M.Type == EOVWeaponType::Beam) ++Copy->BeamSlots;
            else if (M.Type == EOVWeaponType::Cannon) ++Copy->CannonSlots;
            else if (M.Type == EOVWeaponType::Missile) ++Copy->MissileSlots;
        }
        Health->HP = FMath::Min(Health->HP, Updated->MaxHP);
        Systems->Layout = Copy;
    }
    FOVCombatMountState& Mount = Ships[Index].Mounts[MountIndex];
    Mount.Profile = *Profile; Mount.SalvoRemaining = 0; Mount.bFireRequested = false;
    if (bGeneral) Ship->RefreshWeaponVisuals();
    return true;
}

void UOVCombatSubsystem::NotifyFTLDeparture(AOVShip* Ship)
{
    const int32 Index = FindShip(Ship);
    if (!Ships.IsValidIndex(Index)) return;
    Ships[Index].bDeparted = true;
    ClearPending(Ships[FindShip(Ship)]);
    Ship->StopMovement(); Ship->AttackTarget = nullptr;
    Ship->SetActorHiddenInGame(true);
    Ship->SetActorEnableCollision(false);
}

FOVDetectionInfo UOVCombatSubsystem::GetDetection(AOVShip* Ship, AOVShip* Target) const
{
    FOVDetectionInfo Best;
    if (!Settings || !IsValidTarget(Ship, Target) || IsDeparted(Ship) || Ship->Systems->bWrecked || Ship->Systems->bNeutral) return Best;
    if (Settings->bDevelopmentOmniscient)
    { Best.bDetected = true; Best.Source = TEXT("DEV OMNISCIENT"); return Best; }
    if (FVector::Dist(Ship->GetActorLocation(), Target->GetActorLocation()) <= Settings->GuaranteedRange)
    { Best.bDetected = true; Best.Source = TEXT("GUARANTEED NEAR"); Best.SourceId = Ship->GetFName(); }
    if (EnvironmentRadarDisruptRemaining > 0.f) return Best;
    for (const FOVCombatShipState& Observer : Ships)
    {
        AOVShip* Sensor = Observer.Ship;
        if (!IsValid(Sensor) || !Sensor->Systems || Observer.bDeparted || !Sensor->Systems->HasRadar()
            || Sensor->Systems->FactionId != Ship->Systems->FactionId) continue;
        const FOVRadarProfile* Radar = Settings->Radars.FindByPredicate(
            [&](const FOVRadarProfile& R) { return R.Tier == Observer.RadarTier; });
        if (!Radar || FVector::Dist(Sensor->GetActorLocation(), Target->GetActorLocation()) > Radar->Range) continue;
        const float Coefficient = Sensor == Ship ? 1.f : Observer.bRadarSupport
            ? Settings->SupportShareCoefficient : Settings->OrdinaryShareCoefficient;
        const float Bonus = Radar->AccuracyBonus * Sensor->Systems->GetEfficiency(EOVFacilityType::Radar) * Coefficient;
        if (!Best.bDetected || Bonus > Best.AccuracyBonus)
        {
            Best.bDetected = true; Best.AccuracyBonus = Bonus; Best.SourceId = Observer.ShipId;
            Best.Source = Sensor == Ship ? TEXT("OWN RADAR") : Observer.bRadarSupport ? TEXT("SUPPORT RADAR") : TEXT("SHARED RADAR");
        }
    }
    return Best;
}

FOVFireReadiness UOVCombatSubsystem::GetReadiness(AOVShip* Ship, FName MountId) const
{
    const int32 Index = FindShip(Ship);
    if (!bEnabled || !Ships.IsValidIndex(Index)) return FOVFireReadiness();
    const int32 Mount = FindMount(Ships[Index], MountId);
    if (Mount == INDEX_NONE) { FOVFireReadiness R; R.Reason = TEXT("NO MOUNT"); return R; }
    return Evaluate(Ships[Index], Ships[Index].Mounts[Mount], false);
}

FOVFireReadiness UOVCombatSubsystem::Evaluate(const FOVCombatShipState& State, const FOVCombatMountState& Mount, bool bIgnoreCooldown) const
{
    FOVFireReadiness R;
    auto Block = [&](const TCHAR* Reason) { R.Reason = Reason; return R; };
    AOVShip* Ship = State.Ship;
    if (!bEnabled || !IsValid(Ship) || !Ship->Systems || !Settings) return Block(TEXT("COMBAT OFF"));
    const UOVShipSystemsComponent* Systems = Ship->Systems;
    R.Cooldown = Mount.SalvoRemaining > 0 ? Mount.SalvoDelay : Mount.Cooldown;
    if (State.bDeparted) return Block(TEXT("DEPARTED"));
    if (Systems->bWrecked) return Block(TEXT("WRECKED"));
    if (Systems->bNeutral) return Block(TEXT("NEUTRAL"));
    const FOVWeaponMountDefinition* Def = MountDefinition(State, Mount.MountId);
    if (!Def || Mount.Profile.ProfileId.IsNone()) return Block(TEXT("PROFILE UNAVAILABLE"));
    if (Mount.Profile.Tier > State.WeaponTierCap) return Block(TEXT("TIER LIMIT"));
    if (Mount.Profile.Tier != FMath::RoundToInt(Def->Tier)) return Block(TEXT("LOADOUT TIER MISMATCH"));
    if (!FMath::IsFinite(Mount.Profile.Range) || Mount.Profile.Range <= 0
        || !FMath::IsFinite(Mount.Profile.Damage) || Mount.Profile.Damage <= 0
        || !FMath::IsFinite(Mount.Profile.Interval) || Mount.Profile.Interval <= 0
        || !FMath::IsFinite(Mount.Profile.Speed) || Mount.Profile.Speed <= 0)
        return Block(TEXT("INVALID PROFILE"));
    const FOVWeaponMountState* Health = Systems->WeaponMountStates.FindByPredicate(
        [&](const FOVWeaponMountState& H) { return H.WeaponMountId == Mount.MountId; });
    if (!Health || Health->HP <= 0) return Block(TEXT("MOUNT DESTROYED"));
    if (Systems->GetEfficiency(EOVFacilityType::WeaponControl) <= 0) return Block(TEXT("WEAPON CONTROL OFF"));
    if (Systems->GetEfficiency(SupplyFor(Def->Type)) <= 0) return Block(TEXT("SUPPLY OFF"));
    if (Def->Type == EOVWeaponType::Beam && Ship->bFTLCharging) return Block(TEXT("FTL CHARGING"));
    AOVShip* Target = Ship->AttackTarget;
    if (!IsValidTarget(Ship, Target)) return Block(TEXT("NO VALID TARGET"));
    const FVector Origin = MountOrigin(State, *Def);
    if (IsTargetProtected(Ship, Target) && !(State.Intent==EOVAttackIntent::DisableWeapons && State.ForcedMount==Mount.MountId)) return Block(TEXT("INTERIOR ALLY HOLD"));
    FVector AimPoint;
    if (!ResolveAimPoint(State,Target,AimPoint))
    {
        if(!GetDetection(Ship,Target).bDetected)return Block(TEXT("NO DETECTION"));
        const bool bAlive=Target->Systems->WeaponMountStates.ContainsByPredicate([](const FOVWeaponMountState& M){return M.HP>0;});
        return Block(bAlive?TEXT("NO ACCESSIBLE WEAPONS - WAIT"):TEXT("NO OBSERVED WEAPONS - WAIT"));
    }
    const FVector Offset = AimPoint - Origin;
    R.Distance = Offset.Size();
    if (R.Distance > Mount.Profile.Range) return Block(TEXT("OUT OF RANGE"));
    R.Detection = GetDetection(Ship, Target);
    if (!R.Detection.bDetected) return Block(TEXT("NO DETECTION"));
    const float ControlBonus = Settings->ControlAccuracyBonus * (Systems->GetEfficiency(EOVFacilityType::WeaponControl) - 1.f);
    const auto Duty = GetWorld()->GetSubsystem<UOVInteriorSubsystem>()->GetDutyBonuses(State.Ship);
    R.Accuracy = FMath::Clamp(Mount.Profile.Accuracy + R.Detection.AccuracyBonus + ControlBonus + Duty.AccuracyBonus, 0.f,
        Mount.Profile.Kind == EOVAttackKind::Beam ? (Settings->bHeroD114Aim?0.98f:0.995f) : 1.f);
    R.AimSpreadDegrees = Mount.Profile.Kind == EOVAttackKind::Beam ? 0.f
        : FMath::Clamp(Mount.Profile.AimSpreadDegrees * (1.f - R.Accuracy), 0.f, 45.f);
    if(Settings->bHeroD114Aim && Mount.Profile.Kind!=EOVAttackKind::Beam)
        R.AimSpreadDegrees=FMath::Clamp(Mount.Profile.AimSpreadDegrees /
            (1.f+4.f*FMath::Max(0.f,R.Detection.AccuracyBonus+Duty.AccuracyBonus)),0.f,45.f);
    const FVector LocalDirection = Ship->GetActorQuat().UnrotateVector(Offset).GetSafeNormal();
    const double FaceDot=FVector::DotProduct(OVWeaponGeometry::SurfaceNormal(*Def),LocalDirection);
    if (FaceDot < -FMath::Sin(FMath::DegreesToRadians(double(NeutralElevation)))) return Block(TEXT("MOUNT BLIND ARC"));
    R.AimError = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(
        FVector::DotProduct(Mount.LocalAim.Vector(), LocalDirection), -1.0, 1.0)));
    if (R.AimError > Mount.Profile.FireTolerance) return Block(TEXT("TURRET ALIGNING"));
    if (Mount.Profile.Family == EOVWeaponType::Missile)
    {
        FOVAttackState Planned;
        Planned.Origin = Planned.Position = Origin;
        Planned.FrozenAimPoint = AimPoint;
        Planned.Weapon = Mount.Profile;
        Planned.TrackingLifetime = Settings->GuidedTrackingSeconds;
        PrepareMissileFlight(Planned, Ship->GetActorQuat().RotateVector(OVWeaponGeometry::SurfaceNormal(*Def)));
        FMissilePath Path;
        PredictMissilePath(Planned, Path);
        if (FindBlockingFriend(Ship, Target, Origin, &Path)) return Block(TEXT("FRIENDLY IN FLIGHT PATH"));
    }
    else if (FindBlockingFriend(Ship, Target, Origin)) return Block(TEXT("FRIENDLY IN LINE"));
    if (!bIgnoreCooldown && R.Cooldown > UE_KINDA_SMALL_NUMBER) return Block(TEXT("COOLDOWN"));
    R.bReady = true; R.Reason = TEXT("READY"); return R;
}

void UOVCombatSubsystem::PredictMissilePath(const FOVAttackState& Shot, FMissilePath& Points)
{
    Points.Reset();
    Points.Add(Shot.Position);
    constexpr int32 Samples = 20;
    if (Shot.Weapon.Kind == EOVAttackKind::Missile && Shot.VLSDuration > 0)
    {
        const double Start = Shot.Age / Shot.VLSDuration;
        const double End = FMath::Min(1., double(Shot.Weapon.Lifetime / Shot.VLSDuration));
        if (Start < End)
            for (int32 I=1; I<=Samples; ++I)
                Points.Add(FixedVLSPoint(Shot, FMath::Lerp(Start, End, double(I)/Samples)));
    }
    else if (Shot.Weapon.Kind == EOVAttackKind::GuidedMissile)
    {
        const AOVShip* Target = Shot.Target.Get();
        const FVector Aim = Target ? Target->GetActorTransform().TransformPosition(Shot.TargetLocalPoint)+Shot.AimOffset : Shot.FrozenAimPoint;
        FVector Position = Shot.Position, Direction = Shot.Direction;
        const double Horizon = FMath::Min<double>(FMath::Min(Shot.TrackingLifetime,Shot.Weapon.Lifetime)-Shot.Age,
            FVector::Dist(Position,Aim)/Shot.Weapon.Speed + 1.);
        if (Horizon <= 0) return;
        const double Dt = Horizon/(Samples*4);
        for (int32 I=1; I<=Samples*4; ++I)
        {
            const FVector Offset = Aim - Position;
            if (Offset.SizeSquared() <= FMath::Square(Dt*Shot.Weapon.Speed)) { Points.Add(Aim); break; }
            Direction = GuidedDirection(Direction,Offset.GetSafeNormal(),Shot.Weapon.GuidanceTurnRate,Dt);
            Position += Direction*Shot.Weapon.Speed*Dt;
            if (I%4 == 0) Points.Add(Position);
        }
    }
}

AOVShip* UOVCombatSubsystem::FindBlockingFriend(AOVShip* Ship, AOVShip* Target, FVector Origin, const FMissilePath* Path) const
{
    FVector Endpoint; const int32 SI=FindShip(Ship);
    if(SI==INDEX_NONE || !ResolveAimPoint(Ships[SI],Target,Endpoint)) return nullptr;
    for (const FOVCombatShipState& Other : Ships)
    {
        AOVShip* Friend = Other.Ship;
        if (!IsValid(Friend) || Friend == Ship || Friend == Target || !Friend->Systems || !Friend->Systems->Layout
            || Friend->Systems->bWrecked || Other.bDeparted || Friend->Systems->FactionId != Ship->Systems->FactionId) continue;
        float Fraction; FVector Normal;
        if (Path)
        {
            const FTransform Transform = Friend->GetActorTransform();
            const bool Shield = Friend->Systems->bShieldActive && Friend->Systems->Shield > 0;
            const FVector Extent = Shield ? Friend->GetLocalShieldRadii() : Friend->Systems->Layout->LocalHalfExtent;
            for (int32 I=1; I<Path->Num(); ++I)
            {
                const FVector Start=(*Path)[I-1], End=(*Path)[I];
                if (Shield)
                {
                    if (SegmentEllipsoid(Start,End,Transform,Extent,Fraction,Normal)) return Friend;
                    continue;
                }
                const auto* Layout=Friend->Systems->Layout.Get();
                if (!SegmentBox(Start,End,Transform,Extent,Fraction,Normal)) continue;
                if (Layout->CombatHullBoxes.IsEmpty()) return Friend;
                for (const auto& Box:Layout->CombatHullBoxes)
                {
                    FTransform BoxTransform=Transform;
                    BoxTransform.SetLocation(Transform.TransformPosition(Box.LocalCenter));
                    if (SegmentBox(Start,End,BoxTransform,Box.HalfExtent,Fraction,Normal)) return Friend;
                }
            }
            continue;
        }
        if (SegmentBox(Origin, Endpoint, Friend->GetActorTransform(),
            Friend->Systems->Layout->LocalHalfExtent, Fraction, Normal)) return Friend;
        for (const FOVWeaponMountDefinition& Mount : Friend->Systems->Layout->WeaponMounts)
        {
            const FOVWeaponMountState* Health = Friend->Systems->WeaponMountStates.FindByPredicate(
                [&](const FOVWeaponMountState& H) { return H.WeaponMountId == Mount.WeaponId; });
            if (Health && Health->HP > 0 && SegmentMount(Origin, Endpoint, Friend->GetActorTransform(),
                Mount.LocalPosition, Mount.GeometryScale * Settings->MountCollisionRadius, Fraction, Normal)) return Friend;
        }
    }
    return nullptr;
}

void UOVCombatSubsystem::Simulate(float GameDelta)
{
    if (!bEnabled || !Settings || !FMath::IsFinite(GameDelta) || GameDelta <= 0) return;
    Accumulator += GameDelta;
    while (Accumulator + 1.e-7 >= FixedStep)
    {
        Step(FixedStep); Accumulator -= FixedStep;
        if (Accumulator < 0) Accumulator = 0;
    }
    // Display synchronizes once per outer frame, not for every 120 Hz combat substep.
    for (const auto& State : Ships)
    {
        if (IsValid(State.Ship))
        {
            State.Ship->RefreshWeaponVisuals();
            State.Ship->RefreshShipVisualVFX(GameDelta);
        }
    }
    if (UOVCombatVFXSubsystem* VFX = GetWorld()->GetSubsystem<UOVCombatVFXSubsystem>())
    {
        VFX->SynchronizeVFX(GameDelta);
    }
}

void UOVCombatSubsystem::Step(float Delta)
{
    GameTime += Delta;
    EnvironmentRadarDisruptRemaining = FMath::Max(0.f, EnvironmentRadarDisruptRemaining - Delta);
    for (FOVCombatShipState& State : Ships)
    {
        AOVShip* Ship = State.Ship;
        if (!IsValid(Ship) || !Ship->Systems || State.bDeparted) continue;
        if(State.OrderFaction!=Ship->Systems->FactionId) {
            Ship->bFTLCharging=false;
            Ship->StopMovement(); Ship->Velocity=FVector::ZeroVector;
            Ship->bManualAttitude=false; Ship->SetAutomation(false,false);
            if(auto* PC=Cast<AOVController>(GetWorld()->GetFirstPlayerController()))PC->TrialFTLRemaining.Remove(Ship);
        }
        if(State.OrderFaction!=Ship->Systems->FactionId || (Ship->AttackTarget && State.TargetFaction>=0 && State.TargetFaction!=Ship->AttackTarget->Systems->FactionId)) {
            ClearPending(State); State.Intent=EOVAttackIntent::DestroyHull; Ship->AttackTarget=nullptr; State.TargetFaction=-1;
        }
        if(State.bLastAttackAutomatic!=Ship->IsAttackAutomatic()) { ClearPending(State); State.bLastAttackAutomatic=Ship->IsAttackAutomatic(); }
        State.OrderFaction=Ship->Systems->FactionId;
        if(Ship->Systems->bWrecked || (Ship->AttackTarget && !IsValidTarget(Ship,Ship->AttackTarget))) { ClearPending(State); Ship->AttackTarget=nullptr; }
        Ship->Systems->Simulate(Delta);
        Ship->bFriendly = Ship->Systems->FactionId == 0;
        if (Ship->bFTLCharging && !Ship->Systems->HasPhysicalFacility(EOVFacilityType::Engine)) Ship->bFTLCharging = false;
        // Bridge loss preserves the existing 2A stop rule and hands only combat to the fallback AI.
        if (!Ship->Systems->CanCommand()) { Ship->SetAutomation(true, true); Ship->StopMovement(); }
        Ship->Simulate(Delta);
    }
    if (AOVController* Controller = Cast<AOVController>(GetWorld()->GetFirstPlayerController()))
        Controller->ResolveContacts(Delta);
    // One game clock: aircraft/transport positions, PD opportunities, then surviving ordnance.
    GetWorld()->GetSubsystem<UOVTransportSubsystem>()->Simulate(Delta);
    GetWorld()->GetSubsystem<UOVInteriorSubsystem>()->Simulate(Delta);
    GetWorld()->GetSubsystem<UOVLogisticsSubsystem>()->Simulate(Delta);
    GetWorld()->GetSubsystem<UOVMissionSubsystem>()->Simulate(Delta);
    GetWorld()->GetSubsystem<UOVCarrierSubsystem>()->Simulate(Delta);
    GetWorld()->GetSubsystem<UOVPointDefenseSubsystem>()->Step(Delta);
    if(auto* PC=Cast<AOVController>(GetWorld()->GetFirstPlayerController())) PC->AdvancePhase2C(Delta);
    AdvanceAttacks(Delta);
    for (FOVCombatShipState& State : Ships)
    {
        AOVShip* Ship = State.Ship;
        if (!IsValid(Ship) || !Ship->Systems || State.bDeparted || Ship->Systems->bWrecked || Ship->Systems->bNeutral) continue;
        const bool bAutomatic = Ship->IsAttackAutomatic() || !Ship->Systems->CanCommand();
        const auto* Mission=GetWorld()->GetSubsystem<UOVMissionSubsystem>();
        const bool bManualRequest=State.Mounts.ContainsByPredicate([](const FOVCombatMountState& M){return M.bFireRequested;});
        const bool bAutoHeld=Mission&&Mission->ShouldHoldAutoAssaultFire(Ship,Ship->AttackTarget);
        if (bAutomatic && (!IsValidTarget(Ship, Ship->AttackTarget) || !GetDetection(Ship, Ship->AttackTarget).bDetected || (IsTargetProtected(Ship,Ship->AttackTarget) && State.ForcedMount.IsNone()) || (bAutoHeld&&!bManualRequest&&State.ForcedMount.IsNone())))
        {
            AOVShip* Best = nullptr; double BestDistance = TNumericLimits<double>::Max();
            for (const FOVCombatShipState& Candidate : Ships)
            {
                AOVShip* Target = Candidate.Ship;
                if (!IsValidTarget(Ship, Target) || !GetDetection(Ship, Target).bDetected) continue;
                FVector CandidateAim;
                if(IsTargetProtected(Ship,Target) || (Mission&&Mission->ShouldHoldAutoAssaultFire(Ship,Target)) || !ResolveAimPoint(State,Target,CandidateAim)) continue;
                double Distance = FVector::DistSquared(Ship->GetActorLocation(), Target->GetActorLocation());
                const auto* Play=GetWorld()->GetSubsystem<UOVPlayScenarioSubsystem>();
                if (Play && Play->GetActiveDefinition())
                {
                    const auto Skill=Ship->Systems->FactionId==0?EOVCommandDifficulty::Hard:Play->ActiveDifficulty;
                    if (Skill!=EOVCommandDifficulty::Easy)
                    {
                        const float Integrity=FMath::Clamp(Target->Systems->HullHP/FMath::Max(1.f,Target->Systems->MaxHullHP),0.f,1.f);
                        Distance *= .55+.45*Integrity;
                        if (Skill==EOVCommandDifficulty::Hard && !Target->Systems->bShieldActive) Distance*=.72;
                    }
                }
                if (Distance < BestDistance) { Best = Target; BestDistance = Distance; }
            }
            if (Ship->AttackTarget != Best)
            {
                for (FOVCombatMountState& Mount : State.Mounts)
                { Mount.SalvoRemaining = 0; Mount.bFireRequested = false; Mount.RequestedTarget.Reset(); Mount.SalvoTarget.Reset(); }
                State.ForcedMount=NAME_None; State.TargetFaction=Best?Best->Systems->FactionId:-1;
                Ship->AttackTarget = Best;
            }
        }
        // Existing destination, Hold, manual posture and FTL always precede optional AI clearance.
        if (Ship->IsMovementAutomatic() && IsValidTarget(Ship, Ship->AttackTarget)
            && GetDetection(Ship, Ship->AttackTarget).bDetected && !Ship->bHold && !Ship->bHasDestination
            && !Ship->bManualAttitude && !Ship->bFTLCharging && Ship->Systems->CanMove())
        {
            if (Ship->Systems->Layout->DeckCount <= 2
                && FindBlockingFriend(Ship, Ship->AttackTarget, Ship->GetActorLocation())
                && !State.Mounts.ContainsByPredicate([&](const FOVCombatMountState& M){
                    return M.Profile.Family==EOVWeaponType::Missile && Evaluate(State,M,true).bReady;
                }))
                Ship->SetDestination(Ship->GetActorLocation() + FVector(0, 0, Settings->AutoClearanceHeight));
            else
            {
                FRotator Desired = (Ship->AttackTarget->GetActorLocation() - Ship->GetActorLocation()).Rotation();
                Desired.Roll = 0;
                const FQuat Before = Ship->GetActorQuat();
                const FQuat Wanted = Desired.Quaternion();
                const double Angle = Before.AngularDistance(Wanted);
                const double MaxTurn = FMath::DegreesToRadians(Ship->TurnRate * Ship->Systems->GetEfficiency(EOVFacilityType::Engine)) * Delta;
                Ship->SetActorRotation(FQuat::Slerp(Before, Wanted, Angle > UE_SMALL_NUMBER ? FMath::Min(1.0, MaxTurn / Angle) : 1.0));
            }
        }
        for (FOVCombatMountState& Mount : State.Mounts)
        {
            Mount.Cooldown = FMath::Max(0.f, Mount.Cooldown - Delta);
            Mount.SalvoDelay = FMath::Max(0.f, Mount.SalvoDelay - Delta);
            UpdateAim(State, Mount, Delta);
            if (Mount.bFireRequested && Mount.RequestedTarget.Get() != Ship->AttackTarget)
            { Mount.bFireRequested = false; Mount.RequestedTarget.Reset(); }
            if (Mount.SalvoRemaining > 0 && (Mount.SalvoTarget.Get() != Ship->AttackTarget
                || (Mount.bAutomaticSalvo && !bAutomatic)))
            { Mount.SalvoRemaining = 0; Mount.SalvoTarget.Reset(); }
            if (bAutomatic && !Mount.bFireRequested && Mount.SalvoRemaining<=0 && State.ForcedMount!=Mount.MountId && Mission
                && Mission->ShouldHoldAutoAssaultFire(Ship,Ship->AttackTarget)) continue;
            if (!(bAutomatic || Mount.bFireRequested || Mount.SalvoRemaining > 0)) continue;
            if (Mount.SalvoRemaining<=0 && Mount.Cooldown>UE_KINDA_SMALL_NUMBER &&
                !IsTargetProtected(Ship,Ship->AttackTarget)) continue;
            const FOVFireReadiness Ready = Evaluate(State, Mount, false);
            if (!Ready.bReady)
            {
                if(IsTargetProtected(Ship,Ship->AttackTarget) && State.ForcedMount!=Mount.MountId) { Mount.bFireRequested=false; Mount.RequestedTarget.Reset(); }
                // Cooldown is a timer, not a failed salvo gate. Each remaining round rechecks all other gates.
                if (Mount.SalvoRemaining > 0 && !Evaluate(State, Mount, true).bReady)
                { Mount.SalvoRemaining = 0; Mount.SalvoTarget.Reset(); }
                continue;
            }
            Launch(State, Mount, bAutomatic);
            Mount.bFireRequested = false;
            Mount.RequestedTarget.Reset();
        }
    }
    GetWorld()->GetSubsystem<UOVPlayScenarioSubsystem>()->Step(Delta);
}

void UOVCombatSubsystem::UpdateAim(FOVCombatShipState& State, FOVCombatMountState& Mount, float Delta)
{
    const FOVWeaponMountDefinition* Def = MountDefinition(State, Mount.MountId);
    if (!Def) return;
    AOVShip* Ship = State.Ship;
    const auto* Health = Ship->Systems->WeaponMountStates.FindByPredicate(
        [&Mount](const FOVWeaponMountState& H) { return H.WeaponMountId == Mount.MountId; });
    if (!Health || Health->HP <= 0)
    {
        Mount.AngularVelocity = FVector2D::ZeroVector;
        Mount.bHadDetection = false; Mount.TrackingRemaining = 0;
        return;
    }
    FVector ObservedPoint;
    const bool bDetected = ResolveAimPoint(State,Ship->AttackTarget,ObservedPoint);
    if (bDetected)
    {
        Mount.LastTrackedWorldPoint = ObservedPoint;
        Mount.TrackingRemaining = 0;
    }
    else if (Mount.bHadDetection) Mount.TrackingRemaining = LostDetectionTracking;
    else Mount.TrackingRemaining = FMath::Max(0.f, Mount.TrackingRemaining - Delta);
    Mount.bHadDetection = bDetected;
    // No hidden current position is read after loss. Return to authored standby through the same inertial turn.
    const FVector Local = Ship->GetActorQuat().UnrotateVector(Mount.LastTrackedWorldPoint - MountOrigin(State, *Def));
    const FVector LocalDir = Local.GetSafeNormal();
    const double FaceDot = FVector::DotProduct(OVWeaponGeometry::SurfaceNormal(*Def), LocalDir);
    const bool bInArc = FaceDot >= -FMath::Sin(FMath::DegreesToRadians(double(NeutralElevation)));
    const FRotator Desired = (bDetected || Mount.TrackingRemaining > 0) && bInArc ? Local.Rotation() : Mount.Profile.StandbyAim;
    TurnAxis(Mount.LocalAim.Yaw, Mount.AngularVelocity.X, Desired.Yaw, Mount.Profile.TraverseSpeed, Mount.Profile.TraverseAcceleration, Delta);
    TurnAxis(Mount.LocalAim.Pitch, Mount.AngularVelocity.Y, Desired.Pitch, Mount.Profile.TraverseSpeed, Mount.Profile.TraverseAcceleration, Delta);
    Mount.LocalAim.Roll = 0;
}

void UOVCombatSubsystem::Launch(FOVCombatShipState& State, FOVCombatMountState& Mount, bool bAutomatic)
{
    const FOVFireReadiness Ready = Evaluate(State, Mount, false);
    const FOVWeaponMountDefinition* Def = MountDefinition(State, Mount.MountId);
    if (!Ready.bReady || !Def) return;
    FOVAttackState Attack;
    Attack.AttackId = ++ShotCount;
    Attack.AttackerId = State.ShipId; Attack.Attacker = State.Ship;
    Attack.FactionId = State.Ship->Systems->FactionId;
    Attack.LaunchTime = GameTime; Attack.TrackingLifetime = Settings->GuidedTrackingSeconds;
    Attack.WeaponId = Mount.MountId; Attack.Weapon = Mount.Profile;
    Attack.bAntiShipMissile = Def->IsAntiShipMissile();
    Attack.InterceptHP = (Attack.Weapon.Kind == EOVAttackKind::Missile || Attack.Weapon.Kind == EOVAttackKind::GuidedMissile)
        ? FMath::Max(0.f, Attack.Weapon.ProjectileHP) : 0;
    Attack.Target = State.Ship->AttackTarget;
    Attack.Origin = Attack.Position = MountOrigin(State, *Def);
    Attack.Accuracy = Ready.Accuracy;
    Attack.Damage = Mount.Profile.Damage * State.Ship->Systems->GetEfficiency(SupplyFor(Def->Type));
    FVector AimPoint;
    if (!ResolveAimPoint(State,Attack.Target.Get(),AimPoint)) return;
    Attack.TargetLocalPoint=Attack.Target->GetActorTransform().InverseTransformPosition(AimPoint);
    const FVector Sight = (AimPoint - Attack.Position).GetSafeNormal();
    FVector Side, Up; Sight.FindBestAxisVectors(Side, Up);
    if (Attack.Weapon.Kind == EOVAttackKind::Beam && Random.FRand() > Ready.Accuracy)
    {
        const float Angle = Random.FRandRange(0.f, 2.f * PI);
        Attack.AimOffset = (Side * FMath::Cos(Angle) + Up * FMath::Sin(Angle))
            * TargetRadius(Attack.Target.Get()) * FMath::Max(1.1f, Settings->MissOffsetRadiusFactor);
        AimPoint += Attack.AimOffset;
    }
    else if (Attack.Weapon.Kind != EOVAttackKind::Beam)
    {
        // A continuous uniform aim disk, never a binary hit/evasion draw. Collision alone decides impact.
        const float Angle = Random.FRandRange(0.f, 2.f * PI);
        const float Radius = FMath::Sqrt(Random.FRand()) * FMath::Tan(FMath::DegreesToRadians(Ready.AimSpreadDegrees))
            * FVector::Dist(AimPoint, Attack.Position);
        Attack.AimOffset = (Side * FMath::Cos(Angle) + Up * FMath::Sin(Angle)) * Radius;
        AimPoint += Attack.AimOffset;
    }
    Attack.FrozenAimPoint = AimPoint;
    Attack.Direction = (AimPoint - Attack.Position).GetSafeNormal();
    if (Attack.Weapon.Family == EOVWeaponType::Missile)
        PrepareMissileFlight(Attack, State.Ship->GetActorQuat().RotateVector(OVWeaponGeometry::SurfaceNormal(*Def)));
    State.Ship->Systems->NotifyCombatParticipation();
    ++Mount.ShotsFired;
    Mount.LastShotId = Attack.AttackId;
    Mount.LastShotTime = GameTime;
    const float Efficiency = State.Ship->Systems->GetEfficiency(EOVFacilityType::WeaponControl)
        * State.Ship->Systems->GetEfficiency(SupplyFor(Def->Type));
    if (Mount.SalvoRemaining <= 0)
    {
        const auto Duty = GetWorld()->GetSubsystem<UOVInteriorSubsystem>()->GetDutyBonuses(State.Ship);
        Mount.Cooldown = FMath::Max(0.01f, Mount.Profile.Interval / FMath::Max(0.05f, Efficiency * (1.f+Duty.FireRateBonus)));
        Mount.SalvoRemaining = FMath::Max(1, Mount.Profile.Salvo);
        Mount.SalvoTarget = Attack.Target; Mount.bAutomaticSalvo = bAutomatic;
    }
    --Mount.SalvoRemaining;
    Mount.SalvoDelay = FMath::Max(0.001f, Mount.Profile.SalvoInterval);
    if (Attack.Weapon.Kind == EOVAttackKind::Beam)
    {
        if (!SweepAttack(Attack, Attack.Position + Attack.Direction * Attack.Weapon.Range)) Attack.Result = EOVAttackResult::Miss;
        Attack.ResolvedAt = GameTime; RecordEvent(Attack);
    }
    else Attacks.Add(Attack);
}

bool UOVCombatSubsystem::SegmentBox(FVector Start, FVector End, const FTransform& Transform,
    FVector HalfExtent, float& HitFraction, FVector& HitNormal)
{
    const FVector S = Transform.InverseTransformPosition(Start);
    const FVector D = Transform.InverseTransformPosition(End) - S;
    double Entry = 0, Exit = 1; FVector LocalNormal = FVector::ZeroVector;
    for (int32 Axis = 0; Axis < 3; ++Axis)
    {
        if (FMath::Abs(D[Axis]) < 1.e-9)
        { if (S[Axis] < -HalfExtent[Axis] || S[Axis] > HalfExtent[Axis]) return false; continue; }
        double Near = (-HalfExtent[Axis] - S[Axis]) / D[Axis];
        double Far = (HalfExtent[Axis] - S[Axis]) / D[Axis];
        double Sign = -1;
        if (Near > Far) { Swap(Near, Far); Sign = 1; }
        if (Near >= Entry) { Entry = Near; LocalNormal = FVector::ZeroVector; LocalNormal[Axis] = Sign; }
        Exit = FMath::Min(Exit, Far);
        if (Entry > Exit) return false;
    }
    if (Entry < 0 || Entry > 1 || Exit < 0) return false;
    if (LocalNormal.IsNearlyZero()) LocalNormal = -D.GetSafeNormal();
    HitFraction = float(Entry);
    HitNormal = Transform.TransformVectorNoScale(LocalNormal / Transform.GetScale3D()).GetSafeNormal();
    return true;
}

bool UOVCombatSubsystem::SweepAttack(FOVAttackState& Attack, FVector End)
{
    if (Attack.Result != EOVAttackResult::Flying) return false;
    AOVShip* Closest = nullptr; float BestFraction = 1.f; FVector BestNormal = FVector::ZeroVector; FName HitWeaponId;
    for (const FOVCombatShipState& State : Ships)
    {
        AOVShip* Ship = State.Ship;
        if (!IsValid(Ship) || Ship == Attack.Attacker.Get() || !Ship->Systems || !Ship->Systems->Layout
            || Ship->Systems->bWrecked || State.bDeparted) continue;
        float Fraction; FVector Normal;
        const auto& Boxes=Ship->Systems->Layout->CombatHullBoxes;
        if (Ship->Systems->bShieldActive && Ship->Systems->Shield > 0 && (!Boxes.IsEmpty() || Ship->Systems->Layout->FittedShieldRadii.GetMin()>0))
        {
            const FVector ShieldRadii = Ship->GetLocalShieldRadii();
            if (SegmentEllipsoid(Attack.Position, End, Ship->GetActorTransform(), ShieldRadii, Fraction, Normal)
                && Fraction <= BestFraction)
            {
                Closest = Ship; BestFraction = Fraction; BestNormal = Normal; HitWeaponId = NAME_None;
                const FVector RayDir = (End - Attack.Position).GetSafeNormal();
                const FVector LookAheadEnd = Attack.Position + RayDir * FMath::Max(FVector::Dist(Attack.Position, End), TargetRadius(Ship) * 2.5f);
                float BestMountFrac = 1.f;
                for (const FOVWeaponMountDefinition& Mount : Ship->Systems->Layout->WeaponMounts)
                {
                    const FOVWeaponMountState* Health = Ship->Systems->WeaponMountStates.FindByPredicate(
                        [&](const FOVWeaponMountState& H) { return H.WeaponMountId == Mount.WeaponId; });
                    if (!Health || Health->HP <= 0) continue;
                    float MountFrac; FVector MountNorm;
                    if (SegmentMount(Attack.Position, LookAheadEnd, Ship->GetActorTransform(), Mount.LocalPosition,
                        Mount.GeometryScale * Settings->MountCollisionRadius, MountFrac, MountNorm) && MountFrac <= BestMountFrac)
                    { BestMountFrac = MountFrac; HitWeaponId = Mount.WeaponId; }
                }
            }
            continue;
        }
        if (Boxes.IsEmpty())
        {
            if (SegmentBox(Attack.Position, End, Ship->GetActorTransform(), Ship->Systems->Layout->LocalHalfExtent, Fraction, Normal)
                && Fraction <= BestFraction)
            { Closest = Ship; BestFraction = Fraction; BestNormal = Normal; HitWeaponId = NAME_None; }
        }
        else for (const FOVCombatHullBox& Box : Boxes)
        {
            FTransform BoxTransform=Ship->GetActorTransform();
            BoxTransform.SetLocation(BoxTransform.TransformPosition(Box.LocalCenter));
            if (SegmentBox(Attack.Position, End, BoxTransform, Box.HalfExtent, Fraction, Normal) && Fraction <= BestFraction)
            { Closest = Ship; BestFraction = Fraction; BestNormal = Normal; HitWeaponId = NAME_None; }
        }
        // These small local sphere proxies are provisional geometry, not the final turret model.
        for (const FOVWeaponMountDefinition& Mount : Ship->Systems->Layout->WeaponMounts)
        {
            const FOVWeaponMountState* Health = Ship->Systems->WeaponMountStates.FindByPredicate(
                [&](const FOVWeaponMountState& H) { return H.WeaponMountId == Mount.WeaponId; });
            if (!Health || Health->HP <= 0) continue;
            if (SegmentMount(Attack.Position, End, Ship->GetActorTransform(), Mount.LocalPosition,
                Mount.GeometryScale * Settings->MountCollisionRadius, Fraction, Normal) && Fraction <= BestFraction)
            { Closest = Ship; BestFraction = Fraction; BestNormal = Normal; HitWeaponId = Mount.WeaponId; }
        }
    }
    float BestAsteroidFraction = BestFraction;
    bool bHitAsteroid = false;
    for (const FVector4& Ast : EnvironmentAsteroids)
    {
        float AstFrac = 1.f; FVector AstNorm;
        if (SegmentMount(Attack.Position, End, FTransform::Identity, FVector(Ast.X, Ast.Y, Ast.Z), float(Ast.W), AstFrac, AstNorm)
            && AstFrac < BestAsteroidFraction)
        {
            BestAsteroidFraction = AstFrac;
            bHitAsteroid = true;
        }
    }
    if (bHitAsteroid)
    {
        Attack.Position = FMath::Lerp(Attack.Position, End, BestAsteroidFraction);
        Attack.Result = EOVAttackResult::Miss;
        Attack.ResolvedAt = GameTime;
        return false;
    }
    if (!Closest) { Attack.Position = End; return false; }
    const FVector Point = FMath::Lerp(Attack.Position, End, BestFraction);
    return ResolveImpact(Attack, Closest, Point, BestNormal, HitWeaponId);
}

bool UOVCombatSubsystem::ResolveImpact(FOVAttackState& Attack, AOVShip* HitShip, FVector Point, FVector Normal, FName HitWeaponId)
{
    if (Attack.Result != EOVAttackResult::Flying || Attack.AttackId <= 0 || Attack.AttackId > ShotCount || ResolvedAttackIds.Contains(Attack.AttackId)
        || !IsValid(HitShip) || !HitShip->Systems
        || HitShip->Systems->bWrecked || IsDeparted(HitShip)) return false;
    // Mark resolved before the damage path, including its queued secondary explosions.
    Attack.Result = EOVAttackResult::Hit; Attack.Position = Point; Attack.HitShipId = HitShip->GetFName();
    Attack.ResolvedAt = GameTime; ResolvedAttackIds.Add(Attack.AttackId);
    UOVShipSystemsComponent* Systems = HitShip->Systems;
    Attack.bPlayerObserved=Systems->FactionId==0;
    if(!Attack.bPlayerObserved&&Settings&&!Settings->bDevelopmentOmniscient)
        for(const auto& Observer:Ships)
            if(IsValid(Observer.Ship)&&Observer.Ship->Systems&&Observer.Ship->Systems->FactionId==0
                &&GetDetection(Observer.Ship,HitShip).bDetected){Attack.bPlayerObserved=true;break;}
    const bool bShieldWasActive=Systems->bShieldActive&&Systems->Shield>0;
    TArray<bool,TInlineAllocator<64>> ArmorWasAlive,FacilityWasAlive;
    if(Attack.bPlayerObserved) {
        for(const auto& Armor:Systems->ArmorSections)ArmorWasAlive.Add(Armor.HP>0);
    }
    if(Systems->FactionId==0) {
        for(int32 I=0;I<Systems->Slots.Num();++I)FacilityWasAlive.Add(Systems->Layout&&Systems->Layout->Slots.IsValidIndex(I)&&Systems->Layout->Slots[I].Facility!=EOVFacilityType::None&&Systems->Slots[I].HP>0);
    }
    Systems->IncomingSourceId = Attack.AttackerId;
    Systems->IncomingFactionId = Attack.FactionId;
    Systems->IncomingWeaponId = Attack.WeaponId;
    Attack.ImpactWeaponId = HitWeaponId;
    if (!HitWeaponId.IsNone()) Systems->ApplyWorldWeaponHit(HitWeaponId, Point, Normal, Attack.Damage, true);
    else Systems->ApplyWorldHit(Point, Normal, Attack.Damage, EOVDamageCause::External, true);
    if(Attack.bPlayerObserved) {
        Attack.bShieldDepleted=bShieldWasActive&&!Systems->bShieldActive&&Systems->Shield<=0;
        for(int32 I=0;I<ArmorWasAlive.Num()&&I<Systems->ArmorSections.Num();++I)if(ArmorWasAlive[I]&&Systems->ArmorSections[I].HP<=0)++Attack.ArmorSectionsDestroyed;
    }
    if(Systems->FactionId==0) {
        for(int32 I=0;I<FacilityWasAlive.Num()&&I<Systems->Slots.Num();++I)if(FacilityWasAlive[I]&&Systems->Slots[I].HP<=0) {
            ++Attack.FacilitiesDestroyed;
            if(Systems->Layout&&Systems->Layout->Slots.IsValidIndex(I)) {
                const auto Type=Systems->Layout->Slots[I].Facility;
                if(Type==EOVFacilityType::Engine||Type==EOVFacilityType::WeaponControl||Type==EOVFacilityType::ShieldControl||Type==EOVFacilityType::FighterControl||Type==EOVFacilityType::Interception)++Attack.CriticalFacilitiesDestroyed;
            }
        }
    }
    Attack.ArmorId = Systems->LastArmorSection; Attack.SlotId = Systems->LastDamageSlot;
    Attack.HullDamage = Systems->LastHullDamage; Attack.Explosions = Systems->LastExplosionCount;
    if (bShieldWasActive)
    {
        HitShip->TriggerShieldImpact(Point, Normal, Attack.Damage);
    }
    if (Systems->bWrecked)
    {
        HitShip->TriggerWreckDissolve();
    }
    ++HitCount;
    return true;
}

bool UOVCombatSubsystem::ApplyInterceptionDamage(int32 AttackId, float Damage)
{
    if (!bEnabled || !FMath::IsFinite(Damage) || Damage <= 0 || ResolvedAttackIds.Contains(AttackId)) return false;
    const int32 Index = Attacks.IndexOfByPredicate([AttackId](const FOVAttackState& A) { return A.AttackId == AttackId; });
    if (!Attacks.IsValidIndex(Index)) return false;
    FOVAttackState& Attack = Attacks[Index];
    if (Attack.Result != EOVAttackResult::Flying || Attack.InterceptHP <= 0
        || (Attack.Weapon.Kind != EOVAttackKind::Missile && Attack.Weapon.Kind != EOVAttackKind::GuidedMissile)) return false;
    Attack.InterceptHP = FMath::Max(0.f, Attack.InterceptHP - Damage);
    if (Attack.InterceptHP <= 0)
    {
        Attack.Result = EOVAttackResult::Intercepted;
        Attack.ResolvedAt = GameTime;
        RecordEvent(Attack);
        // Remove before the next sweep; RecordEvent also rejects stale copied attack impacts.
        Attacks.RemoveAt(Index);
    }
    return true;
}

void UOVCombatSubsystem::AdvanceAttacks(float Delta)
{
    CSV_SCOPED_TIMING_STAT_EXCLUSIVE(OV_CombatCollision);
    TRACE_CPUPROFILER_EVENT_SCOPE(OV_CombatCollision);
    for (FOVAttackState& Attack : Attacks)
    {
        Attack.Age += Delta;
        if (Attack.Weapon.Kind == EOVAttackKind::GuidedMissile)
        {
            AOVShip* Target = Attack.Target.Get();
            // Sensor/source state is deliberately absent here: this attack already owns its launch snapshot.
            if (!IsValid(Target) || !Target->Systems || Target->Systems->bWrecked) Attack.Result = EOVAttackResult::TargetWrecked;
            else if (IsDeparted(Target)) Attack.Result = EOVAttackResult::TargetDeparted;
            else if (Attack.Age >= Attack.TrackingLifetime) Attack.Result = EOVAttackResult::TrackingExpired;
            else
            {
                const FVector Desired = (Target->GetActorTransform().TransformPosition(Attack.TargetLocalPoint) + Attack.AimOffset - Attack.Position).GetSafeNormal();
                Attack.Direction = GuidedDirection(Attack.Direction, Desired, Attack.Weapon.GuidanceTurnRate, Delta);
            }
        }
        if (Attack.Result == EOVAttackResult::Flying)
        {
            if (Attack.Age >= Attack.Weapon.Lifetime) Attack.Result = EOVAttackResult::Expired;
            else if (Attack.Weapon.Kind == EOVAttackKind::Missile && Attack.VLSDuration > 0)
            {
                // Every coefficient was frozen at launch. This branch never reads Target or attacker transforms.
                const FVector ExitDirection = (Attack.FrozenAimPoint - Attack.VLSControlPoint).GetSafeNormal();
                const FVector End = Attack.Age < Attack.VLSDuration
                    ? FixedVLSPoint(Attack, Attack.Age / Attack.VLSDuration)
                    : Attack.FrozenAimPoint + ExitDirection * Attack.Weapon.Speed * (Attack.Age - Attack.VLSDuration);
                Attack.Direction = (End - Attack.Position).GetSafeNormal();
                SweepAttack(Attack, End);
            }
            else SweepAttack(Attack, Attack.Position + Attack.Direction * Attack.Weapon.Speed * Delta);
        }
        if (Attack.Result != EOVAttackResult::Flying)
        { Attack.ResolvedAt = GameTime; RecordEvent(Attack); }
    }
    Attacks.RemoveAll([](const FOVAttackState& Attack) { return Attack.Result != EOVAttackResult::Flying; });
}

void UOVCombatSubsystem::RecordEvent(const FOVAttackState& Attack)
{
    ResolvedAttackIds.Add(Attack.AttackId);
    RecentEvents.Add(Attack);
    if (RecentEvents.Num() > 128) RecentEvents.RemoveAt(0);
    if(Attack.Result==EOVAttackResult::Hit) {
        const FOVCombatShipState* Hit=Ships.FindByPredicate([&](const FOVCombatShipState& S){return S.ShipId==Attack.HitShipId;});
        if(Hit)if(auto* World=GetWorld())if(auto* Notices=World->GetSubsystem<UOVBattleNotices>())Notices->RecordCombatHit(Attack,Hit->Ship);
    }
    UE_LOG(LogTemp, Log, TEXT("OVCombat attack=%d source=%s faction=%d weapon=%s result=%d target=%s mount=%s hull=%.2f explosions=%d"),
        Attack.AttackId, *Attack.AttackerId.ToString(), Attack.FactionId, *Attack.WeaponId.ToString(),
        int32(Attack.Result), *Attack.HitShipId.ToString(), *Attack.ImpactWeaponId.ToString(), Attack.HullDamage, Attack.Explosions);
}

void UOVCombatSubsystem::ClearPending(FOVCombatShipState& State)
{
 State.ForcedMount=NAME_None;
 for(auto& M:State.Mounts) {M.bFireRequested=false;M.RequestedTarget.Reset();M.SalvoRemaining=0;M.SalvoTarget.Reset();M.TrackingRemaining=0;M.bHadDetection=false;}
}
void UOVCombatSubsystem::CancelAttackOrder(AOVShip* Ship)
{
 const int32 I=FindShip(Ship);if(I==INDEX_NONE)return;
 ClearPending(Ships[I]);Ship->AttackTarget=nullptr;Ships[I].TargetFaction=-1;
}
bool UOVCombatSubsystem::SetAttackIntent(AOVShip* Ship,EOVAttackIntent Intent,FName ForceMount)
{
 const int32 I=FindShip(Ship);if(I==INDEX_NONE||!Ship->Systems->CanCommand()||IsDeparted(Ship))return false;
 auto& S=Ships[I];
 if(!ForceMount.IsNone() && (Intent!=EOVAttackIntent::DisableWeapons||FindMount(S,ForceMount)==INDEX_NONE||!IsValidTarget(Ship,Ship->AttackTarget)))return false;
 ClearPending(S);S.Intent=Intent;S.ForcedMount=ForceMount;return true;
}
void UOVCombatSubsystem::SetProtectionPolicy(AOVShip* Ship,EOVProtectionPolicy Policy)
{
 const int32 I=FindShip(Ship);if(I==INDEX_NONE||!Ship->Systems->CanCommand())return;
 ClearPending(Ships[I]);Ships[I].Protection=Policy;
}
bool UOVCombatSubsystem::IsTargetProtected(AOVShip* Ship,AOVShip* Target) const
{
 const int32 I=FindShip(Ship);if(I==INDEX_NONE||!IsValid(Target)||!Target->Systems)return false;
 const int32 F=Ship->Systems->FactionId;
 if(!Target->Systems->HasInteriorPresence(F))return false;
 const auto Policy=Ships[I].Protection;
 return Policy==EOVProtectionPolicy::UntilExit || (Policy==EOVProtectionPolicy::UntilObjective&&!Target->Systems->IsInteriorObjectiveComplete(F));
}
bool UOVCombatSubsystem::ResolveAimPoint(const FOVCombatShipState& State,AOVShip* Target,FVector& Point) const
{
 if(!IsValidTarget(State.Ship,Target)||!GetDetection(State.Ship,Target).bDetected)return false;
 if(State.Intent==EOVAttackIntent::DestroyHull) {Point=Target->GetActorLocation();return true;}
 // Prototype observation: detection exposes exterior mounting points only, never interior roster/state.
 // For authored hull shapes, do not choose a mount behind the target hull: such
 // a shot hits the hull first and defeats the requested disarm intent.
 for(const auto& Def:Target->Systems->Layout->WeaponMounts) {
  const auto* HP=Target->Systems->WeaponMountStates.FindByPredicate([&](const auto& H){return H.WeaponMountId==Def.WeaponId;});
  if(!HP||HP->HP<=0)continue;
  const FVector Candidate=Target->GetActorTransform().TransformPosition(Def.LocalPosition);
  const auto& Boxes=Target->Systems->Layout->CombatHullBoxes;
  if(!Boxes.IsEmpty()) {
   const FVector Origin=State.Ship->GetActorLocation();float MountFraction=1.f;FVector Normal;
   if(!SegmentMount(Origin,Candidate,Target->GetActorTransform(),Def.LocalPosition,Def.GeometryScale * Settings->MountCollisionRadius,MountFraction,Normal))continue;
   bool bOccluded=false;
   for(const FOVCombatHullBox& Box:Boxes) {
    FTransform Transform=Target->GetActorTransform();Transform.SetLocation(Transform.TransformPosition(Box.LocalCenter));
    float HullFraction=1.f;FVector HullNormal;
    if(SegmentBox(Origin,Candidate,Transform,Box.HalfExtent,HullFraction,HullNormal)&&HullFraction+1.e-5f<MountFraction) {bOccluded=true;break;}
   }
   if(bOccluded)continue;
  }
  Point=Candidate;return true;
 }
 return false;
}
FString UOVCombatSubsystem::GetOrderStatus(AOVShip* Ship) const
{
 const int32 I=FindShip(Ship);if(I==INDEX_NONE)return TEXT("NO ORDER");
 const auto& S=Ships[I];
 if(!IsValidTarget(Ship,Ship->AttackTarget))return TEXT("WAIT TARGET");
 if(IsTargetProtected(Ship,Ship->AttackTarget))return S.ForcedMount.IsNone()?TEXT("HOLD: INTERIOR ALLY"):TEXT("FORCE GUN: HULL HIT RISK");
 FVector P;if(!ResolveAimPoint(S,Ship->AttackTarget,P)) {
  if(!GetDetection(Ship,Ship->AttackTarget).bDetected)return TEXT("WAIT: DETECTION");
  const bool bAlive=Ship->AttackTarget->Systems->WeaponMountStates.ContainsByPredicate([](const FOVWeaponMountState& M){return M.HP>0;});
  return bAlive?TEXT("WAIT: NO ACCESSIBLE GUNS"):TEXT("WAIT: NO OBSERVED GUNS");
 }
 return S.Intent==EOVAttackIntent::DisableWeapons?TEXT("DISABLE OBSERVED GUNS"):TEXT("DESTROY HULL");
}
