"""Persistent units on a temporary room graph; no imported/precomputed battle outcomes."""
import math
from collections import defaultdict


class Layout:
    """Five longitudinal stations, three lateral slots, 1–3 decks; metres."""
    def __init__(self, decks, radius, scale=1.0):
        self.decks = decks
        self.length = max(6, radius * 0.65) * scale
        self.width = max(4, radius * 0.25) * scale
        self.height = 6 * scale
        self.nodes = list(range(15 * decks))
        self.core = (decks // 2) * 15 + 7
        self.bridge = (decks - 1) * 15 + 1
        self.edges = {n: self.neighbors(n) for n in self.nodes}

    def xyz(self, n):
        deck, q = divmod(n, 15)
        row, col = divmod(q, 3)
        return ((row - 2) * self.length, (col - 1) * self.width, deck * self.height)

    def neighbors(self, n):
        deck, q = divmod(n, 15)
        row, col = divmod(q, 3)
        out = []
        for d, r, c in ((deck,row-1,col),(deck,row+1,col),(deck,row,col-1),(deck,row,col+1)):
            if 0 <= r < 5 and 0 <= c < 3:
                out.append(d * 15 + r * 3 + c)
        if row == 2 and col == 1:
            out += [d * 15 + 7 for d in (deck-1,deck+1) if 0 <= d < self.decks]
        return out

    def distance(self, a, b):
        da, qa = divmod(a, 15); db, qb = divmod(b, 15)
        ra, ca = divmod(qa, 3); rb, cb = divmod(qb, 3)
        if da == db:
            return abs(ra-rb)*self.length + abs(ca-cb)*self.width
        return (abs(ra-2)+abs(rb-2))*self.length + (abs(ca-1)+abs(cb-1))*self.width + abs(da-db)*self.height

    def next_node(self, a, b):
        return min(self.edges[a], key=lambda n: self.distance(a,n)+self.distance(n,b))


class Unit:
    def __init__(self, uid, kind, side, node, profile, level=1, gear=0):
        self.uid=uid; self.kind=kind; self.side=side; self.node=node
        self.edge=None; self.progress=0.; self.level=level; self.gear=gear
        self.hero=kind!='soldier'; self.state='aboard'; self.next_attack=0.
        self.cd=defaultdict(float); self.buff=defaultdict(float)
        self.aim_target=None; self.aim_since=0.; self.last_stand_armed=True
        h=profile['heroes']; self.h=h
        d=h['classes'][kind] if self.hero else profile['regular']
        self.d=d
        hp_growth=1+h['hp_growth_per_level']*(level-1) if self.hero else 1
        atk_growth=1+h['attack_growth_per_level']*(level-1) if self.hero else 1
        eq=h['gear_tiers']; hp_eq=eq['armor_hp'][gear-1] if gear and self.hero else 0
        atk_eq=eq['weapon_attack'][gear-1] if gear and self.hero else 0
        self.maxhp=d['hp']*hp_growth*(1+hp_eq); self.hp=self.maxhp
        self.damage=d['damage']*atk_growth*(1+atk_eq)
        self.armor=d['armor']+(eq['armor_reduction'][gear-1] if gear and self.hero else 0)
        self.speed=d['speed']; self.slow_until=0.; self.slow=0.
        self.ranks={i:1 for i,unlock in enumerate(h['skill_unlock_levels'],1) if level>=unlock} if self.hero else {}
        points=sum(level>=l for l in h['skill_point_levels']) if self.hero else 0
        for i in (1,3,2,4):
            if i in self.ranks:
                spend=min(2,points); self.ranks[i]+=spend; points-=spend
        # Only the recruitment package contains four catalysts. Passive capacity is not free refill.
        self.medkits=h['initial_support_medkits'] if kind=='support' else 0
        self.tools=2 if kind=='support' and level>=10 else 0  # explicitly loaded at base in scenarios

    def alive(self):
        return self.hp>0 and self.state in ('aboard','inside','return_walk')

    def skill(self, i):
        return self.h['skills'][f'{self.kind}{i}']

    def value(self, i, key):
        v=self.skill(i)[key]
        return v[self.ranks[i]-1] if isinstance(v,list) else v


def position(u, layout):
    a=layout.xyz(u.node)
    if u.edge is None:return a
    b=layout.xyz(u.edge); length=math.dist(a,b)
    return tuple(x+(y-x)*min(1,u.progress/max(length,1e-9)) for x,y in zip(a,b))


def distance(a,b,layout):
    # A closed/bent multi-room path is not a clear line of fire.
    if a.node!=b.node and b.node not in layout.edges[a.node]:return math.inf
    return math.dist(position(a,layout),position(b,layout))


def move(u, goal, ship, dt, speed_multiplier=1.):
    if u.node==goal and u.edge is None:return
    layout=ship.layout
    if u.edge is None:
        u.edge=layout.next_node(u.node,goal); u.progress=0.
    speed=u.speed*speed_multiplier
    if ship.now<u.slow_until:speed*=1-u.slow
    u.progress+=speed*dt
    length=math.dist(layout.xyz(u.node),layout.xyz(u.edge))
    if u.progress>=length:
        u.node=u.edge;u.edge=None;u.progress=0.


def indoor_step(ship, now, dt, rng, goals, metrics):
    """One simultaneous attack step. All decisions use local unit/room visibility."""
    ship.now=now;layout=ship.layout
    units=[u for u in ship.units if u.alive()]
    damage=defaultdict(float)
    fields=[]
    for u in units:
        if u.kind=='support' and u.buff['field_end']>now:fields.append(u)
    for u in units:
        friends=[v for v in units if v.side==u.side]
        enemies=[v for v in units if v.side!=u.side]
        visible=[v for v in enemies if distance(u,v,layout)<=max(u.d['range'],18)]
        # The retreat goal takes priority; being attacked does not silently cancel it.
        goal,retreat=goals.get(u.side,(layout.bridge,False))
        if retreat:
            bonus=max([v.value(4,'retreat_speed') for v in friends if v.kind=='commander' and 4 in v.ranks and v.node==u.node]+[0])
            move(u,goal,ship,dt,1+min(.2,bonus));continue
        if u.kind=='support':
            candidates=[v for v in friends if distance(u,v,layout)<=u.d['range'] and v.hp/v.maxhp<=.5]
            if candidates and now>=u.cd[1] and u.medkits:
                patient=min(candidates,key=lambda v:(v.hp/v.maxhp,v.uid))
                heal=u.value(1,'heal')*(1+u.h['hp_growth_per_level']*(u.level-1))
                if u.gear:heal*=1+u.h['gear_tiers']['tactical'][u.gear-1]
                actual=min(patient.maxhp-patient.hp,heal);patient.hp+=actual
                u.medkits-=1;u.cd[1]=now+u.value(1,'cooldown');metrics['hero_heal']+=actual;metrics['catalysts_used']+=1
            if 3 in u.ranks and len(candidates)>=2 and u.medkits>=2 and now>=u.cd[3]:
                u.medkits-=2;u.cd[3]=now+90;u.buff['field_end']=now+5;fields.append(u);metrics['catalysts_used']+=2
        if u.kind=='commander' and 3 in u.ranks and visible and now>=u.cd[3]:
            u.buff['command_end']=now+8;u.cd[3]=now+60
        if u.kind=='melee':
            if visible and now>=u.cd[1]:u.buff['brace_end']=now+4;u.cd[1]=now+16
            if u.hp/u.maxhp>=.4:u.last_stand_armed=True
            if 4 in u.ranks and u.hp/u.maxhp<.25 and u.last_stand_armed and now>=u.cd[4]:
                u.buff['stand_end']=now+3;u.cd[4]=now+120;u.last_stand_armed=False
        if not visible:
            u.aim_target=None;u.aim_since=now
            move(u,goal,ship,dt);continue
        target=min(visible,key=lambda v:(distance(u,v,layout),v.uid))
        dist=distance(u,target,layout)
        if dist>u.d['range']:
            dash=1.
            if u.kind=='melee' and 3 in u.ranks:
                if dist<=u.value(3,'range') and now>=u.cd[3]:
                    u.buff['dash_end']=now+2;u.buff['dash_attack']=1;u.cd[3]=now+30
                if u.buff['dash_end']>now:dash=1.5
            move(u,target.node,ship,dt,dash);continue
        # Limited fighting slots per side/room, with overflow waiting behind the line.
        front=u.kind=='melee'
        capacity=2 if front else (2 if ship.size==1 else 4 if ship.size==2 else 5)
        in_line=sorted([v for v in friends if v.node==u.node and (v.kind=='melee')==front],key=lambda v:(not v.hero,v.uid))
        if u not in in_line[:capacity]:continue
        if now<u.next_attack:continue
        commanders=[v for v in friends if v.kind=='commander' and distance(u,v,layout)<=14]
        aura=max([v.value(1,'damage_bonus') for v in commanders]+[0])
        haste=max([v.value(3,'attack_speed') for v in commanders if 3 in v.ranks and v.buff['command_end']>now]+[0])
        u.next_attack=now+u.d['period']/(1+haste)
        hit=u.damage*(1+aura);ignore=0.
        if u.kind=='ranged':
            if u.aim_target!=target.uid:u.aim_target=target.uid;u.aim_since=now
            if 2 in u.ranks:ignore=u.value(2,'ignore_reduction')
            if 3 in u.ranks and now>=u.cd[3]:
                # Three simultaneous expected shots approximate the documented 0.6s burst.
                hit=u.damage*(1+aura)*1.5;target.slow=u.value(3,'slow');target.slow_until=now+4;u.cd[3]=now+30
                u.next_attack=max(u.next_attack,now+.6)
            elif now>=u.cd[1]:hit*=u.value(1,'attack_multiplier');u.cd[1]=now+12
            if 4 in u.ranks and now-u.aim_since>=4:hit*=1+u.value(4,'attack_bonus')
        if u.kind=='melee' and u.buff['dash_attack']:
            hit*=1+u.value(3,'attack_bonus');u.buff['dash_attack']=0
        reduction=target.armor
        if target.kind=='melee':
            if target.buff['brace_end']>now:reduction+=target.value(1,'extra_reduction')
            if target.buff['stand_end']>now:reduction+=target.value(4,'extra_reduction')
        cover=max([v.value(2,'ally_reduction') for v in enemies if v is not target and v.kind=='melee' and 2 in v.ranks and distance(target,v,layout)<=3]+[0])
        reduction=max(0,min(.65,reduction+cover)-ignore)
        if rng.random()<.92*(1-target.d['evasion']):damage[target.uid]+=hit*(1-reduction)
    for u in units:
        healing=max([v.value(3,'heal_fraction_per_second') for v in fields if v.side==u.side and distance(u,v,layout)<=6]+[0])
        amount=min(u.maxhp-u.hp,u.maxhp*healing*dt);u.hp+=amount;metrics['hero_heal']+=amount
    for u in units:
        u.hp=max(0,u.hp-damage[u.uid])
        if u.hp==0:u.state='dead'
