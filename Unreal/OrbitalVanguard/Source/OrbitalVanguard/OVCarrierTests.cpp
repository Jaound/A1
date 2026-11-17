#include "OVCarrierSubsystem.h"
#include "OVBattleNotices.h"
#include "OVCombatSubsystem.h"
#include "OVCalibration.h"
#include "OVShip.h"
#include "OVShipSystemsComponent.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace OVCarrierTest
{
struct FRange
{
    UWorld* World;
    UOVCombatSubsystem* Combat;
    UOVCarrierSubsystem* Fleet;
    AOVShip* Carrier;
    AOVShip* Enemy;
    FRange(int32 Inventory = 4)
    {
        World = UWorld::CreateWorld(EWorldType::Game, false);
        Combat = World->GetSubsystem<UOVCombatSubsystem>();
        Fleet = World->GetSubsystem<UOVCarrierSubsystem>();
        Carrier = Make(TEXT("Carrier"), 0, FVector::ZeroVector);
        Enemy = Make(TEXT("Enemy"), 1, FVector(6000, 0, 0));
        Enemy->SetActorScale3D(FVector(10));
        Combat->EnableCombat({Carrier, Enemy}, 3104);
        Combat->Settings->GuaranteedRange = 100000;
        Fleet->Reset(3104);
        Fleet->RegisterCarrier(Carrier, Inventory);
    }
    ~FRange() { World->DestroyWorld(false); }
    AOVShip* Make(const TCHAR* Name, int32 Faction, FVector Position)
    {
        auto* Ship = World->SpawnActor<AOVShip>();
        Ship->ShipName = Name; Ship->Systems->FactionId = Faction; Ship->bFriendly = Faction == 0;
        Ship->Systems->Initialize(UShipLayoutDataAsset::MakeTestLayout(Ship, 3));
        Ship->SetActorLocation(Position); Ship->DebugResetMotion(); Ship->SetAutomation(false, false);
        return Ship;
    }
    void Advance(float Seconds, float Chunk = 1.f / 60)
    {
        const int32 Count = FMath::RoundToInt(Seconds / Chunk);
        for (int32 I = 0; I < Count; ++I) Fleet->Simulate(Chunk);
    }
    void Facility(EOVFacilityType Type, float Crew, int32 Occupation = -1)
    {
        for (int32 I = 0; I < Carrier->Systems->Slots.Num(); ++I)
            if (Carrier->Systems->Layout->Slots[I].Facility == Type)
            {
                Carrier->Systems->DebugSetCrew(Carrier->Systems->Slots[I].SlotId, Crew);
                Carrier->Systems->DebugSetOccupation(Carrier->Systems->Slots[I].SlotId, Occupation);
            }
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVAircraftLossWarning,"OrbitalVanguard.Carrier.AircraftLossThresholdsAndOwnership",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FOVAircraftLossWarning::RunTest(const FString&)
{
    OVCarrierTest::FRange R(20);auto* Feed=R.World->GetSubsystem<UOVBattleNotices>();
    const int32 Total=R.Fleet->Aircraft.Num();
    TestTrue(TEXT("기체 분모가 10의 배수"),Total>=20&&Total%10==0);
    TestEqual(TEXT("출격은 재고만 이동"),R.Fleet->Launch(R.Carrier),Total);
    for(int32 I=0;I<Total/2;++I)R.Fleet->ApplyAircraftDamage(R.Fleet->Aircraft[I].AircraftId,100000);
    TestFalse(TEXT("생존 정확히 50%는 절반 미만 아님"),Feed->Notices.ContainsByPredicate([](const FOVBattleNotice& N){return N.Kind==EOVBattleNoticeKind::AircraftHalf;}));
    R.Fleet->ApplyAircraftDamage(R.Fleet->Aircraft[Total/2].AircraftId,100000);
    auto Count=[&](EOVBattleNoticeKind Kind){return Feed->Notices.FilterByPredicate([&](const FOVBattleNotice& N){return N.Kind==Kind;}).Num();};
    TestEqual(TEXT("50% 미만의 모함·함대 경보"),Count(EOVBattleNoticeKind::AircraftHalf),2);
    for(int32 I=Total/2+1;I<Total-Total/10;++I)R.Fleet->ApplyAircraftDamage(R.Fleet->Aircraft[I].AircraftId,100000);
    TestEqual(TEXT("정확히 10%에서 모함·함대 경보"),Count(EOVBattleNoticeKind::AircraftCritical),2);
    TestTrue(TEXT("경보는 실제 분모와 생존 수를 보존"),Feed->Notices.ContainsByPredicate([&](const FOVBattleNotice& N){return N.Kind==EOVBattleNoticeKind::AircraftCritical&&N.Baseline==Total&&N.Remaining==Total/10&&N.bFleetScope;}));
    for(int32 I=Total-Total/10;I<Total;++I)R.Fleet->ApplyAircraftDamage(R.Fleet->Aircraft[I].AircraftId,100000);
    TestEqual(TEXT("실제 모든 기체 완파 시 모함·함대 전멸"),Count(EOVBattleNoticeKind::AircraftEliminated),2);
    const int32 Before=Feed->Notices.Num();
    TestFalse(TEXT("완파 기체 반복 피해 거부"),R.Fleet->ApplyAircraftDamage(R.Fleet->Aircraft[0].AircraftId,100000));
    R.Fleet->Recall(R.Carrier);R.Advance(.1f);
    TestEqual(TEXT("반복 피해와 빈 귀환은 경보를 복제하지 않음"),Feed->Notices.Num(),Before);
    OVCarrierTest::FRange Captured(2);auto* CapturedFeed=Captured.World->GetSubsystem<UOVBattleNotices>();
    Captured.Fleet->Launch(Captured.Carrier);Captured.Carrier->Systems->FactionId=1;Captured.Advance(.02f);
    auto LossWarning=[&](){return CapturedFeed->Notices.ContainsByPredicate([](const FOVBattleNotice& N){return N.Kind==EOVBattleNoticeKind::AircraftHalf||N.Kind==EOVBattleNoticeKind::AircraftCritical||N.Kind==EOVBattleNoticeKind::AircraftEliminated;});};
    TestFalse(TEXT("나포에 따른 기체 소속 조정은 격추가 아님"),LossWarning());
    Captured.Carrier->Systems->FactionId=0;Captured.Advance(.02f);
    TestFalse(TEXT("탈환과 분모 조정만으로 손실 경보하지 않음"),LossWarning());
    TestEqual(TEXT("나포와 탈환을 별도 편제 조정으로 기록"),CapturedFeed->Notices.FilterByPredicate([](const FOVBattleNotice& N){return N.Kind==EOVBattleNoticeKind::AircraftAdjusted;}).Num(),2);
    Captured.Combat->NotifyFTLDeparture(Captured.Carrier);Captured.Advance(.02f);
    TestFalse(TEXT("FTL 이탈만으로 전멸 경보하지 않음"),LossWarning());
    OVCarrierTest::FRange Mixed(10);auto* Second=Mixed.Make(TEXT("SecondCarrier"),0,FVector(0,3000,0));
    TestTrue(TEXT("두 번째 모함 실제 기체 등록"),Mixed.Fleet->RegisterCarrier(Second,10));
    const int32 FirstCount=Mixed.Fleet->GetAircraftOverview(Mixed.Carrier).Surviving;
    TestEqual(TEXT("두 모함은 같은 실제 기체 수"),FirstCount,Mixed.Fleet->GetAircraftOverview(Second).Surviving);
    Mixed.Fleet->Launch(Mixed.Carrier);
    TArray<int32> FirstIds;for(const auto& Craft:Mixed.Fleet->Aircraft)if(Craft.Carrier==Mixed.Carrier)FirstIds.Add(Craft.AircraftId);
    for(int32 Id:FirstIds)Mixed.Fleet->ApplyAircraftDamage(Id,100000);
    auto* MixedFeed=Mixed.World->GetSubsystem<UOVBattleNotices>();
    TestTrue(TEXT("첫 모함 전멸은 해당 모함 범위만 전멸"),MixedFeed->Notices.ContainsByPredicate([](const FOVBattleNotice& N){return N.Kind==EOVBattleNoticeKind::AircraftEliminated&&!N.bFleetScope;}));
    TestFalse(TEXT("다른 모함 생존으로 함대 50%는 손실 경보 아님"),MixedFeed->Notices.ContainsByPredicate([](const FOVBattleNotice& N){return N.bFleetScope&&(N.Kind==EOVBattleNoticeKind::AircraftHalf||N.Kind==EOVBattleNoticeKind::AircraftEliminated);}));
    OVCarrierTest::FRange Empty(0);
    TestFalse(TEXT("분모 0에서는 없는 기체에 피해를 줄 수 없음"),Empty.Fleet->ApplyAircraftDamage(1,100000));
    TestTrue(TEXT("분모 0에서는 손실 경보가 없음"),Empty.World->GetSubsystem<UOVBattleNotices>()->Notices.IsEmpty());
    OVCarrierTest::FRange Tiny(1);auto* TinyFeed=Tiny.World->GetSubsystem<UOVBattleNotices>();
    const int32 TinyCount=Tiny.Fleet->Aircraft.Num();TestTrue(TEXT("작은 실제 기체 집단"),TinyCount>0&&TinyCount<10);
    Tiny.Fleet->Launch(Tiny.Carrier);
    for(const auto& Craft:Tiny.Fleet->Aircraft)Tiny.Fleet->ApplyAircraftDamage(Craft.AircraftId,100000);
    const auto* Final=TinyFeed->Notices.FindByPredicate([](const FOVBattleNotice& N){return N.bFleetScope&&N.Kind==EOVBattleNoticeKind::AircraftEliminated;});
    TestTrue(TEXT("마지막 격추의 10%·전멸 동시 통과 근거 보존"),Final&&(Final->CrossedThresholdMask&6)==6);
    OVCarrierTest::FRange Immediate(4);auto* ImmediateFeed=Immediate.World->GetSubsystem<UOVBattleNotices>();
    Immediate.Fleet->Launch(Immediate.Carrier);Immediate.Advance(.02f);
    const int32 FirstId=Immediate.Fleet->Aircraft[0].AircraftId,SecondId=Immediate.Fleet->Aircraft[1].AircraftId;
    Immediate.Carrier->Systems->FactionId=1;
    TestTrue(TEXT("나포 직후 고정 단계 이전 실제 기체 피해"),Immediate.Fleet->ApplyAircraftDamage(FirstId,100000));
    TestEqual(TEXT("피해보다 먼저 실제 기체 소속을 동기화"),Immediate.Fleet->Aircraft[0].FactionId,1);
    TestFalse(TEXT("적 소속으로 이전된 기체 손실을 아군 전멸로 표시하지 않음"),ImmediateFeed->Notices.ContainsByPredicate([](const FOVBattleNotice& N){return N.Kind==EOVBattleNoticeKind::AircraftEliminated;}));
    Immediate.Carrier->Systems->FactionId=0;
    TestTrue(TEXT("탈환 직후 고정 단계 이전 실제 기체 피해"),Immediate.Fleet->ApplyAircraftDamage(SecondId,100000));
    TestEqual(TEXT("탈환 후 피격은 재편된 아군 기체 ID에 귀속"),Immediate.Fleet->Aircraft[1].FactionId,0);
    TestEqual(TEXT("고정 단계 이전 소유권 변경도 편제 조정으로 기록"),ImmediateFeed->Notices.FilterByPredicate([](const FOVBattleNotice& N){return N.Kind==EOVBattleNoticeKind::AircraftAdjusted;}).Num(),2);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVCarrierInventoryTest, "OrbitalVanguard.Carrier.InventoryCapacityAndRecall", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOVCarrierInventoryTest::RunTest(const FString&)
{
    OVCarrierTest::FRange R(20);
    const int32 Count = R.Fleet->Aircraft.Num();
    TestTrue(TEXT("Fixture actually has a hangar"), Count > 0);
    TestEqual(TEXT("Each hangar starts with exactly twenty stable records"), Count, R.Fleet->Hangars.Num() * 20);
    const int32 Id = R.Fleet->Aircraft[0].AircraftId;
    TestFalse(TEXT("Repeated registration never gives free aircraft"), R.Fleet->RegisterCarrier(R.Carrier, 20));
    R.Fleet->SetCapacityBonus(R.Carrier, 6);
    TestEqual(TEXT("Research expands capacity only"), R.Fleet->GetHangarCapacity(R.Fleet->Hangars[0].HangarId), 26);
    TestEqual(TEXT("Expanded capacity does not manufacture inventory"), R.Fleet->Aircraft.Num(), Count);
    TestFalse(TEXT("Cannot add small ship capacity to occupied fighter hangar"), R.Fleet->SetHangarMode(R.Fleet->Hangars[0].HangarId, EOVHangarMode::SmallShip));
    TestEqual(TEXT("Launch moves all stored craft"), R.Fleet->Launch(R.Carrier), Count);
    R.Fleet->ApplyAircraftDamage(Id, 100);
    const float Damaged = R.Fleet->Aircraft[0].HP;
    R.Advance(.5f); R.Fleet->Recall(R.Carrier); R.Advance(5);
    TestEqual(TEXT("All survivors return as the same records"), R.Fleet->CountAircraft(R.Carrier, EOVAircraftState::Stored), Count);
    TestEqual(TEXT("Recall does not repair hull for free"), R.Fleet->Aircraft[0].HP, Damaged);
    TestEqual(TEXT("Recall preserves ID"), R.Fleet->Aircraft[0].AircraftId, Id);
    TestEqual(TEXT("Relaunch inventory is conserved"), R.Fleet->Launch(R.Carrier), Count);
    TestTrue(TEXT("Single lethal aircraft event"), R.Fleet->ApplyAircraftDamage(Id, 10000));
    TestFalse(TEXT("Wreck cannot take another damage event"), R.Fleet->ApplyAircraftDamage(Id, 10000));
    TestEqual(TEXT("Loss counted once"), R.Fleet->LossCount, 1);
    R.Fleet->Recall(R.Carrier); R.Advance(5); R.Fleet->DebugSupplyRepair(R.Carrier);
    TestEqual(TEXT("Repair supply does not rebuild wreck"), R.Fleet->Aircraft[0].State, EOVAircraftState::Wrecked);
    TestEqual(TEXT("Post-loss launch does not replenish"), R.Fleet->Launch(R.Carrier), Count - 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVCarrierOverviewTest, "OrbitalVanguard.Carrier.ReadOnlyAircraftOverview", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOVCarrierOverviewTest::RunTest(const FString&)
{
    OVCarrierTest::FRange R(8);
    if (!TestTrue(TEXT("Enough individual records"), R.Fleet->Aircraft.Num() >= 7)) return false;
    auto& Aircraft = R.Fleet->Aircraft;
    const EOVAircraftState States[] = {EOVAircraftState::Stored, EOVAircraftState::Sortie,
        EOVAircraftState::Returning, EOVAircraftState::DockWaiting, EOVAircraftState::ControlLost,
        EOVAircraftState::Wrecked};
    for (int32 I=0; I<UE_ARRAY_COUNT(States); ++I) Aircraft[I].State=States[I];
    for (int32 I=6; I<Aircraft.Num(); ++I) Aircraft[I].FactionId=1;
    const int32 Id=Aircraft[0].AircraftId;
    const float HP=Aircraft[0].HP;
    const FOVAircraftOverview A=R.Fleet->GetAircraftOverview(R.Carrier);
    const FOVAircraftOverview B=R.Fleet->GetAircraftOverview(R.Carrier);
    TestTrue(TEXT("Disjoint state counts and same-faction records only"), A.Stored==1 && A.Sortie==1 && A.Returning==2 && A.ControlLost==1 && A.Wrecked==1 && A.Surviving==5);
    TestTrue(TEXT("Repeated UI queries preserve records"), B.Surviving==5 && Aircraft[0].AircraftId==Id && Aircraft[0].HP==HP && Aircraft.Num()>=7);
    TestEqual(TEXT("No carrier gives no inventory"), R.Fleet->GetAircraftOverview(nullptr).Surviving, 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVHangarAvailabilityTest, "OrbitalVanguard.Carrier.HangarAvailabilityPresentation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOVHangarAvailabilityTest::RunTest(const FString&)
{
    OVCarrierTest::FRange R(1);
    if (!TestTrue(TEXT("A real hangar exists"), R.Fleet->Hangars.Num()>0)) return false;
    const auto Hangar=R.Fleet->Hangars[0];
    TestEqual(TEXT("Unknown identifier is unknown"), R.Fleet->GetHangarAvailability(-1), EOVHangarAvailability::Unknown);
    TestEqual(TEXT("Operational facility is ready"), R.Fleet->GetHangarAvailability(Hangar.HangarId), EOVHangarAvailability::Ready);
    R.Facility(EOVFacilityType::Hangar,0);
    TestEqual(TEXT("Crew loss is offline, not destroyed"), R.Fleet->GetHangarAvailability(Hangar.HangarId), EOVHangarAvailability::Offline);
    R.Carrier->Systems->DebugDestroySlot(Hangar.SlotId,EOVDamageCause::Sabotage);
    TestEqual(TEXT("Physical damage is destroyed"), R.Fleet->GetHangarAvailability(Hangar.HangarId), EOVHangarAvailability::Destroyed);
    OVCarrierTest::FRange Wreck(1);
    if (!TestTrue(TEXT("Second hangar exists"), Wreck.Fleet->Hangars.Num()>0)) return false;
    const auto Other=Wreck.Fleet->Hangars[0];
    Wreck.Carrier->Systems->DebugDestroySlot(Wreck.Carrier->Systems->Layout->CoreSlotId,EOVDamageCause::Sabotage);
    TestTrue(TEXT("Fixture ship is wrecked"),Wreck.Carrier->Systems->bWrecked);
    TestEqual(TEXT("Wrecked carrier is not reported as physical hangar destruction"),Wreck.Fleet->GetHangarAvailability(Other.HangarId),EOVHangarAvailability::ShipLost);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVCarrierFacilitiesTest, "OrbitalVanguard.Carrier.FacilityControlAndDockQueue", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOVCarrierFacilitiesTest::RunTest(const FString&)
{
    OVCarrierTest::FRange R;
    R.Facility(EOVFacilityType::FighterControl, 0);
    TestEqual(TEXT("No crew blocks launch"), R.Fleet->Launch(R.Carrier), 0);
    R.Facility(EOVFacilityType::FighterControl, 10, 1);
    TestEqual(TEXT("Enemy occupation blocks launch independently"), R.Fleet->Launch(R.Carrier), 0);
    R.Facility(EOVFacilityType::FighterControl, 10);
    R.Fleet->Launch(R.Carrier); R.Advance(.5f);
    R.Facility(EOVFacilityType::FighterControl, 0);
    const FVector Position = R.Fleet->Aircraft[0].Position;
    R.Advance(1);
    TestEqual(TEXT("External craft transition to control lost"), R.Fleet->Aircraft[0].State, EOVAircraftState::ControlLost);
    TestTrue(TEXT("Control loss freezes flight"), R.Fleet->Aircraft[0].Position.Equals(Position, .001));
    R.Facility(EOVFacilityType::FighterControl, 10);
    R.Facility(EOVFacilityType::Hangar, 0); R.Fleet->Recall(R.Carrier); R.Advance(5);
    TestEqual(TEXT("Disabled hangar holds return in queue"), R.Fleet->Aircraft[0].State, EOVAircraftState::DockWaiting);
    R.Facility(EOVFacilityType::Hangar, 10); R.Advance(5);
    TestEqual(TEXT("Restored hangar safely docks survivor"), R.Fleet->Aircraft[0].State, EOVAircraftState::Stored);
    const int32 Count = R.Fleet->Aircraft.Num();
    R.Carrier->Systems->DebugDestroySlot(R.Fleet->Hangars[0].SlotId, EOVDamageCause::Sabotage);
    R.Advance(.1f);
    TestTrue(TEXT("Destroyed hangar loses stored craft"), R.Fleet->LossCount > 0);
    TestEqual(TEXT("Lost inventory records persist for accounting"), R.Fleet->Aircraft.Num(), Count);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVCarrierOwnershipTest, "OrbitalVanguard.Carrier.OwnershipFTLAndLaunchAttribution", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOVCarrierOwnershipTest::RunTest(const FString&)
{
    OVCarrierTest::FRange R;
    R.Fleet->Launch(R.Carrier); R.Advance(.025f, 1.f / 120);
    TestTrue(TEXT("Aircraft actually issue a projectile"), R.Fleet->ShotCount > 0);
    TestTrue(TEXT("Projectile in flight before capture"), !R.Fleet->Bolts.IsEmpty());
    const int32 Faction = R.Fleet->Bolts.IsEmpty() ? -1 : R.Fleet->Bolts[0].FactionId;
    for (const auto& Slot : R.Carrier->Systems->Layout->Slots)
        R.Carrier->Systems->DebugSetCrew(Slot.SlotId, 0);
    TestTrue(TEXT("Fixture has no operating crew and is neutral"), R.Carrier->Systems->bNeutral);
    const FVector FrozenPosition = R.Fleet->Aircraft[0].Position;
    const float HP = R.Fleet->Aircraft[0].HP, Shield = R.Fleet->Aircraft[0].Shield;
    R.Carrier->Systems->FactionId = 1;
    R.Advance(.025f, 1.f / 120);
    TestEqual(TEXT("Exterior aircraft follow captured uncrewed carrier immediately"), R.Fleet->Aircraft[0].FactionId, 1);
    TestEqual(TEXT("Ownership transfer does not restore control"), R.Fleet->Aircraft[0].State, EOVAircraftState::ControlLost);
    TestTrue(TEXT("Uncrewed capture keeps exterior aircraft stationary"), R.Fleet->Aircraft[0].Position.Equals(FrozenPosition));
    TestEqual(TEXT("Capture does not repair hull"), R.Fleet->Aircraft[0].HP, HP);
    TestEqual(TEXT("Capture does not refill shield"), R.Fleet->Aircraft[0].Shield, Shield);
    if (!R.Fleet->Bolts.IsEmpty()) TestEqual(TEXT("Old projectile keeps original attacker faction"), R.Fleet->Bolts[0].FactionId, Faction);
    R.Facility(EOVFacilityType::FighterControl, 10);
    R.Facility(EOVFacilityType::Hangar, 10);
    R.Advance(.025f, 1.f / 120);
    TestEqual(TEXT("Actual control restoration resumes surviving craft"), R.Fleet->Aircraft[0].State, EOVAircraftState::Sortie);
    TestEqual(TEXT("Restored operation keeps the newly transferred faction"), R.Fleet->Aircraft[0].FactionId, 1);
    R.Carrier->DebugSetFTLCharging(true); R.Advance(.025f, 1.f / 120);
    TestTrue(TEXT("Charging orders emergency recall"), R.Fleet->Carriers[0].bRecallOrdered);
    TestFalse(TEXT("FTL waits for airborne inventory"), R.Fleet->CanDepart(R.Carrier));
    const int32 Count = R.Fleet->Aircraft.Num();
    R.Fleet->AbandonOutstanding(R.Carrier);
    TestTrue(TEXT("Explicit abandon releases recovery gate only"), R.Fleet->CanDepart(R.Carrier));
    TestEqual(TEXT("Abandon neither kills nor creates"), R.Fleet->Aircraft.Num(), Count);
    R.Combat->NotifyFTLDeparture(R.Carrier); R.Advance(.025f, 1.f / 120);
    TestEqual(TEXT("Departure strands external craft"), R.Fleet->Aircraft[0].State, EOVAircraftState::ControlLost);
    TestEqual(TEXT("Stranding is not destruction"), R.Fleet->LossCount, 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVCarrierCombatTest, "OrbitalVanguard.Carrier.RealProjectilesAndFixedTime", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOVCarrierCombatTest::RunTest(const FString&)
{
    int32 BaselineShots = -1, BaselineHits = -1;
    float BaselineShield = 0;
    for (int32 Rate : {1, 2, 3})
    {
        OVCarrierTest::FRange R;
        const float InitialShield = R.Enemy->Systems->Shield;
        R.Fleet->Launch(R.Carrier);
        const FVector Position = R.Fleet->Aircraft[0].Position;
        R.Fleet->Simulate(0);
        TestEqual(TEXT("Paused game issues no aircraft shots"), R.Fleet->ShotCount, 0);
        TestTrue(TEXT("Paused craft do not move"), R.Fleet->Aircraft[0].Position.Equals(Position));
        R.Advance(6, .25f * Rate);
        TestTrue(TEXT("Physical bolts swept into enemy hull"), R.Fleet->HitCount > 0);
        TestTrue(TEXT("Ship shield changed through existing damage route"), R.Enemy->Systems->Shield < InitialShield);
        if (BaselineShots < 0) { BaselineShots = R.Fleet->ShotCount; BaselineHits = R.Fleet->HitCount; BaselineShield = R.Enemy->Systems->Shield; }
        else
        {
            TestEqual(TEXT("Same game time, same shots at 1/2/3"), R.Fleet->ShotCount, BaselineShots);
            TestEqual(TEXT("Same game time, same swept hits at low FPS"), R.Fleet->HitCount, BaselineHits);
            TestEqual(TEXT("Same game time, same damage"), R.Enemy->Systems->Shield, BaselineShield);
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVCarrierDogfightTest, "OrbitalVanguard.Carrier.DogfightAndMotherLoss", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOVCarrierDogfightTest::RunTest(const FString&)
{
    OVCarrierTest::FRange R(2);
    R.Enemy->SetActorScale3D(FVector(1));
    R.Fleet->RegisterCarrier(R.Enemy, 2);
    R.Fleet->Launch(R.Carrier); R.Fleet->Launch(R.Enemy);
    // Separated from ship boxes so the shot is a real aircraft-aircraft collision.
    for (auto& C : R.Fleet->Aircraft) C.Position = FVector(C.FactionId == 0 ? 0 : 600, (C.AircraftId % 2) * 400, 10000);
    const float Before = R.Fleet->Aircraft.Last().Shield;
    R.Advance(8);
    TestTrue(TEXT("Enemy aircraft are actually hit, not ship targets only"),
        R.Fleet->Aircraft.ContainsByPredicate([Before](const FOVAircraftRecord& C) { return C.FactionId == 1 && C.Shield < Before; }));
    TestTrue(TEXT("Dogfight damage is attributable to an opposing launched aircraft bolt"),
        R.Fleet->RecentEvents.ContainsByPredicate([](const FOVAircraftBolt& B)
            { return B.bHit && B.FactionId == 0 && (B.HitId == TEXT("Aircraft_3") || B.HitId == TEXT("Aircraft_4")); }));
    R.Carrier->Systems->bWrecked = true;
    R.Advance(.1f);
    for (const auto& C : R.Fleet->Aircraft)
        if (C.Carrier == R.Carrier && C.State != EOVAircraftState::Wrecked)
            TestEqual(TEXT("Mother loss leaves surviving exterior aircraft control-lost"), C.State, EOVAircraftState::ControlLost);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVCarrierProtectionTest, "OrbitalVanguard.Carrier.InteriorProtectionAndInFlightLifetime", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOVCarrierProtectionTest::RunTest(const FString&)
{
    OVCarrierTest::FRange R(2);
    R.Enemy->Systems->SetInteriorPresence(0, true, false, true);
    R.Combat->SetTarget(R.Carrier, R.Enemy);
    const auto ShipState = R.Combat->GetShipState(R.Carrier);
    R.Combat->SetAttackIntent(R.Carrier, EOVAttackIntent::DisableWeapons, ShipState.Mounts[0].MountId);
    R.Fleet->Launch(R.Carrier); R.Advance(1);
    TestEqual(TEXT("Forced ship mount exception cannot leak to aircraft"), R.Fleet->ShotCount, 0);
    R.Enemy->Systems->SetInteriorPresence(0, false, false, true);
    for (auto& C : R.Fleet->Aircraft) { C.Position = FVector(0, C.AircraftId * 400, 1000); C.Leg = EOVAircraftLeg::Ingress; }
    R.Fleet->Profile.BoltSpeed = 1000;
    R.Fleet->Profile.BoltLifetime = 15;
    R.Advance(.025f, 1.f / 120);
    TestTrue(TEXT("Exit releases fighter strike"), R.Fleet->ShotCount > 0);
    const int32 Shots = R.Fleet->ShotCount;
    R.Enemy->Systems->SetInteriorPresence(0, true, false, true);
    R.Carrier->Systems->bWrecked = true;
    R.Advance(10);
    TestEqual(TEXT("Protected target and wrecked source never issue new fighter shots"), R.Fleet->ShotCount, Shots);
    TestTrue(TEXT("Existing fighter bolts survive mother loss and later protection"), R.Fleet->HitCount > 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVCarrierAlternateDockTest, "OrbitalVanguard.Carrier.AlternateHangarAndExclusiveModes", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOVCarrierAlternateDockTest::RunTest(const FString&)
{
    OVCarrierTest::FRange R(0);
    auto* Layout = R.Carrier->Systems->Layout.Get();
    for (auto& Slot : Layout->Slots)
        if (Slot.Facility == EOVFacilityType::None && Slot.AllowedFacilities.Contains(EOVFacilityType::Hangar))
        { Slot.Facility = EOVFacilityType::Hangar; Slot.MaxFacilityHP = 100; Slot.RequiredCrew = 10; break; }
    R.Carrier->Systems->Initialize(Layout);
    R.Fleet->Reset(3104); R.Fleet->RegisterCarrier(R.Carrier, 0);
    TestEqual(TEXT("Fixture has two separately owned hangar rooms"), R.Fleet->Hangars.Num(), 2);
    const int32 H0 = R.Fleet->Hangars[0].HangarId;
    TestTrue(TEXT("Empty hangar can choose small-ship mode"), R.Fleet->SetHangarMode(H0, EOVHangarMode::SmallShip));
    R.Fleet->SetCapacityBonus(R.Carrier, 6);
    TestEqual(TEXT("Research never expands small-ship capacity"), R.Fleet->GetHangarCapacity(H0), 1);
    TestEqual(TEXT("Small-ship mode never generates fighters"), R.Fleet->Aircraft.Num(), 0);
    R.Fleet->Reset(3104); R.Fleet->RegisterCarrier(R.Carrier, 2);
    const int32 Count = R.Fleet->Aircraft.Num();
    R.Fleet->Launch(R.Carrier);
    R.Carrier->Systems->DebugDestroySlot(R.Fleet->Hangars[0].SlotId, EOVDamageCause::Sabotage);
    R.Advance(.1f);
    TestEqual(TEXT("External inventory is not lost when its original hangar is destroyed"), R.Fleet->LossCount, 0);
    R.Fleet->Recall(R.Carrier); R.Advance(5);
    TestEqual(TEXT("Other operational hangar recovers actual surviving records"), R.Fleet->CountAircraft(R.Carrier, EOVAircraftState::Stored), Count);
    TestTrue(TEXT("Every returned craft records its new storage hangar"),
        !R.Fleet->Aircraft.ContainsByPredicate([H0](const FOVAircraftRecord& C) { return C.HangarId == H0; }));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVCarrierStrikeEnvelopeTest, "OrbitalVanguard.Carrier.StrikeMovingHullEnvelope", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOVCarrierStrikeEnvelopeTest::RunTest(const FString&)
{
    for (const float Scale : {6.f, 13.f, 22.f})
    {
        OVCarrierTest::FRange R(4);
        R.Enemy->SetActorScale3D(FVector(Scale));
        R.Enemy->Systems->Shield = R.Enemy->Systems->MaxShield = 10000000;
        R.Carrier->AttackTarget = R.Enemy;
        R.Fleet->Launch(R.Carrier);
        float LargestExcursion = 0; int32 Passes = 0; float Allowed = 0;
        FVector Previous = R.Fleet->Aircraft[0].Position;
        for (int32 I = 0; I < 3600; ++I)
        {
            const float Time = I / 60.f;
            R.Enemy->SetActorLocation(FVector(16000 + 700 * Time, 2500 * FMath::Sin(Time * .12f), 1000));
            R.Enemy->SetActorRotation(FRotator(0, Time * 4, 0));
            R.Enemy->Velocity = FVector(700, 300 * FMath::Cos(Time * .12f), 0);
            R.Fleet->Simulate(1.f / 60);
            const auto& C = R.Fleet->Aircraft[0];
            TestTrue(TEXT("목표 이동은 기체 순간이동을 일으키지 않는다"), FVector::Dist(C.Position, Previous) <= R.Fleet->Profile.Speed / 60 + .1f);
            Previous = C.Position;
            if (Time > 10)
            {
                LargestExcursion = FMath::Max(LargestExcursion, float(FVector::Dist(C.Position, R.Enemy->GetActorLocation())));
                Allowed = C.AllowedRadius + C.TurnRadius;
            }
            Passes = C.AttackPasses;
        }
        TestTrue(TEXT("이동·선회 표적에서 반복 사격이 실제 유지된다"), Passes >= 3 && R.Fleet->HitCount > 0);
        TestTrue(TEXT("정상 공격 구간은 함체·선회 기반 허용 공간에 머문다"), LargestExcursion <= Allowed);
    }
    // 기하만 바꾼 비교 시험과 별개로 실제 후보 함종/시설/기동 값을 사용한다.
    const FName Hulls[] = {TEXT("corvette"), TEXT("vector"), TEXT("carrier")};
    const float Scales[] = {6, 13, 22};
    for (int32 HullIndex = 0; HullIndex < 3; ++HullIndex)
        for (const bool Moving : {false, true})
        {
            OVCarrierTest::FRange R(4);
            R.Combat->Ships.Reset();
            R.Combat->RegisterShip(R.Carrier, 3, false, 3);
            TestTrue(TEXT("실제 후보 함종 등록"), OVCalibration::ConfigureHull(*R.Combat, *R.Enemy, Hulls[HullIndex], false, FVector(16000, 0, 1000), Scales[HullIndex]));
            R.Combat->Settings->GuaranteedRange = 200000;
            const float BeforeHP = R.Enemy->Systems->HullHP + R.Enemy->Systems->Shield;
            const FVector InitialPosition = R.Enemy->GetActorLocation();
            const FRotator InitialRotation = R.Enemy->GetActorRotation();
            if (Moving) R.Enemy->SetDestination(FVector(70000, 28000, 4000));
            R.Carrier->AttackTarget = R.Enemy;
            R.Fleet->Launch(R.Carrier);
            TArray<float> Distances;
            double TotalDistance = 0;
            for (int32 I = 0; I < 1800; ++I)
            {
                R.Enemy->Simulate(1.f / 60);
                R.Fleet->Simulate(1.f / 60);
                if (I >= 480)
                    for (const auto& C : R.Fleet->Aircraft)
                    {
                        const float Distance = FVector::Dist(C.Position, R.Enemy->GetActorLocation());
                        Distances.Add(Distance); TotalDistance += Distance;
                    }
            }
            const float Damage = BeforeHP - R.Enemy->Systems->HullHP - R.Enemy->Systems->Shield;
            TestTrue(TEXT("실제 후보 대상에 사격·물리 명중·쉴드/선체 피해 연결"), R.Fleet->ShotCount > 4 && R.Fleet->HitCount > 0 && Damage > 0);
            TestFalse(TEXT("기동 검사 중 실제 후보 표적이 조기 완파되지 않는다"), R.Enemy->Systems->bWrecked);
            if (Moving)
            {
                TestTrue(TEXT("후보별 실제 이동 성능으로 위치 변화"), FVector::Dist(InitialPosition, R.Enemy->GetActorLocation()) > 1000);
                TestFalse(TEXT("후보별 실제 선회가 적용됨"), InitialRotation.Equals(R.Enemy->GetActorRotation(), 1));
            }
            else TestTrue(TEXT("정지 표적 위치 보존"), InitialPosition.Equals(R.Enemy->GetActorLocation(), .01));
            Distances.Sort();
            const float P95 = Distances[FMath::Min(Distances.Num()-1, FMath::FloorToInt(Distances.Num() * .95))];
            const float Allowed = R.Fleet->Aircraft[0].AllowedRadius + R.Fleet->Aircraft[0].TurnRadius;
            TestTrue(TEXT("실제 소형/중형/대형 정상 공격 p95가 유한 허용 공간 안에 있음"), P95 <= Allowed);
            AddInfo(FString::Printf(TEXT("실함종=%s 이동=%d 구간=8~30게임초 거리평균=%.2fm p95=%.2fm 최대=%.2fm 전체발사=%d 전체명중=%d 전체피해=%.2f"),
                *Hulls[HullIndex].ToString(), Moving, TotalDistance/Distances.Num()/100, P95/100, Distances.Last()/100,
                R.Fleet->ShotCount, R.Fleet->HitCount, Damage));
        }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVCarrierStrikeInterruptTest, "OrbitalVanguard.Carrier.StrikeDefensiveRejoinAndInvalidation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOVCarrierStrikeInterruptTest::RunTest(const FString&)
{
    OVCarrierTest::FRange R(2);
    R.Fleet->RegisterCarrier(R.Enemy, 2);
    R.Carrier->AttackTarget = R.Enemy;
    R.Fleet->Launch(R.Carrier); R.Fleet->Launch(R.Enemy);
    for (auto& C : R.Fleet->Aircraft)
    {
        C.Position = FVector(6000, C.FactionId == 0 ? -400 : 400, 10000);
        C.HP = C.Shield = 100000;
    }
    R.Advance(.1f);
    TestEqual(TEXT("대함 임무 중 인접 적기에는 제한적인 방어 공중전 진입"), R.Fleet->Aircraft[0].Maneuver, EOVAircraftManeuver::Dogfight);
    R.Advance(R.Fleet->ManeuverProfile.DefensiveSeconds + .2f);
    TestTrue(TEXT("방어 추격 시간 뒤 대함 임무로 복귀하고 즉시 재추격하지 않는다"),
        R.Fleet->Aircraft[0].Maneuver != EOVAircraftManeuver::Dogfight && R.Fleet->Aircraft[0].RejoinRemaining > 0);
    R.Enemy->Systems->SetInteriorPresence(0, true, false, true);
    R.Advance(.1f);
    TestFalse(TEXT("보호 활성화는 대함 표적을 즉시 무효화한다"), R.Fleet->Aircraft[0].StrikeTarget.IsValid());
    R.Enemy->Systems->SetInteriorPresence(0, false, false, true);
    R.Advance(.1f);
    TestEqual(TEXT("보호 해제 후 유효한 대함 임무 재선정"), R.Fleet->Aircraft[0].StrikeTarget.Get(), R.Enemy);
    R.Enemy->Systems->bWrecked = true; R.Advance(.1f);
    TestFalse(TEXT("완파 표적은 다음 기동 단계에서 해제된다"), R.Fleet->Aircraft[0].StrikeTarget.IsValid());
    R.Enemy->Systems->bWrecked = false; R.Advance(.1f);
    R.Combat->NotifyFTLDeparture(R.Enemy); R.Advance(.1f);
    TestFalse(TEXT("이탈한 목표의 오래된 공격 구역은 유지하지 않는다"), R.Fleet->Aircraft[0].StrikeTarget.IsValid());
    R.Fleet->Recall(R.Carrier); R.Advance(5);
    TestEqual(TEXT("명시 복귀는 대함 기동과 방어 추격보다 우선한다"), R.Fleet->CountAircraft(R.Carrier, EOVAircraftState::Stored), 2);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVCarrierMultipleSectionsTest, "OrbitalVanguard.Carrier.MultipleCarrierSectionsAndPureDogfight", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOVCarrierMultipleSectionsTest::RunTest(const FString&)
{
    OVCarrierTest::FRange R(8);
    auto* Second = R.Make(TEXT("Second"), 0, FVector(-2000, 1000, 0));
    R.Combat->RegisterShip(Second, 3, false, 3);
    R.Fleet->RegisterCarrier(Second, 8);
    R.Carrier->AttackTarget = Second->AttackTarget = R.Enemy;
    R.Enemy->Systems->Shield = R.Enemy->Systems->MaxShield = 10000000;
    R.Fleet->Launch(R.Carrier); R.Fleet->Launch(Second); R.Advance(4);
    TSet<int32> Ids; TArray<FVector> Approaches;
    bool FirstFired = false, SecondFired = false;
    for (const auto& C : R.Fleet->Aircraft)
    {
        Ids.Add(C.AircraftId); Approaches.Add(C.ApproachPoint);
        if (C.Carrier == R.Carrier && C.ShotsFired > 0) FirstFired = true;
        if (C.Carrier == Second && C.ShotsFired > 0) SecondFired = true;
    }
    TestEqual(TEXT("다중 항모 기체 ID는 유일하다"), Ids.Num(), R.Fleet->Aircraft.Num());
    TestTrue(TEXT("같은 목표에 다중 항모의 실제 사격이 연결된다"), FirstFired && SecondFired);
    TestTrue(TEXT("소편대가 같은 접근점 하나를 공유하지 않는다"), FVector::Dist(Approaches[0], Approaches[8]) > 500);

    // No registered hostile ship: the same independent dogfight code can fight far from a mother ship.
    R.Enemy->SetActorScale3D(FVector(1)); R.Fleet->RegisterCarrier(R.Enemy, 2); R.Fleet->Launch(R.Enemy);
    R.Combat->Ships.Reset(); R.Carrier->AttackTarget = Second->AttackTarget = nullptr;
    for (auto& C : R.Fleet->Aircraft)
    {
        C.StrikeTarget.Reset(); C.InterceptTargetId = 0;
        C.Position = FVector(100000, C.FactionId == 0 ? 0 : 600, 12000 + C.AircraftId * 100);
    }
    R.Advance(.5f);
    TestEqual(TEXT("독립 공중전에 대함 목표 반경 제한을 적용하지 않는다"), R.Fleet->Aircraft[0].Maneuver, EOVAircraftManeuver::Dogfight);
    TestTrue(TEXT("독립 공중전을 모함 위치로 강제 복귀시키지 않는다"), R.Fleet->Aircraft[0].Position.X > 80000);
    return true;
}
#endif
