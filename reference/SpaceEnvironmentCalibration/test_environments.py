"""Behavioral/analytic boundaries for the isolated environment extension."""
import copy
import unittest
from environment_battle import EnvironmentBattle, DEFAULTS, segment_sphere, legacy

class Rules(unittest.TestCase):
    def battle(self, hazard, **options):
        return EnvironmentBattle(["cruiser"],["cruiser"],171800,hazard=hazard,
                                 heroes=False,retreat=False,duration=60,**options)

    def test_reject_combined_hazards(self):
        for value in (["flare","blackhole"],"flare+blackhole",None):
            with self.assertRaises(ValueError):
                self.battle(value)

    def test_swept_contact(self):
        self.assertAlmostEqual(segment_sphere([-100,0,0],[100,0,0],[0,0,0],10),.45)
        self.assertIsNone(segment_sphere([-100,20,0],[100,20,0],[0,0,0],10))

    def test_no_environment_is_not_damage(self):
        b=self.battle("none")
        hp=[s.hp for s in b.ships]
        b.environment(20)
        self.assertEqual(hp,[s.hp for s in b.ships])

    def test_radiation_only_exposed_cells(self):
        b=self.battle("radiation")
        s=b.ships[0]
        s.pos=[b.params["radius"]+s.d["radius"]*.5,0,0]
        s.active=False;s.shield=0
        before=copy.deepcopy(s.armor)
        b.environment(1)
        self.assertEqual(before["front"],s.armor["front"])
        self.assertTrue(any(s.armor[f][i]<v for f,arr in before.items() for i,v in enumerate(arr)))
        self.assertTrue(any(s.armor[f][i]==v for f,arr in before.items() for i,v in enumerate(arr)))

    def test_radiation_resistance_is_damage_reduction(self):
        doses=[]
        for resist in (0,.5):
            b=self.battle("radiation",radiation_resistance=resist)
            for s in b.ships:s.pos=[0,0,0]
            b.environment(.5)
            doses.append(b.metric["env_damage_input_radiation"])
        self.assertAlmostEqual(doses[1],doses[0]*.5)

    def test_radiation_combat_and_no_regen_delay(self):
        b=self.battle("radiation");s=b.ships[0]
        s.pos=[0,0,0];s.active=False;s.shield=.2*s.maxshield
        b.t=50;b.pre_environment(.5);before=s.shield;s.shield_tick(b.t,.5)
        self.assertEqual(s.last_combat,50)
        self.assertGreater(s.shield,before)
        self.assertAlmostEqual(s.shield-before,s.maxshield*.006*.5)

    def test_flare_covered_partial_and_open(self):
        b=self.battle("flare");s=b.ships[0];s.pos=[0,0,0]
        b.bodies=[]
        self.assertEqual(len(b.exposed_samples(s)[0]),b.exposed_samples(s)[1])
        b.bodies=[{"id":0,"pos":[-200,0,0],"radius":8,"destroyed":False,"hp":1000}]
        n,total=len(b.exposed_samples(s)[0]),b.exposed_samples(s)[1]
        self.assertGreater(n,0);self.assertLess(n,total)
        b.bodies[0]["radius"]=120
        self.assertEqual(len(b.exposed_samples(s)[0]),0)

    def test_flare_stops_functions_without_erasing_gauge(self):
        b=self.battle("flare");s=b.ships[0]
        b.bodies=[];s.pos=[0,0,0];s.active=False;s.shield=.2*s.maxshield
        b.t=b.params["warning"]+1600/b.params["speed"]
        b.pre_environment(.5)
        self.assertEqual(s.eff("engine"),0)
        self.assertEqual(s.eff("weapon"),0)
        self.assertEqual(s.eff("pd"),0)
        self.assertGreater(s.eff("life"),0)
        before=s.shield;s.shield_tick(b.t,.5);self.assertEqual(s.shield,before)
        b.t=s.emp_until+.01;s.now=b.t
        self.assertGreater(s.eff("engine"),0)

    def test_flare_no_duplicate_or_skip_on_long_frame(self):
        b=self.battle("flare");b.bodies=[]
        for s in b.ships:s.pos=[0,0,0]
        b.t=b.params["warning"]+4
        b.pre_environment(5)
        contacts=b.metric["env_flare_contacts"]
        b.pre_environment(5)
        self.assertEqual(contacts,2)
        self.assertEqual(b.metric["env_flare_contacts"],contacts)

    def test_blackhole_engine_loss_keeps_drift(self):
        b=self.battle("blackhole");s=b.ships[0]
        s.pos=[200,160,0];s.facilities["engine"]["destroyed"]=True
        b.update_motion(s,1)
        self.assertLess(s.pos[0],200)
        self.assertFalse(s.dead)

    def test_blackhole_core_ignores_hp(self):
        b=self.battle("blackhole");s=b.ships[0]
        s.pos=[b.params["core_radius"]+s.d["radius"]-1,160,0]
        b.update_motion(s,.5)
        self.assertTrue(s.dead)
        self.assertEqual(s.hp,0)

    def test_ftl_unchanged_far_and_wrecked_near(self):
        for distance,escape in ((600,True),(100,False)):
            b=self.battle("blackhole");s=b.ships[0]
            for v in b.ships:v.guns=[]
            s.pos=[distance,160,0];s.ftl=20
            for _ in range(42):b.tick(.5)
            self.assertEqual(s.escaped,escape)
            self.assertEqual(s.dead,not escape)
            if escape:self.assertEqual(b.metric["escaped_cost_0"],5)

    def test_destroyed_asteroid_opens_line(self):
        b=self.battle("asteroid")
        a,v=b.ships;a.pos=[-200,0,0];v.pos=[200,0,0]
        b.bodies=[{"id":0,"pos":[0,0,0],"radius":50,"hp":100,"destroyed":False}]
        e={"kind":"shot","attacker":a,"target":v,"aim":v.pos[:],"damage":200,
           "side":0,"weapon":"beam","beam_hit":True,"gun":None,"source_size":2,"flight":0}
        shield=v.shield
        b.schedule(0,e);b.process_events()
        self.assertTrue(b.bodies[0]["destroyed"])
        self.assertEqual(v.shield,shield)
        self.assertIsNone(b.blockers(a.pos,v.pos))
        b.schedule(0,e);b.process_events()
        self.assertLess(v.shield,shield)
        self.assertEqual(b.metric["env_asteroids_destroyed"],1)

    def test_hold_does_not_cancel_gravity(self):
        b=self.battle("blackhole");s=b.ships[0]
        s.pos=[200,160,0];s.order_until=1e6
        b.update_motion(s,1)
        self.assertLess(s.pos[0],200)

    def test_engine_loss_reaches_core_before_timeout(self):
        b=self.battle("blackhole");s=b.ships[0]
        s.pos=[200,160,0];s.facilities["engine"]["destroyed"]=True
        for _ in range(120):
            b.update_motion(s,.5);b.t+=.5
            if s.dead:break
        self.assertTrue(s.dead)
        self.assertGreater(b.t,1)

    def test_aircraft_shutdown_independent_of_carrier_wave_contact(self):
        b=EnvironmentBattle(["carrier"],["carrier"],171800,hazard="flare",heroes=False)
        b.bodies=[]
        for s in b.ships:s.pos=[500,1000,0]
        a=b.aircraft[0][0];a["pos"]=[0,1000,0];a["active"]=False;a["shield"]=0
        b.t=b.params["warning"]+1600/b.params["speed"]
        b.pre_environment(.5)
        self.assertGreater(a["emp_until"],b.t)
        self.assertNotIn((0,0),b.pulse_seen)

    def test_control_lost_aircraft_still_take_radiation(self):
        b=EnvironmentBattle(["carrier"],["carrier"],171800,hazard="radiation",heroes=False)
        s=b.ships[0];s.facilities["flight"]["destroyed"]=True
        a=b.aircraft[0][0];a["pos"]=[0,0,0];a["shield"]=0;a["active"]=False
        hp=a["hp"];b.fighter_step(.5)
        self.assertLess(a["hp"],hp)

    def test_stopped_pod_waits_and_resumes(self):
        b=self.battle("flare");b.bodies=[]
        a={"id":"test","pos":[0,1000,0],"done":False,"emp_until":0,
           "hp":160,"target":b.ships[1],"event":{}}
        b.pods=[a];b.t=b.params["warning"]+1600/b.params["speed"]
        b.pre_environment(.5);pos=a["pos"][:];b.move_pods(.5)
        self.assertEqual(a["pos"],pos)
        b.t=a["emp_until"]+.01;b.move_pods(.5)
        self.assertNotEqual(a["pos"],pos)

    def test_environment_core_wreck_attributed_once(self):
        b=self.battle("radiation");s=b.ships[0]
        b.destroy_facility(s,"core","radiation")
        b.wreck(s,"radiation")
        self.assertEqual(b.metric["environment_wrecks"],1)
        self.assertEqual(b.metric["wreck_env_radiation_core"],1)

    def test_asteroid_contact_damage_has_cooldown(self):
        b=EnvironmentBattle(["assault"],["assault"],171800,hazard="asteroid",heroes=False,avoidance=False)
        s,v=b.ships;s.pos=[-100,0,0];v.pos=[250,0,0]
        b.bodies=[{"id":0,"pos":[0,0,0],"radius":50,"hp":1000,"destroyed":False}]
        b.knowledge[0]={v.uid:{"id":v.uid,"kind":v.kind,"position":tuple(v.pos),"shield_active":True,"observed_mounts":()}}
        for _ in range(4):b.update_motion(s,.5);b.t+=.5
        self.assertGreater(b.metric["env_collisions"],0)
        self.assertGreater(b.metric["env_damage_input_asteroid_collision"],0)
        damage=b.metric["env_damage_input_asteroid_collision"]
        b.update_motion(s,.5)
        self.assertEqual(b.metric["env_damage_input_asteroid_collision"],damage)

    def test_radiation_time_integral(self):
        doses=[]
        for step,count in ((1,1),(.5,2)):
            b=self.battle("radiation")
            for s in b.ships:s.pos=[0,0,0]
            for _ in range(count):b.environment(step)
            doses.append(b.metric["env_damage_input_radiation"])
        self.assertAlmostEqual(*doses)

    def test_shutdown_is_not_early_stalemate(self):
        b=self.battle("flare");b.duration=100;b.t=95;b.bodies=[]
        for s in b.ships:s.active=False;s.shield=0;s.emp_until=120
        r=b.run(.5)
        self.assertEqual(r["seconds"],100)
        self.assertEqual(r["outcome"],"timeout")

    def test_initial_geometry_has_clearance(self):
        for hazard in ("asteroid","flare"):
            for budget in (100,200,300):
                b=EnvironmentBattle(legacy.roster(budget),legacy.roster(budget),171800,hazard=hazard,heroes=False)
                for ship in b.ships:
                    for body in b.bodies:
                        import math
                        self.assertGreater(math.dist(ship.pos,body["pos"]),body["radius"]+ship.d["radius"])

    def test_unit_identity_short_linked(self):
        b=EnvironmentBattle(legacy.roster(100),legacy.roster(100),171800,hazard="flare",
                            mode="assault",heroes=True,duration=10,retreat=False)
        self.assertTrue(b.run(.5)["hero_id_set_unchanged"])

if __name__=="__main__":unittest.main(verbosity=2)
