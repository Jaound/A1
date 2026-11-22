#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "OVTitleBackdropDataAsset.generated.h"

UENUM(BlueprintType)
enum class EOVTitleBackdropMode : uint8
{
    /** Spawns lightweight non-simulation StaticMeshComponent proxies (same rule as UOVShipPreview). */
    LiveProxyStage UMETA(DisplayName = "Live 3D Ship Proxy Stage"),
    /** Uses high-res 16:9 textures pre-captured from the same SM_Hull formations via Tools/CaptureTitleBackdrops.py. */
    PreRenderedInGameImage UMETA(DisplayName = "Pre-Rendered In-Game Model Image")
};

USTRUCT(BlueprintType)
struct ORBITALVANGUARD_API FOVTitleBackdropShipSlot
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TitleBackdrop")
    FName RoleTag;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TitleBackdrop")
    FString DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TitleBackdrop")
    FSoftObjectPath StaticMeshPath;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TitleBackdrop")
    FVector RelativeLocation = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TitleBackdrop")
    FRotator RelativeRotation = FRotator::ZeroRotator;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TitleBackdrop")
    FVector RelativeScale = FVector::OneVector;
};

USTRUCT(BlueprintType)
struct ORBITALVANGUARD_API FOVTitleBackdropFormation
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TitleBackdrop")
    FName FormationId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TitleBackdrop")
    FName LinkedScenarioId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TitleBackdrop")
    FString DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TitleBackdrop")
    FSoftObjectPath PreRenderedTexturePath;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TitleBackdrop")
    FVector CameraLocation = FVector(-3400.f, -1550.f, 780.f);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TitleBackdrop")
    FRotator CameraRotation = FRotator(-11.f, 22.f, 0.f);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TitleBackdrop")
    float CameraFov = 42.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TitleBackdrop")
    TArray<FOVTitleBackdropShipSlot> Ships;
};

/**
 * Configures how the Start Screen replaces temporary background art with real OrbitalVanguard ship hulls.
 */
UCLASS(BlueprintType)
class ORBITALVANGUARD_API UOVTitleBackdropDataAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TitleBackdrop")
    EOVTitleBackdropMode PreferredMode = EOVTitleBackdropMode::PreRenderedInGameImage;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TitleBackdrop")
    FSoftObjectPath SkyboxMeshPath = FSoftObjectPath(TEXT("/Game/Presentation/Environment/SpaceSkybox/SM_Source.SM_Source"));

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TitleBackdrop")
    TArray<FOVTitleBackdropFormation> Formations;

    static TArray<FOVTitleBackdropFormation> BuildDefaultFormations()
    {
        TArray<FOVTitleBackdropFormation> Result;

        // 1. Fleet100 Vanguard Task Force
        {
            FOVTitleBackdropFormation Formation;
            Formation.FormationId = TEXT("TaskForce100");
            Formation.LinkedScenarioId = TEXT("Fleet100");
            Formation.DisplayName = TEXT("기동 타격 편대 (순양함 · 전함 · 호위함)");
            Formation.PreRenderedTexturePath = FSoftObjectPath(TEXT("/Game/UI/Title/T_TitleBackdrop_Fleet100.T_TitleBackdrop_Fleet100"));
            Formation.CameraLocation = FVector(-4600.f, -1950.f, 1350.f);
            Formation.CameraRotation = FRotator(-14.0f, 22.f, 0.f);
            Formation.CameraFov = 46.f;

            auto AddSlot = [&Formation](const TCHAR* Role, const TCHAR* Label, const TCHAR* MeshPath, FVector Loc, FRotator Rot, float Scale)
            {
                FOVTitleBackdropShipSlot Slot;
                Slot.RoleTag = FName(Role);
                Slot.DisplayName = Label;
                Slot.StaticMeshPath = FSoftObjectPath(MeshPath);
                Slot.RelativeLocation = Loc;
                Slot.RelativeRotation = Rot;
                Slot.RelativeScale = FVector(Scale);
                Formation.Ships.Add(Slot);
            };

            AddSlot(TEXT("LeadBattleship"), TEXT("전함 (Battleship)"), TEXT("/Game/Presentation/Ships/BattleshipFull/SM_Hull.SM_Hull"), FVector(-1710.7f, -315.7f, 496.0f), FRotator(-3.0f, 58.0f, -4.0f), 2.60f);
            AddSlot(TEXT("Cruiser"), TEXT("순양함 (Cruiser)"), TEXT("/Game/Presentation/Ships/CruiserFull/SM_Hull.SM_Hull"), FVector(-598.2f, -794.7f, 606.4f), FRotator(-3.0f, 58.0f, -4.0f), 2.30f);
            AddSlot(TEXT("AirDefense"), TEXT("방공함 (Air Defense)"), TEXT("/Game/Presentation/Ships/Air_DefenseFull/SM_Hull.SM_Hull"), FVector(-821.4f, 924.5f, 706.1f), FRotator(-3.0f, 58.0f, -4.0f), 2.10f);
            AddSlot(TEXT("Vector"), TEXT("벡터급 구축함 (Vector)"), TEXT("/Game/Presentation/Ships/VectorFull/SM_Hull.SM_Hull"), FVector(328.9f, 437.0f, 944.4f), FRotator(-3.0f, 58.0f, -4.0f), 2.00f);
            Result.Add(MoveTemp(Formation));
        }

        // 2. Default Title & Fleet200 Flagship Vanguard
        {
            FOVTitleBackdropFormation Formation;
            Formation.FormationId = TEXT("FlagshipVanguard");
            Formation.LinkedScenarioId = TEXT("Fleet200");
            Formation.DisplayName = TEXT("기본 타이틀 편대 (구축전함 · 항공모함)");
            Formation.PreRenderedTexturePath = FSoftObjectPath(TEXT("/Game/UI/Title/T_TitleBackdrop_FlagshipVanguard.T_TitleBackdrop_FlagshipVanguard"));
            Formation.CameraLocation = FVector(-4600.f, -1950.f, 1350.f);
            Formation.CameraRotation = FRotator(-14.0f, 22.f, 0.f);
            Formation.CameraFov = 46.f;

            auto AddSlot = [&Formation](const TCHAR* Role, const TCHAR* Label, const TCHAR* MeshPath, FVector Loc, FRotator Rot, float Scale)
            {
                FOVTitleBackdropShipSlot Slot;
                Slot.RoleTag = FName(Role);
                Slot.DisplayName = Label;
                Slot.StaticMeshPath = FSoftObjectPath(MeshPath);
                Slot.RelativeLocation = Loc;
                Slot.RelativeRotation = Rot;
                Slot.RelativeScale = FVector(Scale);
                Formation.Ships.Add(Slot);
            };

            // Wide, tiered open-space echelon matching reference: Hero Flagship prominent, Carrier commanding left, escorts distributed with >250px clearance
            AddSlot(TEXT("Flagship"), TEXT("구축전함 (Dreadnought)"), TEXT("/Game/Presentation/Ships/DreadnoughtFull/SM_Hull.SM_Hull"), FVector(-1802.7f, -366.6f, 490.5f), FRotator(-3.0f, 58.0f, -4.0f), 2.60f);
            AddSlot(TEXT("Frigate1"), TEXT("호위함 1 (Frigate)"), TEXT("/Game/Presentation/Ships/FrigateFull/SM_Hull.SM_Hull"), FVector(-1900.3f, 390.6f, 438.4f), FRotator(-3.0f, 58.0f, -4.0f), 1.30f);
            AddSlot(TEXT("Frigate2"), TEXT("호위함 2 (Frigate)"), TEXT("/Game/Presentation/Ships/FrigateFull/SM_Hull.SM_Hull"), FVector(-1455.2f, -1053.0f, 338.4f), FRotator(-3.0f, 58.0f, -4.0f), 1.30f);
            AddSlot(TEXT("Battlecarrier"), TEXT("전투항공모함 (Battlecarrier)"), TEXT("/Game/Presentation/Ships/BattlecarrierFull/SM_Hull.SM_Hull"), FVector(-598.2f, -794.7f, 606.4f), FRotator(-3.0f, 58.0f, -4.0f), 2.50f);
            AddSlot(TEXT("Cruiser1"), TEXT("순양함 1 (Cruiser)"), TEXT("/Game/Presentation/Ships/CruiserFull/SM_Hull.SM_Hull"), FVector(-884.8f, 704.7f, 553.3f), FRotator(-3.0f, 58.0f, -4.0f), 2.00f);
            AddSlot(TEXT("Bastion"), TEXT("바스티온급 구축함 (Bastion)"), TEXT("/Game/Presentation/Ships/BastionFull/SM_Hull.SM_Hull"), FVector(-244.9f, 29.3f, 678.1f), FRotator(-3.0f, 58.0f, -4.0f), 1.90f);
            AddSlot(TEXT("Carrier"), TEXT("항공모함 (Carrier)"), TEXT("/Game/Presentation/Ships/CarrierFull/SM_Hull.SM_Hull"), FVector(676.5f, -221.0f, 834.9f), FRotator(-3.0f, 58.0f, -4.0f), 2.10f);
            AddSlot(TEXT("Battleship"), TEXT("전함 (Battleship)"), TEXT("/Game/Presentation/Ships/BattleshipFull/SM_Hull.SM_Hull"), FVector(99.7f, 937.7f, 844.3f), FRotator(-3.0f, 58.0f, -4.0f), 2.20f);
            AddSlot(TEXT("Vector"), TEXT("벡터급 구축함 (Vector)"), TEXT("/Game/Presentation/Ships/VectorFull/SM_Hull.SM_Hull"), FVector(1125.1f, 703.7f, 999.2f), FRotator(-3.0f, 58.0f, -4.0f), 1.80f);
            AddSlot(TEXT("AirDefense"), TEXT("방공함 (Air Defense)"), TEXT("/Game/Presentation/Ships/Air_DefenseFull/SM_Hull.SM_Hull"), FVector(355.1f, 2029.8f, 954.9f), FRotator(-3.0f, 58.0f, -4.0f), 1.80f);
            AddSlot(TEXT("Fighter1"), TEXT("함재기 편대 1 (Scout Lead)"), TEXT("/Game/Presentation/Ships/FighterFull/SM_Hull.SM_Hull"), FVector(-258.6f, -1272.7f, 1151.3f), FRotator(-3.0f, 58.0f, -4.0f), 0.75f);
            AddSlot(TEXT("Fighter2"), TEXT("함재기 편대 2 (Scout Wing L)"), TEXT("/Game/Presentation/Ships/FighterFull/SM_Hull.SM_Hull"), FVector(83.5f, -1548.4f, 1182.6f), FRotator(-3.0f, 58.0f, -4.0f), 0.70f);
            AddSlot(TEXT("Fighter3"), TEXT("함재기 편대 3 (Scout Wing R)"), TEXT("/Game/Presentation/Ships/FighterFull/SM_Hull.SM_Hull"), FVector(-273.7f, -950.1f, 1166.5f), FRotator(-3.0f, 58.0f, -4.0f), 0.70f);
            Result.Add(MoveTemp(Formation));
        }

        // 3. Fleet300 Carrier Strike Group
        {
            FOVTitleBackdropFormation Formation;
            Formation.FormationId = TEXT("CarrierStrike");
            Formation.LinkedScenarioId = TEXT("Fleet300");
            Formation.DisplayName = TEXT("항공 타격 편대 (항공순양함 · 전함)");
            Formation.PreRenderedTexturePath = FSoftObjectPath(TEXT("/Game/UI/Title/T_TitleBackdrop_CarrierStrike.T_TitleBackdrop_CarrierStrike"));
            Formation.CameraLocation = FVector(-4600.f, -1950.f, 1350.f);
            Formation.CameraRotation = FRotator(-14.0f, 22.f, 0.f);
            Formation.CameraFov = 46.f;

            auto AddSlot = [&Formation](const TCHAR* Role, const TCHAR* Label, const TCHAR* MeshPath, FVector Loc, FRotator Rot, float Scale)
            {
                FOVTitleBackdropShipSlot Slot;
                Slot.RoleTag = FName(Role);
                Slot.DisplayName = Label;
                Slot.StaticMeshPath = FSoftObjectPath(MeshPath);
                Slot.RelativeLocation = Loc;
                Slot.RelativeRotation = Rot;
                Slot.RelativeScale = FVector(Scale);
                Formation.Ships.Add(Slot);
            };

            AddSlot(TEXT("Flagship"), TEXT("항공순양함 (Battlecarrier)"), TEXT("/Game/Presentation/Ships/BattlecarrierFull/SM_Hull.SM_Hull"), FVector(-1710.7f, -315.7f, 496.0f), FRotator(-3.0f, 58.0f, -4.0f), 2.60f);
            AddSlot(TEXT("FireSupport"), TEXT("전함 (Battleship)"), TEXT("/Game/Presentation/Ships/BattleshipFull/SM_Hull.SM_Hull"), FVector(-598.2f, -794.7f, 606.4f), FRotator(-3.0f, 58.0f, -4.0f), 2.30f);
            AddSlot(TEXT("AirDefense"), TEXT("방공함 (Air Defense)"), TEXT("/Game/Presentation/Ships/Air_DefenseFull/SM_Hull.SM_Hull"), FVector(-821.4f, 924.5f, 706.1f), FRotator(-3.0f, 58.0f, -4.0f), 2.00f);
            AddSlot(TEXT("Scout"), TEXT("초계함 (PC14)"), TEXT("/Game/Presentation/Ships/SM_PC14_FullMesh.SM_PC14_FullMesh"), FVector(-258.6f, -1272.7f, 1151.3f), FRotator(-3.0f, 58.0f, -4.0f), 1.20f);
            Result.Add(MoveTemp(Formation));
        }

        // 3. Logistics Convoy (Future Mission 4)
        {
            FOVTitleBackdropFormation Formation;
            Formation.FormationId = TEXT("LogisticsConvoy");
            Formation.LinkedScenarioId = TEXT("Logistics");
            Formation.DisplayName = TEXT("물류 호송 편대 (수송선 · 채굴선)");
            Formation.PreRenderedTexturePath = FSoftObjectPath(TEXT("/Game/UI/Title/T_TitleBackdrop_LogisticsConvoy.T_TitleBackdrop_LogisticsConvoy"));
            Formation.CameraLocation = FVector(-3100.f, -1350.f, 690.f);
            Formation.CameraRotation = FRotator(-9.f, 22.f, 0.f);
            Formation.CameraFov = 42.f;

            auto AddSlot = [&Formation](const TCHAR* Role, const TCHAR* Label, const TCHAR* MeshPath, FVector Loc, FRotator Rot, float Scale)
            {
                FOVTitleBackdropShipSlot Slot;
                Slot.RoleTag = FName(Role);
                Slot.DisplayName = Label;
                Slot.StaticMeshPath = FSoftObjectPath(MeshPath);
                Slot.RelativeLocation = Loc;
                Slot.RelativeRotation = Rot;
                Slot.RelativeScale = FVector(Scale);
                Formation.Ships.Add(Slot);
            };

            AddSlot(TEXT("ConvoyLead"), TEXT("수송선 (Transport)"), TEXT("/Game/Presentation/Ships/TransportFull/SM_Hull.SM_Hull"), FVector(220.f, 440.f, 20.f), FRotator(2.f, -158.f, -3.f), 1.0f);
            AddSlot(TEXT("Miner"), TEXT("채굴선 (Mining)"), TEXT("/Game/Presentation/Ships/MiningFull/SM_Hull.SM_Hull"), FVector(840.f, 820.f, -110.f), FRotator(3.f, -162.f, -4.f), 0.95f);
            AddSlot(TEXT("Cruiser"), TEXT("순양함 (Cruiser)"), TEXT("/Game/Presentation/Ships/CruiserFull/SM_Hull.SM_Hull"), FVector(-460.f, 660.f, 190.f), FRotator(4.f, -154.f, -6.f), 0.88f);
            AddSlot(TEXT("Frigate"), TEXT("호위함 (Frigate)"), TEXT("/Game/Presentation/Ships/FrigateFull/SM_Hull.SM_Hull"), FVector(-220.f, -120.f, -160.f), FRotator(2.f, -166.f, -5.f), 0.85f);
            AddSlot(TEXT("Repair"), TEXT("보급수리함 (Repair)"), TEXT("/Game/Presentation/Ships/RepairFull/SM_Hull.SM_Hull"), FVector(980.f, 280.f, 220.f), FRotator(1.f, -160.f, -3.f), 0.85f);
            Result.Add(MoveTemp(Formation));
        }

        return Result;
    }

    static const FOVTitleBackdropFormation* FindFormationForScenario(
        const TArray<FOVTitleBackdropFormation>& List,
        FName ScenarioOrFormationId)
    {
        for (const FOVTitleBackdropFormation& Item : List)
        {
            if (Item.FormationId == ScenarioOrFormationId || Item.LinkedScenarioId == ScenarioOrFormationId)
            {
                return &Item;
            }
        }
        return List.IsEmpty() ? nullptr : &List[0];
    }
};
