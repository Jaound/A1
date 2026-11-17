#include "OVShipSystemsComponent.h"
#include "GameFramework/Actor.h"
#include "OVShip.h"
#include "OVCombatSubsystem.h"
#include "OVBattleNotices.h"
#include "Engine/World.h"

namespace
{
bool Unmanned(EOVFacilityType Type)
{
    return Type == EOVFacilityType::Core || Type == EOVFacilityType::Security;
}
bool FrontOrRear(EOVArmorFace Face)
{
    return Face == EOVArmorFace::Front || Face == EOVArmorFace::Rear;
}
}

UOVShipSystemsComponent::UOVShipSystemsComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UOVShipSystemsComponent::Initialize(UShipLayoutDataAsset* InLayout)
{
    Layout = InLayout;
    if (Layout)
    {
        TArray<FString> Errors;
        if (!Layout->ValidateLayout(Errors))
        {
            UE_LOG(LogTemp, Warning, TEXT("OVLayout rejected: %s"), *FString::Join(Errors, TEXT("; ")));
            Layout = nullptr;
        }
    }
    ResetState();
}

void UOVShipSystemsComponent::ResetState()
{
    Slots.Reset(); ArmorSections.Reset(); WeaponMountStates.Reset();
    InteriorObjectives.Reset(); InteriorDeveloperInputs.Reset(); RuntimeInteriorObjectives.Reset();
    bBridgeCaptured = bCapturedBridgeDestroyed = false; CaptureCount = 0;
    bWrecked = false; bNeutral = false; bShieldActive = false; bShieldManualOff = false;
    WreckEventCount = 0; LastExplosionCount = 0; LastDamageIndex = INDEX_NONE;
    LastDamageSlot = NAME_None; LastArmorSection = NAME_None; LastWeaponMount = NAME_None;
    LastWorldHit = FVector::ZeroVector; LastLocalHit = FVector::ZeroVector; LastLocalNormal = FVector::ZeroVector;
    HitEventId = 0; LastHullDamage = 0; LastDamageSource = NAME_None;
    ShieldRecoveryRemaining = 0; TimeSinceExternalHit = 0; Shield = 0;
    MaxHullHP = Layout ? FMath::Max(0.f, Layout->MaxHullHP) : 0;
    HullHP = MaxHullHP;
    if (Layout)
    {
        for (const FOVSlotDefinition& Def : Layout->Slots)
        {
            FOVFacilityState State;
            State.SlotId = Def.SlotId;
            State.HP = Def.Facility == EOVFacilityType::None ? 0 : FMath::Max(0.f, Def.MaxFacilityHP);
            State.Crew = FMath::Max(0.f, Def.RequiredCrew);
            Slots.Add(State);
        }
        for (const FOVArmorDefinition& Def : Layout->ArmorSections)
        {
            FOVArmorSectionState State;
            State.ArmorSectionId = Def.ArmorId; State.HP = FMath::Max(0.f, Def.MaxArmor);
            ArmorSections.Add(State);
        }
        for (const FOVWeaponMountDefinition& Def : Layout->WeaponMounts)
        {
            FOVWeaponMountState State;
            State.WeaponMountId = Def.WeaponId; State.HP = FMath::Max(0.f, Def.MaxHP);
            WeaponMountStates.Add(State);
        }
    }
    RefreshDerivedState();
    Shield = MaxShield; bShieldActive = MaxShield > 0 && !bWrecked;
}

bool UOVShipSystemsComponent::IsOperational(int32 Index) const
{
    if (!Layout || !Slots.IsValidIndex(Index) || !Layout->Slots.IsValidIndex(Index)) return false;
    const FOVFacilityState& State = Slots[Index];
    const FOVSlotDefinition& Def = Layout->Slots[Index];
    return Def.Facility != EOVFacilityType::None && State.HP > 0
        && (State.OccupyingFaction < 0 || State.OccupyingFaction == FactionId)
        && (Unmanned(Def.Facility) || (Def.RequiredCrew > 0 && State.Crew > 0));
}

float UOVShipSystemsComponent::GetEfficiency(EOVFacilityType Type) const
{
    if (!Layout || bWrecked || Type == EOVFacilityType::None) return 0;
    float Highest = 0, Sum = 0;
    for (int32 Index = 0; Index < Slots.Num(); ++Index)
    {
        if (Layout->Slots[Index].Facility != Type || !IsOperational(Index)) continue;
        const FOVSlotDefinition& Def = Layout->Slots[Index];
        const float Ratio = Unmanned(Type) ? 1.f : FMath::Clamp(Slots[Index].Crew / Def.RequiredCrew, 0.f, 1.f);
        Sum += Ratio; Highest = FMath::Max(Highest, Ratio);
    }
    return 0.5f * (Sum + Highest);
}

bool UOVShipSystemsComponent::CanAct() const { return Layout && !bWrecked && !bNeutral; }
bool UOVShipSystemsComponent::CanMove() const { return CanAct() && GetEfficiency(EOVFacilityType::Engine) > 0; }
bool UOVShipSystemsComponent::CanCommand() const { return CanAct() && GetEfficiency(EOVFacilityType::Bridge) > 0; }
bool UOVShipSystemsComponent::CanFireCannons() const { return CanAct() && GetEfficiency(EOVFacilityType::WeaponControl) > 0 && GetEfficiency(EOVFacilityType::Ammo) > 0; }
bool UOVShipSystemsComponent::CanFireBeams() const { return CanAct() && GetEfficiency(EOVFacilityType::WeaponControl) > 0 && GetEfficiency(EOVFacilityType::Capacitor) > 0; }
bool UOVShipSystemsComponent::CanFireMissiles() const { return CanAct() && GetEfficiency(EOVFacilityType::WeaponControl) > 0 && GetEfficiency(EOVFacilityType::MissileStorage) > 0; }
bool UOVShipSystemsComponent::HasLifeSupport() const { return CanAct() && GetEfficiency(EOVFacilityType::LifeSupport) > 0; }
bool UOVShipSystemsComponent::HasRadar() const { return CanAct() && GetEfficiency(EOVFacilityType::Radar) > 0; }
bool UOVShipSystemsComponent::CanIntercept() const { return CanAct() && GetEfficiency(EOVFacilityType::Interception) > 0; }
bool UOVShipSystemsComponent::CanLaunchFighters() const { return CanUseHangar() && GetEfficiency(EOVFacilityType::FighterControl) > 0; }
bool UOVShipSystemsComponent::CanUseHangar() const { return CanAct() && GetEfficiency(EOVFacilityType::Hangar) > 0; }
bool UOVShipSystemsComponent::CanUseResources() const { return CanAct() && GetEfficiency(EOVFacilityType::ResourceStorage) > 0; }
float UOVShipSystemsComponent::GetRadarRange() const { return HasRadar() ? Layout->BaseRadarRange : 0; }
float UOVShipSystemsComponent::GetRadarAccuracyMultiplier() const { return HasRadar() ? 1.f + Tuning.RadarAccuracyBase * GetEfficiency(EOVFacilityType::Radar) : 1.f; }

void UOVShipSystemsComponent::SyncArmorDisplay()
{
    if (!Layout) return;
    for (FOVFacilityState& Slot : Slots)
    {
        float Minimum = TNumericLimits<float>::Max();
        for (int32 Index = 0; Index < ArmorSections.Num(); ++Index)
            if (Layout->ArmorSections[Index].InnerSlotIds.Contains(Slot.SlotId)) Minimum = FMath::Min(Minimum, ArmorSections[Index].HP);
        Slot.Armor = Minimum == TNumericLimits<float>::Max() ? 0 : Minimum;
    }
}

void UOVShipSystemsComponent::MarkWrecked()
{
    if (bWrecked) return;
    bWrecked = true; ++WreckEventCount;
    Shield = 0; MaxShield = 0; ShieldRegen = 0; bShieldActive = false;
    for (FOVFacilityState& Slot : Slots) Slot.Crew = 0;
}

void UOVShipSystemsComponent::RefreshDerivedState()
{
    if (!Layout) { MaxShield = 0; ShieldRegen = 0; Shield = 0; bShieldActive = false; return; }
    if (HullHP <= 0) MarkWrecked();
    const int32 Core = Layout->GetSlotIndex(Layout->CoreSlotId);
    if (Slots.IsValidIndex(Core) && Slots[Core].HP <= 0) MarkWrecked();
    float CrewTotal = 0;
    for (const FOVFacilityState& State : Slots) CrewTotal += State.Crew;
    bNeutral = !bWrecked && (bCapturedBridgeDestroyed || (CrewTotal <= 0 && !HasInteriorPresence(FactionId)));
    int32 OperationalControls = 0;
    for (int32 Index = 0; Index < Slots.Num(); ++Index)
        if (Layout->Slots[Index].Facility == EOVFacilityType::ShieldControl && IsOperational(Index)) ++OperationalControls;
    // Partial staffing does not shrink nominal capacity. Multiple-room capacity remains a provisional rule.
    const float CapacityFactor = OperationalControls > 0 ? 1.f + (OperationalControls - 1) * FMath::Max(0.f, Tuning.AdditionalShieldCapacity) : 0.f;
    const float NominalCrewEfficiency = OperationalControls > 0 ? .5f * (OperationalControls + 1) : 0.f;
    ShieldCrewEfficiency = NominalCrewEfficiency > 0 ? GetEfficiency(EOVFacilityType::ShieldControl) / NominalCrewEfficiency : 0.f;
    MaxShield = FMath::Max(0.f, Tuning.ShieldBase) * CapacityFactor;
    ShieldRegen = MaxShield * FMath::Max(0.f, Tuning.ShieldRegenFraction) * ShieldCrewEfficiency;
    Shield = FMath::Clamp(Shield, 0.f, MaxShield);
    if (MaxShield <= 0 || bWrecked || bShieldManualOff || Shield <= 0) bShieldActive = false;
    else if (!bShieldActive && Shield >= MaxShield * 0.5f)
    {
        // Capacity loss can move existing charge past 50%; transition before any recovery time is applied.
        bShieldActive = true;
        NotifyShieldActivated();
    }
    SyncArmorDisplay();
}

void UOVShipSystemsComponent::NotifyShieldActivated()
{
    if(auto* World=GetWorld())if(auto* Combat=World->GetSubsystem<UOVCombatSubsystem>())if(Combat->bEnabled)
        if(auto* Feed=World->GetSubsystem<UOVBattleNotices>())Feed->RecordShieldRestored(Cast<AOVShip>(GetOwner()),Combat->GameTime);
}

void UOVShipSystemsComponent::Simulate(float SimDelta)
{
    if (!Layout || bWrecked || !FMath::IsFinite(SimDelta) || SimDelta <= 0) return;
    RefreshDerivedState();
    if (bWrecked) return;
    TimeSinceExternalHit += SimDelta;
    ShieldRecoveryRemaining = 0; // D116: even legacy nonzero settings cannot delay recovery.
    // D083: active shields never recharge, including outside combat. Legacy RecoveryPolicy is ignored.
    if (MaxShield <= 0 || bShieldManualOff || bShieldActive) return;
    const float RecoverTime = SimDelta;
    const float RecoveryStart = TimeSinceExternalHit - RecoverTime;
    const float CombatTime = FMath::Clamp(Tuning.NoHitRecoveryInterval - RecoveryStart, 0.f, RecoverTime);
    const float NonCombatTime = RecoverTime - CombatTime;
    const float Recovery = ShieldRegen * CombatTime
        + MaxShield * FMath::Max(0.f, Tuning.ShieldNonCombatRegenFraction) * ShieldCrewEfficiency * NonCombatTime;
    const float Half = 0.5f * MaxShield;
    Shield = FMath::Min(Half, Shield + Recovery);
    if (Shield + UE_KINDA_SMALL_NUMBER >= Half) {
        Shield = Half; bShieldActive = true;
        NotifyShieldActivated();
    }
}

FOVFacilityState UOVShipSystemsComponent::GetSlotState(FName SlotId) const
{
    const int32 Index = Layout ? Layout->GetSlotIndex(SlotId) : INDEX_NONE;
    return Slots.IsValidIndex(Index) ? Slots[Index] : FOVFacilityState();
}

FString UOVShipSystemsComponent::GetSlotStatus(FName SlotId) const
{
    const int32 Index = Layout ? Layout->GetSlotIndex(SlotId) : INDEX_NONE;
    if (!Slots.IsValidIndex(Index)) return TEXT("Unknown");
    if (Layout->Slots[Index].Facility == EOVFacilityType::None) return TEXT("Empty");
    if (Slots[Index].HP <= 0) return TEXT("Destroyed");
    if (Slots[Index].OccupyingFaction >= 0 && Slots[Index].OccupyingFaction != FactionId) return TEXT("Occupied");
    if (!IsOperational(Index)) return TEXT("Uncrewed");
    return TEXT("Operational");
}

void UOVShipSystemsComponent::BeginDamageEvent(EOVDamageCause Cause, FName Source)
{
    ++HitEventId; LastCause = Cause; LastDamageSource = Source;
    LastDamageIndex = INDEX_NONE; LastDamageSlot = NAME_None; LastArmorSection = NAME_None;
    LastWeaponMount = NAME_None; LastHullDamage = 0; LastExplosionCount = 0;
}

void UOVShipSystemsComponent::NotifyCombatParticipation()
{
    TimeSinceExternalHit = 0;
}

float UOVShipSystemsComponent::AbsorbExternalDamage(float Damage)
{
    ShieldRecoveryRemaining = 0;
    if (!bShieldActive || Shield <= 0) return Damage;
    const float Overflow = FMath::Max(0.f, Damage - Shield);
    Shield = FMath::Max(0.f, Shield - Damage);
    if (Shield <= 0) bShieldActive = false;
    return Overflow * FMath::Clamp(Tuning.ShieldOverflowFraction, 0.f, 1.f);
}

void UOVShipSystemsComponent::ApplyWorldWeaponHit(FName WeaponMountId, FVector WorldPoint, FVector WorldNormal, float Damage, bool bCombatAttack)
{
    if (!Layout || bWrecked || !FMath::IsFinite(Damage) || Damage <= 0 || WorldPoint.ContainsNaN() || WorldNormal.ContainsNaN()) return;
    FOVWeaponMountState* Mount = WeaponMountStates.FindByPredicate([WeaponMountId](const FOVWeaponMountState& State) { return State.WeaponMountId == WeaponMountId; });
    if (!Mount || Mount->HP <= 0) return;
    if (bCombatAttack) NotifyCombatParticipation();
    BeginDamageEvent(EOVDamageCause::External, IncomingSourceId.IsNone() ? FName(TEXT("WorldWeaponHit")) : IncomingSourceId);
    const FTransform Transform = GetOwner() ? GetOwner()->GetActorTransform() : FTransform::Identity;
    LastWorldHit = WorldPoint; LastLocalHit = Transform.InverseTransformPosition(WorldPoint);
    LastLocalNormal = (Transform.InverseTransformVectorNoScale(WorldNormal) * Transform.GetScale3D()).GetSafeNormal();
    LastWeaponMount = WeaponMountId;
    Mount->HP = FMath::Max(0.f, Mount->HP - AbsorbExternalDamage(Damage));
}

void UOVShipSystemsComponent::ApplyWorldHit(FVector WorldPoint, FVector WorldNormal, float Damage, EOVDamageCause Cause, bool bCombatAttack)
{
    if (!Layout || bWrecked || !FMath::IsFinite(Damage) || Damage <= 0 || WorldPoint.ContainsNaN() || WorldNormal.ContainsNaN()) return;
    const FTransform Transform = GetOwner() ? GetOwner()->GetActorTransform() : FTransform::Identity;
    const FVector LocalPoint = Transform.InverseTransformPosition(WorldPoint);
    const FVector LocalNormal = (Transform.InverseTransformVectorNoScale(WorldNormal) * Transform.GetScale3D()).GetSafeNormal();
    const int32 ArmorIndex = Layout->ResolveArmor(LocalPoint, LocalNormal);
    if (!ArmorSections.IsValidIndex(ArmorIndex)) return;
    int32 SlotIndex = Layout->MapArmorToSlot(ArmorIndex, LocalPoint);
    if (!Slots.IsValidIndex(SlotIndex)) return;
    TGuardValue<bool> CombatDamageScope(bProcessingCombatDamage, bCombatAttack);
    if (bCombatAttack) NotifyCombatParticipation();
    BeginDamageEvent(Cause, IncomingSourceId.IsNone() ? FName(TEXT("WorldHit")) : IncomingSourceId);
    LastWorldHit = WorldPoint; LastLocalHit = LocalPoint; LastLocalNormal = LocalNormal;
    LastDamageIndex = SlotIndex; LastDamageSlot = Slots[SlotIndex].SlotId;
    LastArmorSection = ArmorSections[ArmorIndex].ArmorSectionId;
    if (Cause == EOVDamageCause::External)
    {
        Damage = AbsorbExternalDamage(Damage);
    }
    if (Damage <= 0) return;
    bool bDirectHull = false;
    bool bBypassArmor = Cause == EOVDamageCause::Sabotage;
    const EOVArmorFace Face = Layout->ArmorSections[ArmorIndex].Face;
    const bool bVertical = Face == EOVArmorFace::Top || Face == EOVArmorFace::Bottom;
    if (Cause == EOVDamageCause::External && Layout->DeckCount == 3 && bVertical
        && ArmorSections[ArmorIndex].HP <= 0
        && (Layout->Slots[SlotIndex].Facility == EOVFacilityType::None || Slots[SlotIndex].HP <= 0))
    {
        const FName Column = Layout->Slots[SlotIndex].ColumnId;
        for (int32 Index = 0; Index < Slots.Num(); ++Index)
            if (Layout->Slots[Index].DeckId == 2 && Layout->Slots[Index].ColumnId == Column) { SlotIndex = Index; break; }
        bBypassArmor = true;
        bDirectHull = Layout->Slots[SlotIndex].Facility == EOVFacilityType::None || Slots[SlotIndex].HP <= 0;
    }
    ProcessDamage(SlotIndex, ArmorIndex, Damage, Cause, bBypassArmor, bDirectHull);
}

float UOVShipSystemsComponent::GetExplosionDamage(int32 SlotIndex) const
{
    return Layout && Layout->Slots.IsValidIndex(SlotIndex) ? ExplosionDamage(SlotIndex) : 0.f;
}

float UOVShipSystemsComponent::ExplosionDamage(int32 SlotIndex) const
{
    const EOVFacilityType Type = Layout->Slots[SlotIndex].Facility;
    const int32 Mounts = Type == EOVFacilityType::Ammo ? Layout->CannonSlots
        : Type == EOVFacilityType::Capacitor ? Layout->BeamSlots
        : Type == EOVFacilityType::MissileStorage ? Layout->MissileSlots : 0;
    const EOVWeaponType Family = Type == EOVFacilityType::Ammo ? EOVWeaponType::Cannon
        : Type == EOVFacilityType::Capacitor ? EOVWeaponType::Beam : EOVWeaponType::Missile;
    float CoefficientSum = 0;
    bool bHasExplicitCoefficients = false;
    for (const FOVWeaponMountDefinition& Mount : Layout->WeaponMounts)
    {
        if (Mount.Type != Family || Mounts == 0) continue;
        bHasExplicitCoefficients |= Mount.ExplosionTierCoefficient > 0;
        CoefficientSum += Mount.ExplosionTierCoefficient > 0 ? Mount.ExplosionTierCoefficient : FMath::Max(0.f, Mount.Tier);
    }
    // Serialized legacy layouts retain their original count × layout tier calculation.
    if (!bHasExplicitCoefficients) CoefficientSum = Mounts * FMath::Max(0.f, Layout->WeaponTier);
    return FMath::Max(0.f, Tuning.ExplosionBase) * CoefficientSum * FMath::Max(0.f, Tuning.ExplosionTierCoefficient);
}

void UOVShipSystemsComponent::DestroyFacility(int32 SlotIndex, EOVDamageCause Cause, TArray<int32>& ExplosionQueue)
{
    if (!Slots.IsValidIndex(SlotIndex) || Layout->Slots[SlotIndex].Facility == EOVFacilityType::None) return;
    FOVFacilityState& State = Slots[SlotIndex];
    State.HP = 0;
    if (Layout->Slots[SlotIndex].Facility == EOVFacilityType::Bridge && bBridgeCaptured)
    {
        bCapturedBridgeDestroyed = true; FactionId = -1;
        if (AOVShip* Ship = Cast<AOVShip>(GetOwner()))
        {
            Ship->bFriendly = false; Ship->bFTLCharging = false;
            Ship->StopMovement(); Ship->Velocity = FVector::ZeroVector;
            Ship->bManualAttitude = false; Ship->SetAutomation(false, false);
            if (GetWorld()) GetWorld()->GetSubsystem<UOVCombatSubsystem>()->CancelAttackOrder(Ship);
        }
    }
    if (Cause != EOVDamageCause::Sabotage)
    {
        const int32 Deck = Layout->Slots[SlotIndex].DeckId;
        for (int32 Armor = 0; Armor < ArmorSections.Num(); ++Armor)
        {
            const FOVArmorDefinition& Definition = Layout->ArmorSections[Armor];
            if (FrontOrRear(Definition.Face) || !Definition.InnerSlotIds.Contains(State.SlotId)) continue;
            // Three-deck destruction links only its authored outer face. The one/two-deck
            // test fixtures retain their provisional non-end-face overlap mapping.
            const bool bBreakFace = Layout->DeckCount != 3
                || (Deck == 3 && Definition.Face == EOVArmorFace::Top)
                || (Deck == 2 && (Definition.Face == EOVArmorFace::Left || Definition.Face == EOVArmorFace::Right))
                || (Deck == 1 && Definition.Face == EOVArmorFace::Bottom);
            if (bBreakFace) ArmorSections[Armor].HP = 0;
        }
    }
    if (Layout->Slots[SlotIndex].Facility == EOVFacilityType::Core) MarkWrecked();
    if (ExplosionDamage(SlotIndex) > 0) ExplosionQueue.AddUnique(SlotIndex);
}

void UOVShipSystemsComponent::ProcessDamage(int32 SlotIndex, int32 ArmorIndex, float Damage, EOVDamageCause Cause, bool bBypassArmor, bool bDirectHull)
{
    struct FPending { int32 Slot; int32 Armor; float Amount; EOVDamageCause Cause; bool Bypass; bool Direct; };
    TArray<FPending> Pending;
    Pending.Add({SlotIndex, ArmorIndex, Damage, Cause, bBypassArmor, bDirectHull});
    TSet<int32> Exploded;
    TArray<int32> ExplosionQueue;
    for (int32 Event = 0; Event < Pending.Num() && !bWrecked; ++Event)
    {
        const FPending Hit = Pending[Event];
        if (!Slots.IsValidIndex(Hit.Slot) || Hit.Amount <= 0) continue;
        if(Event==0) { LastDamageIndex = Hit.Slot; LastDamageSlot = Slots[Hit.Slot].SlotId; }
        float Delivered = Hit.Amount;
        if (!Hit.Bypass && !Hit.Direct && ArmorSections.IsValidIndex(Hit.Armor))
        {
            FOVArmorSectionState& Armor = ArmorSections[Hit.Armor];
            const float Maximum = Layout->ArmorSections[Hit.Armor].MaxArmor;
            const float Ratio = Maximum > 0 ? FMath::Clamp(Armor.HP / Maximum, 0.f, 1.f) : 0;
            const float Reduction=Layout->ArmorSections[Hit.Armor].Face==EOVArmorFace::Front?Tuning.FrontArmorMaxReduction:Tuning.ArmorMaxReduction;
            Delivered *= 1.f - FMath::Clamp(Reduction * Ratio, 0.f, 1.f);
            // This wear applies to armor only; it is never added again to facility/hull damage.
            Armor.HP = FMath::Max(0.f, Armor.HP - Hit.Amount * FMath::Max(0.f, Tuning.ArmorWear));
        }
        FOVFacilityState& Slot = Slots[Hit.Slot];
        const bool bAliveFacility = Layout->Slots[Hit.Slot].Facility != EOVFacilityType::None && Slot.HP > 0;
        if (!Hit.Direct && bAliveFacility)
        {
            if (bProcessingCombatDamage && !Unmanned(Layout->Slots[Hit.Slot].Facility))
            {
                const auto& Def = Layout->Slots[Hit.Slot];
                const float Loss = Def.RequiredCrew * FMath::Clamp(Delivered / FMath::Max(1.f, Def.MaxFacilityHP), 0.f, 1.f)
                    * FMath::Max(0.f, Tuning.CrewDamageCoefficient);
                Slot.Crew = FMath::Max(0.f, Slot.Crew - Loss);
            }
            Slot.HP = FMath::Max(0.f, Slot.HP - Delivered);
            if (Slot.HP <= 0) DestroyFacility(Hit.Slot, Hit.Cause, ExplosionQueue);
        }
        const float HullLoss = Hit.Direct ? Delivered : Delivered * FMath::Clamp(Tuning.FacilityHPToHull, 0.f, 1.f);
        const float AppliedHull = FMath::Min(HullHP, HullLoss);
        HullHP = FMath::Max(0.f, HullHP - AppliedHull); LastHullDamage += AppliedHull;
        if (HullHP <= 0) MarkWrecked();
        for (const int32 Explosive : ExplosionQueue)
        {
            if (Exploded.Contains(Explosive)) continue;
            Exploded.Add(Explosive); ++LastExplosionCount;
            TSet<int32> Neighbors;
            const FName ExplosiveId = Slots[Explosive].SlotId;
            for (const FOVConnectionDefinition& Connection : Layout->Connections)
            {
                const FName Other = Connection.FromSlot == ExplosiveId ? Connection.ToSlot : Connection.ToSlot == ExplosiveId ? Connection.FromSlot : NAME_None;
                const int32 Neighbor = Layout->GetSlotIndex(Other);
                if (Neighbor != INDEX_NONE && Neighbor != Explosive) Neighbors.Add(Neighbor);
            }
            for (const int32 Neighbor : Neighbors)
            {
                // Interior explosions bypass an unrelated exterior face. Destruction still breaks linked armor.
                Pending.Add({Neighbor, INDEX_NONE, ExplosionDamage(Explosive), EOVDamageCause::Explosion, true, false});
            }
        }
        ExplosionQueue.Reset();
    }
    RefreshDerivedState();
    UE_LOG(LogTemp, Log, TEXT("OVDamage event=%d source=%s faction=%d weapon=%s cause=%d armor=%s slot=%s hullLoss=%.2f explosions=%d wreck=%d"),
        HitEventId, *LastDamageSource.ToString(), IncomingFactionId, *IncomingWeaponId.ToString(), static_cast<int32>(LastCause),
        *LastArmorSection.ToString(), *LastDamageSlot.ToString(), LastHullDamage, LastExplosionCount, bWrecked);
}

void UOVShipSystemsComponent::DebugDestroySlot(FName SlotId, EOVDamageCause Cause)
{
    const int32 Index = Layout ? Layout->GetSlotIndex(SlotId) : INDEX_NONE;
    if (bWrecked || !Slots.IsValidIndex(Index) || Slots[Index].HP <= 0) return;
    BeginDamageEvent(Cause, FName(TEXT("DebugDestroySlot")));
    LastLocalHit = Layout->Slots[Index].LocalCenter;
    LastWorldHit = GetOwner() ? GetOwner()->GetActorTransform().TransformPosition(LastLocalHit) : LastLocalHit;
    ProcessDamage(Index, INDEX_NONE, Slots[Index].HP, Cause, true, false);
}

void UOVShipSystemsComponent::DebugRepairSlot(FName SlotId)
{
    const int32 Index = Layout ? Layout->GetSlotIndex(SlotId) : INDEX_NONE;
    if (bWrecked || !Slots.IsValidIndex(Index)) return;
    Slots[Index].HP = Layout->Slots[Index].MaxFacilityHP;
    // Physical repair never changes crew, occupation, weapon health, or current shield charge.
    RefreshDerivedState();
}

void UOVShipSystemsComponent::DebugSetCrew(FName SlotId, float Crew)
{
    const int32 Index = Layout ? Layout->GetSlotIndex(SlotId) : INDEX_NONE;
    if (bWrecked || !Slots.IsValidIndex(Index) || !FMath::IsFinite(Crew)) return;
    Slots[Index].Crew = FMath::Clamp(Crew, 0.f, Layout->Slots[Index].RequiredCrew);
    RefreshDerivedState();
}

void UOVShipSystemsComponent::DebugSetOccupation(FName SlotId, int32 OccupyingFaction)
{
    const int32 Index = Layout ? Layout->GetSlotIndex(SlotId) : INDEX_NONE;
    if (bWrecked || !Slots.IsValidIndex(Index) || Slots[Index].HP <= 0) return;
    Slots[Index].OccupyingFaction = FMath::Max(-1, OccupyingFaction);
    RefreshDerivedState();
}

void UOVShipSystemsComponent::DebugSetArmor(FName ArmorSectionId, float HP)
{
    const int32 Index = Layout ? Layout->GetArmorIndex(ArmorSectionId) : INDEX_NONE;
    if (bWrecked || !ArmorSections.IsValidIndex(Index) || !FMath::IsFinite(HP)) return;
    ArmorSections[Index].HP = FMath::Clamp(HP, 0.f, Layout->ArmorSections[Index].MaxArmor);
    SyncArmorDisplay();
}

void UOVShipSystemsComponent::DebugHitWeapon(FName WeaponMountId, float Damage)
{
    if (bWrecked || !Layout || !FMath::IsFinite(Damage) || Damage <= 0) return;
    for (FOVWeaponMountState& State : WeaponMountStates)
        if (State.WeaponMountId == WeaponMountId)
        {
            BeginDamageEvent(EOVDamageCause::External, FName(TEXT("DebugWeaponOnly_R14")));
            LastWeaponMount = WeaponMountId;
            State.HP = FMath::Max(0.f, State.HP - Damage);
            return;
        }
}

void UOVShipSystemsComponent::DebugRepairWeapon(FName WeaponMountId)
{
    if (bWrecked || !Layout) return;
    for (int32 Index = 0; Index < WeaponMountStates.Num(); ++Index)
        if (WeaponMountStates[Index].WeaponMountId == WeaponMountId) WeaponMountStates[Index].HP = Layout->WeaponMounts[Index].MaxHP;
}

bool UOVShipSystemsComponent::IsWeaponAvailable(FName WeaponMountId) const
{
    if (!CanAct()) return false;
    for (int32 Index = 0; Index < WeaponMountStates.Num(); ++Index)
        if (WeaponMountStates[Index].WeaponMountId == WeaponMountId && WeaponMountStates[Index].HP > 0)
        {
            switch (Layout->WeaponMounts[Index].Type)
            {
                case EOVWeaponType::Cannon: return CanFireCannons();
                case EOVWeaponType::Beam: return CanFireBeams();
                case EOVWeaponType::Missile: return CanFireMissiles();
                case EOVWeaponType::Interception: return CanIntercept();
            }
        }
    return false;
}

bool UOVShipSystemsComponent::SetShieldManualOff(bool bOff)
{
    if (bWrecked || !Tuning.bAllowManualShieldToggle) return false;
    bShieldManualOff = bOff;
    RefreshDerivedState();
    if (!bOff && MaxShield > 0 && Shield >= MaxShield * 0.5f) bShieldActive = true;
    return true;
}

void UOVShipSystemsComponent::SetInteriorPresence(int32 Faction, bool bPresent, bool bObjectiveComplete, bool bDeveloperInput)
{
    if (Faction < 0 || bWrecked) return;
    if (bPresent) InteriorObjectives.Add(Faction, bObjectiveComplete); else InteriorObjectives.Remove(Faction);
    if (bPresent && bDeveloperInput) InteriorDeveloperInputs.Add(Faction); else InteriorDeveloperInputs.Remove(Faction);
    RefreshDerivedState();
}
bool UOVShipSystemsComponent::HasInteriorPresence(int32 Faction) const { return !bWrecked && (RuntimeInteriorObjectives.Contains(Faction) || InteriorObjectives.Contains(Faction)); }
bool UOVShipSystemsComponent::IsInteriorObjectiveComplete(int32 Faction) const
{
    if (const bool* Runtime = RuntimeInteriorObjectives.Find(Faction)) return *Runtime;
    const bool* Value = InteriorObjectives.Find(Faction); return Value && *Value;
}
bool UOVShipSystemsComponent::IsInteriorDeveloperInput(int32 Faction) const { return !RuntimeInteriorObjectives.Contains(Faction) && InteriorDeveloperInputs.Contains(Faction); }

void UOVShipSystemsComponent::SetRuntimeInteriorPresence(int32 Faction, bool bPresent, bool bObjectiveComplete)
{
    if (Faction < 0) return;
    if (bPresent && !bWrecked) RuntimeInteriorObjectives.Add(Faction, bObjectiveComplete);
    else RuntimeInteriorObjectives.Remove(Faction);
    RefreshDerivedState();
}

bool UOVShipSystemsComponent::CompleteInteriorOperation(FName SlotId, int32 ActingFaction, bool bSabotage)
{
    const int32 Index = Layout ? Layout->GetSlotIndex(SlotId) : INDEX_NONE;
    if (ActingFaction < 0 || bWrecked || !Slots.IsValidIndex(Index) || Slots[Index].HP <= 0 ||
        Layout->Slots[Index].Facility == EOVFacilityType::None) return false;
    if (bSabotage)
    {
        BeginDamageEvent(EOVDamageCause::Sabotage, TEXT("InteriorSabotage"));
        IncomingFactionId = ActingFaction;
        LastLocalHit = Layout->Slots[Index].LocalCenter;
        LastWorldHit = GetOwner()->GetActorTransform().TransformPosition(LastLocalHit);
        ProcessDamage(Index, INDEX_NONE, Slots[Index].HP, EOVDamageCause::Sabotage, true, false);
        return true;
    }
    Slots[Index].OccupyingFaction = ActingFaction;
    if (Layout->Slots[Index].Facility == EOVFacilityType::Bridge && FactionId != ActingFaction)
    {
        FactionId = ActingFaction; bBridgeCaptured = true; bCapturedBridgeDestroyed = false; ++CaptureCount;
        if (AOVShip* Ship = Cast<AOVShip>(GetOwner()))
        {
            Ship->StopMovement(); Ship->Velocity = FVector::ZeroVector;
            Ship->bFTLCharging = false; Ship->bManualAttitude = false;
            Ship->SetAutomation(false, false); Ship->bFriendly = ActingFaction == 0;
            if (GetWorld()) GetWorld()->GetSubsystem<UOVCombatSubsystem>()->CancelAttackOrder(Ship);
        }
    }
    RefreshDerivedState();
    return true;
}
