#include "OVShipLayout.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#endif

namespace OVLayout
{
bool Finite(const FVector& V) { return FMath::IsFinite(V.X) && FMath::IsFinite(V.Y) && FMath::IsFinite(V.Z); }
bool Finite(const FVector2D& V) { return FMath::IsFinite(V.X) && FMath::IsFinite(V.Y); }
bool Uncrewed(EOVFacilityType Type) { return Type == EOVFacilityType::None || Type == EOVFacilityType::Core || Type == EOVFacilityType::Security; }
FName SlotName(int32 Deck, int32 X, int32 Y) { return FName(*FString::Printf(TEXT("D%d_X%d_Y%d"), Deck, X, Y)); }
FVector2D UV(EOVArmorFace Face, const FVector& V)
{
    if (Face == EOVArmorFace::Front || Face == EOVArmorFace::Rear) return FVector2D(V.Y, V.Z);
    if (Face == EOVArmorFace::Left || Face == EOVArmorFace::Right) return FVector2D(V.X, V.Z);
    return FVector2D(V.X, V.Y);
}
bool IsEnd(EOVArmorFace Face) { return Face == EOVArmorFace::Front || Face == EOVArmorFace::Rear; }
bool IsSide(EOVArmorFace Face) { return Face == EOVArmorFace::Left || Face == EOVArmorFace::Right; }
bool CellContains(double Value, double Min, double Max)
{
    // Lower inclusive, upper exclusive; the outer +1 edge belongs to the last cell.
    return Value >= Min && (Value < Max || (Max >= 1.0 && Value <= 1.0));
}
bool SurfaceSlot(const FOVSlotDefinition& Slot, EOVArmorFace Face, const FVector& Extent, int32 Decks)
{
    switch (Face)
    {
    case EOVArmorFace::Front: return FMath::IsNearlyEqual(Slot.LocalCenter.X + Slot.HalfExtent.X, Extent.X, .01);
    case EOVArmorFace::Rear: return FMath::IsNearlyEqual(Slot.LocalCenter.X - Slot.HalfExtent.X, -Extent.X, .01);
    case EOVArmorFace::Left: return FMath::IsNearlyEqual(Slot.LocalCenter.Y - Slot.HalfExtent.Y, -Extent.Y, .01);
    case EOVArmorFace::Right: return FMath::IsNearlyEqual(Slot.LocalCenter.Y + Slot.HalfExtent.Y, Extent.Y, .01);
    case EOVArmorFace::Top: return Slot.DeckId == Decks;
    case EOVArmorFace::Bottom: return Slot.DeckId == 1;
    }
    return false;
}
bool SectionOverlapsSlot(const FOVArmorDefinition& Armor, const FOVSlotDefinition& Slot, const FVector& Bounds)
{
    const FVector2D Min = UV(Armor.Face, (Slot.LocalCenter - Slot.HalfExtent) / Bounds);
    const FVector2D Max = UV(Armor.Face, (Slot.LocalCenter + Slot.HalfExtent) / Bounds);
    return Max.X > Armor.NormalizedMin.X + .000001 && Min.X < Armor.NormalizedMax.X - .000001
        && Max.Y > Armor.NormalizedMin.Y + .000001 && Min.Y < Armor.NormalizedMax.Y - .000001;
}
}

UShipLayoutDataAsset* UShipLayoutDataAsset::MakeTestLayout(UObject* Outer, int32 Decks)
{
    if (Decks < 1 || Decks > 3) return nullptr;
    UShipLayoutDataAsset* Layout = NewObject<UShipLayoutDataAsset>(Outer ? Outer : GetTransientPackage());
    Layout->DeckCount = Decks;
    Layout->bSmallHull = Decks == 1;
    Layout->MaxHullHP = 2000 + Decks * 1000;
    Layout->CannonSlots = Layout->BeamSlots = Layout->MissileSlots = 1;
    // The two-deck core is explicitly authored on 1F, rather than inferred from rounding.
    Layout->CoreSlotId = OVLayout::SlotName(Decks == 3 ? 2 : 1, 2, 1);
    const float DeckHalfHeight = Layout->HalfHeight / Decks;
    for (int32 Deck = 1; Deck <= Decks; ++Deck)
    {
        for (int32 Y = 0; Y < 3; ++Y)
        {
            for (int32 X = 0; X < 5; ++X)
            {
                FOVSlotDefinition Slot;
                Slot.SlotId = OVLayout::SlotName(Deck, X, Y);
                Slot.DeckId = Deck;
                Slot.ColumnId = FName(*FString::Printf(TEXT("XY_%d_%d"), X, Y));
                Slot.LocalCenter = FVector((X - 2) * 70, (Y - 1) * 50, -Layout->HalfHeight + DeckHalfHeight * (2 * Deck - 1));
                Slot.HalfExtent = FVector(35, 25, DeckHalfHeight);
                Slot.AllowedFacilities.Add(EOVFacilityType::None);
                for (int32 Type = static_cast<int32>(EOVFacilityType::LifeSupport); Type <= static_cast<int32>(EOVFacilityType::Security); ++Type)
                {
                    if (Layout->bSmallHull && Type == static_cast<int32>(EOVFacilityType::Hangar)) continue;
                    Slot.AllowedFacilities.Add(static_cast<EOVFacilityType>(Type));
                }
                if (Slot.SlotId == Layout->CoreSlotId) Slot.AllowedFacilities.Add(EOVFacilityType::Core);
                if ((Decks != 3 || Deck != 2) && (X == 0 || X == 4) && Y == 1)
                {
                    Layout->BridgeAllowedSlots.Add(Slot.SlotId);
                    Slot.AllowedFacilities.Add(EOVFacilityType::Bridge);
                }
                Layout->Slots.Add(Slot);
            }
        }
    }
    auto Install = [&](int32 Index, EOVFacilityType Type)
    {
        FOVSlotDefinition& Slot = Layout->Slots[Index];
        Slot.Facility = Type;
        Slot.MaxFacilityHP = Type == EOVFacilityType::Bridge ? 180 : 100;
        Slot.RequiredCrew = OVLayout::Uncrewed(Type) ? 0 : 10;
    };
    Install(Layout->GetSlotIndex(Layout->CoreSlotId), EOVFacilityType::Core);
    Install(Layout->GetSlotIndex(OVLayout::SlotName(Decks, 4, 1)), EOVFacilityType::Bridge);
    TArray<EOVFacilityType> Facilities = {
        EOVFacilityType::LifeSupport, EOVFacilityType::Engine, EOVFacilityType::Engine, EOVFacilityType::Engine,
        EOVFacilityType::WeaponControl, EOVFacilityType::Ammo, EOVFacilityType::Capacitor,
        EOVFacilityType::MissileStorage, EOVFacilityType::ShieldControl, EOVFacilityType::Radar, EOVFacilityType::Security
    };
    if (Decks > 1)
    {
        Facilities.Append({EOVFacilityType::ShieldControl, EOVFacilityType::ShieldControl, EOVFacilityType::Radar,
            EOVFacilityType::Radar, EOVFacilityType::Interception, EOVFacilityType::Boarding,
            EOVFacilityType::ResourceStorage, EOVFacilityType::Hangar, EOVFacilityType::FighterControl});
    }
    int32 Next = 0;
    for (int32 Index = 0; Index < Layout->Slots.Num() && Next < Facilities.Num(); ++Index)
    {
        const FOVSlotDefinition& Slot = Layout->Slots[Index];
        // Keep an empty vertical column for explicit empty-slot/penetration experiments.
        if (Slot.Facility == EOVFacilityType::None && Slot.ColumnId != FName(TEXT("XY_0_0"))) Install(Index, Facilities[Next++]);
    }
    auto Connect = [&](FName A, FName B, EOVConnectionKind Kind)
    {
        FOVConnectionDefinition Link;
        Link.ConnectionId = FName(*FString::Printf(TEXT("Passage_%03d"), Layout->Connections.Num()));
        Link.FromSlot = A; Link.ToSlot = B; Link.Kind = Kind;
        Layout->Slots[Layout->GetSlotIndex(A)].ConnectionIds.Add(Link.ConnectionId);
        Layout->Slots[Layout->GetSlotIndex(B)].ConnectionIds.Add(Link.ConnectionId);
        Layout->Connections.Add(Link);
    };
    for (int32 Deck = 1; Deck <= Decks; ++Deck)
    {
        for (int32 Y = 0; Y < 3; ++Y)
        {
            for (int32 X = 0; X < 5; ++X)
            {
                if (X < 4) Connect(OVLayout::SlotName(Deck, X, Y), OVLayout::SlotName(Deck, X + 1, Y), EOVConnectionKind::Corridor);
                if (Y < 2) Connect(OVLayout::SlotName(Deck, X, Y), OVLayout::SlotName(Deck, X, Y + 1), EOVConnectionKind::Door);
            }
        }
        if (Deck < Decks) Connect(OVLayout::SlotName(Deck, 1, 1), OVLayout::SlotName(Deck + 1, 1, 1), EOVConnectionKind::Vertical);
    }
    for (int32 FaceIndex = 0; FaceIndex < 6; ++FaceIndex)
    {
        const EOVArmorFace Face = static_cast<EOVArmorFace>(FaceIndex);
        const bool End = OVLayout::IsEnd(Face);
        const int32 Rows = End ? 1 : Decks;
        const int32 Columns = End ? 1 : 3;
        for (int32 Row = 0; Row < Rows; ++Row)
        {
            for (int32 Column = 0; Column < Columns; ++Column)
            {
                FOVArmorDefinition Armor;
                Armor.Face = Face; Armor.Row = Row; Armor.Column = Column;
                Armor.ArmorId = FName(*FString::Printf(TEXT("Armor_F%d_R%d_C%d"), FaceIndex, Row, Column));
                Armor.NormalizedMin = FVector2D(-1.0 + 2.0 * Column / Columns, -1.0 + 2.0 * Row / Rows);
                Armor.NormalizedMax = FVector2D(-1.0 + 2.0 * (Column + 1) / Columns, -1.0 + 2.0 * (Row + 1) / Rows);
                const FVector2D Mid = (Armor.NormalizedMin + Armor.NormalizedMax) * .5;
                const FVector2D UVHalf = (Armor.NormalizedMax - Armor.NormalizedMin) * .5;
                const FVector Bounds = Layout->LocalHalfExtent;
                if (End)
                {
                    Armor.LocalCenter = FVector(Face == EOVArmorFace::Front ? Bounds.X : -Bounds.X, Mid.X * Bounds.Y, Mid.Y * Bounds.Z);
                    Armor.HalfExtent = FVector(1, UVHalf.X * Bounds.Y, UVHalf.Y * Bounds.Z);
                }
                else if (OVLayout::IsSide(Face))
                {
                    Armor.LocalCenter = FVector(Mid.X * Bounds.X, Face == EOVArmorFace::Right ? Bounds.Y : -Bounds.Y, Mid.Y * Bounds.Z);
                    Armor.HalfExtent = FVector(UVHalf.X * Bounds.X, 1, UVHalf.Y * Bounds.Z);
                }
                else
                {
                    Armor.LocalCenter = FVector(Mid.X * Bounds.X, Mid.Y * Bounds.Y, Face == EOVArmorFace::Top ? Bounds.Z : -Bounds.Z);
                    Armor.HalfExtent = FVector(UVHalf.X * Bounds.X, UVHalf.Y * Bounds.Y, 1);
                }
                Armor.MaxArmor = Face == EOVArmorFace::Front ? 400 : 100;
                for (const FOVSlotDefinition& Slot : Layout->Slots)
                    if (OVLayout::SurfaceSlot(Slot, Face, Bounds, Decks) && OVLayout::SectionOverlapsSlot(Armor, Slot, Bounds)) Armor.InnerSlotIds.Add(Slot.SlotId);
                Layout->ArmorSections.Add(Armor);
            }
        }
    }
    for (int32 Type = 0; Type < 3; ++Type)
    {
        FOVWeaponMountDefinition Weapon;
        Weapon.WeaponId = FName(*FString::Printf(TEXT("Weapon_%d"), Type));
        Weapon.Type = static_cast<EOVWeaponType>(Type);
        Weapon.LocalPosition = FVector((Type - 1) * 70, 0, Layout->HalfHeight + 10);
        Layout->WeaponMounts.Add(Weapon);
    }
    return Layout;
}

int32 UShipLayoutDataAsset::GetSlotIndex(FName SlotId) const
{
    return Slots.IndexOfByPredicate([SlotId](const FOVSlotDefinition& Slot) { return Slot.SlotId == SlotId; });
}

int32 UShipLayoutDataAsset::GetArmorIndex(FName ArmorId) const
{
    return ArmorSections.IndexOfByPredicate([ArmorId](const FOVArmorDefinition& Armor) { return Armor.ArmorId == ArmorId; });
}

int32 UShipLayoutDataAsset::ResolveArmor(FVector LocalPoint, FVector LocalNormal) const
{
    if (!OVLayout::Finite(LocalPoint) || !OVLayout::Finite(LocalNormal) || !OVLayout::Finite(LocalHalfExtent)
        || LocalHalfExtent.GetMin() <= 0) return INDEX_NONE;
    const FVector Normalized = LocalPoint / LocalHalfExtent;
    const FVector Direction = Normalized.IsNearlyZero() ? LocalNormal : Normalized;
    if (Direction.IsNearlyZero()) return INDEX_NONE;
    const FVector Magnitude = Direction.GetAbs();
    EOVArmorFace Face;
    // Deterministic corner/edge priority: X, then Y, then Z.
    if (Magnitude.X >= Magnitude.Y && Magnitude.X >= Magnitude.Z) Face = Direction.X >= 0 ? EOVArmorFace::Front : EOVArmorFace::Rear;
    else if (Magnitude.Y >= Magnitude.Z) Face = Direction.Y >= 0 ? EOVArmorFace::Right : EOVArmorFace::Left;
    else Face = Direction.Z >= 0 ? EOVArmorFace::Top : EOVArmorFace::Bottom;
    FVector2D Point = OVLayout::UV(Face, Normalized);
    Point.X = FMath::Clamp(Point.X, -1.0, 1.0); Point.Y = FMath::Clamp(Point.Y, -1.0, 1.0);
    for (int32 Index = 0; Index < ArmorSections.Num(); ++Index)
    {
        const FOVArmorDefinition& Armor = ArmorSections[Index];
        if (Armor.Face == Face && OVLayout::CellContains(Point.X, Armor.NormalizedMin.X, Armor.NormalizedMax.X)
            && OVLayout::CellContains(Point.Y, Armor.NormalizedMin.Y, Armor.NormalizedMax.Y)) return Index;
    }
    return INDEX_NONE;
}

int32 UShipLayoutDataAsset::MapArmorToSlot(int32 ArmorIndex, FVector LocalPoint) const
{
    if (!ArmorSections.IsValidIndex(ArmorIndex) || DeckCount < 1 || HalfHeight <= 0 || !OVLayout::Finite(LocalPoint)) return INDEX_NONE;
    const FOVArmorDefinition& Armor = ArmorSections[ArmorIndex];
    const int32 Deck = Armor.Face == EOVArmorFace::Top ? DeckCount : Armor.Face == EOVArmorFace::Bottom ? 1
        : FMath::Clamp(FMath::FloorToInt((LocalPoint.Z + HalfHeight) / (2.0 * HalfHeight) * DeckCount) + 1, 1, DeckCount);
    FVector Point = LocalPoint;
    if (Armor.Face == EOVArmorFace::Front) Point.X = LocalHalfExtent.X;
    if (Armor.Face == EOVArmorFace::Rear) Point.X = -LocalHalfExtent.X;
    if (Armor.Face == EOVArmorFace::Left) Point.Y = -LocalHalfExtent.Y;
    if (Armor.Face == EOVArmorFace::Right) Point.Y = LocalHalfExtent.Y;
    int32 Best = INDEX_NONE;
    double BestDistance = TNumericLimits<double>::Max();
    for (FName SlotId : Armor.InnerSlotIds)
    {
        const int32 Index = GetSlotIndex(SlotId);
        if (!Slots.IsValidIndex(Index) || Slots[Index].DeckId != Deck) continue;
        const FOVSlotDefinition& Slot = Slots[Index];
        const double Distance = FMath::Square(Point.X - Slot.LocalCenter.X) + FMath::Square(Point.Y - Slot.LocalCenter.Y);
        const bool PositiveBoundary = Best != INDEX_NONE && (Slot.LocalCenter.X > Slots[Best].LocalCenter.X
            || (Slot.LocalCenter.X == Slots[Best].LocalCenter.X && Slot.LocalCenter.Y > Slots[Best].LocalCenter.Y));
        if (Distance < BestDistance - .000001 || (FMath::IsNearlyEqual(Distance, BestDistance, .000001) && PositiveBoundary))
        {
            Best = Index; BestDistance = Distance;
        }
    }
    return Best;
}

int32 UShipLayoutDataAsset::ResolveHit(FVector LocalPoint, FVector LocalNormal) const
{
    return MapArmorToSlot(ResolveArmor(LocalPoint, LocalNormal), LocalPoint);
}

bool UShipLayoutDataAsset::ValidateLayout(TArray<FString>& Errors) const
{
    Errors.Reset();
    auto Require = [&](bool Condition, const FString& Error) { if (!Condition) Errors.Add(Error); };
    Require(DeckCount >= 1 && DeckCount <= 3, TEXT("DeckCount must be 1..3"));
    Require(OVLayout::Finite(LocalHalfExtent) && LocalHalfExtent.GetMin() > 0, TEXT("Local bounds must be finite and positive"));
    Require(FMath::IsFinite(HalfHeight) && HalfHeight > 0 && FMath::IsNearlyEqual(double(HalfHeight), LocalHalfExtent.Z), TEXT("HalfHeight must match local bounds"));
    Require(FMath::IsFinite(MaxHullHP) && MaxHullHP > 0, TEXT("Hull HP must be positive"));
    Require(FMath::IsFinite(BaseRadarRange) && BaseRadarRange >= 0 && FMath::IsFinite(WeaponTier) && WeaponTier > 0, TEXT("Invalid radar range or weapon tier"));
    Require(Slots.Num() > 0, TEXT("Layout has no slots"));
    TSet<FName> SlotIds;
    TSet<FString> DeckColumns;
    TMap<EOVFacilityType, int32> Counts;
    TMap<FName, FVector2D> ColumnCenters;
    for (const FOVSlotDefinition& Slot : Slots)
    {
        Require(!Slot.SlotId.IsNone() && !SlotIds.Contains(Slot.SlotId), TEXT("Duplicate or empty SlotId"));
        SlotIds.Add(Slot.SlotId);
        Require(Slot.DeckId >= 1 && Slot.DeckId <= DeckCount, TEXT("Invalid slot deck"));
        const FString Key = FString::Printf(TEXT("%d:%s"), Slot.DeckId, *Slot.ColumnId.ToString());
        Require(!Slot.ColumnId.IsNone() && !DeckColumns.Contains(Key), TEXT("Duplicate or empty deck/column"));
        DeckColumns.Add(Key);
        const FVector2D XY(Slot.LocalCenter.X, Slot.LocalCenter.Y);
        if (const FVector2D* Previous = ColumnCenters.Find(Slot.ColumnId)) Require(Previous->Equals(XY, .01), TEXT("Vertical column centers differ"));
        else ColumnCenters.Add(Slot.ColumnId, XY);
        Require(OVLayout::Finite(Slot.LocalCenter) && OVLayout::Finite(Slot.HalfExtent) && Slot.HalfExtent.GetMin() > 0, TEXT("Invalid slot bounds"));
        Require(Slot.AllowedFacilities.Contains(Slot.Facility), TEXT("Facility violates slot placement constraint"));
        Require(static_cast<uint8>(Slot.Facility) <= static_cast<uint8>(EOVFacilityType::Security), TEXT("Unknown facility type"));
        ++Counts.FindOrAdd(Slot.Facility);
        Require(FMath::IsFinite(Slot.MaxArmor) && Slot.MaxArmor >= 0, TEXT("Invalid legacy armor authoring value"));
        Require(FMath::IsFinite(Slot.MaxFacilityHP) && (Slot.Facility == EOVFacilityType::None ? Slot.MaxFacilityHP >= 0 : Slot.MaxFacilityHP > 0), TEXT("Invalid facility HP"));
        Require(FMath::IsFinite(Slot.RequiredCrew) && (OVLayout::Uncrewed(Slot.Facility) ? Slot.RequiredCrew == 0 : Slot.RequiredCrew > 0), TEXT("Invalid crew requirement or unmanned exception"));
        Require(!(bSmallHull && Slot.Facility == EOVFacilityType::Hangar), TEXT("Small hulls cannot install hangars"));
        if (Slot.Facility == EOVFacilityType::Bridge)
            Require(BridgeAllowedSlots.Contains(Slot.SlotId) && (DeckCount != 3 || Slot.DeckId != 2), TEXT("Bridge must occupy an explicit outer-deck slot"));
    }
    Require(Counts.FindRef(EOVFacilityType::Core) == 1 && Counts.FindRef(EOVFacilityType::Bridge) == 1, TEXT("Exactly one core and bridge required"));
    Require(Counts.FindRef(EOVFacilityType::LifeSupport) > 0 && Counts.FindRef(EOVFacilityType::Engine) > 0, TEXT("Life support and engine required"));
    const int32 CoreIndex = GetSlotIndex(CoreSlotId);
    Require(Slots.IsValidIndex(CoreIndex), TEXT("Explicit CoreSlotId is missing"));
    if (Slots.IsValidIndex(CoreIndex))
    {
        const FOVSlotDefinition& Core = Slots[CoreIndex];
        Require(Core.Facility == EOVFacilityType::Core && FMath::IsNearlyZero(Core.LocalCenter.X) && FMath::IsNearlyZero(Core.LocalCenter.Y), TEXT("Core must occupy the explicit central slot"));
        Require(DeckCount != 3 || Core.DeckId == 2, TEXT("Three-deck core must be on 2F"));
    }
    for (FName Id : BridgeAllowedSlots)
    {
        const int32 Index = GetSlotIndex(Id);
        Require(Slots.IsValidIndex(Index), TEXT("Unknown bridge-allowed slot"));
        if (Slots.IsValidIndex(Index))
        {
            const FOVSlotDefinition& Slot = Slots[Index];
            Require((DeckCount != 3 || Slot.DeckId != 2) && (DeckCount != 1 || FMath::IsNearlyEqual(FMath::Abs(Slot.LocalCenter.X) + Slot.HalfExtent.X, LocalHalfExtent.X, .01)), TEXT("Bridge slot must satisfy outer placement"));
        }
    }
    Require(CannonSlots >= 0 && BeamSlots >= 0 && MissileSlots >= 0, TEXT("Weapon counts cannot be negative"));
    Require(CannonSlots + BeamSlots + MissileSlots == 0 || Counts.FindRef(EOVFacilityType::WeaponControl) > 0, TEXT("Armed ships require weapon control"));
    Require(CannonSlots == 0 || Counts.FindRef(EOVFacilityType::Ammo) > 0, TEXT("Cannons require ammo facility"));
    Require(BeamSlots == 0 || Counts.FindRef(EOVFacilityType::Capacitor) > 0, TEXT("Beams require capacitor"));
    Require(MissileSlots == 0 || Counts.FindRef(EOVFacilityType::MissileStorage) > 0, TEXT("Missiles require missile storage"));
    TSet<FName> ConnectionIds;
    TMap<FName, TArray<FName>> Neighbors;
    for (const FOVConnectionDefinition& Connection : Connections)
    {
        Require(!Connection.ConnectionId.IsNone() && !ConnectionIds.Contains(Connection.ConnectionId), TEXT("Duplicate or empty connection ID"));
        ConnectionIds.Add(Connection.ConnectionId);
        const int32 From = GetSlotIndex(Connection.FromSlot), To = GetSlotIndex(Connection.ToSlot);
        const bool Endpoints = Slots.IsValidIndex(From) && Slots.IsValidIndex(To) && From != To;
        Require(Endpoints, TEXT("Connection has invalid endpoints"));
        if (!Endpoints) continue;
        const FOVSlotDefinition& A = Slots[From]; const FOVSlotDefinition& B = Slots[To];
        Require(A.ConnectionIds.Contains(Connection.ConnectionId) && B.ConnectionIds.Contains(Connection.ConnectionId), TEXT("Connection missing from endpoint references"));
        Require(Connection.Kind == EOVConnectionKind::Vertical
            ? (A.ColumnId == B.ColumnId && FMath::Abs(A.DeckId - B.DeckId) == 1)
            : ((Connection.Kind == EOVConnectionKind::Door || Connection.Kind == EOVConnectionKind::Corridor) && A.DeckId == B.DeckId), TEXT("Passage kind contradicts floor/column geometry"));
        Neighbors.FindOrAdd(A.SlotId).Add(B.SlotId); Neighbors.FindOrAdd(B.SlotId).Add(A.SlotId);
    }
    for (const FOVSlotDefinition& Slot : Slots)
    {
        TSet<FName> Seen;
        for (FName Id : Slot.ConnectionIds)
        {
            const FOVConnectionDefinition* Link = Connections.FindByPredicate([Id](const FOVConnectionDefinition& C) { return C.ConnectionId == Id; });
            Require(!Seen.Contains(Id) && Link && (Link->FromSlot == Slot.SlotId || Link->ToSlot == Slot.SlotId), TEXT("Dangling, unrelated or repeated slot connection"));
            Seen.Add(Id);
        }
    }
    if (Slots.Num() > 0)
    {
        TSet<FName> Reached; TArray<FName> Queue = { Slots[0].SlotId }; Reached.Add(Slots[0].SlotId);
        for (int32 Cursor = 0; Cursor < Queue.Num(); ++Cursor)
            for (FName Next : Neighbors.FindRef(Queue[Cursor])) if (!Reached.Contains(Next)) { Reached.Add(Next); Queue.Add(Next); }
        Require(Reached.Num() == Slots.Num(), TEXT("Passage graph must connect every slot"));
    }
    TSet<FName> ArmorIds;
    TSet<FString> FaceCells;
    int32 FaceCounts[6] = {};
    for (const FOVArmorDefinition& Armor : ArmorSections)
    {
        Require(!Armor.ArmorId.IsNone() && !ArmorIds.Contains(Armor.ArmorId) && !SlotIds.Contains(Armor.ArmorId), TEXT("Armor ID must be unique and distinct from slot ID"));
        ArmorIds.Add(Armor.ArmorId);
        const int32 Face = static_cast<int32>(Armor.Face);
        Require(Face >= 0 && Face < 6, TEXT("Invalid armor face"));
        if (Face < 0 || Face >= 6) continue;
        ++FaceCounts[Face];
        const int32 Rows = OVLayout::IsEnd(Armor.Face) ? 1 : DeckCount, Columns = OVLayout::IsEnd(Armor.Face) ? 1 : 3;
        const FString Key = FString::Printf(TEXT("%d:%d:%d"), Face, Armor.Row, Armor.Column);
        Require(!FaceCells.Contains(Key) && Armor.Row >= 0 && Armor.Row < Rows && Armor.Column >= 0 && Armor.Column < Columns, TEXT("Duplicate or invalid armor grid cell"));
        FaceCells.Add(Key);
        Require(OVLayout::Finite(Armor.NormalizedMin) && OVLayout::Finite(Armor.NormalizedMax) && Rows > 0, TEXT("Invalid armor normalized bounds"));
        if (Rows > 0)
        {
            const FVector2D ExpectedMin(-1.0 + 2.0 * Armor.Column / Columns, -1.0 + 2.0 * Armor.Row / Rows);
            const FVector2D ExpectedMax(-1.0 + 2.0 * (Armor.Column + 1) / Columns, -1.0 + 2.0 * (Armor.Row + 1) / Rows);
            Require(Armor.NormalizedMin.Equals(ExpectedMin, .000001) && Armor.NormalizedMax.Equals(ExpectedMax, .000001), TEXT("Armor bounds must match authored row/column partition"));
        }
        Require(OVLayout::Finite(Armor.LocalCenter) && OVLayout::Finite(Armor.HalfExtent) && Armor.HalfExtent.GetMin() > 0 && FMath::IsFinite(Armor.MaxArmor) && Armor.MaxArmor > 0, TEXT("Invalid armor shape or durability"));
        TSet<FName> Linked;
        for (FName Id : Armor.InnerSlotIds)
        {
            const int32 Index = GetSlotIndex(Id);
            Require(Slots.IsValidIndex(Index) && !Linked.Contains(Id), TEXT("Unknown or duplicate armor-to-slot link"));
            Linked.Add(Id);
            if (Slots.IsValidIndex(Index)) Require(OVLayout::SurfaceSlot(Slots[Index], Armor.Face, LocalHalfExtent, DeckCount)
                && OVLayout::SectionOverlapsSlot(Armor, Slots[Index], LocalHalfExtent), TEXT("Armor-to-slot link does not overlap its face"));
        }
        for (const FOVSlotDefinition& Slot : Slots)
            if (OVLayout::SurfaceSlot(Slot, Armor.Face, LocalHalfExtent, DeckCount) && OVLayout::SectionOverlapsSlot(Armor, Slot, LocalHalfExtent))
                Require(Linked.Contains(Slot.SlotId), TEXT("Armor-to-slot mapping omits an overlapping room"));
        Require(Armor.InnerSlotIds.Num() > 0, TEXT("Armor section has no internal mapping"));
    }
    for (int32 Face = 0; Face < 6; ++Face) Require(FaceCounts[Face] == (Face < 2 ? 1 : 3 * DeckCount), TEXT("Face section count violates 14/26/38 layout"));
    TSet<FName> WeaponIds;
    int32 MountCounts[4] = {};
    for (const FOVWeaponMountDefinition& Mount : WeaponMounts)
    {
        Require(!Mount.WeaponId.IsNone() && !WeaponIds.Contains(Mount.WeaponId) && !SlotIds.Contains(Mount.WeaponId) && !ArmorIds.Contains(Mount.WeaponId), TEXT("Weapon ID must be unique and independent"));
        WeaponIds.Add(Mount.WeaponId);
        const int32 Type = static_cast<int32>(Mount.Type);
        Require(Type >= 0 && Type < 4 && OVLayout::Finite(Mount.LocalPosition) && FMath::IsFinite(Mount.MaxHP) && Mount.MaxHP > 0 && FMath::IsFinite(Mount.Tier) && Mount.Tier > 0 && FMath::IsFinite(Mount.ExplosionTierCoefficient) && Mount.ExplosionTierCoefficient >= 0, TEXT("Invalid weapon mount"));
        if (Type >= 0 && Type < 4) ++MountCounts[Type];
    }
    Require(MountCounts[0] == CannonSlots && MountCounts[1] == BeamSlots && MountCounts[2] == MissileSlots, TEXT("Weapon count metadata must match independent mounts"));
    return Errors.IsEmpty();
}

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVLayoutMappingTest, "OrbitalVanguard.Layout.SixFacesAndBoundaryMapping", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOVLayoutMappingTest::RunTest(const FString& Parameters)
{
    for (int32 Decks = 1; Decks <= 3; ++Decks)
    {
        UShipLayoutDataAsset* Layout = UShipLayoutDataAsset::MakeTestLayout(GetTransientPackage(), Decks);
        TArray<FString> Errors;
        TestTrue(FString::Printf(TEXT("%d-deck authored layout valid: %s"), Decks, *FString::Join(Errors, TEXT(";"))), Layout->ValidateLayout(Errors));
        TestEqual(TEXT("Independent armor count is 14/26/38"), Layout->ArmorSections.Num(), 2 + 12 * Decks);
        const FVector Points[6] = {FVector(175, 0, 0), FVector(-175, 0, 0), FVector(0, -75, 0), FVector(0, 75, 0), FVector(0, 0, 30), FVector(0, 0, -30)};
        for (int32 Face = 0; Face < 6; ++Face)
        {
            const int32 Armor = Layout->ResolveArmor(Points[Face], Points[Face].GetSafeNormal());
            TestTrue(TEXT("Every face resolves to armor"), Layout->ArmorSections.IsValidIndex(Armor));
            if (!Layout->ArmorSections.IsValidIndex(Armor)) continue;
            TestEqual(TEXT("Face identity follows UE axes"), int32(Layout->ArmorSections[Armor].Face), Face);
            TestTrue(TEXT("Armor maps to an internal slot"), Layout->Slots.IsValidIndex(Layout->MapArmorToSlot(Armor, Points[Face])));
        }
        const int32 Upper = Layout->ResolveHit(FVector(0, 0, 30), FVector::UpVector);
        const int32 Lower = Layout->ResolveHit(FVector(0, 0, -30), -FVector::UpVector);
        TestEqual(TEXT("Top maps to highest floor"), Layout->Slots[Upper].DeckId, Decks);
        TestEqual(TEXT("Bottom maps to floor one"), Layout->Slots[Lower].DeckId, 1);
        const int32 Front = Layout->ResolveArmor(FVector(175, 0, 0), FVector::ForwardVector);
        TestEqual(TEXT("Front is one section linked to several rooms"), Layout->ArmorSections[Front].InnerSlotIds.Num(), 3 * Decks);
        const int32 LowFront = Layout->ResolveHit(FVector(175, -50, -29), FVector::ForwardVector);
        const int32 HighFront = Layout->ResolveHit(FVector(175, 50, 29), FVector::ForwardVector);
        TestTrue(TEXT("One front armor routes distinct Y/Z impacts to distinct rooms"), LowFront != HighFront);
        TestEqual(TEXT("Front lower hit uses lowest deck"), Layout->Slots[LowFront].DeckId, 1);
        TestEqual(TEXT("Front upper hit uses highest deck"), Layout->Slots[HighFront].DeckId, Decks);
        const int32 Corner = Layout->ResolveArmor(FVector(175, 75, 30), FVector::UpVector);
        TestEqual(TEXT("Exact corner ties choose X before Y/Z"), int32(Layout->ArmorSections[Corner].Face), int32(EOVArmorFace::Front));
        const double BoundaryX = Layout->LocalHalfExtent.X * (-1.0 + 2.0 / 3.0);
        const int32 GridBoundary = Layout->ResolveArmor(FVector(BoundaryX, 75, 0), FVector::RightVector);
        TestEqual(TEXT("Exact column boundary belongs to positive cell"), Layout->ArmorSections[GridBoundary].Column, 1);
        const int32 MidHit = Layout->ResolveHit(FVector(0, 75, 0), FVector::RightVector);
        TestEqual(TEXT("Zero-height side hits use upper half or central third"), Layout->Slots[MidHit].DeckId, Decks == 1 ? 1 : 2);
        TestEqual(TEXT("Malformed zero direction is rejected"), Layout->ResolveArmor(FVector::ZeroVector, FVector::ZeroVector), INDEX_NONE);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVLayoutValidationTest, "OrbitalVanguard.Layout.RejectInvalidAuthoring", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOVLayoutValidationTest::RunTest(const FString& Parameters)
{
    auto Invalid = [&](const TCHAR* Label, TFunctionRef<void(UShipLayoutDataAsset*)> Mutate)
    {
        UShipLayoutDataAsset* Layout = UShipLayoutDataAsset::MakeTestLayout(GetTransientPackage(), 3);
        Mutate(Layout); TArray<FString> Errors;
        TestFalse(Label, Layout->ValidateLayout(Errors));
        TestTrue(TEXT("Invalid data explains the failure"), Errors.Num() > 0);
    };
    Invalid(TEXT("Duplicate room ID rejected"), [](UShipLayoutDataAsset* L) { L->Slots[1].SlotId = L->Slots[0].SlotId; });
    Invalid(TEXT("Missing weapon supply rejected"), [](UShipLayoutDataAsset* L) { for (FOVSlotDefinition& S : L->Slots) if (S.Facility == EOVFacilityType::Ammo) { S.Facility = EOVFacilityType::None; S.RequiredCrew = 0; } });
    Invalid(TEXT("Dangling graph endpoint rejected"), [](UShipLayoutDataAsset* L) { L->Connections[0].ToSlot = TEXT("Missing"); });
    Invalid(TEXT("Wrong explicit core slot rejected"), [](UShipLayoutDataAsset* L) { L->CoreSlotId = L->Slots[0].SlotId; });
    Invalid(TEXT("Core crew exception enforced"), [](UShipLayoutDataAsset* L) { L->Slots[L->GetSlotIndex(L->CoreSlotId)].RequiredCrew = 10; });
    Invalid(TEXT("Small hull hangar prohibited"), [](UShipLayoutDataAsset* L) { L->bSmallHull = true; });
    Invalid(TEXT("Missing armor section rejected"), [](UShipLayoutDataAsset* L) { L->ArmorSections.Pop(); });
    Invalid(TEXT("Incomplete front mapping rejected"), [](UShipLayoutDataAsset* L) { L->ArmorSections[0].InnerSlotIds.Pop(); });
    Invalid(TEXT("Weapon counts cannot diverge from mounts"), [](UShipLayoutDataAsset* L) { L->WeaponMounts.Pop(); });
    Invalid(TEXT("Bridge cannot occupy middle floor"), [](UShipLayoutDataAsset* L) { for (FOVSlotDefinition& S : L->Slots) if (S.Facility == EOVFacilityType::Bridge) S.DeckId = 2; });
    return true;
}
#endif
