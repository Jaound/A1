"""D123/D124 isolated-environment extension of the historical fleet design model.
No UE/mesh certification. Exactly one environmental hazard per battle.
"""
from __future__ import annotations
import copy
import heapq
import math
import random
import sys
from pathlib import Path
from types import MethodType

BASE = Path(__file__).resolve().parents[1] / "IntegratedCombat"
sys.path.insert(0, str(BASE))
import model as legacy

# Independent process-local overlay: never rewrite the historical profile.
legacy.P = copy.deepcopy(legacy.P)
legacy.P["ships"]["battlecarrier"]["weapons"] = (
    [w for w in legacy.P["ships"]["battlecarrier"]["weapons"] if w["kind"] != "spinal"]
    + [{"kind": "spinal", "tier": 4, "mount": mount} for mount in ("top", "bottom")]
)
legacy.P["ships"]["carrier"]["max_hangars"] = 4
legacy.P["ships"]["battlecarrier"]["max_hangars"] = 8
legacy.C = legacy.P["integration"]
ORIGINAL_EFF = legacy.Ship.eff
ORIGINAL_SHIELD = legacy.Ship.shield_tick

def add(a, b): return [x+y for x, y in zip(a, b)]
def sub(a, b): return [x-y for x, y in zip(a, b)]
def mul(a, k): return [x*k for x in a]
def norm(a): return math.sqrt(sum(x*x for x in a))
def unit(a):
    n = norm(a)
    return mul(a, 1/n) if n > 1e-9 else [1., 0., 0.]
def clamp_length(a, maximum):
    n = norm(a)
    return mul(a, maximum/n) if n > maximum else a

def segment_sphere(start, end, center, radius):
    d, f = sub(end, start), sub(start, center)
    aa = sum(x*x for x in d)
    cc = sum(x*x for x in f)-radius*radius
    if cc <= 0: return 0.
    if aa < 1e-12: return None
    bb = 2*sum(x*y for x, y in zip(f, d))
    disc = bb*bb-4*aa*cc
    if disc < 0: return None
    t = (-bb-math.sqrt(disc))/(2*aa)
    return t if 0 <= t <= 1 else None

def eff(ship, role):
    if ship.now < getattr(ship, "emp_until", 0) and role not in ("core", "life"):
        return 0.
    value = ORIGINAL_EFF(ship, role)
    if role == "radar" and ship.now < getattr(ship, "radar_until", 0):
        value *= getattr(ship, "radar_scale", 1.)
    return value

def shield_tick(ship, now, dt):
    # Skip, rather than eff(shield)==0 wiping the accumulated gauge each frame.
    if now < getattr(ship, "emp_until", 0): return
    ORIGINAL_SHIELD(ship, now, dt)

DEFAULTS = {
    "none": {},
    "asteroid": {"hp_50m": 1800., "collision_hp_fraction": .006, "collision_interval": 4.,
                 "radii": [45., 65., 85.], "avoid_margin": 18.},
    "radiation": {"radius": 850., "peak_shield_fraction_per_second": .006,
                  "falloff_power": 1.5, "resistance": 0.},
    "flare": {"warning": 45., "period": 210., "speed": 1200., "thickness": 60.,
              "shield_fraction": .35, "emp_seconds": 3., "radar_seconds": 6.,
              "radar_scale": .5, "cover_radius": 120.},
    "blackhole": {"radius": 1000., "core_radius": 45., "peak_acceleration": .8,
                  "falloff_power": 2., "propulsion_acceleration": [5., 3., 1.2, .6],
                  "surface_contact": True},
}

class EnvironmentBattle(legacy.Battle):
    def __init__(self, left, right, seed=0, *, hazard="none", parameters=None,
                 avoidance=True, radiation_resistance=0., **kwargs):
        if not isinstance(hazard, str) or hazard not in DEFAULTS:
            raise ValueError("Choose exactly one of none/asteroid/radiation/flare/blackhole")
        self.hazard = hazard
        self.params = {**DEFAULTS[hazard], **(parameters or {})}
        self.avoidance = avoidance
        self.env_rng = random.Random(seed+81000001)
        self.bodies = []
        self.env_events = []
        self.pods = []
        self.env_sequence = 0
        self.pulse_seen = set()
        self.pulse_air_seen = set()
        self.pulse_pod_seen = set()
        self.collision_until = {}
        self.env_trace = []
        self.aircraft = {}
        self.no_env_seed = seed
        super().__init__(left, right, seed, **kwargs)
        for ship in self.ships:
            if ship.kind == "assault":
                # Dedicated shuttle has no free 12-person guard garrison.
                ship.units = [u for u in ship.units if u.hero or getattr(u,"assault",False)]
                soldiers = [u for u in ship.units if not u.hero]
                for u in soldiers[6:]: ship.units.remove(u)
        self.rad_resistance = radiation_resistance
        self.wave_direction = 1 if seed % 2 == 0 else -1
        self.gravity_center = [0., 160., 0.]
        self.setup_bodies()
        self.initial_unit_ids = set()
        for ship in self.ships:
            ship.emp_until = ship.radar_until = 0.
            ship.radar_scale = 1.
            ship.gravity_velocity = [0., 0., 0.]
            ship.eff = MethodType(eff, ship)
            ship.shield_tick = MethodType(shield_tick, ship)
            self.initial_unit_ids.update(u.uid for u in ship.units+ship.officers)
            # Individual aircraft IDs and hazard locations; combat remains aggregate.
            wing = []
            for i in range(ship.fighters):
                wing.append({"id": f"{ship.uid}:{i}", "hp": legacy.C["fighter_hp"],
                             "shield": 75., "active": True, "pos": list(ship.pos),
                             "emp_until": 0., "phase": i*2.399963, "docked": False})
            if wing: self.aircraft[ship.uid] = wing
        self.initial_air_ids = {a["id"] for wing in self.aircraft.values() for a in wing}

    def setup_bodies(self):
        if self.hazard == "asteroid":
            # Mirrored geometry: neither side gets a free starting obstacle.
            positions = [(0., -240., 0.), (0., 20., 70.), (0., 270., -50.),
                         (-160., 150., 0.), (160., 150., 0.)]
            for i, pos in enumerate(positions):
                radius = self.params["radii"][i % len(self.params["radii"])]
                self.bodies.append({"id": i, "pos": list(pos), "radius": radius,
                                    "hp": self.params["hp_50m"]*(radius/50.)**3,
                                    "destroyed": False})
        elif self.hazard == "flare" or self.params.get("static_shelter", False):
            # Static shelter geometry only: no second asteroid hazard or collision damage.
            for i, (x, y) in enumerate(((-620., -150.), (-180., 200.), (180., 200.), (620., -150.))):
                self.bodies.append({"id": i, "pos": [x, y, 0.],
                                    "radius": self.params["cover_radius"],
                                    "hp": math.inf, "destroyed": False})

    def samples(self, ship):
        r = ship.d["radius"]
        for face, values in ship.armor.items():
            for i in range(len(values)):
                n = len(values); t = ((i+.5)/n-.5)*1.6
                if face == "front": offset = [r, 0., 0.]
                elif face == "rear": offset = [-r, 0., 0.]
                elif face == "left": offset = [t*r, -.4*r, ((i%ship.d["decks"])-.5*(ship.d["decks"]-1))*6]
                elif face == "right": offset = [t*r, .4*r, ((i%ship.d["decks"])-.5*(ship.d["decks"]-1))*6]
                elif face == "top": offset = [t*r, ((i%3)-1)*.3*r, .3*r]
                else: offset = [t*r, ((i%3)-1)*.3*r, -.3*r]
                yield face, i, add(ship.pos, offset)

    def blockers(self, start, end, expansion=0.):
        choices = []
        for body in self.bodies:
            if body["destroyed"]: continue
            t = segment_sphere(start, end, body["pos"], body["radius"]+expansion)
            if t is not None: choices.append((t, body["id"], body))
        return min(choices, default=None, key=lambda x: (x[0], x[1]))

    def exposed_samples(self, ship):
        samples = list(self.samples(ship))
        unblocked = []
        for face, cell, point in samples:
            sunward = add(point, [-self.wave_direction*4000., 0., 0.])
            if self.blockers(sunward, point) is None: unblocked.append((face, cell, point))
        return unblocked, len(samples)

    def face_node(self, ship, face, cell):
        # Deterministic exposed-face mapping onto the legacy temporary room graph.
        if face in ("top", "bottom"):
            return (ship.d["decks"]-1 if face == "top" else 0)*15 + min(14, cell*15//len(ship.armor[face]))
        if face in ("front", "rear"):
            return (ship.d["decks"]-1)*15 + (1 if face == "front" else 13)
        deck = min(ship.d["decks"]-1, cell//3)
        row = min(4, (cell%3)*2)
        return deck*15+row*3+(0 if face == "left" else 2)

    def damage_surface(self, ship, amount, face, cell, cause):
        if not ship.alive() or amount <= 0: return
        before = ship.hp
        ship.last_combat = self.t
        self.last_damage = self.t
        self.metric["env_damage_input_"+cause] += amount
        if ship.active:
            spill = max(0., amount-ship.shield)
            absorbed = min(amount, ship.shield)
            ship.shield = max(0., ship.shield-amount)
            self.metric["env_shield_damage_"+cause] += absorbed
            if ship.shield <= 0: ship.active = False
            amount = spill*legacy.P["global"]["overflow_fraction"]
        if amount <= 0: return
        remaining = ship.armor[face][cell]/ship.armor_max[face][cell]
        passed = amount*(1-(.6 if face == "front" else .45)*remaining)
        ship.armor[face][cell] = max(0., ship.armor[face][cell]-amount*.45)
        node = self.face_node(ship, face, cell)
        role = ship.by_node.get(node)
        if role:
            facility = ship.facilities[role]
            facility["hp"] = max(0., facility["hp"]-passed)
            if role != "core":
                facility["crew"] = max(0., facility["crew"]-passed/facility["maxhp"]*legacy.C["crew_injury_fraction"])
            if facility["hp"] <= 0: self.destroy_facility(ship, role, cause)
        ship.hp = max(0., ship.hp-passed*.65)
        self.metric["env_hull_damage_"+cause] += min(before, before-ship.hp)
        for u in ship.units:
            if u.alive() and u.node == node:
                u.hp = max(0., u.hp-min(u.maxhp*.35, passed*.015))
                if u.hp == 0: u.state = "dead"
        if ship.hp <= 0: self.wreck(ship, cause)

    def destroy_facility(self, ship, role, origin="external", chain=None):
        old = getattr(self,"environment_damage_origin",None)
        if origin in ("radiation","flare","asteroid_collision"):
            self.environment_damage_origin = origin
        try:
            return super().destroy_facility(ship,role,origin,chain)
        finally:
            self.environment_damage_origin = old

    def wreck(self, ship, cause):
        if ship.dead: return
        root = getattr(self,"environment_damage_origin",None)
        if root and not cause.startswith("env_"): cause = "env_"+root+"_"+cause
        elif cause in ("radiation","flare","asteroid_collision"): cause = "env_"+cause
        # External aircraft survive the carrier wreck as control-lost records.
        outside = [a for a in self.aircraft.get(ship.uid, []) if a["hp"] > 0 and not a["docked"]] if hasattr(self, "aircraft") else []
        super().wreck(ship, cause)
        ship.ftl = None
        for a in self.aircraft.get(ship.uid, []):
            if a["docked"]: a["hp"] = 0
        ship.fighters = len(outside)
        if cause.startswith("env_"): self.metric["environment_wrecks"] += 1

    def schedule(self, at, event):
        if event["kind"] == "pod":
            op = event["op"]
            source, target = (op["target"], op["source"]) if event["returning"] else (op["source"], op["target"])
            self.pods.append({"event": event, "pos": list(source.pos), "target": target,
                              "hp": legacy.C["pod_hp"], "emp_until": 0., "done": False,
                              "id": f'{event["unit"].uid}:{int(event["returning"])}'})
            return
        if event["kind"] == "shot" and self.bodies:
            start = list(event["attacker"].pos)
            block = self.blockers(start, event["aim"])
            if block is not None:
                fraction, _, body = block
                self.env_sequence += 1
                arrival = self.t+max(0., (at-self.t)*fraction)
                heapq.heappush(self.env_events, (arrival, self.env_sequence, body, event, at))
                self.metric["env_blocked_shots"] += 1
                return
        super().schedule(at, event)

    def process_events(self):
        self.process_environment_impacts()
        super().process_events()
        self.process_environment_impacts()

    def process_environment_impacts(self):
        while self.env_events and self.env_events[0][0] <= self.t+1e-9:
            _, _, body, event, original_at = heapq.heappop(self.env_events)
            if body["destroyed"]:
                # A removed obstacle cannot absorb an attack later in its flight.
                super().schedule(max(self.t, original_at), event)
                continue
            if self.hazard == "asteroid":
                body["hp"] -= event["damage"]
                self.metric["env_asteroid_damage"] += event["damage"]
                if body["hp"] <= 0:
                    body["destroyed"] = True
                    self.metric["env_asteroids_destroyed"] += 1
                    self.metric["env_topology_version"] += 1

    def update_motion(self, ship, dt):
        old = list(ship.pos)
        old_velocity = list(ship.gravity_velocity)
        super().update_motion(ship, dt)
        desired_velocity = list(ship.vel)
        proposed = list(ship.pos)
        ship.pos = old
        if self.hazard == "blackhole":
            center = self.gravity_center
            offset = sub(old, center)
            distance = norm(offset)
            radius = self.params["radius"]
            own_acceleration = [0., 0., 0.]
            thrust = self.params["propulsion_acceleration"][ship.size-1]*ORIGINAL_EFF(ship,"engine")
            safe_radius = max(self.params["core_radius"]+ship.d["radius"]+80.,
                self.params["core_radius"]+(1-math.sqrt(min(1.,.7*thrust/max(.001,self.params["peak_acceleration"]))))*(radius-self.params["core_radius"]))
            incoming_speed = max(0.,-sum(v*n for v,n in zip(old_velocity,unit(offset))))
            local_gravity = self.params["peak_acceleration"]*max(0.,1-max(0.,distance-self.params["core_radius"])/(radius-self.params["core_radius"]))**self.params["falloff_power"] if distance < radius else 0.
            braking = incoming_speed**2/(2*max(.05,thrust-local_gravity))
            safe_radius += braking+incoming_speed*2.
            if self.avoidance and ship.eff("engine") and ship.ftl is None and distance < safe_radius:
                outward = unit(offset)
                desired_velocity = mul(outward, ship.d["speed"]*.9)
            if ship.ftl is None and self.t >= ship.order_until and ship.eff("engine"):
                maximum = self.params["propulsion_acceleration"][ship.size-1]*ship.eff("engine")
                own_acceleration = clamp_length(mul(sub(desired_velocity, old_velocity), 1/2.), maximum)
            gravity = [0., 0., 0.]
            if distance < radius:
                factor = max(0., 1-max(0., distance-self.params["core_radius"])/(radius-self.params["core_radius"]))
                gravity = mul(unit(sub(center, old)), self.params["peak_acceleration"]*factor**self.params["falloff_power"])
                self.metric["env_gravity_ship_seconds"] += dt
                self.metric["env_gravity_impulse_sum"] += norm(gravity)*dt
            acceleration = add(own_acceleration, gravity)
            ship.vel = add(old_velocity, mul(acceleration, dt))
            proposed = add(old, add(mul(old_velocity, dt), mul(acceleration, .5*dt*dt)))
            ship.gravity_velocity = list(ship.vel)
            contact = self.params["core_radius"]+(ship.d["radius"] if self.params["surface_contact"] else 0.)
            if segment_sphere(old, proposed, center, contact) is not None:
                ship.pos = proposed
                self.wreck(ship, "env_blackhole")
                return
        elif self.bodies:
            seeking_cover = False
            # Outside a warned pulse, move around permanent shelter to regain LOS.
            if self.avoidance and ship.eff("engine") and ship.ftl is None and self.t >= ship.order_until and ship.target and ship.target.alive():
                block = self.blockers(old,ship.target.pos)
                if block and math.isinf(block[2]["hp"]):
                    body = block[2]
                    margin = body["radius"]+ship.d["radius"]+35.
                    candidates = [add(body["pos"],off) for off in ([0,0,margin],[0,0,-margin],[0,margin,0],[0,-margin,0])]
                    candidates = [point for point in candidates if self.blockers(old,point,ship.d["radius"]*.65) is None and math.dist(old,point)>5.]
                    if candidates:
                        goal=min(candidates,key=lambda point:math.dist(old,point)+math.dist(point,ship.target.pos)+500.*sum(segment_sphere(point,ship.target.pos,body2["pos"],body2["radius"]) is not None for body2 in self.bodies if not body2["destroyed"]))
                        ship.vel=mul(unit(sub(goal,old)),ship.d["speed"]*.7*ship.eff("engine"))
                        desired_velocity=list(ship.vel);proposed=add(old,mul(ship.vel,dt))
                        self.metric["env_los_reposition_steps"]+=1
            if self.hazard == "flare" and self.avoidance and ship.eff("engine") and ship.ftl is None and self.t >= ship.order_until:
                current_wave = max(0, math.floor((self.t-self.params["warning"])/self.params["period"]))
                for wave in (current_wave, current_wave+1):
                    arrival = self.params["warning"]+wave*self.params["period"]+(1600+old[0]*self.wave_direction)/self.params["speed"]
                    if -(self.params["thickness"]+2*ship.d["radius"])/self.params["speed"] <= arrival-self.t <= self.params["warning"]:
                        key = (wave, self.wave_direction)
                        if getattr(ship,"cover_key",None) != key:
                            candidates = [add(body["pos"],[self.wave_direction*(body["radius"]+ship.d["radius"]+12),0,0]) for body in self.bodies]
                            ship.cover_goal = min(candidates,key=lambda point:math.dist(old,point))
                            ship.cover_key = key
                        delta = sub(ship.cover_goal,old)
                        speed = ship.d["speed"]*.7*ship.eff("engine")
                        ship.vel = mul(unit(delta),min(speed,norm(delta)/max(dt,.001)))
                        desired_velocity = list(ship.vel)
                        proposed = add(old,mul(ship.vel,dt))
                        break
            radius = ship.d["radius"]*.65
            lookahead = add(old, mul(unit(desired_velocity), max(norm(desired_velocity)*8, 130.)))
            block = self.blockers(old, lookahead, radius+8.) if norm(desired_velocity) > 0 else None
            if self.avoidance and block and ship.eff("engine") and ship.ftl is None and self.t >= ship.order_until:
                body = block[2]
                margin = body["radius"]+radius+25.
                candidates = [add(body["pos"], off) for off in ([0,0,margin],[0,0,-margin],[0,margin,0],[0,-margin,0])]
                candidates = [point for point in candidates if self.blockers(old, point, radius) is None]
                if candidates:
                    goal = min(candidates, key=lambda p: math.dist(old,p)+abs(p[0]))
                    ship.vel = mul(unit(sub(goal,old)), norm(desired_velocity))
                    proposed = add(old, mul(ship.vel, dt))
                    self.metric["env_avoidance_steps"] += 1
            collision = self.blockers(old, proposed, radius)
            if collision:
                fraction, _, body = collision
                ship.pos = add(old, mul(sub(proposed,old), max(0., fraction-.001)))
                if self.hazard == "asteroid" and self.t >= self.collision_until.get((ship.uid,body["id"]),0.):
                    speed_fraction = min(2., norm(ship.vel)/max(1.,ship.d["speed"]))
                    amount = ship.maxhp*self.params["collision_hp_fraction"]*speed_fraction**2
                    face = "front" if ship.vel[0] >= 0 else "rear"
                    self.damage_surface(ship, amount, face, 0, "asteroid_collision")
                    self.collision_until[ship.uid,body["id"]] = self.t+self.params["collision_interval"]
                    self.metric["env_collisions"] += 1
                ship.vel = [0.,0.,0.]
                return
        ship.pos = proposed

    def observe(self):
        if self.hazard != "flare":
            return super().observe()
        for side in (0,1):
            contacts = {}
            for enemy in self.living(1-side):
                found = False
                for own in self.living(side):
                    radar = ORIGINAL_EFF(own,"radar")
                    if own.now < own.emp_until: radar = 0.
                    multiplier = own.radar_scale if own.now < own.radar_until else 1.
                    if legacy.surface(own,enemy) <= 250 or (radar > 0 and legacy.dist(own,enemy) <= legacy.P["radar"][str(own.d["radar_tier"])]["range"]*multiplier):
                        found = True; break
                if found:
                    contacts[enemy.uid] = {"id":enemy.uid,"kind":enemy.kind,"position":tuple(enemy.pos),
                                           "shield_active":enemy.active,
                                           "observed_mounts":tuple(i for i,w in enumerate(enemy.guns) if w["hp"]>0)}
            self.knowledge[side] = contacts

    def flare_crosses(self, pos, radius, wave, dt):
        emitted = self.params["warning"]+wave*self.params["period"]
        elapsed = self.t-emitted
        if elapsed < 0: return False
        current = -1600.+elapsed*self.params["speed"]
        previous = current-self.params["speed"]*dt
        projected = pos[0]*self.wave_direction
        half = self.params["thickness"]*.5+radius
        return previous <= projected+half and current >= projected-half

    def pre_environment(self, dt):
        for ship in self.ships: ship.now = self.t
        if self.hazard == "radiation":
            for ship in self.ships:
                if ship.alive() and math.dist(ship.pos,[0,0,0])-ship.d["radius"] < self.params["radius"]:
                    ship.last_combat = self.t
        if self.hazard != "flare": return
        wave = math.floor((self.t-self.params["warning"])/self.params["period"])
        if wave < 0: return
        for ship in self.ships:
            if not ship.alive() or (wave,ship.uid) in self.pulse_seen: continue
            if not self.flare_crosses(ship.pos,ship.d["radius"],wave,dt): continue
            self.pulse_seen.add((wave,ship.uid))
            exposed,total = self.exposed_samples(ship)
            fraction = len(exposed)/total
            self.metric["env_flare_exposure_sum"] += fraction
            self.metric["env_flare_contacts"] += 1
            if not exposed:
                self.metric["env_flare_fully_covered"] += 1
                continue
            amount = max(ship.maxshield,ship.maxhp*.2)*self.params["shield_fraction"]/total
            # Trial: no active shield before or after pulse => temporary shutdown.
            for face,cell,_ in exposed: self.damage_surface(ship,amount,face,cell,"flare")
            if not ship.alive(): continue
            ship.radar_until = self.t+self.params["radar_seconds"]
            ship.radar_scale = self.params["radar_scale"]
            if not ship.active:
                ship.emp_until = max(ship.emp_until,self.t+self.params["emp_seconds"])
                ship.pd_budget = 0.
                self.metric["env_ship_shutdowns"] += 1
        for ship in self.ships:
            for a in self.aircraft.get(ship.uid,[]):
                if a["hp"] <= 0 or a["docked"] or (wave,a["id"]) in self.pulse_air_seen: continue
                if self.flare_crosses(a["pos"],2.,wave,dt):
                    self.pulse_air_seen.add((wave,a["id"]))
                    if self.blockers(add(a["pos"],[-self.wave_direction*4000,0,0]),a["pos"]): continue
                    # Aircraft own shield; pulse cannot damage passengers/units twice.
                    if a["active"]:
                        a["shield"] = max(0.,a["shield"]-75*self.params["shield_fraction"])
                        a["active"] = a["shield"] > 0
                    if not a["active"]:
                        a["emp_until"] = self.t+self.params["emp_seconds"]
                        self.metric["env_aircraft_shutdowns"] += 1
        for pod in self.pods:
            if pod["done"] or (wave,pod["id"]) in self.pulse_pod_seen: continue
            if self.flare_crosses(pod["pos"],2.,wave,dt):
                self.pulse_pod_seen.add((wave,pod["id"]))
                if not self.blockers(add(pod["pos"],[-self.wave_direction*4000,0,0]),pod["pos"]):
                    pod["emp_until"] = self.t+self.params["emp_seconds"]
                    self.metric["env_pod_shutdowns"] += 1

    def environment(self, dt):
        super().environment(dt)
        if self.hazard != "radiation": return
        for ship in self.ships:
            if not ship.alive(): continue
            samples = list(self.samples(ship))
            dose = max(ship.maxshield,ship.maxhp*.2)*self.params["peak_shield_fraction_per_second"]/len(samples)
            for face,cell,point in samples:
                distance = norm(point)
                if distance >= self.params["radius"]: continue
                intensity = (1-distance/self.params["radius"])**self.params["falloff_power"]
                self.damage_surface(ship,dose*intensity*(1-self.rad_resistance)*dt,face,cell,"radiation")
                self.metric["env_radiation_exposed_cell_seconds"] += dt

    def move_pods(self, dt):
        for pod in self.pods:
            if pod["done"]: continue
            event, target = pod["event"],pod["target"]
            if not target.alive():
                pod["done"] = True
                super().pod_arrive(event)
                continue
            if self.t < pod["emp_until"]: continue
            old = list(pod["pos"])
            distance = math.dist(old,target.pos)
            step = min(distance,legacy.C["pod_speed"]*dt)
            new = add(old,mul(unit(sub(target.pos,old)),step))
            if self.hazard == "asteroid" and self.blockers(old,new,2.):
                pod["hp"] = 0.
            if self.hazard == "radiation":
                intensity = max(0.,1-norm(new)/self.params["radius"])**self.params["falloff_power"]
                pod["hp"] -= legacy.C["pod_hp"]*self.params["peak_shield_fraction_per_second"]*intensity*dt
            pod["pos"] = new
            if pod["hp"] <= 0:
                pod["done"] = True
                self.rescue(event["unit"],event["op"])
                self.metric["env_pod_losses"] += 1
            elif distance <= step+2.:
                pod["done"] = True
                super().pod_arrive(event)

    def fighter_step(self, dt):
        # Preserve legacy aggregate combat, but maintain actual counted IDs for exposure.
        for carrier in self.ships:
            wing = self.aircraft.get(carrier.uid)
            if not wing: continue
            live = [a for a in wing if a["hp"] > 0]
            target = carrier.target if carrier.alive() and carrier.eff("flight") and carrier.target and carrier.target.alive() else None
            for a in live:
                a["docked"] = carrier.fighter_docked >= len(live)
                if a["docked"]: a["pos"] = list(carrier.pos); continue
                if carrier.alive() and carrier.eff("flight") and self.t >= a["emp_until"]:
                    if target and not carrier.fighter_return:
                        radius = target.d["radius"]+65.
                        goal = add(target.pos,[0.,radius*math.cos(a["phase"]+self.t*.7),radius*math.sin(a["phase"]+self.t*.7)])
                    else: goal = carrier.pos
                    delta = sub(goal,a["pos"])
                    a["pos"] = add(a["pos"],mul(unit(delta),min(norm(delta),100.*dt)))
                if self.hazard == "radiation":
                    intensity = max(0.,1-norm(a["pos"])/self.params["radius"])**self.params["falloff_power"]
                    amount = (75.+legacy.C["fighter_hp"]*.2)*self.params["peak_shield_fraction_per_second"]*intensity*dt
                    if a["active"]:
                        a["shield"] = max(0.,a["shield"]-amount)
                        if not a["shield"]: a["active"] = False
                    else: a["hp"] = max(0.,a["hp"]-amount)
            before = len([a for a in wing if a["hp"] > 0])
            enabled = len([a for a in wing if a["hp"]>0 and self.t>=a["emp_until"]])
            carrier.fighters = enabled
        super().fighter_step(dt)
        for carrier in self.ships:
            wing = self.aircraft.get(carrier.uid)
            if not wing: continue
            combat_survivors = carrier.fighters
            enabled = [a for a in wing if a["hp"]>0 and self.t>=a["emp_until"]]
            losses = max(0,len(enabled)-combat_survivors)
            for a in enabled[:losses]: a["hp"] = 0.
            carrier.fighters = sum(a["hp"]>0 for a in wing)

    def tick(self, dt):
        self.pre_environment(dt)
        self.move_pods(dt)
        super().tick(dt)
        if int(self.t)%30 == 0 and (not self.env_trace or self.env_trace[-1]["t"] != round(self.t)):
            self.env_trace.append({"t":round(self.t),"remaining":[sum(s.d["fleet_cost"] for s in self.living(side)) for side in (0,1)]})

    def run(self, dt=None):
        if self.hazard == "none": return super().run(dt)
        dt = dt or legacy.C["dt"]
        while self.t < self.duration:
            self.tick(min(dt,self.duration-self.t))
            if not self.living(0) or not self.living(1): break
            # Scheduled pulses, temporary shutdowns and gravity remain live even
            # when both sides are briefly unable to fire. Never declare victory.
        return self.result()

    def result(self):
        out = super().result()
        if out["outcome"] == "stalemate" and any(s.alive() and self.t < s.emp_until for s in self.ships):
            out["outcome"] = "timeout"
            out["end_reason"] = "observation_limit_during_temporary_shutdown"
        current_ids = [u.uid for ship in self.ships for u in ship.units+ship.officers]
        current_ids += [u.uid for op in self.operations for u in op["units"]]
        out.update({
            "environment":self.hazard,"active_environment_count":int(self.hazard!="none"),
            "parameters":self.params,"avoidance":self.avoidance,"resistance":self.rad_resistance,
            "environment_trace":self.env_trace,
            "surviving_ship_positions":{str(s.uid):[round(x,3) for x in s.pos] for s in self.ships if s.alive()},
            "initial_total_hull_hp":sum(s.maxhp for s in self.ships),
            "asteroids_remaining":sum(not x["destroyed"] for x in self.bodies) if self.hazard=="asteroid" else None,
            "aircraft_surviving":sum(a["hp"]>0 for wing in self.aircraft.values() for a in wing),
            "aircraft_records":len(self.initial_air_ids),
            "hero_id_set_unchanged":set(current_ids)==self.initial_unit_ids,
            "pending_pods":sum(not p["done"] for p in self.pods),
            "limitations":["legacy aggregate fighter damage/PD; per-ID exposure uses proxy flight",
                           "temporary room/armor face samples, not UE mesh raycasts",
                           "blackhole contact uses sphere surface; tow is not implemented",
                           "flare eligibility uses shield state before/after pulse; life support and indoor units not EMP-stunned"],
        })
        return out
