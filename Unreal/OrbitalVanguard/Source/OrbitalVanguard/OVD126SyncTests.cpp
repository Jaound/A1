#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "OVAudioSubsystem.h"
#include "OVBlackHoleHazardActor.h"
#include "OVCalibration.h"
#include "OVCarrierSubsystem.h"
#include "OVCombatSubsystem.h"
#include "OVController.h"
#include "OVInteriorSubsystem.h"
#include "OVLogisticsSubsystem.h"
#include "OVPlayScenarioSubsystem.h"
#include "OVPointDefenseSubsystem.h"
#include "OVShip.h"
#include "OVShipLayout.h"
#include "OVShipSystemsComponent.h"
#include "OVSpaceEnvironmentTypes.h"
#include "OVTransportSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOVD126GridRulesAndCapacityAuditTest,
    "OrbitalVanguard.D126.GridRulesAndCapacityAudit",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOVD126GridRulesAndCapacityAuditTest::RunTest(const FString&)
{
    struct FGridCase
    {
        EOVHullSizeClass Class;
        int32 Decks;
        bool bTopHeavy;
        TArray<int32> ExpectedSlots;
        int32 ExpectedArmor;
    };

    const TArray<FGridCase> ValidCases = {
        { EOVHullSizeClass::Small,      1, false, { 7 },          14 },
        { EOVHullSizeClass::Medium,     1, false, { 9 },          14 },
        { EOVHullSizeClass::Medium,     2, false, { 9, 6 },       26 },
        { EOVHullSizeClass::Medium,     2, true,  { 6, 9 },       26 },
        { EOVHullSizeClass::Large,      2, false, { 12, 9 },      26 },
        { EOVHullSizeClass::Large,      2, true,  { 9, 12 },      26 },
        { EOVHullSizeClass::Large,      3, false, { 12, 9, 6 },   38 },
        { EOVHullSizeClass::Large,      3, true,  { 6, 9, 12 },   38 },
        { EOVHullSizeClass::ExtraLarge, 3, false, { 15, 12, 9 },  38 },
        { EOVHullSizeClass::ExtraLarge, 3, true,  { 9, 12, 15 },  38 },
    };

    for (const FGridCase& Case : ValidCases)
    {
        UShipLayoutDataAsset* Layout = UShipLayoutDataAsset::MakeD126Layout(
            GetTransientPackage(), Case.Class, Case.Decks, Case.bTopHeavy, 0, false);
        if (!TestNotNull(TEXT("Valid D126 layout constructed"), Layout)) continue;

        TArray<FString> Errors;
        TestTrue(FString::Printf(TEXT("D126 layout validates: %s"), *FString::Join(Errors, TEXT("; "))),
            Layout->ValidateLayout(Errors));

        // Verify per-deck slot counts
        TArray<int32> ActualPerDeck;
        ActualPerDeck.Init(0, Case.Decks);
        for (const FOVSlotDefinition& Slot : Layout->Slots)
        {
            if (Slot.DeckId >= 1 && Slot.DeckId <= Case.Decks)
            {
                ++ActualPerDeck[Slot.DeckId - 1];
            }
        }
        TestEqual(TEXT("Deck slot pattern matches D126"), ActualPerDeck, Case.ExpectedSlots);

        // Verify armor section count (14/26/38) is completely preserved
        TestEqual(TEXT("Armor section count preserved (14/26/38)"), Layout->ArmorSections.Num(), Case.ExpectedArmor);

        // 3-deck core must be on 2F
        if (Case.Decks == 3)
        {
            const int32 CoreIdx = Layout->GetSlotIndex(Layout->CoreSlotId);
            TestTrue(TEXT("3-deck core is strictly on 2F"),
                Layout->Slots.IsValidIndex(CoreIdx) && Layout->Slots[CoreIdx].DeckId == 2);
        }
    }

    // Disallowed deck counts per class
    TestNull(TEXT("Small 2-deck rejected"), UShipLayoutDataAsset::MakeD126Layout(GetTransientPackage(), EOVHullSizeClass::Small, 2));
    TestNull(TEXT("Medium 3-deck rejected"), UShipLayoutDataAsset::MakeD126Layout(GetTransientPackage(), EOVHullSizeClass::Medium, 3));
    TestNull(TEXT("Large 1-deck rejected"), UShipLayoutDataAsset::MakeD126Layout(GetTransientPackage(), EOVHullSizeClass::Large, 1));
    TestNull(TEXT("ExtraLarge 2-deck rejected (3-deck only)"), UShipLayoutDataAsset::MakeD126Layout(GetTransientPackage(), EOVHullSizeClass::ExtraLarge, 2));

    // Explicitly reject non-monotonic 15-9-12 layout!
    UShipLayoutDataAsset* NonMonotonic = UShipLayoutDataAsset::MakeD126Layout(
        GetTransientPackage(), EOVHullSizeClass::ExtraLarge, 3, false, 4, false);
    if (TestNotNull(TEXT("ExtraLarge base layout exists for non-monotonic mutation"), NonMonotonic))
    {
        // Mutate deck counts from 15-12-9 into 15-9-12 by swapping DeckId 2 and DeckId 3 on non-core/bridge slots
        // Keep core on 2F, drop 3 empty slots from 2F, and add 3 slots to 3F to form exact 15-9-12 counts.
        int32 RemovedFrom2F = 0;
        TArray<FOVSlotDefinition> Reassigned;
        for (int32 I = NonMonotonic->Slots.Num() - 1; I >= 0 && RemovedFrom2F < 3; --I)
        {
            const FOVSlotDefinition& S = NonMonotonic->Slots[I];
            if (S.DeckId == 2 && S.Facility == EOVFacilityType::None)
            {
                FOVSlotDefinition Moved = S;
                Moved.DeckId = 3;
                Moved.SlotId = FName(*FString::Printf(TEXT("Mutated_D3_%d"), RemovedFrom2F));
                Moved.LocalCenter.Z = NonMonotonic->HalfHeight - (NonMonotonic->HalfHeight / 3.f);
                Moved.ConnectionIds.Reset();
                Reassigned.Add(Moved);
                NonMonotonic->Slots.RemoveAt(I);
                ++RemovedFrom2F;
            }
        }
        for (const FOVSlotDefinition& M : Reassigned) NonMonotonic->Slots.Add(M);

        TArray<FString> Errors;
        TestFalse(TEXT("Non-monotonic 15-9-12 layout explicitly rejected"), NonMonotonic->ValidateLayout(Errors));
        TestTrue(TEXT("Error explicitly reports D126 monotonic -3 violation"),
            Errors.ContainsByPredicate([](const FString& E) { return E.Contains(TEXT("monotonically")); }));
    }

    // Small-7 & Medium-9 facility capacity audit (must NOT merge/delete mandatory facilities)
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    auto* Combat = World->GetSubsystem<UOVCombatSubsystem>();
    OVCalibration::BeginSession(*Combat, 260112);

    AOVShip* Ship = World->SpawnActor<AOVShip>();
    if (TestTrue(TEXT("Corvette configures cleanly on D126 Small-7"),
            OVCalibration::ConfigureHull(*Combat, *Ship, TEXT("corvette"), true, FVector::ZeroVector, 6.0)))
    {
        const UShipLayoutDataAsset* CorvLayout = Ship->Systems->Layout;
        TestEqual(TEXT("Corvette has exactly 7 interior slots"), CorvLayout->Slots.Num(), 7);
        const TArray<EOVFacilityType> FullDesired = {
            EOVFacilityType::Core, EOVFacilityType::Bridge, EOVFacilityType::LifeSupport, EOVFacilityType::Engine,
            EOVFacilityType::WeaponControl, EOVFacilityType::Ammo, EOVFacilityType::Capacitor,
            EOVFacilityType::ShieldControl, EOVFacilityType::Interception, EOVFacilityType::Radar
        };
        TArray<EOVFacilityType> Unplaced;
        FString AuditReport;
        TestFalse(TEXT("Small-7 corvette reports capacity deficit honestly without merging mandatory rooms"),
            UShipLayoutDataAsset::AuditFacilityCapacity(CorvLayout, FullDesired, Unplaced, AuditReport));
        TestEqual(TEXT("Corvette has exactly 3 unplaced secondary rooms (7/7 mandatory occupied)"), Unplaced.Num(), 3);
        TestTrue(TEXT("Audit report contains explicit capacity shortage note"), AuditReport.Contains(TEXT("deficit")));
        TestTrue(TEXT("Corvette keeps Cannon and Beam firing capability"),
            Ship->Systems->CanFireCannons() && Ship->Systems->CanFireBeams());
    }

    World->DestroyWorld(false);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOVD126HangarAndAircraftRestrictionsTest,
    "OrbitalVanguard.D126.HangarAndAircraftRestrictions",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOVD126HangarAndAircraftRestrictionsTest::RunTest(const FString&)
{
    struct FHangarSpec
    {
        const TCHAR* HullId;
        int32 ExpectedMaxHangars;
        bool bExpectedAircraft;
    };

    const FHangarSpec Specs[] = {
        { TEXT("corvette"),      0, false },
        { TEXT("frigate"),       0, false },
        { TEXT("vector"),        0, false },
        { TEXT("bastion"),       0, false },
        { TEXT("missile_ship"),  1, false },
        { TEXT("battleship"),    2, false },
        { TEXT("dreadnought"),   4, false },
        { TEXT("carrier"),       4, true  },
        { TEXT("battlecarrier"), 6, true  },
    };

    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    auto* Combat = World->GetSubsystem<UOVCombatSubsystem>();
    auto* CarrierSub = World->GetSubsystem<UOVCarrierSubsystem>();
    OVCalibration::BeginSession(*Combat, 260112);

    for (const FHangarSpec& Spec : Specs)
    {
        FOVHullMetadata Meta;
        if (!TestTrue(FString::Printf(TEXT("Metadata exists for %s"), Spec.HullId),
                OVCalibration::GetHullMetadata(FName(Spec.HullId), Meta)))
        {
            continue;
        }
        TestEqual(FString::Printf(TEXT("%s MaxHangarInstallations"), Spec.HullId), Meta.MaxHangarInstallations, Spec.ExpectedMaxHangars);
        TestEqual(FString::Printf(TEXT("%s bAllowAircraftOperations"), Spec.HullId), Meta.bAllowAircraftOperations, Spec.bExpectedAircraft);

        AOVShip* Ship = World->SpawnActor<AOVShip>();
        if (!TestTrue(FString::Printf(TEXT("ConfigureHull %s"), Spec.HullId),
                OVCalibration::ConfigureHull(*Combat, *Ship, FName(Spec.HullId), true, FVector::ZeroVector, 6.0)))
        {
            continue;
        }

        UShipLayoutDataAsset* Layout = Ship->Systems->Layout;
        if (!Spec.bExpectedAircraft)
        {
            // Try forcing Hangar + FighterControl on non-aircraft hulls:
            // 1) ValidateLayout must reject FighterControl on non-aircraft hulls (and Hangar on Small/Medium)
            // 2) OVCarrierSubsystem::RegisterCarrier must refuse aircraft operations
            UShipLayoutDataAsset* Mutated = DuplicateObject<UShipLayoutDataAsset>(Layout, GetTransientPackage());
            bool bInstalledFighterControl = false;
            for (FOVSlotDefinition& Slot : Mutated->Slots)
            {
                if (Slot.Facility == EOVFacilityType::None)
                {
                    Slot.Facility = EOVFacilityType::FighterControl;
                    Slot.AllowedFacilities.AddUnique(EOVFacilityType::FighterControl);
                    Slot.MaxFacilityHP = 100.f;
                    Slot.RequiredCrew = 10.f;
                    bInstalledFighterControl = true;
                    break;
                }
            }
            if (bInstalledFighterControl)
            {
                TArray<FString> Errors;
                TestFalse(FString::Printf(TEXT("%s rejects FighterControl in ValidateLayout"), Spec.HullId),
                    Mutated->ValidateLayout(Errors));
            }

            // Even if a non-aircraft Large/ExtraLarge hull installs small-craft Hangars up to limit,
            // OVCarrierSubsystem must NOT allow registering aircraft or launching fighters.
            if (Spec.ExpectedMaxHangars > 0)
            {
                for (FOVSlotDefinition& Slot : Layout->Slots)
                {
                    if (Slot.Facility == EOVFacilityType::None && Slot.AllowedFacilities.Contains(EOVFacilityType::Hangar))
                    {
                        Slot.Facility = EOVFacilityType::Hangar;
                        Slot.MaxFacilityHP = 100.f;
                        Slot.RequiredCrew = 10.f;
                        break;
                    }
                }
                Ship->Systems->Initialize(Layout);
            }

            TestFalse(FString::Printf(TEXT("%s blocked from RegisterCarrier aircraft ops"), Spec.HullId),
                CarrierSub->RegisterCarrier(Ship, 10));
            TestFalse(FString::Printf(TEXT("%s CanLaunchFighters is false"), Spec.HullId),
                Ship->Systems->CanLaunchFighters());
            TestEqual(FString::Printf(TEXT("%s Launch returns 0 aircraft"), Spec.HullId),
                CarrierSub->Launch(Ship), 0);
        }
        else
        {
            TestTrue(FString::Printf(TEXT("%s accepts RegisterCarrier aircraft ops"), Spec.HullId),
                CarrierSub->RegisterCarrier(Ship, 10));
            TestTrue(FString::Printf(TEXT("%s CanLaunchFighters is true"), Spec.HullId),
                Ship->Systems->CanLaunchFighters());
        }
    }

    World->DestroyWorld(false);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOVD126HullBalanceTableSyncTest,
    "OrbitalVanguard.D126.HullBalanceTableSync",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOVD126HullBalanceTableSyncTest::RunTest(const FString&)
{
    struct FHullCheck
    {
        const TCHAR* Id;
        int32 SizeClass;
        int32 Decks;
        int32 TotalSlots;
        int32 MaxHangars;
        bool bAircraft;
        float SpeedCm;
        int32 AttackMounts;
        int32 PD;
        float ArmorCap;
    };

    const FHullCheck Checks[] = {
        { TEXT("corvette"),      1, 1,  7, 0, false, 15500.f,  2, 2, 0.60f },
        { TEXT("frigate"),       1, 1,  7, 0, false, 13800.f,  3, 2, 0.60f },
        { TEXT("vector"),        2, 1,  9, 0, false, 14800.f,  4, 3, 0.26f },
        { TEXT("bastion"),       2, 2, 15, 0, false,  8800.f,  7, 4, 0.42f },
        { TEXT("battleship"),    3, 3, 27, 2, false,  6800.f, 10, 5, 0.60f },
        { TEXT("missile_ship"),  3, 2, 21, 1, false,  7400.f,  9, 5, 0.60f },
        { TEXT("carrier"),       3, 3, 27, 4, true,   6600.f,  4, 5, 0.60f },
        { TEXT("dreadnought"),   4, 3, 36, 4, false,  4800.f, 16, 8, 0.60f },
        { TEXT("battlecarrier"), 4, 3, 36, 6, true,   5200.f, 10, 8, 0.60f },
    };

    for (const FHullCheck& C : Checks)
    {
        FOVHullMetadata Meta;
        if (!TestTrue(FString::Printf(TEXT("Metadata loaded for %s"), C.Id), OVCalibration::GetHullMetadata(FName(C.Id), Meta)))
            continue;
        TestEqual(FString::Printf(TEXT("%s SizeClass"), C.Id), Meta.SizeClass, C.SizeClass);
        TestEqual(FString::Printf(TEXT("%s DeckCount"), C.Id), Meta.DeckCount, C.Decks);
        TestEqual(FString::Printf(TEXT("%s TotalInteriorSlots"), C.Id), Meta.TotalInteriorSlots, C.TotalSlots);
        TestEqual(FString::Printf(TEXT("%s MaxHangars"), C.Id), Meta.MaxHangarInstallations, C.MaxHangars);
        TestEqual(FString::Printf(TEXT("%s bAllowAircraft"), C.Id), Meta.bAllowAircraftOperations, C.bAircraft);
        TestTrue(FString::Printf(TEXT("%s SpeedCm %.0f"), C.Id, C.SpeedCm), FMath::IsNearlyEqual(Meta.MoveSpeedCmPerSec, C.SpeedCm, 0.1f));
        TestEqual(FString::Printf(TEXT("%s AttackMounts"), C.Id), Meta.AttackMountCount, C.AttackMounts);
        TestEqual(FString::Printf(TEXT("%s PointDefenseCount"), C.Id), Meta.PointDefenseCount, C.PD);
        TestTrue(FString::Printf(TEXT("%s ArmorMaxReduction"), C.Id), FMath::IsNearlyEqual(Meta.ArmorMaxReduction, C.ArmorCap, 0.001f));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOVE1BlackHoleAndScenarioLimitTest,
    "OrbitalVanguard.E1.BlackHoleGravityFTLAndInstantWreck",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOVE1BlackHoleAndScenarioLimitTest::RunTest(const FString&)
{
    // 1. Exact E1 formula check: [1 - max(0, r - 45m) / 955m]^2 * 80 cm/s^2
    TestTrue(TEXT("E1 accel at 45m core boundary = 80 cm/s^2"),
        FMath::IsNearlyEqual(FOVSpaceEnvironmentProfile::ComputeBlackHoleGravityAccelerationCm(4500.f), 80.f, 0.01f));
    TestTrue(TEXT("E1 accel at midpoint 522.5m = 20 cm/s^2 (0.5^2 * 80)"),
        FMath::IsNearlyEqual(FOVSpaceEnvironmentProfile::ComputeBlackHoleGravityAccelerationCm(52250.f), 20.f, 0.01f));
    TestTrue(TEXT("E1 accel at 1000m outer boundary = 0"),
        FMath::IsNearlyEqual(FOVSpaceEnvironmentProfile::ComputeBlackHoleGravityAccelerationCm(100000.f), 0.f, 0.001f));
    TestTrue(TEXT("E1 accel beyond 1000m = 0"),
        FMath::IsNearlyEqual(FOVSpaceEnvironmentProfile::ComputeBlackHoleGravityAccelerationCm(150000.f), 0.f, 0.001f));

    // 2. Actor kinematics: FTL-charging ship MUST still displace and suffer instant HP/shield-ignoring wreck inside 45m
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    AOVBlackHoleHazardActor* BH = World->SpawnActor<AOVBlackHoleHazardActor>(
        AOVBlackHoleHazardActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("BlackHole hazard actor spawned"), BH))
    {
        World->DestroyWorld(false);
        return false;
    }
    TestTrue(TEXT("Default pull radius = 100,000 cm (1,000m)"), FMath::IsNearlyEqual(BH->GravityPullRadius, 100000.f, 0.1f));
    TestTrue(TEXT("Default core radius = 4,500 cm (45m)"), FMath::IsNearlyEqual(BH->SingularityRadius, 4500.f, 0.1f));
    TestTrue(TEXT("Default max accel = 80 cm/s^2 (0.8 m/s^2)"), FMath::IsNearlyEqual(BH->GravitationalStrength, 80.f, 0.1f));

    auto* Combat = World->GetSubsystem<UOVCombatSubsystem>();
    OVCalibration::BeginSession(*Combat, 260112);

    AOVShip* FTLVictim = World->SpawnActor<AOVShip>();
    OVCalibration::ConfigureHull(*Combat, *FTLVictim, TEXT("dreadnought"), true, FVector(7000.f, 0.f, 0.f), 6.0);
    FTLVictim->DebugSetFTLCharging(true); // Must NOT exempt from gravity or core destruction!
    const float InitialDist = static_cast<float>(FTLVictim->GetActorLocation().Size());
    TestTrue(TEXT("FTL ship starts intact with huge dreadnought HP and Shield"),
        FTLVictim->bFTLCharging && !FTLVictim->Systems->bWrecked && FTLVictim->Systems->HullHP > 800000.f && FTLVictim->Systems->Shield > 300000.f);

    AOVShip* SafeShip = World->SpawnActor<AOVShip>();
    OVCalibration::ConfigureHull(*Combat, *SafeShip, TEXT("corvette"), true, FVector(120000.f, 0.f, 0.f), 6.0);
    const FVector SafeStart = SafeShip->GetActorLocation();

    BH->SimulateHazard(1.0f);
    const float AfterOneSecDist = static_cast<float>(FTLVictim->GetActorLocation().Size());
    TestTrue(TEXT("FTL-charging ship displaces inward under Black Hole gravity"), AfterOneSecDist < InitialDist - 20.f);
    TestTrue(TEXT("Ship beyond 1,000m experiences zero displacement"), SafeShip->GetActorLocation().Equals(SafeStart, 0.01));

    // Continue simulating until FTL-charging dreadnought crosses 45m core -> instant total wreck!
    for (int32 Step = 0; Step < 25 && !FTLVictim->Systems->bWrecked; ++Step)
    {
        BH->SimulateHazard(0.5f);
    }
    TestTrue(TEXT("FTL-charging dreadnought instantly wrecked upon entering 45m core"), FTLVictim->Systems->bWrecked);
    TestEqual(TEXT("Core instant wreck zeroes HullHP"), FTLVictim->Systems->HullHP, 0.f);
    TestEqual(TEXT("Core instant wreck zeroes Shield"), FTLVictim->Systems->Shield, 0.f);
    TestEqual(TEXT("BlackHole recorded 1 core instant wreck"), BH->CoreInstantWreckCount, 1);
    TestFalse(TEXT("Ship beyond 1,000m remains intact"), SafeShip->Systems->bWrecked);

    // 3. Scenario default 900s limit removed & BlackHoleTrial integration check
    for (const FOVPlaySceneDefinition& Def : UOVPlayScenarioSubsystem::Definitions())
    {
        TestEqual(FString::Printf(TEXT("Scenario %s disables forced 900s limit by default"), *Def.Id.ToString()),
            Def.LimitSeconds, 0.f);
    }

    auto* PlaySub = World->GetSubsystem<UOVPlayScenarioSubsystem>();
    TestTrue(TEXT("BlackHoleTrial definition registered and available"),
        UOVPlayScenarioSubsystem::Definitions().ContainsByPredicate([](const FOVPlaySceneDefinition& D) {
            return D.Id == TEXT("BlackHoleTrial") && D.bAvailable && D.EnvironmentHazard == EOVSpaceHazardKind::BlackHole;
        }));

    AOVController* Controller = World->SpawnActor<AOVController>();
    Controller->Combat = World->GetSubsystem<UOVCombatSubsystem>();
    Controller->Carrier = World->GetSubsystem<UOVCarrierSubsystem>();
    Controller->PointDefense = World->GetSubsystem<UOVPointDefenseSubsystem>();
    Controller->Transport = World->GetSubsystem<UOVTransportSubsystem>();
    Controller->Interior = World->GetSubsystem<UOVInteriorSubsystem>();

    if (TestTrue(TEXT("BlackHoleTrial starts cleanly"), PlaySub->Start(*Controller, TEXT("BlackHoleTrial"))))
    {
        TestNotNull(TEXT("BlackHoleTrial spawns active E1 BlackHole hazard actor"), PlaySub->GetActiveBlackHoleHazard());
        TestEqual(TEXT("Controller environment synced to BlackHole"), Controller->ActiveEnvironment, EOVSpaceEnvironmentType::BlackHole);
        PlaySub->Clear();
        TestNull(TEXT("Scenario Clear destroys BlackHole hazard actor"), PlaySub->GetActiveBlackHoleHazard());
    }

    // Verify optional time limit works ONLY when explicitly requested, never at 900s by default
    if (TestTrue(TEXT("Fleet100 starts cleanly"), PlaySub->Start(*Controller, TEXT("Fleet100"))))
    {
        PlaySub->ElapsedSeconds = 950.0;
        // Simulate a tiny integrity change so stalemate timer resets and ONLY time limit would fire if active
        if (!Controller->Ships.IsEmpty() && Controller->Ships[0]->Systems)
        {
            Controller->Ships[0]->Systems->HullHP -= 10.f;
        }
        PlaySub->Step(0.3f);
        TestEqual(TEXT("Fleet100 continues running past 900s with LimitSeconds=0"), PlaySub->Result, EOVPlaySceneResult::Running);

        PlaySub->SetOptionalTimeLimit(960.f);
        PlaySub->ElapsedSeconds = 965.0;
        if (!Controller->Ships.IsEmpty() && Controller->Ships[0]->Systems)
        {
            Controller->Ships[0]->Systems->HullHP -= 10.f;
        }
        PlaySub->Step(0.3f);
        TestEqual(TEXT("Optional explicit time limit still works when enabled"), PlaySub->Result, EOVPlaySceneResult::TimeLimit);
        PlaySub->Clear();
    }

    World->DestroyWorld(false);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOVAudioSafeMissingAssetHooksTest,
    "OrbitalVanguard.Audio.SafeMissingAssetHooks",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOVAudioSafeMissingAssetHooksTest::RunTest(const FString&)
{
    UGameInstance* GI = NewObject<UGameInstance>(GetTransientPackage());
    GI->Init();
    UOVAudioSubsystem* Audio = GI->GetSubsystem<UOVAudioSubsystem>();
    if (!TestNotNull(TEXT("Audio subsystem initialized"), Audio))
    {
        GI->Shutdown();
        return false;
    }

    Audio->SetVolumes(0.75f, 0.50f, 0.90f, false);
    TestTrue(TEXT("Master volume updated"), FMath::IsNearlyEqual(Audio->GetMasterVolume(), 0.75f, 0.001f));
    TestTrue(TEXT("BGM volume updated"), FMath::IsNearlyEqual(Audio->GetBGMVolume(), 0.50f, 0.001f));
    TestTrue(TEXT("SFX volume updated"), FMath::IsNearlyEqual(Audio->GetSFXVolume(), 0.90f, 0.001f));

    const int32 StartCount = Audio->GetHookTriggerCount();
    Audio->PlayTitleBGM();
    Audio->PlayUIClick();
    Audio->PlayCombatSFX(EOVAudioCue::LaserFire, FVector(100, 0, 0));
    Audio->PlayCombatSFX(EOVAudioCue::CannonFire, FVector(200, 0, 0));
    Audio->PlayCombatSFX(EOVAudioCue::ShieldHit, FVector(300, 0, 0));
    Audio->PlayCombatSFX(EOVAudioCue::ShieldBreak, FVector(400, 0, 0));
    Audio->PlayCombatSFX(EOVAudioCue::WreckExplosion, FVector(500, 0, 0));
    Audio->StopTitleBGM();

    TestEqual(TEXT("All 7 audio hooks executed safely even with missing assets"),
        Audio->GetHookTriggerCount() - StartCount, 7);
    TestEqual(TEXT("Last triggered cue recorded accurately"),
        Audio->GetLastTriggeredCue(), EOVAudioCue::WreckExplosion);

    GI->Shutdown();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOVD126OutpostAndMediumTransportLoopTest,
    "OrbitalVanguard.D126.OutpostAndMediumTransportLoop",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOVD126OutpostAndMediumTransportLoopTest::RunTest(const FString&)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    UOVLogisticsSubsystem* Logi = World->GetSubsystem<UOVLogisticsSubsystem>();
    if (!TestNotNull(TEXT("Logistics subsystem exists"), Logi))
    {
        World->DestroyWorld(false);
        return false;
    }

    Logi->ResetOutpostLogistics();

    // 1. Outpost cannot be built on Home Base (0 hops)
    TestEqual(TEXT("Home-system outpost rejected"),
        Logi->AddRemoteOutpost(TEXT("home_vanguard"), 1, EOVOutpostRole::ResourceOutpost, 0, 600.f), 0);

    // 2. CombatOutpost strictly rejects resource mining / medium transport hub assignments
    const int32 CombatId = Logi->AddRemoteOutpost(TEXT("arcadia_rim"), 1, EOVOutpostRole::CombatOutpost, 2, 0.f);
    TestTrue(TEXT("CombatOutpost created"), CombatId > 0);
    TestEqual(TEXT("CombatOutpost rejects miner offload assignment"),
        Logi->AssignRemoteMiners(1, CombatId, 4, 80.f, 200.f, 20.f), 0);
    TestEqual(TEXT("CombatOutpost rejects R37 medium transport route"),
        Logi->AddMediumTransportRoute(1, CombatId, 2, 250.f, 220.f, 15.f), 0);

    // 3. Faction 0 (Direct multi-system mining without outpost) vs Faction 1 (ResourceOutpost + R37 Medium Transports)
    // Setup matching D126 2-system distance scenario (4 miners; 2 medium transports for Faction 1):
    // Direct miners spend 1,230 s round-trip across 2 star systems -> ~13.4/min
    // Local miners spend 20 s local drop-off to ResourceOutpost, R37 Medium Transports carry 250 each with 220 s one-way -> ~60+/min (> 4x)
    const int32 DirectId = Logi->AssignRemoteMiners(0, 0, 4, 80.f, 200.f, 1230.f);
    TestTrue(TEXT("Direct multi-system miner assignment created"), DirectId > 0);

    const int32 ResourceOutpostId = Logi->AddRemoteOutpost(TEXT("kronos_nebula"), 1, EOVOutpostRole::ResourceOutpost, 2, 600.f);
    TestTrue(TEXT("ResourceOutpost created"), ResourceOutpostId > 0);
    const int32 LocalMinerId = Logi->AssignRemoteMiners(1, ResourceOutpostId, 4, 80.f, 200.f, 20.f);
    const int32 RouteId = Logi->AddMediumTransportRoute(1, ResourceOutpostId, 2, 250.f, 220.f, 15.f);
    TestTrue(TEXT("ResourceOutpost local miners and R37 medium transports linked"), LocalMinerId > 0 && RouteId > 0);

    // Simulate 60 minutes (3,600 seconds)
    Logi->SimulateOutpostLogistics(3600.f);

    const float DirectRatePerMin = Logi->GetHomeBaseRatePerMinute(0);
    const float OutpostRatePerMin = Logi->GetHomeBaseRatePerMinute(1);
    const float Ratio = DirectRatePerMin > KINDA_SMALL_NUMBER ? (OutpostRatePerMin / DirectRatePerMin) : 0.f;

    UE_LOG(LogTemp, Display, TEXT("D126 Outpost Logistics Benchmark: Direct=%.2f/min, Outpost+R37=%.2f/min, Gain=%.2fx"),
        DirectRatePerMin, OutpostRatePerMin, Ratio);

    TestTrue(TEXT("Direct multi-system delivery rate is positive (~13/min)"), DirectRatePerMin > 10.f && DirectRatePerMin < 18.f);
    TestTrue(TEXT("ResourceOutpost + R37 Medium Transport outperforms direct mining by > 4x"), Ratio > 4.0f);

    // 4. Verify warehouse bottleneck diagnosis triggers when outpost warehouse accumulates stock faster than R37 capacity
    FString Diagnosis;
    const bool bBottleneck = Logi->DiagnoseOutpostBottleneck(ResourceOutpostId, Diagnosis);
    TestTrue(TEXT("Warehouse bottleneck detected under 4 miners / 2 R37 transports"), bBottleneck);
    TestTrue(TEXT("Diagnosis recommends increasing R37 transport count or capacity"), Diagnosis.Contains(TEXT("중형 수송선(R37)")));

    World->DestroyWorld(false);
    return true;
}

#endif
