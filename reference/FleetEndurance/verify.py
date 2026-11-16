import json,hashlib
from pathlib import Path
from run import trial
from model import P,roster,cost
R=Path(__file__).parent;r=json.loads((R/'results.json').read_text())
for filename,key in [('profile.json','profile_sha256'),('model.py','model_sha256'),('run.py','runner_sha256')]:assert hashlib.sha256((R/filename).read_bytes()).hexdigest()==r[key],filename
assert hashlib.sha256((R.parent/'CombatSimulation/engine.py').read_bytes()).hexdigest()==r['indoor_source_sha256']
assert hashlib.sha256((R.parent/'CombatSimulation/profile.json').read_bytes()).hexdigest()==r['indoor_profile_sha256']
assert len(r['trials'])==912
assert len({(x['budget'],x['left'],x['right'],x['policy'],x['seed'],x['legacy'],x['smart']) for x in r['trials']})==912
assert all(x['n']==24 for x in r['summary'].values()) and len(r['summary'])==38
for x in r['trials']:
 assert cost(roster(x['budget'],x['left']))==cost(roster(x['budget'],x['right']))==x['budget']
 for k in ('hero_dead','heroes_alive_end','hero_rescue','heroes_aboard_end','heroes_transit_end'):assert 0<=x.get(k,0)<=4
 for k in ('mining','transport','assault'):assert P['ships'][k]['fleet_cost']==1
for name,n in [('paired_controls.json',72),('sensitivity.json',18),('counter_checks.json',48),('route_checks.json',36)]:
 data=json.loads((R/name).read_text());assert len(data['trials'])==n;assert data['profile_sha256']==r['profile_sha256'];assert data['model_sha256']==r['model_sha256']
defense=json.loads((R/'defense_checks.json').read_text());assert len(defense['trials'])==224;assert defense['profile_sha256']==r['profile_sha256']
for budget,policy,seed in [(100,'none',50000),(200,'weapon',70000),(300,'pod',70000)]:
 old=next(x for x in r['trials'] if x['budget']==budget and x['policy']==policy and x['seed']==seed and not x['legacy'] and not x['smart'] and x['left']==x['right']=='mixed');new=trial((budget,'mixed','mixed',policy,seed,False,False));assert new==old,(budget,policy)
print('PASS: provenance, 912 unique trials/38 groups, exact budgets, auxiliary1310 total and 3 deterministic replays')
