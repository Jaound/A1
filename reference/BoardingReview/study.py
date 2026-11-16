"""Independent count/transport/scale diagnostics; preserve historical v0.3 results."""
import json,sys,random,hashlib,statistics,math
from pathlib import Path
from collections import defaultdict
R=Path(__file__).parent
sys.path.insert(0,str(R.parent/'CombatSimulation'))
from engine import HeroBattle
CFG=json.loads((R/'profile.json').read_text())
ROLES={'balanced':['melee','ranged','support','commander'],'front':['melee','melee','ranged','support'],'ranged':['ranged','ranged','support','commander'],'support':['support','support','melee','commander']}
def heroes(n,kind):return [ROLES[kind][i%4] for i in range(n)]
def fight(a,d,seed,kind='balanced',cover=False,attacker_soldiers=8):
 b=HeroBattle(heroes(a,kind)+['soldier']*attacker_soldiers,heroes(d,kind)+['soldier']*8,seed)
 if cover:
  for u in b.units:
   if u.side==1:u.d['armor']=min(.65,u.d['armor']+.10)
 result=b.run(dt=.1)
 return dict(attacker_heroes=a,defender_heroes=d,composition=kind,cover=cover,attacker_soldiers=attacker_soldiers,seed=seed,**result)
def pod_wave(people,pd,seed,selection='focus',distance=250,hp=None):
 rng=random.Random(seed);hp=CFG['study']['pod_hp'] if hp is None else hp;pods=[dict(id=i,role=role,launch=i*.25,arrive=i*.25+distance/165,enter_zone=i*.25+max(0,distance-111)/165,hp=hp,state='flying') for i,role in enumerate(people)]
 events=[]
 for gun in range(pd):
  t=rng.uniform(0,.5)
  while t<=pods[-1]['arrive']+1e-9:events.append((t,gun));t+=.5
 events.sort()
 for t,gun in events:
  candidates=[p for p in pods if p['hp']>0 and p['enter_zone']<=t<p['arrive']]
  if not candidates:continue
  p=min(candidates,key=lambda x:(x['arrive'],x['id'])) if selection=='focus' else rng.choice(candidates)
  if rng.random()<.72*(1-.40):p['hp']-=12
 for p in pods:p['state']='arrived' if p['hp']>0 else 'hero_rescue' if p['role']!='soldier' else 'soldier_dead'
 return dict(seed=seed,pd=pd,selection=selection,count=len(people),soldiers=people.count('soldier'),arrived=sum(p['state']=='arrived' for p in pods),hero_rescue=sum(p['state']=='hero_rescue' for p in pods),soldier_dead=sum(p['state']=='soldier_dead' for p in pods),pods=pods)
def assault_transit(dps,seed,pd=12):
 # PD cannot target this small ship. Counterpart anti-ship fire is an explicit stress input.
 rng=random.Random(seed);ship=CFG['ships']['assault'];hp=float(ship['hp']);shield=float(ship['shield']);armor=ship['armor_cell']*14.;time=(700/ship['speed'])+3;shots=[]
 for period,share in ((.9,.6),(1.8,.4)):
  t=rng.uniform(0,period)
  while t<time:shots.append((t,dps*share*period));t+=period
 for t,damage in sorted(shots):
  if rng.random()>.86:continue
  spill=max(0,damage-shield);was_shield=shield>0;shield=max(0,shield-damage);damage=spill*.35 if was_shield else damage
  reduction=.45*max(0,armor/(ship['armor_cell']*14));armor=max(0,armor-damage*.45);hp-=damage*(1-reduction)
  if hp<=0:break
 return dict(seed=seed,nominal_enemy_dps=dps,pd=pd,docking_seconds=time,arrived=hp>0,hp=max(0,hp),shield=shield)
def aggregate(rows):
 groups=defaultdict(list)
 for x in rows:groups[(x['attacker_heroes'],x['defender_heroes'],x['cover'])].append(x)
 out=[]
 for (a,d,c),rr in sorted(groups.items()):
  out.append(dict(attacker_heroes=a,defender_heroes=d,cover=c,n=len(rr),attacker_wins=sum(x['winner']==0 for x in rr),defender_wins=sum(x['winner']==1 for x in rr),mutual=sum(x['winner']==-1 for x in rr),timeouts=sum(x['winner'] is None for x in rr),median_seconds=statistics.median(x['seconds'] for x in rr)))
 return out
if __name__=='__main__':
 grid=[fight(a,d,210000+s,k,c) for a in (1,2,4) for d in (0,1,2,4,6) for c in (False,True) for k in ROLES for s in range(32)]
 biased=[fight(4,d,220000+s,attacker_soldiers=0) for d in (0,1,2,4,6) for s in range(64)]
 waves=[pod_wave(people,pd,230000+s,selection) for people in (['soldier']*10,heroes(4,'balanced')+['soldier']*6,heroes(4,'balanced')) for pd in (0,3,6,12,24) for selection in ('focus','spread') for s in range(32)]
 transit=[assault_transit(dps,240000+s,pd) for dps in (0,500,1000,2000) for pd in (0,24) for s in range(64)]
 source={name:hashlib.sha256((R.parent/'CombatSimulation'/name).read_bytes()).hexdigest() for name in ('engine.py','profile.json')}
 out=dict(profile_sha256=hashlib.sha256((R/'profile.json').read_bytes()).hexdigest(),source=source,study_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),seed_ranges={'grid':210000,'old_conditions':220000,'pods':230000,'ship':240000},grid_summary=aggregate(grid),old_conditions_summary=aggregate(biased),grid=grid,old_conditions=biased,pods=waves,transit=transit)
 (R/'results.json').write_text(json.dumps(out,ensure_ascii=False,indent=2)+'\n');print('Completed:',len(grid),len(biased),len(waves),len(transit))
