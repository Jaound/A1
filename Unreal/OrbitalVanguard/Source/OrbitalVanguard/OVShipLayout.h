#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "OVShipLayout.generated.h"

UENUM(BlueprintType)
enum class EOVFacilityType : uint8
{
    None, Core, Bridge, LifeSupport, Engine, WeaponControl, Ammo, Capacitor,
    MissileStorage, ShieldControl, Radar, Interception, Boarding, Hangar,
    FighterControl, ResourceStorage, Security
};

UENUM(BlueprintType)
enum class EOVConnectionKind : uint8
{
    Corridor, Door, Vertical
};

USTRUCT(BlueprintType)
struct ORBITALVANGUARD_API FOVSlotDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Layout") FName SlotId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Layout") int32 DeckId = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Layout") FName ColumnId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Layout") FVector LocalCenter = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Layout") FVector HalfExtent = FVector(35, 25, 30);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Layout") EOVFacilityType Facility = EOVFacilityType::None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Layout") float MaxFacilityHP = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Layout") float MaxArmor = 100;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Layout") float RequiredCrew = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Layout") TArray<EOVFacilityType> AllowedFacilities;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Layout") TArray<FName> ConnectionIds;
};

USTRUCT(BlueprintType)
struct ORBITALVANGUARD_API FOVConnectionDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Layout") FName ConnectionId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Layout") FName FromSlot;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Layout") FName ToSlot;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Layout") EOVConnectionKind Kind = EOVConnectionKind::Corridor;
};

/** Immutable design data. Per-ship damage, occupation and crew belong to the systems component. */
UCLASS(BlueprintType)
class ORBITALVANGUARD_API UShipLayoutDataAsset : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Layout") int32 DeckCount = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Layout") bool bSmallHull = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Layout") FName CoreSlotId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Layout") TArray<FName> BridgeAllowedSlots;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Layout") TArray<FOVSlotDefinition> Slots;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Layout") TArray<FOVConnectionDefinition> Connections;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Layout") int32 CannonSlots = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Layout") int32 BeamSlots = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Layout") int32 MissileSlots = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Layout") float WeaponTier = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Layout") float BaseRadarRange = 6000;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Layout") float HalfHeight = 30;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Layout") float MaxHullHP = 3000;

    UFUNCTION(BlueprintCallable, Category="Layout")
    static UShipLayoutDataAsset* MakeTestLayout(UObject* Outer, int32 Decks);

    UFUNCTION(BlueprintPure, Category="Layout") int32 GetSlotIndex(FName SlotId) const;
    UFUNCTION(BlueprintCallable, Category="Layout") bool ValidateLayout(TArray<FString>& Errors) const;
    UFUNCTION(BlueprintPure, Category="Layout") int32 ResolveHit(FVector LocalPoint, FVector LocalNormal) const;
};
