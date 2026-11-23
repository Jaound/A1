#include "OVTransportSubsystem.h"
#include "OVBattleNotices.h"
#include "OVCombatSubsystem.h"
#include "OVShip.h"
#include "OVShipSystemsComponent.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace OVTransportTest
{
struct FRange
{
    UWorld* World;
    UOVTransportSubsystem* Transport;
    UOVCombatSubsystem* Combat;
    AOVShip* Origin;
    AOVShip* Target;
    AOVShip* Friend;
    FRange()
    {
        World = UWorld::CreateWorld(EWorldType::Game, false);
        Transport = World->GetSubsystem<UOVTransportSubsystem>();
        Combat = World->GetSubsystem<UOVCombatSubsystem>();
        Origin = Make(TEXT("PodOrigin"), 0, FVector(-5000, 0, 0));
        Target = Make(TEXT("TransportTarget"), 1, FVector::ZeroVector);
        Friend = Make(TEXT("ReturnAlternate"), 0, FVector(-5000, 3000, 0));
        Combat->EnableCombat({Origin, Target, Friend}, 260112);
        Transport->ResetScenario({Origin, Target, Friend});
        Target->Systems->Shield = 0; Target->Systems->bShieldActive = false;
    }
    ~FRange() { World->DestroyWorld(false); }
    AOVShip* Make(const TCHAR* Name, int32 Faction, FVector Position)
    {
        AOVShip* Ship = World->SpawnActor<AOVShip>();
        Ship->ShipName = Name; Ship->Systems->FactionId = Faction;
        Ship->Systems->Initialize(UShipLayoutDataAsset::MakeTestLayout(Ship, 3));
        Ship->SetActorLocation(Position); Ship->DebugResetMotion(); return Ship;
    }
    void NoBoarding()
    {
        for (int32 I = 0; I < Origin->Systems->Slots.Num(); ++I)
            if (Origin->Systems->Layout->Slots[I].Facility == EOVFacilityType::Boarding)
                Origin->Systems->DebugSetCrew(Origin->Systems->Slots[I].SlotId, 0);
    }
    AOVShip* Shuttle()
    {
        AOVShip* Ship = Make(TEXT("AssaultShuttle"), 0, FVector(-1000, 0, 0));
        UShipLayoutDataAsset* Layout = UShipLayoutDataAsset::MakeTestLayout(Ship, 1);
        Layout->WeaponMounts.Reset(); Layout->CannonSlots = Layout->BeamSlots = Layout->MissileSlots = 0;
        FOVWeaponMountDefinition PD; PD.WeaponId = TEXT("PD1"); PD.Type = EOVWeaponType::Interception; Layout->WeaponMounts.Add(PD);
        Ship->Systems->Initialize(Layout);
        Combat->RegisterShip(Ship);
        Transport->ResetScenario({Origin, Target, Friend, Ship});
        Transport->RegisterShuttle(Ship, Origin);
        return Ship;
    }
    void Drive(AOVShip* Ship, float Duration)
    {
        for (int32 I = 0; I < FMath::RoundToInt(Duration * 120); ++I)
        { Transport->Simulate(1.f/120); Ship->Simulate(1.f/120); }
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVTransportPersonalLaunchTest, "OrbitalVanguard.Transport.PersonalLaunchScheduleAndFacilityRecheck", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOVTransportPersonalLaunchTest::RunTest(const FString&)
{
    OVTransportTest::FRange R;
    const auto Ids = R.Transport->AddDevPassengers(R.Origin, 4, 6);
    TestEqual(TEXT("Four hero-type plus six ordinary DEV people are ten individuals"), Ids.Num(), 10);
    TestEqual(TEXT("One pod per person"), R.Transport->LaunchPods(R.Origin, R.Target), 10);
    TestEqual(TEXT("Repeated request cannot queue a person twice"), R.Transport->LaunchPods(R.Origin, R.Target), 0);
    R.Transport->Simulate(0);
    TestEqual(TEXT("Pause leaves launch time untouched"), R.Transport->Pods[0].LaunchTime, -1.0);
    R.Transport->Simulate(.3f);
    TestTrue(TEXT("Individual launch times are staggered"), R.Transport->Pods[1].LaunchTime - R.Transport->Pods[0].LaunchTime >= .24);
    R.NoBoarding(); R.Transport->Simulate(3);
    int32 Cancelled = 0; for (const auto& Pod : R.Transport->Pods) Cancelled += Pod.State == EOVPodFlightState::Cancelled ? 1 : 0;
    TestEqual(TEXT("Facility loss cancels only eight unlaunched people"), Cancelled, 8);
    TestEqual(TEXT("Cancelled people stay aboard"), R.Transport->CountState(EOVPassengerState::Aboard), 8);
    TestEqual(TEXT("Already launched pods still enter without launch facility"), R.Transport->EntryEvents.Num(), 2);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVTransportInterceptionTest, "OrbitalVanguard.Transport.InterceptionShieldAndSingleCasualty", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOVTransportInterceptionTest::RunTest(const FString&)
{
    OVTransportTest::FRange R;
    R.Transport->Settings.PodSpeed = 1000;
    const auto Ids = R.Transport->AddDevPassengers(R.Origin, 1, 2);
    R.Transport->LaunchPods(R.Origin, R.Target); R.Transport->Simulate(.6f);
    TestTrue(TEXT("Airborne hero pod takes interception damage"), R.Transport->InterceptPod(R.Transport->Pods[0].PodId, 10000));
    TestFalse(TEXT("Destroyed pod cannot be killed twice"), R.Transport->InterceptPod(R.Transport->Pods[0].PodId, 10000));
    TestFalse(TEXT("DEV hero-type token is not an actual hero rescue notice"),R.World->GetSubsystem<UOVBattleNotices>()->Notices.ContainsByPredicate([](const FOVBattleNotice& N){return N.Kind==EOVBattleNoticeKind::HeroRescue;}));
    TestTrue(TEXT("Airborne troop pod takes interception damage"), R.Transport->InterceptPod(R.Transport->Pods[1].PodId, 10000));
    TestEqual(TEXT("Hero-type token waits for rescue, not implemented hero death rule"), R.Transport->GetPerson(Ids[0]).State, EOVPassengerState::RescueWaiting);
    TestEqual(TEXT("Ordinary troop token dies"), R.Transport->GetPerson(Ids[1]).State, EOVPassengerState::Dead);
    R.Target->Systems->bShieldActive = true; R.Target->Systems->Shield = 100;
    R.Transport->Simulate(6);
    TestEqual(TEXT("Active hostile shield prevents remaining pod entry"), R.Transport->EntryEvents.Num(), 0);
    TestEqual(TEXT("Shield collision has ordinary troop casualty semantics"), R.Transport->GetPerson(Ids[2]).State, EOVPassengerState::Dead);
    TestFalse(TEXT("Dead passengers do not create protection"), R.Target->Systems->HasInteriorPresence(0));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVTransportLocalEntryReturnTest, "OrbitalVanguard.Transport.LocalEntryOriginalPodAndProtection", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOVTransportLocalEntryReturnTest::RunTest(const FString&)
{
    OVTransportTest::FRange R;
    R.Target->SetActorRotation(FRotator(70, 120, 30));
    R.Origin->SetActorLocation(R.Target->GetActorTransform().TransformPosition(FVector(5000, 0, 0)));
    const int32 Id = R.Transport->AddDevPassengers(R.Origin, 1, 0)[0];
    R.Transport->LaunchPods(R.Origin, R.Target); R.Transport->Simulate(.1f);
    R.Transport->InterceptPod(R.Transport->Pods[0].PodId, 10); R.Transport->Simulate(.9f);
    TestEqual(TEXT("Actual swept arrival emits one entry"), R.Transport->EntryEvents.Num(), 1);
    TestEqual(TEXT("Rotated target local middle height remains middle deck"), R.Transport->GetPerson(Id).EntryDeck, 2);
    TestEqual(TEXT("Local front is not world-Z hemisphere"), R.Transport->GetPerson(Id).EntryFace, EOVArmorFace::Front);
    TestTrue(TEXT("Arrival sets faction-specific interior protection"), R.Target->Systems->HasInteriorPresence(0));
    TestFalse(TEXT("Transport never completes an interior objective"), R.Target->Systems->IsInteriorObjectiveComplete(0));
    const int32 OriginalPod = R.Transport->GetPerson(Id).PodId;
    TestTrue(TEXT("Explicit DEV token reaching entry resumes original pod"), R.Transport->DevReturnPersonToEntry(Id));
    TestFalse(TEXT("Leaving last passenger removes protection"), R.Target->Systems->HasInteriorPresence(0));
    R.NoBoarding(); R.Transport->Simulate(1);
    TestEqual(TEXT("Return works without origin launch facility"), R.Transport->GetPerson(Id).State, EOVPassengerState::Aboard);
    TestEqual(TEXT("Return preserves individual pod ID"), R.Transport->GetPerson(Id).PodId, OriginalPod);
    TestEqual(TEXT("Return creates no additional pods"), R.Transport->Pods.Num(), 1);
    TestEqual(TEXT("Return does not duplicate entry events"), R.Transport->EntryEvents.Num(), 1);
    R.Transport->Simulate(30); // Isolate pod ownership from the per-target launch interval.
    // Fixture for a future passenger-only transfer; physical pod is still stored at original return host.
    R.Transport->Persons[0].CurrentShip = R.Friend;
    TestEqual(TEXT("Passenger transfer cannot teleport or mint replacement pod hardware"), R.Transport->LaunchPods(R.Friend, R.Target), 0);
    TestEqual(TEXT("Remote pod stays in its recorded return host"), R.Transport->Pods[0].ReturnDestination.Get(), R.Origin);
    TestEqual(TEXT("Remote reuse rejection preserves original hardware count"), R.Transport->Pods.Num(), 1);
    R.Transport->Persons[0].CurrentShip = R.Origin;
    for (int32 I = 0; I < R.Origin->Systems->Slots.Num(); ++I)
        if (R.Origin->Systems->Layout->Slots[I].Facility == EOVFacilityType::Boarding)
            R.Origin->Systems->DebugSetCrew(R.Origin->Systems->Slots[I].SlotId, 10);
    TestEqual(TEXT("A second sortie can reuse recovered pod"), R.Transport->LaunchPods(R.Origin, R.Target), 1);
    TestEqual(TEXT("Second sortie creates no free replacement pod"), R.Transport->Pods.Num(), 1);
    TestEqual(TEXT("Recovered pod retains damage through re-launch"), R.Transport->Pods[0].HP, R.Transport->Settings.PodHP - 10);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVTransportLossBoundaryTest, "OrbitalVanguard.Transport.TargetDepartureReturnFallbackAndResidentDeath", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOVTransportLossBoundaryTest::RunTest(const FString&)
{
    OVTransportTest::FRange R;
    R.Transport->Settings.PodSpeed = 1000;
    int32 Id = R.Transport->AddDevPassengers(R.Origin, 1, 0)[0];
    R.Transport->LaunchPods(R.Origin, R.Target); R.Transport->Simulate(.1f);
    R.Combat->NotifyFTLDeparture(R.Target); R.Origin->Systems->FactionId = 2;
    R.Transport->Simulate(10);
    TestEqual(TEXT("Target departure turns airborne pod back"), R.Transport->Pods[0].State, EOVPodFlightState::Recovered);
    TestTrue(TEXT("Captured origin is replaced by nearest valid friendly destination"), R.Transport->GetPerson(Id).CurrentShip == R.Friend);
    TestEqual(TEXT("Person affiliation is never retroactively captured with origin"), R.Transport->GetPerson(Id).FactionId, 0);
    OVTransportTest::FRange Inside;
    Id = Inside.Transport->AddDevPassengers(Inside.Origin, 1, 0)[0];
    Inside.Transport->LaunchPods(Inside.Origin, Inside.Target); Inside.Transport->Simulate(1);
    Inside.Target->Systems->bWrecked = true; Inside.Transport->Simulate(.1f);
    TestEqual(TEXT("Resident target destruction kills hero-type token instead of space rescue"), Inside.Transport->GetPerson(Id).State, EOVPassengerState::Dead);
    TestEqual(TEXT("Resident death cannot mint a return pod"), Inside.Transport->Pods.Num(), 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVTransportShuttleTest, "OrbitalVanguard.Transport.ShuttleDeliveryRecoveryAndLoss", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOVTransportShuttleTest::RunTest(const FString&)
{
    OVTransportTest::FRange R;
    AOVShip* Shuttle = R.Shuttle();
    R.Target->SetActorScale3D(FVector(13)); Shuttle->SetActorScale3D(FVector(6));
    Shuttle->SetActorLocation(FVector(-8000, 0, 0));
    auto Ids = R.Transport->AddDevPassengers(Shuttle, 4, 6);
    TestTrue(TEXT("Unarmed small shuttle dispatches ten explicit people"), R.Transport->DispatchShuttle(Shuttle, R.Target, Ids));
    R.Drive(Shuttle, .1f);
    TestTrue(TEXT("Explicit manual command suspends transport automation"), R.Transport->SuspendShuttle(Shuttle));
    const FVector ManualDestination(-8000, -2000, 400);
    Shuttle->SetDestination(ManualDestination); R.Drive(Shuttle, 1);
    TestTrue(TEXT("Transport does not overwrite manual XYZ destination"), Shuttle->Destination.Equals(ManualDestination));
    Shuttle->StopMovement(); R.Drive(Shuttle, .5f);
    TestTrue(TEXT("Persistent manual Hold remains stopped while transport is suspended"), Shuttle->bHold && !Shuttle->bHasDestination);
    TestEqual(TEXT("Manual suspension preserves actual embarked passengers"), R.Transport->CountState(EOVPassengerState::InShuttle), 10);
    TestTrue(TEXT("Explicit recall resumes suspended transport"), R.Transport->RecallShuttle(Shuttle));
    TestFalse(TEXT("Recall clears manual suspension"), R.Transport->Shuttles[0].bManualSuspended);
    R.Drive(Shuttle, 20);
    R.Transport->Simulate(30);
    TestTrue(TEXT("Returned passengers can explicitly restart the mission"), R.Transport->DispatchShuttle(Shuttle, R.Target, Ids));
    R.Drive(Shuttle, 12);
    TestEqual(TEXT("Shuttle delivers all ten without creating pods"), R.Transport->EntryEvents.Num(), 10);
    TestEqual(TEXT("No free pods on shuttle entry"), R.Transport->Pods.Num(), 0);
    TestTrue(TEXT("Shuttle entry activates protection"), R.Target->Systems->HasInteriorPresence(0));
    for (int32 Id : Ids) TestTrue(TEXT("DEV token explicitly returns to original entry"), R.Transport->DevReturnPersonToEntry(Id));
    TestTrue(TEXT("Same living shuttle receives recall"), R.Transport->RecallShuttle(Shuttle));
    R.Drive(Shuttle, 20);
    TestEqual(TEXT("Same shuttle recovers all ten IDs"), R.Transport->CountState(EOVPassengerState::Aboard), 10);
    TestEqual(TEXT("Recovery never creates pods"), R.Transport->Pods.Num(), 0);
    TestFalse(TEXT("Recovery clears last faction resident"), R.Target->Systems->HasInteriorPresence(0));
    TestTrue(TEXT("Second sortie reuses same people"), R.Transport->DispatchShuttle(Shuttle, R.Target, Ids));
    R.Transport->SuspendShuttle(Shuttle);
    Shuttle->Systems->bWrecked = true; R.Transport->Simulate(.1f);
    TestEqual(TEXT("Manual suspension does not suppress transport destruction"), R.Transport->Shuttles[0].State, EOVShuttleState::Lost);
    TestEqual(TEXT("Undelivered hero-type shuttle passengers await rescue"), R.Transport->CountState(EOVPassengerState::RescueWaiting), 4);
    TestEqual(TEXT("Undelivered ordinary passengers die"), R.Transport->CountState(EOVPassengerState::Dead), 6);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVTransportTimingTest, "OrbitalVanguard.Transport.FixedTimePauseAndLowFps", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOVTransportTimingTest::RunTest(const FString&)
{
    int32 ReferenceEntries = -1;
    double ReferenceLast = 0;
    for (int32 Rate = 1; Rate <= 3; ++Rate)
    {
        OVTransportTest::FRange R;
        R.Transport->AddDevPassengers(R.Origin, 4, 6); R.Transport->LaunchPods(R.Origin, R.Target);
        R.Transport->Simulate(0);
        TestEqual(TEXT("Pause never launches a pod"), R.Transport->CountState(EOVPassengerState::Aboard), 10);
        // 250ms host frame batches multiplied by time scale; exactly six game seconds.
        for (int32 I = 0; I < 24 / Rate; ++I) R.Transport->Simulate(.25f * Rate);
        if (!TestTrue(TEXT("Fast physical pods reached a target"), !R.Transport->EntryEvents.IsEmpty())) return false;
        if (Rate == 1) { ReferenceEntries = R.Transport->EntryEvents.Num(); ReferenceLast = R.Transport->EntryEvents.Last().Time; }
        TestEqual(TEXT("Equal game time preserves arrivals under time scale"), R.Transport->EntryEvents.Num(), ReferenceEntries);
        TestTrue(TEXT("Fixed step sweep preserves arrival time under low FPS"), FMath::IsNearlyEqual(R.Transport->EntryEvents.Last().Time, ReferenceLast, .00001));
        TestEqual(TEXT("All ten fast pods sweep into target, not through it"), R.Transport->EntryEvents.Num(), 10);
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVBoardingTeamLimitsTest, "OrbitalVanguard.Transport.TeamsPerTargetAndPersonnel", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOVBoardingTeamLimitsTest::RunTest(const FString&)
{
    OVTransportTest::FRange R;
    auto* T=R.Transport;
    auto* Other=R.Make(TEXT("OtherTarget"),1,FVector(0,2500,0));
    Other->Systems->Shield=0;
    Other->Systems->bShieldActive=false;
    R.Combat->RegisterShip(Other);
    T->AddShip(Other);
    const auto FiveHeroes=T->AddDevPassengers(R.Origin,5,0);
    const auto TooMany=T->AddDevPassengers(R.Origin,0,13);
    TestEqual(TEXT("Five heroes exceed team limit"),T->LaunchPodTeam(R.Origin,R.Target,FiveHeroes),0);
    TestEqual(TEXT("Thirteen troops exceed team limit"),T->LaunchPodTeam(R.Origin,R.Target,TooMany),0);
    auto First=T->AddDevPassengers(R.Origin,4,8);
    auto Second=T->AddDevPassengers(R.Origin,0,12);
    auto Third=T->AddDevPassengers(R.Origin,2,6);
    TestEqual(TEXT("Full mixed team"),T->LaunchPodTeam(R.Origin,R.Target,First),12);
    TestEqual(TEXT("Same people cannot board twice"),T->LaunchPodTeam(R.Origin,Other,First),0);
    TestEqual(TEXT("Other target has its own clock"),T->LaunchPodTeam(R.Origin,Other,Third),8);
    T->Simulate(29.9f);
    TestEqual(TEXT("Spacing measured in game seconds"),T->LaunchPodTeam(R.Origin,R.Target,Second),0);
    T->Simulate(.2f);
    TestEqual(TEXT("Second wave allowed"),T->LaunchPodTeam(R.Origin,R.Target,Second),12);
    T->Simulate(30.1f);
    TestEqual(TEXT("Target has two teams"),T->ActiveTeamsAt(R.Target),2);
    TestEqual(TEXT("Third concurrent team on different target"),T->ActiveTeamsAt(Other),1);
    TestEqual(TEXT("Third team on same target blocked"),T->LaunchPodTeam(R.Origin,R.Target,{FiveHeroes[0]}),0);
    return true;
}
#endif
