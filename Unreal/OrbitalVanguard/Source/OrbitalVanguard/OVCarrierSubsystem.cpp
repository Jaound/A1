#include "OVCarrierSubsystem.h"
#include "OVCombatSubsystem.h"
#include "OVShip.h"
#include "OVShipSystemsComponent.h"
#include "OVPerformance.h"
#include "Engine/World.h"

namespace
{
bool LivingShip(const AOVShip* Ship)
{
    return IsValid(Ship) && Ship->Systems && Ship->Systems->Layout && !Ship->Systems->bWrecked && !Ship->Systems->bNeutral;
}

bool SegmentSphere(const FVector& Start, const FVector& End, const FVector& Center, double Radius, float& Fraction)
{
    const FVector V = End - Start, Offset = Start - Center;
    const double A = V.SizeSquared();
    if (Offset.SizeSquared() <= Radius * Radius) { Fraction = 0; return true; }
    if (A <= UE_SMALL_NUMBER) return false;
    const double B = FVector::DotProduct(Offset, V), C = Offset.SizeSquared() - Radius * Radius;
    const double D = B * B - A * C;
    if (D < 0) return false;
    const double T = (-B - FMath::Sqrt(D)) / A;
    if (T < 0 || T > 1) return false;
    Fraction = float(T); return true;
}

FVector LeadPoint(FVector Origin, FVector Target, FVector Velocity, double Speed, double Lifetime)
{
    const FVector R = Target - Origin;
    const double A = Velocity.SizeSquared() - Speed * Speed;
    const double B = 2 * FVector::DotProduct(R, Velocity), C = R.SizeSquared();
    const double D = B * B - 4 * A * C;
    if (Speed <= 0 || D < 0 || FMath::Abs(A) < UE_SMALL_NUMBER) return Target;
    const double T1 = (-B - FMath::Sqrt(D)) / (2 * A), T2 = (-B + FMath::Sqrt(D)) / (2 * A);
    double T = T1 > 0 ? T1 : T2;
    if (T2 > 0 && T2 < T) T = T2;
    return T > 0 && T <= Lifetime ? Target + Velocity * T : Target;
}
}

void UOVCarrierSubsystem::Reset(int32 Seed)
{
    Hangars.Reset(); Aircraft.Reset(); Bolts.Reset(); RecentEvents.Reset(); Carriers.Reset();
    StrikeCandidateCache.Reset();
    Profile = FOVAircraftProfile();
    ManeuverProfile = FOVAircraftManeuverProfile();
    GameTime = Accumulator = 0;
    ShotCount = HitCount = LossCount = DeveloperRepairCount = 0;
    NextAircraftId = NextHangarId = NextBoltId = 1;
    Random.Initialize(Seed);
    PatternOffset = Random.FRandRange(0, 2 * PI);
}

bool UOVCarrierSubsystem::RegisterCarrier(AOVShip* Ship, int32 InitialPerHangar)
{
    if (!LivingShip(Ship) || Ship->Systems->Layout->bSmallHull) return false;
    if (Carriers.ContainsByPredicate([Ship](const FOVCarrierState& S) { return S.Carrier == Ship; })) return false;
    TArray<FString> LayoutErrors;
    if (!Ship->Systems->Layout->ValidateLayout(LayoutErrors)) return false;
    bool bAny = false;
    for (const auto& Slot : Ship->Systems->Layout->Slots)
    {
        if (Slot.Facility != EOVFacilityType::Hangar) continue;
        bAny = true;
        FOVHangarState& Hangar = Hangars.AddDefaulted_GetRef();
        Hangar.HangarId = NextHangarId++; Hangar.Carrier = Ship; Hangar.SlotId = Slot.SlotId;
        for (int32 I = 0; I < FMath::Clamp(InitialPerHangar, 0, 20); ++I)
        {
            FOVAircraftRecord& Craft = Aircraft.AddDefaulted_GetRef();
            Craft.AircraftId = NextAircraftId++; Craft.HangarId = Hangar.HangarId;
            Craft.Carrier = Ship; Craft.FactionId = Ship->Systems->FactionId;
            Craft.HP = Profile.Hull; Craft.Shield = Profile.Shield; Craft.Evasion = Profile.Evasion;
            Craft.Position = HangarPosition(Hangar);
        }
    }
    if (bAny)
    {
        auto& State = Carriers.AddDefaulted_GetRef(); State.Carrier = Ship;
        State.LastFaction = Ship->Systems->FactionId;
    }
    return bAny;
}

bool UOVCarrierSubsystem::IsAirborneState(EOVAircraftState State)
{
    return State != EOVAircraftState::Stored && State != EOVAircraftState::Wrecked;
}

bool UOVCarrierSubsystem::IsHangarOperational(const FOVHangarState& Hangar) const
{
    auto* Ship = Hangar.Carrier.Get();
    if (!LivingShip(Ship)) return false;
    const int32 I = Ship->Systems->Layout->GetSlotIndex(Hangar.SlotId);
    if (!Ship->Systems->Slots.IsValidIndex(I)) return false;
    const auto& Def = Ship->Systems->Layout->Slots[I];
    const auto& State = Ship->Systems->Slots[I];
    return Def.Facility == EOVFacilityType::Hangar && State.HP > 0 && State.Crew > 0 && Def.RequiredCrew > 0
        && (State.OccupyingFaction < 0 || State.OccupyingFaction == Ship->Systems->FactionId);
}

bool UOVCarrierSubsystem::IsHangarDestroyed(const FOVHangarState& Hangar) const
{
    auto* Ship = Hangar.Carrier.Get();
    if (!IsValid(Ship) || !Ship->Systems || Ship->Systems->bWrecked) return true;
    const int32 I = Ship->Systems->Layout ? Ship->Systems->Layout->GetSlotIndex(Hangar.SlotId) : INDEX_NONE;
    return !Ship->Systems->Slots.IsValidIndex(I) || Ship->Systems->Slots[I].HP <= 0;
}

bool UOVCarrierSubsystem::HasControl(AOVShip* Ship) const
{
    const auto* Combat = GetWorld()->GetSubsystem<UOVCombatSubsystem>();
    return LivingShip(Ship) && !Combat->IsDeparted(Ship)
        && Ship->Systems->GetEfficiency(EOVFacilityType::FighterControl) > 0;
}

FVector UOVCarrierSubsystem::HangarPosition(const FOVHangarState& Hangar) const
{
    auto* Ship = Hangar.Carrier.Get();
    if (!IsValid(Ship) || !Ship->Systems || !Ship->Systems->Layout) return FVector::ZeroVector;
    const auto* Layout = Ship->Systems->Layout.Get();
    const int32 I = Layout->GetSlotIndex(Hangar.SlotId);
    const FVector Local = Layout->Slots.IsValidIndex(I) ? Layout->Slots[I].LocalCenter : FVector::ZeroVector;
    // Distinct doors outside the upper hull; placeholder geometry, not a new physical docking model.
    return Ship->GetActorTransform().TransformPosition(FVector(Local.X, Local.Y, Layout->LocalHalfExtent.Z + 40));
}

int32 UOVCarrierSubsystem::Launch(AOVShip* Ship)
{
    auto* State = Carriers.FindByPredicate([Ship](const FOVCarrierState& S) { return S.Carrier == Ship; });
    if (!State || !HasControl(Ship) || Ship->bFTLCharging) return 0;
    State->bRecallOrdered = State->bAbandonOutstanding = false;
    int32 Count = 0;
    for (auto& Craft : Aircraft)
    {
        if (Craft.Carrier != Ship || Craft.State != EOVAircraftState::Stored) continue;
        const auto* Hangar = Hangars.FindByPredicate([&Craft](const FOVHangarState& H) { return H.HangarId == Craft.HangarId; });
        if (!Hangar || Hangar->Mode != EOVHangarMode::Aircraft || !IsHangarOperational(*Hangar)) continue;
        Craft.State = EOVAircraftState::Sortie; Craft.Leg = EOVAircraftLeg::Ingress;
        Craft.Maneuver = EOVAircraftManeuver::Approach; Craft.StrikeTarget.Reset();
        Craft.InterceptTargetId = 0; Craft.DefensiveRemaining = Craft.RejoinRemaining = 0;
        Craft.AttackDelay = ((Craft.AircraftId - 1) % FMath::Max(1, ManeuverProfile.SectionSize)) * ManeuverProfile.SectionDelay;
        Craft.FactionId = Ship->Systems->FactionId;
        const double Angle = PatternOffset + Craft.AircraftId * 2.399963229728653;
        const FVector Offset(400 * FMath::Cos(Angle), 400 * FMath::Sin(Angle), 120 * (Craft.AircraftId % 5));
        Craft.Position = HangarPosition(*Hangar) + Ship->GetActorTransform().TransformVectorNoScale(Offset);
        Craft.Direction = Ship->GetActorForwardVector();
        // HP/shield/cooldown are retained. A new sortie is not a newly produced aircraft.
        ++Count;
    }
    return Count;
}

void UOVCarrierSubsystem::Recall(AOVShip* Ship)
{
    auto* State = Carriers.FindByPredicate([Ship](const FOVCarrierState& S) { return S.Carrier == Ship; });
    if (!State) return;
    State->bRecallOrdered = true;
    for (auto& Craft : Aircraft)
    {
        if (Craft.Carrier != Ship || !IsAirborneState(Craft.State)) continue;
        if (Craft.State == EOVAircraftState::ControlLost) Craft.ResumeState = EOVAircraftState::Returning;
        else if (Craft.State != EOVAircraftState::DockWaiting) Craft.State = EOVAircraftState::Returning;
    }
}

int32 UOVCarrierSubsystem::GetHangarCapacity(int32 HangarId) const
{
    const auto* H = Hangars.FindByPredicate([HangarId](const FOVHangarState& V) { return V.HangarId == HangarId; });
    return H ? (H->Mode == EOVHangarMode::Aircraft ? 20 + H->ExtraAircraftCapacity : 1) : 0;
}

void UOVCarrierSubsystem::SetCapacityBonus(AOVShip* Ship, int32 ExtraPerHangar)
{
    for (auto& H : Hangars) if (H.Carrier == Ship) H.ExtraAircraftCapacity = FMath::Max(0, ExtraPerHangar);
    // No aircraft creation, healing, cooldown reset, or deletion when capacity changes.
}

bool UOVCarrierSubsystem::SetHangarMode(int32 HangarId, EOVHangarMode Mode)
{
    auto* H = Hangars.FindByPredicate([HangarId](const FOVHangarState& V) { return V.HangarId == HangarId; });
    if (!H) return false;
    if (H->Mode == Mode) return true;
    if (H->StoredSmallShip.IsValid() || Aircraft.ContainsByPredicate([HangarId](const FOVAircraftRecord& C)
        { return C.HangarId == HangarId && C.State != EOVAircraftState::Wrecked; })) return false;
    H->Mode = Mode; return true;
}

int32 UOVCarrierSubsystem::CountAircraft(AOVShip* Ship, EOVAircraftState State) const
{
    int32 Result = 0;
    for (const auto& C : Aircraft) if (C.Carrier == Ship && C.State == State) ++Result;
    return Result;
}

bool UOVCarrierSubsystem::HasOutstandingAircraft(AOVShip* Ship) const
{
    return Aircraft.ContainsByPredicate([Ship](const FOVAircraftRecord& C) { return C.Carrier == Ship && IsAirborneState(C.State); });
}

bool UOVCarrierSubsystem::CanDepart(AOVShip* Ship) const
{
    const auto* C = Carriers.FindByPredicate([Ship](const FOVCarrierState& S) { return S.Carrier == Ship; });
    return !C || C->bAbandonOutstanding || !HasOutstandingAircraft(Ship);
}

void UOVCarrierSubsystem::AbandonOutstanding(AOVShip* Ship)
{
    if (auto* C = Carriers.FindByPredicate([Ship](const FOVCarrierState& S) { return S.Carrier == Ship; }))
        C->bAbandonOutstanding = true;
    // Permission to leave only. Neither complete charging nor destroy/recreate stranded craft.
}

int32 UOVCarrierSubsystem::DebugSupplyRepair(AOVShip* Ship)
{
    int32 Count = 0;
    for (auto& Craft : Aircraft)
    {
        if (Craft.Carrier != Ship || Craft.State != EOVAircraftState::Stored) continue;
        const auto* H = Hangars.FindByPredicate([&Craft](const FOVHangarState& V) { return V.HangarId == Craft.HangarId; });
        if (!H || !IsHangarOperational(*H)) continue;
        if (Craft.HP >= Profile.Hull && Craft.Shield >= Profile.Shield) continue;
        Craft.HP = Profile.Hull; Craft.Shield = Profile.Shield; ++Count;
    }
    DeveloperRepairCount += Count; return Count;
}

FString UOVCarrierSubsystem::GetCarrierStatus(AOVShip* Ship) const
{
    if (!Carriers.ContainsByPredicate([Ship](const FOVCarrierState& S) { return S.Carrier == Ship; })) return TEXT("함재기 운용 없음");
    if (!LivingShip(Ship)) return TEXT("모함 상실 · 외부 기체 관제 정지");
    if (!HasControl(Ship)) return TEXT("관제 상실 · 시설 복구 대기");
    if (Ship->bFTLCharging) return CanDepart(Ship) ? TEXT("회수 경계 통과 · FTL 충전 완료 별도") : TEXT("긴급 회수 대기");
    return FString::Printf(TEXT("보관 %d · 출격 %d · 귀환 %d · 대기 %d · 손실 %d"),
        CountAircraft(Ship, EOVAircraftState::Stored), CountAircraft(Ship, EOVAircraftState::Sortie),
        CountAircraft(Ship, EOVAircraftState::Returning), CountAircraft(Ship, EOVAircraftState::DockWaiting),
        CountAircraft(Ship, EOVAircraftState::Wrecked));
}

void UOVCarrierSubsystem::DestroyAircraft(FOVAircraftRecord& Craft)
{
    if (Craft.State == EOVAircraftState::Wrecked) return;
    Craft.HP = Craft.Shield = 0; Craft.State = EOVAircraftState::Wrecked; ++LossCount;
}

bool UOVCarrierSubsystem::ApplyAircraftDamage(int32 AircraftId, float Damage)
{
    auto* Craft = Aircraft.FindByPredicate([AircraftId](const FOVAircraftRecord& C) { return C.AircraftId == AircraftId; });
    if (!Craft || !IsAirborneState(Craft->State) || !FMath::IsFinite(Damage) || Damage <= 0) return false;
    const float Absorbed = FMath::Min(Craft->Shield, Damage);
    Craft->Shield -= Absorbed; Craft->HP = FMath::Max(0.f, Craft->HP - (Damage - Absorbed));
    if (Craft->HP <= 0) DestroyAircraft(*Craft);
    return true;
}

FOVHangarState* UOVCarrierSubsystem::FindDock(FOVAircraftRecord& Craft)
{
    // R25 trial: same-carrier operational hangars only, stable ID order. No teleport to another carrier.
    for (auto& H : Hangars)
    {
        if (H.Carrier != Craft.Carrier || H.Mode != EOVHangarMode::Aircraft || !IsHangarOperational(H)) continue;
        int32 Reserved = 0;
        for (const auto& C : Aircraft)
            if (C.AircraftId != Craft.AircraftId && C.HangarId == H.HangarId
                && (C.State == EOVAircraftState::Stored || C.State == EOVAircraftState::DockWaiting)) ++Reserved;
        if (Reserved < GetHangarCapacity(H.HangarId)) return &H;
    }
    return nullptr;
}

void UOVCarrierSubsystem::Simulate(float GameDelta)
{
    if (!FMath::IsFinite(GameDelta) || GameDelta <= 0 || Carriers.IsEmpty()) return;
    Accumulator += GameDelta;
    constexpr double Fixed = 1.0 / 120.0;
    while (Accumulator + 1.e-9 >= Fixed)
    {
        Step(float(Fixed)); Accumulator -= Fixed;
    }
}

void UOVCarrierSubsystem::Step(float Delta)
{
    GameTime += Delta;
    for (auto& State : Carriers)
    {
        auto* Ship = State.Carrier.Get();
        // Ownership is independent of operational crew/control. A captured, uncrewed
        // carrier transfers its surviving inventory immediately but cannot make it fly.
        if (!IsValid(Ship) || !Ship->Systems || Ship->Systems->bWrecked) continue;
        if (Ship->Systems->FactionId != State.LastFaction)
        {
            State.LastFaction = Ship->Systems->FactionId;
            State.bAbandonOutstanding = false;
            for (auto& Craft : Aircraft)
                if (Craft.Carrier == Ship && Craft.State != EOVAircraftState::Wrecked)
                {
                    Craft.FactionId = State.LastFaction; Craft.StrikeTarget.Reset(); Craft.InterceptTargetId = 0;
                    Craft.Maneuver = EOVAircraftManeuver::Approach; Craft.Leg = EOVAircraftLeg::Ingress;
                    Craft.DefensiveRemaining = 0; Craft.RejoinRemaining = 0;
                }
            // Launched Bolts intentionally retain their immutable previous faction.
        }
        if (!LivingShip(Ship)) continue;
        if (Ship->bFTLCharging && !State.bWasFTLCharging) Recall(Ship);
        State.bWasFTLCharging = Ship->bFTLCharging;
        if (!State.bRecallOrdered && !Ship->bFTLCharging && LivingShip(Ship->AttackTarget)
            && Ship->AttackTarget->Systems->FactionId != Ship->Systems->FactionId) Launch(Ship);
    }
    // Crew/ownership/interior protection have already advanced for this fixed step.
    // No projectile resolves until all aircraft have moved, so this is a shared current-step observation.
    RebuildStrikeCandidates();
    // All craft expose the same start-of-step positions to relative projectile sweeps.
    for (auto& Craft : Aircraft) Craft.PreviousPosition = Craft.Position;
    for (auto& Craft : Aircraft)
    {
        AdvanceAircraft(Craft, Delta);
        Craft.Velocity = (Craft.Position - Craft.PreviousPosition) / Delta;
    }
    AdvanceBolts(Delta);
}

void UOVCarrierSubsystem::AdvanceAircraft(FOVAircraftRecord& Craft, float Delta)
{
    CSV_SCOPED_TIMING_STAT_EXCLUSIVE(OV_Carrier);
    TRACE_CPUPROFILER_EVENT_SCOPE(OV_Carrier);
    if (Craft.State == EOVAircraftState::Wrecked) return;
    auto* Ship = Craft.Carrier.Get();
    auto* Hangar = Hangars.FindByPredicate([&Craft](const FOVHangarState& H) { return H.HangarId == Craft.HangarId; });
    if (Craft.State == EOVAircraftState::Stored)
    {
        if (!Hangar || IsHangarDestroyed(*Hangar)) DestroyAircraft(Craft);
        else Craft.Position = HangarPosition(*Hangar);
        return;
    }
    if (!HasControl(Ship))
    {
        if (Craft.State != EOVAircraftState::ControlLost) { Craft.ResumeState = Craft.State; Craft.State = EOVAircraftState::ControlLost; }
        return;
    }
    if (Craft.State == EOVAircraftState::ControlLost) Craft.State = Craft.ResumeState;
    Craft.Cooldown = FMath::Max(0.f, Craft.Cooldown - Delta);
    if (Craft.State == EOVAircraftState::Returning || Craft.State == EOVAircraftState::DockWaiting)
    {
        FOVHangarState* Dock = FindDock(Craft);
        if (!Dock) { Craft.State = EOVAircraftState::DockWaiting; Craft.DockRemaining = Profile.DockSeconds; return; }
        const FVector Point = HangarPosition(*Dock);
        const FVector V = Point - Craft.Position;
        if (V.Size() <= FMath::Max(100.f, Profile.ReturnSpeed * Delta))
        {
            if (Craft.State != EOVAircraftState::DockWaiting || Craft.HangarId != Dock->HangarId) Craft.DockRemaining = Profile.DockSeconds;
            Craft.HangarId = Dock->HangarId; Craft.Position = Point; Craft.State = EOVAircraftState::DockWaiting;
            Craft.DockRemaining = FMath::Max(0.f, Craft.DockRemaining - Delta);
            if (Craft.DockRemaining <= 0) Craft.State = EOVAircraftState::Stored;
        }
        else
        {
            Craft.State = EOVAircraftState::Returning; Craft.Direction = V.GetSafeNormal();
            Craft.Position += Craft.Direction * FMath::Min(double(Profile.ReturnSpeed * Delta), V.Size());
        }
        return;
    }

    AOVShip* Target = SelectStrikeTarget(Craft);
    if (Target != Craft.StrikeTarget.Get())
    {
        Craft.StrikeTarget = Target; Craft.InterceptTargetId = 0;
        Craft.DefensiveRemaining = 0; Craft.Maneuver = EOVAircraftManeuver::Approach;
        Craft.Leg = EOVAircraftLeg::Ingress;
    }
    Craft.AttackDelay = FMath::Max(0.f, Craft.AttackDelay - Delta);
    const auto* AirTarget = SelectAirTarget(Craft, Target, Delta);
    if (AirTarget) AdvanceDogfight(Craft, *AirTarget, Delta);
    else if (Target) AdvanceStrike(Craft, Target, Delta);
    else
    {
        Craft.Maneuver = EOVAircraftManeuver::Patrol;
        const double Angle = PatternOffset + Craft.AircraftId * 2.399963229728653 + GameTime * .45;
        Craft.StrikeCenter = Ship->GetActorLocation();
        Craft.AllowedRadius = 0;
        FlyToward(Craft, Craft.StrikeCenter + FVector(FMath::Cos(Angle), FMath::Sin(Angle), .3) * Profile.AttackRange * .6, Delta, false);
    }
}

void UOVCarrierSubsystem::RebuildStrikeCandidates()
{
    CSV_SCOPED_TIMING_STAT_EXCLUSIVE(OV_CarrierTargeting);
    TRACE_CPUPROFILER_EVENT_SCOPE(OV_CarrierSharedShipTargets);
    const auto* Combat = GetWorld()->GetSubsystem<UOVCombatSubsystem>();
    StrikeCandidateCache.SetNum(Carriers.Num());
    for (int32 I = 0; I < Carriers.Num(); ++I)
    {
        auto& Entry = StrikeCandidateCache[I];
        AOVShip* Source = Carriers[I].Carrier.Get();
        Entry.Source = Source; Entry.Candidates.Reset();
        if (!LivingShip(Source)) continue;
        for (const auto& State : Combat->Ships)
        {
            AOVShip* Candidate = State.Ship.Get();
            if (!LivingShip(Candidate) || State.bDeparted || Candidate->Systems->FactionId == Source->Systems->FactionId
                || Combat->IsTargetProtected(Source, Candidate) || !Combat->GetDetection(Source, Candidate).bDetected) continue;
            Entry.Candidates.Add(Candidate);
        }
    }
}

AOVShip* UOVCarrierSubsystem::SelectStrikeTarget(const FOVAircraftRecord& Craft) const
{
    CSV_SCOPED_TIMING_STAT_EXCLUSIVE(OV_CarrierTargeting);
    TRACE_CPUPROFILER_EVENT_SCOPE(OV_CarrierSelectShipTarget);
    const auto* Entry = StrikeCandidateCache.FindByPredicate([&Craft](const FStrikeCandidateCache& C) { return C.Source == Craft.Carrier; });
    if (!Entry) return nullptr;
    auto Eligible = [&](AOVShip* Candidate) { return Candidate && Entry->Candidates.Contains(Candidate); };
    // The carrier's explicit target wins; otherwise keep a valid mission, not a new nearest ship each frame.
    if (Eligible(Craft.Carrier->AttackTarget)) return Craft.Carrier->AttackTarget;
    if (Eligible(Craft.StrikeTarget.Get())) return Craft.StrikeTarget.Get();
    AOVShip* Best = nullptr; double Distance = TNumericLimits<double>::Max();
    // Candidate order is unchanged Combat->Ships order, preserving equal-distance ties.
    for (const auto& Candidate : Entry->Candidates)
        if (Candidate.IsValid())
        {
            const double D = FVector::DistSquared(Craft.Position, Candidate->GetActorLocation());
            if (D < Distance) { Best = Candidate.Get(); Distance = D; }
        }
    return Best;
}

const FOVAircraftRecord* UOVCarrierSubsystem::SelectAirTarget(FOVAircraftRecord& Craft, AOVShip* StrikeTarget, float Delta)
{
    CSV_SCOPED_TIMING_STAT_EXCLUSIVE(OV_CarrierTargeting);
    TRACE_CPUPROFILER_EVENT_SCOPE(OV_CarrierTargeting);
    Craft.RejoinRemaining = FMath::Max(0.f, Craft.RejoinRemaining - Delta);
    Craft.DefensiveRemaining = FMath::Max(0.f, Craft.DefensiveRemaining - Delta);
    const double HullRadius = StrikeTarget ? (StrikeTarget->Systems->Layout->LocalHalfExtent * StrikeTarget->GetActorScale3D()).Size() : 0;
    const double TurnRadius = Profile.Speed / FMath::DegreesToRadians(FMath::Max(1.f, ManeuverProfile.TurnDegreesPerSecond));
    const double Leash = HullRadius + Profile.AttackRange + TurnRadius * 2;
    const bool HadIntercept = Craft.InterceptTargetId != 0;
    const FOVAircraftRecord* Best = nullptr;
    double Distance = FMath::Square(double(Profile.AttackRange * (StrikeTarget ? ManeuverProfile.DefensiveRangeFraction : 1.5f)));
    if (!StrikeTarget || Craft.RejoinRemaining <= 0)
    {
        for (const auto& Other : Aircraft)
        {
            if (Other.AircraftId == Craft.AircraftId || Other.FactionId == Craft.FactionId || !IsAirborneState(Other.State)) continue;
            if (StrikeTarget && (FVector::DistSquared(Other.Position, StrikeTarget->GetActorLocation()) > Leash * Leash
                || FVector::DistSquared(Craft.Position, StrikeTarget->GetActorLocation()) > Leash * Leash)) continue;
            const double D = FVector::DistSquared(Craft.Position, Other.Position);
            if (Other.AircraftId == Craft.InterceptTargetId && (!StrikeTarget || Craft.DefensiveRemaining > 0)) return &Other;
            if (!HadIntercept && D < Distance) { Best = &Other; Distance = D; }
        }
    }
    if (HadIntercept)
    {
        Craft.InterceptTargetId = 0;
        if (StrikeTarget)
        {
            Craft.RejoinRemaining = ManeuverProfile.RejoinSeconds;
            Craft.Maneuver = EOVAircraftManeuver::Regroup; Craft.Leg = EOVAircraftLeg::Regroup;
        }
        return nullptr;
    }
    if (Best)
    {
        Craft.InterceptTargetId = Best->AircraftId; Craft.DefensiveRemaining = ManeuverProfile.DefensiveSeconds;
    }
    return Best;
}

void UOVCarrierSubsystem::FlyToward(FOVAircraftRecord& Craft, FVector Point, float Delta, bool bFiniteTurn)
{
    Craft.Waypoint = Point;
    const FVector V = Point - Craft.Position;
    FVector Desired = V.GetSafeNormal();
    if (Desired.IsNearlyZero()) return;
    if (bFiniteTurn && Craft.StrikeTarget.IsValid())
    {
        const auto* Target = Craft.StrikeTarget.Get();
        const double HullRadius = (Target->Systems->Layout->LocalHalfExtent * Target->GetActorScale3D()).Size();
        const FVector Radial = Craft.Position - Target->GetActorLocation();
        const double Guard = HullRadius + ManeuverProfile.HullClearance + Craft.TurnRadius;
        if (Radial.Size() < Guard && FVector::DotProduct(Desired, Radial) < 0)
        {
            FVector Tangent = FVector::CrossProduct(FVector::UpVector, Radial.GetSafeNormal()).GetSafeNormal();
            if (Craft.AircraftId % 2) Tangent *= -1;
            Desired = (Tangent + Radial.GetSafeNormal() * .9).GetSafeNormal();
        }
    }
    const FVector Current = Craft.Direction.GetSafeNormal();
    if (bFiniteTurn && !Current.IsNearlyZero())
    {
        const double Angle = FMath::Acos(FMath::Clamp(FVector::DotProduct(Current, Desired), -1., 1.));
        const double Limit = FMath::DegreesToRadians(FMath::Max(1.f, ManeuverProfile.TurnDegreesPerSecond)) * Delta;
        Craft.Direction = Angle > Limit ? FQuat::Slerp(FQuat::Identity, FQuat::FindBetweenNormals(Current, Desired), Limit / Angle).RotateVector(Current).GetSafeNormal() : Desired;
    }
    else Craft.Direction = Desired;
    Craft.Position += Craft.Direction * FMath::Min(double(Profile.Speed * Delta), V.Size());
}

void UOVCarrierSubsystem::AdvanceStrike(FOVAircraftRecord& Craft, AOVShip* Target, float Delta)
{
    Craft.StrikeCenter = Target->GetActorLocation();
    Craft.TurnRadius = Profile.Speed / FMath::DegreesToRadians(FMath::Max(1.f, ManeuverProfile.TurnDegreesPerSecond));
    const double HullRadius = (Target->Systems->Layout->LocalHalfExtent * Target->GetActorScale3D()).Size();
    const double AttackRadius = FMath::Max(HullRadius + ManeuverProfile.HullClearance + Craft.TurnRadius * .45,
        double(Profile.AttackRange * FMath::Clamp(ManeuverProfile.AttackRangeFraction, .1f, .9f)));
    const int32 Section = (Craft.AircraftId - 1) / FMath::Max(1, ManeuverProfile.SectionSize);
    const int32 Lane = (Craft.AircraftId - 1) % FMath::Max(1, ManeuverProfile.SectionSize);
    const double Angle = PatternOffset + Section * 2.399963229728653 + Lane * .13;
    const double Side = Section % 2 == 0 ? 1. : -1.;
    auto Offset = [&](double Heading, double Radius)
    {
        return Target->GetActorRotation().RotateVector(FVector(FMath::Cos(Heading), FMath::Sin(Heading), Side * (.18 + Lane * .04)).GetSafeNormal() * Radius);
    };
    const double EgressRadius = AttackRadius + Profile.Speed * FMath::Max(.05f, ManeuverProfile.EgressSeconds);
    const double RegroupRadius = AttackRadius + Craft.TurnRadius * FMath::Max(.5f, ManeuverProfile.RegroupTurnRadii);
    Craft.ApproachPoint = Craft.StrikeCenter + Offset(Angle, AttackRadius);
    Craft.EgressPoint = Craft.StrikeCenter + Offset(Angle + Side * .5, EgressRadius);
    Craft.RegroupPoint = Craft.StrikeCenter + Offset(Angle + Side * 1.1, RegroupRadius);
    // Includes turn/exit space; it is a steering guard, never a teleport/clamp of position.
    Craft.AllowedRadius = FMath::Max(EgressRadius, RegroupRadius) + 2 * Craft.TurnRadius + Profile.Speed * .1;
    if (Craft.Maneuver == EOVAircraftManeuver::Dogfight || Craft.Maneuver == EOVAircraftManeuver::Patrol)
        Craft.Maneuver = EOVAircraftManeuver::Regroup;
    const double CaptureDistance = FMath::Max(double(Profile.Speed * Delta + 120), double(Craft.TurnRadius * 1.15));
    if (Craft.Maneuver == EOVAircraftManeuver::Approach && FVector::DistSquared(Craft.Position, Craft.StrikeCenter) <= FMath::Square(double(Profile.AttackRange)))
        Craft.Maneuver = EOVAircraftManeuver::AttackRun;
    if (Craft.Maneuver == EOVAircraftManeuver::AttackRun)
    {
        Craft.Leg = EOVAircraftLeg::Ingress;
        if (Craft.Cooldown <= 0 && Craft.AttackDelay <= 0 && FVector::DistSquared(Craft.Position, Craft.StrikeCenter) <= FMath::Square(double(Profile.AttackRange)))
        {
            const FVector Aim = LeadPoint(Craft.Position, Craft.StrikeCenter, Target->Velocity, Profile.BoltSpeed, Profile.BoltLifetime);
            if (Fire(Craft, Aim, FName(*Target->ShipName)))
            {
                ++Craft.AttackPasses; Craft.LegRemaining = ManeuverProfile.EgressSeconds;
                Craft.Maneuver = EOVAircraftManeuver::ShortEgress; Craft.Leg = EOVAircraftLeg::Egress;
            }
        }
    }
    if (Craft.Maneuver == EOVAircraftManeuver::ShortEgress)
    {
        Craft.LegRemaining = FMath::Max(0.f, Craft.LegRemaining - Delta);
        if (Craft.LegRemaining <= 0 || FVector::DistSquared(Craft.Position, Craft.EgressPoint) < CaptureDistance * CaptureDistance)
        { Craft.Maneuver = EOVAircraftManeuver::Regroup; Craft.Leg = EOVAircraftLeg::Regroup; }
    }
    if (Craft.Maneuver == EOVAircraftManeuver::Regroup && FVector::DistSquared(Craft.Position, Craft.RegroupPoint) < CaptureDistance * CaptureDistance)
    { Craft.Maneuver = EOVAircraftManeuver::Approach; Craft.Leg = EOVAircraftLeg::Ingress; }
    FVector Point = Craft.Maneuver == EOVAircraftManeuver::ShortEgress ? Craft.EgressPoint
        : Craft.Maneuver == EOVAircraftManeuver::Regroup ? Craft.RegroupPoint : Craft.ApproachPoint;
    if (FVector::DistSquared(Craft.Position, Craft.StrikeCenter) > FMath::Square(double(Craft.AllowedRadius)))
    {
        Craft.Maneuver = EOVAircraftManeuver::Approach; Craft.Leg = EOVAircraftLeg::Ingress;
        Point = Craft.ApproachPoint;
    }
    FlyToward(Craft, Point, Delta, true);
}

void UOVCarrierSubsystem::AdvanceDogfight(FOVAircraftRecord& Craft, const FOVAircraftRecord& Target, float Delta)
{
    // Independent pursuit: a ship mission has a bounded defensive excursion; pure dogfight has no ship leash.
    Craft.Maneuver = EOVAircraftManeuver::Dogfight;
    const FVector Aim = LeadPoint(Craft.Position, Target.Position, Target.Velocity, Profile.BoltSpeed, Profile.BoltLifetime);
    if (Craft.Cooldown <= 0 && FVector::DistSquared(Craft.Position, Target.Position) <= FMath::Square(double(Profile.AttackRange)))
        Fire(Craft, Aim, FName(*FString::Printf(TEXT("Aircraft_%d"), Target.AircraftId)));
    const double Side = Craft.AircraftId % 2 ? -1 : 1;
    const FVector Offset(0, Side * Profile.CollisionRadius * 3, ((Craft.AircraftId % 3)-1) * Profile.CollisionRadius * 2);
    FlyToward(Craft, Aim + Offset, Delta, false);
}

bool UOVCarrierSubsystem::Fire(FOVAircraftRecord& Craft, FVector AimPoint, FName TargetId)
{
    CSV_SCOPED_TIMING_STAT_EXCLUSIVE(OV_CarrierFriendlyFire);
    TRACE_CPUPROFILER_EVENT_SCOPE(OV_CarrierFriendlyFire);
    // Block a known friendly line before launch. Later crossing/capture never rewrites a bolt.
    const auto* Combat = GetWorld()->GetSubsystem<UOVCombatSubsystem>();
    for (const auto& S : Combat->Ships)
    {
        if (!LivingShip(S.Ship) || S.bDeparted || S.Ship.Get() == Craft.Carrier.Get() || S.Ship->Systems->FactionId != Craft.FactionId) continue;
        float T; FVector N;
        if (UOVCombatSubsystem::SegmentBox(Craft.Position, AimPoint, S.Ship->GetActorTransform(), S.Ship->Systems->Layout->LocalHalfExtent, T, N)) return false;
    }
    for (const auto& Other : Aircraft)
    {
        if (Other.AircraftId == Craft.AircraftId || Other.FactionId != Craft.FactionId || !IsAirborneState(Other.State)) continue;
        float T;
        if (SegmentSphere(Craft.Position, AimPoint, Other.Position, Profile.CollisionRadius, T)) return false;
    }
    auto& Bolt = Bolts.AddDefaulted_GetRef();
    Bolt.BoltId = NextBoltId++; Bolt.AircraftId = Craft.AircraftId; Bolt.FactionId = Craft.FactionId;
    Bolt.SourceCarrier = Craft.Carrier;
    Bolt.SourceId = FName(*FString::Printf(TEXT("Aircraft_%d"), Craft.AircraftId)); Bolt.ProfileId = Profile.ProfileId;
    Bolt.TargetId = TargetId;
    Bolt.Origin = Bolt.Position = Craft.Position; Bolt.Direction = (AimPoint - Craft.Position).GetSafeNormal();
    Bolt.Damage = Profile.Damage; Bolt.Speed = Profile.BoltSpeed; Bolt.Lifetime = Profile.BoltLifetime;
    Craft.Cooldown = FMath::Max(.01f, Profile.Period); ++Craft.ShotsFired; ++ShotCount;
    if (auto* Ship = Craft.Carrier.Get()) Ship->Systems->NotifyCombatParticipation();
    return true;
}

void UOVCarrierSubsystem::AdvanceBolts(float Delta)
{
    CSV_SCOPED_TIMING_STAT_EXCLUSIVE(OV_CarrierCollision);
    TRACE_CPUPROFILER_EVENT_SCOPE(OV_CarrierCollision);
    const auto* Combat = GetWorld()->GetSubsystem<UOVCombatSubsystem>();
    for (auto& Bolt : Bolts)
    {
        if (Bolt.bResolved) continue;
        Bolt.Age += Delta;
        const FVector End = Bolt.Position + Bolt.Direction * Bolt.Speed * Delta;
        float First = 2;
        AOVShip* HitShip = nullptr;
        int32 HitAircraft = INDEX_NONE;
        FVector HitNormal = -Bolt.Direction;
        for (const auto& S : Combat->Ships)
        {
            if (!LivingShip(S.Ship) || S.bDeparted || S.Ship.Get() == Bolt.SourceCarrier.Get()) continue;
            float T; FVector N;
            if (UOVCombatSubsystem::SegmentBox(Bolt.Position, End, S.Ship->GetActorTransform(), S.Ship->Systems->Layout->LocalHalfExtent, T, N) && T < First)
            { First = T; HitShip = S.Ship; HitAircraft = INDEX_NONE; HitNormal = N; }
        }
        for (const auto& Other : Aircraft)
        {
            if (Other.AircraftId == Bolt.AircraftId || !IsAirborneState(Other.State)) continue;
            float T;
            if (SegmentSphere(Bolt.Position - Other.PreviousPosition, End - Other.Position, FVector::ZeroVector, Profile.CollisionRadius, T) && T < First)
            { First = T; HitAircraft = Other.AircraftId; HitShip = nullptr; }
        }
        if (First <= 1)
        {
            Bolt.Position = FMath::Lerp(Bolt.Position, End, First); Bolt.bResolved = Bolt.bHit = true;
            Bolt.ResolvedAt = GameTime; ++HitCount;
            if (HitShip)
            {
                Bolt.HitId = FName(*HitShip->ShipName);
                auto* Systems = HitShip->Systems.Get();
                Systems->IncomingSourceId = Bolt.SourceId; Systems->IncomingWeaponId = Bolt.ProfileId;
                Systems->IncomingFactionId = Bolt.FactionId;
                Systems->ApplyWorldHit(Bolt.Position, HitNormal, Bolt.Damage, EOVDamageCause::External, true);
            }
            else
            {
                Bolt.HitId = FName(*FString::Printf(TEXT("Aircraft_%d"), HitAircraft));
                ApplyAircraftDamage(HitAircraft, Bolt.Damage);
            }
        }
        else
        {
            Bolt.Position = End;
            if (Bolt.Age >= Bolt.Lifetime) { Bolt.bResolved = true; Bolt.ResolvedAt = GameTime; }
        }
        if (Bolt.bResolved) RecentEvents.Add(Bolt);
    }
    Bolts.RemoveAll([](const FOVAircraftBolt& B) { return B.bResolved; });
    if (RecentEvents.Num() > 256) RecentEvents.RemoveAt(0, RecentEvents.Num() - 256);
}
