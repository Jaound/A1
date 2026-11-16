#include "OVShipSystemsComponent.h"
#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
namespace OVTest {
UOVShipSystemsComponent* Make(int32 Decks=3) { auto* S=NewObject<UOVShipSystemsComponent>(); S->Initialize(UShipLayoutDataAsset::MakeTestLayout(S,Decks)); return S; }
TArray<int32> Find(UOVShipSystemsComponent* S,EOVFacilityType Type) { TArray<int32> R;for(int32 I=0;I<S->Slots.Num();++I)if(S->Layout->Slots[I].Facility==Type)R.Add(I);return R; }
void Hit(UOVShipSystemsComponent* S,FVector P,float Amount) { S->ApplyWorldHit(P,P.GetSafeNormal(),Amount,EOVDamageCause::External); }
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVEfficiencyTest,"OrbitalVanguard.Systems.IndependentInstancesAndCrew",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FOVEfficiencyTest::RunTest(const FString&)
{
 auto* A=OVTest::Make();auto* B=NewObject<UOVShipSystemsComponent>();B->Initialize(A->Layout);
 const auto Engines=OVTest::Find(A,EOVFacilityType::Engine); TestEqual(TEXT("Three engines"),Engines.Num(),3);
 TestEqual(TEXT("100+50+50 percent"),A->GetEfficiency(EOVFacilityType::Engine),2.f);
 A->DebugSetCrew(A->Slots[Engines[2]].SlotId,0);TestEqual(TEXT("Two full engines"),A->GetEfficiency(EOVFacilityType::Engine),1.5f);
 A->DebugSetCrew(A->Slots[Engines[1]].SlotId,0);TestEqual(TEXT("One full engine"),A->GetEfficiency(EOVFacilityType::Engine),1.f);
 A->DebugSetCrew(A->Slots[Engines[0]].SlotId,4);A->DebugSetCrew(A->Slots[Engines[1]].SlotId,8);
 TestTrue(TEXT("Highest crew ratio is primary"),FMath::IsNearlyEqual(A->GetEfficiency(EOVFacilityType::Engine),1.f));
 A->DebugSetOccupation(A->Slots[Engines[1]].SlotId,1);TestTrue(TEXT("Occupation excludes own physical repair"),FMath::IsNearlyEqual(A->GetEfficiency(EOVFacilityType::Engine),.4f));
 A->DebugRepairSlot(A->Slots[Engines[1]].SlotId);TestEqual(TEXT("Repair preserves occupation"),A->Slots[Engines[1]].OccupyingFaction,1);
 TestEqual(TEXT("Other ship crew remains independent"),B->GetEfficiency(EOVFacilityType::Engine),2.f);
 const float Range=A->GetRadarRange();const auto Radars=OVTest::Find(A,EOVFacilityType::Radar);
 A->DebugSetCrew(A->Slots[Radars[0]].SlotId,0);TestEqual(TEXT("Radar duplicates never extend range"),A->GetRadarRange(),Range);
 for(auto& Slot:A->Slots) Slot.Crew=0;A->RefreshDerivedState();TestTrue(TEXT("Crew extinction neutral not wreck"),A->bNeutral&&!A->bWrecked&&!A->CanMove());
 TestEqual(TEXT("Unmanned core remains operational"),A->GetEfficiency(EOVFacilityType::Core),1.f);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVShieldTest,"OrbitalVanguard.Systems.ShieldStateAndTime",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FOVShieldTest::RunTest(const FString&)
{
 auto* S=OVTest::Make();const FVector Point(0,0,30);const float Max=S->MaxShield;
 OVTest::Hit(S,Point,Max);TestTrue(TEXT("Exact break causes no hull loss"),!S->bShieldActive&&S->Shield==0&&S->LastHullDamage==0);
 S->Tuning.NoHitRecoveryInterval=1000;S->Tuning.ShieldRecoveryDelay=999;
 S->Simulate(1);TestTrue(TEXT("D116 ignores legacy delay and immediately recovers"),FMath::IsNearlyEqual(S->Shield,Max*.006f,.001f));
 S->Simulate(90);TestTrue(TEXT("50 percent auto reactivate"),S->bShieldActive&&FMath::IsNearlyEqual(S->Shield,Max*.5f,.01f));
 const float Half=S->Shield;S->Simulate(2);TestEqual(TEXT("Active combat no regen"),S->Shield,Half);
 S->Tuning.NoHitRecoveryInterval=15;S->Simulate(100);TestEqual(TEXT("D083 active shield never regenerates, including non-combat"),S->Shield,Half);
 S->Tuning.RecoveryPolicy=EOVShieldRecoveryPolicy::FullAfterNoHitInterval;
 S->Simulate(100);TestEqual(TEXT("Legacy serialized recovery policy cannot restore active-shield full charge"),S->Shield,Half);
 const auto Controls=OVTest::Find(S,EOVFacilityType::ShieldControl);
 for(int32 I:Controls)S->DebugSetOccupation(S->Slots[I].SlotId,1);
 TestTrue(TEXT("All controls unavailable immediately disappear"),S->Shield==0&&S->MaxShield==0&&!S->bShieldActive);
 S->Simulate(100);TestEqual(TEXT("No controls no recovery"),S->Shield,0.f);
 S->DebugSetOccupation(S->Slots[Controls[0]].SlotId,-1);TestEqual(TEXT("Restore control never gifts charge"),S->Shield,0.f);
 float Results[3];for(int32 Scale=1;Scale<=3;++Scale){auto* T=OVTest::Make();OVTest::Hit(T,Point,T->MaxShield);for(int32 I=0;I<600/Scale;++I)T->Simulate(Scale/60.f);Results[Scale-1]=T->Shield;}
 TestTrue(TEXT("Same game time across speeds"),FMath::IsNearlyEqual(Results[0],Results[1],.01f)&&FMath::IsNearlyEqual(Results[0],Results[2],.01f));
 auto* Fast=OVTest::Make();Fast->Tuning.ShieldRecoveryDelay=0;Fast->Tuning.NoHitRecoveryInterval=2;
 Fast->Tuning.ShieldRegenFraction=.03f;Fast->Tuning.ShieldNonCombatRegenFraction=.06f;
 OVTest::Hit(Fast,Point,Fast->MaxShield);Fast->Simulate(1);
 TestTrue(TEXT("Inactive combat recovery uses explicitly configured slow rate"),FMath::IsNearlyEqual(Fast->Shield,Fast->MaxShield*.03f,.001f));
 Fast->Simulate(1);TestTrue(TEXT("Combat to non-combat boundary has no instant fill"),FMath::IsNearlyEqual(Fast->Shield,Fast->MaxShield*.06f,.001f));
 Fast->Simulate(1);TestTrue(TEXT("Inactive non-combat recovery uses faster configured rate"),FMath::IsNearlyEqual(Fast->Shield,Fast->MaxShield*.12f,.001f));
 Fast->Simulate(100);TestTrue(TEXT("Large non-combat step stops at 50 percent reactivation"),Fast->bShieldActive&&FMath::IsNearlyEqual(Fast->Shield,Fast->MaxShield*.5f,.001f));
 auto* Boundary=OVTest::Make();Boundary->Tuning.ShieldRecoveryDelay=0;Boundary->Tuning.NoHitRecoveryInterval=2;
 Boundary->Tuning.ShieldRegenFraction=.03f;Boundary->Tuning.ShieldNonCombatRegenFraction=.06f;
 OVTest::Hit(Boundary,Point,Boundary->MaxShield);Boundary->Simulate(3);
 TestTrue(TEXT("Single step crossing combat boundary equals split recovery"),FMath::IsNearlyEqual(Boundary->Shield,Boundary->MaxShield*.12f,.001f));
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVDamageTest,"OrbitalVanguard.Systems.ArmorCausesAndSingleHullDamage",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FOVDamageTest::RunTest(const FString&)
{
 auto* S=OVTest::Make(1);FVector P(175,0,0);const int32 A=S->Layout->ResolveArmor(P,FVector::ForwardVector);const int32 Room=S->Layout->MapArmorToSlot(A,P);
 const float Armor=S->ArmorSections[A].HP;S->DebugDestroySlot(S->Slots[Room].SlotId,EOVDamageCause::External);
 TestEqual(TEXT("Single end face survives room destruction"),S->ArmorSections[A].HP,Armor);
 S->ResetState();const int32 Engine=OVTest::Find(S,EOVFacilityType::Engine)[0];
 S->DebugDestroySlot(S->Slots[Engine].SlotId,EOVDamageCause::Sabotage);
 for(int32 I=0;I<S->ArmorSections.Num();++I)TestEqual(TEXT("Sabotage preserves all armor"),S->ArmorSections[I].HP,S->Layout->ArmorSections[I].MaxArmor);
 S->ResetState();S->bShieldActive=false;S->Shield=0;
 const float Before=S->HullHP;OVTest::Hit(S,P,40);
 TestTrue(TEXT("Front 75 percent reduction then HP ratio only once"),FMath::IsNearlyEqual(Before-S->HullHP,5.f));
 S->ResetState();const float Initial=S->HullHP;OVTest::Hit(S,P,S->Shield+40);
 TestTrue(TEXT("Overflow fraction .25 applied only at break"),FMath::IsNearlyEqual(Initial-S->HullHP,1.25f));
 S->ResetState();S->DebugDestroySlot(S->Layout->CoreSlotId,EOVDamageCause::Sabotage);
 TestTrue(TEXT("Core immediately wrecks with remaining HP"),S->bWrecked&&S->HullHP>0&&S->WreckEventCount==1);
 const float WreckHP=S->HullHP;S->DebugRepairSlot(S->Layout->CoreSlotId);OVTest::Hit(S,P,9999);TestTrue(TEXT("Post-wreck repair/damage cannot revive or repeat"),S->bWrecked&&S->HullHP==WreckHP&&S->WreckEventCount==1);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVPenetrationTest,"OrbitalVanguard.Systems.VerticalPenetrationAndEmptySlots",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FOVPenetrationTest::RunTest(const FString&)
{
 auto* S=OVTest::Make();S->bShieldActive=false;S->Shield=0;
 FVector P(-140,-50,30);int32 Armor=S->Layout->ResolveArmor(P,FVector::UpVector);S->DebugSetArmor(S->ArmorSections[Armor].ArmorSectionId,0);
 const float Before=S->HullHP;OVTest::Hit(S,P,100);
 TestEqual(TEXT("Empty outer and middle deliver raw hull once"),Before-S->HullHP,100.f);TestEqual(TEXT("Empty never explodes"),S->LastExplosionCount,0);
 TestEqual(TEXT("Penetration ends on middle, not bottom"),S->Layout->Slots[S->LastDamageIndex].DeckId,2);
 S->ResetState();S->bShieldActive=false;S->Shield=0;
 int32 Middle=INDEX_NONE;for(int32 I=0;I<S->Slots.Num();++I)if(S->Layout->Slots[I].DeckId==2&&S->Layout->Slots[I].Facility!=EOVFacilityType::None&&S->Layout->Slots[I].Facility!=EOVFacilityType::Core){Middle=I;break;}
 TestTrue(TEXT("Middle populated fixture"),Middle!=INDEX_NONE);if(Middle==INDEX_NONE)return false;
 P=S->Layout->Slots[Middle].LocalCenter;P.Z=30;Armor=S->Layout->ResolveArmor(P,FVector::UpVector);S->DebugSetArmor(S->ArmorSections[Armor].ArmorSectionId,0);
 OVTest::Hit(S,P,20);TestEqual(TEXT("Populated middle receives penetration"),S->LastDamageIndex,Middle);
 TestEqual(TEXT("Middle room receives raw damage"),S->Slots[Middle].HP,S->Layout->Slots[Middle].MaxFacilityHP-20);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVExplosionTest,"OrbitalVanguard.Systems.ExplosionQueueAndWeaponIndependence",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FOVExplosionTest::RunTest(const FString&)
{
 auto* S=OVTest::Make();const int32 Ammo=OVTest::Find(S,EOVFacilityType::Ammo)[0];
 for(EOVFacilityType Type:{EOVFacilityType::Ammo,EOVFacilityType::Capacitor,EOVFacilityType::MissileStorage})S->Slots[OVTest::Find(S,Type)[0]].HP=1;
 S->DebugDestroySlot(S->Slots[Ammo].SlotId,EOVDamageCause::External);
 TestEqual(TEXT("Three adjacent explosives detonate once each"),S->LastExplosionCount,3);const int32 Event=S->HitEventId;
 S->DebugDestroySlot(S->Slots[Ammo].SlotId,EOVDamageCause::Explosion);TestEqual(TEXT("Destroyed source cannot repeat"),S->HitEventId,Event);
 S->ResetState();const float Hull=S->HullHP;const FName Gun=S->WeaponMountStates[0].WeaponMountId;
 S->DebugHitWeapon(Gun,10000);TestTrue(TEXT("Weapon HP blocks independently"),!S->IsWeaponAvailable(Gun)&&S->HullHP==Hull);
 const auto Control=OVTest::Find(S,EOVFacilityType::WeaponControl)[0];S->DebugSetCrew(S->Slots[Control].SlotId,0);S->DebugRepairWeapon(Gun);
 TestFalse(TEXT("Repair gun does not fix control crew"),S->IsWeaponAvailable(Gun));return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVOuterFaceDestructionTest,"OrbitalVanguard.Systems.ThreeDeckDestructionFaces",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FOVOuterFaceDestructionTest::RunTest(const FString&)
{
    for (int32 Deck = 1; Deck <= 3; ++Deck)
    {
        for (EOVDamageCause Cause : {EOVDamageCause::External, EOVDamageCause::Explosion, EOVDamageCause::Sabotage})
        {
            auto* Layout = UShipLayoutDataAsset::MakeTestLayout(GetTransientPackage(), 3);
            const FName RoomId(*FString::Printf(TEXT("D%d_X4_Y0"), Deck));
            const int32 Room = Layout->GetSlotIndex(RoomId);
            // A non-explosive edge room overlaps an end face, a side, and (on outer decks) top/bottom.
            Layout->Slots[Room].Facility = EOVFacilityType::Engine;
            Layout->Slots[Room].MaxFacilityHP = 100;
            Layout->Slots[Room].RequiredCrew = 10;
            auto* Systems = NewObject<UOVShipSystemsComponent>();
            Systems->Initialize(Layout);
            TestNotNull(TEXT("Edge-room fixture validates"), Systems->Layout.Get());
            if (!Systems->Layout) return false;
            Systems->DebugDestroySlot(RoomId, Cause);
            int32 Broken = 0;
            int32 PreservedLinked = 0;
            for (int32 Index = 0; Index < Layout->ArmorSections.Num(); ++Index)
            {
                const auto& Armor = Layout->ArmorSections[Index];
                const bool Linked = Armor.InnerSlotIds.Contains(RoomId);
                const bool MatchingFace = (Deck == 3 && Armor.Face == EOVArmorFace::Top)
                    || (Deck == 2 && (Armor.Face == EOVArmorFace::Left || Armor.Face == EOVArmorFace::Right))
                    || (Deck == 1 && Armor.Face == EOVArmorFace::Bottom);
                const bool ShouldBreak = Linked && MatchingFace && Cause != EOVDamageCause::Sabotage;
                TestEqual(FString::Printf(TEXT("Deck %d cause %d section %s"), Deck, int32(Cause), *Armor.ArmorId.ToString()),
                    Systems->ArmorSections[Index].HP, ShouldBreak ? 0.f : Armor.MaxArmor);
                if (ShouldBreak) ++Broken;
                if (Linked && !ShouldBreak) ++PreservedLinked;
            }
            if (Cause != EOVDamageCause::Sabotage) TestTrue(TEXT("The deck's appropriate armor face is destroyed"), Broken > 0);
            TestTrue(TEXT("Unrelated linked faces survive room destruction"), PreservedLinked > 0);
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVShieldCapacityTransitionTest,"OrbitalVanguard.Systems.PartialShieldCapacityTransition",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FOVShieldCapacityTransitionTest::RunTest(const FString&)
{
    auto* Systems = OVTest::Make();
    Systems->Tuning.NoHitRecoveryInterval = 100;
    Systems->Tuning.ShieldRegenFraction = .03f;
    Systems->RefreshDerivedState();
    const auto Controls = OVTest::Find(Systems, EOVFacilityType::ShieldControl);
    TestEqual(TEXT("Three control-room fixture"), Controls.Num(), 3);
    Systems->DebugSetOccupation(Systems->Slots[Controls[2]].SlotId, 1);
    TestEqual(TEXT("Two controllers provide 450 capacity"), Systems->MaxShield, 450.f);
    OVTest::Hit(Systems, FVector(0, 0, 30), Systems->Shield);
    TestTrue(TEXT("Initial break remains inactive at zero charge"), !Systems->bShieldActive && Systems->Shield == 0);
    Systems->Simulate(180.f / Systems->ShieldRegen);
    TestTrue(TEXT("180/450 recovery remains below reactivation threshold"), !Systems->bShieldActive && FMath::IsNearlyEqual(Systems->Shield, 180.f, .001f));
    Systems->DebugSetOccupation(Systems->Slots[Controls[1]].SlotId, 1);
    TestTrue(TEXT("Capacity loss immediately activates existing 180/300 charge"), Systems->bShieldActive && Systems->MaxShield == 300.f);
    const float Before = Systems->Shield;
    Systems->Simulate(2);
    TestEqual(TEXT("Capacity transition does not grant active combat regeneration"), Systems->Shield, Before);
    Systems->DebugSetOccupation(Systems->Slots[Controls[1]].SlotId, -1);
    TestTrue(TEXT("Capacity restoration neither refills nor deactivates an active shield"), Systems->bShieldActive && Systems->Shield == Before && Systems->MaxShield == 450.f);
    Systems->Simulate(0);
    TestEqual(TEXT("Paused transition does not create charge"), Systems->Shield, Before);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVInvalidRuntimeLayoutTest,"OrbitalVanguard.Systems.RejectInvalidRuntimeLayout",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FOVInvalidRuntimeLayoutTest::RunTest(const FString&)
{
    auto* Systems = OVTest::Make();
    auto* Invalid = UShipLayoutDataAsset::MakeTestLayout(GetTransientPackage(), 3);
    Invalid->ArmorSections.Pop();
    AddExpectedMessagePlain(TEXT("OVLayout rejected:"), ELogVerbosity::Warning);
    Systems->Initialize(Invalid);
    TestNull(TEXT("Invalid layout cannot become active runtime data"), Systems->Layout.Get());
    TestTrue(TEXT("Previous runtime arrays are discarded on rejected initialization"), Systems->Slots.IsEmpty() && Systems->ArmorSections.IsEmpty() && Systems->WeaponMountStates.IsEmpty());
    TestTrue(TEXT("Invalid design has no usable actions or shields"), !Systems->CanMove() && !Systems->CanCommand() && !Systems->bShieldActive && Systems->MaxShield == 0);
    Systems->Simulate(10);
    Systems->ApplyWorldHit(FVector(175, 0, 0), FVector::ForwardVector, 100, EOVDamageCause::External);
    TestEqual(TEXT("Invalid-design simulation and hits remain inert"), Systems->HitEventId, 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVD116ShieldCrewTest,"OrbitalVanguard.Systems.D116NoDelayCrewAndDamage",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FOVD116ShieldCrewTest::RunTest(const FString&)
{
 auto* S=OVTest::Make();const auto Controls=OVTest::Find(S,EOVFacilityType::ShieldControl);
 for(int32 N=1;N<Controls.Num();++N)S->DebugSetOccupation(S->Slots[Controls[N]].SlotId,1);
 const int32 C=Controls[0];const FName Id=S->Slots[C].SlotId;const float Crew=S->Layout->Slots[C].RequiredCrew;
 const float Max=S->MaxShield;
 S->Shield=0;S->bShieldActive=false;S->DebugSetCrew(Id,Crew*.5f);
 TestEqual(TEXT("Half crew preserves nominal maximum"),S->MaxShield,Max);
 S->Tuning.ShieldRecoveryDelay=999;S->Simulate(1);
 TestTrue(TEXT("Crew applied exactly once"),FMath::IsNearlyEqual(S->Shield,Max*.006f*.5f,.001f));
 const float Before=S->Shield;S->NotifyCombatParticipation();S->Simulate(1);
 TestTrue(TEXT("Repeated participation never resets recovery delay"),FMath::IsNearlyEqual(S->Shield-Before,Max*.003f,.001f));
 S->TimeSinceExternalHit=20;S->Simulate(1);
 TestTrue(TEXT("Half crew noncombat rate"),FMath::IsNearlyEqual(S->Shield-Before,Max*(.003f+.009f),.001f));
 S->DebugSetCrew(Id,0);TestTrue(TEXT("Zero crew removes shield"),S->MaxShield==0&&S->Shield==0);
 S->DebugSetCrew(Id,Crew);TestEqual(TEXT("Restoring personnel grants no free charge"),S->Shield,0.f);
 float TotalBefore=0;for(const auto& Slot:S->Slots)TotalBefore+=Slot.Crew;
 S->TimeSinceExternalHit=30;
 FVector CrewHitPoint=FVector::ZeroVector,CrewHitNormal=FVector::UpVector;
 bool FoundCrewFace=false;
 for(const auto& A:S->Layout->ArmorSections) {
  FVector N;
  switch(A.Face) {
   case EOVArmorFace::Front:N=FVector::ForwardVector;break;
   case EOVArmorFace::Rear:N=-FVector::ForwardVector;break;
   case EOVArmorFace::Left:N=-FVector::RightVector;break;
   case EOVArmorFace::Right:N=FVector::RightVector;break;
   case EOVArmorFace::Top:N=FVector::UpVector;break;
   default:N=-FVector::UpVector;break;
  }
  const int32 AI=S->Layout->ResolveArmor(A.LocalCenter,N);
  const int32 SI=S->Layout->MapArmorToSlot(AI,A.LocalCenter);
  if(S->Slots.IsValidIndex(SI)&&S->Slots[SI].Crew>0) {CrewHitPoint=A.LocalCenter;CrewHitNormal=N;FoundCrewFace=true;break;}
 }
 TestTrue(TEXT("Fixture exposes a staffed facility face"),FoundCrewFace);
 S->ApplyWorldHit(CrewHitPoint,CrewHitNormal,10,EOVDamageCause::External,true);
 float TotalAfter=0;for(const auto& Slot:S->Slots)TotalAfter+=Slot.Crew;
 TestTrue(TEXT("Actual combat facility hit removes partial crew"),TotalAfter<TotalBefore&&TotalAfter>0);
 TestEqual(TEXT("Combat hit resets participation clock"),S->TimeSinceExternalHit,0.f);
 const FName Damaged=S->LastDamageSlot;const float DamagedCrew=S->GetSlotState(Damaged).Crew;
 S->DebugRepairSlot(Damaged);TestEqual(TEXT("Physical repair never restores crew"),S->GetSlotState(Damaged).Crew,DamagedCrew);
 S->TimeSinceExternalHit=20;S->ApplyWorldHit(FVector(0,0,30),FVector::UpVector,1,EOVDamageCause::External);
 TestEqual(TEXT("DEV hit does not mark combat participation"),S->TimeSinceExternalHit,20.f);
 return true;
}

#endif
