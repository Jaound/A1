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
 S->Tuning.NoHitRecoveryInterval=100;S->Simulate(6);TestEqual(TEXT("Recovery delay"),S->Shield,0.f);
 S->Simulate(1);TestTrue(TEXT("Only post-delay half second"),FMath::IsNearlyEqual(S->Shield,Max*.03f*.5f,.001f));
 S->Simulate(16.166667f);TestTrue(TEXT("50 percent auto reactivate"),S->bShieldActive&&FMath::IsNearlyEqual(S->Shield,Max*.5f,.01f));
 const float Half=S->Shield;S->Simulate(2);TestEqual(TEXT("Active combat no regen"),S->Shield,Half);
 S->Tuning.NoHitRecoveryInterval=15;S->Simulate(1);TestTrue(TEXT("Provisional gradual out of combat"),S->Shield>Half&&S->Shield<Max);
 const auto Controls=OVTest::Find(S,EOVFacilityType::ShieldControl);
 for(int32 I:Controls)S->DebugSetOccupation(S->Slots[I].SlotId,1);
 TestTrue(TEXT("All controls unavailable immediately disappear"),S->Shield==0&&S->MaxShield==0&&!S->bShieldActive);
 S->Simulate(100);TestEqual(TEXT("No controls no recovery"),S->Shield,0.f);
 S->DebugSetOccupation(S->Slots[Controls[0]].SlotId,-1);TestEqual(TEXT("Restore control never gifts charge"),S->Shield,0.f);
 float Results[3];for(int32 Scale=1;Scale<=3;++Scale){auto* T=OVTest::Make();OVTest::Hit(T,Point,T->MaxShield);for(int32 I=0;I<600/Scale;++I)T->Simulate(Scale/60.f);Results[Scale-1]=T->Shield;}
 TestTrue(TEXT("Same game time across speeds"),FMath::IsNearlyEqual(Results[0],Results[1],.01f)&&FMath::IsNearlyEqual(Results[0],Results[2],.01f));return true;
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
#endif
