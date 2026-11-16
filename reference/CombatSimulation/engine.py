"""Reduced 3D ballistic/compartment and 1D indoor-combat model. Not UE gameplay."""
from __future__ import annotations
import json,math,random,copy
from pathlib import Path
P=json.loads((Path(__file__).parent/'profile.json').read_text())
def add(a,b):return tuple(x+y for x,y in zip(a,b))
def sub(a,b):return tuple(x-y for x,y in zip(a,b))
def mul(a,k):return tuple(x*k for x in a)
def dot(a,b):return sum(x*y for x,y in zip(a,b))
def norm(a):return math.sqrt(dot(a,a))
def unit(a):return mul(a,1/max(1e-10,norm(a)))
def cross(a,b):return (a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0])
def clamp(x,a,b):return max(a,min(b,x))
def turn(a,b,degrees):
 a,b=unit(a),unit(b);angle=math.acos(clamp(dot(a,b),-1,1));limit=math.radians(degrees)
 if angle<=limit:return b
 if angle>math.pi-.001:
  tangent=unit(cross(a,(0,0,1)))
  if norm(tangent)<.1:tangent=(0,1,0)
  return unit(add(mul(a,math.cos(limit)),mul(tangent,math.sin(limit))))
 f=limit/angle;return unit(add(mul(a,math.sin((1-f)*angle)/math.sin(angle)),mul(b,math.sin(f*angle)/math.sin(angle))))
def basis(forward):
 right=unit(cross((0,0,1),forward))
 if norm(right)<.1:right=(0,1,0)
 return forward,right,unit(cross(forward,right))
def intercept(rel,vel,speed):
 a=dot(vel,vel)-speed*speed;b=2*dot(rel,vel);c=dot(rel,rel)
 if abs(a)<1e-8:return max(0,-c/b) if abs(b)>1e-8 else norm(rel)/speed
 d=b*b-4*a*c
 if d<0:return norm(rel)/speed
 roots=[x for x in ((-b-math.sqrt(d))/(2*a),(-b+math.sqrt(d))/(2*a)) if x>=0]
 return min(roots) if roots else norm(rel)/speed

def slot_key(decks,x,y,z):return (int(clamp(z,0,decks-1)),int(clamp(x,0,4)),int(clamp(y,0,2)))
class Ship:
 def __init__(self,kind,side,idx,pos,rng,loadout=None):
  self.kind=kind;self.d=copy.deepcopy(P['ships'][kind]);self.side=side;self.initial_side=side;self.idx=idx;self.pos=pos;self.prev=pos;self.velocity=(0,0,0);self.forward=(1 if side==0 else -1,0,0);self.up=(0,0,1)
  self.hp=self.d['hp'];self.shield=self.d['shield'];self.shield_active=True;self.recovery=0.;self.last_hit=-1e6;self.dead=False;self.disabled={};self.last_face=None
  self.hero_bonus=False;self.repair_left=0.;self.repair_next=0.;self.repair_charges=2;self.repair_resource=[400,80,0];self.captured_hold=False
  self.deck=self.d['decks'];self.slots={slot_key(self.deck,x,y,z):{'type':'empty','hp':0.} for z in range(self.deck) for x in range(5) for y in range(3)}
  mid=1 if self.deck==3 else 0
  locations={'core':(mid,2,1),'bridge':(self.deck-1,4,1),'engine':(0,0,1),'shield':(mid,2,2),'radar':(mid,3,0),'weapon':(mid,3,1),'ammo':(0,1,0),'capacitor':(self.deck-1,1,2),'missile':(0,0,0),'pd':(mid,4,2),'life_support':(0,2,0),'pod_launcher':(0,0,2)}
  for name,key in locations.items():self.slots[key]={'type':name,'hp':self.d['hp']*(.35 if name=='core' else .18 if name=='bridge' else .1)}
  self.facility_keys={v['type']:k for k,v in self.slots.items() if v['type']!='empty'}
  self.armor={}
  for face in ('front','rear','left','right','top','bottom'):
   count=1 if face in ('front','rear') else 3*self.deck
   self.armor[face]=[self.d['front_armor'] if face=='front' else self.d['armor_cell']]*count
  assert sum(map(len,self.armor.values()))=={1:14,2:26,3:38}[self.deck]
  self.weapons=[]
  defs=loadout if loadout is not None else self.d['weapons']
  for w in defs:
   self.weapons.append({'kind':w['kind'],'tier':w['tier'],'mount':w['mount'],'hp':P['weapons'][w['kind']]['hp']*(1+.5*(w['tier']-1)),'dir':self.forward,'next':rng.uniform(0,P['weapons'][w['kind']]['period']),'last_seen':0.,'last_point':pos,'target':None,'salvo':[]})
  self.pd_timers=[rng.uniform(0,P['global']['pd_interval']) for _ in range(self.d['pd'])]
 def axes(self):return basis(self.forward)
 def local(self,p):return tuple(dot(sub(p,self.pos),axis) for axis in self.axes())
 def functional(self,name):return name not in self.disabled and not self.dead
 def bounds(self):return (self.d['radius']*1.6,self.d['radius']*.7,self.d['radius']*.5)
 def intersect(self,a,b,previous=False):
  # Relative segment vs moving ellipsoid; no extra random dodge roll.
  axes=self.axes();r=self.bounds();origin=self.prev if previous else self.pos
  p=tuple(dot(sub(a,origin),axis)/rr for axis,rr in zip(axes,r));q=tuple(dot(sub(b,self.pos),axis)/rr for axis,rr in zip(axes,r));v=sub(q,p)
  A=dot(v,v);B=2*dot(p,v);C=dot(p,p)-1
  if C<=0:return 0.
  disc=B*B-4*A*C
  if A<1e-12 or disc<0:return None
  roots=[t for t in ((-B-math.sqrt(disc))/(2*A),(-B+math.sqrt(disc))/(2*A)) if 0<=t<=1]
  return min(roots) if roots else None
 def available(self,w):
  supply='capacitor' if 'beam' in w['kind'] else 'missile' if w['kind'] in ('missile','guided') else 'ammo'
  return w['hp']>0 and self.functional('weapon') and self.functional(supply) and not self.captured_hold
 def armor_slot(self,point):
  x,y,z=self.local(point);rx,ry,rz=self.bounds();n=(x/rx,y/ry,z/rz);axis=max(range(3),key=lambda i:abs(n[i]))
  positive=n[axis]>=0;face=(('front','rear'),('right','left'),('top','bottom'))[axis][0 if positive else 1]
  ix=int(clamp((n[0]+1)*2.5,0,4));iy=int(clamp((n[1]+1)*1.5,0,2));iz=int(clamp((n[2]+1)*self.deck/2,0,self.deck-1))
  if face=='front':ix=4;index=0
  elif face=='rear':ix=0;index=0
  elif face in ('left','right'):
   iy=0 if face=='left' else 2;index=iz*3+int(clamp((n[0]+1)*1.5,0,2))
  else:
   iz=self.deck-1 if face=='top' else 0;index=int(clamp((n[1]+1)*self.deck/2,0,self.deck-1))*3+int(clamp((n[0]+1)*1.5,0,2))
  return face,index,slot_key(self.deck,ix,iy,iz)
 def wreck(self):
  if self.dead:return
  self.dead=True;self.shield=0;self.shield_active=False;self.velocity=(0,0,0)
 def kill_room(self,key,cause,queue):
  room=self.slots[key]
  if room['type']=='empty':return
  room['hp']=0;name=room['type'];self.disabled[name]=cause
  if name=='core':self.wreck()
  if name=='shield':self.shield=0;self.shield_active=False
  if cause!='sabotage':
   z,x,y=key
   if self.deck==3:
    if z==2:self.armor['top'][int(y)*3+min(2,int(x*3/5))]=0
    elif z==0:self.armor['bottom'][int(y)*3+min(2,int(x*3/5))]=0
    elif y in (0,2):self.armor['left' if y==0 else 'right'][3+min(2,int(x*3/5))]=0
   # 1/2 deck face linkage is intentionally not invented here.
  if name in ('ammo','capacitor','missile'):queue.append(key)
 def internal_damage(self,key,amount,cause,queue):
  if self.dead:return 0.
  room=self.slots[key];live=room['type']!='empty' and room['hp']>0
  if live:
   room['hp']=max(0.,room['hp']-amount)
   if room['hp']==0:self.kill_room(key,cause,queue)
  loss=amount*(P['global']['facility_to_hull'] if live else 1)
  self.hp=max(0.,self.hp-loss)
  if self.hp==0:self.wreck()
  return loss
 def damage(self,amount,point,time):
  if self.dead:return 0.
  self.last_hit=time;self.recovery=P['global']['shield_delay']
  if self.shield_active:
   overflow=max(0,amount-self.shield);self.shield=max(0,self.shield-amount)
   if self.shield==0:self.shield_active=False
   amount=overflow*P['global']['overflow_fraction']
  if amount<=0:return 0.
  face,index,key=self.armor_slot(point);self.last_face=face;maximum=self.d['front_armor'] if face=='front' else self.d['armor_cell'];armor=self.armor[face][index]
  penetrating=self.deck==3 and face in ('top','bottom') and armor<=0 and self.slots[key]['hp']<=0
  if penetrating:key=(1,key[1],key[2])
  reduction=0 if penetrating else (P['global']['front_reduction'] if face=='front' else P['global']['side_reduction'])*armor/maximum
  self.armor[face][index]=max(0.,armor-amount*P['global']['armor_wear'])
  delivered=amount*(1-reduction);queue=[];loss=self.internal_damage(key,delivered,'external',queue);seen=set()
  while queue and not self.dead:
   roomkey=queue.pop(0)
   if roomkey in seen:continue
   seen.add(roomkey);name=self.slots[roomkey]['type'];relevant=[w for w in self.weapons if ('beam' in w['kind'] if name=='capacitor' else w['kind'] in ('missile','guided') if name=='missile' else w['kind'] in ('cannon','heavy_cannon','spinal'))]
   blast=20*sum(w['tier'] for w in relevant)
   z,x,y=roomkey
   for neighbor in ((z,x-1,y),(z,x+1,y),(z,x,y-1),(z,x,y+1),(z-1,x,y),(z+1,x,y)):
    if neighbor in self.slots:loss+=self.internal_damage(neighbor,blast,'explosion',queue)
  return loss
 def tick_shield(self,dt,time):
  if not self.functional('shield'):self.shield=0;self.shield_active=False;return
  delay=self.recovery;self.recovery=max(0.,self.recovery-dt)
  if not self.shield_active:
   duration=max(0.,dt-delay);rate=P['global']['shield_noncombat_regen'] if time-self.last_hit>=15 else P['global']['shield_combat_regen']
   self.shield=min(self.d['shield']*.5,self.shield+self.d['shield']*rate*duration)
   if self.shield>=self.d['shield']*.5-1e-6:self.shield_active=True

class FleetBattle:
 def __init__(self,left,right,seed=0,distance=700,motion='orbit',loadout=None):
  self.rng=random.Random(seed);self.t=0.;self.motion=motion;self.ships=[];self.projectiles=[];self.hold_targets=set();self.on_pod_arrival=None;self.on_pod_loss=None;self.metrics={'shots':0,'beam_hits':0,'projectile_hits':0,'pd_kills':0,'friendly_blocks':0,'captured':0}
  idx=0
  for side,kinds in enumerate((left,right)):
   spacing=max(P['ships'][k]['radius'] for k in kinds)*2.5
   for j,kind in enumerate(kinds):
    pos=((-.5 if side==0 else .5)*distance,(j-(len(kinds)-1)/2)*spacing,self.rng.uniform(-10,10))
    self.ships.append(Ship(kind,side,idx,pos,self.rng,loadout.get((side,j)) if loadout else None));idx+=1
 def enemies(self,s):return [a for a in self.ships if not a.dead and a.side!=s.side and a.idx not in self.hold_targets]
 def radar(self,s,target):
  dist=norm(sub(target.pos,s.pos));values=[]
  if max(0,dist-s.bounds()[0]-target.bounds()[0])<=P['global']['guaranteed_range']:values.append(0.)
  for ally in self.ships:
   if ally.dead or ally.side!=s.side or not ally.functional('radar'):continue
   r=P['radar'][str(ally.d['radar_tier'])]
   if norm(sub(target.pos,ally.pos))<=r['range']:
    factor=1 if ally is s else .8 if ally.kind=='radar' else 0
    values.append(r['bonus']*factor)
  return max(values) if values else None
 def target(self,s):
  es=self.enemies(s)
  return min(es,key=lambda e:(norm(sub(e.pos,s.pos)),e.idx)) if es else None
 def pd_origins(self,s):
  axes=s.axes();rx,ry,rz=s.bounds();n=max(1,len(s.pd_timers));out=[]
  for i in range(n):
   angle=2*math.pi*i/n;out.append(add(s.pos,add(mul(axes[0],math.cos(angle)*rx*.8),add(mul(axes[1],math.sin(angle)*ry*.9),mul(axes[2],rz*.9)))))
  return out
 def step(self,dt=.1):
  self.t+=dt
  for s in self.ships:
   s.prev=s.pos
   if s.dead:continue
   s.tick_shield(dt,self.t);target=self.target(s)
   if target and s.functional('engine'):
    direction=unit(sub(target.pos,s.pos));s.forward=turn(s.forward,direction,s.d['hull_turn']*dt);s.up=basis(s.forward)[2]
    if self.motion!='fixed' and not s.captured_hold:
     pref=(s.bounds()[0]+target.bounds()[0]+200) if self.motion=='close' or self.radar(s,target) is None else 585;dist=norm(sub(target.pos,s.pos));radial=clamp((dist-pref)/180,-1,1);tangent=unit(cross((0,0,1),direction));desired=unit(add(mul(direction,radial),mul(tangent,.38 if s.idx%2 else -.38)));v=mul(desired,s.d['speed']*.7*(1+P['officers']['commander_speed'] if s.hero_bonus else 1))
     delta=sub(v,s.velocity);limit=s.d['speed']*.45*dt;s.velocity=add(s.velocity,mul(unit(delta),min(limit,norm(delta))));s.pos=add(s.pos,mul(s.velocity,dt))
   elif not s.functional('engine'):s.velocity=(0,0,0)
   if s.hero_bonus and s.hp<s.d['hp']*.85 and s.repair_charges and self.t>=s.repair_next and s.repair_left<=0 and s.repair_resource[0]>=100 and s.repair_resource[1]>=20:
    s.repair_left=20;s.repair_charges-=1;s.repair_next=self.t+45;s.repair_resource[0]-=100;s.repair_resource[1]-=20
   if s.repair_left>0 and not s.dead:
    used=min(dt,s.repair_left);s.repair_left-=used;s.hp=min(s.d['hp'],s.hp+s.d['hp']*.0008*used)
  # PD measures from surface-mounted origins, not from a large ship's center.
  for s in self.ships:
   if s.dead:continue
   if not s.functional('pd'):continue
   origins=self.pd_origins(s)
   for i in range(len(s.pd_timers)):
    if self.t+1e-8<s.pd_timers[i]:continue
    s.pd_timers[i]+=P['global']['pd_interval']
    candidates=[p for p in self.projectiles if p.get('hp') is not None and p['hp']>0 and p['side']!=s.side and norm(sub(p['pos'],origins[i]))<=111*s.d['pd_range_scale']]
    if candidates:
     p=min(candidates,key=lambda p:(0 if p['kind']=='pod' else 1,norm(sub(p['pos'],origins[i]))))
     if self.rng.random()<.72*(1-p.get('evasion',.25)):
      p['hp']-=12
      if p['hp']<=0:
       self.metrics['pd_kills']+=1
       if p['kind']=='pod' and self.on_pod_loss:self.on_pod_loss(p,'intercepted')
  pending=[];remaining=[]
  for p in self.projectiles:
   if p.get('hp') is not None and p['hp']<=0:continue
   if self.t>p['expiry']:
    if p['kind']=='pod' and self.on_pod_loss:self.on_pod_loss(p,'expired_rescue')
    continue
   a=p['pos'];target=p['target']
   if p['kind'] in ('guided','pod') and not target.dead:p['direction']=turn(p['direction'],sub(target.pos,a),(P['weapons']['guided']['guide_turn'] if p['kind']=='guided' else 100)*dt)
   if p['kind']=='pod' and target.dead:
    if self.on_pod_loss:self.on_pod_loss(p,'returned_target_lost')
    continue
   b=add(a,mul(p['direction'],p['speed']*dt));hits=[]
   for victim in self.ships:
    if victim.dead or victim.idx==p['owner']:continue
    # Friendly post-launch intersections are counted neither as damage nor shield here: unresolved R21.
    if victim.side==p['side'] and victim.idx!=p['target'].idx:continue
    frac=victim.intersect(a,b,True)
    if frac is not None:hits.append((frac,victim))
   if hits:
    frac,victim=min(hits,key=lambda x:x[0]);point=add(a,mul(sub(b,a),frac))
    if p['kind']=='pod':
     if victim.shield_active and victim.side!=p['side']:
      if self.on_pod_loss:self.on_pod_loss(p,'shield_collision')
     elif self.on_pod_arrival:self.on_pod_arrival(p,victim,point)
    else:pending.append((victim,p['damage'],point));self.metrics['projectile_hits']+=1
   else:p['pos']=b;remaining.append(p)
  self.projectiles=remaining
  # Compute firing intents before applying this frame's damage, preserving same-step reciprocity.
  for s in self.ships:
   if s.dead:continue
   target=self.target(s)
   for w in s.weapons:
    spec=P['weapons'][w['kind']]
    if not target:continue
    bonus=self.radar(s,target);direction=unit(sub(target.pos,s.pos));up=s.up;elevation=math.degrees(math.asin(clamp(dot(direction,up),-1,1)))
    valid=bonus is not None
    if valid:w['last_seen']=self.t;w['last_point']=target.pos
    aim=w['last_point'];w['dir']=turn(w['dir'],sub(aim,s.pos),spec['turn']*dt)
    if not valid and self.t-w['last_seen']>3.5:w['dir']=turn(w['dir'],s.forward,spec['turn']*dt)
    arc=elevation>=-15 if w['mount']=='top' else elevation<=15
    origin=add(s.pos,mul(up,(1 if w['mount']=='top' else -1)*s.d['radius']*.55));dist=norm(sub(target.pos,origin))
    aligned=dot(w['dir'],unit(sub(target.pos,origin)))>=math.cos(math.radians(12))
    block=any(a is not s and not a.dead and a.side==s.side and a.intersect(origin,target.pos) is not None for a in self.ships)
    eligible=valid and arc and aligned and dist<=spec['range'] and s.available(w) and not block
    if block:self.metrics['friendly_blocks']+=1
    if self.t+1e-8>=w['next']:
     # A blocked/unavailable weapon stays ready; it does not bank multiple future salvo cycles.
     if eligible:w['salvo']=[self.t+.22*k for k in range(spec['salvo'])];w['next']=self.t+spec['period']
    while w['salvo'] and w['salvo'][0]<=self.t+1e-8:
     w['salvo'].pop(0)
     if not eligible:w['salvo']=[];break
     self.metrics['shots']+=1;damage=spec['damage']*P['tier_damage'][str(w['tier'])]
     if 'beam' in w['kind']:
      chance=min(.98,.88+bonus+(.02 if s.hero_bonus else 0))
      if self.rng.random()<chance:
       point=add(target.pos,mul(unit(sub(origin,target.pos)),target.d['radius']));pending.append((target,damage,point));self.metrics['beam_hits']+=1
     else:
      rel=sub(target.pos,origin);lead=intercept(rel,target.velocity,spec['speed']);aim=add(target.pos,mul(target.velocity,lead));d=unit(sub(aim,origin));axes=basis(d)
      sigma=math.radians(spec['spread_deg'])/(1+4*(bonus or 0));d=unit(add(d,add(mul(axes[1],self.rng.gauss(0,sigma)),mul(axes[2],self.rng.gauss(0,sigma)))))
      # Unguided fixed VLS aesthetic curve is omitted, never replaced by target homing.
      missile=w['kind'] in ('missile','guided')
      self.projectiles.append({'kind':w['kind'],'owner':s.idx,'side':s.side,'target':target,'pos':origin,'direction':d,'speed':spec['speed'],'damage':damage,'hp':24 if missile else None,'evasion':.25,'expiry':self.t+(22 if w['kind']=='guided' else spec['range']/spec['speed']+3)})
  for target,damage,point in pending:target.damage(damage,point,self.t)
 def outcome(self):
  alive=[any(not s.dead and s.side==side for s in self.ships) for side in (0,1)]
  return None if all(alive) else 0 if alive[0] else 1 if alive[1] else -1
 def run(self,limit=360,dt=.1):
  while self.t<limit and self.outcome() is None:self.step(dt)
  return {'winner':self.outcome(),'seconds':round(self.t,3),'survivors':[sum(not s.dead and s.side==side for s in self.ships) for side in (0,1)],'remaining_hp':[round(sum(s.hp for s in self.ships if not s.dead and s.side==side),2) for side in (0,1)],**self.metrics}

class Unit:
 def __init__(self,role,side,i,rng,level=1):
  self.role=role;self.d=copy.deepcopy(P['heroes'][role]);self.side=side;self.idx=i;self.x=0. if side==0 else 24.;self.hp=self.d['hp']*(1+.04*(level-1));self.maximum=self.hp;self.damage=self.d['damage']*(1+.03*(level-1));self.next=rng.uniform(0,self.d['period']);self.skill_next=rng.uniform(0,2);self.brace_until=0.;self.charges=self.d.get('charges',0);self.healing=0.
 @property
 def alive(self):return self.hp>0
class HeroBattle:
 def __init__(self,left,right,seed=0,level=1,defender_level=None):
  self.rng=random.Random(seed);self.t=0.;self.units=[];self.damage_dealt=[0.,0.];self.heal_total=[0.,0.]
  for side,roles in enumerate((left,right)):
   for role in roles:self.units.append(Unit(role,side,len(self.units),self.rng,level if side==0 else defender_level or level))
 def outcome(self):
  alive=[any(u.alive and u.side==side for u in self.units) for side in (0,1)]
  return None if all(alive) else 0 if alive[0] else 1 if alive[1] else -1
 def step(self,dt=.1):
  self.t+=dt;intents=[]
  for u in self.units:
   if not u.alive:continue
   enemies=[v for v in self.units if v.alive and v.side!=u.side];friends=[v for v in self.units if v.alive and v.side==u.side]
   if not enemies:continue
   target=min(enemies,key=lambda v:(abs(v.x-u.x),v.hp,v.idx));dist=abs(target.x-u.x);direction=1 if target.x>u.x else -1
   if dist>u.d['range']:u.x=clamp(u.x+direction*u.d['speed']*dt,0,40)
   elif u.role in ('ranged','support') and dist<4:u.x=clamp(u.x-direction*u.d['speed']*.7*dt,0,40)
   if self.t>=u.skill_next:
    if u.role=='melee':u.brace_until=self.t+4;u.skill_next=self.t+16
    elif u.role=='support' and u.charges:
     patient=min(friends,key=lambda x:x.hp/x.maximum)
     if patient.hp<=patient.maximum-40:
      heal=min(u.d['heal'],patient.maximum-patient.hp);patient.hp+=heal;u.charges-=1;u.healing+=heal;self.heal_total[u.side]+=heal;u.skill_next=self.t+u.d['skill_cd']
   if self.t+1e-8>=u.next and abs(target.x-u.x)<=u.d['range']+.1:
    u.next=self.t+u.d['period'];buff=1+max((f.d['aura_damage'] for f in friends if f.role=='commander' and abs(f.x-u.x)<=f.d['aura_range']),default=0)
    damage=u.damage*buff
    if u.role=='ranged' and self.t>=u.skill_next:damage*=2;u.skill_next=self.t+12
    if self.rng.random()<.92*(1-target.d['evasion']):
     reduction=min(.65,target.d['armor']+(.20 if target.brace_until>self.t else 0));intents.append((target,damage*(1-reduction),u.side))
  for target,damage,side in intents:
   applied=min(target.hp,damage);target.hp=max(0,target.hp-damage);self.damage_dealt[side]+=applied
 def run(self,limit=180,dt=.1):
  while self.t<limit and self.outcome() is None:self.step(dt)
  return {'winner':self.outcome(),'seconds':round(self.t,3),'survivors':[sum(u.alive and u.side==s for u in self.units) for s in (0,1)],'remaining_hp':[round(sum(u.hp for u in self.units if u.alive and u.side==s),2) for s in (0,1)],'healing':self.heal_total}

class HybridBattle:
 """A close-contact case: actual approach navigation/capture recapture are outside this reduced model."""
 def __init__(self,seed=0,policy='officers',goal='shield',escort=False):
  left=['cruiser','vector'];right=['cruiser','vector']+(['air_defense'] if escort else [])
  self.b=FleetBattle(left,right,seed=seed,distance=220,motion='fixed');self.seed=seed;self.policy=policy;self.goal=goal
  self.host=self.b.ships[0];self.target=next(s for s in self.b.ships if s.side==1 and s.kind=='cruiser')
  self.host.hero_bonus=policy=='officers';self.roles=['melee','ranged','support','commander'];self.arrived=[];self.rescue=[];self.returned=[];self.dead=[];self.in_space=[];self.safe_home=[];self.returning=[];self.hero_homes={};self.sent=0;self.launched_at=None
  self.entry_key=None;self.phase='waiting';self.remaining=0.;self.indoor=None;self.entered=False;self.done=False;self.objective=False;self.extracted=False;self.event_times={};self.survivors=[];self.pod_hp=P['global']['pod_hp']
  self.b.on_pod_arrival=self.arrival;self.b.on_pod_loss=self.pod_loss
 def pod_loss(self,p,cause):
  role=p['role']
  if role in self.in_space:self.in_space.remove(role)
  if cause.startswith('returned'):self.returned.append(role)
  else:self.rescue.append(role)
  if p.get('returning') and role in self.returning:self.returning.remove(role)
 def arrival(self,p,ship,point):
  role=p['role']
  if role in self.in_space:self.in_space.remove(role)
  if p.get('returning'):
   if role in self.returning:self.returning.remove(role)
   if ship.side==0:self.safe_home.append(role);self.hero_homes[role]=ship
   else:self.rescue.append(role)
  elif ship is self.target:
   self.arrived.append(role);self.entered=True;self.event_times.setdefault('first_entry',self.b.t)
   if self.entry_key is None:
    z,x,y=ship.armor_slot(point)[2];self.entry_key=(z,x,self.b.rng.randrange(3))
  else:self.rescue.append(role)
 def onboard_roles(self):
  if self.indoor:return [u.role for u in self.indoor.units if u.side==0 and u.alive]
  return list(self.arrived)
 def step(self,dt):
  b=self.b
  # Protecting boarders is a player firing policy, not an invulnerability window.
  aboard=self.entered and not self.done and self.phase in ('launching','walking','fighting','capturing','extracting','timeout_aboard') and bool(self.onboard_roles())
  if self.policy=='boarding_safe' and aboard and self.target.hp<self.target.d['hp']*.5:b.hold_targets.add(self.target.idx)
  else:b.hold_targets.discard(self.target.idx)
  b.step(dt)
  # Docking is not immunity: a later wreck kills heroes who have returned aboard.
  for role,home in list(self.hero_homes.items()):
   if home.dead:
    self.hero_homes.pop(role);self.safe_home.remove(role);self.dead.append(role)
  if self.policy in ('officers','fleet_only'):return
  if self.host.dead and not self.launched_at:
   if not self.done:self.dead.extend(self.roles)
   self.done=True;self.phase='host_lost';return
  if self.host.dead and self.sent<4 and self.launched_at:
   self.dead.extend(self.roles[self.sent:]);self.sent=4
  if not self.launched_at and not self.target.dead and not self.target.shield_active and norm(sub(self.target.pos,self.host.pos))-sum((self.target.bounds()[0],self.host.bounds()[0]))<=250:
   self.launched_at=b.t;self.phase='launching';self.event_times['launch']=b.t
  if self.launched_at and self.sent<4 and b.t>=self.launched_at+.25*self.sent and not self.host.dead:
   role=self.roles[self.sent];self.sent+=1;direction=unit(sub(self.target.pos,self.host.pos));origin=add(self.host.pos,mul(direction,self.host.bounds()[0]+1))
   b.projectiles.append({'kind':'pod','owner':self.host.idx,'side':0,'target':self.target,'pos':origin,'direction':direction,'speed':165,'damage':0,'hp':self.pod_hp,'evasion':.4,'expiry':b.t+12,'role':role});self.in_space.append(role)
  if self.phase=='launching' and self.sent==4 and not self.in_space:
   if not self.arrived:self.phase='all_pods_lost';self.done=True;return
   self.phase='walking';self.entered=True;self.event_times['entry']=b.t
   # Graph-derived Manhattan representative route, not a full crowd/pathfinding model.
   room=self.target.facility_keys[self.goal];entry=self.entry_key
   edges=abs(entry[1]-room[1])+abs(entry[2]-room[2]);vertical=abs(entry[0]-room[0]);length=(edges*8+vertical*6)*(1+.15*(self.target.d['size']-1))
   self.walk_time=length/min(P['heroes'][r]['speed'] for r in self.arrived);self.remaining=self.walk_time+1.5
  if self.entered and not self.done and self.phase!='return_flight' and self.target.dead:
   self.dead.extend(self.onboard_roles());self.phase='aboard_wreck';self.done=True;return
  if self.entered and self.phase in ('walking','fighting','capturing') and self.target.slots[self.target.facility_keys[self.goal]]['hp']<=0:
   self.event_times.setdefault('facility_already_destroyed',b.t);self.phase='extracting';self.remaining=self.walk_time
  if self.phase=='walking':
   self.remaining-=dt
   if self.remaining<=0:
    defenders=['soldier']*(12 if self.goal=='bridge' else 8)+['commander'];self.indoor=HeroBattle(self.arrived,defenders,seed=self.seed+1000);self.phase='fighting'
  elif self.phase=='fighting':
   self.indoor.step(dt)
   if self.indoor.outcome() is not None:
    self.dead.extend(u.role for u in self.indoor.units if u.side==0 and not u.alive)
    if self.indoor.outcome()!=0:self.phase='boarding_defeated';self.done=True;return
    self.survivors=self.onboard_roles();factor=min(2,(1+.12*(len(self.survivors)-1))*max(P['heroes'][x]['occupation'] for x in self.survivors))
    self.remaining=(36 if self.goal=='bridge' else 18)/factor;self.phase='capturing'
  elif self.phase=='capturing':
   self.remaining-=dt
   if self.remaining<=0:
    self.objective=True;self.event_times['objective']=b.t
    if self.goal in ('shield','weapon'):
     self.target.disabled[self.goal]='occupied'
     if self.goal=='shield':self.target.shield=0;self.target.shield_active=False
    elif self.goal=='bridge':
     self.target.side=0;self.target.captured_hold=True;self.target.velocity=(0,0,0);self.b.metrics['captured']+=1
     for w in self.target.weapons:w['salvo']=[]
    self.remaining=self.walk_time;self.phase='extracting'
  elif self.phase=='extracting':
   self.remaining-=dt
   if self.remaining<=0:
    self.event_times['exit_pod']=b.t;self.phase='return_flight'
    for role in self.onboard_roles():
     homes=[s for s in b.ships if not s.dead and s.side==0 and s is not self.target]
     if not homes:self.rescue.append(role);continue
     home=min(homes,key=lambda s:norm(sub(s.pos,self.target.pos)));direction=unit(sub(home.pos,self.target.pos));origin=add(self.target.pos,mul(direction,self.target.bounds()[0]+1))
     self.returning.append(role);self.in_space.append(role)
     b.projectiles.append({'kind':'pod','owner':self.target.idx,'side':0,'target':home,'pos':origin,'direction':direction,'speed':165,'damage':0,'hp':80,'evasion':.4,'expiry':b.t+12,'role':role,'returning':True})
  if self.phase=='return_flight' and not self.returning:
   self.extracted=bool(self.safe_home);self.event_times['return_resolved']=b.t;self.phase='returned' if self.safe_home else 'return_lost';self.done=True
 def run(self,limit=300,dt=.1):
  while self.b.t<limit:
   self.step(dt)
   if self.b.outcome() is not None and (self.policy in ('officers','fleet_only') or self.done or (not self.entered and not self.in_space)):break
  if self.entered and not self.done and not self.target.dead and self.phase!='return_flight':self.phase='timeout_aboard'
  living=self.onboard_roles() if self.entered and not self.target.dead else []
  if self.indoor:self.dead.extend(u.role for u in self.indoor.units if u.side==0 and not u.alive)
  if self.policy in ('officers','fleet_only') and self.host.dead:self.dead.extend(self.roles)
  return {'winner':self.b.outcome(),'seconds':round(self.b.t,3),'policy':self.policy,'goal':self.goal,'objective':self.objective,'extracted':self.extracted,'phase':self.phase,'pods_sent':self.sent,'pods_entered':len(self.arrived),'heroes_rescue':len(self.rescue),'heroes_dead':len(set(self.dead)),'heroes_extracted':len(self.safe_home),'heroes_aboard':len(living) if self.phase in ('walking','fighting','capturing','extracting','timeout_aboard') else 0,'events':{k:round(v,3) for k,v in self.event_times.items()},'captured':self.b.metrics['captured'],'target_wrecked':self.target.dead,'host_wrecked':self.host.dead}
