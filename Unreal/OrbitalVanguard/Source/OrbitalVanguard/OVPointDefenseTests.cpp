#include "OVPointDefenseSubsystem.h"
#include "OVCarrierSubsystem.h"
#include "OVBattleNotices.h"
#include "OVInteriorSubsystem.h"
#include "OVCombatSubsystem.h"
#include "OVTransportSubsystem.h"
#include "OVShip.h"
#include "OVShipSystemsComponent.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace OVPDTest
{
struct FRange
{
    UWorld* World = nullptr;
    UOVCombatSubsystem* Combat = nullptr;
    UOVPointDefenseSubsystem* PD = nullptr;
    AOVShip* Source = nullptr;
    AOVShip* Target = nullptr;
    AOVShip* Escort = nullptr;
    FName Control;
    FRange()
    {
        World = UWorld::CreateWorld(EWorldType::Game, false);
        Combat = World->GetSubsystem<UOVCombatSubsystem>();
        PD = World->GetSubsystem<UOVPointDefenseSubsystem>();
        Source = Make(0, FVector::ZeroVector);
        Target = Make(1, FVector(18000, 0, 0));
        Escort = Make(1, FVector(7000, 2000, 0));
        Combat->EnableCombat({Source, Target, Escort});
        Combat->Settings->GuaranteedRange = 50000;
        Combat->SetTarget(Source, Target);
        PD->Reset();
        for (const auto& Def : Escort->Systems->Layout->Slots)
            if (Def.Facility == EOVFacilityType::Interception) { Control = Def.SlotId; break; }
    }
    ~FRange() { World->DestroyWorld(false); }
    AOVShip* Make(int32 Faction, FVector Position)
    {
        AOVShip* Ship = World->SpawnActor<AOVShip>();
        Ship->Systems->FactionId = Faction;
        Ship->Systems->Initialize(UShipLayoutDataAsset::MakeTestLayout(Ship, 3));
        Ship->bFriendly = Faction == 0;
        Ship->DebugResetMotion(); Ship->SetAutomation(false, false);
        Ship->SetActorLocation(Position);
        return Ship;
    }
    FOVCombatMountState& Missile()
    {
        auto& State = *Combat->Ships.FindByPredicate([this](const auto& S) { return S.Ship == Source; });
        return *State.Mounts.FindByPredicate([](const auto& M) { return M.Profile.Family == EOVWeaponType::Missile; });
    }
    int32 Launch()
    {
        auto& Mount = Missile();
        Mount.Profile.Kind = EOVAttackKind::GuidedMissile;
        Mount.Profile.Range = 50000; Mount.Profile.Speed = 1000;
        Mount.Profile.Accuracy = 1; Mount.Profile.AimSpreadDegrees = 0;
        Mount.Profile.FireTolerance = 180; Mount.Profile.Salvo = 1;
        Mount.Profile.Interval = .01f; Mount.Profile.ProjectileHP = 24;
        Mount.Profile.InterceptionEvasion = 0;
        Mount.Cooldown = 0;
        Combat->RequestFire(Source, Mount.MountId);
        Combat->Simulate(1.f / 120.f);
        return Combat->ShotCount;
    }
    void Register()
    {
        PD->RegisterShip(Escort, 1);
        auto& Mount = PD->Ships[0].Mounts[0];
        Mount.Profile.Accuracy = 1; Mount.Profile.Damage = 24;
    }
    void Step(float Duration)
    {
        const int32 Count = FMath::RoundToInt(Duration * 120);
        for (int32 I = 0; I < Count; ++I) PD->Step(1.f / 120.f);
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVPDSideMountArc,"OrbitalVanguard.PointDefense.MarkedSideMountArc",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FOVPDSideMountArc::RunTest(const FString&)
{
    OVPDTest::FRange R;
    auto* Layout=R.Escort->Systems->Layout.Get();
    auto& Def=Layout->WeaponMounts.AddDefaulted_GetRef();
    Def.WeaponId=TEXT("SidePD"); Def.Type=EOVWeaponType::Interception; Def.MaxHP=100;
    Def.LocalPosition=FVector::ZeroVector; Def.LocalSurfaceNormal=FVector(0,1,0);
    R.Escort->Systems->Initialize(Layout);
    auto* Carrier=R.World->GetSubsystem<UOVCarrierSubsystem>(); Carrier->Reset(3104);
    TestTrue(TEXT("Enemy carrier registers"),Carrier->RegisterCarrier(R.Source,1));
    TestTrue(TEXT("Aircraft launch"),Carrier->Launch(R.Source)>0);
    for(auto& Craft:Carrier->Aircraft){Craft.Position=R.Escort->GetActorLocation()+FVector(0,-3000,0);Craft.Shield=0;Craft.HP=1;Craft.Evasion=0;}
    R.Register(); R.PD->Ships[0].Mounts[0].Profile.Range=50000;
    R.Step(.1f);
    TestEqual(TEXT("Hull blocks firing at opposite side"),R.PD->ShotCount,0);
    for(auto& Craft:Carrier->Aircraft) Craft.Position=R.Escort->GetActorLocation()+FVector(0,3000,0);
    R.Step(.1f);
    TestTrue(TEXT("The same mount fires at a target on its outward side"),R.PD->ShotCount>0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVPDAircraftWarning,"OrbitalVanguard.PointDefense.ActualAircraftKillsTriggerWarnings",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FOVPDAircraftWarning::RunTest(const FString&)
{
    OVPDTest::FRange R;
    auto* Carrier=R.World->GetSubsystem<UOVCarrierSubsystem>();Carrier->Reset(3104);
    TestTrue(TEXT("실제 아군 모함과 재고 등록"),Carrier->RegisterCarrier(R.Source,1));
    const int32 Total=Carrier->Aircraft.Num();TestTrue(TEXT("실제 기체 집단 존재"),Total>0);
    TestEqual(TEXT("실제 기체 출격"),Carrier->Launch(R.Source),Total);
    for(auto& Craft:Carrier->Aircraft){Craft.Position=R.Escort->GetActorLocation()+FVector(1000,0,0);Craft.Shield=0;Craft.HP=1;Craft.Evasion=0;}
    R.Register();auto& Mount=R.PD->Ships[0].Mounts[0];
    Mount.Profile.Accuracy=1;Mount.Profile.Damage=10000;Mount.Profile.Interval=.01f;Mount.Profile.Range=50000;
    R.PD->SetPriority(R.Escort,{EOVPDTargetKind::Aircraft,EOVPDTargetKind::Missile,EOVPDTargetKind::Pod});
    R.Step(2);
    TestEqual(TEXT("실제 적 PD 발사로 모든 기체 격추"),R.PD->KillCount,Total);
    TestEqual(TEXT("실제 기체 손실 횟수"),Carrier->LossCount,Total);
    auto* Feed=R.World->GetSubsystem<UOVBattleNotices>();
    TestTrue(TEXT("실제 PD 격추는 모함 및 함대 전멸 경보와 연결"),Feed->Notices.ContainsByPredicate([](const FOVBattleNotice& N){return N.Kind==EOVBattleNoticeKind::AircraftEliminated&&!N.bFleetScope;})&&Feed->Notices.ContainsByPredicate([](const FOVBattleNotice& N){return N.Kind==EOVBattleNoticeKind::AircraftEliminated&&N.bFleetScope;}));
    Feed->RecordAircraftAdjustment(R.Source,0,0,2.0);
    const int32 Toast=Feed->GetToastIndex(FPlatformTime::Seconds());
    TestTrue(TEXT("실제 격추 직후 일반 편제 알림이 전멸 경보를 가리지 않음"),Feed->Notices.IsValidIndex(Toast)&&Feed->Notices[Toast].Kind==EOVBattleNoticeKind::AircraftEliminated);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVPDHeroRescueNotice,"OrbitalVanguard.PointDefense.ActualHeroPodRescueWarning",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FOVPDHeroRescueNotice::RunTest(const FString&)
{
    OVPDTest::FRange R;
    auto* Transport=R.World->GetSubsystem<UOVTransportSubsystem>();Transport->ResetScenario({R.Source,R.Target,R.Escort});
    auto* Interior=R.World->GetSubsystem<UOVInteriorSubsystem>();Interior->ResetScenario({R.Source,R.Target,R.Escort},3104);
    const auto Units=Interior->RegisterBoardingTeam(R.Source,0,1,0);
    TestEqual(TEXT("실제 강습 영웅 등록"),Units.Num(),1);
    if(Units.Num()!=1)return false;
    TestEqual(TEXT("영웅 원 포드 발사"),Transport->LaunchPods(R.Source,R.Target),1);
    Transport->Simulate(1.f/120.f);
    if(!TestTrue(TEXT("실제 포드가 비행 중"),!Transport->Pods.IsEmpty()&&Transport->Pods[0].State==EOVPodFlightState::Outbound))return false;
    Transport->Pods[0].Evasion=0;Transport->Pods[0].HP=1;
    const FVector LastPosition=Transport->Pods[0].Position;
    R.Register();auto& Mount=R.PD->Ships[0].Mounts[0];
    Mount.Profile.Accuracy=1;Mount.Profile.Damage=10000;Mount.Profile.Range=50000;
    R.PD->SetPriority(R.Escort,{EOVPDTargetKind::Pod,EOVPDTargetKind::Aircraft,EOVPDTargetKind::Missile});
    R.PD->Step(1.f/120.f);Interior->Simulate(1.f/60.f);
    TestEqual(TEXT("실제 적 PD가 포드를 격추"),R.PD->KillCount,1);
    TestEqual(TEXT("실제 영웅은 구조 대기"),Interior->GetUnit(Units[0]).State,EOVInteriorState::RescueWaiting);
    auto* Feed=R.World->GetSubsystem<UOVBattleNotices>();
    TestTrue(TEXT("실제 PD 격추 위치를 구조 경보에 보존"),Feed->Notices.ContainsByPredicate([&](const FOVBattleNotice& N){return N.Kind==EOVBattleNoticeKind::HeroRescue&&N.UnitId==Units[0]&&N.LastPosition.Equals(LastPosition,.01);}));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVPDProbabilityTest, "OrbitalVanguard.PointDefense.ProbabilityAndTargetBoundary", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOVPDProbabilityTest::RunTest(const FString&)
{
    TestTrue(TEXT("D091 laser aircraft .2736, no additive/subtractive evasion"), FMath::IsNearlyEqual(UOVPointDefenseSubsystem::HitProbability(.72f, .62f), .2736f));
    TestTrue(TEXT("Kinetic pods .372"), FMath::IsNearlyEqual(UOVPointDefenseSubsystem::HitProbability(.62f, .4f), .372f));
    TestTrue(TEXT("Laser missile .54"), FMath::IsNearlyEqual(UOVPointDefenseSubsystem::HitProbability(.72f, .25f), .54f));
    TestEqual(TEXT("Probability clamps high"), UOVPointDefenseSubsystem::HitProbability(2, 0), 1.f);
    TestEqual(TEXT("Full evasion blocks"), UOVPointDefenseSubsystem::HitProbability(1, 1), 0.f);
    OVPDTest::FRange R;
    R.Register();
    const float HP = R.Source->Systems->HullHP;
    R.Step(2);
    TestEqual(TEXT("Enemy ship in PD range is never a PD candidate"), R.PD->ShotCount, 0);
    TestEqual(TEXT("PD cannot damage assault or other hulls"), R.Source->Systems->HullHP, HP);
    R.PD->Ships[0].Mounts[0].HP = 25;
    R.PD->RegisterShip(R.Escort, 11, 1.5f);
    TestEqual(TEXT("Repeated registration does not duplicate mounts"), R.PD->Ships[0].Mounts.Num(), 1);
    TestEqual(TEXT("Repeated registration does not heal mount"), R.PD->Ships[0].Mounts[0].HP, 25.f);
    TestFalse(TEXT("Priorities reject repeated/missing category"), R.PD->SetPriority(R.Escort, {EOVPDTargetKind::Missile, EOVPDTargetKind::Missile, EOVPDTargetKind::Aircraft}));
    TestTrue(TEXT("User may prioritize all three categories"), R.PD->SetPriority(R.Escort, {EOVPDTargetKind::Pod, EOVPDTargetKind::Aircraft, EOVPDTargetKind::Missile}));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVPDMissileTest, "OrbitalVanguard.PointDefense.LaunchedMissileSingleOpportunityAndImpact", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOVPDMissileTest::RunTest(const FString&)
{
    OVPDTest::FRange R;
    const int32 First = R.Launch();
    const int32 Second = R.Launch();
    TestTrue(TEXT("Two actual launch requests created physical attacks"), Second == First + 1 && R.Combat->Attacks.Num() == 2);
    if (R.Combat->Attacks.Num() != 2) return false;
    FOVAttackState Stale = R.Combat->Attacks[0];
    R.Register();
    R.PD->Step(0);
    TestEqual(TEXT("Paused PD cannot fire"), R.PD->ShotCount, 0);
    R.PD->Step(1.f / 120.f);
    TestEqual(TEXT("One mount consumes exactly one opportunity against multiple missiles"), R.PD->ShotCount, 1);
    TestEqual(TEXT("Only one actual missile destroyed"), R.Combat->Attacks.Num(), 1);
    TestEqual(TEXT("Kill event emitted once"), R.PD->KillCount, 1);
    TestFalse(TEXT("Destroyed attack cannot accept damage again"), R.Combat->ApplyInterceptionDamage(Stale.AttackId, 24));
    const int32 Hits = R.Target->Systems->HitEventId;
    TestFalse(TEXT("Stale copy cannot apply ship impact after interception"), R.Combat->ResolveImpact(Stale, R.Target, R.Target->GetActorLocation(), -FVector::ForwardVector));
    TestEqual(TEXT("Intercepted projectile delivered no later hull hit"), R.Target->Systems->HitEventId, Hits);
    R.PD->Step(1.f / 120.f);
    TestEqual(TEXT("Cooldown cannot be reused on next target"), R.PD->ShotCount, 1);
    R.Step(.5f);
    TestEqual(TEXT("Next independent opportunity kills remaining missile"), R.PD->KillCount, 2);
    TestEqual(TEXT("Attack resolved as Intercepted"), int32(R.Combat->RecentEvents.Last().Result), int32(EOVAttackResult::Intercepted));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVPDGatesTest, "OrbitalVanguard.PointDefense.FacilityRangeOwnershipAndCrew", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOVPDGatesTest::RunTest(const FString&)
{
    OVPDTest::FRange R;
    R.Launch(); R.Launch(); R.Launch();
    R.Register();
    TestFalse(TEXT("Fixture has a real PD control facility"), R.Control.IsNone());
    if (R.Control.IsNone()) return false;
    R.Escort->SetActorLocation(FVector(50000, 0, 0));
    R.Step(1);
    TestEqual(TEXT("Actual distance outside gun range blocks"), R.PD->ShotCount, 0);
    R.Escort->SetActorLocation(FVector(7000, 2000, 0));
    R.Escort->Systems->DebugSetCrew(R.Control, 0);
    R.Step(1);
    TestEqual(TEXT("No PD operators blocks"), R.PD->ShotCount, 0);
    R.Escort->Systems->ResetState();
    R.Escort->Systems->DebugSetOccupation(R.Control, 0);
    R.Step(1);
    TestEqual(TEXT("Occupied PD control blocks"), R.PD->ShotCount, 0);
    R.Escort->Systems->ResetState();
    R.Escort->Systems->DebugDestroySlot(R.Control, EOVDamageCause::Sabotage);
    R.Step(1);
    TestEqual(TEXT("Destroyed PD control blocks"), R.PD->ShotCount, 0);
    R.Escort->Systems->ResetState();
    R.Escort->Systems->FactionId = 0;
    R.Step(1);
    TestEqual(TEXT("Capture immediately changes valid hostile projectiles"), R.PD->ShotCount, 0);
    R.Escort->Systems->FactionId = 1;
    for (const auto& Def : R.Escort->Systems->Layout->Slots)
    {
        if (Def.Facility == EOVFacilityType::WeaponControl) R.Escort->Systems->DebugSetCrew(Def.SlotId, 0);
        if (Def.Facility == EOVFacilityType::Interception) R.Escort->Systems->DebugSetCrew(Def.SlotId, Def.RequiredCrew * .5f);
    }
    R.Step(.5f);
    TestEqual(TEXT("PD independent of offensive weapon control, half crew first shot"), R.PD->ShotCount, 1);
    R.Step(.5f);
    TestEqual(TEXT("Half crew has not regained another .5 base interval yet"), R.PD->ShotCount, 1);
    R.Step(1.f / 120.f);
    TestEqual(TEXT("Half crew opportunity restores at one game second"), R.PD->ShotCount, 2);
    R.Combat->NotifyFTLDeparture(R.Escort);
    R.Step(2);
    TestEqual(TEXT("Departed escort cannot intercept"), R.PD->ShotCount, 2);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVPDPriorityTest, "OrbitalVanguard.PointDefense.PriorityAndAlliedCoverage", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOVPDPriorityTest::RunTest(const FString&)
{
    OVPDTest::FRange R;
    R.Launch();
    auto* Transport = R.World->GetSubsystem<UOVTransportSubsystem>();
    Transport->ResetScenario({R.Source, R.Target, R.Escort});
    const TArray<int32> People = Transport->AddDevPassengers(R.Source, 1, 0);
    TestEqual(TEXT("Real transport order queues one individual pod"), Transport->LaunchPods(R.Source, R.Target), 1);
    Transport->Simulate(1.f / 120.f);
    if (Transport->Pods.IsEmpty() || People.IsEmpty()) return false;
    Transport->Pods[0].Evasion = 0;
    R.Register();
    R.PD->Ships[0].Mounts[0].Profile.Damage = 10000;
    R.PD->SetPriority(R.Escort, {EOVPDTargetKind::Pod, EOVPDTargetKind::Missile, EOVPDTargetKind::Aircraft});
    R.PD->Step(1.f / 120.f);
    if (R.PD->RecentEvents.IsEmpty()) return false;
    TestEqual(TEXT("User priority chooses pod over nearby missile"), int32(R.PD->RecentEvents.Last().TargetKind), int32(EOVPDTargetKind::Pod));
    TestEqual(TEXT("Shot-down pod cannot reach the target"), int32(Transport->Pods[0].State), int32(EOVPodFlightState::Destroyed));
    TestEqual(TEXT("PD pod loss uses transport hero rescue boundary"), int32(Transport->GetPerson(People[0]).State), int32(EOVPassengerState::RescueWaiting));
    TestEqual(TEXT("Same mount did not consume a second chance against missile"), R.Combat->Attacks.Num(), 1);
    R.Step(.5f);
    TestEqual(TEXT("Escort can protect another friendly ship targeted by missile"), R.Combat->Attacks.Num(), 0);
    TestEqual(TEXT("No extra target evasion rolls after success"), R.PD->HitCount, 2);
    return true;
}
#endif
