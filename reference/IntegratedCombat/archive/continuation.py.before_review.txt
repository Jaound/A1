"""Two consecutive encounters with persistent damage, people, charges and cargo."""
import hashlib
import json
from collections import Counter
from pathlib import Path
from model import Battle, P, roster, cost

R=Path(__file__).parent


def trial(seed,repair=True):
    left=roster(100)
    if not repair:left[left.index('repair')]='frigate'
    first=Battle(left,roster(75),seed,mode='assault',level=10)
    one=first.run()
    survivors=[s for s in first.ships if s.side==0 and not s.dead]
    if not survivors:return {'seed':seed,'repair':repair,'first':one,'second':None,'reason':'no_surviving_fleet'}
    second=Battle([],roster(100),seed+200000,heroes=False)
    # Subsequent enemy faction/mission; same persistent player assets, no free resupply.
    for s in second.ships:
        s.uid+=10000
        for u in s.units:u.uid+=100000
        for w in s.guns:w['next']+=first.t
    before=[{'id':s.uid,'hp':s.hp,'shield':s.shield,'crew':{k:f['crew'] for k,f in s.facilities.items()},
             'cargo':list(s.cargo),'fighters':s.fighters,
             'heroes':[{'id':u.uid,'hp':u.hp,'medkits':u.medkits,'tools':u.tools,'cooldowns':dict(u.cd)} for u in s.units+s.officers if u.hero and u.hp>0]}
            for s in survivors]
    for s in survivors:
        # New travel/order, not a repair/consumable/health reset.
        s.escaped=False;s.ftl=None;s.target=None
        s.fighter_return=False;s.fighter_return_at=None;s.fighter_docked=0
        s.pos[0]=-425;s.pos[1]*=.5
    second.ships=survivors+second.ships;second._live_cache=None;second.t=first.t;second.duration=first.t+1800
    second.unit_seq=200000;second.initial=[sum(s.d['fleet_cost'] for s in survivors),100]
    second.initial_heroes=[sum(u.hero and u.hp>0 for s in survivors for u in s.units+s.officers),0]
    second.initial_fighters=[sum(s.fighters for s in survivors),0];second.fighter_package_cost=[[0,0,0],[0,0,0]]
    two=second.run();two['encounter_seconds']=two['seconds']-first.t
    return {'seed':seed,'repair':repair,'first':one,'second':two,'persistent_start':before}


if __name__=='__main__':
    rows=[]
    for repair in (False,True):
        for i in range(8):
            rows.append(trial(920000+i,repair));print('continuation',repair,i+1,flush=True)
    data={'version':P['version'],'hashes':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in (R/'model.py',R/'indoor.py',R/'profile.json',R/'continuation.py')},'trials':rows}
    (R/'continuation_results.json').write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n')
