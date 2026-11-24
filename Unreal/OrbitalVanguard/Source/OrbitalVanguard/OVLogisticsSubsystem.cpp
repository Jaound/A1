#include "OVLogisticsSubsystem.h"
#include "OVShip.h"
#include "OVShipSystemsComponent.h"

namespace
{
#include "OVFieldCargo.inl"
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
    if(auto* Party=DamageParties.Find(Ship)) ReleaseParty(Ship,*Party);
    DamageParties.Remove(Ship); RepairOutcome.Remove(Ship);
    Approaches.Remove(Ship);
    for(auto It=Approaches.CreateIterator();It;++It) if(It.Value()==Ship) It.RemoveCurrent();
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
const FOVDevCargo* UOVLogisticsSubsystem::FindCargo(const AOVShip* Ship) const
{
    return Cargo.FindByPredicate([Ship](const FOVDevCargo& Item) { return Item.Ship == Ship; });
}

float UOVLogisticsSubsystem::UnloadTransport(AOVShip* Ship, int32 DepotFaction, FVector Depot, float Range, float Requested)
{
    if (!Active(Ship) || Ship->Systems->FactionId != DepotFaction || Depot.ContainsNaN() ||
        !FMath::IsFinite(Range) || Range <= 0 || !FMath::IsFinite(Requested) || Requested <= 0 ||
        FVector::DistSquared(Ship->GetActorLocation(), Depot) > FMath::Square(Range)) return 0;
    auto* Item = Cargo.FindByPredicate([Ship](const FOVDevCargo& Entry) { return Entry.Ship == Ship; });
    if (!Item || Item->Role != EOVDevLogisticsRole::Transport) return 0;
    const float Amount = FMath::Min(Requested, Item->Amount);
    Item->Amount -= Amount;
    return Amount;
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
float UOVLogisticsSubsystem::FieldCapacity(FName Hull)
{
    if(Hull==TEXT("repair"))return RepairStock.X+3*RepairStock.Y+8*RepairStock.Z;
    return Hull==TEXT("mining") ? MiningStock : TransportStock;
}
bool UOVLogisticsSubsystem::StockFieldShip(AOVShip* Ship, int32 ReplacementStock)
{
    if (!Ship || !Ship->Systems || !Ship->Systems->HasPhysicalFacility(EOVFacilityType::ResourceStorage)) return false;
    const FName Id=Ship->HullDefinitionId;
    const bool Repair=Id==TEXT("repair"), Mining=Id==TEXT("mining");
    if(Ship->Systems->Layout->bAllowAircraftOperations)
    {
        const int32 Bays=Ship->Systems->Layout->Slots.FilterByPredicate([](const auto& S){ return S.Facility==EOVFacilityType::Hangar; }).Num();
        const int32 Kits=ReplacementStock<0 ? Bays*25 : ReplacementStock;
        const FVector Materials=AircraftKitCost*Kits;
        return RegisterDevShip(Ship,EOVDevLogisticsRole::Carrier,FMath::Max(Bays*25,Kits)*(AircraftKitCost.X+3*AircraftKitCost.Y),0)
            && SetDevMaterials(Ship,Materials);
    }
    const FVector Stock=Repair ? RepairStock : FVector(FieldCapacity(Id),0,0);
    const float Capacity=Stock.X+3*Stock.Y+8*Stock.Z;
    return RegisterDevShip(Ship,Repair ? EOVDevLogisticsRole::Repair : Mining ? EOVDevLogisticsRole::Mining : EOVDevLogisticsRole::Transport,Capacity,0) && SetDevMaterials(Ship,Stock);
}
int32 UOVLogisticsSubsystem::AircraftKits(const AOVShip* Ship) const
{
    const auto* C=FindCargo(Ship);
    return C ? FMath::FloorToInt(ResourceLimit(*C,AircraftKitCost)) : 0;
}
bool UOVLogisticsSubsystem::ConsumeAircraftKit(AOVShip* Ship)
{
    if(!Active(Ship) || AircraftKits(Ship)<1) return false;
    const auto* C=FindCargo(Ship);
    return SetDevMaterials(Ship,FVector(C->Amount,C->Advanced,C->Rare)-AircraftKitCost);
}
void UOVLogisticsSubsystem::LoseStoredCargo(AOVShip* Ship)
{
    SetDevMaterials(Ship,FVector::ZeroVector);
}
int32 UOVLogisticsSubsystem::ResupplyCarrier(AOVShip* Ship)
{
    if(!Active(Ship) || !Ship->Systems->Layout->bAllowAircraftOperations || Ship->bFTLCharging || Ship->IsHidden()) return 0;
    // Legacy missions can supply an empty physical bay without minting cargo.
    if(!FindCargo(Ship)) StockFieldShip(Ship,0);
    const auto* Dest=FindCargo(Ship);
    if(!Dest || Dest->Role!=EOVDevLogisticsRole::Carrier) return 0;
    int32 Received=0;
    for(auto& Source:Cargo)
    {
        if(Source.Role!=EOVDevLogisticsRole::Repair || !Active(Source.Ship) || Source.Ship->IsHidden() || Source.Ship->bFTLCharging ||
           Source.Ship->Systems->FactionId!=Ship->Systems->FactionId || SurfaceDistance(Source.Ship,Ship)>12000) continue;
        const double Free=Dest->Capacity-Dest->Amount-3*Dest->Advanced-8*Dest->Rare;
        const int32 Kits=FMath::Min(AircraftKits(Source.Ship),FMath::FloorToInt(Free/(AircraftKitCost.X+3*AircraftKitCost.Y)));
        if(Kits<=0) continue;
        const FVector Load=Kits*AircraftKitCost;
        if(!SetDevMaterials(Ship,FVector(Dest->Amount,Dest->Advanced,Dest->Rare)+Load)) continue;
        Source.Amount-=Load.X; Source.Advanced-=Load.Y; Received+=Kits;
    }
    return Received;
}
namespace
{
int32 RepairPriority(EOVFacilityType Type)
{
    switch(Type)
    {
    case EOVFacilityType::Core:
    case EOVFacilityType::Bridge: return 0;
    case EOVFacilityType::LifeSupport: return 1;
    case EOVFacilityType::ShieldControl: return 2;
    case EOVFacilityType::Engine: return 3;
    case EOVFacilityType::Interception: return 4;
    case EOVFacilityType::WeaponControl: return 5;
    default: return 6;
    }
}
int32 DamagedRoom(const UOVShipSystemsComponent& S,bool OnlyWrecked)
{
    if(!S.Layout || S.bWrecked)return INDEX_NONE;
    int32 Best=INDEX_NONE,Rank=MAX_int32;
    for(int32 I=0;I<S.Slots.Num();++I)
    {
        const auto& Room=S.Layout->Slots[I]; const auto& State=S.Slots[I];
        if(Room.Facility==EOVFacilityType::None || (Room.Facility==EOVFacilityType::Core && State.HP<=0) ||
            (State.OccupyingFaction>=0 && State.OccupyingFaction!=S.FactionId) ||
            State.HP>=Room.MaxFacilityHP || (OnlyWrecked && State.HP>0))continue;
        const int32 Priority=RepairPriority(Room.Facility)+(State.HP>0?10:0);
        if(Priority<Rank){ Rank=Priority; Best=I; }
    }
    return Best;
}
int32 DamagedMount(const UOVShipSystemsComponent& S)
{
    if (!S.Layout) return INDEX_NONE;
    for (int32 I=0; I<S.WeaponMountStates.Num() && I<S.Layout->WeaponMounts.Num(); ++I)
        if (S.WeaponMountStates[I].HP < S.Layout->WeaponMounts[I].MaxHP) return I;
    return INDEX_NONE;
}
bool NeedsService(const UOVShipSystemsComponent& S)
{
    return S.HullHP<S.MaxHullHP || DamagedRoom(S,false)!=INDEX_NONE || DamagedMount(S)!=INDEX_NONE;
}
float FieldRate(const UOVShipSystemsComponent& S){ return .5f*FMath::Max(64.f,.001f*S.MaxHullHP); }
FVector RoomCost(FVector HullCost)
{
    return FVector(HullCost.X+3*HullCost.Y+8*HullCost.Z,0,0);
}
float RepairRoom(UOVShipSystemsComponent& S,int32 I,float Budget)
{
    auto& State=S.Slots[I]; const float Max=S.Layout->Slots[I].MaxFacilityHP;
    const bool Rebuilding=State.HP<=0;
    float& Work=Rebuilding?State.RebuildHP:State.HP;
    const float Before=Work;
    Work=FMath::Min(Max,Before+Budget);
    const float Actual=Work-Before;
    if(Rebuilding && Work>=Max){ State.HP=Max; State.RebuildHP=0; S.RefreshDerivedState(); }
    return Actual;
}
void Spend(FOVDevCargo& C,FVector Bill)
{
    C.Amount=FMath::Max(0.f,float(C.Amount-Bill.X)); C.Advanced=FMath::Max(0.f,float(C.Advanced-Bill.Y)); C.Rare=FMath::Max(0.f,float(C.Rare-Bill.Z));
}
}
void UOVLogisticsSubsystem::RepairBerthedHull(AOVShip* Host,AOVShip* Guest,float GameDelta,float RateScale)
{
    if(!Active(Host)||!IsValid(Guest)||Guest->HangarHost!=Host||!Guest->Systems||Guest->Systems->bWrecked||
       Guest->Systems->bNeutral||Guest->Systems->FactionId!=Host->Systems->FactionId)return;
    auto& S=*Guest->Systems;
    auto* Stock=Cargo.FindByPredicate([Host](const auto& C){return C.Ship==Host;});
    if(!Stock||S.MaxHullHP<=0||GameDelta<=0||RateScale<=0)return;
    const FVector Cost=PackageCost(Guest->HullDefinitionId)*(.45/S.MaxHullHP);
    if(Cost.IsNearlyZero())return;
    const float HP=FMath::Min3(S.MaxHullHP-S.HullHP,FieldRate(S)*RateScale*GameDelta,ResourceLimit(*Stock,Cost));
    if(HP>0){Spend(*Stock,Cost*HP);S.HullHP+=HP;}
}

void UOVLogisticsSubsystem::CancelFieldOrder(AOVShip* Source)
{
    RepairOutcome.Remove(Source); Approaches.Remove(Source);
    RepairLinks.RemoveAll([Source](const auto& Link){return !Link.bHero && Link.Source==Source;});
}
bool UOVLogisticsSubsystem::OrderRepair(AOVShip* Source,AOVShip* Target)
{
    const auto* C=FindCargo(Source);
    if(!C || C->Role!=EOVDevLogisticsRole::Repair || !Active(Source) || !IsValid(Target) || Source==Target ||
       !Target->Systems || Target->Systems->bWrecked || Target->Systems->bNeutral || Source->bFTLCharging || Target->bFTLCharging ||
       Source->Systems->FactionId!=Target->Systems->FactionId || !NeedsService(*Target->Systems) ||
       PackageCost(Target->HullDefinitionId).IsZero()) { LastReason=TEXT("수리 불가 · 손상된 아군 함선을 지정하세요"); return false; }
    CancelFieldOrder(Source);
    Source->SetAutomation(false,Source->IsAttackAutomatic());
    Approaches.Add(Source,Target);
    LastReason=TEXT("수리함 접근 중");
    return true;
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
    if (!C || !Active(Source) || !IsValid(Target) || !Target->Systems || Target->Systems->bWrecked || Target->Systems->bNeutral || Source->Systems->FactionId != Target->Systems->FactionId || !ValidQuote || !FMath::IsFinite(Range) || Range < 0 || Range > 12000 || SurfaceDistance(Source, Target) > Range || (bHero ? Target->Systems->HullHP >= Target->Systems->MaxHullHP : !NeedsService(*Target->Systems)) || ResourceLimit(*C, (!bHero && DamagedRoom(*Target->Systems,false)!=INDEX_NONE ? RoomCost(BaseCostPerHP) : BaseCostPerHP) * 1.5) <= 0 || (!bHero && (C->Role != EOVDevLogisticsRole::Repair || Source == Target)) || (bHero && (!HeroId.IsValid() || Source != Target || !FMath::IsFinite(HeroRate) || HeroRate <= 0 || !FMath::IsFinite(HeroDuration) || HeroDuration <= 0 || HeroDuration > 10 || !FMath::IsFinite(HeroTotalHP) || HeroTotalHP <= 0))) { LastReason = TEXT("수리 링크 조건 또는 DEV 견적 불충족"); return false; }
    const float Rate = bHero ? HeroRate : FieldRate(*Target->Systems);
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
    for(auto It=Approaches.CreateIterator();It;++It)
    {
        AOVShip* Source=It.Key().Get(); AOVShip* Target=It.Value().Get();
        if(!Active(Source) || !IsValid(Target) || !Target->Systems || Target->Systems->bWrecked ||
           Target->Systems->bNeutral || Target->Systems->FactionId!=Source->Systems->FactionId || Source->IsHidden() || Target->IsHidden() ||
           Source->bFTLCharging || Target->bFTLCharging || !NeedsService(*Target->Systems))
        { if(IsValid(Source))RepairOutcome.Add(Source,IsValid(Target) && Target->Systems && !Target->Systems->bWrecked && !NeedsService(*Target->Systems)?TEXT("수리 완료"):TEXT("수리 중단")); It.RemoveCurrent(); continue; }
        const FVector Quote=PackageCost(Target->HullDefinitionId)*(.45/Target->Systems->MaxHullHP);
        if(const auto* C=FindCargo(Source);!C || ResourceLimit(*C,DamagedRoom(*Target->Systems,false)!=INDEX_NONE?RoomCost(Quote):Quote)<=KINDA_SMALL_NUMBER){
            LastReason=TEXT("수리 자원 소진");RepairOutcome.Add(Source,LastReason);It.RemoveCurrent();continue;
        }
        const float Gap=SurfaceDistance(Source,Target);
        if(Gap>10500)
        {
            const FVector Toward=(Target->GetActorLocation()-Source->GetActorLocation()).GetSafeNormal();
            const FVector Point=Source->GetActorLocation()+Toward*(Gap-8500);
            if(!Source->bHasDestination || FVector::DistSquared(Point,Source->Destination)>FMath::Square(500.)) Source->SetDestination(Point);
            continue;
        }
        if(!Source->bHold)Source->StopMovement();
        if(RepairLinks.ContainsByPredicate([Source,Target](const auto& L){return !L.bHero && L.Source==Source && L.Target==Target;}))continue;
        if(!StartDevRepair(Source,Target,false,{},PackageCost(Target->HullDefinitionId)*(.30/Target->Systems->MaxHullHP),12000,0,0,0)){RepairOutcome.Add(Source,LastReason);It.RemoveCurrent();}
    }
    for (int32 I = RepairLinks.Num() - 1; I >= 0; --I)
    {
        auto& L = RepairLinks[I]; auto* C = Cargo.FindByPredicate([&L](const auto& Item) { return Item.Ship == L.Source; });
        if (!C || !Active(L.Source) || !IsValid(L.Target) || !L.Target->Systems || L.Target->Systems->bWrecked || L.Target->Systems->bNeutral || L.Source->Systems->FactionId != L.Faction || L.Target->Systems->FactionId != L.Faction || L.Source->bFTLCharging || L.Target->bFTLCharging || L.Source->IsHidden() || L.Target->IsHidden() || SurfaceDistance(L.Source, L.Target) > L.Range) { LastReason = TEXT("수리 링크 조건 상실"); if(!L.bHero && IsValid(L.Source))RepairOutcome.Add(L.Source,LastReason); RepairLinks.RemoveAt(I); continue; }
        float Delta = GameDelta;
        const float Setup = FMath::Min(Delta, L.SetupRemaining); L.SetupRemaining -= Setup; Delta -= Setup;
        if (Delta <= 0) continue;
        L.State = TEXT("수리 중");
        auto& S=*L.Target->Systems;
        const int32 Room=L.bHero?INDEX_NONE:DamagedRoom(S,false);
        const FVector Cost=(Room!=INDEX_NONE?RoomCost(L.BaseCostPerHP):L.BaseCostPerHP)*1.5;
        const float Time=FMath::Min(Delta,L.TimeRemaining);
        const float Budget=FMath::Min3(L.Rate*Time,L.HPRemaining,ResourceLimit(*C,Cost));
        float HP=0;
        if(Room!=INDEX_NONE){ L.State=TEXT("시설 수리 중"); HP=RepairRoom(S,Room,Budget); }
        else if (!L.bHero && S.HullHP>=S.MaxHullHP && DamagedMount(S)!=INDEX_NONE)
        {
            const int32 Index = DamagedMount(S);
            auto& Mount = S.WeaponMountStates[Index];
            const float Maximum = S.Layout->WeaponMounts[Index].MaxHP;
            const bool Rebuild = Mount.HP <= 0;
            float& Progress = Rebuild ? Mount.RebuildHP : Mount.HP;
            HP = FMath::Min(Budget, Maximum - Progress);
            Progress += HP;
            if (Rebuild && Progress >= Maximum) { Mount.HP = Maximum; Mount.RebuildHP = 0; }
            L.State = Rebuild ? TEXT("파괴 무장 재건 중") : TEXT("무장 수리 중");
        }
        else { L.State=TEXT("선체 수리 중"); HP=FMath::Min(Budget,S.MaxHullHP-S.HullHP); S.HullHP+=HP; }
        Spend(*C,Cost*HP); L.HPRemaining-=HP; L.TimeRemaining-=Time;
        const bool Finished=L.bHero ? S.HullHP>=S.MaxHullHP : !NeedsService(S);
        if(HP<=0 || Finished || L.HPRemaining<=KINDA_SMALL_NUMBER || L.TimeRemaining<=KINDA_SMALL_NUMBER)
        {
            if(!L.bHero){
                Approaches.Remove(L.Source);
                RepairOutcome.Add(L.Source,Finished?TEXT("수리 완료"):TEXT("자원 부족"));
            }
            RepairLinks.RemoveAt(I);
        }
    }
    AdvanceParties(GameDelta);
    if (!Outposts.IsEmpty() || !RemoteMiners.IsEmpty() || !TransportRoutes.IsEmpty())
    {
        SimulateOutpostLogistics(GameDelta);
    }
}

void UOVLogisticsSubsystem::ReleaseParty(AOVShip* Ship,FDamageParty& P)
{
    if(IsValid(Ship) && Ship->Systems && !Ship->Systems->bWrecked)
        for(const auto& Loan:P.Donors) if(Ship->Systems->Slots.IsValidIndex(Loan.Key))
            Ship->Systems->Slots[Loan.Key].Crew+=Loan.Value;
    if(IsValid(Ship) && Ship->Systems && Ship->Systems->Slots.IsValidIndex(P.Slot))Ship->Systems->Slots[P.Slot].RepairCrew=0;
    P.Slot=INDEX_NONE; P.Donors.Reset();
    if(IsValid(Ship) && Ship->Systems)Ship->Systems->RefreshDerivedState();
}
void UOVLogisticsSubsystem::ResetFieldOrders()
{
    for(auto& Pair:DamageParties)ReleaseParty(Pair.Key.Get(),Pair.Value);
    DamageParties.Reset(); Approaches.Reset(); RepairOutcome.Reset();
}
bool UOVLogisticsSubsystem::InternalRepairEnabled(AOVShip* Ship) const {return DamageParties.Contains(Ship);}
FString UOVLogisticsSubsystem::InternalRepairStatus(AOVShip* Ship) const
{
    if(const auto* P=DamageParties.Find(Ship))return P->Status;
    return TEXT("꺼짐");
}
bool UOVLogisticsSubsystem::ToggleInternalRepair(AOVShip* Ship)
{
    if(auto* P=DamageParties.Find(Ship)){ReleaseParty(Ship,*P);DamageParties.Remove(Ship);return true;}
    if(!IsValid(Ship) || !Ship->Systems || !Ship->Systems->CanAct() || !Ship->Systems->HasPhysicalFacility(EOVFacilityType::ResourceStorage))return false;
    auto& P=DamageParties.Add(Ship); P.Faction=Ship->Systems->FactionId; P.Status=TEXT("대기");
    return true;
}
FString UOVLogisticsSubsystem::RepairStatus(AOVShip* Ship) const
{
    if(const auto* L=RepairLinks.FindByPredicate([Ship](const auto& Link){return !Link.bHero && Link.Source==Ship;}))
        return IsValid(L->Target)?FString::Printf(TEXT("%s · %s"),*L->State,*L->Target->ShipName):FString(TEXT("연결 해제"));
    if(const auto* T=Approaches.Find(Ship); T && T->IsValid())return TEXT("접근 중 · ")+T->Get()->ShipName;
    if(const auto* Last=RepairOutcome.Find(Ship))return *Last;
    return TEXT("수리 대기");
}
void UOVLogisticsSubsystem::AdvanceParties(float Delta)
{
    constexpr float Efficiency=.3f;
    for(auto It=DamageParties.CreateIterator();It;++It)
    {
        AOVShip* Ship=It.Key().Get(); auto& P=It.Value();
        if(!IsValid(Ship) || !Ship->Systems || !Ship->Systems->CanAct() || Ship->Systems->FactionId!=P.Faction || Ship->IsHidden()){
            ReleaseParty(Ship,P); It.RemoveCurrent();continue;
        }
        auto& S=*Ship->Systems;
        const int32 Room=DamagedRoom(S,true);
        auto* C=Cargo.FindByPredicate([Ship](const auto& Item){return Item.Ship==Ship;});
        const FVector Cost=RoomCost(PackageCost(Ship->HullDefinitionId))*(.45/(FMath::Max(1.f,S.MaxHullHP)*Efficiency));
        const bool Supplied=Active(Ship) && C && !Cost.IsNearlyZero() && ResourceLimit(*C,Cost)>KINDA_SMALL_NUMBER;
        const bool External=RepairLinks.ContainsByPredicate([Ship](const auto& L){return !L.bHero && L.Target==Ship;});
        if(P.Slot!=INDEX_NONE && (P.Slot!=Room || !Supplied || External || Ship->bFTLCharging))ReleaseParty(Ship,P);
        if(Room==INDEX_NONE){P.Status=TEXT("대기");continue;}
        if(!Supplied || Ship->bFTLCharging || External){P.Status=External?TEXT("외부 수리 중"):Ship->bFTLCharging?TEXT("도약 대기"):TEXT("적재실·자원 확인");continue;}
        // Unstaffed security rooms need one visiting technician.
        const float Required=FMath::Max(1.f,.5f*S.Layout->Slots[Room].RequiredCrew);
        if(P.Slot==INDEX_NONE)
        {
            float Left=Required;
            // Keep at least half the normal watch in each donor room.
            for(int32 Rank=6;Rank>=0 && Left>KINDA_SMALL_NUMBER;--Rank)
                for(int32 I=0;I<S.Slots.Num() && Left>KINDA_SMALL_NUMBER;++I)
                {
                    const auto& D=S.Layout->Slots[I]; auto& Slot=S.Slots[I];
                    if(I==Room || D.RequiredCrew<=0 || Slot.HP<=0 || RepairPriority(D.Facility)!=Rank ||
                       (Slot.OccupyingFaction>=0 && Slot.OccupyingFaction!=S.FactionId))continue;
                    const float N=FMath::Min(Left,FMath::Max(0.f,Slot.Crew-.5f*D.RequiredCrew));
                    if(N>0){P.Donors.Emplace(I,N);Slot.Crew-=N;Left-=N;}
                }
            if(Left>KINDA_SMALL_NUMBER){ReleaseParty(Ship,P);P.Status=TEXT("인원 부족");continue;}
            P.Slot=Room; S.Slots[Room].RepairCrew=Required; S.RefreshDerivedState();
        }
        const float HP=RepairRoom(S,Room,FMath::Min(FieldRate(S)*Efficiency*Delta,ResourceLimit(*C,Cost)));
        Spend(*C,Cost*HP);
        P.Status=FString::Printf(TEXT("복구 %.0f%% · %.0f명"),100*S.Slots[Room].RebuildHP/S.Layout->Slots[Room].MaxFacilityHP,Required);
        if(S.Slots[Room].HP>0){ReleaseParty(Ship,P);P.Status=TEXT("복구 완료");}
    }
}
void UOVLogisticsSubsystem::ResetOutpostLogistics()
{
    Outposts.Reset();
    RemoteMiners.Reset();
    TransportRoutes.Reset();
    HomeBaseDelivered.Reset();
    OutpostLogisticsElapsedSeconds = 0.0;
    NextOutpostId = 1;
    NextMinerAssignmentId = 1;
    NextTransportRouteId = 1;
}

int32 UOVLogisticsSubsystem::AddRemoteOutpost(
    FName StarSystemId,
    int32 FactionId,
    EOVOutpostRole Role,
    int32 SystemDistanceHops,
    float WarehouseCapacity)
{
    if (StarSystemId.IsNone() || SystemDistanceHops <= 0)
    {
        LastReason = TEXT("전초기지는 본 기지 성계가 아닌 타 성계(거리 1성계 이상)에만 건설 가능");
        return 0;
    }
    if (Role == EOVOutpostRole::ResourceOutpost && (!FMath::IsFinite(WarehouseCapacity) || WarehouseCapacity <= 0.f))
    {
        LastReason = TEXT("자원 전초기지 창고 용량이 유효하지 않음");
        return 0;
    }

    FOVRemoteOutpostState& State = Outposts.AddDefaulted_GetRef();
    State.OutpostId = NextOutpostId++;
    State.StarSystemId = StarSystemId;
    State.FactionId = FactionId;
    State.Role = Role;
    State.SystemDistanceHops = SystemDistanceHops;
    State.WarehouseCapacity = (Role == EOVOutpostRole::ResourceOutpost) ? WarehouseCapacity : 0.0f;
    LastReason = (Role == EOVOutpostRole::ResourceOutpost) ? TEXT("자원 전초기지 건설 완료") : TEXT("전투 전초기지 건설 완료");
    return State.OutpostId;
}

int32 UOVLogisticsSubsystem::AssignRemoteMiners(
    int32 FactionId,
    int32 OutpostId,
    int32 MinerCount,
    float CargoPerMiner,
    float MiningSecondsPerLoad,
    float RoundTripSeconds)
{
    if (MinerCount <= 0 || !FMath::IsFinite(CargoPerMiner) || CargoPerMiner <= 0.f
        || !FMath::IsFinite(MiningSecondsPerLoad) || MiningSecondsPerLoad <= 0.f
        || !FMath::IsFinite(RoundTripSeconds) || RoundTripSeconds < 0.f)
    {
        LastReason = TEXT("채굴 배정 인자가 유효하지 않음");
        return 0;
    }

    if (OutpostId > 0)
    {
        const FOVRemoteOutpostState* Outpost = Outposts.FindByPredicate(
            [OutpostId](const FOVRemoteOutpostState& O) { return O.OutpostId == OutpostId; });
        if (!Outpost || Outpost->FactionId != FactionId)
        {
            LastReason = TEXT("유효한 아군 전초기지를 찾을 수 없음");
            return 0;
        }
        if (Outpost->Role != EOVOutpostRole::ResourceOutpost)
        {
            LastReason = TEXT("전투 전초기지는 채굴 하역 허브로 사용할 수 없음 (자원 전초기지 전용)");
            return 0;
        }
    }

    FOVRemoteMiningAssignment& Assign = RemoteMiners.AddDefaulted_GetRef();
    Assign.AssignmentId = NextMinerAssignmentId++;
    Assign.FactionId = FactionId;
    Assign.OutpostId = OutpostId;
    Assign.MinerCount = MinerCount;
    Assign.CargoPerMiner = CargoPerMiner;
    Assign.MiningSecondsPerLoad = MiningSecondsPerLoad;
    Assign.RoundTripSeconds = RoundTripSeconds;
    LastReason = OutpostId > 0 ? TEXT("자원 전초기지 채굴선 배정 완료") : TEXT("본 기지 직송 채굴선 배정 완료");
    return Assign.AssignmentId;
}

int32 UOVLogisticsSubsystem::AddMediumTransportRoute(
    int32 FactionId,
    int32 OutpostId,
    int32 TransportCount,
    float CargoCapacityPerShip,
    float OneWayTransitSeconds,
    float LoadUnloadSeconds)
{
    const FOVRemoteOutpostState* Outpost = Outposts.FindByPredicate(
        [OutpostId](const FOVRemoteOutpostState& O) { return O.OutpostId == OutpostId; });
    if (!Outpost || Outpost->FactionId != FactionId)
    {
        LastReason = TEXT("중형 수송선(R37) 노선 대상 전초기지가 없음");
        return 0;
    }
    if (Outpost->Role != EOVOutpostRole::ResourceOutpost)
    {
        LastReason = TEXT("전투 전초기지에는 자원 수송 노선을 연결할 수 없음 (자원 전초기지 전용)");
        return 0;
    }
    if (TransportCount <= 0 || !FMath::IsFinite(CargoCapacityPerShip) || CargoCapacityPerShip <= 0.f
        || !FMath::IsFinite(OneWayTransitSeconds) || OneWayTransitSeconds <= 0.f
        || !FMath::IsFinite(LoadUnloadSeconds) || LoadUnloadSeconds < 0.f)
    {
        LastReason = TEXT("중형 수송선(R37) 노선 파라미터가 유효하지 않음");
        return 0;
    }

    FOVMediumTransportRoute& Route = TransportRoutes.AddDefaulted_GetRef();
    Route.RouteId = NextTransportRouteId++;
    Route.FactionId = FactionId;
    Route.OutpostId = OutpostId;
    Route.TransportCount = TransportCount;
    Route.CargoCapacityPerShip = CargoCapacityPerShip;
    Route.OneWayTransitSeconds = OneWayTransitSeconds;
    Route.LoadUnloadSeconds = LoadUnloadSeconds;
    Route.Phase = 0;
    Route.PhaseRemainingSeconds = 0.0f;
    LastReason = TEXT("중형 수송선(R37) 왕복 물류 노선 연결 완료");
    return Route.RouteId;
}

void UOVLogisticsSubsystem::SimulateOutpostLogistics(float GameDeltaSeconds)
{
    if (!FMath::IsFinite(GameDeltaSeconds) || GameDeltaSeconds <= 0.f) return;

    // Step in <= 1.0 s substeps for deterministic cycle accounting across large time jumps.
    float Remaining = GameDeltaSeconds;
    while (Remaining > KINDA_SMALL_NUMBER)
    {
        const float Dt = FMath::Min(Remaining, 1.0f);
        Remaining -= Dt;
        OutpostLogisticsElapsedSeconds += Dt;

        // 1. Local / Direct Mining -> Outpost Warehouse or Direct Home Base
        for (FOVRemoteMiningAssignment& Miner : RemoteMiners)
        {
            const float CycleSeconds = FMath::Max(1.0f, Miner.MiningSecondsPerLoad + Miner.RoundTripSeconds);
            const float BatchCargo = Miner.CargoPerMiner * Miner.MinerCount;

            if (Miner.OutpostId > 0)
            {
                FOVRemoteOutpostState* Outpost = Outposts.FindByPredicate(
                    [&Miner](const FOVRemoteOutpostState& O) { return O.OutpostId == Miner.OutpostId; });
                if (!Outpost || Outpost->Role != EOVOutpostRole::ResourceOutpost) continue;

                // Flush pending unload first if warehouse has free space
                if (Miner.PendingUnload > KINDA_SMALL_NUMBER)
                {
                    const float FreeSpace = FMath::Max(0.0f, Outpost->WarehouseCapacity - Outpost->StoredResources);
                    const float Deposited = FMath::Min(Miner.PendingUnload, FreeSpace);
                    Outpost->StoredResources += Deposited;
                    Outpost->TotalUnloaded += Deposited;
                    Miner.PendingUnload -= Deposited;
                    if (Miner.PendingUnload > KINDA_SMALL_NUMBER)
                    {
                        Outpost->WarehouseCongestedSeconds += Dt;
                        Outpost->bWarehouseBottleneck = true;
                        continue; // Miners blocked waiting for warehouse space
                    }
                }

                Miner.CycleProgressSeconds += Dt;
                while (Miner.CycleProgressSeconds >= CycleSeconds)
                {
                    Miner.CycleProgressSeconds -= CycleSeconds;
                    const float FreeSpace = FMath::Max(0.0f, Outpost->WarehouseCapacity - Outpost->StoredResources);
                    const float Deposited = FMath::Min(BatchCargo, FreeSpace);
                    Outpost->StoredResources += Deposited;
                    Outpost->TotalUnloaded += Deposited;
                    const float Overflow = BatchCargo - Deposited;
                    if (Overflow > KINDA_SMALL_NUMBER)
                    {
                        Miner.PendingUnload += Overflow;
                        Outpost->WarehouseCongestedSeconds += Dt;
                        Outpost->bWarehouseBottleneck = true;
                        break;
                    }
                }
            }
            else
            {
                // Direct multi-system haul to Home Base (no outpost)
                Miner.CycleProgressSeconds += Dt;
                while (Miner.CycleProgressSeconds >= CycleSeconds)
                {
                    Miner.CycleProgressSeconds -= CycleSeconds;
                    HomeBaseDelivered.FindOrAdd(Miner.FactionId) += BatchCargo;
                }
            }
        }

        // 2. Medium Transport (R37) Shuttle: ResourceOutpost <-> Home Base
        for (FOVMediumTransportRoute& Route : TransportRoutes)
        {
            FOVRemoteOutpostState* Outpost = Outposts.FindByPredicate(
                [&Route](const FOVRemoteOutpostState& O) { return O.OutpostId == Route.OutpostId; });
            if (!Outpost || Outpost->Role != EOVOutpostRole::ResourceOutpost) continue;

            const float FleetCapacity = Route.CargoCapacityPerShip * Route.TransportCount;
            if (Route.Phase == 0)
            {
                // Loading at ResourceOutpost once enough stock is available (or >= 50% capacity ready)
                if (Outpost->StoredResources >= FMath::Min(FleetCapacity, Outpost->WarehouseCapacity * 0.25f)
                    && Outpost->StoredResources > KINDA_SMALL_NUMBER)
                {
                    const float Loaded = FMath::Min(FleetCapacity, Outpost->StoredResources);
                    Outpost->StoredResources -= Loaded;
                    Outpost->TotalDispatched += Loaded;
                    Route.CarriedCargo = Loaded;
                    Route.Phase = 1;
                    Route.PhaseRemainingSeconds = Route.OneWayTransitSeconds + Route.LoadUnloadSeconds;
                }
            }
            else if (Route.Phase == 1)
            {
                Route.PhaseRemainingSeconds -= Dt;
                if (Route.PhaseRemainingSeconds <= 0.0f)
                {
                    HomeBaseDelivered.FindOrAdd(Route.FactionId) += Route.CarriedCargo;
                    Route.TotalDelivered += Route.CarriedCargo;
                    Route.CarriedCargo = 0.0f;
                    Route.Phase = 2;
                    Route.PhaseRemainingSeconds = Route.OneWayTransitSeconds;
                }
            }
            else
            {
                Route.PhaseRemainingSeconds -= Dt;
                if (Route.PhaseRemainingSeconds <= 0.0f)
                {
                    Route.Phase = 0;
                    Route.PhaseRemainingSeconds = 0.0f;
                }
            }
        }

        // 3. Warehouse fill-ratio congestion monitoring
        for (FOVRemoteOutpostState& Outpost : Outposts)
        {
            if (Outpost.Role == EOVOutpostRole::ResourceOutpost && Outpost.WarehouseCapacity > 0.0f)
            {
                if (Outpost.StoredResources >= Outpost.WarehouseCapacity * 0.90f)
                {
                    Outpost.WarehouseCongestedSeconds += Dt;
                    Outpost.bWarehouseBottleneck = true;
                }
            }
        }
    }
}

float UOVLogisticsSubsystem::GetHomeBaseDelivered(int32 FactionId) const
{
    if (const float* Value = HomeBaseDelivered.Find(FactionId)) return *Value;
    return 0.0f;
}

float UOVLogisticsSubsystem::GetHomeBaseRatePerMinute(int32 FactionId) const
{
    if (OutpostLogisticsElapsedSeconds <= KINDA_SMALL_NUMBER) return 0.0f;
    return static_cast<float>((GetHomeBaseDelivered(FactionId) / OutpostLogisticsElapsedSeconds) * 60.0);
}

bool UOVLogisticsSubsystem::DiagnoseOutpostBottleneck(int32 OutpostId, FString& OutDiagnosis) const
{
    const FOVRemoteOutpostState* Outpost = Outposts.FindByPredicate(
        [OutpostId](const FOVRemoteOutpostState& O) { return O.OutpostId == OutpostId; });
    if (!Outpost)
    {
        OutDiagnosis = TEXT("지정된 전초기지를 찾을 수 없습니다.");
        return false;
    }
    if (Outpost->Role != EOVOutpostRole::ResourceOutpost)
    {
        OutDiagnosis = TEXT("전투 전초기지는 자원 물류 창고를 운용하지 않습니다.");
        return false;
    }
    const float FillRatio = Outpost->WarehouseCapacity > 0.0f
        ? (Outpost->StoredResources / Outpost->WarehouseCapacity) : 0.0f;
    if (Outpost->bWarehouseBottleneck || Outpost->WarehouseCongestedSeconds > 0.0f || FillRatio >= 0.85f)
    {
        OutDiagnosis = FString::Printf(
            TEXT("자원 전초기지 창고에 자원이 적체 중입니다 (적재율 %.0f%%, 적체 %.0f초). 중형 수송선(R37) 대수나 적재량을 늘려야 합니다."),
            FillRatio * 100.0f, Outpost->WarehouseCongestedSeconds);
        return true;
    }
    OutDiagnosis = FString::Printf(TEXT("자원 전초기지 물류 정상 (창고 적재율 %.0f%%)."), FillRatio * 100.0f);
    return false;
}
