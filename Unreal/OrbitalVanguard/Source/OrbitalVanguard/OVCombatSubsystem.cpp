#include "OVCombatSubsystem.h"
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
    return State.Ship->GetActorTransform().TransformPosition(Def.LocalPosition);
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
    bEnabled = false; GameTime = 0; Accumulator = 0;
    ShotCount = 0; HitCount = 0; Ships.Reset(); Attacks.Reset(); RecentEvents.Reset();
    ResolvedAttackIds.Reset();
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
    if (!Def || !Profile || MountIndex == INDEX_NONE || Profile->Family != Def->Type) return false;
    // Changing kind is supported. Re-equipping a different tier requires a new authored
    // loadout, keeping the existing storage-explosion tier data authoritative.
    if (Profile->Tier > Ships[Index].WeaponTierCap || Profile->Tier != FMath::RoundToInt(Def->Tier)) return false;
    FOVCombatMountState& Mount = Ships[Index].Mounts[MountIndex];
    Mount.Profile = *Profile; Mount.SalvoRemaining = 0; Mount.bFireRequested = false;
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
    if (!ResolveAimPoint(State,Target,AimPoint)) return Block(GetDetection(Ship,Target).bDetected?TEXT("NO OBSERVED WEAPONS - WAIT"):TEXT("NO DETECTION"));
    const FVector Offset = AimPoint - Origin;
    R.Distance = Offset.Size();
    if (R.Distance > Mount.Profile.Range) return Block(TEXT("OUT OF RANGE"));
    R.Detection = GetDetection(Ship, Target);
    if (!R.Detection.bDetected) return Block(TEXT("NO DETECTION"));
    const float ControlBonus = Settings->ControlAccuracyBonus * (Systems->GetEfficiency(EOVFacilityType::WeaponControl) - 1.f);
    R.Accuracy = FMath::Clamp(Mount.Profile.Accuracy + R.Detection.AccuracyBonus + ControlBonus, 0.f,
        Mount.Profile.Kind == EOVAttackKind::Beam ? 0.995f : 1.f);
    R.AimSpreadDegrees = Mount.Profile.Kind == EOVAttackKind::Beam ? 0.f
        : FMath::Clamp(Mount.Profile.AimSpreadDegrees * (1.f - R.Accuracy), 0.f, 45.f);
    const FVector LocalDirection = Ship->GetActorQuat().UnrotateVector(Offset).GetSafeNormal();
    const float Elevation = LocalDirection.Rotation().Pitch;
    if ((Def->LocalPosition.Z >= 0 && Elevation < -NeutralElevation)
        || (Def->LocalPosition.Z < 0 && Elevation > NeutralElevation)) return Block(TEXT("MOUNT BLIND ARC"));
    R.AimError = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(
        FVector::DotProduct(Mount.LocalAim.Vector(), LocalDirection), -1.0, 1.0)));
    if (R.AimError > Mount.Profile.FireTolerance) return Block(TEXT("TURRET ALIGNING"));
    if (FindBlockingFriend(Ship, Target, Origin)) return Block(TEXT("FRIENDLY IN LINE"));
    if (!bIgnoreCooldown && R.Cooldown > UE_KINDA_SMALL_NUMBER) return Block(TEXT("COOLDOWN"));
    R.bReady = true; R.Reason = TEXT("READY"); return R;
}

AOVShip* UOVCombatSubsystem::FindBlockingFriend(AOVShip* Ship, AOVShip* Target, FVector Origin) const
{
    FVector Endpoint; const int32 SI=FindShip(Ship);
    if(SI==INDEX_NONE || !ResolveAimPoint(Ships[SI],Target,Endpoint)) return nullptr;
    for (const FOVCombatShipState& Other : Ships)
    {
        AOVShip* Friend = Other.Ship;
        if (!IsValid(Friend) || Friend == Ship || Friend == Target || !Friend->Systems || !Friend->Systems->Layout
            || Friend->Systems->bWrecked || Other.bDeparted || Friend->Systems->FactionId != Ship->Systems->FactionId) continue;
        float Fraction; FVector Normal;
        if (SegmentBox(Origin, Endpoint, Friend->GetActorTransform(),
            Friend->Systems->Layout->LocalHalfExtent, Fraction, Normal)) return Friend;
        for (const FOVWeaponMountDefinition& Mount : Friend->Systems->Layout->WeaponMounts)
        {
            const FOVWeaponMountState* Health = Friend->Systems->WeaponMountStates.FindByPredicate(
                [&](const FOVWeaponMountState& H) { return H.WeaponMountId == Mount.WeaponId; });
            if (Health && Health->HP > 0 && SegmentMount(Origin, Endpoint, Friend->GetActorTransform(),
                Mount.LocalPosition, Settings->MountCollisionRadius, Fraction, Normal)) return Friend;
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
}

void UOVCombatSubsystem::Step(float Delta)
{
    GameTime += Delta;
    for (FOVCombatShipState& State : Ships)
    {
        AOVShip* Ship = State.Ship;
        if (!IsValid(Ship) || !Ship->Systems || State.bDeparted) continue;
        if(State.OrderFaction!=Ship->Systems->FactionId || (Ship->AttackTarget && State.TargetFaction>=0 && State.TargetFaction!=Ship->AttackTarget->Systems->FactionId)) {
            ClearPending(State); State.Intent=EOVAttackIntent::DestroyHull; Ship->AttackTarget=nullptr; State.TargetFaction=-1;
        }
        if(State.bLastAttackAutomatic!=Ship->IsAttackAutomatic()) { ClearPending(State); State.bLastAttackAutomatic=Ship->IsAttackAutomatic(); }
        State.OrderFaction=Ship->Systems->FactionId;
        if(Ship->Systems->bWrecked || (Ship->AttackTarget && !IsValidTarget(Ship,Ship->AttackTarget))) { ClearPending(State); Ship->AttackTarget=nullptr; }
        Ship->Systems->Simulate(Delta);
        Ship->bFriendly = Ship->Systems->FactionId == 0;
        if (Ship->Systems->GetEfficiency(EOVFacilityType::Engine) <= 0) Ship->bFTLCharging = false;
        // Bridge loss preserves the existing 2A stop rule and hands only combat to the fallback AI.
        if (!Ship->Systems->CanCommand()) { Ship->SetAutomation(true, true); Ship->StopMovement(); }
        Ship->Simulate(Delta);
    }
    if (AOVController* Controller = Cast<AOVController>(GetWorld()->GetFirstPlayerController()))
        Controller->ResolveContacts(Delta);
    AdvanceAttacks(Delta);
    for (FOVCombatShipState& State : Ships)
    {
        AOVShip* Ship = State.Ship;
        if (!IsValid(Ship) || !Ship->Systems || State.bDeparted || Ship->Systems->bWrecked || Ship->Systems->bNeutral) continue;
        const bool bAutomatic = Ship->IsAttackAutomatic() || !Ship->Systems->CanCommand();
        if (bAutomatic && (!IsValidTarget(Ship, Ship->AttackTarget) || !GetDetection(Ship, Ship->AttackTarget).bDetected || (IsTargetProtected(Ship,Ship->AttackTarget) && State.ForcedMount.IsNone())))
        {
            AOVShip* Best = nullptr; double BestDistance = TNumericLimits<double>::Max();
            for (const FOVCombatShipState& Candidate : Ships)
            {
                AOVShip* Target = Candidate.Ship;
                if (!IsValidTarget(Ship, Target) || !GetDetection(Ship, Target).bDetected) continue;
                FVector CandidateAim;
                if(IsTargetProtected(Ship,Target) || !ResolveAimPoint(State,Target,CandidateAim)) continue;
                const double Distance = FVector::DistSquared(Ship->GetActorLocation(), Target->GetActorLocation());
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
                && FindBlockingFriend(Ship, Ship->AttackTarget, Ship->GetActorLocation()))
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
            if (!(bAutomatic || Mount.bFireRequested || Mount.SalvoRemaining > 0)) continue;
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
}

void UOVCombatSubsystem::UpdateAim(FOVCombatShipState& State, FOVCombatMountState& Mount, float Delta)
{
    const FOVWeaponMountDefinition* Def = MountDefinition(State, Mount.MountId);
    if (!Def) return;
    AOVShip* Ship = State.Ship;
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
    const FRotator Desired = bDetected || Mount.TrackingRemaining > 0 ? Local.Rotation() : Mount.Profile.StandbyAim;
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
    if (Attack.Weapon.Kind == EOVAttackKind::Missile)
    {
        const FVector LaunchUp = State.Ship->GetActorQuat().GetUpVector() * (Def->LocalPosition.Z < 0 ? -1.f : 1.f);
        Attack.VLSControlPoint = Attack.Origin + LaunchUp * FMath::Max(1.f, Attack.Weapon.VLSHeight);
        FVector Previous = Attack.Origin; double Length = 0;
        for (int32 Sample = 1; Sample <= 16; ++Sample)
        {
            const FVector Point = FixedVLSPoint(Attack, Sample / 16.0);
            Length += FVector::Dist(Previous, Point); Previous = Point;
        }
        Attack.VLSDuration = FMath::Max(.01f, float(Length / Attack.Weapon.Speed));
        Attack.Direction = LaunchUp;
    }
    State.Ship->Systems->NotifyCombatParticipation();
    ++Mount.ShotsFired;
    const float Efficiency = State.Ship->Systems->GetEfficiency(EOVFacilityType::WeaponControl)
        * State.Ship->Systems->GetEfficiency(SupplyFor(Def->Type));
    if (Mount.SalvoRemaining <= 0)
    {
        Mount.Cooldown = FMath::Max(0.01f, Mount.Profile.Interval / FMath::Max(0.05f, Efficiency));
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
        if (SegmentBox(Attack.Position, End, Ship->GetActorTransform(), Ship->Systems->Layout->LocalHalfExtent, Fraction, Normal)
            && Fraction <= BestFraction)
        { Closest = Ship; BestFraction = Fraction; BestNormal = Normal; HitWeaponId = NAME_None; }
        // These small local sphere proxies are provisional geometry, not the final turret model.
        for (const FOVWeaponMountDefinition& Mount : Ship->Systems->Layout->WeaponMounts)
        {
            const FOVWeaponMountState* Health = Ship->Systems->WeaponMountStates.FindByPredicate(
                [&](const FOVWeaponMountState& H) { return H.WeaponMountId == Mount.WeaponId; });
            if (!Health || Health->HP <= 0) continue;
            if (SegmentMount(Attack.Position, End, Ship->GetActorTransform(), Mount.LocalPosition,
                Settings->MountCollisionRadius, Fraction, Normal) && Fraction <= BestFraction)
            { Closest = Ship; BestFraction = Fraction; BestNormal = Normal; HitWeaponId = Mount.WeaponId; }
        }
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
    Systems->IncomingSourceId = Attack.AttackerId;
    Systems->IncomingFactionId = Attack.FactionId;
    Systems->IncomingWeaponId = Attack.WeaponId;
    Attack.ImpactWeaponId = HitWeaponId;
    if (!HitWeaponId.IsNone()) Systems->ApplyWorldWeaponHit(HitWeaponId, Point, Normal, Attack.Damage, true);
    else Systems->ApplyWorldHit(Point, Normal, Attack.Damage, EOVDamageCause::External, true);
    Attack.ArmorId = Systems->LastArmorSection; Attack.SlotId = Systems->LastDamageSlot;
    Attack.HullDamage = Systems->LastHullDamage; Attack.Explosions = Systems->LastExplosionCount;
    ++HitCount;
    return true;
}

void UOVCombatSubsystem::AdvanceAttacks(float Delta)
{
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
                const FQuat Before = Attack.Direction.ToOrientationQuat();
                const FQuat Wanted = Desired.ToOrientationQuat();
                const double Angle = Before.AngularDistance(Wanted);
                const double MaxTurn = FMath::DegreesToRadians(Attack.Weapon.GuidanceTurnRate) * Delta;
                Attack.Direction = FQuat::Slerp(Before, Wanted, Angle > UE_SMALL_NUMBER ? FMath::Min(1.0, MaxTurn / Angle) : 1.0).GetForwardVector();
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
 for(const auto& Def:Target->Systems->Layout->WeaponMounts) {
  const auto* HP=Target->Systems->WeaponMountStates.FindByPredicate([&](const auto& H){return H.WeaponMountId==Def.WeaponId;});
  if(HP&&HP->HP>0) {Point=Target->GetActorTransform().TransformPosition(Def.LocalPosition);return true;}
 }
 return false;
}
FString UOVCombatSubsystem::GetOrderStatus(AOVShip* Ship) const
{
 const int32 I=FindShip(Ship);if(I==INDEX_NONE)return TEXT("NO ORDER");
 const auto& S=Ships[I];
 if(!IsValidTarget(Ship,Ship->AttackTarget))return TEXT("WAIT TARGET");
 if(IsTargetProtected(Ship,Ship->AttackTarget))return S.ForcedMount.IsNone()?TEXT("HOLD: INTERIOR ALLY"):TEXT("FORCE GUN: HULL HIT RISK");
 FVector P;if(!ResolveAimPoint(S,Ship->AttackTarget,P))return GetDetection(Ship,Ship->AttackTarget).bDetected?TEXT("WAIT: NO OBSERVED GUNS"):TEXT("WAIT: DETECTION");
 return S.Intent==EOVAttackIntent::DisableWeapons?TEXT("DISABLE OBSERVED GUNS"):TEXT("DESTROY HULL");
}
