#include "OVLogisticsSubsystem.h"
#include "OVShip.h"
#include "OVShipSystemsComponent.h"

namespace
{
bool Active(const AOVShip* Ship)
{
    return IsValid(Ship) && Ship->Systems && !Ship->Systems->bWrecked && !Ship->Systems->bNeutral && Ship->Systems->CanUseResources();
}
bool Close(const AOVShip* A, const AOVShip* B, float Range)
{
    return FMath::IsFinite(Range) && Range >= 0 && FVector::Dist(A->GetActorLocation(), B->GetActorLocation()) <= Range;
}
}
bool UOVLogisticsSubsystem::RegisterDevShip(AOVShip* Ship, EOVDevLogisticsRole Role, float Capacity, float InitialAmount)
{
    if (!IsValid(Ship) || !FMath::IsFinite(Capacity) || !FMath::IsFinite(InitialAmount) || Capacity < 0 || InitialAmount < 0 || InitialAmount > Capacity || Cargo.ContainsByPredicate([Ship](const auto& C) { return C.Ship == Ship; })) return false;
    auto& Item = Cargo.AddDefaulted_GetRef(); Item.Ship = Ship; Item.Role = Role; Item.Capacity = Capacity; Item.Amount = InitialAmount; return true;
}
void UOVLogisticsSubsystem::RemoveDevShip(AOVShip* Ship)
{
    RepairLinks.RemoveAll([Ship](const auto& L) { return L.Source == Ship || L.Target == Ship; });
    Cargo.RemoveAll([Ship](const auto& C) { return C.Ship == Ship; });
}
int32 UOVLogisticsSubsystem::AddDevNode(FVector Position, float Amount)
{
    if (Position.ContainsNaN() || !FMath::IsFinite(Amount) || Amount < 0) return 0;
    auto& Node = Nodes.AddDefaulted_GetRef(); Node.Id = NextNodeId++; Node.Position = Position; Node.Amount = Amount; return Node.Id;
}
float UOVLogisticsSubsystem::DevMine(AOVShip* Ship, int32 NodeId, float Requested, float MaxRange)
{
    auto* Item = Cargo.FindByPredicate([Ship](const auto& C) { return C.Ship == Ship; });
    auto* Node = Nodes.FindByPredicate([NodeId](const auto& N) { return N.Id == NodeId; });
    if (!Item || Item->Role != EOVDevLogisticsRole::Mining || !Node || !Active(Ship) || !FMath::IsFinite(Requested) || Requested <= 0 || !FMath::IsFinite(MaxRange) || MaxRange < 0 || FVector::Dist(Ship->GetActorLocation(), Node->Position) > MaxRange) { LastReason = TEXT("채굴 조건 불충족"); return 0; }
    const float Amount = FMath::Min3(Requested, Node->Amount, Item->Capacity - Item->Amount - 3 * Item->Advanced - 8 * Item->Rare);
    Node->Amount -= Amount; Item->Amount += Amount; LastReason = Amount > 0 ? TEXT("채굴 완료") : TEXT("자원 또는 적재 공간 부족"); return Amount;
}
float UOVLogisticsSubsystem::DevTransfer(AOVShip* From, AOVShip* To, float Requested, float MaxRange)
{
    auto* Source = Cargo.FindByPredicate([From](const auto& C) { return C.Ship == From; });
    auto* Dest = Cargo.FindByPredicate([To](const auto& C) { return C.Ship == To; });
    if (!Source || !Dest || From == To || (Source->Role != EOVDevLogisticsRole::Transport && Dest->Role != EOVDevLogisticsRole::Transport) || !Active(From) || !Active(To) || From->Systems->FactionId != To->Systems->FactionId || !Close(From, To, MaxRange) || !FMath::IsFinite(Requested) || Requested <= 0) { LastReason = TEXT("수송 조건 불충족"); return 0; }
    const float Amount = FMath::Min3(Requested, Source->Amount, Dest->Capacity - Dest->Amount - 3 * Dest->Advanced - 8 * Dest->Rare);
    Source->Amount -= Amount; Dest->Amount += Amount; LastReason = Amount > 0 ? TEXT("수송 완료") : TEXT("재고 또는 적재 공간 부족"); return Amount;
}
float UOVLogisticsSubsystem::DevRepairHull(AOVShip* RepairShip, AOVShip* Target, float RequestedHP, float CostPerHP, float MaxRange)
{
    auto* Source = Cargo.FindByPredicate([RepairShip](const auto& C) { return C.Ship == RepairShip; });
    if (RepairLinks.ContainsByPredicate([Target](const auto& L) { return L.Target == Target; })) { LastReason = TEXT("대상에 진행 중인 수리 링크가 있음"); return 0; }
    if (!Source || Source->Role != EOVDevLogisticsRole::Repair || RepairShip == Target || !Active(RepairShip) || !IsValid(Target) || !Target->Systems || Target->Systems->bWrecked || Target->Systems->bNeutral || RepairShip->Systems->FactionId != Target->Systems->FactionId || !Close(RepairShip, Target, MaxRange) || !FMath::IsFinite(RequestedHP) || RequestedHP <= 0 || !FMath::IsFinite(CostPerHP) || CostPerHP <= 0) { LastReason = TEXT("수리 조건 불충족"); return 0; }
    const float HP = FMath::Min3(RequestedHP, Source->Amount / CostPerHP, Target->Systems->MaxHullHP - Target->Systems->HullHP);
    Source->Amount -= HP * CostPerHP; Target->Systems->HullHP += HP; LastReason = HP > 0 ? TEXT("선체 수리 완료") : TEXT("자원 또는 손상 부족"); return HP;
}

namespace
{
float ResourceLimit(const FOVDevCargo& Cargo, const FVector& Cost)
{
    float HP = TNumericLimits<float>::Max();
    if (Cost.X > 0) HP = FMath::Min(HP, float(Cargo.Amount / Cost.X));
    if (Cost.Y > 0) HP = FMath::Min(HP, float(Cargo.Advanced / Cost.Y));
    if (Cost.Z > 0) HP = FMath::Min(HP, float(Cargo.Rare / Cost.Z));
    return HP;
}
float SurfaceDistance(const AOVShip* A, const AOVShip* B)
{
    if (A == B) return 0;
    const FVector Diff = B->GetActorLocation() - A->GetActorLocation();
    const double Distance = Diff.Size();
    if (Distance <= UE_SMALL_NUMBER) return 0;
    const FVector Direction = Diff / Distance;
    auto Radius = [&](const AOVShip* Ship) {
        if (!Ship->Systems || !Ship->Systems->Layout) return 0.0;
        const FVector Local = Ship->GetActorQuat().UnrotateVector(Direction);
        const FVector Extent = Ship->Systems->Layout->LocalHalfExtent * Ship->GetActorScale3D().GetAbs();
        return FMath::Abs(Local.X) * Extent.X + FMath::Abs(Local.Y) * Extent.Y + FMath::Abs(Local.Z) * Extent.Z;
    };
    // Projected box separation is a DEV approximation, pending exact surface-distance geometry.
    return float(FMath::Max(0.0, Distance - Radius(A) - Radius(B)));
}
}
bool UOVLogisticsSubsystem::SetDevMaterials(AOVShip* Ship, FVector Materials)
{
    FOVDevCargo* C = Cargo.FindByPredicate([Ship](const auto& Item) { return Item.Ship == Ship; });
    if (!C || Materials.ContainsNaN() || Materials.X < 0 || Materials.Y < 0 || Materials.Z < 0 || Materials.X + 3 * Materials.Y + 8 * Materials.Z > C->Capacity) return false;
    C->Amount = Materials.X; C->Advanced = Materials.Y; C->Rare = Materials.Z; return true;
}
bool UOVLogisticsSubsystem::StartDevRepair(AOVShip* Source, AOVShip* Target, bool bHero, FGuid HeroId, FVector BaseCostPerHP, float Range, float HeroRate, float HeroDuration, float HeroTotalHP)
{
    FOVDevCargo* C = Cargo.FindByPredicate([Source](const auto& Item) { return Item.Ship == Source; });
    const bool ValidQuote = !BaseCostPerHP.ContainsNaN() && BaseCostPerHP.X >= 0 && BaseCostPerHP.Y >= 0 && BaseCostPerHP.Z >= 0 && !BaseCostPerHP.IsNearlyZero();
    if (!C || !Active(Source) || !IsValid(Target) || !Target->Systems || Target->Systems->bWrecked || Target->Systems->bNeutral || Source->Systems->FactionId != Target->Systems->FactionId || !ValidQuote || !FMath::IsFinite(Range) || Range < 0 || Range > 12000 || SurfaceDistance(Source, Target) > Range || Target->Systems->HullHP >= Target->Systems->MaxHullHP || ResourceLimit(*C, BaseCostPerHP * 1.5) <= 0 || (!bHero && (C->Role != EOVDevLogisticsRole::Repair || Source == Target)) || (bHero && (!HeroId.IsValid() || Source != Target || !FMath::IsFinite(HeroRate) || HeroRate <= 0 || !FMath::IsFinite(HeroDuration) || HeroDuration <= 0 || HeroDuration > 10 || !FMath::IsFinite(HeroTotalHP) || HeroTotalHP <= 0))) { LastReason = TEXT("수리 링크 조건 또는 DEV 견적 불충족"); return false; }
    const float Rate = bHero ? HeroRate : .5f * FMath::Max(64.f, .001f * Target->Systems->MaxHullHP);
    FOVDevRepairLink* Existing = RepairLinks.FindByPredicate([Target](const auto& L) { return L.Target == Target; });
    if (Existing && Existing->Rate >= Rate) { LastReason = TEXT("더 빠르거나 같은 기존 수리 링크가 있음"); return false; }
    if (RepairLinks.ContainsByPredicate([Source, Target, bHero](const auto& L) { return !bHero && !L.bHero && L.Source == Source && L.Target != Target; })) { LastReason = TEXT("수리함은 동시에 한 대상만 작업"); return false; }
    if (Existing) RepairLinks.RemoveAll([Target](const auto& L) { return L.Target == Target; });
    auto& L = RepairLinks.AddDefaulted_GetRef(); L.Source = Source; L.Target = Target; L.HeroId = HeroId; L.bHero = bHero; L.Faction = Source->Systems->FactionId; L.BaseCostPerHP = BaseCostPerHP; L.Range = Range; L.Rate = Rate;
    L.SetupRemaining = bHero ? 0 : 3; L.TimeRemaining = bHero ? HeroDuration : TNumericLimits<float>::Max(); L.HPRemaining = bHero ? HeroTotalHP : TNumericLimits<float>::Max(); L.State = bHero ? TEXT("수리 중") : TEXT("연결 준비"); LastReason = L.State; return true;
}
bool UOVLogisticsSubsystem::CancelDevRepair(AOVShip* Target, FGuid HeroId)
{
    return RepairLinks.RemoveAll([Target, HeroId](const auto& L) { return L.Target == Target && (L.bHero ? L.HeroId == HeroId : !HeroId.IsValid()); }) > 0;
}
void UOVLogisticsSubsystem::Simulate(float GameDelta)
{
    if (!FMath::IsFinite(GameDelta) || GameDelta <= 0) return;
    for (int32 I = RepairLinks.Num() - 1; I >= 0; --I)
    {
        auto& L = RepairLinks[I]; auto* C = Cargo.FindByPredicate([&L](const auto& Item) { return Item.Ship == L.Source; });
        if (!C || !Active(L.Source) || !IsValid(L.Target) || !L.Target->Systems || L.Target->Systems->bWrecked || L.Target->Systems->bNeutral || L.Source->Systems->FactionId != L.Faction || L.Target->Systems->FactionId != L.Faction || SurfaceDistance(L.Source, L.Target) > L.Range) { LastReason = TEXT("수리 링크 조건 상실"); RepairLinks.RemoveAt(I); continue; }
        float Delta = GameDelta;
        const float Setup = FMath::Min(Delta, L.SetupRemaining); L.SetupRemaining -= Setup; Delta -= Setup;
        if (Delta <= 0) continue;
        L.State = TEXT("수리 중");
        const FVector Cost = L.BaseCostPerHP * 1.5;
        const float Time = FMath::Min(Delta, L.TimeRemaining);
        const float HP = FMath::Min(FMath::Min3(L.Rate * Time, L.HPRemaining, L.Target->Systems->MaxHullHP - L.Target->Systems->HullHP), ResourceLimit(*C, Cost));
        if (HP > 0) { C->Amount = FMath::Max(0.f, float(C->Amount - HP * Cost.X)); C->Advanced = FMath::Max(0.f, float(C->Advanced - HP * Cost.Y)); C->Rare = FMath::Max(0.f, float(C->Rare - HP * Cost.Z)); L.Target->Systems->HullHP += HP; L.HPRemaining -= HP; }
        L.TimeRemaining -= Time;
        if (HP <= 0 || L.HPRemaining <= KINDA_SMALL_NUMBER || L.TimeRemaining <= KINDA_SMALL_NUMBER || L.Target->Systems->HullHP >= L.Target->Systems->MaxHullHP || ResourceLimit(*C, Cost) <= KINDA_SMALL_NUMBER) { LastReason = TEXT("수리 완료 또는 자원 소진"); RepairLinks.RemoveAt(I); }
    }
}
