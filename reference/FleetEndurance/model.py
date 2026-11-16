"""Cost-scale sustained-fire model; statistical range/tracking and aggregate PD, not XYZ physics."""
import json,random,math,copy
from pathlib import Path
from collections import Counter,defaultdict
P=json.loads((Path(__file__).parent/'profile.json').read_text());S=P['study']
OLD=json.loads((Path(__file__).parent.parent/'CombatSimulation/profile.json').read_text())
COMBAT=['corvette','frigate','vector','bastion','cruiser','air_defense','radar','battleship','carrier','missile_ship','dreadnought','battlecarrier']
def cost(roster):return sum(P['ships'][k]['fleet_cost'] for k in roster)
def roster(budget,kind):
 if kind=='mixed':
  if budget==100:r=['battleship','carrier']+['vector','bastion','cruiser','cruiser','air_defense','radar']+['corvette']*10+['frigate']*8+['repair','assault']
  elif budget==200:r=['battleship']*2+['carrier','missile_ship']+(['vector','bastion','cruiser','air_defense','radar']*3)+['cruiser']+['corvette']*10+['frigate']*8+['repair','assault']
  else:r=['dreadnought','battleship','battleship','carrier','missile_ship']+['vector','bastion','cruiser','air_defense','radar']*2+['corvette']*13+['frigate']*10+['repair','assault']
 elif kind=='xl':
  if budget==100:return None
  r=['battlecarrier' if budget==200 else 'dreadnought']*(budget//125);remaining=budget-cost(r);r+=['battleship']*(remaining//25);r+=['corvette']*(remaining%25)
 elif kind=='heavy':r=['battleship']*(budget//25)
 elif kind=='light':r=['corvette','frigate']*(budget//2)
 elif kind=='missile':r=['missile_ship']*(budget//25)
 elif kind=='carrier':r=['carrier']*(budget//25)
 elif kind=='logistics':
  r=roster(budget,'mixed');r[r.index('corvette')]='mining';r[r.index('frigate')]='transport'
 elif kind=='no_pd':
  r=roster(budget,'mixed');r=['cruiser' if x=='air_defense' else x for x in r]
 else:raise ValueError(kind)
 assert cost(r)==budget,(budget,kind,cost(r));return r
class Ship:
 def __init__(self,kind,side,idx,rng,legacy=False):
  self.kind=kind;self.side=side;self.idx=idx;self.d=P['ships'][kind];self.size=self.d['size'];self.maxhp=OLD['ships'][kind]['hp'] if legacy and kind in OLD['ships'] else self.d['hp'];self.hp=self.maxhp;self.maxshield=OLD['ships'][kind]['shield'] if legacy and kind in OLD['ships'] else self.d['shield'];self.shield=self.maxshield;self.active=True;self.delay=0;self.dead=False;self.armor=1.;self.target=None;self.retarget=0;self.disabled_until=0;self.owner_order=0;self.repair_pool=S['repair_resource_hp'];self.resources=0;self.fighters=self.d.get('fighters',0);self.fighter_damage=0.;self.fighter_incoming=0.;self.guns=[]
  for w in self.d['weapons']:self.guns.append({**w,'next':rng.uniform(0,P['weapons'][w['kind']]['period'])})
 def alive(self):return not self.dead
 def hit(self,damage,t,penetration=1):
  if self.dead:return
  if self.active:
   spill=max(0,damage-self.shield);self.shield=max(0,self.shield-damage)
   if self.shield<=0:self.active=False;self.delay=t+6.5
   damage=spill*.35
  if damage>0:
   damage*=penetration
   # Distributed exterior hits: armor/hull aggregate, not single front-bridge proxy.
   dr=.45*self.armor;self.armor=max(0,self.armor-damage/(self.maxhp*.35));self.hp=max(0,self.hp-damage*(1-dr)*.85)
   if self.hp==0:self.dead=True;self.fighters=0
class Battle:
 def __init__(self,left,right,seed=0,policy='none',legacy=False,smart=False,reverse=False):
  self.rng=random.Random(seed);self.board_rng=random.Random(seed+1000000);self.retakes=[];self.ships=[];self.t=0;self.events=[];self.policy=policy;self.smart=smart;self.legacy=legacy;self.first_loss=None;self.main_end=None;self.board=None;self.next_board=S['boarding_start'];self.metric=defaultdict(float);self.damage_by_size=defaultdict(float);self.incoming_by_size=defaultdict(float);self.initial=[cost(left),cost(right)];self.pending=[];self.reverse=reverse
  for side,rs in enumerate((left,right)):
   for kind in rs:self.ships.append(Ship(kind,side,len(self.ships),self.rng,legacy))
 def living(self,side):return [s for s in self.ships if s.side==side and s.alive()]
 def pd(self,side):return sum(s.d['pd']*s.d['pd_range_scale'] for s in self.living(side))*S['pd_coverage']
 def distance(self,t):return max(S['near_range'],S['opening_range']-t*5)
 def pick(self,a,enemies,loads):
  # Soft proximity/threat weighting, never a forced taunt. Smart AI can bypass the screen.
  choices=[];weights=[]
  for v in enemies:
   if loads[v.idx]>=S['focus_cap'][str(v.size)]:continue
   w=(4.5 if v.size==1 and v.kind in ('corvette','frigate') else 2 if v.size==2 else 1)
   if self.smart:w=(2.5 if v.kind in ('carrier','missile_ship','repair','radar') else 1)/max(.5,v.size**.25)
   if a.size>=3 and v.size==1:w*=.35
   choices.append(v);weights.append(w)
  if not choices:choices=enemies;weights=[1]*len(choices)
  return self.rng.choices(choices,weights)[0]
 def step(self,dt=1):
  self.t+=dt;t=self.t
  for time,v,side in self.retakes[:]:
   if t>=time:
    if not v.dead:v.side=side;v.target=None;v.owner_order=t+2
    self.retakes.remove((time,v,side))
  allies=[self.living(0),self.living(1)];pd_left=[self.pd(i)*12/.5*dt for i in (0,1)];loads=Counter()
  for a in self.ships:
   if a.target is not None and not a.dead and not a.target.dead and a.target.side!=a.side:loads[a.target.idx]+=1
  attacks=[]
  has_radar=[any(x.kind=='radar' for x in group) for group in allies]
  fighter_total=[sum(a.fighters for a in allies[i]) for i in (0,1)]
  order=list(self.ships)
  if self.reverse:order.reverse()
  for a in order:
   if a.dead:continue
   if not a.active and t>=a.delay:
    a.shield=min(a.maxshield*.5,a.shield+a.maxshield*S['shield_regen_fraction']*dt)
    if a.shield>=a.maxshield*.5:a.active=True
   es=allies[1-a.side]
   if not es:continue
   if a.target is None or a.target.dead or a.target.side==a.side or t>=a.retarget:
    if a.target is not None:loads[a.target.idx]-=1
    a.target=self.pick(a,es,loads);loads[a.target.idx]+=1;a.retarget=t+S['target_switch_seconds']+self.rng.uniform(-2,2)
   v=a.target
   if a.kind=='repair' and a.repair_pool>0:
    patient=min(allies[a.side],key=lambda x:x.hp/x.maxhp)
    restored=min(S['repair_hp_per_second']*dt if 'repair_hp_per_second' in S else 24*dt,patient.maxhp-patient.hp,a.repair_pool);patient.hp+=restored;a.repair_pool-=restored;a.resources+=restored*.6;self.metric['repair_hp']+=restored
   if a.disabled_until>t or a.owner_order>t:continue
   for w in a.guns:
    spec=P['weapons'][w['kind']]
    if t+1e-8<w['next'] or self.distance(t)>spec['range']:continue
    # absolute schedule avoids a full-dt cooldown drift
    w['next']+=spec['period']
    if w['next']<t:w['next']=t+spec['period']
    self.metric['shots']+=spec['salvo'];kind=w['kind'];tracking=(S['capital_small_tracking'] if a.size>=3 and v.size==1 else S['capital_medium_tracking'] if a.size>=3 and v.size==2 else .82)
    if 'beam' in kind:hit=.90
    else:hit=tracking
    if has_radar[a.side]:hit=min(.98,hit+.08)
    for _ in range(spec['salvo']):
     raw=spec['damage']*P['tier_damage'][str(w['tier'])]
     delay=0 if 'beam' in kind else self.distance(t)/spec['speed']
     self.pending.append([t+delay,v,raw,kind,a.side,a.size,hit])
   if a.fighters:
    # PD competes with missiles; casualties are permanent, no automatic respawn.
    work=min(pd_left[1-a.side],S['local_pd_cap']*12/.5*dt)*.3;pd_left[1-a.side]-=work;a.fighter_incoming+=work*.72*(1-.62)
    if fighter_total[a.side] and fighter_total[1-a.side]:a.fighter_incoming+=fighter_total[1-a.side]*S['fighter_anti_fighter_dps']*dt*a.fighters/fighter_total[a.side]
    exposure=min(a.fighters,int(a.fighter_incoming/S['fighter_hp']));a.fighter_incoming-=exposure*S['fighter_hp'];a.fighters-=exposure;self.metric['fighter_losses']+=exposure
    a.fighter_damage+=a.fighters*S['fighter_damage']/S['fighter_period']*.72*dt*(.5 if fighter_total[1-a.side] else 1)
    if a.fighter_damage>=1:
     attacks.append((v,a.fighter_damage,a.side,a.size));a.fighter_damage=0
  keep=[]
  for due,v,d,kind,side,size,hit in self.pending:
   if due>t:keep.append([due,v,d,kind,side,size,hit]);continue
   if v.dead:continue
   if kind in ('missile','guided'):
    # Expected hit work with Bernoulli kill; no second target evasion roll.
    work=min(24,pd_left[v.side]*.54);pd_left[v.side]-=work/.54
    if self.rng.random()<work/24:self.metric['missile_intercepts']+=1;continue
   if self.rng.random()<hit:attacks.append((v,d,side,size))
  self.pending=keep
  for v,d,side,size in attacks:
   before=v.hp+v.shield;v.hit(d,t,S['armor_penetration'].get(f'{min(size,3)}:{v.size}',1));applied=max(0,before-v.hp-v.shield);self.damage_by_size[(side,size)]+=applied;self.incoming_by_size[(v.side,v.size)]+=applied
  for a in self.ships:
   if a.dead and self.first_loss is None:self.first_loss=t
  if self.main_end is None and any(sum(x.d['fleet_cost'] for x in self.living(side))<=self.initial[side]*S['main_battle_remaining_fraction'] for side in (0,1)):self.main_end=t
  self.board_step(dt,pd_left)
 def board_step(self,dt,pd_left):
  if self.policy=='none':return
  if self.board is None and self.t>=self.next_board:
   enemies=[s for s in self.living(1) if not s.active and s.size>=2 and s.disabled_until<=self.t]
   transports=[s for s in self.living(0) if s.kind=='assault']
   if enemies and transports:
    target=max(enemies,key=lambda s:(s.kind in ('carrier','missile_ship','battleship','dreadnought','battlecarrier'),s.hp));self.board={'phase':'pod' if self.policy=='pod' else 'approach','target':target,'ship':transports[0],'remaining':250/S['pod_speed'] if self.policy=='pod' else S['approach_start'],'heroes':4,'hp':S['assault_hull'],'shield':S['assault_shield'],'pods':[S['pod_hp']]*4 if self.policy=='pod' else None,'goal':False,'phase_start':self.t};self.metric['boarding_launched']+=1;self.metric['launch_at']=self.t
  b=self.board
  if b is None:return
  target=b['target']
  if target.dead:
   if b['phase'] in ('indoor','return_walk'):self.metric['hero_dead']+=b['heroes'];self.metric['target_wreck_while_aboard']+=1
   else:self.metric['hero_rescue']+=b['heroes']
   self.board=None;self.next_board=1e9;return
  if b['phase']=='approach':
   if target.active:self.metric['shield_wait_seconds']+=dt
   local=min(pd_left[1],S['local_pd_cap']*12/.5*dt);pdpower=local*.72*(1-S['assault_evasion'])
   pd_left[1]=0
   incoming=pdpower # already a per-step budget
   b['ship'].hit(incoming,self.t)
   if not target.active:b['remaining']-=S['assault_speed']*dt
   if b['ship'].dead:self.metric['hero_rescue']+=4;self.metric['approach_lost']+=1;self.board=None;self.next_board=1e9;return
   if b['remaining']<=S['assault_release_range']:
    b['phase']='pod';b['remaining']=max(0,b['remaining'])/S['pod_speed'];b['pods']=[S['pod_hp']]*4
  elif b['phase']=='pod':
   if target.active:self.metric['hero_rescue']+=4;self.board=None;self.next_board=1e9;return
   damage=min(pd_left[1],S['local_pd_cap']*12/.5*dt)*.72*(1-.4)*min(1,b['remaining']/dt);pd_left[1]=0
   for i in range(4):
    used=min(max(0,b['pods'][i]),damage);b['pods'][i]-=used;damage-=used
   b['remaining']-=dt
   if b['remaining']<=0:
    b['heroes']=sum(x>0 for x in b['pods']);self.metric['hero_rescue']+=4-b['heroes']
    if not b['heroes']:self.board=None;self.next_board=1e9;return
    self.metric['heroes_entered']+=b['heroes'];self.metric['entry_at']=self.t
    # Indoor duration/losses sampled from precomputed HeroBattle trials, not instant-win capture.
    sample=self.board_rng.choice(INDOOR[b['heroes']]);b['surviving']=sample['survivors'][0] if sample['winner']==0 else 0;b['remaining']=sample['seconds']+self.board_rng.uniform(10,25)+(36 if self.policy=='capture' else 18)/min(2,1.2*(1+.12*(b['heroes']-1)));b['phase']='indoor'
  elif b['phase']=='indoor':
   b['remaining']-=dt
   if b['remaining']<=0:
    self.metric['hero_dead']+=b['heroes']-b['surviving'];b['heroes']=b['surviving']
    if not b['heroes']:self.board=None;self.next_board=1e9;return
    b['goal']=True;target.disabled_until=self.t+S['retake_seconds'];self.metric['objective']+=1;self.metric['objective_at']=self.t
    if self.policy=='capture':self.retakes.append((self.t+S['retake_seconds'],target,1));target.side=0;target.owner_order=self.t+S['post_capture_command_seconds'];target.target=None;target.disabled_until=0
    b['phase']='return_walk';b['remaining']=self.board_rng.uniform(10,25)
  elif b['phase']=='return_walk':
   b['remaining']-=dt
   if b['remaining']<=0:b['phase']='return_flight';b['remaining']=S['approach_start']/S['pod_speed'];b['exposure']=111/S['pod_speed'];b['return_pods']=[S['pod_hp']]*b['heroes']
  elif b['phase']=='return_flight':
   exposure=min(dt,b['exposure']);b['exposure']-=exposure
   damage=min(pd_left[1],S['local_pd_cap']*12/.5*dt)*.72*.6*exposure/dt;pd_left[1]=0;b['remaining']-=dt
   for i in range(len(b['return_pods'])):
    used=min(max(0,b['return_pods'][i]),damage);b['return_pods'][i]-=used;damage-=used
   alive=sum(hp>0 for hp in b['return_pods']);self.metric['hero_rescue']+=b['heroes']-alive;b['heroes']=alive
   if b['remaining']<=0:
    # End operation on arrival; full battle-end survival is separately checked via home reference.
    self.metric['heroes_home_arrival']+=b['heroes'];self.home=(max(self.living(0),key=lambda s:s.hp,default=b['ship']),b['heroes']);self.board=None;self.next_board=1e9;self.metric['return_at']=self.t
 def run(self,limit=None):
  limit=limit or S['limit'];self.home=None
  while self.t<limit and all(self.living(i) for i in (0,1)):self.step(S['dt'])
  alive=[bool(self.living(i)) for i in (0,1)];winner=None if all(alive) else 0 if alive[0] else 1 if alive[1] else -1
  if self.home:
   home,n=self.home
   if home.dead:self.metric['hero_dead']+=n
   elif home.side!=0:self.metric['heroes_captive_or_hostile_aboard_end']+=n
   else:self.metric['heroes_alive_end']+=n
  if self.board:
   b=self.board
   if b['phase'] in ('indoor','return_walk'):self.metric['heroes_aboard_end']=b['heroes']
   else:self.metric['heroes_transit_end']=b['heroes']
  return {'winner':winner,'seconds':self.t,'first_loss':self.first_loss,'main_end':self.main_end,'remaining_cost':[sum(s.d['fleet_cost'] for s in self.living(i)) for i in (0,1)],'damage_by_size':{f'{a}:{b}':round(v,2) for (a,b),v in self.damage_by_size.items()},'incoming_by_size':{f'{a}:{b}':round(v,2) for (a,b),v in self.incoming_by_size.items()},**dict(self.metric)}
INDOOR={}
def load_indoor():
 import sys
 sys.path.insert(0,str(Path(__file__).parent.parent/'CombatSimulation'))
 from engine import HeroBattle
 for n in (1,2,3,4):
  INDOOR[n]=[HeroBattle(['melee','ranged','support','commander'][:n],['soldier']*8,seed=800+i).run() for i in range(32)]
load_indoor()
