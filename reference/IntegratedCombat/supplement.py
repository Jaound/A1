"""Deployment-budget and one-sided weapon-disable orders, separate matched seeds."""
import hashlib
import json
from concurrent.futures import ProcessPoolExecutor
from pathlib import Path
from model import Battle,P,roster

R=Path(__file__).parent


class DirectedBattle(Battle):
    def pick(self,attacker):
        if attacker.side!=0:return super().pick(attacker)
        original=self.knowledge[0]
        self.knowledge[0]={k:v for k,v in original.items() if v['observed_mounts']}
        try:return super().pick(attacker)
        finally:self.knowledge[0]=original

    def fire(self,a,v):
        original=self.aim;self.aim='weapons' if a.side==0 else 'hull'
        if a.side==0 and v is not None and not self.knowledge[0].get(v.uid,{}).get('observed_mounts'):
            a.target=None;self.aim=original;return
        try:return super().fire(a,v)
        finally:self.aim=original


def task(arg):
    name,options,seed=arg
    cls=DirectedBattle if name=='left_weapon_order' else Battle
    battle=cls(roster(100),roster(100),seed,**options);result=battle.run()
    result['case']=name
    result['enemy_weapon_disabled']=not any(s.combat_ready() for s in battle.living(1))
    return result


if __name__=='__main__':
    cases={f'active_{n}':{'hero_budget':n,'mode':'assault','level':10} for n in (6,10,15)}
    cases['left_weapon_order']={'level':10};cases['left_hull_order']={'level':10}
    args=[(name,options,930000+i) for name,options in cases.items() for i in range(8)]
    rows=[]
    with ProcessPoolExecutor(max_workers=1) as pool:
        for i,row in enumerate(pool.map(task,args),1):rows.append(row);print('supplement',i,'/',len(args),flush=True)
    payload={'version':P['version'],'cases':cases,'trials':rows,'hashes':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in (R/'model.py',R/'indoor.py',R/'profile.json',R/'supplement.py')}}
    (R/'supplement_results.json').write_text(json.dumps(payload,ensure_ascii=False,indent=2)+'\n')
