#include "OVBattleNotices.h"
#include "OVCombatSubsystem.h"
#include "OVShip.h"
#include "OVShipSystemsComponent.h"
#include "HAL/PlatformTime.h"

void UOVBattleNotices::Reset()
{
    Notices.Reset();RecentAttackIds.Reset();DroppedCount=0;
}

void UOVBattleNotices::RecordCombatHit(const FOVAttackState& Attack,AOVShip* Victim)
{
    if(Attack.Result!=EOVAttackResult::Hit||Attack.AttackId<=0||RecentAttackIds.Contains(Attack.AttackId)
        ||!IsValid(Victim)||!Victim->Systems||Victim->Systems->FactionId!=0||Victim->GetFName()!=Attack.HitShipId)return;
    RecentAttackIds.Add(Attack.AttackId);
    if(RecentAttackIds.Num()>256)RecentAttackIds.RemoveAt(0);
    const bool bWrecked=Victim->Systems->bWrecked;
    FOVBattleNotice* Last=Notices.IsEmpty()?nullptr:&Notices.Last();
    if(!bWrecked&&Last&&!Last->bShipWrecked&&Last->ShipId==Attack.HitShipId&&Attack.ResolvedAt>=Last->GameTime&&Attack.ResolvedAt-Last->GameTime<=2.0)
    {
        ++Last->Count;Last->GameTime=Attack.ResolvedAt;Last->LastPosition=Attack.Position;Last->DisplayTime=FPlatformTime::Seconds();return;
    }
    FOVBattleNotice& Notice=Notices.AddDefaulted_GetRef();
    Notice.ShipName=Victim->ShipName;Notice.ShipId=Attack.HitShipId;Notice.LastPosition=Attack.Position;
    Notice.GameTime=Attack.ResolvedAt;Notice.DisplayTime=FPlatformTime::Seconds();Notice.Count=1;Notice.bShipWrecked=bWrecked;
    if(Notices.Num()>64){Notices.RemoveAt(0);++DroppedCount;}
}
