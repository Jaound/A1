"""D116 integrated design experiment. Spatial flight approximation, not mesh physics.

Inputs, enemy observations and developer diagnostics are deliberately separate.
Legacy result files/engines are never imported or modified.
"""
import copy
import heapq
import json
import math
import random
from collections import Counter, defaultdict
from pathlib import Path

from indoor import Layout, Unit, indoor_step, move

P=json.loads((Path(__file__).parent/'profile.json').read_text())
C=P['integration']
CLASSES=('melee','ranged','support','commander')


def cost(roster):return sum(P['ships'][k]['fleet_cost'] for k in roster)
def dist(a,b):return math.dist(a.pos,b.pos)
def surface(a,b):return max(0,dist(a,b)-a.d['radius']-b.d['radius'])


def roster(budget,kind='mixed'):
    if kind in ('mixed','logistics','no_pd'):
        if budget==100:r=['battleship','carrier','vector','bastion','cruiser','cruiser','air_defense','radar']+['corvette']*10+['frigate']*8+['repair','assault']
        elif budget==200:r=['battleship']*2+['carrier','missile_ship']+['vector','bastion','cruiser','air_defense','radar']*3+['cruiser']+['corvette']*10+['frigate']*8+['repair','assault']
        elif budget==300:r=['dreadnought','battleship','battleship','carrier','missile_ship']+['vector','bastion','cruiser','air_defense','radar']*2+['corvette']*13+['frigate']*10+['repair','assault']
        else:
            # Unequal campaign encounters use explicit packet construction, not hidden DPS buffs.
            r=['battleship']*(budget//50)+['cruiser']*((budget%50)//10)
            r+=['corvette']*(budget-cost(r))
        if kind=='logistics':r[r.index('corvette')]='mining';r[r.index('frigate')]='transport'
        if kind=='no_pd':r=['cruiser' if x=='air_defense' else x for x in r]
    elif kind=='light':r=['corvette','frigate']*(budget//2)+['corvette']*(budget%2)
    elif kind in ('heavy','carrier','missile'):
        hull={'heavy':'battleship','carrier':'carrier','missile':'missile_ship'}[kind]
        r=[hull]*(budget//25)+['corvette']*(budget%25)
    elif kind=='xl':r=['battlecarrier']*(budget//125)+['battleship']*((budget%125)//25)+['corvette']*(budget%25)
    else:raise ValueError(kind)
    assert cost(r)==budget
    return r


class Ship:
    def __init__(self,kind,side,uid,index,profile,rng,layout_scale=1.,capacity_rank=0,fighter_rank=0,refit=False):
        self.kind=kind;self.side=side;self.original_side=side;self.uid=uid;self.d=profile['ships'][kind]
        self.size=self.d['size'];self.hp=self.maxhp=self.d['hp'];self.shield=self.maxshield=self.d['shield']
        self.active=True;self.dead=False;self.escaped=False;self.now=0.;self.last_combat=-1e6
        self.pos=[(-1 if side==0 else 1)*(C['opening_range']/2+(110 if self.size>=3 else 0)),(index%7-3)*100.,(index//7%3-1)*45.]
        self.vel=[0.,0.,0.];self.target=None;self.target_until=0.;self.ftl=None;self.order_until=0.
        self.layout=Layout(self.d['decks'],self.d['radius'],layout_scale);self.units=[];self.officers=[]
        self.facilities={};self.by_node={};self.armor={};self.armor_max={};self.exploded=set();self.life_since=None
        self.controlled=False;self.hangar_count=self.d.get('hangars',0)
        self.capacity=self.hangar_count*(20+2*capacity_rank);self.fighters=self.capacity
        self.fighter_hp=C['fighter_hp']*(1+.05*fighter_rank+(.08 if refit else 0))
        self.fighter_damage=C['fighter_damage']*(1+.04*fighter_rank+(.08 if refit else 0))
        self.fighter_evasion=min(.7,C['fighter_evasion']+(.03 if refit else 0))
        self.fighter_loss_hp=0.;self.fighter_next=0.;self.fighter_return=False;self.fighter_docked=0;self.fighter_return_at=None
        self.cargo=list(profile['repair']['standard_material_load']) if kind in ('repair','transport') else [0.,0.,0.]
        self.repair_target=None;self.repair_ready=0.;self.supply_ready=0.;self.pd_budget=0.
        self.guns=[{**w,'hp':profile['weapons'][w['kind']]['hp']*profile['tier_damage'][str(w['tier'])],
                    'maxhp':profile['weapons'][w['kind']]['hp']*profile['tier_damage'][str(w['tier'])],
                    'next':rng.random()*profile['weapons'][w['kind']]['period'],'restart':False}
                   for w in self.d['weapons']]
        roles=['core','bridge','life','engine','shield','radar','pd','cargo']
        if self.guns:roles+=['weapon']
        for supply,group in [('ammo',('cannon','heavy_cannon','spinal')),('energy',('beam','heavy_beam')),('missile',('missile','guided'))]:
            if any(w['kind'] in group for w in self.guns):roles.append(supply)
        if kind=='assault':roles.append('pod')
        if self.hangar_count:roles+=['hangar','flight']
        slots=[n for n in self.layout.nodes if n not in (self.layout.core,self.layout.bridge)]
        slots.sort(key=lambda n: (n%3==1, -self.layout.distance(n,self.layout.core),n))
        for role in roles:
            node=self.layout.core if role=='core' else self.layout.bridge if role=='bridge' else slots.pop(0)
            hp=self.maxhp*(C['core_hp_fraction'] if role=='core' else C['facility_hp_fraction'])*(C['bridge_hp_multiplier'] if role=='bridge' else 1)
            self.facilities[role]={'hp':hp,'maxhp':hp,'crew':0. if role=='core' else 1.,'node':node,'owner':side,'destroyed':False}
            self.by_node[node]=role
        for face in ('front','rear','left','right','top','bottom'):
            count=1 if face in ('front','rear') else self.d['decks']*3 if face in ('left','right') else (3 if self.d['decks']==1 else 6 if self.d['decks']==2 else 9)
            hp=self.d['front_armor'] if face=='front' else self.d['armor_cell']
            self.armor[face]=[float(hp)]*count;self.armor_max[face]=[float(hp)]*count

    def alive(self):return not self.dead and not self.escaped and self.side in (0,1)
    def eff(self,role):
        f=self.facilities.get(role)
        if not f or f['destroyed'] or f['hp']<=0 or f['owner']!=self.side:return 0.
        return 1. if role=='core' else f['crew']

    def combat_ready(self):
        return self.alive() and self.eff('weapon')>0 and any(w['hp']>0 and not w['restart'] and self.eff(supply(w['kind']))>0 for w in self.guns)

    def shield_tick(self,now,dt):
        efficiency=self.eff('shield')
        if not efficiency:self.active=False;self.shield=0;return
        if self.active:return
        rate=C['combat_regen'] if now-self.last_combat<C['noncombat_after'] else C['noncombat_regen']
        self.shield=min(self.maxshield*.5,self.shield+self.maxshield*rate*efficiency*dt)
        if self.shield>=self.maxshield*.5-1e-9:self.active=True


def supply(kind):
    if kind in ('beam','heavy_beam'):return 'energy'
    return 'missile' if kind in ('missile','guided') else 'ammo'


class Battle:
    def __init__(self,left,right,seed=0,*,mode='none',protection='until_exit',level=1,defenders=2,
                 gear=0,enemy_level=None,capacity_rank=0,fighter_rank=0,refit=False,aim='hull',
                 reverse=False,layout_scale=1.,duration=None,retreat=True,heroes=True,objective='disable',hero_budget=15,
                 enemy_capacity_rank=0,enemy_fighter_rank=0,enemy_refit=False,force_weapons=False):
        self.rng=random.Random(seed);self.board_rng=random.Random(seed+1000003);self.seed=seed
        self.mode=mode;self.protection=protection;self.aim=aim;self.reverse=reverse;self.retreat=retreat;self.objective=objective
        self.force_weapons=force_weapons
        self.duration=duration or C['limit'];self.t=0.;self.metric=defaultdict(float)
        self.events=[];self.sequence=0;self.ships=[];self.operations=[];self.initial=[cost(left),cost(right)]
        self.trace=[];self.knowledge=[{},{}];self.first_loss=None;self.main_end=None;self.no_work=0.
        self.transport_log=[]
        self._live_cache=None;self._radar_cache={}
        self.hero_counts=[0,0];self.last_damage=0.;self.damage_by_size=Counter();self.shots_by_size=Counter()
        for side,rs in enumerate((left,right)):
            for i,kind in enumerate(rs):
                self.ships.append(Ship(kind,side,len(self.ships),i,P,self.rng,layout_scale,
                                       capacity_rank if side==0 else enemy_capacity_rank,
                                       fighter_rank if side==0 else enemy_fighter_rank,
                                       refit if side==0 else enemy_refit))
        self.unit_seq=0
        if heroes:
            for side in (0,1):
                lv=level if side==0 else (enemy_level or level)
                owned=self.living(side);caps=sorted(owned,key=lambda s:-s.size)
                attack_ship=next((s for s in owned if s.kind=='assault'),None)
                if side==0 and mode!='none' and attack_ship:
                    for k in CLASSES:self.add_unit(attack_ship,k,side,lv,gear,True)
                    for _ in range(8):self.add_unit(attack_ship,'soldier',side,assault=True)
                if caps:
                    for i in range(min(defenders,P['heroes']['hero_slots_by_ship_size'][caps[0].size-1])):
                        self.add_unit(caps[0],CLASSES[i%4],side,lv,gear)
                # Remaining optional officers occupy actual ship/seat slots and faction budget.
                for ship in caps:
                    if self.hero_counts[side]>=hero_budget:break
                    if ship is attack_ship or ship is caps[0]:continue
                    existing=sum(u.hero for u in ship.units)+len(ship.officers)
                    if existing>=P['heroes']['hero_slots_by_ship_size'][ship.size-1]:continue
                    u=self.make_unit(CLASSES[self.hero_counts[side]%4],side,ship.layout.core,lv,gear)
                    u.state='officer';ship.officers.append(u)
                    if u.kind=='support':ship.cargo=[120.,30.,4.]
                assert self.hero_counts[side]<=15
        # Normal initial guards: three squads, split across bridge/core/life, not free heroes.
        for ship in self.ships:
            for node in (ship.layout.bridge,ship.layout.core,ship.facilities['life']['node']):
                for _ in range(4):self.add_unit(ship,'soldier',ship.side,node=node)
        self.initial_heroes=list(self.hero_counts)
        self.initial_fighters=[sum(s.fighters for s in self.living(side)) for side in (0,1)]
        self.fighter_package_cost=[[sum(s.fighters*(C['fighter_new_cost'][i]+(C['fighter_refit_cost'][i] if (refit if side==0 else enemy_refit) else 0)) for s in self.living(side)) for i in range(3)] for side in (0,1)]

    def make_unit(self,kind,side,node,level=1,gear=0):
        u=Unit(self.unit_seq,kind,side,node,P,level,gear);self.unit_seq+=1
        if u.hero:self.hero_counts[side]+=1
        return u

    def add_unit(self,ship,kind,side,level=1,gear=0,assault=False,node=None):
        u=self.make_unit(kind,side,node if node is not None else ship.layout.bridge,level,gear)
        u.assault=assault;ship.units.append(u);return u

    def living(self,side):
        if self._live_cache is None:self._live_cache={i:[s for s in self.ships if s.side==i and s.alive()] for i in (0,1)}
        return self._live_cache.get(side,[])

    def observe(self):
        # Detector state determines contact. Never copy hero stats, cargo or hangar inventory.
        for side in (0,1):
            owned=self.living(side);contacts={}
            for enemy in self.living(1-side):
                if any(surface(a,enemy)<=250 or (a.eff('radar')>0 and dist(a,enemy)<=P['radar'][str(a.d['radar_tier'])]['range']) for a in owned):
                    contacts[enemy.uid]={'id':enemy.uid,'kind':enemy.kind,'position':tuple(enemy.pos),'shield_active':enemy.active,
                                         'observed_mounts':tuple(i for i,w in enumerate(enemy.guns) if w['hp']>0)}
            self.knowledge[side]=contacts

    def protected(self,target,side):
        if self.protection=='off':return False
        if self.protection=='until_exit':
            return any(u.hero and u.hp>0 and u.side==side and u.state in ('inside','aboard','return_walk') for u in target.units)
        for op in self.operations:
            if op['side']==side and op['target'] is target:
                inside=any(u.hp>0 and u.hero and u.state in ('inside','return_walk') for u in op['units'])
                if inside and (self.protection=='until_exit' or not op['objective']):return True
        return False

    def pick(self,attacker):
        contacts=self.knowledge[attacker.side]
        if self.force_weapons and self.aim=='weapons':
            for op in self.operations:
                target=op['target']
                if target.alive() and target.side!=attacker.side and target.uid in contacts and contacts[target.uid]['observed_mounts']:return target
        candidates=[s for s in self.living(1-attacker.side) if s.uid in contacts and not self.protected(s,attacker.side)]
        if not candidates:return None
        # Soft screen preference only; no focus cap, forced taunt, HP/cargo/hero oracle.
        def weight(s):
            kind=contacts[s.uid]['kind'];screen=C['screen_weight'] if kind in ('corvette','frigate') else 1
            if attacker.size>=3 and s.size==1:screen*=.35
            return screen/(.5+dist(attacker,s)/800)
        return self.rng.choices(candidates,[weight(s) for s in candidates])[0]

    def schedule(self,at,event):
        self.sequence+=1;heapq.heappush(self.events,(at,self.sequence,event))

    def wreck(self,s,cause):
        if s.dead:return
        s.dead=True;s.wreck_at=self.t;s.hp=0;s.active=False;s.shield=0;s.fighters=0;s.fighter_docked=0;self._live_cache=None
        for u in s.units+s.officers:
            if u.hp>0 and u.state in ('aboard','inside','return_walk','officer'):
                u.hp=0;u.state='dead'
                if u.hero:self.metric['hero_wreck_deaths']+=1
        if self.first_loss is None:self.first_loss=self.t
        self.metric['wreck_'+cause]+=1

    def destroy_facility(self,s,role,origin='external',chain=None):
        f=s.facilities[role]
        if f['destroyed']:return
        chain=set() if chain is None else chain
        if role in chain:return
        chain.add(role)
        f['hp']=0;f['destroyed']=True;f['crew']=0
        if origin!='sabotage':f['hazard_end']=self.t+20
        if origin!='sabotage':
            deck,q=divmod(f['node'],15);row,col=divmod(q,3)
            faces=[]
            if deck==s.d['decks']-1:faces.append('top')
            if deck==0:faces.append('bottom')
            if col==0:faces.append('left')
            if col==2:faces.append('right')
            # Representative prototype mapping; front/rear single plates are preserved.
            for face in faces:
                index=min(len(s.armor[face])-1,(deck*3+min(2,row//2)) if face in ('left','right') else min(2,row//2)*3+col)
                s.armor[face][index]=0.
        self.metric['facility_destroyed']+=1
        if role=='core':self.wreck(s,'core');return
        if role=='shield':s.shield=0;s.active=False
        if role=='cargo':s.cargo=[0.,0.,0.]
        if role=='hangar':s.fighter_docked=0
        if role=='bridge' and s.controlled:
            s.side=-1;s.target=None;s.vel=[0.,0.,0.];self._live_cache=None
        if role not in ('ammo','energy','missile'):return
        # Visited state is scoped to this damage chain. A repaired magazine is not immune forever.
        s.exploded.add(role)
        applicable=[w for w in s.guns if supply(w['kind'])==role]
        blast=sum(P['tier_damage'][str(w['tier'])] for w in applicable)*C['explosion_scale']
        for other,g in s.facilities.items():
            if other!=role and s.layout.distance(f['node'],g['node'])<=max(s.layout.length,s.layout.width)*1.1:
                dealt=min(g['hp'],blast);g['hp']-=dealt;s.hp=max(0,s.hp-blast*.65)
                if g['hp']<=0:self.destroy_facility(s,other,'explosion',chain)
        self.metric['explosions']+=1
        if s.hp<=0:self.wreck(s,'explosion')

    def hit(self,s,amount,attacker_side,weapon_index=None,point=None,penetration=1.):
        if s.dead or s.escaped:return
        s.last_combat=self.t;self.last_damage=self.t
        if s.active:
            spill=max(0,amount-s.shield);s.shield=max(0,s.shield-amount)
            if s.shield<=0:s.active=False
            amount=spill*P['global']['overflow_fraction']
        if amount<=0:return
        if weapon_index is not None and weapon_index<len(s.guns):
            gun=s.guns[weapon_index];before=gun['hp'];gun['hp']=max(0,before-amount)
            if gun['hp']==0 and before>0:gun['restart']=True;self.metric['weapons_destroyed']+=1
            self.metric['weapon_only_damage']+=min(before,amount);return
        amount*=penetration
        # Surface sample -> face/cell -> fixed prototype room, not an actual mesh raycast.
        face=self.rng.choice(('front','rear','left','right','top','bottom'))
        cell=self.rng.randrange(len(s.armor[face]));remaining=s.armor[face][cell]/s.armor_max[face][cell]
        reduction=(.6 if face=='front' else .45)*remaining
        passed=amount*(1-reduction);s.armor[face][cell]=max(0,s.armor[face][cell]-amount*.45)
        surface_nodes=[n for n in s.layout.nodes if n%3 in (0,2) or n%15<3 or n%15>=12]
        if face in ('top','bottom'):
            deck=s.d['decks']-1 if face=='top' else 0
            node=deck*15+self.rng.randrange(15)
        else:node=self.rng.choice(surface_nodes)
        role=s.by_node.get(node)
        if s.d['decks']==3 and face in ('top','bottom') and s.armor[face][cell]<=0 and (not role or s.facilities[role]['destroyed']):
            node=15+node%15;role=s.by_node.get(node)
        if role:
            f=s.facilities[role];f['hp']=max(0,f['hp']-passed)
            if role!='core':f['crew']=max(0,f['crew']-passed/f['maxhp']*C['crew_injury_fraction'])
            if f['hp']<=0:self.destroy_facility(s,role)
        # Damage transmission happens exactly once, regardless of facility remaining HP.
        damage=passed*.65;s.hp=max(0,s.hp-damage)
        self.damage_by_size[(attacker_side,s.size)]+=damage
        for u in s.units:
            if u.alive() and u.node==node:
                u.hp=max(0,u.hp-min(u.maxhp*.35,passed*.015))
                if u.hp==0:u.state='dead'
        if s.hp<=0:self.wreck(s,'hull')

    def pd_count(self,target):
        return sum(s.d['pd']*s.eff('pd') for s in self.living(target.side)
                   if surface(s,target)<=111*s.d.get('pd_range_scale',1))

    def intercept(self,target,hp,evasion,exposure,kind):
        # Per-owner budget shared between missiles, fighters, and pods; no duplicated PD output.
        for s in self.living(target.side):
            if surface(s,target)>111*s.d.get('pd_range_scale',1):continue
            attempts=min(int(s.pd_budget),math.ceil(s.d['pd']*s.eff('pd')*exposure/C['pd_period']))
            for _ in range(attempts):
                s.pd_budget-=1
                if self.rng.random()<.72*(1-evasion):hp-=C['pd_damage'];self.metric['pd_damage_'+kind]+=C['pd_damage']
                if hp<=0:return False
        return True

    def update_motion(self,s,dt):
        if s.ftl is not None or self.t<s.order_until or not s.eff('engine'):
            s.vel=[0.,0.,0.];return
        # Tactical range/formation approximation; no collision/mesh/autopilot certification.
        enemy_contacts=list(self.knowledge[s.side].values())
        if not enemy_contacts:s.vel=[0.,0.,0.];return
        center=[sum(x['position'][i] for x in enemy_contacts)/len(enemy_contacts) for i in range(3)]
        nearest=min(self.living(1-s.side),key=lambda a:math.dist(a.pos,center),default=None)
        desired=(900 if getattr(s,'extracting',False) else 160) if s.kind=='assault' else 450 if s.size<=2 else 600
        if s.kind in ('repair','transport'):
            friends=[a for a in self.living(s.side) if a is not s and a.hp<a.maxhp]
            if friends:
                patient=min(friends,key=lambda a:a.hp/a.maxhp);center=list(patient.pos);desired=patient.d['radius']+s.d['radius']+75
        vector=[center[i]-s.pos[i] for i in range(3)];length=math.sqrt(sum(x*x for x in vector))
        direction=1 if length>desired+20 else -1 if length<desired-80 else 0
        speed=s.d['speed']*.7*s.eff('engine')*(1+self.command_buffs(s)[1])
        s.vel=[direction*speed*x/max(1,length) for x in vector]
        for i in range(3):s.pos[i]+=s.vel[i]*dt

    def radar_bonus(self,a,v):
        key=(a.uid,v.uid)
        if key in self._radar_cache:return self._radar_cache[key]
        values=[0.]
        for r in self.living(a.side):
            if not r.eff('radar'):continue
            radar=P['radar'][str(r.d['radar_tier'])]
            if dist(r,v)<=radar['range']:values.append(radar['bonus']*r.eff('radar')*(1 if r is a else .8 if r.kind=='radar' else 0))
        self._radar_cache[key]=max(values)
        return max(values)

    def command_buffs(self,s):
        fire_rate=0.;movement=0.
        for u in s.officers:
            if u.hp<=0:continue
            if u.kind=='ranged' and 3 in u.ranks and s.eff('weapon'):
                if self.t>=u.cd[3] and s.target is not None:
                    u.buff['officer_fire_end']=self.t+8;u.cd[3]=self.t+60
                if u.buff['officer_fire_end']>self.t:fire_rate=max(fire_rate,u.value(3,'officer_fire_rate'))
            if u.kind=='commander' and s.eff('bridge'):
                movement=max(movement,u.value(1,'officer_speed_turn'))
                if 3 in u.ranks:
                    if self.t>=u.cd[3] and s.target is not None:u.buff['officer_move_end']=self.t+10;u.cd[3]=self.t+60
                    if u.buff['officer_move_end']>self.t:movement=max(movement,u.value(3,'officer_turn_bonus'))
        return min(.3,fire_rate),min(.2,movement)

    def fire(self,a,v):
        forced=self.force_weapons and self.aim=='weapons' and any(op['target'] is v for op in self.operations)
        if not v or not a.combat_ready() or v.uid not in self.knowledge[a.side] or (self.protected(v,a.side) and not forced):return
        if not any(w['next']<=self.t and w['hp']>0 for w in a.guns):return
        distance=dist(a,v);bonus=self.radar_bonus(a,v)
        for w in a.guns:
            spec=P['weapons'][w['kind']]
            if self.t<w['next'] or w['hp']<=0 or w['restart'] or not a.eff(supply(w['kind'])):continue
            if a.ftl is not None and supply(w['kind'])=='energy':continue
            if distance>spec['range']:continue
            vertical=math.degrees(math.asin(max(-1,min(1,(v.pos[2]-a.pos[2])/max(1,distance)))))
            if (w['mount']=='top' and vertical< -15) or (w['mount']=='bottom' and vertical>15):continue
            period=spec['period']/max(.1,a.eff('weapon'))/(1+self.command_buffs(a)[0]);w['next']=self.t+period
            a.last_combat=self.t;self.shots_by_size[a.size]+=1
            gun_index=None
            if self.aim=='weapons':
                observed=self.knowledge[a.side][v.uid]['observed_mounts']
                if observed:gun_index=self.rng.choice(observed)
                else:continue
            dmg=spec['damage']*P['tier_damage'][str(w['tier'])]
            officer_bonus=max([u.value(1,'officer_accuracy') for u in a.officers if u.kind=='ranged' and u.hp>0]+[0])
            bonus+=min(.08,officer_bonus)
            for shot in range(spec.get('salvo',1)):
                flight=distance/spec['speed'] if spec['speed'] else 0.
                sigma=distance*math.tan(math.radians(spec.get('spread_deg',0)))/(1+4*bonus)
                # Tracking error scales with target transverse speed, size and weapon rotation.
                tracking=(v.d['speed']/max(1,spec['turn']))*(2 if a.size>=3 and v.size==1 else 1)
                aim=[v.pos[i]+v.vel[i]*flight+self.rng.gauss(0,sigma+tracking) for i in range(3)]
                event={'kind':'shot','target':v,'side':a.side,'damage':dmg,'aim':aim,'gun':gun_index,
                       'weapon':w['kind'],'source_size':a.size,'flight':flight,'attacker':a,'mount':w,
                       'beam_hit':self.rng.random()<min(.98,(.86 if a.size<=v.size else .74)+bonus)*max(.3,a.eff('weapon'))}
                self.schedule(self.t+shot*C['salvo_interval'],{'kind':'launch','shot':event})

    def process_events(self):
        while self.events and self.events[0][0]<=self.t+1e-9:
            at,_,e=heapq.heappop(self.events);kind=e['kind']
            if kind=='launch':
                s=e['shot'];a=s['attacker'];v=s['target'];w=s['mount']
                if not a.alive() or a.side!=s['side'] or not a.combat_ready() or w['hp']<=0 or not a.eff(supply(w['kind'])):continue
                forced=self.force_weapons and self.aim=='weapons' and any(op['target'] is v for op in self.operations)
                if v.side==a.side or v.uid not in self.knowledge[a.side] or (self.protected(v,a.side) and not forced):continue
                if dist(a,v)>P['weapons'][w['kind']]['range']:continue
                v.last_combat=at
                self.metric['shots_launched']+=1
                self.schedule(at+s['flight'],s)
            elif kind=='shot':
                v=e['target'];weapon=e['weapon']
                if v.dead or v.escaped:continue
                if weapon in ('beam','heavy_beam'):
                    if not e['beam_hit']:continue
                else:
                    aim=list(e['aim'])
                    if weapon=='guided':
                        if e['flight']>P['weapons']['guided']['lifetime']:continue
                        # Finite guided correction, not perfect targeting or another dodge roll.
                        aim=[v.pos[i]+(aim[i]-v.pos[i])*.2 for i in range(3)]
                    if math.dist(aim,v.pos)>v.d['radius']:continue
                    if weapon in ('missile','guided') and not self.intercept(v,24,.25,min(e['flight'],111/P['weapons'][weapon]['speed']),'missile'):continue
                gun=e['gun']
                # Mount targeting uses a small hit region; deviation hits the hull instead.
                if gun is not None and self.rng.gauss(0,C['weapon_aim_sigma']*(1+dist(e['attacker'],v)/1000))**2>4:gun=None
                pen={('1','3'):.35,('1','4'):.15,('2','4'):.65}.get((str(min(e['source_size'],3)),str(v.size)),1)
                self.hit(v,e['damage'],e['side'],gun,penetration=pen)
            elif kind=='pod':self.pod_arrive(e)

    def repair_step(self,dt):
        claimed=set()
        for patient in [s for s in self.ships if s.alive() and s.hp<s.maxhp and s.eff('cargo')]:
            supporters=[u for u in patient.officers if u.kind=='support' and 3 in u.ranks and u.hp>0]
            if not supporters:continue
            u=max(supporters,key=lambda u:u.value(3,'officer_hull_fraction'))
            speed=patient.maxhp*u.value(3,'officer_hull_fraction')/10
            field_speed=max(64,.001*patient.maxhp)*.5
            linked=any(r.alive() and r.kind=='repair' and r.repair_target is patient and self.t>=r.repair_ready for r in self.ships)
            if linked and field_speed>=speed:continue
            if u.buff['repair_end']<=self.t and u.tools and self.t>=u.cd[3]:
                u.buff['repair_end']=self.t+10;u.cd[3]=self.t+90;u.tools-=1
            if u.buff['repair_end']<=self.t:continue
            amount=min(patient.maxhp-patient.hp,speed*dt)
            price=[b*.3*1.5/patient.maxhp for b in patient.d['build_cost']]
            amount=min([amount]+[patient.cargo[i]/unit for i,unit in enumerate(price) if unit])
            if amount<=0:continue
            for i,unit in enumerate(price):patient.cargo[i]-=unit*amount;self.metric[f'repair_resource_{i}']+=unit*amount
            patient.hp+=amount;claimed.add(patient.uid);self.metric['hero_repair_hp']+=amount
        for r in [s for s in self.ships if s.alive() and s.kind=='repair']:
            patients=[s for s in self.living(r.side) if s is not r and s.uid not in claimed and surface(s,r)<=120
                      and (s.hp<s.maxhp or any(f['hp']<f['maxhp'] for f in s.facilities.values()) or any(w['hp']<w['maxhp'] for w in s.guns))]
            if not patients:r.repair_target=None;continue
            patient=min(patients,key=lambda s:(s.hp/s.maxhp,s.uid));claimed.add(patient.uid)
            if r.repair_target is not patient:r.repair_target=patient;r.repair_ready=self.t+3
            if self.t<r.repair_ready or not r.eff('cargo'):continue
            jobs=[]
            for role in ('core','life','engine','weapon','pd','shield'):
                f=patient.facilities.get(role)
                if f and f['hp']<f['maxhp'] and (role in ('core','life') or patient.hp/patient.maxhp>=.35):jobs.append(('facilities',f,f['maxhp']-f['hp']))
            if patient.hp<patient.maxhp:jobs.append(('hull',patient,patient.maxhp-patient.hp))
            for role,f in patient.facilities.items():
                if f['hp']<f['maxhp']:jobs.append(('facilities',f,f['maxhp']-f['hp']))
            for w in patient.guns:
                if w['hp']<w['maxhp']:jobs.append(('external_weapons',w,w['maxhp']-w['hp']))
            damaged_armor=[(face,i) for face,vals in patient.armor.items() for i,hp in enumerate(vals) if hp<patient.armor_max[face][i]]
            if damaged_armor:jobs.append(('armor',damaged_armor[0],0))
            if not jobs:continue
            category,obj,missing=jobs[0];j=P['repair']['jobs'][category]
            speed=max(64,.001*patient.maxhp)*.5
            if category=='hull':den=patient.maxhp
            elif category=='facilities':den=sum(f['maxhp'] for f in patient.facilities.values())
            elif category=='external_weapons':den=sum(w['maxhp'] for w in patient.guns)
            else:
                den=sum(sum(x) for x in patient.armor_max.values());face,i=obj;missing=patient.armor_max[face][i]-patient.armor[face][i]
            work_to_part=den/(patient.maxhp*j['work_fraction'])
            amount=min(missing,speed*dt*work_to_part)
            price=[b*j['base_cost_fraction']*1.5/max(1,den) for b in patient.d['build_cost']]
            amount=min([amount]+[r.cargo[i]/unit for i,unit in enumerate(price) if unit>0])
            if amount<=0:self.metric['repair_starved']+=dt;continue
            for i,unit in enumerate(price):r.cargo[i]=max(0,r.cargo[i]-unit*amount);self.metric[f'repair_resource_{i}']+=unit*amount
            if category=='hull':patient.hp+=amount;self.metric['repair_hull_hp']+=amount
            elif category=='armor':face,i=obj;patient.armor[face][i]+=amount
            else:
                obj['hp']+=amount
                if obj['hp']>=obj['maxhp']*.25:
                    if 'destroyed' in obj:obj['destroyed']=False
                    if 'restart' in obj:obj['restart']=False
            # Crew is deliberately not restored by physical repair.
            self.metric['repair_work']+=amount/max(1e-9,work_to_part)
        for transport in [s for s in self.ships if s.alive() and s.kind=='transport' and s.eff('cargo')]:
            candidates=[s for s in self.living(transport.side) if s.kind=='repair' and surface(s,transport)<=60]
            if not candidates:transport.supply_ready=self.t+5;continue
            if self.t<transport.supply_ready:continue
            receiver=min(candidates,key=lambda s:sum(q*v for q,v in zip(s.cargo,(1,3,8))))
            volume=min(100*dt,5000-sum(q*v for q,v in zip(receiver.cargo,(1,3,8))))
            for i,unit in enumerate((1,3,8)):
                transfer=min(transport.cargo[i],volume/unit);transport.cargo[i]-=transfer;receiver.cargo[i]+=transfer;volume-=transfer*unit

    def fighter_step(self,dt):
        for carrier in [s for s in self.ships if s.alive() and s.fighters and s.eff('flight')]:
            if carrier.fighter_return:continue
            if self.t<carrier.fighter_next:continue
            carrier.fighter_next=self.t+C['fighter_period']
            target=self.pick(carrier)
            if not target:continue
            if self.protected(target,carrier.side):continue
            # Group damage accounting retains exact total losses, not individual flight dynamics.
            local_pd=self.pd_count(target)
            attempts=min(local_pd*C['fighter_period']/C['pd_period'],sum(s.pd_budget for s in self.living(target.side)))
            # Spend the same PD budget used by incoming missiles/pods.
            left=attempts
            for s in self.living(target.side):
                if surface(s,target)>111*s.d.get('pd_range_scale',1):continue
                spent=min(s.pd_budget,left);s.pd_budget-=spent;left-=spent
            carrier.fighter_loss_hp+=attempts*.72*(1-carrier.fighter_evasion)*C['pd_damage']
            opposing=sum(s.fighters for s in self.living(target.side) if s.eff('flight'))
            carrier.fighter_loss_hp+=min(carrier.fighters,opposing)*C['fighter_vs_fighter_dps']*C['fighter_period']*.25
            losses=min(carrier.fighters,int(carrier.fighter_loss_hp/carrier.fighter_hp));carrier.fighters-=losses;carrier.fighter_loss_hp-=losses*carrier.fighter_hp
            self.metric['fighter_losses']+=losses
            dmg=carrier.fighters*carrier.fighter_damage*.35
            self.hit(target,dmg,carrier.side,penetration=.35 if target.size>=3 else 1)

    def launch_operation(self):
        if self.mode=='none' or self.operations or self.t<C['boarding_start']:return
        source=next((s for s in self.living(0) if s.kind=='assault'),None)
        if not source:return
        contacts=self.knowledge[0]
        options=[s for s in self.living(1) if s.uid in contacts and not contacts[s.uid]['shield_active']]
        if not options:return
        # Target selection is based on visible hull class and distance, never defender counts.
        target=min(options,key=lambda s:(-s.size,surface(source,s),s.uid))
        if self.mode=='pod' and surface(source,target)>250:return
        units=[u for u in source.units if getattr(u,'assault',False)]
        if not units:return
        op={'side':0,'target':target,'source':source,'units':units,'phase':'flight','objective':False,
            'launch':self.t,'entry':None,'finish':None,'exit':None,'entry_node':None,'work':0.,'checkpoint':0.,
            'goal':target.layout.bridge if self.objective=='capture' else target.facilities.get('weapon',target.facilities['bridge'])['node'],
            'goal_role':'bridge' if self.objective=='capture' else 'weapon' if 'weapon' in target.facilities else 'bridge','mode':self.mode,'seen':set(),
            'original_target_side':target.side,'rescue':{},'guard_alarm':False,'return_started':False}
        self.operations.append(op);self.metric['operations_launched']+=1
        for u in units:source.units.remove(u);u.state='transit'
        if self.mode=='pod':
            for i,u in enumerate(units):
                self.schedule(self.t+i*.25+surface(source,target)/165,{'kind':'pod','unit':u,'op':op,'returning':False})
        else:op['arrival']=self.t+surface(source,target)/source.d['speed']+3

    def rescue(self,u,op):
        if not u.hero:u.hp=0;u.state='dead';self.metric['soldier_transit_dead']+=1;return
        u.state='rescue';op['rescue'][u.uid]=self.t;self.metric['hero_rescue_events']+=1

    def pod_arrive(self,e):
        u=e['unit'];op=e['op'];target=e.get('destination',op['source']) if e['returning'] else op['target']
        if u.hp<=0:return
        if target.dead or target.escaped or (e['returning'] and target.side!=u.side):
            choices=self.living(u.side)
            if e['returning'] and choices:
                destination=min(choices,key=lambda s:dist(s,target))
                self.schedule(self.t+max(.25,dist(destination,target)/165),{**e,'destination':destination});return
            elif not e['returning']:
                self.schedule(self.t+5,{'kind':'pod','unit':u,'op':op,'returning':True});return
            else:self.rescue(u,op);return
        # Friendly shields accept return docking; hostile active shields block ingress.
        hostile=target.side!=u.side
        survived=self.intercept(target,C['pod_hp'],.4,111/165,'pod') if hostile and not e.get('direct') else True
        if e['returning']:
            # Return flight traverses the hostile target's PD zone too.
            enemy_ship=op['target']
            if enemy_ship.alive() and enemy_ship.side!=u.side:
                survived=self.intercept(enemy_ship,C['pod_hp'],.4,111/165,'pod')
        if not survived or (hostile and target.active):self.rescue(u,op);return
        self.transport_log.append({'unit_id':u.uid,'hero':u.hero,'time':self.t,'returning':e['returning'],'direct':bool(e.get('direct')),'destination':target.uid})
        if e['returning']:
            u.state='aboard';u.node=target.layout.bridge;target.units.append(u);self.metric['returned_heroes' if u.hero else 'returned_soldiers']+=1;return
        if op['entry_node'] is None:
            perimeter=[n for n in target.layout.nodes if n%3 in (0,2)]
            op['entry_node']=self.board_rng.choice(perimeter);op['entry']=self.t
            self.metric['entry_at']=self.t;self.metric['launch_to_entry']=self.t-op['launch']
        u.node=op['entry_node'];u.edge=None;u.progress=0;u.state='inside';target.units.append(u)
        self.metric['entered_heroes' if u.hero else 'entered_soldiers']+=1;op['phase']='inside'

    def operations_step(self,dt):
        self.launch_operation()
        for op in self.operations:
            target=op['target'];source=op['source'];units=op['units']
            if op['mode']=='assault' and op['phase']=='flight' and self.t>=op['arrival']:
                if source.dead:
                    for u in units:
                        if u.state=='transit':self.rescue(u,op)
                    op['phase']='failed'
                elif target.dead or target.escaped:
                    for u in units:
                        if u.state=='transit':u.state='aboard';u.node=source.layout.bridge;source.units.append(u)
                    op['phase']='failed'
                elif target.active:op['arrival']=self.t+1
                else:
                    for u in units:self.pod_arrive({'unit':u,'op':op,'returning':False,'direct':True})
            alive=[u for u in units if u.alive() and u in target.units]
            if target.dead:
                for u in alive:u.hp=0;u.state='dead'
                continue
            if alive:
                op['seen'].update(u.node for u in alive)
                op['seen'].update(n for u in alive for n in target.layout.edges[u.node])
                # Interior structure becomes known after entry; enemy units remain locally visible.
                if not op['guard_alarm']:
                    for officer in target.officers:
                        officer.state='aboard';target.units.append(officer)
                    target.officers=[];op['guard_alarm']=True
                goal=op['goal'];enemy_at_goal=[u for u in target.units if u.alive() and u.side!=op['side'] and u.node==goal]
                at_goal=[u for u in alive if u.node==goal and u.edge is None]
                danger=target.hp/target.maxhp<C['boarding_abort_fraction'] or sum(u.hero for u in alive)<=1
                if danger and not op['objective']:op['phase']='return_walk';self.metric['aborted_operations']=1
                if at_goal and not enemy_at_goal and not op['objective'] and op['phase']!='return_walk':
                    bonus=sum(u.value(2,'occupation_bonus') if u.kind=='commander' and 2 in u.ranks else 0 for u in at_goal)
                    speed=min(2,1+.12*(len(at_goal)-1)+bonus)
                    op['work']+=dt*speed
                    required=36 if self.objective=='capture' else 30 if self.objective=='sabotage' else 18
                    if self.objective=='sabotage':
                        checkpoint=math.floor(op['work']/(required/3))*(required/3);op['checkpoint']=max(op['checkpoint'],min(required*2/3,checkpoint))
                    if op['work']>=required:
                        f=target.facilities.get(op['goal_role'])
                        if f and not f['destroyed']:
                            if self.objective=='sabotage':self.destroy_facility(target,op['goal_role'],'sabotage')
                            else:f['owner']=op['side'];f['occupied']=True
                            if self.objective=='capture':self.change_owner(target,op['side'])
                            self.metric['objectives']+=1
                        op['objective']=True;op['finish']=self.t;op['phase']='return_walk'
                        self.metric['entry_to_objective']=self.t-op['entry']
                elif not at_goal or enemy_at_goal:op['work']=op['checkpoint']
                # Each person must physically reach the original entry point before extraction.
                if op['phase']=='return_walk':
                    for u in alive:
                        u.state='return_walk'
                        if u.node==op['entry_node'] and u.edge is None:
                            if op['mode']=='assault':
                                waits=op.setdefault('pickup_wait',{})
                                if source.alive() and source.side==u.side:
                                    ready=waits.setdefault(u.uid,self.t+3+surface(source,target)/max(1,source.d['speed']))
                                    if self.t<ready:continue
                                    target.units.remove(u);u.state='aboard';u.node=source.layout.bridge
                                    source.units.append(u);source.extracting=True
                                    self.metric['returned_heroes' if u.hero else 'returned_soldiers']+=1
                                else:
                                    ready=waits.setdefault(u.uid,self.t+C['emergency_exit_seconds'])
                                    if self.t<ready:continue
                                    target.units.remove(u);self.rescue(u,op)
                                continue
                            target.units.remove(u);u.state='transit'
                            self.schedule(self.t+surface(source,target)/165,{'kind':'pod','unit':u,'op':op,'returning':True})
                    remaining=[u for u in units if u.hp>0 and u in target.units]
                    if not remaining and op['exit'] is None:
                        op['exit']=self.t;self.metric['entry_to_exit']=self.t-op['entry'];op['phase']='extracted'
            for uid,since in list(op['rescue'].items()):
                unit=next(u for u in units if u.uid==uid)
                if unit.state!='rescue':continue
                candidates=[s for s in self.ships if s.alive() and surface(s,target)<=150]
                if candidates and self.t-since>=5:
                    winner=min(candidates,key=lambda s:(surface(s,target),s.uid))
                    if winner.side==unit.side:unit.state='aboard';unit.node=winner.layout.bridge;winner.units.append(unit);self.metric['rescued_heroes']+=1
                    else:unit.state='captive';self.metric['captured_heroes']+=1
                elif self.t-since>=120:unit.state='dead';unit.hp=0;self.metric['rescue_expired']+=1

    def indoor(self,dt):
        for ship in self.ships:
            if ship.dead:continue
            if not any(u.alive() and u.side!=ship.side for u in ship.units):continue
            goals={}
            for op in self.operations:
                if op['target'] is ship:
                    goals[op['side']]=(op['entry_node'] if op['phase']=='return_walk' else op['goal'],op['phase']=='return_walk')
                    goals[op['original_target_side']]=(op['goal'],False)
            n=max(1,round(dt/C['indoor_dt']))
            for i in range(n):indoor_step(ship,self.t+i*dt/n,dt/n,self.board_rng,goals,self.metric)
            # Retaking requires living defenders physically clearing/occupying the room.
            for role,f in ship.facilities.items():
                if f['destroyed']:continue
                owners={u.side for u in ship.units if u.alive() and u.node==f['node']}
                side=next(iter(owners)) if len(owners)==1 else None
                if side is None or side==f['owner']:f['retake']=0;continue
                # Defenders can reclaim their ship. Attackers require their explicit operation goal.
                if side!=ship.side and not (ship.controlled and role=='bridge' and side==ship.original_side):continue
                if f.get('retake_side')!=side:f['retake']=0;f['retake_side']=side
                f['retake']=f.get('retake',0)+dt
                # The attacking operation owns its objective progress; do not run a second clock.
                if any(op['target'] is ship and op['side']==side and op['goal']==f['node'] and not op['objective'] for op in self.operations):continue
                if f['retake']>=(36 if role=='bridge' else 18):
                    f['owner']=side;f['occupied']=True;f['retake']=0;self.metric['actual_retakes']+=1
                    if role=='bridge':self.change_owner(ship,side)

    def change_owner(self,ship,side):
        if ship.dead:return
        ship.side=side;ship.controlled=True;ship.target=None;ship.ftl=None;ship.order_until=self.t+2;ship.vel=[0.,0.,0.]
        self._live_cache=None;self._radar_cache={}
        for role,f in ship.facilities.items():
            if not f.get('occupied') or role=='bridge':f['owner']=side
        self.metric['captures']+=1

    def environment(self,dt):
        for s in self.ships:
            if not s.alive():continue
            for f in s.facilities.values():
                if f.get('hazard_end',0)<=self.t:continue
                for u in s.units:
                    if not u.alive() or u.node!=f['node']:continue
                    resistance=max([v.value(2,'hazard_reduction') for v in s.units+s.officers if v.hp>0 and v.kind=='support' and 2 in v.ranks and v.node==u.node]+[0])
                    u.hp=max(0,u.hp-C['hazard_damage_per_second']*(1-resistance)*dt)
                    if u.hp==0:u.state='dead'
            if not s.eff('life'):
                if s.life_since is None:s.life_since=self.t
                elapsed=self.t-s.life_since
                if elapsed>=C['life_support_grace']:
                    for f in s.facilities.values():f['crew']=max(0,f['crew']-dt/(C['life_support_death_after']-C['life_support_grace']))
                    for u in s.units:
                        if not u.alive():continue
                        resistance=max([v.value(2,'hazard_reduction') for v in s.units+s.officers if v.hp>0 and v.kind=='support' and 2 in v.ranks and v.node==u.node]+[0])
                        u.hp=max(0,u.hp-C['hazard_damage_per_second']*(1-resistance)*dt)
                        if u.hp==0 or elapsed>=C['life_support_death_after']:u.hp=0;u.state='dead'
            else:s.life_since=None
            if not any(f['crew']>0 for role,f in s.facilities.items() if role!='core') and not any(u.hp>0 and u.side==s.side for u in s.units+s.officers):
                s.side=-1;s.target=None;self._live_cache=None

    def tick(self,dt):
        self._live_cache=None;self._radar_cache={}
        self.observe()
        for s in self.ships:
            if not s.alive():continue
            s.now=self.t;s.shield_tick(self.t,dt)
            s.pd_budget=min(s.d['pd']*s.eff('pd')*2,s.pd_budget+s.d['pd']*s.eff('pd')*dt/.5)
            self.update_motion(s,dt)
            if s.ftl is not None and s.facilities['engine']['destroyed']:s.ftl=None
            if s.ftl is not None and self.t>=s.ftl:
                if s.fighters and s.fighter_docked<s.fighters:
                    if s.fighter_return_at is None:s.fighter_return_at=self.t+10
                    if self.t<s.fighter_return_at:continue
                    s.fighter_docked=s.fighters
                s.escaped=True;self._live_cache=None;self.metric['escaped_cost_'+str(s.side)]+=s.d['fleet_cost']
        # Alternate evaluation order per tick; hits are queued so beams do not gain first-side initiative.
        order=list(self.ships)
        if (int(self.t)%2==0)^self.reverse:order.reverse()
        for a in order:
            if not a.alive():continue
            if a.target is None or not a.target.alive() or a.target.side==a.side or a.target.uid not in self.knowledge[a.side] or self.protected(a.target,a.side) or self.t>=a.target_until:
                a.target=self.pick(a);a.target_until=self.t+C['target_switch_seconds']
            self.fire(a,a.target)
        self.process_events();self.fighter_step(dt);self.operations_step(dt);self.indoor(dt);self.environment(dt);self.repair_step(dt)
        for side in (0,1):
            remaining=sum(s.d['fleet_cost'] for s in self.living(side))
            if self.main_end is None and remaining<=self.initial[side]*.3:self.main_end=self.t
            if self.retreat and self.t>90 and remaining<=self.initial[side]*C['retreat_fraction']:
                for s in self.living(side):
                    if s.ftl is None and s.eff('engine'):s.ftl=self.t+C['ftl_seconds'];s.fighter_return=True
        self.t+=dt

    def result(self):
        live=[sum(s.d['fleet_cost'] for s in self.living(side)) for side in (0,1)]
        ready=[sum(s.combat_ready() for s in self.living(side)) for side in (0,1)]
        if not live[0] and not live[1]:outcome='mutual';winner=None
        elif not live[0]:outcome='retreat' if self.metric['escaped_cost_0'] else 'defeat';winner=1
        elif not live[1]:outcome='retreat' if self.metric['escaped_cost_1'] else 'victory';winner=0
        elif not any(ready):outcome='stalemate';winner=None
        else:outcome='timeout';winner=None
        hero_states=Counter();seen=set()
        all_units=[u for s in self.ships for u in s.units+s.officers]+[u for op in self.operations for u in op['units']]
        for u in all_units:
            if not u.hero or u.uid in seen:continue
            seen.add(u.uid);hero_states[f'{u.side}:{u.state}']+=1
        return {'seed':self.seed,'seconds':round(self.t,3),'main_seconds':self.main_end,'first_loss_seconds':self.first_loss,
                'winner':winner,'outcome':outcome,'remaining_cost':live,'initial_cost':self.initial,'initial_heroes':self.initial_heroes,
                'initial_fighters':self.initial_fighters,'fighter_package_cost':self.fighter_package_cost,
                'hero_states':dict(hero_states),'metrics':dict(self.metric),'damage_by_target_size':{str(k):v for k,v in self.damage_by_size.items()},
                'mounts_remaining':[sum(w['hp']>0 for s in self.living(side) for w in s.guns) for side in (0,1)],
                'transport_log':self.transport_log,
                'operation_timeline':[{'launch':op['launch'],'entry':op['entry'],'objective':op['finish'],'exit':op['exit'],
                                       'target_id':op['target'].uid,'target_kind':op['target'].kind,'target_wreck_at':getattr(op['target'],'wreck_at',None),
                                       'observed_survival_after_entry':None if op['entry'] is None else max(0,min(self.t,getattr(op['target'],'wreck_at',self.t))-op['entry']),
                                       'units':[{'id':u.uid,'kind':u.kind,'state':u.state,'hp':u.hp} for u in op['units']]} for op in self.operations],
                'target_survival_after_entry':next((None if not op['target'].dead or op['entry'] is None else max(0,op['target'].wreck_at-op['entry']) for op in self.operations),None),
                'target_wreck_before_exit':any(op['entry'] is not None and op['target'].dead and (op['exit'] is None or op['target'].wreck_at<op['exit']) for op in self.operations)}

    def run(self,dt=None):
        dt=dt or C['dt']
        while self.t<self.duration:
            self.tick(dt)
            if not self.living(0) or not self.living(1):break
            if self.t>90 and not any(s.combat_ready() for s in self.ships) and not any(u.alive() and u.side!=s.side for s in self.ships for u in s.units) and not self.events:break
        return self.result()
