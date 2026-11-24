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
                    if (Type != static_cast<int32>(EOVFacilityType::Engine)) Slot.AllowedFacilities.Add(static_cast<EOVFacilityType>(Type));
                }
                if (Slot.SlotId == Layout->CoreSlotId) Slot.AllowedFacilities.Add(EOVFacilityType::Core);
                if ((Decks != 3 || Deck != 2) && (X == 1 || X == 4) && Y == 1)
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
    Install(Layout->GetSlotIndex(OVLayout::SlotName(Decks, 1, 1)), EOVFacilityType::Bridge);
    for (int32 Row = 0; Row < 3; ++Row)
    {
        const int32 Index = Layout->GetSlotIndex(OVLayout::SlotName(1, 0, Row));
        Install(Index, EOVFacilityType::Engine);
        Layout->Slots[Index].AllowedFacilities = { EOVFacilityType::Engine };
    }
    // This range exercises propagation through three adjacent magazines.
    Install(Layout->GetSlotIndex(OVLayout::SlotName(1, 1, 0)), EOVFacilityType::Ammo);
    Install(Layout->GetSlotIndex(OVLayout::SlotName(1, 2, 0)), EOVFacilityType::Capacitor);
    Install(Layout->GetSlotIndex(OVLayout::SlotName(1, 3, 0)), EOVFacilityType::MissileStorage);
    TArray<EOVFacilityType> Facilities = {
        EOVFacilityType::LifeSupport, EOVFacilityType::WeaponControl,
        EOVFacilityType::ShieldControl, EOVFacilityType::Radar, EOVFacilityType::Security
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

int32 UShipLayoutDataAsset::GetMaxFloorSlotsForClass(EOVHullSizeClass SizeClass)
{
    switch (SizeClass)
    {
    case EOVHullSizeClass::Small:      return 7;
    case EOVHullSizeClass::Medium:     return 9;
    case EOVHullSizeClass::Large:      return 12;
    case EOVHullSizeClass::ExtraLarge: return 15;
    default:                           return 0;
    }
}

bool UShipLayoutDataAsset::ComputeExpectedD126DeckSlots(EOVHullSizeClass SizeClass, int32 Decks, bool bTopHeavy, TArray<int32>& OutCounts)
{
    OutCounts.Reset();
    const int32 MaxFloor = GetMaxFloorSlotsForClass(SizeClass);
    if (MaxFloor <= 0 || Decks < 1 || Decks > 3) return false;

    // D126 R17 allowed deck counts per hull size class:
    // Small: 1F only (7)
    // Medium: 1F (9) or 2F (9-6 / 6-9)
    // Large: 2F (12-9 / 9-12) or 3F (12-9-6 / 6-9-12)
    // ExtraLarge: 3F only (15-12-9 / 9-12-15)
    if (SizeClass == EOVHullSizeClass::Small && Decks != 1) return false;
    if (SizeClass == EOVHullSizeClass::Medium && (Decks < 1 || Decks > 2)) return false;
    if (SizeClass == EOVHullSizeClass::Large && (Decks < 2 || Decks > 3)) return false;
    if (SizeClass == EOVHullSizeClass::ExtraLarge && Decks != 3) return false;

    OutCounts.SetNum(Decks);
    for (int32 D = 0; D < Decks; ++D)
    {
        const int32 StepFromPeak = bTopHeavy ? (Decks - 1 - D) : D;
        const int32 Count = MaxFloor - StepFromPeak * 3;
        if (Count <= 0) return false;
        OutCounts[D] = Count;
    }
    return true;
}

UShipLayoutDataAsset* UShipLayoutDataAsset::MakeD126Layout(
    UObject* Outer,
    EOVHullSizeClass SizeClass,
    int32 Decks,
    bool bTopHeavy,
    int32 MaxHangars,
    bool bAircraftCapable)
{
    TArray<int32> DeckSlots;
    if (!ComputeExpectedD126DeckSlots(SizeClass, Decks, bTopHeavy, DeckSlots)) return nullptr;

    UShipLayoutDataAsset* Layout = NewObject<UShipLayoutDataAsset>(Outer ? Outer : GetTransientPackage());
    Layout->LayoutVersion = TEXT("D126_Grid_v1");
    Layout->DeckCount = Decks;
    Layout->HullSizeClass = SizeClass;
    Layout->bSmallHull = (SizeClass == EOVHullSizeClass::Small);
    const bool bCanHaveHangars = (SizeClass == EOVHullSizeClass::Large || SizeClass == EOVHullSizeClass::ExtraLarge);
    Layout->MaxHangarInstallations = bCanHaveHangars ? FMath::Max(0, MaxHangars) : 0;
    Layout->bAllowAircraftOperations = bCanHaveHangars && bAircraftCapable && Layout->MaxHangarInstallations > 0;
    Layout->MaxHullHP = 2000 + Decks * 1000;
    Layout->CannonSlots = Layout->BeamSlots = Layout->MissileSlots = 0;

    // 3-deck core is strictly placed on 2F; 1-2 deck core is placed on 1F at exact (0,0) center column XY_2_1.
    const int32 CoreDeck = (Decks == 3) ? 2 : 1;
    Layout->CoreSlotId = FName(*FString::Printf(TEXT("D%d_X2_Y1"), CoreDeck));

    const float DeckHalfHeight = Layout->HalfHeight / Decks;

    // Exact symmetric 5-band X-partition (X = -140, -70, 0, +70, +140; HalfX = 35)
    // Every count {6, 7, 9, 12, 15} tiles 100% of [-175..+175] x [-75..+75] with zero gaps,
    // guarantees exact (0,0) column XY_2_1 and exact bow (+140,0) column XY_4_1 on EVERY deck.
    auto BandCountsForFloor = [](int32 FloorSlots, int32 OutBands[5])
    {
        switch (FloorSlots)
        {
        case 15: OutBands[0]=3; OutBands[1]=3; OutBands[2]=3; OutBands[3]=3; OutBands[4]=3; return true;
        case 12: OutBands[0]=2; OutBands[1]=3; OutBands[2]=3; OutBands[3]=3; OutBands[4]=1; return true;
        case 9:  OutBands[0]=1; OutBands[1]=2; OutBands[2]=3; OutBands[3]=2; OutBands[4]=1; return true;
        case 7:  OutBands[0]=1; OutBands[1]=1; OutBands[2]=3; OutBands[3]=1; OutBands[4]=1; return true;
        case 6:  OutBands[0]=1; OutBands[1]=2; OutBands[2]=1; OutBands[3]=1; OutBands[4]=1; return true;
        default: return false;
        }
    };

    TArray<TArray<TArray<FName>>> DeckBandSlots; // [DeckIdx][BandX 0..4][Row]
    DeckBandSlots.SetNum(Decks);

    for (int32 Deck = 1; Deck <= Decks; ++Deck)
    {
        int32 Bands[5] = {};
        if (!BandCountsForFloor(DeckSlots[Deck - 1], Bands)) return nullptr;
        DeckBandSlots[Deck - 1].SetNum(5);

        const double ZCenter = -Layout->HalfHeight + DeckHalfHeight * (2 * Deck - 1);
        for (int32 BX = 0; BX < 5; ++BX)
        {
            const int32 RowsInBand = Bands[BX];
            const double XCenter = (BX - 2) * 70.0;
            for (int32 R = 0; R < RowsInBand; ++R)
            {
                double YCenter = 0.0;
                double HalfY = 75.0;
                FName ColId;
                FName SlotId;
                if (RowsInBand == 1)
                {
                    YCenter = 0.0;
                    HalfY = 75.0;
                    ColId = FName(*FString::Printf(TEXT("XY_%d_1"), BX));
                    SlotId = FName(*FString::Printf(TEXT("D%d_X%d_Y1"), Deck, BX));
                }
                else if (RowsInBand == 2)
                {
                    YCenter = (R == 0) ? -37.5 : 37.5;
                    HalfY = 37.5;
                    ColId = FName(*FString::Printf(TEXT("XY_%d_H%d"), BX, R));
                    SlotId = FName(*FString::Printf(TEXT("D%d_X%d_H%d"), Deck, BX, R));
                }
                else
                {
                    YCenter = (R - 1) * 50.0;
                    HalfY = 25.0;
                    ColId = FName(*FString::Printf(TEXT("XY_%d_%d"), BX, R));
                    SlotId = FName(*FString::Printf(TEXT("D%d_X%d_Y%d"), Deck, BX, R));
                }

                FOVSlotDefinition Slot;
                Slot.SlotId = SlotId;
                Slot.DeckId = Deck;
                Slot.ColumnId = ColId;
                Slot.LocalCenter = FVector(XCenter, YCenter, ZCenter);
                Slot.HalfExtent = FVector(35.0, HalfY, DeckHalfHeight);
                Slot.AllowedFacilities.Add(EOVFacilityType::None);
                for (int32 Type = static_cast<int32>(EOVFacilityType::LifeSupport); Type <= static_cast<int32>(EOVFacilityType::Security); ++Type)
                {
                    const EOVFacilityType Fac = static_cast<EOVFacilityType>(Type);
                    if (Fac == EOVFacilityType::Hangar && (!bCanHaveHangars || Layout->MaxHangarInstallations <= 0)) continue;
                    if (Fac == EOVFacilityType::FighterControl && !Layout->bAllowAircraftOperations) continue;
                    if (Fac != EOVFacilityType::Engine) Slot.AllowedFacilities.Add(Fac);
                }
                if (Slot.SlotId == Layout->CoreSlotId)
                {
                    Slot.AllowedFacilities.Add(EOVFacilityType::Core);
                }
                const bool bOuterDeck = (Decks != 3 || Deck != 2);
                if (bOuterDeck && ((BX == 4 && FMath::IsNearlyZero(YCenter)) || BX == 1))
                {
                    Layout->BridgeAllowedSlots.AddUnique(Slot.SlotId);
                    Slot.AllowedFacilities.AddUnique(EOVFacilityType::Bridge);
                }
                Layout->Slots.Add(Slot);
                DeckBandSlots[Deck - 1][BX].Add(Slot.SlotId);
            }
        }
    }

    auto InstallAt = [&](int32 Index, EOVFacilityType Type)
    {
        if (!Layout->Slots.IsValidIndex(Index)) return;
        FOVSlotDefinition& Slot = Layout->Slots[Index];
        Slot.Facility = Type;
        Slot.MaxFacilityHP = Type == EOVFacilityType::Bridge ? 180.f : 100.f;
        Slot.RequiredCrew = OVLayout::Uncrewed(Type) ? 0.f : 10.f;
    };

    InstallAt(Layout->GetSlotIndex(Layout->CoreSlotId), EOVFacilityType::Core);
    const auto& Stern = DeckBandSlots[0][0];
    const int32 EngineIndex = Layout->GetSlotIndex(Stern[Stern.Num()/2]);
    InstallAt(EngineIndex, EOVFacilityType::Engine);
    Layout->Slots[EngineIndex].AllowedFacilities = { EOVFacilityType::Engine };
    const auto& AftBand = DeckBandSlots.Last()[1];
    const FName BridgeSlotId = AftBand[AftBand.Num()/2];
    InstallAt(Layout->GetSlotIndex(BridgeSlotId), EOVFacilityType::Bridge);

    // Base mandatory life support + engine; caller (OVCalibration or customizer) populates remaining weapon/role rooms.
    TArray<EOVFacilityType> BaseMandatory = { EOVFacilityType::LifeSupport };
    int32 Next = 0;
    for (int32 Index = 0; Index < Layout->Slots.Num() && Next < BaseMandatory.Num(); ++Index)
    {
        if (Layout->Slots[Index].Facility == EOVFacilityType::None)
        {
            InstallAt(Index, BaseMandatory[Next++]);
        }
    }

    // Build fully connected passage graph (Door within band, Corridor between adjacent X bands, Vertical along XY_2_1)
    auto Connect = [&](FName A, FName B, EOVConnectionKind Kind)
    {
        if (A == B) return;
        const int32 IA = Layout->GetSlotIndex(A);
        const int32 IB = Layout->GetSlotIndex(B);
        if (!Layout->Slots.IsValidIndex(IA) || !Layout->Slots.IsValidIndex(IB)) return;
        FOVConnectionDefinition Link;
        Link.ConnectionId = FName(*FString::Printf(TEXT("Passage_%03d"), Layout->Connections.Num()));
        Link.FromSlot = A;
        Link.ToSlot = B;
        Link.Kind = Kind;
        Layout->Slots[IA].ConnectionIds.Add(Link.ConnectionId);
        Layout->Slots[IB].ConnectionIds.Add(Link.ConnectionId);
        Layout->Connections.Add(Link);
    };

    for (int32 Deck = 1; Deck <= Decks; ++Deck)
    {
        const auto& Bands = DeckBandSlots[Deck - 1];
        for (int32 BX = 0; BX < 5; ++BX)
        {
            for (int32 R = 0; R + 1 < Bands[BX].Num(); ++R)
            {
                Connect(Bands[BX][R], Bands[BX][R + 1], EOVConnectionKind::Door);
            }
            if (BX < 4)
            {
                // Connect every room in band BX to its closest Y neighbor in band BX+1
                const auto& NextBand = Bands[BX + 1];
                TSet<TPair<FName, FName>> Added;
                for (FName FromId : Bands[BX])
                {
                    const double FromY = Layout->Slots[Layout->GetSlotIndex(FromId)].LocalCenter.Y;
                    FName BestTo = NextBand[0];
                    double BestDist = TNumericLimits<double>::Max();
                    for (FName ToId : NextBand)
                    {
                        const double Dist = FMath::Abs(Layout->Slots[Layout->GetSlotIndex(ToId)].LocalCenter.Y - FromY);
                        if (Dist < BestDist) { BestDist = Dist; BestTo = ToId; }
                    }
                    if (!Added.Contains({FromId, BestTo}))
                    {
                        Added.Add({FromId, BestTo});
                        Connect(FromId, BestTo, EOVConnectionKind::Corridor);
                    }
                }
                for (FName ToId : NextBand)
                {
                    const double ToY = Layout->Slots[Layout->GetSlotIndex(ToId)].LocalCenter.Y;
                    FName BestFrom = Bands[BX][0];
                    double BestDist = TNumericLimits<double>::Max();
                    for (FName FromId : Bands[BX])
                    {
                        const double Dist = FMath::Abs(Layout->Slots[Layout->GetSlotIndex(FromId)].LocalCenter.Y - ToY);
                        if (Dist < BestDist) { BestDist = Dist; BestFrom = FromId; }
                    }
                    if (!Added.Contains({BestFrom, ToId}))
                    {
                        Added.Add({BestFrom, ToId});
                        Connect(BestFrom, ToId, EOVConnectionKind::Corridor);
                    }
                }
            }
        }
        if (Deck < Decks)
        {
            const FName LowerShaft(*FString::Printf(TEXT("D%d_X2_Y1"), Deck));
            const FName UpperShaft(*FString::Printf(TEXT("D%d_X2_Y1"), Deck + 1));
            Connect(LowerShaft, UpperShaft, EOVConnectionKind::Vertical);
        }
    }

    // Preserve exact 14 / 26 / 38 armor section partitioning (independent of internal slot counts!)
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
                Armor.MaxArmor = Face == EOVArmorFace::Front ? 400.f : 100.f;
                for (const FOVSlotDefinition& Slot : Layout->Slots)
                {
                    if (OVLayout::SurfaceSlot(Slot, Face, Bounds, Decks) && OVLayout::SectionOverlapsSlot(Armor, Slot, Bounds))
                    {
                        Armor.InnerSlotIds.Add(Slot.SlotId);
                    }
                }
                Layout->ArmorSections.Add(Armor);
            }
        }
    }

    return Layout;
}

bool UShipLayoutDataAsset::AuditFacilityCapacity(
    const UShipLayoutDataAsset* Layout,
    const TArray<EOVFacilityType>& DesiredFacilities,
    TArray<EOVFacilityType>& OutUnplaced,
    FString& OutReport)
{
    OutUnplaced.Reset();
    OutReport.Reset();
    if (!Layout)
    {
        OutReport = TEXT("Audit failed: Layout is null");
        return false;
    }

    TMap<EOVFacilityType, int32> Installed;
    int32 FreeSlots = 0;
    for (const FOVSlotDefinition& Slot : Layout->Slots)
    {
        if (Slot.Facility == EOVFacilityType::None) ++FreeSlots;
        else ++Installed.FindOrAdd(Slot.Facility);
    }

    TMap<EOVFacilityType, int32> Needed;
    for (EOVFacilityType Fac : DesiredFacilities)
    {
        if (Fac != EOVFacilityType::None) ++Needed.FindOrAdd(Fac);
    }

    int32 AdditionalRequired = 0;
    for (const auto& Pair : Needed)
    {
        const int32 Have = Installed.FindRef(Pair.Key);
        if (Pair.Value > Have)
        {
            const int32 Deficit = Pair.Value - Have;
            for (int32 K = 0; K < Deficit; ++K)
            {
                if (AdditionalRequired < FreeSlots)
                {
                    ++AdditionalRequired;
                }
                else
                {
                    OutUnplaced.Add(Pair.Key);
                }
            }
        }
    }

    if (OutUnplaced.IsEmpty())
    {
        OutReport = FString::Printf(TEXT("OK: %d total slots accommodate all %d desired facilities (free remaining: %d)"),
            Layout->Slots.Num(), DesiredFacilities.Num(), FreeSlots - AdditionalRequired);
        return true;
    }

    TArray<FString> MissingNames;
    const UEnum* FacEnum = StaticEnum<EOVFacilityType>();
    for (EOVFacilityType Missing : OutUnplaced)
    {
        MissingNames.Add(FacEnum ? FacEnum->GetNameStringByValue(static_cast<int64>(Missing)) : FString::FromInt(int32(Missing)));
    }
    OutReport = FString::Printf(
        TEXT("D126 Capacity Deficit: %d slots available cannot hold %d desired facilities without merging/deleting (%d unplaced: %s)"),
        Layout->Slots.Num(), DesiredFacilities.Num(), OutUnplaced.Num(), *FString::Join(MissingNames, TEXT(", ")));
    return false;
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
    const FVector Direction = !CombatHullBoxes.IsEmpty() && !LocalNormal.IsNearlyZero() ? LocalNormal
        : Normalized.IsNearlyZero() ? LocalNormal : Normalized;
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
    Require(CombatHullBoxes.Num() <= 32, TEXT("Too many authored combat hull boxes"));
    for (const FOVCombatHullBox& Box : CombatHullBoxes)
    {
        const FVector Edge=Box.LocalCenter.GetAbs()+Box.HalfExtent;
        Require(OVLayout::Finite(Box.LocalCenter) && OVLayout::Finite(Box.HalfExtent) && Box.HalfExtent.GetMin() > 0
            && Edge.X<=LocalHalfExtent.X+.01 && Edge.Y<=LocalHalfExtent.Y+.01 && Edge.Z<=LocalHalfExtent.Z+.01,
            TEXT("Combat hull box must be finite, positive and inside original bounds"));
    }
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
        if (Slot.Facility == EOVFacilityType::Engine)
            Require(FMath::IsNearlyEqual(Slot.LocalCenter.X - Slot.HalfExtent.X, -LocalHalfExtent.X, .01), TEXT("Engine must occupy the stern"));
        Require(static_cast<uint8>(Slot.Facility) <= static_cast<uint8>(EOVFacilityType::Security), TEXT("Unknown facility type"));
        ++Counts.FindOrAdd(Slot.Facility);
        Require(FMath::IsFinite(Slot.MaxArmor) && Slot.MaxArmor >= 0, TEXT("Invalid legacy armor authoring value"));
        Require(FMath::IsFinite(Slot.MaxFacilityHP) && (Slot.Facility == EOVFacilityType::None ? Slot.MaxFacilityHP >= 0 : Slot.MaxFacilityHP > 0), TEXT("Invalid facility HP"));
        Require(FMath::IsFinite(Slot.RequiredCrew) && (OVLayout::Uncrewed(Slot.Facility) ? Slot.RequiredCrew == 0 : Slot.RequiredCrew > 0), TEXT("Invalid crew requirement or unmanned exception"));
        Require(!(bSmallHull && Slot.Facility == EOVFacilityType::Hangar), TEXT("Small hulls cannot install hangars"));
        if (HullSizeClass == EOVHullSizeClass::Medium)
        {
            Require(Slot.Facility != EOVFacilityType::Hangar, TEXT("Medium hulls cannot install hangars (D126 R15: Large+ only)"));
        }
        if (!bAllowAircraftOperations)
        {
            Require(Slot.Facility != EOVFacilityType::FighterControl, TEXT("Non-aircraft hull cannot install fighter control facility"));
        }
        if (Slot.Facility == EOVFacilityType::Bridge)
            Require(BridgeAllowedSlots.Contains(Slot.SlotId) && (DeckCount != 3 || Slot.DeckId != 2), TEXT("Bridge must occupy an explicit outer-deck slot"));
    }
    if (LayoutVersion == TEXT("D126_Grid_v1") || HullSizeClass != EOVHullSizeClass::Unspecified)
    {
        const int32 MaxFloor = GetMaxFloorSlotsForClass(HullSizeClass);
        Require(MaxFloor > 0, TEXT("D126 layout requires a valid HullSizeClass (Small..ExtraLarge)"));
        if (HullSizeClass == EOVHullSizeClass::Small)
            Require(DeckCount == 1, TEXT("D126 Small hulls must be 1 deck"));
        else if (HullSizeClass == EOVHullSizeClass::Medium)
            Require(DeckCount >= 1 && DeckCount <= 2, TEXT("D126 Medium hulls must be 1..2 decks"));
        else if (HullSizeClass == EOVHullSizeClass::Large)
            Require(DeckCount >= 2 && DeckCount <= 3, TEXT("D126 Large hulls must be 2..3 decks"));
        else if (HullSizeClass == EOVHullSizeClass::ExtraLarge)
            Require(DeckCount == 3, TEXT("D126 ExtraLarge hulls must be 3 decks only"));

        if (MaxFloor > 0 && DeckCount >= 1 && DeckCount <= 3)
        {
            TArray<int32> DeckSlotCounts;
            DeckSlotCounts.Init(0, DeckCount);
            for (const FOVSlotDefinition& Slot : Slots)
            {
                if (Slot.DeckId >= 1 && Slot.DeckId <= DeckCount) ++DeckSlotCounts[Slot.DeckId - 1];
            }
            TArray<int32> ExpectedBottomHeavy, ExpectedTopHeavy;
            const bool bHasBottom = ComputeExpectedD126DeckSlots(HullSizeClass, DeckCount, false, ExpectedBottomHeavy);
            const bool bHasTop = ComputeExpectedD126DeckSlots(HullSizeClass, DeckCount, true, ExpectedTopHeavy);
            const bool bMatchesMonotonic = (bHasBottom && DeckSlotCounts == ExpectedBottomHeavy)
                || (bHasTop && DeckSlotCounts == ExpectedTopHeavy);
            Require(bMatchesMonotonic, TEXT("D126 deck slot counts must decrease monotonically by 3 from bottom (1F) or top deck"));
        }
        if (HullSizeClass == EOVHullSizeClass::Small || HullSizeClass == EOVHullSizeClass::Medium)
        {
            Require(MaxHangarInstallations <= 0, TEXT("D126 R15: Small/Medium hulls must have MaxHangarInstallations <= 0"));
            Require(!bAllowAircraftOperations, TEXT("D126 R15: Small/Medium hulls cannot operate aircraft"));
        }
    }
    Require(Counts.FindRef(EOVFacilityType::Core) == 1 && Counts.FindRef(EOVFacilityType::Bridge) == 1, TEXT("Exactly one core and bridge required"));
    Require(MaxHangarInstallations >= -1, TEXT("Invalid hangar installation limit"));
    Require(MaxHangarInstallations < 0 || Counts.FindRef(EOVFacilityType::Hangar) <= MaxHangarInstallations,
        TEXT("Hangar installation limit exceeded"));
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
            Require((DeckCount != 3 || Slot.DeckId != 2) && (DeckCount != 1 || Slot.LocalCenter.X < 0 || FMath::IsNearlyEqual(Slot.LocalCenter.X + Slot.HalfExtent.X, LocalHalfExtent.X, .01)), TEXT("Bridge slot must satisfy aft or outer placement"));
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
        Require(Type >= 0 && Type < 4 && OVLayout::Finite(Mount.LocalPosition) && OVLayout::Finite(Mount.LocalSurfaceNormal) && FMath::IsFinite(Mount.MaxHP) && Mount.MaxHP > 0 && FMath::IsFinite(Mount.Tier) && Mount.Tier > 0 && FMath::IsFinite(Mount.ExplosionTierCoefficient) && Mount.ExplosionTierCoefficient >= 0, TEXT("Invalid weapon mount"));
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
