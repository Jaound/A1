#include "OVTransportSubsystem.h"
#include "OVCombatSubsystem.h"
#include "OVShip.h"
#include "OVShipSystemsComponent.h"
#include "OVInteriorSubsystem.h"
#include "Engine/World.h"

namespace OVTransport
{
constexpr float Step = 1.f / 120.f;
bool IsFlying(EOVPodFlightState State) { return State == EOVPodFlightState::Outbound || State == EOVPodFlightState::Returning; }
EOVArmorFace Face(FVector Normal)
{
    const FVector Abs = Normal.GetAbs();
    // R26 provisional edge priority matches the deterministic box contact normal.
    if (Abs.Z >= Abs.X && Abs.Z >= Abs.Y) return Normal.Z >= 0 ? EOVArmorFace::Top : EOVArmorFace::Bottom;
    if (Abs.X >= Abs.Y) return Normal.X >= 0 ? EOVArmorFace::Front : EOVArmorFace::Rear;
    return Normal.Y >= 0 ? EOVArmorFace::Right : EOVArmorFace::Left;
}
}

bool UOVTransportSubsystem::Usable(AOVShip* Ship) const
{
    if (!IsValid(Ship) || !Ship->Systems || Ship->Systems->bWrecked || !Ship->Systems->Layout) return false;
    const UOVCombatSubsystem* Combat = GetWorld()->GetSubsystem<UOVCombatSubsystem>();
    return !Combat || !Combat->IsDeparted(Ship);
}

FOVPassengerToken* UOVTransportSubsystem::FindPerson(int32 Id)
{
    return Persons.FindByPredicate([Id](const FOVPassengerToken& Person) { return Person.PersonId == Id; });
}

void UOVTransportSubsystem::ResetScenario(const TArray<AOVShip*>& Ships)
{
    Disable();
    ScenarioShips.Reset();
    for (AOVShip* Ship : Ships) if (IsValid(Ship)) ScenarioShips.Add(Ship);
    Persons.Reset(); Pods.Reset(); Shuttles.Reset(); EntryEvents.Reset();
    GameTime = Accumulator = 0;
    bEnabled = true;
    LastMessage = TEXT("DEV 승객 시험 준비");
}

bool UOVTransportSubsystem::AddShip(AOVShip* Ship)
{
    if(!Usable(Ship))return false;
    ScenarioShips.AddUnique(Ship); bEnabled=true; return true;
}

bool UOVTransportSubsystem::CanRemoveShip(AOVShip* Ship) const
{
    for(const auto& P:Persons)
        if(P.State!=EOVPassengerState::Dead && (P.CurrentShip==Ship || P.Shuttle==Ship))return false;
    for(const auto& P:Pods)
        if((OVTransport::IsFlying(P.State)||P.State==EOVPodFlightState::Queued||P.State==EOVPodFlightState::Attached) &&
            (P.Origin==Ship||P.Target==Ship||P.ReturnDestination==Ship))return false;
    return true;
}

bool UOVTransportSubsystem::RemoveShip(AOVShip* Ship)
{
    if(!CanRemoveShip(Ship))return false;
    ScenarioShips.Remove(Ship);
    Shuttles.RemoveAll([Ship](const auto& S){return S.Ship==Ship;});
    return true;
}

void UOVTransportSubsystem::Disable()
{
    for (const FPresence& Presence : OwnedPresence)
        if (AOVShip* Ship = Presence.Ship.Get())
            Ship->Systems->SetInteriorPresence(Presence.Faction, Presence.Baseline, Presence.BaselineObjective, Presence.BaselineDev);
    OwnedPresence.Reset();
    bEnabled = false;
    Accumulator = 0;
}

TArray<int32> UOVTransportSubsystem::AddDevPassengers(AOVShip* Origin, int32 HeroTypes, int32 Troops)
{
    TArray<int32> Ids;
    if (!bEnabled || !Usable(Origin) || Origin->Systems->FactionId < 0) return Ids;
    HeroTypes = FMath::Clamp(HeroTypes, 0, 100);
    Troops = FMath::Clamp(Troops, 0, 100);
    for (int32 I = 0; I < HeroTypes + Troops; ++I)
    {
        FOVPassengerToken Person;
        Person.PersonId = NextPersonId++;
        Person.FactionId = Origin->Systems->FactionId;
        Person.bHeroTypeDEV = I < HeroTypes;
        Person.CurrentShip = Origin;
        Ids.Add(Person.PersonId); Persons.Add(Person);
    }
    LastMessage = FString::Printf(TEXT("DEV 승객 %d명 추가 (실제 영웅 아님)"), Ids.Num());
    return Ids;
}

int32 UOVTransportSubsystem::LaunchPods(AOVShip* Origin, AOVShip* Target)
{
    if (!bEnabled || !Usable(Origin) || !Usable(Target) || Origin == Target ||
        Origin->Systems->FactionId == Target->Systems->FactionId || Origin->Systems->GetEfficiency(EOVFacilityType::Boarding) <= 0 ||
        FVector::Distance(Origin->GetActorLocation(), Target->GetActorLocation()) > Settings.PodLaunchRange)
    { LastMessage = TEXT("발사 불가: 강습 시설·거리·표적 확인"); return 0; }
    int32 Count = 0, RemotePodCount = 0;
    double NextTime = GameTime;
    for (const FOVPodState& Pod : Pods)
        if (Pod.Origin == Origin && Pod.State == EOVPodFlightState::Queued) NextTime = FMath::Max(NextTime, Pod.ScheduledTime + Settings.LaunchInterval);
    for (const FOVPassengerToken& Person : Persons)
    {
        if (Person.RuntimeUnitId > 0 && !Person.bBoardingAssigned) continue;
        if (Person.State != EOVPassengerState::Aboard || Person.CurrentShip != Origin || Person.FactionId != Origin->Systems->FactionId) continue;
        if (Pods.ContainsByPredicate([&](const FOVPodState& Pod) { return Pod.PersonId == Person.PersonId && Pod.State == EOVPodFlightState::Queued; })) continue;
        FOVPodState* Reusable = Pods.FindByPredicate([&](const FOVPodState& Pod) {
            return Pod.PersonId == Person.PersonId && Pod.HP > 0 &&
                ((Pod.State == EOVPodFlightState::Recovered && Pod.ReturnDestination == Origin) ||
                 (Pod.State == EOVPodFlightState::Cancelled && Pod.Origin == Origin)); });
        if (Reusable)
        {
            Reusable->State = EOVPodFlightState::Queued; Reusable->Origin = Origin; Reusable->Target = Target;
            Reusable->ReturnDestination = nullptr; Reusable->Position = Origin->GetActorLocation();
            Reusable->ScheduledTime = NextTime; Reusable->LaunchTime = -1;
            // Recovered pod identity and damage persist: a new sortie is not free replacement/repair.
            NextTime += FMath::Max(.001f, Settings.LaunchInterval); ++Count; continue;
        }
        if (Pods.ContainsByPredicate([&](const FOVPodState& Pod) {
            return Pod.PersonId == Person.PersonId && Pod.HP > 0 &&
                (Pod.State == EOVPodFlightState::Recovered || Pod.State == EOVPodFlightState::Cancelled); }))
        { ++RemotePodCount; continue; } // A person transfer is not a physical pod inventory transfer.
        FOVPodState Pod;
        Pod.PodId = NextPodId++; Pod.PersonId = Person.PersonId; Pod.FactionId = Person.FactionId;
        Pod.Origin = Origin; Pod.Target = Target; Pod.Position = Origin->GetActorLocation();
        Pod.HP = Settings.PodHP; Pod.Evasion = Settings.PodEvasion; Pod.ScheduledTime = NextTime;
        Pods.Add(Pod); NextTime += FMath::Max(.001f, Settings.LaunchInterval); ++Count;
    }
    LastMessage = RemotePodCount > 0 ? FString::Printf(TEXT("포드 %d기 예약 · %d명은 타 함선 보관 포드 이송 필요(미지원)"), Count, RemotePodCount) :
        FString::Printf(TEXT("개인 포드 %d기 발사 예약"), Count);
    return Count;
}

AOVShip* UOVTransportSubsystem::FindReturn(AOVShip* Preferred, int32 Faction, FVector Position, AOVShip* Exclude) const
{
    if (Preferred != Exclude && Usable(Preferred) && Preferred->Systems->FactionId == Faction && !Preferred->Systems->bNeutral) return Preferred;
    AOVShip* Best = nullptr;
    double Distance = TNumericLimits<double>::Max();
    for (AOVShip* Ship : ScenarioShips)
    {
        if (Ship == Exclude || !Usable(Ship) || Ship->Systems->FactionId != Faction || Ship->Systems->bNeutral) continue;
        const double Candidate = FVector::DistSquared(Ship->GetActorLocation(), Position);
        if (Candidate < Distance) { Best = Ship; Distance = Candidate; }
    }
    return Best;
}

void UOVTransportSubsystem::BeginReturn(FOVPodState& Pod)
{
    if (Pod.State == EOVPodFlightState::Destroyed || Pod.State == EOVPodFlightState::Recovered || Pod.State == EOVPodFlightState::Cancelled) return;
    Pod.ReturnDestination = FindReturn(Pod.Origin.Get(), Pod.FactionId, Pod.Position);
    Pod.State = Pod.ReturnDestination.IsValid() ? EOVPodFlightState::Returning : EOVPodFlightState::Stranded;
    if (FOVPassengerToken* Person = FindPerson(Pod.PersonId))
    {
        Person->CurrentShip = nullptr;
        Person->State = Pod.State == EOVPodFlightState::Returning ? EOVPassengerState::InPod : EOVPassengerState::RescueWaiting;
    }
}

void UOVTransportSubsystem::RecallPods(AOVShip* Origin)
{
    for (FOVPodState& Pod : Pods)
    {
        if (Pod.Origin != Origin) continue;
        if (Pod.State == EOVPodFlightState::Queued) Pod.State = EOVPodFlightState::Cancelled;
        else if (Pod.State == EOVPodFlightState::Outbound) BeginReturn(Pod);
    }
    const bool HasRuntimePassenger = Pods.ContainsByPredicate([&](const auto& Pod) {
        const auto* P = FindPerson(Pod.PersonId); return Pod.Origin == Origin && P && P->RuntimeUnitId > 0; });
    LastMessage = HasRuntimePassenger ? TEXT("비행 포드 회항 · 선내 인원은 원 진입점까지 실제 철수 필요") :
        TEXT("DEV 비행 포드 회항 · 내부 토큰은 진입점 복귀 확인 필요");
}

bool UOVTransportSubsystem::DevReturnPersonToEntry(int32 PersonId)
{
    FOVPassengerToken* Person = FindPerson(PersonId);
    if (!bEnabled || !Person || Person->RuntimeUnitId != 0 || Person->State != EOVPassengerState::InsideTarget || !Usable(Person->CurrentShip.Get())) return false;
    return ReturnPersonAtEntry(*Person);
}

int32 UOVTransportSubsystem::RegisterRuntimePassenger(AOVShip* Origin, int32 UnitId, int32 Faction, bool bHero)
{
    if (!bEnabled || !Usable(Origin) || UnitId <= 0 || Faction < 0 ||
        Persons.ContainsByPredicate([UnitId](const auto& P) { return P.RuntimeUnitId == UnitId; })) return 0;
    FOVPassengerToken Person; Person.PersonId = NextPersonId++; Person.RuntimeUnitId = UnitId;
    Person.FactionId = Faction; Person.bRuntimeHero = bHero; Person.CurrentShip = Origin;
    Persons.Add(Person); return Person.PersonId;
}

bool UOVTransportSubsystem::MarkRuntimePersonDead(int32 PersonId, int32 UnitId)
{
    FOVPassengerToken* Person = FindPerson(PersonId);
    if (!Person || UnitId <= 0 || Person->RuntimeUnitId != UnitId || Person->State == EOVPassengerState::Dead) return false;
    Person->State = EOVPassengerState::Dead; Person->CurrentShip = nullptr; RefreshPresence(); return true;
}

bool UOVTransportSubsystem::SetRuntimeBoardingAssignment(int32 PersonId,int32 UnitId,bool bAssigned)
{
    auto* P=FindPerson(PersonId);
    if(!P || UnitId<=0 || P->RuntimeUnitId!=UnitId || P->State!=EOVPassengerState::Aboard)return false;
    P->bBoardingAssigned=bAssigned;return true;
}

bool UOVTransportSubsystem::CancelRuntimeRecovery(int32 PersonId,int32 UnitId,AOVShip* InteriorShip)
{
    auto* P=FindPerson(PersonId);
    if(!P || UnitId<=0 || P->RuntimeUnitId!=UnitId || P->State!=EOVPassengerState::AwaitingRecovery ||
        P->CurrentShip!=InteriorShip || !Usable(InteriorShip))return false;
    P->State=EOVPassengerState::InsideTarget;return true;
}

bool UOVTransportSubsystem::ReturnRuntimePersonToEntry(int32 PersonId, int32 UnitId, AOVShip* Ship, FVector ActualLocalPoint, FName LayoutVersion)
{
    FOVPassengerToken* Person = FindPerson(PersonId);
    const auto Unit=GetWorld()->GetSubsystem<UOVInteriorSubsystem>()->GetUnit(UnitId);
    if (!bEnabled || !Person || UnitId <= 0 || Person->RuntimeUnitId != UnitId ||
        Unit.UnitId != UnitId || Unit.PersonId != PersonId || Unit.Ship != Ship || Unit.Order != EOVInteriorOrder::Retreat ||
        Unit.State != EOVInteriorState::Retreating || !FVector::PointsAreNear(Unit.LocalPosition,ActualLocalPoint,.01f) ||
        Person->State != EOVPassengerState::InsideTarget || !Usable(Ship) || Person->CurrentShip != Ship ||
        Person->EntryLayoutVersion != LayoutVersion || Ship->Systems->Layout->LayoutVersion != LayoutVersion ||
        ActualLocalPoint.ContainsNaN() || ((ActualLocalPoint-Person->EntryLocalPoint)*Ship->GetActorScale3D()).Size() > 40) return false;
    return ReturnPersonAtEntry(*Person);
}

bool UOVTransportSubsystem::ReturnPersonAtEntry(FOVPassengerToken& Person)
{
    if (Person.Shuttle.IsValid())
    {
        Person.State = EOVPassengerState::AwaitingRecovery;
        LastMessage = Usable(Person.Shuttle.Get()) && Person.Shuttle->Systems->FactionId == Person.FactionId ?
            TEXT("원 진입점 도착: 생존 강습선 회수 대기") : TEXT("원 강습선 회수 불가 · 비상탈출/대체수송은 미지원");
        return true;
    }
    FOVPodState* Pod = Pods.FindByPredicate([&Person](const FOVPodState& P) { return P.PodId == Person.PodId; });
    if (!Pod || Pod->State != EOVPodFlightState::Attached) return false;
    Pod->Position = Person.CurrentShip->GetActorTransform().TransformPosition(Person.EntryLocalPoint);
    BeginReturn(*Pod);
    RefreshPresence();
    LastMessage = TEXT("원 진입점 도착: 원래 포드로 귀환");
    return true;
}

void UOVTransportSubsystem::DestroyPod(FOVPodState& Pod)
{
    if (!OVTransport::IsFlying(Pod.State)) return;
    Pod.State = EOVPodFlightState::Destroyed; Pod.HP = 0;
    if (FOVPassengerToken* Person = FindPerson(Pod.PersonId))
    {
        Person->CurrentShip = nullptr;
        Person->State = (Person->bHeroTypeDEV || Person->bRuntimeHero) ? EOVPassengerState::RescueWaiting : EOVPassengerState::Dead;
    }
    const auto* Person = FindPerson(Pod.PersonId);
    LastMessage = Person && Person->RuntimeUnitId > 0 ?
        (Person->bRuntimeHero ? TEXT("포드 상실: 영웅 우주 구조 대기 · 후속 구조 미지원") : TEXT("포드 상실: 일반 병력 사망")) :
        TEXT("DEV 포드 상실: 일반 병력형 토큰 사망 / 영웅형 토큰 구조 대기");
}

bool UOVTransportSubsystem::InterceptPod(int32 Id, float Damage)
{
    if (!bEnabled || !FMath::IsFinite(Damage) || Damage <= 0) return false;
    FOVPodState* Pod = Pods.FindByPredicate([Id](const FOVPodState& P) { return P.PodId == Id; });
    if (!Pod || !OVTransport::IsFlying(Pod->State) || Pod->HP <= 0) return false;
    Pod->HP = FMath::Max(0.f, Pod->HP - Damage);
    if (Pod->HP <= 0) DestroyPod(*Pod);
    return true;
}

void UOVTransportSubsystem::Enter(FOVPassengerToken& Person, AOVShip* Target, FVector WorldPoint, FVector WorldNormal)
{
    if (!Usable(Target) || Person.State == EOVPassengerState::InsideTarget || Person.State == EOVPassengerState::Dead) return;
    Person.CurrentShip = Target; Person.State = EOVPassengerState::InsideTarget;
    // Entry protection is immediate even when the interior logic uses a coarser fixed step.
    if (Person.RuntimeUnitId > 0) Target->Systems->SetRuntimeInteriorPresence(Person.FactionId, true, false);
    Person.EntryLocalPoint = Target->GetActorTransform().InverseTransformPosition(WorldPoint);
    Person.EntryLocalNormal = (Target->GetActorTransform().InverseTransformVectorNoScale(WorldNormal) * Target->GetActorScale3D()).GetSafeNormal();
    Person.EntryFace = OVTransport::Face(Person.EntryLocalNormal);
    const UShipLayoutDataAsset* Layout = Target->Systems->Layout;
    Person.EntryLayoutVersion = Layout->LayoutVersion;
    Person.EntryDeck = FMath::Clamp(FMath::FloorToInt((Person.EntryLocalPoint.Z / FMath::Max(1., Layout->LocalHalfExtent.Z) + 1) * .5 * Layout->DeckCount) + 1, 1, Layout->DeckCount);
    if (Person.EntryFace == EOVArmorFace::Top) Person.EntryDeck = Layout->DeckCount;
    if (Person.EntryFace == EOVArmorFace::Bottom) Person.EntryDeck = 1;
    const int32 Slot = Layout->ResolveHit(Person.EntryLocalPoint, Person.EntryLocalNormal);
    if (Layout->Slots.IsValidIndex(Slot)) Person.EntrySlot = Layout->Slots[Slot].SlotId;
    if (Person.RuntimeUnitId == 0 && !OwnedPresence.ContainsByPredicate([&](const FPresence& P) { return P.Ship == Target && P.Faction == Person.FactionId; }))
        OwnedPresence.Add({Target, Person.FactionId, Target->Systems->HasLegacyInteriorPresence(Person.FactionId),
            Target->Systems->IsLegacyInteriorObjectiveComplete(Person.FactionId), Target->Systems->IsLegacyInteriorDeveloperInput(Person.FactionId)});
    FOVTransportEntryEvent Event;
    Event.PersonId = Person.PersonId; Event.Target = Target; Event.FactionId = Person.FactionId;
    Event.LocalPoint = Person.EntryLocalPoint; Event.Face = Person.EntryFace; Event.Deck = Person.EntryDeck; Event.SlotId = Person.EntrySlot; Event.Time = GameTime;
    EntryEvents.Add(Event);
    if (EntryEvents.Num() > 256) EntryEvents.RemoveAt(0, EntryEvents.Num()-256, EAllowShrinking::No);
    LastMessage = Person.RuntimeUnitId > 0 ? TEXT("진입 완료: 선내 인원 이동·교전 시작") : TEXT("진입 완료: DEV 승객 잔류, 선내 작전·나포는 미구현");
    RefreshPresence();
}

void UOVTransportSubsystem::RefreshPresence()
{
    for (const FPresence& Presence : OwnedPresence)
    {
        AOVShip* Ship = Presence.Ship.Get();
        if (!IsValid(Ship) || !Ship->Systems) continue;
        const bool Present = Persons.ContainsByPredicate([&](const FOVPassengerToken& P) {
            return P.RuntimeUnitId == 0 && P.FactionId == Presence.Faction && P.CurrentShip == Ship &&
                (P.State == EOVPassengerState::InsideTarget || P.State == EOVPassengerState::AwaitingRecovery); });
        const bool Desired = Presence.Baseline || Present;
        const bool Developer = Present || Presence.BaselineDev;
        // Transport never invents objective success, and never overwrites an external interior result.
        const bool Objective = Desired && Ship->Systems->IsLegacyInteriorObjectiveComplete(Presence.Faction);
        if (Ship->Systems->HasLegacyInteriorPresence(Presence.Faction) != Desired ||
            Ship->Systems->IsLegacyInteriorDeveloperInput(Presence.Faction) != Developer)
            Ship->Systems->SetInteriorPresence(Presence.Faction, Desired, Objective, Developer);
    }
}

void UOVTransportSubsystem::AdvancePod(FOVPodState& Pod, float Delta)
{
    FOVPassengerToken* Person = FindPerson(Pod.PersonId);
    if (!Person) return;
    if (Pod.State == EOVPodFlightState::Attached && Person->State == EOVPassengerState::Dead)
    { Pod.State = EOVPodFlightState::Destroyed; Pod.HP = 0; return; }
    if (Pod.State == EOVPodFlightState::Queued)
    {
        if (GameTime + .00001 < Pod.ScheduledTime) return;
        AOVShip* Origin = Pod.Origin.Get(); AOVShip* Target = Pod.Target.Get();
        if (!Usable(Origin) || !Usable(Target) || Origin->Systems->FactionId != Pod.FactionId ||
            Target->Systems->FactionId == Pod.FactionId || Origin->Systems->GetEfficiency(EOVFacilityType::Boarding) <= 0 ||
            FVector::Distance(Origin->GetActorLocation(), Target->GetActorLocation()) > Settings.PodLaunchRange ||
            Person->State != EOVPassengerState::Aboard || Person->CurrentShip != Origin)
        { Pod.State = EOVPodFlightState::Cancelled; return; }
        Pod.State = EOVPodFlightState::Outbound; Pod.LaunchTime = GameTime; Pod.Position = Origin->GetActorLocation();
        Person->State = EOVPassengerState::InPod; Person->CurrentShip = nullptr; Person->PodId = Pod.PodId; Person->Shuttle = nullptr;
    }
    if (!OVTransport::IsFlying(Pod.State)) return;
    if (Pod.State == EOVPodFlightState::Outbound && (!Usable(Pod.Target.Get()) || Pod.Target->Systems->FactionId == Pod.FactionId)) BeginReturn(Pod);
    if (Pod.State == EOVPodFlightState::Returning && (!Usable(Pod.ReturnDestination.Get()) || Pod.ReturnDestination->Systems->FactionId != Pod.FactionId)) BeginReturn(Pod);
    if (!OVTransport::IsFlying(Pod.State)) return;
    AOVShip* Destination = Pod.State == EOVPodFlightState::Outbound ? Pod.Target.Get() : Pod.ReturnDestination.Get();
    if (!Usable(Destination)) return;
    const FVector Direction = (Destination->GetActorLocation() - Pod.Position).GetSafeNormal();
    const FVector End = Pod.Position + Direction * FMath::Min(FVector::Distance(Pod.Position, Destination->GetActorLocation()), Settings.PodSpeed * Delta);
    float Fraction; FVector Normal;
    if (UOVCombatSubsystem::SegmentBox(Pod.Position, End, Destination->GetActorTransform(), Destination->Systems->Layout->LocalHalfExtent, Fraction, Normal))
    {
        Pod.Position = FMath::Lerp(Pod.Position, End, Fraction);
        if (Pod.State == EOVPodFlightState::Returning)
        {
            Pod.State = EOVPodFlightState::Recovered;
            Person->State = EOVPassengerState::Aboard; Person->CurrentShip = Destination;
            LastMessage = TEXT("개인 포드 귀환 완료 (개체ID 보존)");
        }
        else if (Destination->Systems->bShieldActive && Destination->Systems->Shield > 0)
            DestroyPod(Pod);
        else
        {
            Pod.State = EOVPodFlightState::Attached;
            Enter(*Person, Destination, Pod.Position, Normal);
        }
    }
    else Pod.Position = End;
}

bool UOVTransportSubsystem::RegisterShuttle(AOVShip* Ship, AOVShip* Home)
{
    if (!bEnabled || !Usable(Ship) || !Ship->Systems->Layout->bSmallHull ||
        Shuttles.ContainsByPredicate([Ship](const FOVShuttleRecord& S) { return S.Ship == Ship; })) return false;
    // General ship status is preserved: this actor is never a PD target.
    for (const FOVWeaponMountDefinition& Mount : Ship->Systems->Layout->WeaponMounts)
        if (Mount.Type != EOVWeaponType::Interception) return false;
    FOVShuttleRecord Shuttle;
    Shuttle.Ship = Ship; Shuttle.Home = Home; Shuttle.FactionId = Ship->Systems->FactionId;
    Shuttle.HomePosition = Ship->GetActorLocation();
    Shuttles.Add(Shuttle);
    return true;
}

bool UOVTransportSubsystem::DispatchShuttle(AOVShip* Ship, AOVShip* Target, const TArray<int32>& Ids)
{
    FOVShuttleRecord* Shuttle = Shuttles.FindByPredicate([Ship](const FOVShuttleRecord& S) { return S.Ship == Ship; });
    if (!bEnabled || !Shuttle || Shuttle->State != EOVShuttleState::Ready || !Usable(Ship) || !Usable(Target) ||
        Ship->Systems->FactionId == Target->Systems->FactionId || Ids.IsEmpty() || Ids.Num() > Settings.ShuttleCapacity)
    { LastMessage = TEXT("강습선 출발 불가 · 준비 상태/표적/정원 확인"); return false; }
    int32 Heroes = 0; TSet<int32> Seen;
    for (int32 Id : Ids)
    {
        FOVPassengerToken* Person = FindPerson(Id);
        if (!Person || Seen.Contains(Id) || Person->State != EOVPassengerState::Aboard || Person->FactionId != Ship->Systems->FactionId)
        { LastMessage = TEXT("출발 불가 · 승객은 동일 소속의 대기 인원이어야 함"); return false; }
        if (Person->CurrentShip != Ship && (Person->CurrentShip != Shuttle->Home || !Usable(Shuttle->Home.Get()) ||
            FVector::Distance(Ship->GetActorLocation(), Shuttle->Home->GetActorLocation()) > Settings.PodLaunchRange))
        { LastMessage = TEXT("출발 불가 · 승객 원함이 탑승 범위 밖"); return false; }
        Seen.Add(Id); Heroes += (Person->bHeroTypeDEV || Person->bRuntimeHero) ? 1 : 0;
    }
    if (Heroes > Settings.ShuttleHeroCapacity) { LastMessage = TEXT("출발 불가 · 영웅 탑승 정원 초과"); return false; }
    Shuttle->PassengerIds = Ids; Shuttle->Target = Target; Shuttle->FactionId = Ship->Systems->FactionId;
    Shuttle->State = EOVShuttleState::Approaching; Shuttle->Progress = 0; Shuttle->HomePosition = Ship->GetActorLocation();
    Shuttle->bManualSuspended = false;
    for (int32 Id : Ids) { auto& Person = *FindPerson(Id); Person.State = EOVPassengerState::InShuttle; Person.CurrentShip = Ship; Person.Shuttle = Ship; Person.PodId = 0; }
    Ship->SetAutomation(false, false);
    LastMessage = TEXT("강습선 접근 시작 (공격 무장 없음)");
    return true;
}

bool UOVTransportSubsystem::SuspendShuttle(AOVShip* Ship)
{
    FOVShuttleRecord* Shuttle = Shuttles.FindByPredicate([Ship](const FOVShuttleRecord& S) { return S.Ship == Ship; });
    if (!bEnabled || !Shuttle || Shuttle->State == EOVShuttleState::Ready || Shuttle->State == EOVShuttleState::Lost) return false;
    Shuttle->bManualSuspended = true;
    Shuttle->Reason = LastMessage = TEXT("수동 이동/자세/정지 우선 · 수송선 회수 명령으로 운송 재개");
    return true;
}

bool UOVTransportSubsystem::RecallShuttle(AOVShip* Ship)
{
    FOVShuttleRecord* Shuttle = Shuttles.FindByPredicate([Ship](const FOVShuttleRecord& S) { return S.Ship == Ship; });
    if (!bEnabled || !Shuttle || !Usable(Ship) || Shuttle->FactionId != Ship->Systems->FactionId ||
        Shuttle->State == EOVShuttleState::Lost || Shuttle->State == EOVShuttleState::Ready)
    { LastMessage = TEXT("회수 불가 · 원 강습선 상태/소속/진행 작전 확인"); return false; }
    Shuttle->bManualSuspended = false;
    if (Shuttle->State == EOVShuttleState::Delivered || Shuttle->State == EOVShuttleState::RecoveryBlocked)
    { Shuttle->State = EOVShuttleState::Recovering; Shuttle->Progress = 0; }
    else if (Shuttle->State == EOVShuttleState::Approaching || Shuttle->State == EOVShuttleState::Docking)
        Shuttle->State = EOVShuttleState::Returning;
    LastMessage = TEXT("강습선 회수 요청: 원 진입점 승객 대기");
    return true;
}

void UOVTransportSubsystem::AdvanceShuttle(FOVShuttleRecord& Shuttle, float Delta)
{
    AOVShip* Ship = Shuttle.Ship.Get();
    if (Shuttle.State == EOVShuttleState::Lost) return;
    if (!Usable(Ship) || Ship->Systems->FactionId != Shuttle.FactionId)
    {
        const bool Destroyed = !IsValid(Ship) || !Ship->Systems || Ship->Systems->bWrecked;
        Shuttle.State = EOVShuttleState::Lost;
        for (int32 Id : Shuttle.PassengerIds)
        {
            FOVPassengerToken* Person = FindPerson(Id);
            if (Person && Person->State == EOVPassengerState::InShuttle)
            {
                // R18 unresolved capture: retained affiliation, no fake prisoner success or free escape.
                if (Destroyed) { Person->State = (Person->bHeroTypeDEV || Person->bRuntimeHero) ? EOVPassengerState::RescueWaiting : EOVPassengerState::Dead; Person->CurrentShip = nullptr; }
                else Person->State = EOVPassengerState::AwaitingRecovery;
            }
        }
        Shuttle.Reason = Destroyed ? TEXT("수송선 완파; 내부 진입자는 영향 없음") : TEXT("수송선 이탈/소속 변경; 회수 불가, 포로 처리는 미지원");
        return;
    }
    // Explicit manual orders suspend only automation, never ownership/loss or passenger accounting.
    if (Shuttle.bManualSuspended) return;
    if (Shuttle.State == EOVShuttleState::Ready || Shuttle.State == EOVShuttleState::Delivered || Shuttle.State == EOVShuttleState::RecoveryBlocked) return;
    AOVShip* Target = Shuttle.Target.Get();
    if (Shuttle.State != EOVShuttleState::Returning && !Usable(Target)) Shuttle.State = EOVShuttleState::Returning;
    if (Shuttle.State == EOVShuttleState::Returning)
    {
        AOVShip* Home = FindReturn(Shuttle.Home.Get(), Shuttle.FactionId, Ship->GetActorLocation(), Ship);
        const FVector Goal = Home && Home != Ship ? Home->GetActorLocation() : Shuttle.HomePosition;
        const float Radius = Home && Home != Ship ?
            (Home->Systems->Layout->LocalHalfExtent * Home->GetActorScale3D()).Size() +
            (Ship->Systems->Layout->LocalHalfExtent * Ship->GetActorScale3D()).Size() + 100 : Settings.DockTolerance;
        if (FVector::Distance(Ship->GetActorLocation(), Goal) <= Radius)
        {
            Ship->StopMovement(); Shuttle.State = EOVShuttleState::Ready; Shuttle.Reason = TEXT("강습선 귀환 완료");
            for (int32 Id : Shuttle.PassengerIds) if (FOVPassengerToken* Person = FindPerson(Id))
                if (Person->State == EOVPassengerState::InShuttle) { Person->State = EOVPassengerState::Aboard; Person->CurrentShip = Ship; }
            Shuttle.PassengerIds.Reset();
        }
        else Ship->SetDestination(Goal);
        return;
    }
    if (!Usable(Target)) return;
    FVector Direction = (Ship->GetActorLocation() - Target->GetActorLocation()).GetSafeNormal(SMALL_NUMBER, FVector::ForwardVector);
    if (Shuttle.State == EOVShuttleState::Recovering)
        Direction = Target->GetActorTransform().TransformVector(Shuttle.DockLocalPoint).GetSafeNormal(SMALL_NUMBER, FVector::ForwardVector);
    // Keep center outside existing conservative contact spheres; surface point is independently projected.
    const float Separation = (Target->Systems->Layout->LocalHalfExtent * Target->GetActorScale3D()).Size() +
        (Ship->Systems->Layout->LocalHalfExtent * Ship->GetActorScale3D()).Size() + 150;
    const FVector Goal = Target->GetActorLocation() + Direction * Separation;
    if (FVector::Distance(Ship->GetActorLocation(), Goal) > Settings.DockTolerance)
    { Ship->SetDestination(Goal); Shuttle.Progress = 0; return; }
    Ship->StopMovement();
    if (Target->Systems->bShieldActive && Target->Systems->Shield > 0 && Target->Systems->FactionId != Shuttle.FactionId)
    {
        Ship->Systems->Shield = FMath::Max(0.f, Ship->Systems->Shield - Settings.ShieldContactDamage * Delta);
        Ship->Systems->RefreshDerivedState(); Shuttle.Progress = 0; Shuttle.Reason = TEXT("적 활성 쉴드: 진입 불가 / 강습선 쉴드 소모"); return;
    }
    if (Shuttle.State == EOVShuttleState::Approaching) Shuttle.State = EOVShuttleState::Docking;
    if (Shuttle.State == EOVShuttleState::Recovering)
    {
        const bool AllWaiting = Shuttle.PassengerIds.ContainsByPredicate([&](int32 Id) { const FOVPassengerToken* P = FindPerson(Id); return P && P->State == EOVPassengerState::InsideTarget; }) == false;
        if (!AllWaiting)
        {
            const bool RuntimeWaiting = Shuttle.PassengerIds.ContainsByPredicate([&](int32 Id) {
                const auto* P = FindPerson(Id); return P && P->RuntimeUnitId > 0 && P->State == EOVPassengerState::InsideTarget; });
            Shuttle.Reason = RuntimeWaiting ? TEXT("선내 인원의 실제 원 진입점 이동 대기") : TEXT("DEV 토큰의 원 진입점 복귀 확인 대기"); return;
        }
    }
    Shuttle.Progress += Delta;
    if (Shuttle.Progress < Settings.DockingSeconds) return;
    float Fraction; FVector Normal;
    const FVector RayStart = Target->GetActorLocation() + Direction * Separation;
    if (!UOVCombatSubsystem::SegmentBox(RayStart, Target->GetActorLocation(), Target->GetActorTransform(), Target->Systems->Layout->LocalHalfExtent, Fraction, Normal)) return;
    const FVector Surface = Shuttle.State == EOVShuttleState::Recovering ?
        Target->GetActorTransform().TransformPosition(Shuttle.DockLocalPoint) : FMath::Lerp(RayStart, Target->GetActorLocation(), Fraction);
    if (Shuttle.State != EOVShuttleState::Recovering)
    {
        Shuttle.DockLocalPoint = Target->GetActorTransform().InverseTransformPosition(Surface);
        Shuttle.DockLocalNormal = Target->GetActorTransform().InverseTransformVectorNoScale(Normal);
    }
    for (int32 Id : Shuttle.PassengerIds)
    {
        FOVPassengerToken* Person = FindPerson(Id);
        if (!Person) continue;
        if (Shuttle.State == EOVShuttleState::Docking && Person->State == EOVPassengerState::InShuttle) Enter(*Person, Target, Surface, Normal);
        else if (Shuttle.State == EOVShuttleState::Recovering && Person->State == EOVPassengerState::AwaitingRecovery && Person->CurrentShip == Target)
        { Person->State = EOVPassengerState::InShuttle; Person->CurrentShip = Ship; }
    }
    Shuttle.State = Shuttle.State == EOVShuttleState::Docking ? EOVShuttleState::Delivered : EOVShuttleState::Returning;
    Shuttle.Progress = 0; RefreshPresence();
    const bool HasRuntimePassenger = Shuttle.PassengerIds.ContainsByPredicate([&](int32 Id) {
        const auto* P = FindPerson(Id); return P && P->RuntimeUnitId > 0; });
    Shuttle.Reason = Shuttle.State == EOVShuttleState::Delivered ?
        (HasRuntimePassenger ? TEXT("실제 선내 이송 완료 · 내부 아군 보호 활성") : TEXT("DEV 이송 완료 · 잔류 보호 활성")) :
        TEXT("같은 강습선 회수 완료: 귀환 중");
}

void UOVTransportSubsystem::Simulate(float GameDelta)
{
    if (!bEnabled || !FMath::IsFinite(GameDelta) || GameDelta <= 0) return;
    Accumulator += GameDelta;
    while (Accumulator + 1.e-7 >= OVTransport::Step)
    { Step(OVTransport::Step); Accumulator -= OVTransport::Step; if (Accumulator < 0) Accumulator = 0; }
}

void UOVTransportSubsystem::Step(float Delta)
{
    GameTime += Delta;
    for (FOVPassengerToken& Person : Persons)
    {
        if (Person.State == EOVPassengerState::InsideTarget || Person.State == EOVPassengerState::AwaitingRecovery)
        {
            AOVShip* Target = Person.CurrentShip.Get();
            if (!IsValid(Target) || !Target->Systems || Target->Systems->bWrecked)
            { Person.State = EOVPassengerState::Dead; Person.CurrentShip = nullptr; }
        }
        else if (Person.State == EOVPassengerState::Aboard && (!Person.CurrentShip.IsValid() || Person.CurrentShip->Systems->bWrecked))
        { Person.State = EOVPassengerState::Dead; Person.CurrentShip = nullptr; }
    }
    for (FOVPodState& Pod : Pods) AdvancePod(Pod, Delta);
    for (FOVShuttleRecord& Shuttle : Shuttles) AdvanceShuttle(Shuttle, Delta);
    RefreshPresence();
}

FOVPassengerToken UOVTransportSubsystem::GetPerson(int32 Id) const
{
    const FOVPassengerToken* Person = Persons.FindByPredicate([Id](const FOVPassengerToken& P) { return P.PersonId == Id; });
    return Person ? *Person : FOVPassengerToken();
}

int32 UOVTransportSubsystem::CountState(EOVPassengerState State) const
{
    int32 Count = 0;
    for (const FOVPassengerToken& Person : Persons) if (Person.State == State) ++Count;
    return Count;
}

FString UOVTransportSubsystem::GetSummary() const
{
    return FString::Printf(TEXT("DEV 승객 %d | 비행 %d | 내부 %d | 회수대기 %d | 구조 %d | 사망 %d"),
        Persons.Num(), CountState(EOVPassengerState::InPod), CountState(EOVPassengerState::InsideTarget),
        CountState(EOVPassengerState::AwaitingRecovery), CountState(EOVPassengerState::RescueWaiting), CountState(EOVPassengerState::Dead));
}
