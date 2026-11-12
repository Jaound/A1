#include "OVCombatSubsystem.h"
#include "OVShip.h"
#include "OVShipSystemsComponent.h"
#include "OVController.h"
#include "Engine/World.h"

namespace
{
constexpr float FixedStep = 1.f / 120.f;
constexpr float NeutralElevation = 15.f;
constexpr float PostShotTracking = 3.5f;

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
}

int32 UOVCombatSubsystem::FindShip(AOVShip* Ship) const
{
    return Ships.IndexOfByPredicate([Ship](const FOVCombatShipState& State) { return State.Ship == Ship; });
}

int32 UOVCombatSubsystem::FindMount(const FOVCombatShipState& State, FName Id) const
{
    return State.Mounts.IndexOfByPredicate([Id](const FOVCombatMountState& Mount) { return Mount.MountId == Id; });
}

void UOVCombatSubsystem::RegisterShip(AOVShip* Ship, int32 RadarTier, bool bRadarSupport)
{
    if (!IsValid(Ship) || !Ship->Systems || !Ship->Systems->Layout || !Settings) return;
    const int32 Existing = FindShip(Ship);
    if (Existing != INDEX_NONE)
    { Ships[Existing].RadarTier = RadarTier; Ships[Existing].bRadarSupport = bRadarSupport; return; }
    FOVCombatShipState State;
    State.Ship = Ship; State.ShipId = Ship->GetFName();
    State.RadarTier = FMath::Clamp(RadarTier, 1, 4); State.bRadarSupport = bRadarSupport;
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
    if (!bEnabled || FindShip(Ship) == INDEX_NONE || !Ship->Systems->CanCommand()
        || (Target && !IsValidTarget(Ship, Target))) return false;
    Ship->AttackTarget = Target;
    // A change of target cannot carry a queued volley to a new recipient.
    for (FOVCombatMountState& Mount : Ships[FindShip(Ship)].Mounts)
    { Mount.bFireRequested = false; Mount.SalvoRemaining = 0; }
    return true;
}

bool UOVCombatSubsystem::RequestFire(AOVShip* Ship, FName MountId)
{
    const int32 Index = FindShip(Ship);
    if (!bEnabled || !Ships.IsValidIndex(Index) || !Ship->Systems->CanCommand()
        || !IsValidTarget(Ship, Ship->AttackTarget)) return false;
    bool bAccepted = false;
    for (FOVCombatMountState& Mount : Ships[Index].Mounts)
        if (MountId.IsNone() || Mount.MountId == MountId)
        { Mount.bFireRequested = true; bAccepted = true; }
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
    // Fixture layouts encode only small vs deck count. Three-deck test hulls permit T4.
    const int32 Cap = Ship->Systems->Layout->bSmallHull ? 1 : Ship->Systems->Layout->DeckCount < 3 ? 2 : 4;
    if (Profile->Tier > Cap) return false;
    FOVCombatMountState& Mount = Ships[Index].Mounts[MountIndex];
    Mount.Profile = *Profile; Mount.SalvoRemaining = 0; Mount.bFireRequested = false;
    return true;
}

void UOVCombatSubsystem::NotifyFTLDeparture(AOVShip* Ship)
{
    const int32 Index = FindShip(Ship);
    if (!Ships.IsValidIndex(Index)) return;
    Ships[Index].bDeparted = true;
    Ship->StopMovement(); Ship->AttackTarget = nullptr;
    Ship->SetActorHiddenInGame(true);
}

FOVDetectionInfo UOVCombatSubsystem::GetDetection(AOVShip* Ship, AOVShip* Target) const
{
    FOVDetectionInfo Best;
    if (!Settings || !IsValidTarget(Ship, Target) || IsDeparted(Ship) || Ship->Systems->bWrecked) return Best;
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
    const FOVWeaponMountState* Health = Systems->WeaponMountStates.FindByPredicate(
        [&](const FOVWeaponMountState& H) { return H.WeaponMountId == Mount.MountId; });
    if (!Health || Health->HP <= 0) return Block(TEXT("MOUNT DESTROYED"));
    if (Systems->GetEfficiency(EOVFacilityType::WeaponControl) <= 0) return Block(TEXT("WEAPON CONTROL OFF"));
    if (Systems->GetEfficiency(SupplyFor(Def->Type)) <= 0) return Block(TEXT("SUPPLY OFF"));
    if (Def->Type == EOVWeaponType::Beam && Ship->bFTLCharging) return Block(TEXT("FTL CHARGING"));
    AOVShip* Target = Ship->AttackTarget;
    if (!IsValidTarget(Ship, Target)) return Block(TEXT("NO VALID TARGET"));
    const FVector Origin = MountOrigin(State, *Def);
    const FVector Offset = Target->GetActorLocation() - Origin;
    R.Distance = Offset.Size();
    if (R.Distance > Mount.Profile.Range) return Block(TEXT("OUT OF RANGE"));
    R.Detection = GetDetection(Ship, Target);
    if (!R.Detection.bDetected) return Block(TEXT("NO DETECTION"));
    const FVector LocalDirection = Ship->GetActorQuat().UnrotateVector(Offset).GetSafeNormal();
    const float Elevation = LocalDirection.Rotation().Pitch;
    if ((Def->LocalPosition.Z >= 0 && Elevation < -NeutralElevation)
        || (Def->LocalPosition.Z < 0 && Elevation > NeutralElevation)) return Block(TEXT("MOUNT BLIND ARC"));
    R.AimError = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(
        FVector::DotProduct(Mount.LocalAim.Vector(), LocalDirection), -1.0, 1.0)));
    if (R.AimError > Mount.Profile.FireTolerance) return Block(TEXT("TURRET ALIGNING"));
    if (FindBlockingFriend(Ship, Target, Origin)) return Block(TEXT("FRIENDLY IN LINE"));
    const float ControlBonus = Settings->ControlAccuracyBonus * (Systems->GetEfficiency(EOVFacilityType::WeaponControl) - 1.f);
    R.Accuracy = FMath::Clamp(Mount.Profile.Accuracy + R.Detection.AccuracyBonus + ControlBonus, 0.f,
        Mount.Profile.Kind == EOVAttackKind::Beam ? 0.995f : 1.f);
    if (!bIgnoreCooldown && R.Cooldown > UE_KINDA_SMALL_NUMBER) return Block(TEXT("COOLDOWN"));
    R.bReady = true; R.Reason = TEXT("READY"); return R;
}

AOVShip* UOVCombatSubsystem::FindBlockingFriend(AOVShip* Ship, AOVShip* Target, FVector Origin) const
{
    for (const FOVCombatShipState& Other : Ships)
    {
        AOVShip* Friend = Other.Ship;
        if (!IsValid(Friend) || Friend == Ship || Friend == Target || !Friend->Systems || !Friend->Systems->Layout
            || Friend->Systems->bWrecked || Other.bDeparted || Friend->Systems->FactionId != Ship->Systems->FactionId) continue;
        float Fraction; FVector Normal;
        if (SegmentBox(Origin, Target->GetActorLocation(), Friend->GetActorTransform(),
            Friend->Systems->Layout->LocalHalfExtent, Fraction, Normal)) return Friend;
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
        Ship->Systems->Simulate(Delta);
        Ship->bFriendly = Ship->Systems->FactionId == 0;
        if (Ship->Systems->GetEfficiency(EOVFacilityType::Engine) <= 0) Ship->bFTLCharging = false;
        Ship->Simulate(Delta);
    }
    if (AOVController* Controller = Cast<AOVController>(GetWorld()->GetFirstPlayerController()))
        Controller->ResolveContacts(Delta);
    for (FOVCombatShipState& State : Ships)
    {
        AOVShip* Ship = State.Ship;
        if (!IsValid(Ship) || !Ship->Systems || State.bDeparted || Ship->Systems->bWrecked || Ship->Systems->bNeutral) continue;
        const bool bAutomatic = !Ship->bManual || !Ship->Systems->CanCommand();
        if (bAutomatic && (!IsValidTarget(Ship, Ship->AttackTarget) || !GetDetection(Ship, Ship->AttackTarget).bDetected))
        {
            AOVShip* Best = nullptr; double BestDistance = TNumericLimits<double>::Max();
            for (const FOVCombatShipState& Candidate : Ships)
            {
                AOVShip* Target = Candidate.Ship;
                if (!IsValidTarget(Ship, Target) || !GetDetection(Ship, Target).bDetected) continue;
                const double Distance = FVector::DistSquared(Ship->GetActorLocation(), Target->GetActorLocation());
                if (Distance < BestDistance) { Best = Target; BestDistance = Distance; }
            }
            Ship->AttackTarget = Best;
        }
        for (FOVCombatMountState& Mount : State.Mounts)
        {
            Mount.Cooldown = FMath::Max(0.f, Mount.Cooldown - Delta);
            Mount.SalvoDelay = FMath::Max(0.f, Mount.SalvoDelay - Delta);
            UpdateAim(State, Mount, Delta);
            if (!(bAutomatic || Mount.bFireRequested || Mount.SalvoRemaining > 0)) continue;
            const FOVFireReadiness Ready = Evaluate(State, Mount, false);
            if (!Ready.bReady) continue;
            Launch(State, Mount);
            Mount.bFireRequested = false;
        }
    }
    AdvanceAttacks(Delta);
}

void UOVCombatSubsystem::UpdateAim(FOVCombatShipState& State, FOVCombatMountState& Mount, float Delta)
{
    const FOVWeaponMountDefinition* Def = MountDefinition(State, Mount.MountId);
    if (!Def) return;
    AOVShip* Ship = State.Ship;
    Mount.TrackingRemaining = FMath::Max(0.f, Mount.TrackingRemaining - Delta);
    const bool bDetected = IsValidTarget(Ship, Ship->AttackTarget) && GetDetection(Ship, Ship->AttackTarget).bDetected;
    if (bDetected) Mount.LastTrackedWorldPoint = Ship->AttackTarget->GetActorLocation();
    if (!bDetected && Mount.TrackingRemaining <= 0)
    { Mount.AngularVelocity = FVector2D::ZeroVector; return; }
    const FVector Local = Ship->GetActorQuat().UnrotateVector(Mount.LastTrackedWorldPoint - MountOrigin(State, *Def));
    const FRotator Desired = Local.Rotation();
    TurnAxis(Mount.LocalAim.Yaw, Mount.AngularVelocity.X, Desired.Yaw, Mount.Profile.TraverseSpeed, Mount.Profile.TraverseAcceleration, Delta);
    TurnAxis(Mount.LocalAim.Pitch, Mount.AngularVelocity.Y, Desired.Pitch, Mount.Profile.TraverseSpeed, Mount.Profile.TraverseAcceleration, Delta);
    Mount.LocalAim.Roll = 0;
}

void UOVCombatSubsystem::Launch(FOVCombatShipState& State, FOVCombatMountState& Mount)
{
    const FOVFireReadiness Ready = Evaluate(State, Mount, false);
    const FOVWeaponMountDefinition* Def = MountDefinition(State, Mount.MountId);
    if (!Ready.bReady || !Def) return;
    FOVAttackState Attack;
    Attack.AttackId = ++ShotCount;
    Attack.AttackerId = State.ShipId; Attack.Attacker = State.Ship;
    Attack.FactionId = State.Ship->Systems->FactionId;
    Attack.WeaponId = Mount.MountId; Attack.Weapon = Mount.Profile;
    Attack.Target = State.Ship->AttackTarget;
    Attack.Origin = Attack.Position = MountOrigin(State, *Def);
    Attack.Accuracy = Ready.Accuracy;
    Attack.Damage = Mount.Profile.Damage * State.Ship->Systems->GetEfficiency(SupplyFor(Def->Type));
    FVector AimPoint = Attack.Target->GetActorLocation();
    if (Random.FRand() > Ready.Accuracy)
    {
        const FVector Sight = (AimPoint - Attack.Position).GetSafeNormal();
        FVector Side, Up; Sight.FindBestAxisVectors(Side, Up);
        const float Angle = Random.FRandRange(0.f, 2.f * PI);
        Attack.AimOffset = (Side * FMath::Cos(Angle) + Up * FMath::Sin(Angle))
            * TargetRadius(Attack.Target.Get()) * FMath::Max(1.1f, Settings->MissOffsetRadiusFactor);
        AimPoint += Attack.AimOffset;
    }
    Attack.Direction = (AimPoint - Attack.Position).GetSafeNormal();
    ++Mount.ShotsFired; Mount.TrackingRemaining = PostShotTracking;
    const float Efficiency = State.Ship->Systems->GetEfficiency(EOVFacilityType::WeaponControl)
        * State.Ship->Systems->GetEfficiency(SupplyFor(Def->Type));
    Mount.Cooldown = FMath::Max(0.01f, Mount.Profile.Interval / FMath::Max(0.05f, Efficiency));
    if (Attack.Weapon.Kind == EOVAttackKind::Beam)
    {
        if (!SweepAttack(Attack, Attack.Position + Attack.Direction * Attack.Weapon.Range)) Attack.Result = EOVAttackResult::Miss;
        RecordEvent(Attack);
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
    AOVShip* Closest = nullptr; float BestFraction = 1.f; FVector BestNormal = FVector::ZeroVector;
    for (const FOVCombatShipState& State : Ships)
    {
        AOVShip* Ship = State.Ship;
        if (!IsValid(Ship) || Ship == Attack.Attacker.Get() || !Ship->Systems || !Ship->Systems->Layout
            || Ship->Systems->bWrecked || State.bDeparted) continue;
        float Fraction; FVector Normal;
        if (SegmentBox(Attack.Position, End, Ship->GetActorTransform(), Ship->Systems->Layout->LocalHalfExtent, Fraction, Normal)
            && Fraction <= BestFraction)
        { Closest = Ship; BestFraction = Fraction; BestNormal = Normal; }
    }
    if (!Closest) { Attack.Position = End; return false; }
    const FVector Point = FMath::Lerp(Attack.Position, End, BestFraction);
    return ResolveImpact(Attack, Closest, Point, BestNormal);
}

bool UOVCombatSubsystem::ResolveImpact(FOVAttackState& Attack, AOVShip* HitShip, FVector Point, FVector Normal)
{
    if (Attack.Result != EOVAttackResult::Flying || !IsValid(HitShip) || !HitShip->Systems
        || HitShip->Systems->bWrecked || IsDeparted(HitShip)) return false;
    // Mark resolved before the damage path, including its queued secondary explosions.
    Attack.Result = EOVAttackResult::Hit; Attack.Position = Point; Attack.HitShipId = HitShip->GetFName();
    UOVShipSystemsComponent* Systems = HitShip->Systems;
    Systems->IncomingSourceId = Attack.AttackerId;
    Systems->IncomingFactionId = Attack.FactionId;
    Systems->IncomingWeaponId = Attack.WeaponId;
    Systems->ApplyWorldHit(Point, Normal, Attack.Damage, EOVDamageCause::External);
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
        if (!SweepAttack(Attack, Attack.Position + Attack.Direction * Attack.Weapon.Speed * Delta)
            && Attack.Age >= Attack.Weapon.Lifetime) Attack.Result = EOVAttackResult::Expired;
        if (Attack.Result != EOVAttackResult::Flying) RecordEvent(Attack);
    }
    Attacks.RemoveAll([](const FOVAttackState& Attack) { return Attack.Result != EOVAttackResult::Flying; });
}

void UOVCombatSubsystem::RecordEvent(const FOVAttackState& Attack)
{
    RecentEvents.Add(Attack);
    if (RecentEvents.Num() > 128) RecentEvents.RemoveAt(0);
    UE_LOG(LogTemp, Log, TEXT("OVCombat attack=%d source=%s faction=%d weapon=%s result=%d target=%s hull=%.2f explosions=%d"),
        Attack.AttackId, *Attack.AttackerId.ToString(), Attack.FactionId, *Attack.WeaponId.ToString(),
        int32(Attack.Result), *Attack.HitShipId.ToString(), Attack.HullDamage, Attack.Explosions);
}
