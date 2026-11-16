"""Isolated indoor defense sensitivity; not coupled fleet defender deployment."""
import sys,json,hashlib,statistics
from pathlib import Path
R=Path(__file__).parent;sys.path.insert(0,str(R.parent/'CombatSimulation'))
from engine import HeroBattle
if __name__=='__main__':
 rows=[]
 for role in ('none','melee','ranged','support','commander','two_front_guards','two_support_guards'):
  for seed in range(101000,101032):
   defenders=['soldier']*12+(['melee','ranged'] if role=='two_front_guards' else ['support','commander']) if role.startswith('two_') else ['soldier']*8+([] if role=='none' else [role])
   result=HeroBattle(['melee','ranged','support','commander'],defenders,seed).run()
   rows.append(dict(defender=role,seed=seed,**result))
 summary={}
 for role in ('none','melee','ranged','support','commander','two_front_guards','two_support_guards'):
  rr=[r for r in rows if r['defender']==role];summary[role]={'n':32,'attacker_wins':sum(x['winner']==0 for x in rr),'median_seconds':statistics.median(x['seconds'] for x in rr),'mean_attacker_survivors':statistics.mean(x['survivors'][0] for x in rr)}
 (R/'defense_checks.json').write_text(json.dumps({'profile_sha256':hashlib.sha256((R/'profile.json').read_bytes()).hexdigest(),'indoor_engine_sha256':hashlib.sha256((R.parent/'CombatSimulation/engine.py').read_bytes()).hexdigest(),'indoor_profile_sha256':hashlib.sha256((R.parent/'CombatSimulation/profile.json').read_bytes()).hexdigest(),'scope':'Indoor-only; base coupled fleet trials do not deploy defender heroes','summary':summary,'trials':rows},ensure_ascii=False,indent=2)+'\n');print(summary)
