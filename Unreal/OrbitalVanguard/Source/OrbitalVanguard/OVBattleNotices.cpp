#include "OVBattleNotices.h"
#include "OVCombatSubsystem.h"
#include "OVShip.h"
#include "OVShipSystemsComponent.h"
#include "OVInteriorSubsystem.h"
#include "HAL/PlatformTime.h"

void UOVBattleNotices::Reset()
{
    Notices.Reset();RecentAttackIds.Reset();DroppedCount=0;LastCriticalDisplayTime=-1000;
}

void UOVBattleNotices::RecordCombatHit(const FOVAttackState& Attack,AOVShip* Victim)
{
    if(Attack.Result!=EOVAttackResult::Hit||Attack.AttackId<=0||RecentAttackIds.Contains(Attack.AttackId)
        ||!IsValid(Victim)||!Victim->Systems||Victim->Systems->FactionId!=0||Victim->GetFName()!=Attack.HitShipId)return;
    RecentAttackIds.Add(Attack.AttackId);
    if(RecentAttackIds.Num()>256)RecentAttackIds.RemoveAt(0);
    if(Victim->Systems->bWrecked){Append(Attack,Victim,EOVBattleNoticeKind::ShipWrecked,1);return;}
    Append(Attack,Victim,EOVBattleNoticeKind::Hit,1);
    if(Attack.bShieldDepleted)Append(Attack,Victim,EOVBattleNoticeKind::ShieldDepleted,1);
    if(Attack.ArmorSectionsDestroyed>0)Append(Attack,Victim,EOVBattleNoticeKind::ArmorDestroyed,Attack.ArmorSectionsDestroyed);
    if(Attack.FacilitiesDestroyed>0)Append(Attack,Victim,EOVBattleNoticeKind::FacilityDestroyed,Attack.FacilitiesDestroyed);
}

void UOVBattleNotices::RecordShieldRestored(AOVShip* Ship,double GameTime)
{
    if(!IsValid(Ship)||!Ship->Systems||Ship->Systems->FactionId!=0||Ship->Systems->bWrecked)return;
    FOVAttackState Event;Event.HitShipId=Ship->GetFName();Event.Position=Ship->GetActorLocation();Event.ResolvedAt=GameTime;
    Append(Event,Ship,EOVBattleNoticeKind::ShieldRestored,1);
}

void UOVBattleNotices::RecordInteriorEvent(FName EventKind,const FOVInteriorUnitRecord& Unit,double GameTime)
{
    AOVShip* Ship=Unit.Ship.Get();
    if(!IsValid(Ship)||!Ship->Systems)return;
    EOVBattleNoticeKind Kind;
    if(EventKind==TEXT("Entry")&&Unit.FactionId!=0&&Ship->Systems->FactionId==0)Kind=EOVBattleNoticeKind::HostileBoarding;
    else if(Unit.FactionId!=0)return;
    else if(EventKind==TEXT("Death")&&Unit.Role!=EOVInteriorRole::Trooper)Kind=EOVBattleNoticeKind::HeroDeath;
    else if(EventKind==TEXT("Damage")&&Unit.Role!=EOVInteriorRole::Trooper)Kind=EOVBattleNoticeKind::HeroHit;
    else if(EventKind==TEXT("Entry"))Kind=EOVBattleNoticeKind::BoardingEntry;
    else if(EventKind==TEXT("Return"))Kind=EOVBattleNoticeKind::BoardingReturn;
    else if(EventKind==TEXT("Capture"))Kind=EOVBattleNoticeKind::FacilityCaptured;
    else return;
    FOVAttackState Event;Event.HitShipId=Ship->GetFName();Event.Position=Ship->GetActorTransform().TransformPosition(Unit.LocalPosition);Event.ResolvedAt=GameTime;
    Append(Event,Ship,Kind,1,Unit.UnitId);
}

void UOVBattleNotices::Append(const FOVAttackState& Attack,AOVShip* Victim,EOVBattleNoticeKind Kind,int32 Count,int32 UnitId)
{
    const bool bWrecked=Kind==EOVBattleNoticeKind::ShipWrecked;
    FOVBattleNotice* Last=Notices.IsEmpty()?nullptr:&Notices.Last();
    if((Kind==EOVBattleNoticeKind::Hit||Kind==EOVBattleNoticeKind::HeroHit)&&Last&&Last->Kind==Kind&&Last->ShipId==Attack.HitShipId&&Last->UnitId==UnitId&&Attack.ResolvedAt>=Last->GameTime&&Attack.ResolvedAt-Last->GameTime<=2.0)
    {
        ++Last->Count;Last->GameTime=Attack.ResolvedAt;Last->LastPosition=Attack.Position;Last->DisplayTime=FPlatformTime::Seconds();return;
    }
    FOVBattleNotice& Notice=Notices.AddDefaulted_GetRef();
    Notice.ShipName=Victim->ShipName;Notice.ShipId=Attack.HitShipId;Notice.LastPosition=Attack.Position;
    Notice.GameTime=Attack.ResolvedAt;Notice.DisplayTime=FPlatformTime::Seconds();Notice.Count=Count;Notice.UnitId=UnitId;Notice.bShipWrecked=bWrecked;Notice.Kind=Kind;
    Notice.bCritical=bWrecked||Kind==EOVBattleNoticeKind::HeroDeath||Kind==EOVBattleNoticeKind::HostileBoarding||(Kind==EOVBattleNoticeKind::FacilityDestroyed&&Attack.CriticalFacilitiesDestroyed>0);
    if(Notice.bCritical)LastCriticalDisplayTime=Notice.DisplayTime;
    if(Notices.Num()>64){Notices.RemoveAt(0);++DroppedCount;}
}
