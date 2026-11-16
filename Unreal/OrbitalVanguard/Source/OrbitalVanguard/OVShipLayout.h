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

UENUM(BlueprintType)
enum class EOVArmorFace : uint8 { Front, Rear, Left, Right, Top, Bottom };

UENUM(BlueprintType)
enum class EOVWeaponType : uint8 { Cannon, Beam, Missile, Interception };

USTRUCT(BlueprintType)
struct ORBITALVANGUARD_API FOVArmorDefinition
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Armor") FName ArmorId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Armor") EOVArmorFace Face = EOVArmorFace::Front;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Armor") int32 Row = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Armor") int32 Column = 0;
    // UV: front/rear = Y/Z; left/right = X/Z; top/bottom = X/Y.
    // Rows follow V, columns follow U. Shared boundaries belong to the positive cell.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Armor") FVector2D NormalizedMin = FVector2D(-1, -1);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Armor") FVector2D NormalizedMax = FVector2D(1, 1);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Armor") FVector LocalCenter = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Armor") FVector HalfExtent = FVector::OneVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Armor") float MaxArmor = 100;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Armor") TArray<FName> InnerSlotIds;
};

USTRUCT(BlueprintType)
struct ORBITALVANGUARD_API FOVWeaponMountDefinition
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weapon") FName WeaponId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weapon") EOVWeaponType Type = EOVWeaponType::Cannon;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weapon") float Tier = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weapon") FVector LocalPosition = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weapon") float MaxHP = 200;
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
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Layout") FName LayoutVersion = TEXT("PrototypeInterior_v1");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Layout") int32 DeckCount = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Layout") bool bSmallHull = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Layout") FName CoreSlotId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Layout") TArray<FName> BridgeAllowedSlots;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Layout") TArray<FOVSlotDefinition> Slots;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Layout") TArray<FOVConnectionDefinition> Connections;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Layout") TArray<FOVArmorDefinition> ArmorSections;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Layout") TArray<FOVWeaponMountDefinition> WeaponMounts;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Layout") FVector LocalHalfExtent = FVector(175, 75, 30);
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
    UFUNCTION(BlueprintPure, Category="Layout") int32 ResolveArmor(FVector LocalPoint, FVector LocalNormal) const;
    UFUNCTION(BlueprintPure, Category="Layout") int32 GetArmorIndex(FName ArmorId) const;
    UFUNCTION(BlueprintPure, Category="Layout") int32 MapArmorToSlot(int32 ArmorIndex, FVector LocalPoint) const;
};
