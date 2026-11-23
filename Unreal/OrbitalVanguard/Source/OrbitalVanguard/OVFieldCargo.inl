// SimulateFieldCargo.py / D126 시험 후보
static const FVector AircraftKitCost(35, 5, 0);
static const FVector RepairStock(1809, 267, 17);
static constexpr int32 MiningStock=80, TransportStock=130;
static FVector PackageCost(FName Hull)
{
    if (Hull == TEXT("corvette")) return FVector(450,0,0);
    if (Hull == TEXT("frigate")) return FVector(600,0,0);
    if (Hull == TEXT("vector")) return FVector(1600,180,0);
    if (Hull == TEXT("bastion")) return FVector(2600,420,0);
    if (Hull == TEXT("cruiser")) return FVector(2400,350,0);
    if (Hull == TEXT("air_defense")) return FVector(2200,300,0);
    if (Hull == TEXT("radar")) return FVector(2000,300,0);
    if (Hull == TEXT("battleship")) return FVector(7200,1200,100);
    if (Hull == TEXT("carrier")) return FVector(8000,1500,150);
    if (Hull == TEXT("missile_ship")) return FVector(7600,1400,120);
    if (Hull == TEXT("dreadnought")) return FVector(28800,6000,800);
    if (Hull == TEXT("battlecarrier")) return FVector(32000,7000,1000);
    if (Hull == TEXT("repair")) return FVector(400,0,0);
    if (Hull == TEXT("mining")) return FVector(300,0,0);
    if (Hull == TEXT("transport")) return FVector(1600,180,0);
    if (Hull == TEXT("assault")) return FVector(600,0,0);
    return FVector::ZeroVector;
}
