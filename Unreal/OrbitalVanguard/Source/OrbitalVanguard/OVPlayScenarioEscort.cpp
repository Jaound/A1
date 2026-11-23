#include "OVPlayScenarioSubsystem.h"
#include "OVCombatSubsystem.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "OVController.h"
#include "OVLogisticsSubsystem.h"
#include "OVShip.h"
#include "OVShipSystemsComponent.h"

namespace
{
// Mission calibration in UE centimetres, game seconds and units of common ore.
struct FEscortTrial
{
    FVector Vein = FVector(-38000, 0, 2000);
    FVector Exchange = FVector(-29000, 0, 2000);
    FVector Depot = FVector(34000, 0, 2000);
    float Stock = 650;
    float Goal = 500;
    float MiningPerSecond = 5;
    float LoadingPerSecond = 18;
    float UnloadingPerSecond = 25;
    float MiningRange = 7500;
    float ExchangeRange = 16500;
    float DockRange = 8500;
    float LaneOffset = 5500;
    float RaidAt = 18;
};
const FEscortTrial Trial;

bool StillInstalled(const AOVShip& Ship, EOVFacilityType Type)
{
    const auto& Systems = *Ship.Systems;
    for (int32 I = 0; I < Systems.Slots.Num(); ++I)
        if (Systems.Layout->Slots.IsValidIndex(I) && Systems.Layout->Slots[I].Facility == Type && Systems.Slots[I].HP > 0) return true;
    return false;
}

bool InConvoy(AOVShip* Ship, const UOVCombatSubsystem& Combat)
{
    return IsValid(Ship) && Ship->Systems && Ship->Systems->FactionId == 0 &&
        !Ship->Systems->bNeutral && !Ship->Systems->bWrecked && !Combat.IsDeparted(Ship);
}
}
FVector UOVPlayScenarioSubsystem::MiningSite() { return Trial.Vein; }
FVector UOVPlayScenarioSubsystem::DeliverySite() { return Trial.Depot; }
float UOVPlayScenarioSubsystem::EscortQuota() { return Trial.Goal; }

void UOVPlayScenarioSubsystem::OrderConvoyLeg(AOVShip& Ship, int32 Lane, bool bMiner)
{
    FVector Destination = bMiner ? Trial.Vein : (DeliveryLeg.Contains(&Ship) ? Trial.Depot : Trial.Exchange);
    Destination.Y += (Lane == 0 ? -1 : 1) * Trial.LaneOffset;
    Ship.SetDestination(Destination);
}

bool UOVPlayScenarioSubsystem::StartEscort(AOVController& Controller)
{
    auto* Logistics = GetWorld()->GetSubsystem<UOVLogisticsSubsystem>();
    OreNode = Logistics->AddDevNode(Trial.Vein, Trial.Stock);
    if (!OreNode) return false;
    if (auto* Rock = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Presentation/Environment/Asteroid/SM_Source.SM_Source")))
    {
        // This rock marks the real finite node. It is scenery, so docking and projectiles use their normal rules.
        if (auto* Marker = GetWorld()->SpawnActor<AStaticMeshActor>(Trial.Vein, FRotator(12, 30, 6)))
        {
            OreMarker = Marker;
            auto* Mesh = Marker->GetStaticMeshComponent();
            Mesh->SetMobility(EComponentMobility::Movable);
            Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Mesh->SetStaticMesh(Rock);
            Marker->SetActorScale3D(FVector(1900.0 / FMath::Max(1.0, Rock->GetBounds().BoxExtent.GetMax())));
        }
    }
    int32 EscortIndex = 0;
    for (const FOVPlayFleetEntry& Entry : GetActiveDefinition()->Fleet)
    {
        const bool bMiner = Entry.Hull == TEXT("mining");
        const bool bHauler = Entry.Hull == TEXT("transport");
        for (int32 Lane = 0; Lane < Entry.Count; ++Lane)
        {
            const float Side = Lane == 0 ? -1.f : 1.f;
            FVector Position(-8000 + EscortIndex * 9500, 22000, 2000);
            if (bMiner) Position = Trial.Vein + FVector(-10000, Side * Trial.LaneOffset, 0);
            if (bHauler) Position = Trial.Depot + FVector(0, Side * Trial.LaneOffset, 0);
            auto* Ship = Spawn(Controller, Entry, 0, bMiner || bHauler ? Lane : EscortIndex, Position);
            if (!Ship) return false;
            if (!bMiner && !bHauler)
            {
                ++EscortIndex;
                Ship->SetDestination(Position + FVector(0, 8500, 0));
                continue;
            }
            if (!Logistics->RegisterDevShip(Ship, bMiner ? EOVDevLogisticsRole::Mining : EOVDevLogisticsRole::Transport,
                                           Logistics->FieldCapacity(Entry.Hull), Logistics->FieldCapacity(Entry.Hull))) return false;
            InitialOre.Add(Ship,Logistics->FieldCapacity(Entry.Hull));
            if(bHauler) DeliveryLeg.Add(Ship);
            (bMiner ? Miners : Haulers).Add(Ship);
            Ship->SetAutomation(false, true);
            OrderConvoyLeg(*Ship, Lane, bMiner);
        }
    }
    return true;
}

void UOVPlayScenarioSubsystem::StepEscort(AOVController& Controller, float Delta)
{
    auto& Combat = *Controller.Combat;
    auto& Logistics = *GetWorld()->GetSubsystem<UOVLogisticsSubsystem>();
    if (!bRaidLaunched && ElapsedSeconds >= Trial.RaidAt)
    {
        for (int32 Lane = 0; Lane < Haulers.Num(); ++Lane)
        {
            const FVector Entry(6000 + Lane * 12000, 62000, 2000);
            auto* Raider = Spawn(Controller, {TEXT("corvette"), 1}, 1, Lane, Entry);
            if (!Raider)
            {
                Finish(Controller, EOVPlaySceneResult::NoCombatCapability, TEXT("습격대 생성 실패 · 임무를 재시작하세요"));
                return;
            }
            Raider->SetDestination(Trial.Exchange + FVector(13000, 9000 * Lane, 0));
            if (InConvoy(Haulers[Lane].Get(), Combat)) Combat.SetTarget(Raider, Haulers[Lane].Get());
        }
        bRaidLaunched = true;
    }

    const auto* Node = Logistics.Nodes.FindByPredicate([this](const auto& N) { return N.Id == OreNode; });
    double Recoverable = DeliveredCargo;
    int32 ActiveHaulers = 0;
    bool bCanMineMore = false;
    float WaitingCargo = 0, UncreditedWaiting=0;
    for (const auto& Weak : Miners)
    {
        auto* Ship = Weak.Get();
        if (!InConvoy(Ship, Combat)) continue;
        if (!StillInstalled(*Ship, EOVFacilityType::ResourceStorage)) continue;
        const bool bAtVein = FVector::DistSquared(Ship->GetActorLocation(), Trial.Vein) <= FMath::Square(Trial.MiningRange);
        const bool bMobile = StillInstalled(*Ship, EOVFacilityType::Engine);
        // A disabled miner at the vein can still extract. Its cargo must be reachable from the exchange.
        const bool bCanMeet = bMobile || FVector::Dist(Ship->GetActorLocation(), Trial.Exchange) <= Trial.ExchangeRange + Trial.DockRange;
        if (!bCanMeet) continue;
        bCanMineMore |= bMobile || bAtVein;
        if (Node && Node->Amount > 0)
            Logistics.DevMine(Ship, OreNode, Trial.MiningPerSecond * Delta, Trial.MiningRange);
        if (const auto* Bin = Logistics.FindCargo(Ship)) {WaitingCargo += Bin->Amount; UncreditedWaiting+=FMath::Min(Bin->Amount,InitialOre.FindRef(Ship));}
    }
    bool bHasShuttle = false;
    const double Unshipped = WaitingCargo - UncreditedWaiting + (Node && bCanMineMore ? Node->Amount : 0);
    for (int32 Lane = 0; Lane < Haulers.Num(); ++Lane)
    {
        auto* Ship = Haulers[Lane].Get();
        if (!InConvoy(Ship, Combat)) continue;
        const auto* Bin = Logistics.FindCargo(Ship);
        if (!Bin || !StillInstalled(*Ship, EOVFacilityType::ResourceStorage)) continue;
        const bool bMobile = StillInstalled(*Ship, EOVFacilityType::Engine);
        const bool bAtDepot = FVector::DistSquared(Ship->GetActorLocation(), Trial.Depot) <= FMath::Square(Trial.DockRange);
        if (!bMobile && (!bAtDepot || Bin->Amount <= 0)) continue;
        if (!bMobile) DeliveryLeg.Add(Ship);
        bHasShuttle |= bMobile;
        ++ActiveHaulers;
        Recoverable += FMath::Max(0.f,Bin->Amount-InitialOre.FindRef(Ship));
        if (DeliveryLeg.Contains(Ship))
        {
            const float Unloaded = Logistics.UnloadTransport(Ship, 0, Trial.Depot, Trial.DockRange,
                FMath::Min(Trial.UnloadingPerSecond * Delta, Trial.Goal - DeliveredCargo));
            float& Reserve=InitialOre.FindOrAdd(Ship);
            const float Starting=FMath::Min(Reserve,Unloaded);Reserve-=Starting;DischargedStores+=Starting;DeliveredCargo+=Unloaded-Starting;
            if (Unloaded > 0 && Bin->Amount <= UE_KINDA_SMALL_NUMBER)
            {
                DeliveryLeg.Remove(Ship);
                OrderConvoyLeg(*Ship, Lane, false);
            }
        }
        else
        {
            if (FVector::DistSquared(Ship->GetActorLocation(), Trial.Exchange) <= FMath::Square(Trial.DockRange))
            {
                float Allowance = Trial.LoadingPerSecond * Delta;
                for (int32 Offset = 0; Offset < Miners.Num() && Allowance > 0; ++Offset)
                {
                    auto* Miner = Miners[(Lane + Offset) % Miners.Num()].Get();
                    if (!InConvoy(Miner, Combat)) continue;
                    const float Loaded = Logistics.DevTransfer(Miner, Ship, Allowance, Trial.ExchangeRange);
                    float& Reserve=InitialOre.FindOrAdd(Miner);
                    const float Starting=FMath::Min(Reserve,Loaded);Reserve-=Starting;InitialOre.FindOrAdd(Ship)+=Starting;HandedMinedOre+=Loaded-Starting;
                    WaitingCargo -= Loaded;
                    Allowance -= Loaded;
                }
            }
            const bool bNoMoreOre = (!bCanMineMore || !Node || Node->Amount <= 0) && WaitingCargo <= UE_KINDA_SMALL_NUMBER;
            if (Bin->Amount > 0 && (Bin->Amount >= FMath::Min(Bin->Capacity, Trial.Goal - DeliveredCargo) || bNoMoreOre))
            {
                DeliveryLeg.Add(Ship);
                OrderConvoyLeg(*Ship, Lane, false);
            }
        }
    }
    if (bHasShuttle) Recoverable += Unshipped;
    if (DeliveredCargo >= Trial.Goal - UE_KINDA_SMALL_NUMBER)
        Finish(Controller, EOVPlaySceneResult::Victory, TEXT("목표 화물 납품 완료"));
    else if (!ActiveHaulers)
        Finish(Controller, EOVPlaySceneResult::Defeat, TEXT("수송 전력 상실"));
    else if (Recoverable + .01 < Trial.Goal)
        Finish(Controller, EOVPlaySceneResult::Defeat, TEXT("회수 가능한 화물 부족"));
}

FString UOVPlayScenarioSubsystem::GetConvoyStatus() const
{
    const auto* Controller = Owner.Get();
    if (!Controller || !Controller->Combat) return {};
    const auto* Logistics = GetWorld()->GetSubsystem<UOVLogisticsSubsystem>();
    int32 Count[2] = {};
    float Cargo[2] = {};
    for (int32 Role = 0; Role < 2; ++Role)
        for (const auto& Weak : Role == 0 ? Miners : Haulers)
            if (InConvoy(Weak.Get(), *Controller->Combat))
            {
                ++Count[Role];
                if (const auto* Bin = Logistics->FindCargo(Weak.Get())) Cargo[Role] += Bin->Amount;
            }
    return FString::Printf(TEXT("채굴선 %d척 · 원광 %.0f    수송선 %d척 · 화물 %.0f"), Count[0], Cargo[0], Count[1], Cargo[1]);
}
