#include "OVBattleNotices.h"
#include "OVCombatSubsystem.h"
#include "OVShip.h"
#include "OVShipSystemsComponent.h"
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

void UOVBattleNotices::Append(const FOVAttackState& Attack,AOVShip* Victim,EOVBattleNoticeKind Kind,int32 Count)
{
    const bool bWrecked=Kind==EOVBattleNoticeKind::ShipWrecked;
    FOVBattleNotice* Last=Notices.IsEmpty()?nullptr:&Notices.Last();
    if(Kind==EOVBattleNoticeKind::Hit&&Last&&Last->Kind==Kind&&Last->ShipId==Attack.HitShipId&&Attack.ResolvedAt>=Last->GameTime&&Attack.ResolvedAt-Last->GameTime<=2.0)
    {
        ++Last->Count;Last->GameTime=Attack.ResolvedAt;Last->LastPosition=Attack.Position;Last->DisplayTime=FPlatformTime::Seconds();return;
    }
    FOVBattleNotice& Notice=Notices.AddDefaulted_GetRef();
    Notice.ShipName=Victim->ShipName;Notice.ShipId=Attack.HitShipId;Notice.LastPosition=Attack.Position;
    Notice.GameTime=Attack.ResolvedAt;Notice.DisplayTime=FPlatformTime::Seconds();Notice.Count=Count;Notice.bShipWrecked=bWrecked;Notice.Kind=Kind;
    Notice.bCritical=bWrecked||(Kind==EOVBattleNoticeKind::FacilityDestroyed&&Attack.CriticalFacilitiesDestroyed>0);
    if(Notice.bCritical)LastCriticalDisplayTime=Notice.DisplayTime;
    if(Notices.Num()>64){Notices.RemoveAt(0);++DroppedCount;}
}
